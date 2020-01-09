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
#include <string.h>
#include <eggdbus/eggdbustypes.h>
#include <eggdbus/eggdbusmethodinvocation.h>
#include <eggdbus/eggdbusmessage.h>
#include <eggdbus/eggdbusutils.h>
#include <eggdbus/eggdbusconnection.h>
#include <eggdbus/eggdbusprivate.h>

/**
 * SECTION:eggdbusmethodinvocation
 * @title: EggDBusMethodInvocation
 * @short_description: Handling remote method calls
 *
 * Instances of the #EggDBusMethodInvocation class are used when handling D-Bus method calls. It
 * provides a way to get information (such as the UNIX process identifier if applicable) about
 * the remote end invoking the method. It also provides a mechanism to return errors.
 */

typedef struct
{
  EggDBusMessage *request_message;

  gpointer source_tag;

  /* A list of pointers
   *
   *  func0, data0, func1, data1, ...
   *
   * that will be used when finalizing the EggDBusMethodInvocation instance
   *
   *  func0 (data0);
   *  func1 (data1);
   *  ...
   *
   * See egg_dbus_method_invocation_add_destroy_notify() for details.
   */
  GSList *destroy_notifiers;

} EggDBusMethodInvocationPrivate;

#define EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_METHOD_INVOCATION, EggDBusMethodInvocationPrivate))

G_DEFINE_TYPE (EggDBusMethodInvocation, egg_dbus_method_invocation, G_TYPE_OBJECT);

static void
egg_dbus_method_invocation_init (EggDBusMethodInvocation *method_invocation)
{
}

static void
egg_dbus_method_invocation_finalize (GObject *object)
{
  EggDBusMethodInvocationPrivate *priv;
  GSList *l;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (object);

  if (priv->request_message != NULL)
    g_object_unref (priv->request_message);

  for (l = priv->destroy_notifiers; l != NULL; l = l->next)
    {
      GDestroyNotify func;
      gpointer data;

      func = l->data;

      l = l->next;
      g_assert (l != NULL);

      data = l->data;

      func (data);
    }

  g_slist_free (priv->destroy_notifiers);

  G_OBJECT_CLASS (egg_dbus_method_invocation_parent_class)->finalize (object);
}

static void
egg_dbus_method_invocation_class_init (EggDBusMethodInvocationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_method_invocation_finalize;

  g_type_class_add_private (klass, sizeof (EggDBusMethodInvocationPrivate));
}

/**
 * egg_dbus_method_invocation_new:
 * @request_message: The message encapsulating the method call request.
 * @source_tag: An user provided tag.
 *
 * Creates a new #EggDBusMethodInvocation. This method is only useful for
 * language bindings.
 *
 * Returns: A #EggDBusMethodInvocation. Free with g_object_unref().
 **/
EggDBusMethodInvocation *
egg_dbus_method_invocation_new (EggDBusMessage         *request_message,
                                gpointer                source_tag)
{
  EggDBusMethodInvocation *method_invocation;
  EggDBusMethodInvocationPrivate *priv;

  method_invocation = EGG_DBUS_METHOD_INVOCATION (g_object_new (EGG_DBUS_TYPE_METHOD_INVOCATION,
                                                              NULL));

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  priv->request_message = g_object_ref (request_message);
  priv->source_tag = source_tag;

  return method_invocation;
}

/**
 * egg_dbus_method_invocation_get_source_tag:
 * @method_invocation: A #EggDBusMethodInvocation.
 *
 * Gets the user provided tag for @method_invocation. This method is only useful for
 * language bindings.
 *
 * Returns: The user provided tag set when @method_invocation was constructed.
 **/
gpointer
egg_dbus_method_invocation_get_source_tag (EggDBusMethodInvocation *method_invocation)
{
  EggDBusMethodInvocationPrivate *priv;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  return priv->source_tag;
}

/**
 * egg_dbus_method_invocation_create_reply_message:
 * @method_invocation: A #EggDBusMethodInvocation.
 *
 * Creates a #EggDBusMessage in reply to the #EggDBusMessage passed when
 * @method_invocation was created. This method is only useful for
 * language bindings.
 *
 * Returns: A new #EggDBusMessage. Free with g_object_unref().
 **/
EggDBusMessage *
egg_dbus_method_invocation_create_reply_message (EggDBusMethodInvocation *method_invocation)
{
  EggDBusMessage *reply;
  EggDBusMethodInvocationPrivate *priv;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  reply = egg_dbus_message_new_for_method_reply (priv->request_message);

  return reply;
}

/**
 * egg_dbus_method_invocation_get_connection:
 * @method_invocation: A #EggDBusMethodInvocation.
 *
 * Gets the #EggDBusConnection that @method_invocation is associated with.
 *
 * Returns: A #EggDBusConnection. Do not free, the returned object is owned by @method_invocation.
 **/
EggDBusConnection *
egg_dbus_method_invocation_get_connection (EggDBusMethodInvocation *method_invocation)
{
  EggDBusMethodInvocationPrivate *priv;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  return egg_dbus_message_get_connection (priv->request_message);
}

/**
 * egg_dbus_method_invocation_get_caller:
 * @method_invocation: A #EggDBusMethodInvocation.
 *
 * Gets the unique bus name of the caller of the method.
 *
 * Returns: The unique bus name of the caller. Do not free, the returned string is owned by @method_invocation.
 **/
const gchar *
egg_dbus_method_invocation_get_caller (EggDBusMethodInvocation *method_invocation)
{
  EggDBusMethodInvocationPrivate *priv;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  return egg_dbus_message_get_sender (priv->request_message);
}

/**
 * egg_dbus_method_invocation_add_destroy_notify:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @data: Data to free.
 * @func: Free function.
 *
 * Makes @method_invocation call @func with a a single parameter,
 * @data, upon finalization.
 **/
void
egg_dbus_method_invocation_add_destroy_notify (EggDBusMethodInvocation *method_invocation,
                                             gpointer               data,
                                             GDestroyNotify         func)
{
  EggDBusMethodInvocationPrivate *priv;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  priv->destroy_notifiers = g_slist_prepend (priv->destroy_notifiers, data);
  priv->destroy_notifiers = g_slist_prepend (priv->destroy_notifiers, func);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_method_invocation_return_error_valist:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @domain: Error domain.
 * @code: Error code.
 * @format: printf() style format for human readable message.
 * @var_args: Arguments for @format.
 *
 * Like egg_dbus_method_invocation_return_error() but intended for language bindings.
 **/
void
egg_dbus_method_invocation_return_error_valist (EggDBusMethodInvocation *method_invocation,
                                              GQuark                 domain,
                                              gint                   code,
                                              const gchar           *format,
                                              va_list                var_args)
{
  gchar *literal_message;

  literal_message = g_strdup_vprintf (format, var_args);
  egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                 domain,
                                                 code,
                                                 literal_message);
  g_free (literal_message);
}


/**
 * egg_dbus_method_invocation_return_error:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @domain: Error domain.
 * @code: Error code.
 * @format: printf() style format for human readable message.
 * @...: Arguments for @format.
 *
 * Use this to return an error when handling a D-Bus method call. The error will be propagated
 * to the remote caller.
 *
 * This completes the method invocation and you don't have to call the corresponding
 * <literal>_finish()</literal> method in your D-Bus method call handler.
 **/
void
egg_dbus_method_invocation_return_error (EggDBusMethodInvocation *method_invocation,
                                         GQuark                 domain,
                                         gint                   code,
                                         const gchar           *format,
                                         ...)
{
  va_list va_args;

  va_start (va_args, format);
  egg_dbus_method_invocation_return_error_valist (method_invocation,
                                                domain,
                                                code,
                                                format,
                                                va_args);
  va_end (va_args);
}

