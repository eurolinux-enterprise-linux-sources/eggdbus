/*
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <eggdbus/eggdbusmessage.h>
#include <eggdbus/eggdbusmisctypes.h>
#include <eggdbus/eggdbusobjectpath.h>
#include <eggdbus/eggdbussignature.h>
#include <eggdbus/eggdbuserror.h>
#include <eggdbus/eggdbusutils.h>
#include <eggdbus/eggdbusarrayseq.h>
#include <eggdbus/eggdbushashmap.h>
#include <eggdbus/eggdbusstructure.h>
#include <eggdbus/eggdbusvariant.h>
#include <eggdbus/eggdbusconnection.h>
#include <eggdbus/eggdbusprivate.h>

#include <stdio.h>

/**
 * SECTION:eggdbusmessage
 * @title: EggDBusMessage
 * @short_description: Represents a D-Bus message
 *
 * The #EggDBusMessage class is used for sending and receiving D-Bus messages. This class is
 * only useful for language bindings.
 */

typedef struct
{
  EggDBusConnection *connection;

  gchar *interface_name;
  gchar *signal_name;
  gchar *method_name;
  EggDBusMessage *in_reply_to;
  gchar *sender;
  gchar *destination;
  gchar *object_path;

  gchar *error_name;
  gchar *error_message;

  EggDBusMessageType message_type;

  gboolean read_iter_initialized;
  gboolean write_iter_initialized;

  DBusMessageIter message_read_iter;
  DBusMessageIter message_write_iter;

} EggDBusMessagePrivate;

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_MESSAGE_TYPE,
  PROP_OBJECT_PATH,
  PROP_INTERFACE_NAME,
  PROP_METHOD_NAME,
  PROP_SIGNAL_NAME,
  PROP_IN_REPLY_TO,
  PROP_ERROR_NAME,
  PROP_ERROR_MESSAGE,
  PROP_SENDER,
  PROP_DESTINATION,
  PROP_SIGNATURE,
};

static gboolean egg_dbus_get_value_from_iter  (DBusMessageIter              *iter,
                                               GValue                       *out_value,
                                               GError                      **error);

static gboolean egg_dbus_append_value_to_iter (DBusMessageIter              *iter,
                                               const gchar                  *signature,
                                               const GValue                 *value,
                                               GError                      **error);


#define EGG_DBUS_MESSAGE_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_MESSAGE, EggDBusMessagePrivate))

G_DEFINE_TYPE (EggDBusMessage, egg_dbus_message, G_TYPE_OBJECT);

static void
egg_dbus_message_init (EggDBusMessage *message)
{
}

static void
egg_dbus_message_finalize (GObject *object)
{
  EggDBusMessage *message;
  EggDBusMessagePrivate *priv;

  message = EGG_DBUS_MESSAGE (object);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  if (priv->connection != NULL)
    g_object_unref (priv->connection);
  if (priv->in_reply_to != NULL)
    g_object_unref (priv->in_reply_to);
  g_free (priv->interface_name);
  g_free (priv->method_name);
  g_free (priv->signal_name);
  g_free (priv->sender);
  g_free (priv->destination);
  g_free (priv->object_path);
  g_free (priv->error_name);
  g_free (priv->error_message);

  G_OBJECT_CLASS (egg_dbus_message_parent_class)->finalize (object);
}

