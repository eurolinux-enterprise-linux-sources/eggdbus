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
#include <eggdbus/eggdbusobjectproxy.h>
#include <eggdbus/eggdbusinterfaceproxy.h>
#include <eggdbus/eggdbuserror.h>
#include <eggdbus/eggdbusobjectpath.h>
#include <eggdbus/eggdbusprivate.h>
#include <eggdbus/eggdbusmessage.h>
#include <eggdbus/eggdbusconnection.h>
#include <eggdbus/eggdbuspeer.h>
#include <eggdbus/eggdbusbus.h>
#include <eggdbus/eggdbusproperties.h>
#include <eggdbus/eggdbusvariant.h>
#include <eggdbus/eggdbusintrospectable.h>
#include <eggdbus/eggdbushashmap.h>

/**
 * SECTION:eggdbusobjectproxy
 * @title: EggDBusObjectProxy
 * @short_description: Proxy for remote objects
 * @see_also: #EggDBusConnection
 *
 * Instances of the #EggDBusObjectProxy class represents remote objects. You can't instantiate
 * this class directly, you have to use egg_dbus_connection_get_object_proxy(). The tripple
 * consisting of a connection, bus name and object path uniquely identifies a remote object.
 *
 * You can get the connection name, object path for a #EggDBusObjectProxy instance from the
 * #EggDBusObjectProxy:connection, #EggDBusObjectProxy:name and #EggDBusObjectProxy:object-path
 * properties. Also, the <emphasis>current</emphasis> owner of the name the object proxy is
 * associated with can be obtained from the #EggDBusObjectProxy:name-owner property.
 *
 * Note that an #EggDBusObjectProxy instance may refer to a non-existant remote object. For
 * example names on the message bus can come and go (can be checked with #EggDBusObjectProxy:name-owner)
 * and the remote object itself may be destroyed (for example if the object represents a
 * piece of hardware on the system, the object may disappear when the hardware is unplugged).
 * One way (if the remote service supports introspection) to find out if a remote object exists
 * is to use the egg_dbus_object_proxy_introspect() method. If the call succeeds and the result
 * includes one or more interfaces, the object is alive:
 *
 * <programlisting>
 * EggDBusInterfaceNodeInfo *node_info;
 * GError *error;
 *
 * error = NULL;
 * node_info = egg_dbus_object_proxy_introspect_sync (slash_object_proxy,
 *                                                    EGG_DBUS_CALL_FLAGS_NONE,
 *                                                    NULL,
 *                                                    &error);
 * if (node_info == NULL)
 *   {
 *     /<!-- -->* handle error *<!-- -->/
 *   }
 * else
 *   {
 *     if (node_info->num_interfaces > 0)
 *       {
 *         /<!-- -->* object is alive, look at node_info for more information *<!-- -->/
 *       }
 *     egg_dbus_interface_node_info_free (node_info);
 *   }
 * </programlisting>
 *
 * To use a D-Bus interface on a #EggDBusObjectProxy instance you will need a #EggDBusInterfaceProxy
 * instance for the D-Bus interface in question. Interface proxies can be obtained using the
 * egg_dbus_object_proxy_query_interface() method. Typically language bindings will provide a
 * way to obtain it; for generated C/GObject code, a macro is provided. For example, to invoke the
 * <literal>Ping</literal> method on the <literal>org.freedesktop.DBus.Peer</literal> interface
 * on a #EggDBusObjectProxy, the #EggDBusPeer interface proxy is used:
 *
 * <programlisting>
 * EggDBusPeer *peer;
 * GError *error;
 *
 * peer = EGG_DBUS_QUERY_INTERFACE_PEER (object_proxy);
 *
 * error = NULL;
 * if (!egg_dbus_peer_ping_sync (peer,
 *                               EGG_DBUS_CALL_FLAGS_NONE,
 *                               NULL, /<!-- -->* GCancellable *<!-- -->/
 *                               &error))
 *   {
 *     g_warning ("Error: %s", error->message);
 *     g_error_free (error);
 *     goto error;
 *   }
 * </programlisting>
 *
 * See the #EggDBusInterfaceProxy class for more details on using D-Bus interfaces.
 */

typedef struct
{
  EggDBusConnection *connection;
  gchar             *name;
  gchar             *object_path;

  gboolean dont_unref_connection_on_finalize;

  /* A map from the interface GType to a EggDBusInterfaceProxy derived proxy instance.
   */
  GHashTable *interface_type_to_interface_proxy;

} EggDBusObjectProxyPrivate;

enum
{
  PROP_0,
  PROP_NAME,
  PROP_NAME_OWNER,
  PROP_OBJECT_PATH,
  PROP_CONNECTION,
};

#define EGG_DBUS_OBJECT_PROXY_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_OBJECT_PROXY, EggDBusObjectProxyPrivate))

G_DEFINE_TYPE (EggDBusObjectProxy, egg_dbus_object_proxy, G_TYPE_OBJECT);

static void
egg_dbus_object_proxy_init (EggDBusObjectProxy *object_proxy)
{
  EggDBusObjectProxyPrivate *priv;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  priv->interface_type_to_interface_proxy = g_hash_table_new_full (g_direct_hash,
                                                                   g_direct_equal,
                                                                   NULL,
                                                                   (GDestroyNotify) g_object_unref);
}

static void
egg_dbus_object_proxy_dispose (GObject *object)
{
  EggDBusObjectProxy *object_proxy;
  EggDBusObjectProxyPrivate *priv;

  object_proxy = EGG_DBUS_OBJECT_PROXY (object);
  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  if (priv->interface_type_to_interface_proxy != NULL)
    {
      g_hash_table_unref (priv->interface_type_to_interface_proxy);
      priv->interface_type_to_interface_proxy = NULL;
    }

  if (G_OBJECT_CLASS (egg_dbus_object_proxy_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (egg_dbus_object_proxy_parent_class)->dispose (object);
}

static void
egg_dbus_object_proxy_finalize (GObject *object)
{
  EggDBusObjectProxy *object_proxy;
  EggDBusObjectProxyPrivate *priv;

  object_proxy = EGG_DBUS_OBJECT_PROXY (object);
  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  if (!priv->dont_unref_connection_on_finalize)
    {
      _egg_dbus_connection_unregister_object_proxy (priv->connection, object_proxy);
      if (priv->connection != NULL)
        g_object_unref (priv->connection);
    }

  g_free (priv->name);
  g_free (priv->object_path);

  G_OBJECT_CLASS (egg_dbus_object_proxy_parent_class)->finalize (object);
}

/* see EggDBusConnection's constructed() method */
void
_egg_dbus_object_proxy_dont_unref_connection_on_finalize (EggDBusObjectProxy   *object_proxy)
{
  EggDBusObjectProxyPrivate *priv;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  priv->dont_unref_connection_on_finalize = TRUE;
}

static void
egg_dbus_object_proxy_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EggDBusObjectProxy *object_proxy;
  EggDBusObjectProxyPrivate *priv;

  object_proxy = EGG_DBUS_OBJECT_PROXY (object);
  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_OBJECT_PATH:
      g_value_set_boxed (value, priv->object_path);
      break;

    case PROP_NAME_OWNER:
      g_value_take_string (value, egg_dbus_object_proxy_get_name_owner (object_proxy));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
egg_dbus_object_proxy_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EggDBusObjectProxy *object_proxy;
  EggDBusObjectProxyPrivate *priv;

  object_proxy = EGG_DBUS_OBJECT_PROXY (object);
  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      priv->connection = g_value_dup_object (value);
      break;

    case PROP_NAME:
      priv->name = g_strdup (g_value_get_string (value));
      break;

    case PROP_OBJECT_PATH:
      priv->object_path = g_strdup (g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* called by filter_function() in EggDBusConnection
 */
void
_egg_dbus_object_proxy_handle_message (EggDBusObjectProxy   *object_proxy,
                              DBusMessage  *dmessage)
{
  EggDBusObjectProxyPrivate *priv;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  /*g_debug ("in _egg_dbus_object_proxy_handle_message() objpath='%s', sender='%s' if='%s' member='%s' sig='%s'",
           dbus_message_get_path (dmessage),
           dbus_message_get_sender (dmessage),
           dbus_message_get_interface (dmessage),
           dbus_message_get_member (dmessage),
           dbus_message_get_signature (dmessage));*/

  if (dbus_message_get_type (dmessage) == DBUS_MESSAGE_TYPE_SIGNAL)
    {
      EggDBusInterfaceProxy *interface_proxy;
      const gchar *interface_name;
      const gchar *signal_name;
      GHashTableIter hash_iter;
      EggDBusMessage *signal_message;

      interface_name = dbus_message_get_interface (dmessage);
      signal_name = dbus_message_get_member (dmessage);

      /* org.freedesktop.DBus.Properties is special since we're representing
       * properties as GObject properties. We don't want to pollute the
       * signal name space.. so dispatch the changed property bag via function
       * calls to matching interfaces.
       */
      if (strcmp (interface_name, "org.freedesktop.DBus.Properties") == 0 &&
          strcmp (signal_name, "EggDBusChanged") == 0)
        {
          gchar *prop_interface_name;
          EggDBusHashMap *changed_properties;
          GError *error;

          signal_message = egg_dbus_connection_new_message_for_signal (priv->connection,
                                                                       dbus_message_get_sender (dmessage),
                                                                       dbus_message_get_destination (dmessage),
                                                                       priv->object_path,
                                                                       interface_name,
                                                                       signal_name);

          /* overwrites the DBusMessage that EggDBusMessage creates */
          g_object_set_data_full (G_OBJECT (signal_message),
                                  "dbus-1-message",
                                  dbus_message_ref (dmessage),
                                  (GDestroyNotify) dbus_message_unref);

          error = NULL;

          if (!egg_dbus_message_extract_string (signal_message, &prop_interface_name, &error))
            {
              g_warning ("Error extracting interface name when handling EggDBusChanged() on "
                         "org.freedesktop.DBus.Properties: %s", error->message);
              g_error_free (error);
              goto prop_extract_failed;
            }

          if (!egg_dbus_message_extract_map (signal_message, &changed_properties, &error))
            {
              g_warning ("Error extracting interface name when handling EggDBusChanged() on "
                         "org.freedesktop.DBus.Properties: %s", error->message);
              g_error_free (error);
              g_free (prop_interface_name);
              goto prop_extract_failed;
            }

          /* now distribute this decoded signal to interface proxies that match */
          g_hash_table_iter_init (&hash_iter, priv->interface_type_to_interface_proxy);
          while (g_hash_table_iter_next (&hash_iter, NULL, (gpointer) &interface_proxy))
            {
              const EggDBusInterfaceInfo *interface_info;
              EggDBusInterfaceIface *g_iface;

              g_iface = egg_dbus_interface_proxy_get_interface_iface (interface_proxy);
              interface_info = g_iface->get_interface_info ();

              if (strcmp (interface_info->name, prop_interface_name) == 0)
                {
                  _egg_dbus_interface_proxy_handle_property_changed (interface_proxy,
                                                                     changed_properties);
                }
            }

          g_free (prop_interface_name);
          g_object_unref (changed_properties);

        prop_extract_failed:

          g_object_unref (signal_message);
        }
      else
        {
          /* otherwise dispatch the signal to *all* (there may be more than one) of our interface proxies
           * that match the interface name
           *
           * TODO: if this is slow, we could have a hash from D-Bus interface name
           *       to a list of EggDBusInterfaceProxy objects
           */
          g_hash_table_iter_init (&hash_iter, priv->interface_type_to_interface_proxy);
          while (g_hash_table_iter_next (&hash_iter, NULL, (gpointer) &interface_proxy))
            {
              const EggDBusInterfaceInfo *interface_info;
              EggDBusInterfaceIface *g_iface;

              g_iface = egg_dbus_interface_proxy_get_interface_iface (interface_proxy);
              interface_info = g_iface->get_interface_info ();

              if (strcmp (interface_info->name, interface_name) == 0)
                {
                  signal_message = egg_dbus_connection_new_message_for_signal (priv->connection,
                                                                               dbus_message_get_sender (dmessage),
                                                                               dbus_message_get_destination (dmessage),
                                                                               priv->object_path,
                                                                               interface_name,
                                                                               signal_name);

                  /* overwrites the DBusMessage that EggDBusMessage creates */
                  g_object_set_data_full (G_OBJECT (signal_message),
                                          "dbus-1-message",
                                          dbus_message_ref (dmessage),
                                          (GDestroyNotify) dbus_message_unref);

                  g_iface->handle_message ((gpointer) interface_proxy,
                                           signal_message);

                  g_object_unref (signal_message);
                }
            }

        } /* for all interface proxies */

    } /* is_signal */
}

static void
egg_dbus_object_proxy_class_init (EggDBusObjectProxyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose      = egg_dbus_object_proxy_dispose;
  gobject_class->finalize     = egg_dbus_object_proxy_finalize;
  gobject_class->set_property = egg_dbus_object_proxy_set_property;
  gobject_class->get_property = egg_dbus_object_proxy_get_property;

  g_type_class_add_private (klass, sizeof (EggDBusObjectProxyPrivate));

  /**
   * EggDBusObjectProxy:name:
   *
   * The name on the bus that the #EggDBusObjectProxy instance is associated with.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "The name of the remote end",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EggDBusObjectProxy:name-owner:
   *
   * The unique name of the owner of #EggDBusObjectProxy:name, if any.
   *
   * If #EggDBusObjectProxy:name is a well-known name
   * (e.g. <literal>org.freedesktop.DeviceKit</literal>), this
   * property contains the unique name (e.g. <literal>1:6</literal>)
   * of the process that currently owns the name. It is %NULL if no
   * process currently provides the name. If a process starts owning
   * the name, the property will reflect that.
   *
   * If #EggDBusObjectProxy:name itself is a unique name (e.g. of the form
   * <literal>:1.42</literal>), then this property is equal to that
   * name only if the remote end with the given name is connected to
   * the bus. If the remote end disconnects, the property changes to
   * %NULL.  If this is the case then the object proxy is pretty much useless
   * as unique names are never recycled.
   *
   * Connect to the #GObject::notify signal to track changes to this
   * property.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME_OWNER,
                                   g_param_spec_string ("name-owner",
                                                        "Name Owner",
                                                        "The unique name of the owner of name.",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EggDBusObjectProxy:object-path:
   *
   * The object path that the #EggDBusObjectProxy instance is associated with.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_boxed ("object-path",
                                                       "Object Path",
                                                       "The object path of the remote object",
                                                       EGG_DBUS_TYPE_OBJECT_PATH,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK |
                                                       G_PARAM_STATIC_BLURB));

  /**
   * EggDBusObjectProxy:connection:
   *
   * The connection that the #EggDBusObjectProxy instance is associated with.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        "Connection",
                                                        "The connection that owns the object proxy",
                                                        EGG_DBUS_TYPE_CONNECTION,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

}

EggDBusObjectProxy *
_egg_dbus_object_proxy_new (EggDBusConnection *connection,
                     const gchar       *name,
                     const gchar       *object_path)
{
  return EGG_DBUS_OBJECT_PROXY (g_object_new (EGG_DBUS_TYPE_OBJECT_PROXY,
                                     "connection", connection,
                                     "name", name,
                                     "object-path", object_path,
                                     NULL));
}

/**
 * egg_dbus_object_proxy_get_connection:
 * @object_proxy: A #EggDBusObjectProxy.
 *
 * C getter for the #EggDBusObjectProxy:connection property.
 *
 * Returns: The #EggDBusConnection that @object_proxy is associated with. Do not free, the
 *          connection object is owned by @object_proxy.
 **/
EggDBusConnection *
egg_dbus_object_proxy_get_connection (EggDBusObjectProxy *object_proxy)
{
  EggDBusObjectProxyPrivate *priv;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  return priv->connection;
}

/**
 * egg_dbus_object_proxy_get_name:
 * @object_proxy: A #EggDBusObjectProxy.
 *
 * C getter for the #EggDBusObjectProxy:name property.
 *
 * Returns: The bus name that @object_proxy is associated with. Do not free, the
 *          string is owned by @object_proxy.
 **/
const gchar *
egg_dbus_object_proxy_get_name (EggDBusObjectProxy *object_proxy)
{
  EggDBusObjectProxyPrivate *priv;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  return priv->name;
}

/**
 * egg_dbus_object_proxy_get_object_path:
 * @object_proxy: A #EggDBusObjectProxy.
 *
 * C getter for the #EggDBusObjectProxy:object-path property.
 *
 * Returns: The object path that @object_proxy is associated with. Do not free, the
 *          string is owned by @object_proxy.
 **/
const gchar *
egg_dbus_object_proxy_get_object_path (EggDBusObjectProxy *object_proxy)
{
  EggDBusObjectProxyPrivate *priv;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  return priv->object_path;
}

/**
 * egg_dbus_object_proxy_get_name_owner:
 * @object_proxy: A #EggDBusObjectProxy.
 *
 * C getter for the #EggDBusObjectProxy:name-owner property.
 *
 * Returns: The unique name, if any, that owns the name that @object_proxy is associated
 *          with. Free with g_free().
 **/
gchar *
egg_dbus_object_proxy_get_name_owner (EggDBusObjectProxy *object_proxy)
{
  EggDBusObjectProxyPrivate *priv;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  return _egg_dbus_connection_get_owner_for_name (priv->connection, priv->name);
}

/**
 * egg_dbus_object_proxy_query_interface:
 * @object_proxy: A #EggDBusObjectProxy.
 * @interface_type: A #GType for an interface that extends #EggDBusInterface.
 *
 * Gets an interface proxy for accessing a D-Bus interface (as
 * specified by @interface_type) on the remote object represented by
 * @object_proxy.
 *
 * Note that there is no guarantee that @object_proxy actually
 * implements the D-Bus interface specified by @interface_type.
 *
 * Returns: An instance that implements @interface_type and is derived
 * from #EggDBusInterfaceProxy. Do not ref or unref the returned
 * instance; it is owned by @object_proxy.
 **/
EggDBusInterfaceProxy *
egg_dbus_object_proxy_query_interface (EggDBusObjectProxy  *object_proxy,
                                       GType                interface_type)
{
  EggDBusObjectProxyPrivate *priv;
  EggDBusInterfaceIface *g_iface;
  EggDBusInterfaceProxy *interface_proxy;

  g_return_val_if_fail (EGG_DBUS_IS_OBJECT_PROXY (object_proxy), NULL);
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type), NULL);

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  interface_proxy = g_hash_table_lookup (priv->interface_type_to_interface_proxy,
                                         GUINT_TO_POINTER (interface_type));
  if (interface_proxy != NULL)
    goto out;

  g_iface = g_type_default_interface_ref (interface_type);

  /* TODO: check that g_iface "extends" EggDBusInterface type */

  /* interface_proxy will take a weak reference to us */
  interface_proxy = g_iface->get_interface_proxy (object_proxy);

  g_hash_table_insert (priv->interface_type_to_interface_proxy,
                       GUINT_TO_POINTER (interface_type),
                       interface_proxy);

  g_type_default_interface_unref (g_iface);

 out:
  return interface_proxy;
}


/* ---------------------------------------------------------------------------------------------------- */

static void
egg_dbus_object_proxy_introspect_cb (GObject *source_object,
                                     GAsyncResult *res,
                                     gpointer user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  EggDBusObjectProxy *object_proxy = EGG_DBUS_OBJECT_PROXY (source_object);
  gchar *xml_data;
  GError *error;

  error = NULL;
  if (!egg_dbus_introspectable_introspect_finish (EGG_DBUS_QUERY_INTERFACE_INTROSPECTABLE (object_proxy),
                                                  &xml_data,
                                                  res,
                                                  &error))
    {
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (simple, xml_data, g_free);
    }

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

/**
 * egg_dbus_object_proxy_introspect:
 * @object_proxy: A #EggDBusObjectProxy.
 * @call_flags: Flags from #EggDBusCallFlags detailing how the D-Bus message should be sent.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: Callback to invoke when the asynchronous operation finishes.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously introspects @object_proxy. Note that this may fail
 * if the remote object doesn't implement the
 * <literal>org.freedesktop.DBus.Introspectable</literal> D-Bus
 * interface.
 *
 * When the asynchronous operation is finished, @callback will be
 * invoked; use egg_dbus_object_proxy_introspect_finish() to finish
 * the operation.
 *
 * Note that introspection data is never cached.
 *
 * Returns: A pending call id (never zero) that can be used with egg_dbus_connection_pending_call_cancel()
 *          or egg_dbus_connection_pending_call_block().
 **/
guint
egg_dbus_object_proxy_introspect (EggDBusObjectProxy  *object_proxy,
                                  EggDBusCallFlags     call_flags,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GSimpleAsyncResult *simple;
  guint pending_call_id;

  g_return_val_if_fail (EGG_DBUS_IS_OBJECT_PROXY (object_proxy), 0);

  simple = g_simple_async_result_new (G_OBJECT (object_proxy),
                                      callback,
                                      user_data,
                                      egg_dbus_object_proxy_introspect);

  pending_call_id = egg_dbus_introspectable_introspect (EGG_DBUS_QUERY_INTERFACE_INTROSPECTABLE (object_proxy),
                                                        call_flags,
                                                        cancellable,
                                                        egg_dbus_object_proxy_introspect_cb,
                                                        simple);

  return pending_call_id;
}

/**
 * egg_dbus_object_proxy_introspect_finish:
 * @object_proxy: A #EggDBusObjectProxy.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed to
 * egg_dbus_object_proxy_introspect().
 * @error: Return location for error.
 *
 * Finishes introspecting @object_proxy.
 *
 * Returns: %NULL if @error is set, otherwise a #EggDBusInterfaceNodeInfo structure
 * that should be freed with egg_dbus_interface_node_info_free().
 **/
EggDBusInterfaceNodeInfo *
egg_dbus_object_proxy_introspect_finish (EggDBusObjectProxy  *object_proxy,
                                         GAsyncResult        *res,
                                         GError             **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  EggDBusInterfaceNodeInfo *ret;
  gchar *xml_data;

  g_return_val_if_fail (EGG_DBUS_IS_OBJECT_PROXY (object_proxy), NULL);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == egg_dbus_object_proxy_introspect);

  ret = NULL;

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  xml_data = g_simple_async_result_get_op_res_gpointer (simple);

  ret = egg_dbus_interface_new_node_info_from_xml (xml_data, error);

out:
  return ret;
}

/**
 * egg_dbus_object_proxy_introspect_sync:
 * @object_proxy: A #EggDBusObjectProxy.
 * @call_flags: Flags from #EggDBusCallFlags detailing how the D-Bus message should be sent.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error.
 *
 * Syncrhonously introspects @object_proxy. Note that this may fail
 * if the remote object doesn't implement the
 * <literal>org.freedesktop.DBus.Introspectable</literal> D-Bus
 * interface.
 *
 * Note that introspection data is never cached.
 *
 * Returns: %NULL if @error is set, otherwise a #EggDBusInterfaceNodeInfo structure
 * that should be freed with egg_dbus_interface_node_info_free().
 **/
EggDBusInterfaceNodeInfo *
egg_dbus_object_proxy_introspect_sync (EggDBusObjectProxy  *object_proxy,
                                       EggDBusCallFlags     call_flags,
                                       GCancellable        *cancellable,
                                       GError             **error)
{
  EggDBusInterfaceNodeInfo *ret;
  gchar *xml_data;

  g_return_val_if_fail (EGG_DBUS_IS_OBJECT_PROXY (object_proxy), NULL);

  ret = NULL;

  if (!egg_dbus_introspectable_introspect_sync (EGG_DBUS_QUERY_INTERFACE_INTROSPECTABLE (object_proxy),
                                                call_flags,
                                                &xml_data,
                                                cancellable,
                                                error))
    goto out;

  ret = egg_dbus_interface_new_node_info_from_xml (xml_data, error);

  g_free (xml_data);

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_object_proxy_invalidate_properties:
 * @object_proxy: A #EggDBusObjectProxy.
 *
 * Invalidate the property caches for all interface proxies for @object_proxy.
 *
 * Typically, this is not necessary but it's useful when the remote
 * end doesn't use a standard set of signals to indicate property
 * changes.
 **/
void
egg_dbus_object_proxy_invalidate_properties (EggDBusObjectProxy *object_proxy)
{
  EggDBusObjectProxyPrivate *priv;
  EggDBusInterfaceProxy *interface_proxy;
  GHashTableIter hash_iter;

  priv = EGG_DBUS_OBJECT_PROXY_GET_PRIVATE (object_proxy);

  g_hash_table_iter_init (&hash_iter, priv->interface_type_to_interface_proxy);
  while (g_hash_table_iter_next (&hash_iter, NULL, (gpointer) &interface_proxy))
    _egg_dbus_interface_proxy_invalidate_properties (interface_proxy);

}

/* ---------------------------------------------------------------------------------------------------- */