/**
 * egg_dbus_method_invocation_return_error_literal:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @domain: Error domain.
 * @code: Error code.
 * @message: Human readable error message.
 *
 * Like egg_dbus_method_invocation_return_error() but without
 * printf()-style formatting.
 **/
void
egg_dbus_method_invocation_return_error_literal (EggDBusMethodInvocation *method_invocation,
                                               GQuark                 domain,
                                               gint                   code,
                                               const gchar           *message)
{
  GError *error;

  error = g_error_new_literal (domain, code, message);
  egg_dbus_method_invocation_return_gerror (method_invocation, error);
  g_error_free (error);
}

/**
 * egg_dbus_method_invocation_return_gerror:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @error: A #GError.
 *
 * Like egg_dbus_method_invocation_return_error() but takes a a #GError instead.
 **/
void
egg_dbus_method_invocation_return_gerror (EggDBusMethodInvocation *method_invocation,
                                          GError                  *error)
{
  gchar *error_name;
  EggDBusMethodInvocationPrivate *priv;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  error_name = _egg_dbus_error_encode_gerror (error);

  egg_dbus_method_invocation_return_dbus_error_literal (method_invocation,
                                                        error_name,
                                                        error->message);

  g_free (error_name);
}

/**
 * egg_dbus_method_invocation_return_dbus_error_valist:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @name: A D-Bus error name such as <literal>org.freedesktop.DBus.Error.UnknownMethod</literal>.
 * @format: printf() style format for human readable message.
 * @var_args: Arguments for @format.
 *
 * Like egg_dbus_method_invocation_return_dbus_error() but intended for langauge bindings.
 **/
void
egg_dbus_method_invocation_return_dbus_error_valist (EggDBusMethodInvocation *method_invocation,
                                                   const gchar           *name,
                                                   const gchar           *format,
                                                   va_list                var_args)
{
  gchar *literal_message;

  literal_message = g_strdup_vprintf (format, var_args);

  egg_dbus_method_invocation_return_dbus_error_literal (method_invocation,
                                                      name,
                                                      literal_message);

  g_free (literal_message);
}

/**
 * egg_dbus_method_invocation_return_dbus_error:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @name: A D-Bus error name such as <literal>org.freedesktop.DBus.Error.UnknownMethod</literal>.
 * @format: printf() style format for human readable message.
 * @...: Arguments for @format.
 *
 * Use this to return a raw D-Bus error when handling a D-Bus method call. The error will
 * be propagated to the remote caller.
 *
 * This completes the method invocation and you don't have to call the corresponding
 * <literal>_finish()</literal> method in your D-Bus method call handler.
 **/
void
egg_dbus_method_invocation_return_dbus_error (EggDBusMethodInvocation *method_invocation,
                                            const gchar           *name,
                                            const gchar           *format,
                                            ...)
{
  va_list va_args;

  va_start (va_args, format);
  egg_dbus_method_invocation_return_dbus_error_valist (method_invocation,
                                                     name,
                                                     format,
                                                     va_args);
  va_end (va_args);
}

/**
 * egg_dbus_method_invocation_return_dbus_error_literal:
 * @method_invocation: A #EggDBusMethodInvocation.
 * @name: A D-Bus error name such as <literal>org.freedesktop.DBus.Error.UnknownMethod</literal>.
 * @message: Human readable message to pass.
 *
 * Like egg_dbus_method_invocation_return_dbus_error() but without
 * printf()-style formatting.
 **/
void
egg_dbus_method_invocation_return_dbus_error_literal (EggDBusMethodInvocation *method_invocation,
                                                    const gchar           *name,
                                                    const gchar           *message)
{
  EggDBusMessage *reply;
  EggDBusMethodInvocationPrivate *priv;

  priv = EGG_DBUS_METHOD_INVOCATION_GET_PRIVATE (method_invocation);

  reply = egg_dbus_message_new_for_method_error_reply (priv->request_message,
                                                     name,
                                                     message);

  egg_dbus_connection_send_message (egg_dbus_message_get_connection (reply), reply);

  g_object_unref (reply);
}