static void
egg_dbus_message_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EggDBusMessage *message;
  EggDBusMessagePrivate *priv;

  message = EGG_DBUS_MESSAGE (object);
  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  switch (prop_id)
    {
    case PROP_OBJECT_PATH:
      priv->object_path = g_strdup (g_value_get_boxed (value));
      break;

    case PROP_INTERFACE_NAME:
      priv->interface_name = g_strdup (g_value_get_string (value));
      break;

    case PROP_CONNECTION:
      priv->connection = g_value_dup_object (value);
      break;

    case PROP_MESSAGE_TYPE:
      priv->message_type = g_value_get_enum (value);
      break;

    case PROP_METHOD_NAME:
      priv->method_name = g_strdup (g_value_get_string (value));
      break;

    case PROP_SIGNAL_NAME:
      priv->signal_name = g_strdup (g_value_get_string (value));
      break;

    case PROP_IN_REPLY_TO:
      priv->in_reply_to = g_value_dup_object (value);
      break;

    case PROP_ERROR_NAME:
      priv->error_name = g_strdup (g_value_get_string (value));
      break;

    case PROP_ERROR_MESSAGE:
      priv->error_message = g_strdup (g_value_get_string (value));
      break;

    case PROP_SENDER:
      priv->sender = g_strdup (g_value_get_string (value));
      break;

    case PROP_DESTINATION:
      priv->destination = g_strdup (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_message_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EggDBusMessage *message;
  EggDBusMessagePrivate *priv;

  message = EGG_DBUS_MESSAGE (object);
  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;

    case PROP_MESSAGE_TYPE:
      g_value_set_enum (value, priv->message_type);
      break;

    case PROP_OBJECT_PATH:
      g_value_set_boxed (value, priv->object_path);
      break;

    case PROP_INTERFACE_NAME:
      g_value_set_string (value, priv->interface_name);
      break;

    case PROP_METHOD_NAME:
      g_value_set_string (value, priv->method_name);
      break;

    case PROP_SIGNAL_NAME:
      g_value_set_string (value, priv->signal_name);
      break;

    case PROP_IN_REPLY_TO:
      g_value_set_object (value, priv->in_reply_to);
      break;

    case PROP_SIGNATURE:
      g_value_set_string (value, egg_dbus_message_get_signature (message));
      break;

    case PROP_ERROR_NAME:
      g_value_set_string (value, priv->error_name);
      break;

    case PROP_ERROR_MESSAGE:
      g_value_set_string (value, priv->error_message);
      break;

    case PROP_SENDER:
      g_value_set_string (value, priv->sender);
      break;

    case PROP_DESTINATION:
      g_value_set_string (value, priv->destination);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_message_constructed (GObject *object)
{
  EggDBusMessage *message;
  EggDBusMessagePrivate *priv;
  DBusMessage *dmessage;
  DBusMessage *dmessage_in_reply_to;

  if (G_OBJECT_CLASS (egg_dbus_message_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (egg_dbus_message_parent_class)->constructed (object);

  message = EGG_DBUS_MESSAGE (object);
  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  dmessage = NULL;

  switch (priv->message_type)
    {
    case EGG_DBUS_MESSAGE_TYPE_METHOD_CALL:
      dmessage = dbus_message_new_method_call (priv->destination,
                                               priv->object_path,
                                               priv->interface_name,
                                               priv->method_name);
      break;

    case EGG_DBUS_MESSAGE_TYPE_METHOD_REPLY:
      dmessage_in_reply_to = g_object_get_data (G_OBJECT (priv->in_reply_to), "dbus-1-message");
      dmessage = dbus_message_new_method_return (dmessage_in_reply_to);
      break;

    case EGG_DBUS_MESSAGE_TYPE_METHOD_ERROR_REPLY:
      dmessage_in_reply_to = g_object_get_data (G_OBJECT (priv->in_reply_to), "dbus-1-message");
      dmessage = dbus_message_new_error (dmessage_in_reply_to,
                                         priv->error_name,
                                         priv->error_message);
      break;

    case EGG_DBUS_MESSAGE_TYPE_SIGNAL:
      dmessage = dbus_message_new_signal (priv->object_path,
                                          priv->interface_name,
                                          priv->signal_name);
      if (priv->destination != NULL)
        dbus_message_set_destination (dmessage,
                                      priv->destination);
      break;
    }

  g_object_set_data_full (G_OBJECT (message),
                          "dbus-1-message",
                          dmessage,
                          (GDestroyNotify) dbus_message_unref);

}


static void
egg_dbus_message_class_init (EggDBusMessageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_message_finalize;
  gobject_class->set_property = egg_dbus_message_set_property;
  gobject_class->get_property = egg_dbus_message_get_property;
  gobject_class->constructed  = egg_dbus_message_constructed;

  g_type_class_add_private (klass, sizeof (EggDBusMessagePrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        "Connection",
                                                        "The connection this message is for",
                                                        EGG_DBUS_TYPE_CONNECTION,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /* TODO: use flags instead */
  g_object_class_install_property (gobject_class,
                                   PROP_MESSAGE_TYPE,
                                   g_param_spec_enum ("message-type",
                                                      "Message Type",
                                                      "The type of the message",
                                                      EGG_TYPE_DBUS_MESSAGE_TYPE,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_boxed ("object-path",
                                                       "Object Path",
                                                       "The object path",
                                                       EGG_DBUS_TYPE_OBJECT_PATH,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK |
                                                       G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_INTERFACE_NAME,
                                   g_param_spec_string ("interface-name",
                                                        "Interface Name",
                                                        "The name of the interface",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_METHOD_NAME,
                                   g_param_spec_string ("method-name",
                                                        "Method Name",
                                                        "The name of the method",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_SIGNAL_NAME,
                                   g_param_spec_string ("signal-name",
                                                        "Signal Name",
                                                        "The name of the signal",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_IN_REPLY_TO,
                                   g_param_spec_object ("in-reply-to",
                                                        "In Reply To",
                                                        "The message this is a reply to",
                                                        EGG_DBUS_TYPE_MESSAGE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ERROR_NAME,
                                   g_param_spec_string ("error-name",
                                                        "Error Name",
                                                        "The error name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ERROR_MESSAGE,
                                   g_param_spec_string ("error-message",
                                                        "Error message",
                                                        "The error message",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_SENDER,
                                   g_param_spec_string ("sender",
                                                        "Sender",
                                                        "The name of who sent the message",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_DESTINATION,
                                   g_param_spec_string ("destination",
                                                        "Destination",
                                                        "The destination of the message",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_SIGNATURE,
                                   g_param_spec_string ("signature",
                                                        "Signature",
                                                        "The signature of the message",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}

/**
 * egg_dbus_message_new_for_method_reply:
 * @reply_to: The message to reply to.
 *
 * Creates a new message representing a reply to another message.
 *
 * Returns: A #EggDBusMessage. Free with g_object_unref().
 **/
EggDBusMessage *
egg_dbus_message_new_for_method_reply (EggDBusMessage *reply_to)
{
  return EGG_DBUS_MESSAGE (g_object_new (EGG_DBUS_TYPE_MESSAGE,
                                       "connection",     egg_dbus_message_get_connection (reply_to),
                                       "sender",         NULL,
                                       "destination",    NULL,
                                       "message-type",   EGG_DBUS_MESSAGE_TYPE_METHOD_REPLY,
                                       "object-path",    NULL,
                                       "interface-name", NULL,
                                       "method-name",    NULL,
                                       "signal-name",    NULL,
                                       "in-reply-to",    reply_to,
                                       "error-name",     NULL,
                                       "error-message",  NULL,
                                       NULL));
}

/**
 * egg_dbus_message_new_for_method_error_reply:
 * @reply_to: The message to reply to.
 * @error_name: D-Bus error name.
 * @error_message: D-Bus error message.
 *
 * Creates a new error message in reply to another message.
 *
 * Returns: A #EggDBusMessage. Free with g_object_unref().
 **/
EggDBusMessage *
egg_dbus_message_new_for_method_error_reply  (EggDBusMessage         *reply_to,
                                            const gchar          *error_name,
                                            const gchar          *error_message)
{
  return EGG_DBUS_MESSAGE (g_object_new (EGG_DBUS_TYPE_MESSAGE,
                                       "connection",     egg_dbus_message_get_connection (reply_to),
                                       "sender",         NULL,
                                       "destination",    NULL,
                                       "message-type",   EGG_DBUS_MESSAGE_TYPE_METHOD_ERROR_REPLY,
                                       "object-path",    NULL,
                                       "interface-name", NULL,
                                       "method-name",    NULL,
                                       "signal-name",    NULL,
                                       "in-reply-to",    reply_to,
                                       "error-name",     error_name,
                                       "error-message",  error_message,
                                       NULL));
}

/**
 * egg_dbus_message_get_connection:
 * @message: A #EggDBusMessage.
 *
 * Gets the connection that @message is associated with.
 *
 * Returns: The connection that @message is associated with. Do not free, the connection is owned by @message.
 **/
EggDBusConnection *
egg_dbus_message_get_connection (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->connection;
}


/**
 * egg_dbus_message_get_message_type:
 * @message: A #EggDBusMessage.
 *
 * The type of the message.
 *
 * Returns: A value from the #EggDBusMessageType enumeration.
 **/
EggDBusMessageType
egg_dbus_message_get_message_type (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), -1);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->message_type;
}

/**
 * egg_dbus_message_get_object_path:
 * @message: A #EggDBusMessage.
 *
 * Gets the object path of the message.
 *
 * Returns: The object path of @message. Do not free, the value is owned by @message.
 **/
const gchar *
egg_dbus_message_get_object_path (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);
  g_return_val_if_fail (egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_METHOD_CALL ||
                        egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_SIGNAL, NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->object_path;
}

/**
 * egg_dbus_message_get_interface_name:
 * @message: A #EggDBusMessage.
 *
 * Gets the interface name of the message.
 *
 * Returns: The intername name of @message. Do not free, the value is owned by @message.
 **/
const gchar *
egg_dbus_message_get_interface_name (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);
  g_return_val_if_fail (egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_METHOD_CALL ||
                        egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_SIGNAL, NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->interface_name;
}

/**
 * egg_dbus_message_get_method_name:
 * @message: A #EggDBusMessage.
 *
 * Gets the method name of the message.
 *
 * Returns: The method name of the message. Do not free, the value is owned by @message.
 **/
const gchar *
egg_dbus_message_get_method_name (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);
  g_return_val_if_fail (egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_METHOD_CALL, NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->method_name;
}

/**
 * egg_dbus_message_get_signal_name:
 * @message: A #EggDBusMessage.
 *
 * Gets the signal name of the message.
 *
 * Returns: The signal name of the message. Do not free, the value is owned by @message.
 **/
const gchar *
egg_dbus_message_get_signal_name (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);
  g_return_val_if_fail (egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_SIGNAL, NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->signal_name;
}

/**
 * egg_dbus_message_get_in_reply_to:
 * @message: A #EggDBusMessage.
 *
 * Gets the message that @message is a reply to.
 *
 * Returns: A #EggDBusMessage. Do not free, the value is owned by @message.
 **/
EggDBusMessage *
egg_dbus_message_get_in_reply_to (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);
  g_return_val_if_fail (egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_METHOD_REPLY ||
                        egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_METHOD_ERROR_REPLY, NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->in_reply_to;
}

/**
 * egg_dbus_message_get_sender:
 * @message: A #EggDBusMessage.
 *
 * Gets the sender of the message.
 *
 * Returns: The sender of the message. Do not free, the value is owned by @message.
 **/
const gchar *
egg_dbus_message_get_sender (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);
  g_return_val_if_fail (egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_METHOD_CALL ||
                        egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_SIGNAL, NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->sender;
}

/**
 * egg_dbus_message_get_destination:
 * @message: A #EggDBusMessage.
 *
 * Gets the destination of the message.
 *
 * Returns: The destination of the message. Do not free, the value is owned by @message.
 **/
const gchar *
egg_dbus_message_get_destination (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);
  g_return_val_if_fail (egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_METHOD_CALL ||
                        egg_dbus_message_get_message_type (message) == EGG_DBUS_MESSAGE_TYPE_SIGNAL, NULL);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  return priv->destination;
}

/**
 * egg_dbus_message_get_signature:
 * @message: A #EggDBusMessage.
 *
 * Gets the signature of the message.
 *
 * Returns: The signature of the message. Do not free, the value is owned by @message.
 **/
const gchar *
egg_dbus_message_get_signature (EggDBusMessage *message)
{
  DBusMessage *dmessage;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), NULL);

  dmessage = g_object_get_data (G_OBJECT (message), "dbus-1-message");

  return dbus_message_get_signature (dmessage);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
ensure_read_iter (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;
  DBusMessage *dmessage;

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  if (priv->read_iter_initialized)
    return;

  dmessage = g_object_get_data (G_OBJECT (message), "dbus-1-message");

  dbus_message_iter_init (dmessage, &(priv->message_read_iter));

  priv->read_iter_initialized = TRUE;
}

static void
ensure_write_iter (EggDBusMessage *message)
{
  EggDBusMessagePrivate *priv;
  DBusMessage *dmessage;

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  if (priv->write_iter_initialized)
    return;

  dmessage = g_object_get_data (G_OBJECT (message), "dbus-1-message");

  dbus_message_iter_init_append (dmessage, &(priv->message_write_iter));

  priv->write_iter_initialized = TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_message_append_string:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a string to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_string        (EggDBusMessage  *message,
                                     const gchar   *value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_STRING);
  g_value_set_static_string (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_STRING_AS_STRING, error);
}

/**
 * egg_dbus_message_append_object_path:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends an object path to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_object_path (EggDBusMessage     *message,
                                     const gchar        *value,
                                     GError            **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_OBJECT_PATH);
  g_value_set_static_boxed (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_OBJECT_PATH_AS_STRING, error);
}

/**
 * egg_dbus_message_append_signature:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a D-Bus signature to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_signature     (EggDBusMessage     *message,
                                       const gchar        *value,
                                       GError            **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_SIGNATURE);
  g_value_set_static_boxed (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_SIGNATURE_AS_STRING, error);
}

/**
 * egg_dbus_message_append_string_array:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a %NULL-terminated string array to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_string_array  (EggDBusMessage  *message,
                                       gchar          **value,
                                       GError         **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_STRV);
  g_value_take_boxed (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_STRING_AS_STRING, error);
}

/**
 * egg_dbus_message_append_object_path_array:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a %NULL-terminated object path array to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_object_path_array  (EggDBusMessage          *message,
                                            gchar                  **value,
                                            GError                 **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_OBJECT_PATH_ARRAY);
  g_value_take_boxed (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_OBJECT_PATH_AS_STRING, error);
}

/**
 * egg_dbus_message_append_signature_array:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a %NULL-terminated D-Bus signature array to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_signature_array  (EggDBusMessage          *message,
                                          gchar                  **value,
                                          GError                 **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_SIGNATURE_ARRAY);
  g_value_take_boxed (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_SIGNATURE_AS_STRING, error);
}

/**
 * egg_dbus_message_append_byte:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #guchar to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_byte          (EggDBusMessage  *message,
                                     guchar         value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_UCHAR);
  g_value_set_uchar (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_BYTE_AS_STRING, error);
}

/**
 * egg_dbus_message_append_int16:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #gint16 to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_int16         (EggDBusMessage  *message,
                                     gint16         value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_INT16);
  egg_dbus_value_set_int16 (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_INT16_AS_STRING, error);
}

/**
 * egg_dbus_message_append_uint16:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #guint16 to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_uint16        (EggDBusMessage  *message,
                                     guint16        value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_UINT16);
  egg_dbus_value_set_uint16 (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_UINT16_AS_STRING, error);
}

/**
 * egg_dbus_message_append_int:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #gint to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_int         (EggDBusMessage  *message,
                                     gint           value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_INT32_AS_STRING, error);
}

/**
 * egg_dbus_message_append_uint:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #guint to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_uint        (EggDBusMessage  *message,
                                     guint          value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_UINT);
  g_value_set_uint (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_UINT32_AS_STRING, error);
}

/**
 * egg_dbus_message_append_int64:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #gint64 to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_int64         (EggDBusMessage  *message,
                                     gint64         value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_INT64);
  g_value_set_int64 (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_INT64_AS_STRING, error);
}

/**
 * egg_dbus_message_append_uint64:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #guint64 to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_uint64        (EggDBusMessage  *message,
                                     guint64        value,
                                     GError         **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_UINT64);
  g_value_set_uint64 (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_UINT64_AS_STRING, error);
}

/**
 * egg_dbus_message_append_boolean:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #gboolean to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_boolean       (EggDBusMessage  *message,
                                     gboolean       value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_BOOLEAN_AS_STRING, error);
}

/**
 * egg_dbus_message_append_double:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @error: Return location for error.
 *
 * Appends a #gdouble to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_double        (EggDBusMessage  *message,
                                     gdouble        value,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_DOUBLE);
  g_value_set_double (&val, value);
  return egg_dbus_message_append_gvalue (message, &val, DBUS_TYPE_DOUBLE_AS_STRING, error);
}

/**
 * egg_dbus_message_append_seq:
 * @message: A #EggDBusMessage.
 * @seq: Sequence to append to @message.
 * @element_signature: D-Bus signature of elements in @seq.
 * @error: Return location for error.
 *
 * Appends a #EggDBusSeq to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_seq (EggDBusMessage   *message,
                             EggDBusArraySeq  *seq,
                             const gchar      *element_signature,
                             GError          **error)
{
  GValue val = {0};
  gboolean ret;
  gchar *sig;
  g_value_init (&val, EGG_DBUS_TYPE_ARRAY_SEQ);
  g_value_take_object (&val, seq);
  sig = g_strdup_printf ("a%s", element_signature);
  ret = egg_dbus_message_append_gvalue (message, &val, sig, error);
  g_free (sig);
  return ret;
}

/**
 * egg_dbus_message_append_map:
 * @message: A #EggDBusMessage.
 * @map: Map to append to @message.
 * @key_signature: D-Bus signature of keys in @map.
 * @value_signature: D-Bus signature of values in @map.
 * @error: Return location for error.
 *
 * Appends a #EggDBusMap to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_map    (EggDBusMessage   *message,
                                EggDBusHashMap   *map,
                                const gchar      *key_signature,
                                const gchar      *value_signature,
                                GError          **error)
{
  GValue val = {0};
  gboolean ret;
  gchar *sig;
  g_value_init (&val, EGG_DBUS_TYPE_HASH_MAP);
  g_value_take_object (&val, map);
  sig = g_strdup_printf ("a{%s%s}", key_signature, value_signature);
  ret = egg_dbus_message_append_gvalue (message, &val, sig, error);
  g_free (sig);
  return ret;
}

/**
 * egg_dbus_message_append_structure:
 * @message: A #EggDBusMessage.
 * @structure: Structure to append to @message.
 * @error: Return location for error.
 *
 * Appends a #EggDBusStructure to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_structure   (EggDBusMessage    *message,
                                     EggDBusStructure  *structure,
                                     GError         **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_STRUCTURE);
  g_value_take_object (&val, structure);
  return egg_dbus_message_append_gvalue (message,
                                         &val,
                                         egg_dbus_structure_get_signature (structure),
                                         error);
}

/**
 * egg_dbus_message_append_variant:
 * @message: A #EggDBusMessage.
 * @variant: Variant to append to @message.
 * @error: Return location for error.
 *
 * Appends a #EggDBusVariant to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_variant       (EggDBusMessage  *message,
                                     EggDBusVariant  *variant,
                                     GError       **error)
{
  GValue val = {0};
  g_value_init (&val, EGG_DBUS_TYPE_VARIANT);
  g_value_take_object (&val, variant);
  return egg_dbus_message_append_gvalue (message,
                                         &val,
                                         DBUS_TYPE_VARIANT_AS_STRING,
                                         error);
}


/**
 * egg_dbus_message_append_gvalue:
 * @message: A #EggDBusMessage.
 * @value: Value to append to @message.
 * @signature: D-Bus signature of @value.
 * @error: Return location for error.
 *
 * Appends a #GValue to @message.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_append_gvalue (EggDBusMessage   *message,
                                const GValue     *value,
                                const gchar      *signature,
                                GError          **error)
{
  EggDBusMessagePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), FALSE);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  ensure_write_iter (message);

  return egg_dbus_append_value_to_iter (&(priv->message_write_iter),
                                        signature,
                                        value,
                                        error);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_message_extract_string:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a string from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_string        (EggDBusMessage  *message,
                                        gchar                 **out_value,
                                        GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = (gchar *) g_value_get_string (&val);
  else
    g_value_unset (&val);
  return TRUE;
}

/**
 * egg_dbus_message_extract_object_path:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts an object path from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_object_path  (EggDBusMessage     *message,
                                       gchar             **out_value,
                                       GError            **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = (gchar *) g_value_get_boxed (&val);
  else
    g_value_unset (&val);
  return TRUE;
}

/**
 * egg_dbus_message_extract_signature:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a D-Bus signature from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_signature  (EggDBusMessage     *message,
                                     gchar             **out_value,
                                     GError            **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = (gchar *) g_value_get_boxed (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_string_array:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts an array of strings from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_string_array  (EggDBusMessage    *message,
                                        gchar           ***out_value,
                                        GError           **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = (gchar **) g_value_get_boxed (&val);
  else
    g_value_unset (&val);
  return TRUE;
}

/**
 * egg_dbus_message_extract_object_path_array:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts an array of object paths from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_object_path_array  (EggDBusMessage          *message,
                                             gchar                 ***out_value,
                                             GError                 **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = (gchar **) g_value_get_boxed (&val);
  else
    g_value_unset (&val);
  return TRUE;
}

/**
 * egg_dbus_message_extract_signature_array:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts an array of D-Bus signatures from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_signature_array  (EggDBusMessage          *message,
                                           gchar                 ***out_value,
                                           GError                 **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = (gchar **) g_value_get_boxed (&val);
  else
    g_value_unset (&val);
  return TRUE;
}

/**
 * egg_dbus_message_extract_byte:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #guchar from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_byte          (EggDBusMessage  *message,
                                                guchar                 *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = g_value_get_uchar (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_int16:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #gint16 from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_int16         (EggDBusMessage  *message,
                                                gint16                  *out_value,
                                                GError                 **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = egg_dbus_value_get_int16 (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_uint16:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #guint16 from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_uint16        (EggDBusMessage  *message,
                                                guint16                *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = (guint16) egg_dbus_value_get_uint16 (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_int:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #gint from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_int         (EggDBusMessage  *message,
                                                gint                   *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = g_value_get_int (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_uint:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #guint from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_uint        (EggDBusMessage  *message,
                                                guint                  *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = g_value_get_uint (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_int64:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #gint64 from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_int64         (EggDBusMessage  *message,
                                                gint64                 *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = g_value_get_int64 (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_uint64:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #guint64 from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_uint64        (EggDBusMessage  *message,
                                                guint64                *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = g_value_get_uint64 (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_boolean:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #gboolean from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_boolean       (EggDBusMessage  *message,
                                                gboolean               *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = g_value_get_boolean (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_double:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #gdouble from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_double        (EggDBusMessage  *message,
                                                gdouble                *out_value,
                                                GError                **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_value != NULL)
    *out_value = g_value_get_double (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_seq:
 * @message: A #EggDBusMessage.
 * @out_seq: Return location for extracted sequence or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #EggDBusSequence from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_seq         (EggDBusMessage    *message,
                                      EggDBusArraySeq  **out_seq,
                                      GError           **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_seq != NULL)
    *out_seq = (EggDBusArraySeq *) g_value_get_object (&val);
  else
    g_value_unset (&val);
  return TRUE;
}

/**
 * egg_dbus_message_extract_map:
 * @message: A #EggDBusMessage.
 * @out_map: Return location for extracted map or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #EggDBusHashMap from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_map    (EggDBusMessage     *message,
                                 EggDBusHashMap    **out_map,
                                 GError            **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_map != NULL)
    *out_map = (EggDBusHashMap *) g_value_get_object (&val);
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_structure:
 * @message: A #EggDBusMessage.
 * @out_structure: Return location for extracted structure or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #EggDBusStructure from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_structure     (EggDBusMessage        *message,
                                        EggDBusStructure    **out_structure,
                                        GError              **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_structure != NULL)
    *out_structure = EGG_DBUS_STRUCTURE (g_value_get_object (&val));
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_variant:
 * @message: A #EggDBusMessage.
 * @out_variant: Return location for extracted variant or %NULL.
 * @error: Return location for error.
 *
 * Extracts a #EggDBusVariant from @message and moves on to the next element.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_variant       (EggDBusMessage        *message,
                                        EggDBusVariant                **out_variant,
                                        GError                      **error)
{
  GValue val = {0};
  if (!egg_dbus_message_extract_gvalue (message, &val, error))
    return FALSE;
  if (out_variant != NULL)
    *out_variant = EGG_DBUS_VARIANT (g_value_get_object (&val));
  else
    g_value_unset (&val);
  return TRUE;
}


/**
 * egg_dbus_message_extract_gvalue:
 * @message: A #EggDBusMessage.
 * @out_value: Return location for extracted value or %NULL.
 * @error: Return location for error.
 *
 * Extracts the next complete complete type from @message as a #GValue.
 *
 * Returns: %TRUE unless @error is set.
 **/
gboolean
egg_dbus_message_extract_gvalue        (EggDBusMessage        *message,
                                        GValue                       *out_value,
                                        GError                      **error)
{
  EggDBusMessagePrivate *priv;
  gboolean ret;

  g_return_val_if_fail (EGG_DBUS_IS_MESSAGE (message), FALSE);

  priv = EGG_DBUS_MESSAGE_GET_PRIVATE (message);

  ensure_read_iter (message);

  ret = egg_dbus_get_value_from_iter (&(priv->message_read_iter),
                                      out_value,
                                      error);
  dbus_message_iter_next (&(priv->message_read_iter));
  return ret;
}


/* ---------------------------------------------------------------------------------------------------- */

static gboolean
egg_dbus_get_value_from_iter  (DBusMessageIter              *iter,
                               GValue                       *out_value,
                               GError                      **error)
{
  int arg_type;
  int array_arg_type;
  dbus_bool_t bool_val;
  const char *str_val;
  guchar uint8_val;
  dbus_int16_t int16_val;
  dbus_uint16_t uint16_val;
  dbus_int32_t int32_val;
  dbus_uint32_t uint32_val;
  dbus_int64_t int64_val;
  dbus_uint64_t uint64_val;
  double double_val;
  gboolean ret;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (out_value != NULL, FALSE);

  ret = FALSE;

  arg_type = dbus_message_iter_get_arg_type (iter);

  switch (arg_type)
    {
    case DBUS_TYPE_STRING:
      g_value_init (out_value, G_TYPE_STRING);
      dbus_message_iter_get_basic (iter, &str_val);
      g_value_set_string (out_value, str_val);
      break;

    case DBUS_TYPE_OBJECT_PATH:
      g_value_init (out_value, EGG_DBUS_TYPE_OBJECT_PATH);
      dbus_message_iter_get_basic (iter, &str_val);
      g_value_set_boxed (out_value, str_val);
      break;

    case DBUS_TYPE_SIGNATURE:
      g_value_init (out_value, EGG_DBUS_TYPE_SIGNATURE);
      dbus_message_iter_get_basic (iter, &str_val);
      g_value_set_boxed (out_value, str_val);
      break;

    case DBUS_TYPE_BOOLEAN:
      g_value_init (out_value, G_TYPE_BOOLEAN);
      dbus_message_iter_get_basic (iter, &bool_val);
      g_value_set_boolean (out_value, bool_val);
      break;

    case DBUS_TYPE_BYTE:
      g_value_init (out_value, G_TYPE_UCHAR);
      dbus_message_iter_get_basic (iter, &uint8_val);
      g_value_set_uchar (out_value, uint8_val);
      break;

    case DBUS_TYPE_INT16:
      g_value_init (out_value, EGG_DBUS_TYPE_INT16);
      dbus_message_iter_get_basic (iter, &int16_val);
      egg_dbus_value_set_int16 (out_value, int16_val);
      break;

    case DBUS_TYPE_UINT16:
      g_value_init (out_value, EGG_DBUS_TYPE_UINT16);
      dbus_message_iter_get_basic (iter, &uint16_val);
      egg_dbus_value_set_uint16 (out_value, uint16_val);
      break;

    case DBUS_TYPE_INT32:
      g_value_init (out_value, G_TYPE_INT);
      dbus_message_iter_get_basic (iter, &int32_val);
      g_value_set_int (out_value, int32_val);
      break;

    case DBUS_TYPE_UINT32:
      g_value_init (out_value, G_TYPE_UINT);
      dbus_message_iter_get_basic (iter, &uint32_val);
      g_value_set_uint (out_value, uint32_val);
      break;

    case DBUS_TYPE_INT64:
      g_value_init (out_value, G_TYPE_INT64);
      dbus_message_iter_get_basic (iter, &int64_val);
      g_value_set_int64 (out_value, int64_val);
      break;

    case DBUS_TYPE_UINT64:
      g_value_init (out_value, G_TYPE_UINT64);
      dbus_message_iter_get_basic (iter, &uint64_val);
      g_value_set_uint64 (out_value, uint64_val);
      break;

    case DBUS_TYPE_DOUBLE:
      g_value_init (out_value, G_TYPE_DOUBLE);
      dbus_message_iter_get_basic (iter, &double_val);
      g_value_set_double (out_value, double_val);
      break;

    case DBUS_TYPE_STRUCT:
      {
        DBusMessageIter struct_iter;
        char *struct_signature;
        EggDBusStructure *struct_instance;
        GArray *elem_values;
        GValue *elems;

        struct_signature = dbus_message_iter_get_signature (iter);

        elem_values = g_array_new (FALSE,
                                   FALSE,
                                   sizeof (GValue));

        dbus_message_iter_recurse (iter, &struct_iter);

        /* now collect all the elements in the structure as GValue objects */
        while (dbus_message_iter_get_arg_type (&struct_iter) != DBUS_TYPE_INVALID)
          {
            GValue elem_value = {0};
            /* doing the recursive dance! */
            if (!egg_dbus_get_value_from_iter (&struct_iter, &elem_value, error))
              goto out;

            g_array_append_val (elem_values, elem_value);

            dbus_message_iter_next (&struct_iter);
          }

        elems = (GValue *) g_array_free (elem_values, FALSE);

        /* construct() takes ownership of elems */
        struct_instance = egg_dbus_structure_new (struct_signature,
                                                  elems);

        g_value_init (out_value, EGG_DBUS_TYPE_STRUCTURE);
        g_value_take_object (out_value, struct_instance);

        dbus_free (struct_signature);
      }
      break;

    case DBUS_TYPE_ARRAY:
      array_arg_type = dbus_message_iter_get_element_type (iter);
      if (array_arg_type == DBUS_TYPE_STRING ||
          array_arg_type == DBUS_TYPE_OBJECT_PATH ||
          array_arg_type == DBUS_TYPE_SIGNATURE)
        {
          GPtrArray *p;
          DBusMessageIter array_iter;
          GType boxed_type;

          if (array_arg_type == DBUS_TYPE_STRING)
            boxed_type = G_TYPE_STRV;
          else if (array_arg_type == DBUS_TYPE_OBJECT_PATH)
            boxed_type = EGG_DBUS_TYPE_OBJECT_PATH_ARRAY;
          else
            boxed_type = EGG_DBUS_TYPE_SIGNATURE_ARRAY;

          dbus_message_iter_recurse (iter, &array_iter);
          p = g_ptr_array_new ();
          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              dbus_message_iter_get_basic (&array_iter, &str_val);
              g_ptr_array_add (p, (void *) g_strdup (str_val));
              dbus_message_iter_next (&array_iter);
            }
          g_ptr_array_add (p, NULL);
          g_value_init (out_value, boxed_type);
          g_value_take_boxed (out_value, p->pdata);
          g_ptr_array_free (p, FALSE);
        }
      else if (array_arg_type == DBUS_TYPE_BYTE ||
               array_arg_type == DBUS_TYPE_INT16 ||
               array_arg_type == DBUS_TYPE_UINT16 ||
               array_arg_type == DBUS_TYPE_INT32 ||
               array_arg_type == DBUS_TYPE_UINT32 ||
               array_arg_type == DBUS_TYPE_INT64 ||
               array_arg_type == DBUS_TYPE_UINT64 ||
               array_arg_type == DBUS_TYPE_BOOLEAN ||
               array_arg_type == DBUS_TYPE_DOUBLE)
        {
          EggDBusArraySeq *a;
          DBusMessageIter array_iter;
          char *elem_signature;
          GType elem_type;

          switch (array_arg_type)
            {
            case DBUS_TYPE_BYTE:
              elem_type = G_TYPE_UCHAR;
              break;

            case DBUS_TYPE_INT16:
              elem_type = EGG_DBUS_TYPE_INT16;
              break;

            case DBUS_TYPE_UINT16:
              elem_type = EGG_DBUS_TYPE_UINT16;
              break;

            case DBUS_TYPE_INT32:
              elem_type = G_TYPE_UINT;
              break;

            case DBUS_TYPE_UINT32:
              elem_type = G_TYPE_INT;
              break;

            case DBUS_TYPE_INT64:
              elem_type = G_TYPE_INT64;
              break;

            case DBUS_TYPE_UINT64:
              elem_type = G_TYPE_UINT64;
              break;

            case DBUS_TYPE_BOOLEAN:
              elem_type = G_TYPE_BOOLEAN;
              break;

            case DBUS_TYPE_DOUBLE:
              elem_type = G_TYPE_DOUBLE;
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          dbus_message_iter_recurse (iter, &array_iter);

          elem_signature = dbus_message_iter_get_signature (&array_iter);

          a = egg_dbus_array_seq_new (elem_type, NULL, NULL, NULL);
          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              switch (array_arg_type)
                {
                case DBUS_TYPE_BYTE:
                  dbus_message_iter_get_basic (&array_iter, &uint8_val);
                  egg_dbus_array_seq_add_fixed (a, uint8_val);
                  break;

                case DBUS_TYPE_INT16:
                  dbus_message_iter_get_basic (&array_iter, &int16_val);
                  egg_dbus_array_seq_add_fixed (a, int16_val);
                  break;

                case DBUS_TYPE_UINT16:
                  dbus_message_iter_get_basic (&array_iter, &uint16_val);
                  egg_dbus_array_seq_add_fixed (a, uint16_val);
                  break;

                case DBUS_TYPE_INT32:
                  dbus_message_iter_get_basic (&array_iter, &int32_val);
                  egg_dbus_array_seq_add_fixed (a, int32_val);
                  break;

                case DBUS_TYPE_UINT32:
                  dbus_message_iter_get_basic (&array_iter, &uint32_val);
                  egg_dbus_array_seq_add_fixed (a, uint32_val);
                  break;

                case DBUS_TYPE_INT64:
                  dbus_message_iter_get_basic (&array_iter, &int64_val);
                  egg_dbus_array_seq_add_fixed (a, int64_val);
                  break;

                case DBUS_TYPE_UINT64:
                  dbus_message_iter_get_basic (&array_iter, &uint64_val);
                  egg_dbus_array_seq_add_fixed (a, uint64_val);
                  break;

                case DBUS_TYPE_BOOLEAN:
                  dbus_message_iter_get_basic (&array_iter, &bool_val);
                  egg_dbus_array_seq_add_fixed (a, bool_val);
                  break;

                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_get_basic (&array_iter, &double_val);
                  egg_dbus_array_seq_add_float (a, double_val);
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }
              dbus_message_iter_next (&array_iter);
            }
          g_value_init (out_value, EGG_DBUS_TYPE_ARRAY_SEQ);
          g_value_take_object (out_value, a);
          dbus_free (elem_signature);
        }
      else if (array_arg_type == DBUS_TYPE_STRUCT)
        {
          DBusMessageIter array_iter;
          char *struct_signature;
          EggDBusArraySeq *seq;

          seq = egg_dbus_array_seq_new (EGG_DBUS_TYPE_STRUCTURE,
                                        g_object_unref,
                                        NULL,
                                        NULL); /* TODO: have equal_func for structures */

          dbus_message_iter_recurse (iter, &array_iter);

          struct_signature = dbus_message_iter_get_signature (&array_iter);

          /* now collect all the elements in the structure.
           */
          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              GValue elem_value = {0};

              /* recurse */
              if (!egg_dbus_get_value_from_iter (&array_iter, &elem_value, error))
                {
                  dbus_free (struct_signature);
                  g_object_unref (seq);
                  goto out;
                }

              egg_dbus_array_seq_add (seq, g_value_get_object (&elem_value));

              dbus_message_iter_next (&array_iter);
            }

          g_value_init (out_value, EGG_DBUS_TYPE_ARRAY_SEQ);
          g_value_take_object (out_value, seq);

          dbus_free (struct_signature);
        }
      else if (array_arg_type == DBUS_TYPE_DICT_ENTRY)
        {
          DBusMessageIter array_iter;
          EggDBusHashMap *hash_map;
          char key_sig[2];
          char *val_sig;
          char *array_sig;
          GDestroyNotify key_free_func;
          GDestroyNotify val_free_func;
          GType key_type;
          GType val_type;

          dbus_message_iter_recurse (iter, &array_iter);

          array_sig = dbus_message_iter_get_signature (&array_iter);

          /* keys are guaranteed by the D-Bus spec to be primitive types */

          key_sig[0] = array_sig[1];
          key_sig[1] = '\0';
          val_sig = g_strdup (array_sig + 2);
          val_sig[strlen (val_sig) - 1] = '\0';

          key_type = egg_dbus_get_type_for_signature (key_sig);
          val_type = egg_dbus_get_type_for_signature (val_sig);

          //g_warning ("key_type=%c val_sig='%s'", key_type, val_sig);

          /* set up the hash table */

          switch (key_sig[0])
            {
            case DBUS_TYPE_BOOLEAN:
            case DBUS_TYPE_BYTE:
            case DBUS_TYPE_INT16:
            case DBUS_TYPE_UINT16:
            case DBUS_TYPE_INT32:
            case DBUS_TYPE_UINT32:
            case DBUS_TYPE_INT64:
            case DBUS_TYPE_UINT64:
            case DBUS_TYPE_DOUBLE:
              key_free_func = NULL;
              break;

            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
            case DBUS_TYPE_SIGNATURE:
              key_free_func = g_free;
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          switch (val_sig[0])
            {
            case DBUS_TYPE_BOOLEAN:
            case DBUS_TYPE_BYTE:
            case DBUS_TYPE_INT16:
            case DBUS_TYPE_UINT16:
            case DBUS_TYPE_INT32:
            case DBUS_TYPE_UINT32:
            case DBUS_TYPE_INT64:
            case DBUS_TYPE_UINT64:
            case DBUS_TYPE_DOUBLE:
              val_free_func = NULL;
              break;

            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
            case DBUS_TYPE_SIGNATURE:
              val_free_func = g_free;
              break;

            case DBUS_TYPE_ARRAY:
              switch (val_sig[1])
                {
                case DBUS_TYPE_STRING:
                case DBUS_TYPE_OBJECT_PATH:
                case DBUS_TYPE_SIGNATURE:
                  val_free_func = (GDestroyNotify) g_strfreev;
                  break;

                case DBUS_TYPE_BOOLEAN:
                case DBUS_TYPE_BYTE:
                case DBUS_TYPE_INT16:
                case DBUS_TYPE_UINT16:
                case DBUS_TYPE_INT32:
                case DBUS_TYPE_UINT32:
                case DBUS_TYPE_INT64:
                case DBUS_TYPE_UINT64:
                case DBUS_TYPE_DOUBLE:
                case DBUS_STRUCT_BEGIN_CHAR:
                case DBUS_DICT_ENTRY_BEGIN_CHAR:
                  val_free_func = (GDestroyNotify) g_object_unref;
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }
              break;

            case DBUS_STRUCT_BEGIN_CHAR:
              val_free_func = g_object_unref;
              break;

            case DBUS_TYPE_VARIANT:
              val_free_func = g_object_unref;
              break;

            default:
              g_assert_not_reached ();
              break;
            }

          hash_map = egg_dbus_hash_map_new (key_type, key_free_func,
                                            val_type, val_free_func);

          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              DBusMessageIter hash_iter;
              gpointer key, value;
              const char *str_val;

              dbus_message_iter_recurse (&array_iter, &hash_iter);

              switch (key_sig[0])
                {
                case DBUS_TYPE_BOOLEAN:
                  dbus_message_iter_get_basic (&hash_iter, &bool_val);
                  key = GINT_TO_POINTER (bool_val);
                  break;

                case DBUS_TYPE_BYTE:
                  dbus_message_iter_get_basic (&hash_iter, &uint8_val);
                  key = GINT_TO_POINTER (uint8_val);
                  break;

                case DBUS_TYPE_INT16:
                  dbus_message_iter_get_basic (&hash_iter, &int16_val);
                  key = GINT_TO_POINTER (int16_val);
                  break;

                case DBUS_TYPE_UINT16:
                  dbus_message_iter_get_basic (&hash_iter, &uint16_val);
                  key = GINT_TO_POINTER (uint16_val);
                  break;

                case DBUS_TYPE_INT32:
                  dbus_message_iter_get_basic (&hash_iter, &int32_val);
                  key = GINT_TO_POINTER (int32_val);
                  break;

                case DBUS_TYPE_UINT32:
                  dbus_message_iter_get_basic (&hash_iter, &uint32_val);
                  key = GINT_TO_POINTER (uint32_val);
                  break;

                case DBUS_TYPE_INT64:
                  dbus_message_iter_get_basic (&hash_iter, &int64_val);
                  key = g_memdup (&int64_val, sizeof (gint64));
                  break;

                case DBUS_TYPE_UINT64:
                  dbus_message_iter_get_basic (&hash_iter, &uint64_val);
                  key = g_memdup (&uint64_val, sizeof (guint64));
                  break;

                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_get_basic (&hash_iter, &double_val);
                  key = g_memdup (&double_val, sizeof (gdouble));
                  break;

                case DBUS_TYPE_STRING:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  key = g_strdup (str_val);
                  break;

                case DBUS_TYPE_OBJECT_PATH:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  key = g_strdup (str_val);
                  break;

                case DBUS_TYPE_SIGNATURE:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  key = g_strdup (str_val);
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }

              dbus_message_iter_next (&hash_iter);

              switch (val_sig[0])
                {
                case DBUS_TYPE_BOOLEAN:
                  dbus_message_iter_get_basic (&hash_iter, &bool_val);
                  value = GINT_TO_POINTER (bool_val);
                  break;

                case DBUS_TYPE_BYTE:
                  dbus_message_iter_get_basic (&hash_iter, &uint8_val);
                  value = GINT_TO_POINTER (uint8_val);
                  break;

                case DBUS_TYPE_INT16:
                  dbus_message_iter_get_basic (&hash_iter, &int16_val);
                  value = GINT_TO_POINTER (int16_val);
                  break;

                case DBUS_TYPE_UINT16:
                  dbus_message_iter_get_basic (&hash_iter, &uint16_val);
                  value = GINT_TO_POINTER (uint16_val);
                  break;

                case DBUS_TYPE_INT32:
                  dbus_message_iter_get_basic (&hash_iter, &int32_val);
                  value = GINT_TO_POINTER (int32_val);
                  break;

                case DBUS_TYPE_INT64:
                  dbus_message_iter_get_basic (&hash_iter, &int64_val);
                  value = g_memdup (&int64_val, sizeof (gint64));
                  break;

                case DBUS_TYPE_UINT64:
                  dbus_message_iter_get_basic (&hash_iter, &uint64_val);
                  value = g_memdup (&uint64_val, sizeof (guint64));
                  break;

                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_get_basic (&hash_iter, &double_val);
                  value = g_memdup (&double_val, sizeof (gdouble));
                  break;

                case DBUS_TYPE_UINT32:
                  dbus_message_iter_get_basic (&hash_iter, &uint32_val);
                  value = GINT_TO_POINTER (uint32_val);
                  break;

                case DBUS_TYPE_STRING:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  value = g_strdup (str_val);
                  break;

                case DBUS_TYPE_OBJECT_PATH:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  value = g_strdup (str_val);
                  break;

                case DBUS_TYPE_SIGNATURE:
                  dbus_message_iter_get_basic (&hash_iter, &str_val);
                  value = g_strdup (str_val);
                  break;

                case DBUS_TYPE_ARRAY:
                  {
                    GValue array_val = {0};
                    /* recurse */
                    if (!egg_dbus_get_value_from_iter (&hash_iter, &array_val, error))
                      goto out;
                    switch (val_sig[1])
                      {
                      case DBUS_TYPE_BOOLEAN:
                      case DBUS_TYPE_BYTE:
                      case DBUS_TYPE_INT16:
                      case DBUS_TYPE_UINT16:
                      case DBUS_TYPE_INT32:
                      case DBUS_TYPE_UINT32:
                      case DBUS_TYPE_INT64:
                      case DBUS_TYPE_UINT64:
                      case DBUS_TYPE_DOUBLE:
                      case DBUS_STRUCT_BEGIN_CHAR:
                      case DBUS_DICT_ENTRY_BEGIN_CHAR:
                        value = g_value_get_object (&array_val);
                        break;

                      case DBUS_TYPE_STRING:
                      case DBUS_TYPE_OBJECT_PATH:
                      case DBUS_TYPE_SIGNATURE:
                        value = g_value_get_boxed (&array_val);
                        break;

                      default:
                        g_assert_not_reached ();
                        break;
                      }
                  }
                  break;

                case DBUS_STRUCT_BEGIN_CHAR:
                  {
                    GValue object_val = {0};
                    /* recurse */
                    if (!egg_dbus_get_value_from_iter (&hash_iter, &object_val, error))
                      goto out;
                    value = g_value_get_object (&object_val);
                  }
                  break;

                case DBUS_TYPE_VARIANT:
                  {
                    GValue object_val = {0};
                    /* recurse */
                    if (!egg_dbus_get_value_from_iter (&hash_iter, &object_val, error))
                      goto out;
                    value = g_value_get_object (&object_val);
                  }
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }

              egg_dbus_hash_map_insert (hash_map, key, value);

              dbus_message_iter_next (&array_iter);
            }

          g_value_init (out_value, EGG_DBUS_TYPE_HASH_MAP);
          g_value_take_object (out_value, hash_map);

          dbus_free (array_sig);
          g_free (val_sig);

        }
      else if (array_arg_type == DBUS_TYPE_ARRAY)
        {
          EggDBusArraySeq *seq;
          DBusMessageIter array_iter;
          char *elem_signature;
          GType elem_type;
          GDestroyNotify elem_free_func;

          /* handling array of arrays, e.g.
           *
           * - aas:    EggDBusArraySeq of GStrv (gchar **)
           * - aao:    EggDBusArraySeq of EggDBusObjectPathArray (gchar **)
           * - aao:    EggDBusArraySeq of EggDBusSignatureArray (gchar **)
           * - aa{ss}: EggDBusArraySeq of EggDBusHashMap
           * - aai:    EggDBusArraySeq of EggDBusArraySeq
           * - aa(ii): EggDBusArraySeq of EggDBusArraySeq containg GObject-derived type
           * - aaas:   EggDBusArraySeq of EggDBusArraySeq of gchar**
           *
           */

          dbus_message_iter_recurse (iter, &array_iter);

          elem_signature = dbus_message_iter_get_signature (&array_iter);

          elem_type = egg_dbus_get_type_for_signature (elem_signature);
          if (elem_type == G_TYPE_STRING ||
              elem_type == EGG_DBUS_TYPE_OBJECT_PATH ||
              elem_type == EGG_DBUS_TYPE_SIGNATURE)
            {
              elem_free_func = g_free;
            }
          else if (elem_type == G_TYPE_STRV ||
                   elem_type == EGG_DBUS_TYPE_OBJECT_PATH_ARRAY ||
                   elem_type == EGG_DBUS_TYPE_SIGNATURE_ARRAY)
            {
              elem_free_func = (GDestroyNotify) g_strfreev;
            }
          else
            {
              elem_free_func = g_object_unref;
            }

          seq = egg_dbus_array_seq_new (elem_type,
                                        elem_free_func,
                                        NULL,         /* use default copy_func */
                                        NULL);        /* use default equal_func */

          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              GValue elem_val = {0};

              /* recurse */
              if (!egg_dbus_get_value_from_iter (&array_iter, &elem_val, error))
                goto out;

              if (elem_type == G_TYPE_STRING)
                {
                  egg_dbus_array_seq_add (seq, g_value_get_string (&elem_val));
                }
              else if (elem_type == EGG_DBUS_TYPE_OBJECT_PATH ||
                       elem_type == EGG_DBUS_TYPE_SIGNATURE ||
                       elem_type == G_TYPE_STRV ||
                       elem_type == EGG_DBUS_TYPE_OBJECT_PATH_ARRAY ||
                       elem_type == EGG_DBUS_TYPE_SIGNATURE_ARRAY)
                {
                  egg_dbus_array_seq_add (seq, g_value_get_boxed (&elem_val));
                }
              else
                {
                  egg_dbus_array_seq_add (seq, g_value_get_object (&elem_val));
                }

              dbus_message_iter_next (&array_iter);
            } /* for all elements in array */

          g_value_init (out_value, EGG_DBUS_TYPE_ARRAY_SEQ);
          g_value_take_object (out_value, seq);

          dbus_free (elem_signature);
        }
      else if (array_arg_type == DBUS_TYPE_VARIANT)
        {
          EggDBusArraySeq *seq;
          DBusMessageIter array_iter;
          char *elem_signature;

          /* array of variants */

          dbus_message_iter_recurse (iter, &array_iter);

          elem_signature = dbus_message_iter_get_signature (&array_iter);

          seq = egg_dbus_array_seq_new (EGG_DBUS_TYPE_VARIANT,
                                        g_object_unref,
                                        NULL,
                                        NULL); /* TODO: equal_func */

          while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID)
            {
              GValue elem_val = {0};

              /* recurse */
              if (!egg_dbus_get_value_from_iter (&array_iter, &elem_val, error))
                goto out;

              egg_dbus_array_seq_add (seq, g_value_get_object (&elem_val));

              dbus_message_iter_next (&array_iter);
            } /* for all elements in array */

          g_value_init (out_value, EGG_DBUS_TYPE_ARRAY_SEQ);
          g_value_take_object (out_value, seq);

          dbus_free (elem_signature);
        }
      else
        {
          char *signature;
          signature = dbus_message_iter_get_signature (iter);
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "Cannot decode message with array of signature %s", signature);
          g_free (signature);
          goto out;
        }
      break;

    case DBUS_TYPE_VARIANT:
      {
        GValue variant_val = {0};
        EggDBusVariant *variant;
        DBusMessageIter variant_iter;
        gchar *variant_signature;

        dbus_message_iter_recurse (iter, &variant_iter);

        variant_signature = dbus_message_iter_get_signature (&variant_iter);

        /* recurse */
        if (!egg_dbus_get_value_from_iter (&variant_iter, &variant_val, error))
          goto out;

        variant = egg_dbus_variant_new_for_gvalue (&variant_val, variant_signature);

        g_value_init (out_value, EGG_DBUS_TYPE_VARIANT);
        g_value_take_object (out_value, variant);

        dbus_free (variant_signature);
      }
      break;

    default:
      {
        char *signature;
        signature = dbus_message_iter_get_signature (iter);
        g_set_error (error,
                     EGG_DBUS_ERROR,
                     EGG_DBUS_ERROR_FAILED,
                     "Cannot decode message with signature %s", signature);
        g_free (signature);
        goto out;
      }
      break;
    }

  ret = TRUE;

 out:

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
egg_dbus_append_value_to_iter (DBusMessageIter              *iter,
                               const gchar                  *signature,
                               const GValue                 *value,
                               GError                      **error)
{
  gboolean ret;

  //g_debug ("_append_value_to_iter: signature=%s -> value=%s", signature, g_strdup_value_contents (value));

  ret = FALSE;

  /* TODO: we could probably speed this up a great bit by accessing the GValue directly */
  if (strcmp (signature, DBUS_TYPE_STRING_AS_STRING) == 0)
    {
      const char *val = g_value_get_string (value);
      if (val == NULL)
        val = "";
      dbus_message_iter_append_basic (iter, DBUS_TYPE_STRING, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_OBJECT_PATH_AS_STRING) == 0)
    {
      const char *val = g_value_get_boxed (value);
      if (val == NULL)
        val = "";
      dbus_message_iter_append_basic (iter, DBUS_TYPE_OBJECT_PATH, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_SIGNATURE_AS_STRING) == 0)
    {
      const char *val = g_value_get_boxed (value);
      if (val == NULL)
        val = "";
      dbus_message_iter_append_basic (iter, DBUS_TYPE_SIGNATURE, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_BYTE_AS_STRING) == 0)
    {
      guchar val = g_value_get_uchar (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_BYTE, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_BOOLEAN_AS_STRING) == 0)
    {
      dbus_bool_t val = g_value_get_boolean (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_BOOLEAN, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_INT16_AS_STRING) == 0)
    {
      dbus_int16_t val = egg_dbus_value_get_int16 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT16, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_UINT16_AS_STRING) == 0)
    {
      dbus_uint16_t val = egg_dbus_value_get_uint16 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT16, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_INT32_AS_STRING) == 0)
    {
      dbus_int32_t val = g_value_get_int (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT32, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_UINT32_AS_STRING) == 0)
    {
      dbus_uint32_t val;
      if (value->g_type == G_TYPE_UINT)
        {
          val = g_value_get_uint (value);
        }
      else if (G_TYPE_IS_ENUM (value->g_type))
        {
          val = g_value_get_enum (value);
        }
      else if (G_TYPE_IS_FLAGS (value->g_type))
        {
          val = g_value_get_flags (value);
        }
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT32, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_INT64_AS_STRING) == 0)
    {
      dbus_int64_t val = g_value_get_int64 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT64, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_UINT64_AS_STRING) == 0)
    {
      dbus_uint64_t val = g_value_get_uint64 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT64, &val);
    }
  else if (strcmp (signature, DBUS_TYPE_DOUBLE_AS_STRING) == 0)
    {
      double val = g_value_get_double (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_DOUBLE, &val);
    }
  else if (g_str_has_prefix (signature, DBUS_TYPE_ARRAY_AS_STRING))
    {
      if (signature[1] == DBUS_TYPE_BYTE   ||
          signature[1] == DBUS_TYPE_INT16  ||
          signature[1] == DBUS_TYPE_UINT16 ||
          signature[1] == DBUS_TYPE_INT32  ||
          signature[1] == DBUS_TYPE_UINT32 ||
          signature[1] == DBUS_TYPE_INT64  ||
          signature[1] == DBUS_TYPE_UINT64 ||
          signature[1] == DBUS_TYPE_DOUBLE ||
          signature[1] == DBUS_TYPE_BOOLEAN)
        {
          DBusMessageIter array_iter;
          EggDBusArraySeq *seq;

          seq = EGG_DBUS_ARRAY_SEQ (g_value_get_object (value));
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          dbus_message_iter_append_fixed_array (&array_iter, signature[1], &(seq->data.data), seq->size);
          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_TYPE_STRING ||
               signature[1] == DBUS_TYPE_OBJECT_PATH ||
               signature[1] == DBUS_TYPE_SIGNATURE)
        {
          DBusMessageIter array_iter;
          gchar **strv;
          guint n;

          strv = (gchar **) g_value_get_boxed (value);
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; strv[n] != NULL; n++)
            dbus_message_iter_append_basic (&array_iter, signature[1], &(strv[n]));
          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_STRUCT_BEGIN_CHAR)
        {
          DBusMessageIter array_iter;
          EggDBusArraySeq *seq;
          guint n;

          seq = EGG_DBUS_ARRAY_SEQ (g_value_get_object (value));
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; n < seq->size; n++)
            {
              GValue val = {0};
              g_value_init (&val, EGG_DBUS_TYPE_STRUCTURE);
              g_value_take_object (&val, EGG_DBUS_STRUCTURE (seq->data.v_ptr[n]));
              /* recursive dance */
              if (!egg_dbus_append_value_to_iter (&array_iter, signature + 1, &val, error))
                goto out;
            }
          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
        {
          DBusMessageIter array_iter;
          EggDBusHashMap *hash_map;
          GHashTable *hash_table;
          GHashTableIter hash_iter;
          gpointer hash_key;
          gpointer hash_value;
          char *value_signature;

          value_signature = g_strdup (signature + 3);
          value_signature[strlen (value_signature) - 1] = '\0';

          hash_map = EGG_DBUS_HASH_MAP (g_value_get_object (value));
          hash_table = hash_map->data;

          //g_debug ("signature='%s' value_signature='%s'", signature, value_signature);

          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          g_hash_table_iter_init (&hash_iter, hash_table);
          /* TODO: handle passing in NULL for an empty hash table */
          while (g_hash_table_iter_next (&hash_iter, &hash_key, &hash_value))
            {
              GValue val = {0};
              DBusMessageIter dict_iter;

              dbus_message_iter_open_container (&array_iter,
                                                DBUS_TYPE_DICT_ENTRY,
                                                NULL,
                                                &dict_iter);
              switch (signature[2])
                {
                case DBUS_TYPE_STRING:
                case DBUS_TYPE_OBJECT_PATH:
                case DBUS_TYPE_SIGNATURE:
                case DBUS_TYPE_BYTE:
                case DBUS_TYPE_BOOLEAN:
                case DBUS_TYPE_INT16:
                case DBUS_TYPE_UINT16:
                case DBUS_TYPE_INT32:
                case DBUS_TYPE_UINT32:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], &hash_key);
                  break;

                case DBUS_TYPE_INT64:
                case DBUS_TYPE_UINT64:
                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], hash_key);
                  break;

                default:
                  /* the D-Bus spec says the hash_key must be a basic type */
                  g_assert_not_reached ();
                  break;
                }

              //g_debug ("value_signature='%s'", value_signature);
              switch (value_signature[0])
                {
                case DBUS_TYPE_STRING:
                case DBUS_TYPE_OBJECT_PATH:
                case DBUS_TYPE_SIGNATURE:
                case DBUS_TYPE_BYTE:
                case DBUS_TYPE_BOOLEAN:
                case DBUS_TYPE_INT16:
                case DBUS_TYPE_UINT16:
                case DBUS_TYPE_INT32:
                case DBUS_TYPE_UINT32:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], &hash_value);
                  break;

                case DBUS_TYPE_INT64:
                case DBUS_TYPE_UINT64:
                case DBUS_TYPE_DOUBLE:
                  dbus_message_iter_append_basic (&dict_iter, signature[2], hash_value);
                  break;

                case DBUS_STRUCT_BEGIN_CHAR:
                  g_value_init (&val, G_TYPE_OBJECT);
                  g_value_take_object (&val, G_OBJECT (hash_value));
                  /* recursive dance */
                  if (!egg_dbus_append_value_to_iter (&dict_iter, value_signature, &val, error))
                    goto out;
                  break;

                case DBUS_TYPE_ARRAY:
                  if (value_signature[1] == DBUS_TYPE_BYTE ||
                      value_signature[1] == DBUS_TYPE_INT16 ||
                      value_signature[1] == DBUS_TYPE_UINT16 ||
                      value_signature[1] == DBUS_TYPE_INT32 ||
                      value_signature[1] == DBUS_TYPE_UINT32 ||
                      value_signature[1] == DBUS_TYPE_INT64 ||
                      value_signature[1] == DBUS_TYPE_UINT64 ||
                      value_signature[1] == DBUS_TYPE_BOOLEAN ||
                      value_signature[1] == DBUS_TYPE_DOUBLE)
                    {
                      g_value_init (&val, EGG_DBUS_TYPE_ARRAY_SEQ);
                      g_value_take_object (&val, hash_value);
                      /* recurse */
                      if (!egg_dbus_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_TYPE_STRING ||
                           value_signature[1] == DBUS_TYPE_OBJECT_PATH ||
                           value_signature[1] == DBUS_TYPE_SIGNATURE)
                    {
                      GType boxed_type;

                      if (value_signature[1] == DBUS_TYPE_STRING)
                        boxed_type = G_TYPE_STRV;
                      else if (value_signature[1] == DBUS_TYPE_OBJECT_PATH)
                        boxed_type = EGG_DBUS_TYPE_OBJECT_PATH_ARRAY;
                      else if (value_signature[1] == DBUS_TYPE_SIGNATURE)
                        boxed_type = EGG_DBUS_TYPE_SIGNATURE_ARRAY;
                      else
                        g_assert_not_reached ();

                      g_value_init (&val, boxed_type);
                      g_value_set_static_boxed (&val, hash_value);
                      /* recurse */
                      if (!egg_dbus_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_STRUCT_BEGIN_CHAR)
                    {
                      g_value_init (&val, EGG_DBUS_TYPE_ARRAY_SEQ);
                      g_value_take_object (&val, hash_value);
                      /* recurse */
                      if (!egg_dbus_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
                    {
                      g_value_init (&val, EGG_DBUS_TYPE_HASH_MAP);
                      g_value_take_object (&val, hash_value);
                      /* recurse */
                      if (!egg_dbus_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else if (value_signature[1] == DBUS_TYPE_VARIANT)
                    {
                      g_value_init (&val, EGG_DBUS_TYPE_ARRAY_SEQ);
                      g_value_take_object (&val, hash_value);
                      /* recurse */
                      if (!egg_dbus_append_value_to_iter (&dict_iter, value_signature, &val, error))
                        goto out;
                    }
                  else
                    {
                      g_assert_not_reached ();
                    }
                  break;

                case DBUS_TYPE_VARIANT:
                  g_value_init (&val, EGG_DBUS_TYPE_VARIANT);
                  g_value_take_object (&val, hash_value);
                  /* recurse */
                  if (!egg_dbus_append_value_to_iter (&dict_iter, value_signature, &val, error))
                    goto out;
                  break;

                default:
                  g_assert_not_reached ();
                  break;
                }

              dbus_message_iter_close_container (&array_iter, &dict_iter);
            }
          dbus_message_iter_close_container (iter, &array_iter);

          g_free (value_signature);
        }
      else if (signature[1] == DBUS_TYPE_ARRAY) /* array of an array */
        {
          DBusMessageIter array_iter;
          EggDBusArraySeq *seq;
          guint n;

          seq = EGG_DBUS_ARRAY_SEQ (g_value_get_object (value));
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; n < seq->size; n++)
            {
              GValue val = {0};

              g_value_init (&val, egg_dbus_get_type_for_signature (signature + 1));

              /* this can either be boxed (for GStrv et. al.) or an object (for e.g. structs, seqs, maps) */
              if (g_type_is_a (seq->element_type, G_TYPE_BOXED))
                g_value_take_boxed (&val, seq->data.v_ptr[n]);
              else if (g_type_is_a (seq->element_type, G_TYPE_OBJECT))
                g_value_take_object (&val, seq->data.v_ptr[n]);
              else
                g_assert_not_reached ();


              /* recursive dance */
              if (!egg_dbus_append_value_to_iter (&array_iter, signature + 1, &val, error))
                goto out;
            }

          dbus_message_iter_close_container (iter, &array_iter);
        }
      else if (signature[1] == DBUS_TYPE_VARIANT ) /* array of variants */
        {
          DBusMessageIter array_iter;
          EggDBusArraySeq *seq;
          guint n;

          seq = EGG_DBUS_ARRAY_SEQ (g_value_get_object (value));
          dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY, signature + 1, &array_iter);
          for (n = 0; n < seq->size; n++)
            {
              GValue val = {0};

              g_value_init (&val, EGG_DBUS_TYPE_VARIANT);
              g_value_take_object (&val, seq->data.v_ptr[n]);

              /* recursive dance */
              if (!egg_dbus_append_value_to_iter (&array_iter, signature + 1, &val, error))
                goto out;
            }

          dbus_message_iter_close_container (iter, &array_iter);
        }
      else
        {
          g_warning ("xxx signature '%s'", signature);
          g_assert_not_reached ();
        }
    }
  else if (g_str_has_prefix (signature, DBUS_STRUCT_BEGIN_CHAR_AS_STRING))
    {
      DBusMessageIter struct_iter;
      EggDBusStructure *structure;
      guint n;
      guint num_elems;

      structure = EGG_DBUS_STRUCTURE (g_value_get_object (value));

      num_elems = egg_dbus_structure_get_num_elements (structure);

      dbus_message_iter_open_container (iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
      for (n = 0; n < num_elems; n++)
        {
          GValue val = {0};
          const gchar *sig_for_elem;

          egg_dbus_structure_get_element_as_gvalue (structure, n, &val);
          sig_for_elem = egg_dbus_structure_get_signature_for_element (structure, n);

          /* recurse */
          if (!egg_dbus_append_value_to_iter (&struct_iter, sig_for_elem, &val, error))
            {
              g_value_unset (&val);
              goto out;
            }

          g_value_unset (&val);
        }
      dbus_message_iter_close_container (iter, &struct_iter);
    }
  else if (g_str_has_prefix (signature, DBUS_TYPE_VARIANT_AS_STRING))
    {
      EggDBusVariant *variant;
      DBusMessageIter variant_iter;
      const gchar *variant_signature;
      GValue val = {0};

      g_value_init (&val, EGG_DBUS_TYPE_VARIANT);
      variant = g_value_get_object (value);

      variant_signature = egg_dbus_variant_get_variant_signature (variant);

      dbus_message_iter_open_container (iter, DBUS_TYPE_VARIANT, variant_signature, &variant_iter);
      /* recurse */
      if (!egg_dbus_append_value_to_iter (&variant_iter,
                                        variant_signature,
                                        egg_dbus_variant_get_gvalue (variant),
                                        error))
        goto out;
      dbus_message_iter_close_container (iter, &variant_iter);
    }
  else
    {
      g_warning ("signature '%s'", signature);
      g_assert_not_reached ();
    }

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */
