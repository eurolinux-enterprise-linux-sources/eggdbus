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
#include <dbus/dbus-glib-lowlevel.h>
#include <eggdbus/eggdbusconnection.h>
#include <eggdbus/eggdbusobjectproxy.h>
#include <eggdbus/eggdbusbus.h>
#include <eggdbus/eggdbuserror.h>
#include <eggdbus/eggdbusbusnametracker.h>
#include <eggdbus/eggdbusvariant.h>
#include <eggdbus/eggdbusmessage.h>
#include <eggdbus/eggdbusproperties.h>
#include <eggdbus/eggdbusintrospectable.h>
#include <eggdbus/eggdbuspeer.h>
#include <eggdbus/eggdbushashmap.h>
#include <eggdbus/eggdbusprivate.h>

/**
 * SECTION:eggdbusconnection
 * @title: EggDBusConnection
 * @short_description: Connection
 *
 * The #EggDBusConnection class is used to connect to other processes or message buses.
 */

/* TODO: need locking to be MT-safe */

/* global instances for reuse/sharing */
static EggDBusConnection *the_system_bus;
static EggDBusConnection *the_session_bus;

typedef struct
{

  /* The libdbus connection */
  DBusConnection *connection;

  /* The type of bus we're connected to */
  EggDBusBusType bus_type;

  /* Our private bus proxy. This proxy never appears in any of the
   * collections below.
   */
  EggDBusObjectProxy *bus_object_proxy;

  /* ------------- */
  /* PROXY OBJECTS */
  /* ------------- */

  /* A name tracker for bus_object_proxy */
  EggDBusBusNameTracker *bus_name_tracker;

  /* A hash from strings of the form "<objpath>:<name>" to EggDBusObjectProxy objects
   *
   * The size of this hash table is equal to the number of proxies created.
   */
  GHashTable *hash_from_objpath_and_name_to_object_proxy;

  /* A hash from strings of the form "<name>" to a GList of EggDBusObjectProxy objects
   *
   * Essentially, each key in the hash table corresponds to the all
   * the names we have proxies for. Each value in the hash table
   * is a list of proxies for the given name.
   */
  GHashTable *hash_from_name_to_list_of_proxies;

  /* a map from pending_call_id to DBusPendingCall */
  GHashTable *hash_pending_call_id_to_async_result;

  /* used for dispensing pending_call_id's */
  guint pending_call_next;

  /* ----------------- */
  /* EXPORTING OBJECTS */
  /* ----------------- */

  GHashTable *hash_from_object_path_to_export_data;

} EggDBusConnectionPrivate;

static void bus_name_lost_owner_cb   (EggDBusBusNameTracker *bus_name_tracker,
                                      const gchar         *bus_name,
                                      const gchar         *unique_bus_name_of_old_owner,
                                      EggDBusConnection     *connection);

static void bus_name_gained_owner_cb (EggDBusBusNameTracker *bus_name_tracker,
                                      const gchar         *bus_name,
                                      const gchar         *unique_bus_name_of_old_owner,
                                      EggDBusConnection     *connection);

static gchar *concat_objpath_and_name (const gchar *objpath, const gchar *name);

static DBusHandlerResult filter_function_handle_method_call (DBusConnection *dconnection,
                                                             DBusMessage    *message,
                                                             void           *user_data);

static DBusHandlerResult filter_function_handle_signal (DBusConnection *dconnection,
                                                        DBusMessage    *message,
                                                        void           *user_data);

typedef struct _ExportData ExportData;

static void export_data_free (ExportData *data);

enum
{
  PROP_0,
  PROP_BUS_TYPE,
  PROP_UNIQUE_NAME,
};

#define EGG_DBUS_CONNECTION_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_CONNECTION, EggDBusConnectionPrivate))

G_DEFINE_TYPE (EggDBusConnection, egg_dbus_connection, G_TYPE_OBJECT);

static void
egg_dbus_connection_init (EggDBusConnection *connection)
{
  EggDBusConnectionPrivate *priv;

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  priv->hash_from_objpath_and_name_to_object_proxy =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  priv->hash_from_name_to_list_of_proxies =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  priv->hash_from_object_path_to_export_data =
    g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) export_data_free);

  priv->hash_pending_call_id_to_async_result = g_hash_table_new (g_direct_hash, g_direct_equal);

  priv->pending_call_next = 0;
}

static void
free_list_in_hash (gpointer key, GList *list, gpointer user_data)
{
  /* only shallow free the list */
  g_list_free (list);
}

static void
egg_dbus_connection_finalize (GObject *object)
{
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;

  connection = EGG_DBUS_CONNECTION (object);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  g_hash_table_unref (priv->hash_from_objpath_and_name_to_object_proxy);

  g_hash_table_foreach (priv->hash_from_name_to_list_of_proxies, (GHFunc) free_list_in_hash, NULL);
  g_hash_table_unref (priv->hash_from_name_to_list_of_proxies);
  g_hash_table_unref (priv->hash_from_object_path_to_export_data);

  g_hash_table_unref (priv->hash_pending_call_id_to_async_result);

  if (priv->bus_object_proxy != NULL)
    {
      g_object_unref (priv->bus_object_proxy);
    }

  if (priv->connection != NULL)
    {
      dbus_connection_close (priv->connection);
      dbus_connection_unref (priv->connection);
    }

  /*g_debug ("connection finalized");*/

  G_OBJECT_CLASS (egg_dbus_connection_parent_class)->finalize (object);
}

static void
egg_dbus_connection_dispose (GObject *object)
{
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;

  connection = EGG_DBUS_CONNECTION (object);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  switch (priv->bus_type)
    {
    case EGG_DBUS_BUS_TYPE_SESSION:
      the_session_bus = NULL;
      break;

    case EGG_DBUS_BUS_TYPE_SYSTEM:
      the_system_bus = NULL;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  G_OBJECT_CLASS (egg_dbus_connection_parent_class)->dispose (object);
}

static void
egg_dbus_connection_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;

  connection = EGG_DBUS_CONNECTION (object);
  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  switch (prop_id)
    {
    case PROP_BUS_TYPE:
      priv->bus_type = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_connection_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;

  connection = EGG_DBUS_CONNECTION (object);
  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  switch (prop_id)
    {
    case PROP_BUS_TYPE:
      g_value_set_enum (value, priv->bus_type);
      break;

    case PROP_UNIQUE_NAME:
      g_value_set_string (value, egg_dbus_connection_get_unique_name (connection));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static DBusHandlerResult
filter_function (DBusConnection *dconnection,
                 DBusMessage    *message,
                 void           *user_data)
{
  DBusHandlerResult ret;

  switch (dbus_message_get_type (message))
    {
    case DBUS_MESSAGE_TYPE_SIGNAL:
      ret = filter_function_handle_signal (dconnection, message, user_data);
      break;

    case DBUS_MESSAGE_TYPE_METHOD_CALL:
      ret = filter_function_handle_method_call (dconnection, message, user_data);
      break;

    default:
      ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      break;
    }

  return ret;
}

static DBusHandlerResult
filter_function_handle_signal (DBusConnection *dconnection,
                               DBusMessage    *message,
                               void           *user_data)
{
  gchar *id;
  EggDBusObjectProxy *object_proxy;
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;
  DBusHandlerResult result;
  const char *objpath;
  const char *sender;
  const char *interface;
  gchar **well_known_names;
  guint n;

  connection = EGG_DBUS_CONNECTION (user_data);
  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  objpath = dbus_message_get_path (message);
  sender = dbus_message_get_sender (message);
  interface = dbus_message_get_interface (message);

  //g_debug ("filter_function_handle_signal: objpath='%s', sender='%s' if='%s' member='%s'", objpath, sender, dbus_message_get_interface (message), dbus_message_get_member (message));

  if (sender == NULL)
    {
      g_warning ("no sender\n");
      goto out;
    }

  if (objpath == NULL)
    {
      g_warning ("no objpath\n");
      goto out;
    }

  if (interface == NULL)
    {
      g_warning ("no interface\n");
      goto out;
    }

  /* route messages from the bus to our bus object_proxy */
  if (strcmp (sender, "org.freedesktop.DBus") == 0)
    {
      _egg_dbus_object_proxy_handle_message (priv->bus_object_proxy, message);

      /* ...but users may also manually create proxies for that
       * well-known name.. so also check our registered proxies...
       */
    }

  /* The bus always uses unique name in the SENDER field of the
   * message... quoting the D-Bus spec:
   *
   *   "Unique name of the sending connection. The message bus fills in
   *   this field so it is reliable; the field is only meaningful in
   *   combination with the message bus."
   *
   * Since our proxies can contain well-known names, we need to
   * maintain a mapping from unique names into well-known names to
   * determine what proxies to emit the signal on. This is what the
   * EggDBusBusNameTracker class does.
   *
   * However, suppose for a while that the unique name :1.42 owns both
   * of the well-known names com.example.Foo and com.example.Bar.
   *
   * Now, also assume that the name owner emits a signal on the object
   * '/' on the service com.example.Bar. But the message bus rewrites
   * com.example.Bar into a unique name so we only get to see
   * :1.42. So on the client side we don't really know whether this
   * signal is for com.example.Foo or for com.example.Bar.
   *
   * Now suppose both services implements a generic interface, say,
   * org.freedesktop.Service (a hypothetical interface that all
   * services could implement) with a signal Changed(). Now, is the
   * Changed() signal we receive from :1.42 supposed to come from Foo
   * or from Bar? We don't know.
   *
   * So the D-Bus spec is slightly broken in this regard insofar that
   * _broken_ programs like the above can trigger situations like
   * these. The only possible remediation is to make services use a
   * private namespace for their objects (e.g. the name
   * com.example.Foo uses objects in the /com/example/Foo namsspace,
   * ditto for com.example.Bar). Which, as a matter of fact, is what
   * is recommended.  Still, there's a couple of services in the wild
   * for which this is not true.
   *
   * So, back to reality. How do we know which proxy to route it to?
   *
   * For now, we push the signal to all proxies...
   *
   * [1] : on the upside, tracking all this stuff is immensely useful;
   * it means we can easily provide a meaningful :owner property on
   * EggDBusObjectProxy.
   *
   * See also http://0pointer.de/blog/projects/versioning-dbus.html
   * for details
   */

  well_known_names = egg_dbus_bus_name_tracker_get_known_well_known_bus_names_for_unique_bus_name (priv->bus_name_tracker,
                                                                                                 sender);

  if (well_known_names != NULL)
    {
      for (n = 0; well_known_names[n] != NULL; n++)
        {
          GList *proxies;
          GList *l;

          proxies = g_hash_table_lookup (priv->hash_from_name_to_list_of_proxies, well_known_names[n]);
          for (l = proxies; l != NULL; l = l->next)
            {
              object_proxy = EGG_DBUS_OBJECT_PROXY (l->data);

              if (strcmp (objpath, egg_dbus_object_proxy_get_object_path (object_proxy)) == 0)
                _egg_dbus_object_proxy_handle_message (object_proxy, message);
            }
        }
      g_strfreev (well_known_names);
    }

  /* Also, if we have a proxy for the unique name, dispatch it there as well */
  id = concat_objpath_and_name (objpath, sender);
  object_proxy = (EggDBusObjectProxy *) g_hash_table_lookup (priv->hash_from_objpath_and_name_to_object_proxy, id);
  if (object_proxy != NULL)
    _egg_dbus_object_proxy_handle_message (object_proxy, message);
  g_free (id);

  /* TODO: should we ever return DBUS_HANDLER_RESULT_HANDLED for signals? */

 out:
  return result;
}

static void
egg_dbus_connection_constructed (GObject *object)
{
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;
  DBusBusType dbus_bus_type;
  DBusError derror;

  if (G_OBJECT_CLASS (egg_dbus_connection_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (egg_dbus_connection_parent_class)->constructed (object);

  connection = EGG_DBUS_CONNECTION (object);
  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  switch (priv->bus_type)
    {
    case EGG_DBUS_BUS_TYPE_SESSION:
      dbus_bus_type = DBUS_BUS_SESSION;
      break;

    case EGG_DBUS_BUS_TYPE_SYSTEM:
      dbus_bus_type = DBUS_BUS_SYSTEM;
      break;

    case EGG_DBUS_BUS_TYPE_STARTER:
      dbus_bus_type = DBUS_BUS_STARTER;
      break;

    default:
    case EGG_DBUS_BUS_TYPE_NONE: /* TODO: support socket addresses etc.*/
      g_assert_not_reached ();
      break;
    }

  /* hmm.. we probably need to rethink error handling here */

  dbus_error_init (&derror);
  priv->connection = dbus_bus_get_private (dbus_bus_type, &derror);
  if (priv->connection == NULL)
    {
      g_warning ("Error connecting to bus: %s: %s\n",
                 derror.name,
                 derror.message);
      dbus_error_free (&derror);
    }
  g_object_set_data (G_OBJECT (connection), "dbus-1-connection", priv->connection);

  dbus_connection_setup_with_g_main (priv->connection, NULL);

  if (!dbus_connection_add_filter (priv->connection,
                                   filter_function,
                                   connection,
                                   NULL))
    g_assert (FALSE);

  priv->bus_object_proxy = egg_dbus_connection_get_object_proxy (connection,
                                                                 "org.freedesktop.DBus",
                                                                 "/");

  /* EggDBusObjectProxy refs connection. This is unwanted, because then we can
   * never unref EggDBusConnection. So tell the EggDBusObjectProxy instance
   * to avoid unreffing in finalize() and take back the reference.
   */
  g_object_unref (connection);
  _egg_dbus_object_proxy_dont_unref_connection_on_finalize (priv->bus_object_proxy);

  priv->bus_name_tracker = egg_dbus_bus_name_tracker_new (egg_dbus_connection_get_bus (connection));

  g_signal_connect (priv->bus_name_tracker,
                    "bus-name-lost-owner",
                    (GCallback) bus_name_lost_owner_cb,
                    connection);

  g_signal_connect (priv->bus_name_tracker,
                    "bus-name-gained-owner",
                    (GCallback) bus_name_gained_owner_cb,
                    connection);
}


static void
egg_dbus_connection_class_init (EggDBusConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_connection_finalize;
  gobject_class->dispose      = egg_dbus_connection_dispose;
  gobject_class->set_property = egg_dbus_connection_set_property;
  gobject_class->get_property = egg_dbus_connection_get_property;
  gobject_class->constructed  = egg_dbus_connection_constructed;

  g_type_class_add_private (klass, sizeof (EggDBusConnectionPrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_BUS_TYPE,
                                   g_param_spec_enum ("bus-type",
                                                      "Bus Type",
                                                      "Type of the bus we are connected to, if any",
                                                      EGG_TYPE_DBUS_BUS_TYPE,
                                                      EGG_DBUS_BUS_TYPE_NONE,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_UNIQUE_NAME,
                                   g_param_spec_string ("unique-name",
                                                        "Unique Name",
                                                        "The unique name as assigned by the message bus",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}

/**
 * egg_dbus_connection_get_for_bus:
 * @bus_type: The type of bus to get a connection for.
 *
 * Gets a connection to the bus specified by @bus_type. Note that this connection
 * is shared with other processes; use TODO() to get a private connection.
 *
 * Returns: A #EggDBusConnection. Free with g_object_unref().
 **/
EggDBusConnection *
egg_dbus_connection_get_for_bus (EggDBusBusType bus_type)
{
  EggDBusConnection *connection;
  EggDBusConnection **existing_connection;

  switch (bus_type)
    {
    case EGG_DBUS_BUS_TYPE_SESSION:
      existing_connection = &the_session_bus;
      break;

    case EGG_DBUS_BUS_TYPE_SYSTEM:
      existing_connection = &the_system_bus;
      break;

    case EGG_DBUS_BUS_TYPE_STARTER:
      /* TODO: implement and add test cases */
      g_assert_not_reached ();
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (*existing_connection != NULL)
    {
      connection = g_object_ref (*existing_connection);
    }
  else
    {
      connection = EGG_DBUS_CONNECTION (g_object_new (EGG_DBUS_TYPE_CONNECTION,
                                                      "bus-type", bus_type,
                                                      NULL));
      *existing_connection = connection;
    }

  return connection;
}

/**
 * egg_dbus_connection_get_bus:
 * @connection: A #EggDBusConnection.
 *
 * Gets the #EggDBusBus interface proxy for the <literal>org.freedesktop.DBus</literal> interface
 * of the message bus daemon for @connection.
 *
 * This interface proxy is typically used to claim a well-known name on the message bus or to
 * listen for signals like #EggDBusBus::name-acquired or #EggDBusBus::name-lost. For example,
 * to claim the well-known name <literal>com.example.AwesomeProduct</literal> on the session
 * message bus you would do this:
 *
 * <programlisting>
 * EggDBusConnection *connection;
 * EggDBusRequestNameReply request_name_reply;
 * GError *error;
 *
 * connection = egg_dbus_connection_get_for_bus (EGG_DBUS_BUS_TYPE_SESSION);
 *
 * error = NULL;
 * if (!egg_dbus_bus_request_name_sync (egg_dbus_connection_get_bus (connection),
 *                                      EGG_DBUS_CALL_FLAGS_NONE,
 *                                      "com.example.AwesomeProduct",
 *                                      EGG_DBUS_REQUEST_NAME_FLAGS_NONE,
 *                                      &request_name_reply,
 *                                      NULL,
 *                                      &error))
 *   {
 *     g_warning ("Error claiming com.example.AwesomeProduct on session bus: %s", error->message);
 *     g_error_free (error);
 *     goto error;
 *   }
 *
 * if (request_name_reply != EGG_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
 *   {
 *     g_warning ("Could not become primary owner of com.example.AwesomeProduct");
 *     goto error;
 *   }
 *
 * /<!-- -->* We now own com.example.AwesomeProduct  *<!-- -->/
 * </programlisting>
 *
 * Returns: A #EggDBusBus. Do not free this object, it is owned by @connection.
 **/
EggDBusBus *
egg_dbus_connection_get_bus (EggDBusConnection *connection)
{
  EggDBusConnectionPrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_CONNECTION (connection), NULL);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  return EGG_DBUS_QUERY_INTERFACE_BUS (priv->bus_object_proxy);
}


/**
 * egg_dbus_connection_get_unique_name:
 * @connection: A #EggDBusConnection.
 *
 * Gets the unique name (as assigned by the message bus daemon) of @connection. This
 * method can only be used when @connection is a message bus connection.
 *
 * Returns: A unique bus name such as <literal>:1.42</literal>. Do not free this
 * string, it is owned by @connection.
 **/
const gchar *
egg_dbus_connection_get_unique_name (EggDBusConnection *connection)
{
  EggDBusConnectionPrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_CONNECTION (connection), NULL);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  return dbus_bus_get_unique_name (priv->connection);
}


/* ---------------------------------------------------------------------------------------------------- */

static gchar *
concat_objpath_and_name (const gchar *objpath, const gchar *name)
{
  /* this works because ':' is not allowed in an object path */
  return g_strdup_printf ("%s:%s", objpath, name);
}

typedef struct
{
  char *rule;
  EggDBusConnection *connection;
} MatchRuleData;

static MatchRuleData *
match_rule_data_new (const gchar     *rule,
                     EggDBusConnection *connection)
{
  MatchRuleData *data;

  data = g_new0 (MatchRuleData, 1);
  data->rule = g_strdup (rule);
  data->connection = (connection == NULL ? NULL : g_object_ref (connection));

  return data;
}

static void
match_rule_data_free (MatchRuleData *data)
{
  g_free (data->rule);
  if (data->connection != NULL)
    g_object_unref (data->connection);
  g_free (data);
}

static void
add_match_rule_cb (EggDBusBus    *bus,
                   GAsyncResult  *res,
                   MatchRuleData *data)
{
  GError *error;

  error = NULL;
  if (!egg_dbus_bus_add_match_finish (bus,
                                      res,
                                      &error))
    {
      g_warning ("Error adding match rule \"%s\": %s", data->rule, error->message);
      g_error_free (error);
    }

  /*g_debug ("Success adding match rule \"%s\"", data->rule);*/

  match_rule_data_free (data);
}

static void
remove_match_rule_cb (EggDBusBus    *bus,
                      GAsyncResult  *res,
                      MatchRuleData *data)
{
  GError *error;

  error = NULL;
  if (!egg_dbus_bus_remove_match_finish (bus,
                                         res,
                                         &error))
    {
      g_warning ("Error removing match rule \"%s\": %s", data->rule, error->message);
      g_error_free (error);
    }

  /*g_debug ("Success removing match rule \"%s\"", data->rule);*/

  match_rule_data_free (data);
}

/**
 * egg_dbus_connection_get_object_proxy:
 * @connection: A #EggDBusConnection.
 * @name: A message bus name such as <literal>:1.6</literal> or <literal>org.freedesktop.DeviceKit</literal>.
 * @object_path: An object path such as <literal>/org/freedesktop/DeviceKit</literal>.
 *
 * Gets a #EggDBusObjectProxy that represents a remote object at @object_path
 * owned by @name.
 *
 * The returned object proxy can be used to obtain interface proxies for calling methods, listen
 * to signals and read/write properties on the D-Bus interfaces supported by the remote object.
 * See the #EggDBusObjectProxy class for details.
 *
 * This method never fails. If either @name doesn't exist or it doesn't export an object
 * at @object_path, you won't find out until you start invoking messages on it. You can
 * use egg_dbus_object_proxy_introspect() to find out if the remote object exists in addition
 * to what D-Bus interfaces it supports.
 *
 * Note that @connection will track @name and report changes via the #EggDBusObjectProxy:name-owner
 * property on the returned #EggDBusObjectProxy object. To track changes to #EggDBusObjectProxy:name-owner,
 * simply connect to the #GObject::notify signal on the returned object.
 *
 * Returns: A #EggDBusObjectProxy object. Free with g_object_unref().
 **/
EggDBusObjectProxy *
egg_dbus_connection_get_object_proxy (EggDBusConnection       *connection,
                               const gchar             *name,
                               const gchar             *object_path)
{
  EggDBusConnectionPrivate *priv;
  EggDBusObjectProxy *object_proxy;
  gchar *rule;
  gchar *id;
  GList *l;

  g_return_val_if_fail (EGG_DBUS_IS_CONNECTION (connection), NULL);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  id = concat_objpath_and_name (object_path, name);

  object_proxy = g_hash_table_lookup (priv->hash_from_objpath_and_name_to_object_proxy, id);
  if (object_proxy != NULL)
    {
      g_free (id);
      g_object_ref (object_proxy);
      goto out;
    }

  object_proxy = _egg_dbus_object_proxy_new (connection,
                             name,
                             object_path);

  /*g_debug ("created object_proxy for name \"%s\" and path \"%s\" id='%s'", name, object_path, id);*/

  /* In constructed() we create priv->bus_object_proxy.. the construction of
   * that object will lead us here without priv->bus_object_proxy being
   * set. But since priv->bus_object_proxy is never NULL after construction
   * we know the passed in object_proxy must be what priv->bus_object_proxy will
   * be set to.
   *
   * Thus, if, and only if, priv->bus_object_proxy is NULL, we're dealing
   * with the bus object_proxy.
   *
   * Now, we really don't want to listen to signals from the bus by
   * default... otherwise our careful work in only listen to select
   * NameOwnerChanged signals is in vain.. Hence, avoid adding match
   * rules for the bus object_proxy by default.
   */
  if (priv->bus_object_proxy == NULL)
    {
      /* we're dealing with our own bus object_proxy; don't register anything */
      goto out;
    }

  g_hash_table_insert (priv->hash_from_objpath_and_name_to_object_proxy, /* takes ownership of id */
                       (gpointer) id,
                       object_proxy);

  l = g_hash_table_lookup (priv->hash_from_name_to_list_of_proxies, name);
  l = g_list_prepend (l, object_proxy);
  g_hash_table_insert (priv->hash_from_name_to_list_of_proxies,
                       g_strdup (name),
                       l);
  /*g_debug ("added \"%s\" to hash_from_name_to_list_of_proxies", name);*/

  /* listen to all signals on the object path so we can forward these to registered proxies */
  rule = g_strdup_printf ("type='signal',sender='%s',path='%s'",
                          name,
                          object_path);

  /*g_debug ("adding match rule \"%s\"", rule);*/

  egg_dbus_bus_add_match (EGG_DBUS_QUERY_INTERFACE_BUS (priv->bus_object_proxy),
                          0,
                          rule,
                          NULL,
                          (GAsyncReadyCallback) add_match_rule_cb,
                          match_rule_data_new (rule, connection));

  g_free (rule);

  /* watch the name */
  egg_dbus_bus_name_tracker_watch_bus_name (priv->bus_name_tracker, name);

  /*g_debug ("REGISTERED ***** object_proxy for %s @ %s", object_path, name);*/

  /* EggDBusObjectProxy's finalize() method will call _egg_dbus_connection_unregister_object_proxy */

 out:
  return object_proxy;
}

void
_egg_dbus_connection_unregister_object_proxy  (EggDBusConnection *connection,
                                      EggDBusObjectProxy      *object_proxy)
{
  EggDBusConnectionPrivate *priv;
  const gchar *name;
  const gchar *objpath;
  gchar *id;
  gchar *rule;
  GList *l;

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  /* this is called from EggDBusObjectProxy's finalize() method BEFORE name and objpath is freed */

  name = egg_dbus_object_proxy_get_name (object_proxy);
  objpath = egg_dbus_object_proxy_get_object_path (object_proxy);

  id = concat_objpath_and_name (objpath, name);

  g_assert (g_hash_table_remove (priv->hash_from_objpath_and_name_to_object_proxy, id));

  g_free (id);

  l = g_hash_table_lookup (priv->hash_from_name_to_list_of_proxies, name);
  l = g_list_remove (l, object_proxy);
  if (l == NULL)
    {
      g_hash_table_remove (priv->hash_from_name_to_list_of_proxies, name);
    }
  else
    {
      g_hash_table_insert (priv->hash_from_name_to_list_of_proxies, g_strdup (name), l);
    }
  /*g_debug ("removed \"%s\" from hash_from_name_to_list_of_proxies", name);*/

  /* remove match rule */

  /* listen of all signals on the object path so we can forward these to registered proxies */
  rule = g_strdup_printf ("type='signal',sender='%s',path='%s'",
                          name,
                          objpath);

  /*g_debug ("removing match rule \"%s\"", rule);*/

  egg_dbus_bus_remove_match (EGG_DBUS_QUERY_INTERFACE_BUS (priv->bus_object_proxy != NULL ? priv->bus_object_proxy : object_proxy),
                             0,
                             rule,
                             NULL,
                             (GAsyncReadyCallback) remove_match_rule_cb,
                             match_rule_data_new (rule, connection));

  g_free (rule);

  egg_dbus_bus_name_tracker_stop_watching_bus_name (priv->bus_name_tracker, name);

  /*g_debug ("UNREGISTERED *** object_proxy for %s @ %s", objpath, name);*/
}

/* ---------------------------------------------------------------------------------------------------- */

/* This function is used when resolving the 'unique-name' property on
 * EggDBusObjectProxy. That doesn't happen too often.
 *
 * Returns a string or %NULL. Do not free.
 */
gchar *
_egg_dbus_connection_get_owner_for_name (EggDBusConnection *connection,
                                         const gchar     *name)
{
  EggDBusConnectionPrivate *priv;

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  return egg_dbus_bus_name_tracker_get_owner_for_bus_name (priv->bus_name_tracker, name);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
emit_unique_name_changed_for_object_proxy_for_name (EggDBusConnection *connection,
                                             const gchar     *bus_name)
{
  EggDBusConnectionPrivate *priv;
  GList *list_of_proxies;
  GList *l;

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  list_of_proxies = g_hash_table_lookup (priv->hash_from_name_to_list_of_proxies, bus_name);
  /*g_debug ("there are %d proxies with name=\"%s\"", g_list_length (list_of_proxies), bus_name);*/
  for (l = list_of_proxies; l != NULL; l = l->next)
    {
      EggDBusObjectProxy *object_proxy = l->data;
      /*g_debug ("emitting notify::name-owner for object_proxy for name=%s and objpath=%s", bus_name, egg_dbus_object_proxy_get_object_path (object_proxy));*/
      g_object_notify (G_OBJECT (object_proxy), "name-owner");
    }
}

static void
bus_name_lost_owner_cb (EggDBusBusNameTracker *bus_name_tracker,
                        const gchar         *bus_name,
                        const gchar         *unique_bus_name_of_old_owner,
                        EggDBusConnection     *connection)
{
  /*g_debug ("recieved lost-owner for bus_name=%s, old-owner=%s", bus_name, unique_bus_name_of_old_owner);*/
  emit_unique_name_changed_for_object_proxy_for_name (connection, bus_name);
}

static void
bus_name_gained_owner_cb (EggDBusBusNameTracker *bus_name_tracker,
                          const gchar         *bus_name,
                          const gchar         *unique_bus_name_of_new_owner,
                          EggDBusConnection     *connection)
{
  /*g_debug ("recieved gained-owner for bus_name=%s, old-owner=%s", bus_name, unique_bus_name_of_new_owner);*/
  emit_unique_name_changed_for_object_proxy_for_name (connection, bus_name);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  /* the GObject derived instance implementing the interface. This is only a weak reference. */
  GObject *interface_object;

  const EggDBusInterfaceInfo *interface_info;

  EggDBusInterfaceIface *g_iface;

  ExportData *export_data;

  GSList *closures;

  gulong notify_handler_id;

} InterfaceExportData;

struct _ExportData
{
  EggDBusConnection *connection; /* not reffed */

  gchar *object_path;

  GHashTable *dbus_name_to_interface_export_data;
};

static void
remove_export_data_if_empty (ExportData *data)
{
  if (g_hash_table_size (data->dbus_name_to_interface_export_data) == 0)
    {
      EggDBusConnectionPrivate *priv;

      priv = EGG_DBUS_CONNECTION_GET_PRIVATE (data->connection);

      g_hash_table_remove (priv->hash_from_object_path_to_export_data, data->object_path);
    }
}

/* our GWeakNotify'er */
static void
exported_interface_finalized (gpointer  data,
                              GObject  *where_the_object_was)
{
  InterfaceExportData *interface_data = data;

  g_assert (interface_data->interface_object == where_the_object_was);

  /* to avoid accessing an already finalized instance */
  interface_data->interface_object = NULL;

  g_hash_table_remove (interface_data->export_data->dbus_name_to_interface_export_data,
                       interface_data->interface_info->name);
  remove_export_data_if_empty (interface_data->export_data);
}

static void
interface_export_data_free (InterfaceExportData *data)
{
  GSList *l;

  /* data->interface_object is NULL if it's already finalized due to the GWeakNotify'er above */
  if (data->interface_object != NULL)
    {
      for (l = data->closures; l != NULL; l = l->next)
        g_closure_unref (l->data);

      g_signal_handler_disconnect (data->interface_object, data->notify_handler_id);
      g_object_weak_unref (data->interface_object,
                           exported_interface_finalized,
                           data);
    }

  g_slist_free (data->closures);

  g_free (data);
}


typedef struct
{
  GClosure closure;

  gulong signal_handler_id;

  InterfaceExportData *interface_export_data;

  const EggDBusInterfaceSignalInfo *signal_info;

} EggDBusClosure;

static void
export_data_free (ExportData *data)
{
  g_free (data->object_path);
  g_hash_table_unref (data->dbus_name_to_interface_export_data);
  g_free (data);
}

static void
marshal_property_change_onto_dbus (GObject *object,
                                   GParamSpec *pspec,
                                   InterfaceExportData *interface_data)
{
  EggDBusVariant *variant;
  EggDBusHashMap *hash_map;
  const EggDBusInterfacePropertyInfo *property_info;
  EggDBusMessage *signal_message;
  GError *error;

  variant = NULL;
  hash_map = NULL;
  signal_message = NULL;
  error = NULL;

  /* TODO: check marker */

  property_info = egg_dbus_interface_info_lookup_property_for_g_name (interface_data->interface_info,
                                                                      pspec->name);
  if (property_info == NULL)
    {
      g_warning ("Couldn't find property info for property %s on interface %s",
                 pspec->name,
                 interface_data->interface_info->name);
      goto out;
    }

  GValue value = {0};

  g_value_init (&value, pspec->value_type);
  g_object_get_property (interface_data->interface_object,
                         property_info->g_name,
                         &value);

  /* TODO: queue up changes and emit in a) idle; or b) before anything
   * is sent to that name:object-path:interface tripple; whichever
   * comes first
   */

  hash_map = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                                    EGG_DBUS_TYPE_VARIANT, g_object_unref);

  variant = egg_dbus_variant_new_for_gvalue (&value, property_info->signature);

  egg_dbus_hash_map_insert (hash_map, (gpointer) property_info->name, g_object_ref (variant));
  g_value_unset (&value);

  signal_message = egg_dbus_connection_new_message_for_signal (interface_data->export_data->connection,
                                                               NULL,
                                                               NULL,
                                                               interface_data->export_data->object_path,
                                                               "org.freedesktop.DBus.Properties",
                                                               "EggDBusChanged");

  if (!egg_dbus_message_append_string (signal_message,
                                       interface_data->interface_info->name,
                                       &error))
    {
      g_warning ("Error appending interface name for EggDBusChanged signal: %s", error->message);
      g_error_free (error);
      goto out;
    }

  if (!egg_dbus_message_append_map (signal_message,
                                    hash_map, "s", "v",
                                    &error))
    {
      g_warning ("Error appending hash map for EggDBusChanged signal: %s", error->message);
      g_error_free (error);
      goto out;
    }

  egg_dbus_connection_send_message (interface_data->export_data->connection, signal_message);

  //g_debug ("emitting signal for %s (%s) on interface %s (%s)",
  //         pspec->name,
  //         property_info->name,
  //         g_type_name (pspec->owner_type),
  //         interface_info->name);

 out:
  if (variant != NULL)
    g_object_unref (variant);
  if (hash_map != NULL)
    g_object_unref (hash_map);
  if (signal_message != NULL)
    g_object_unref (signal_message);
}

static void
marshal_signal_onto_dbus (GClosure     *_closure,
                          GValue       *return_value,
                          guint         n_param_values,
                          const GValue *param_values,
                          gpointer      invocation_hint,
                          gpointer      marshal_data)
{
  EggDBusClosure *closure = (EggDBusClosure *) _closure;
  EggDBusMessage *signal_message;
  GError *local_error;
  guint n;

  signal_message = NULL;
  local_error = NULL;

  g_assert (closure->signal_info->num_args == n_param_values - 1);

  signal_message = egg_dbus_connection_new_message_for_signal (closure->interface_export_data->export_data->connection,
                                                               NULL,
                                                               NULL,
                                                               closure->interface_export_data->export_data->object_path,
                                                               closure->interface_export_data->interface_info->name,
                                                               closure->signal_info->name);

  //g_debug ("%s: emitting signal %s on interface %s", G_STRFUNC, closure->signal_info->name, closure->interface_info->name);

  /* populate signal_message */
  for (n = 0; n < n_param_values - 1; n++)
    {
      if (!egg_dbus_message_append_gvalue (signal_message,
                                           &(param_values[n + 1]),
                                           closure->signal_info->args[n].signature,
                                           &local_error))
        {
          g_warning ("%s: Error appending arg %u of signature %s onto signal: %s",
                     G_STRFUNC,
                     n,
                     closure->signal_info->args[n].signature,
                     local_error->message);
          g_error_free (local_error);
          goto out;
        }
    }

  egg_dbus_connection_send_message (closure->interface_export_data->export_data->connection, signal_message);

 out:
  if (signal_message != NULL)
    g_object_unref (signal_message);
}

/**
 * egg_dbus_connection_lookup_interface:
 * @connection: A #EggDBusConnection.
 * @object_path: The object path to lookup registered interfaces for.
 * @out_interface_types: Return location for the #GInterface types registered at @object_path or %NULL.
 * @out_interface_stubs: Return location for the #GObject derived instances implementing the
 * corresponding #GInterface or %NULL.
 *
 * This method looks up the interfaces at @object_path for @connection registered
 * using egg_dbus_connection_register_interface(). This method is only useful when
 * exporting a D-Bus service.
 *
 * Returns: Number of interfaces registered at @object_path. If zero is
 * returned @out_interface_types and @out_interface_stubs will be set
 * to %NULL. Otherwise caller must free both of these arrays using
 * g_free().
 **/
guint
egg_dbus_connection_lookup_interface (EggDBusConnection   *connection,
                                      const gchar         *object_path,
                                      GType              **out_interface_types,
                                      GObject           ***out_interface_stubs)
{
  EggDBusConnectionPrivate *priv;
  ExportData *data;
  guint num_interfaces;
  GType *interface_types;
  GObject **interface_stubs;
  GHashTableIter iface_iter;
  InterfaceExportData *interface_data;
  guint n;

  g_return_val_if_fail (EGG_DBUS_IS_CONNECTION (connection), 0);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  num_interfaces = 0;
  interface_types = NULL;
  interface_stubs = NULL;

  data = g_hash_table_lookup (priv->hash_from_object_path_to_export_data, object_path);
  if (data == NULL)
    goto out;

  num_interfaces = g_hash_table_size (data->dbus_name_to_interface_export_data);
  if (num_interfaces == 0)
    goto out;

  interface_types = g_new0 (GType, num_interfaces);
  interface_stubs = g_new0 (GObject*, num_interfaces);

  g_hash_table_iter_init (&iface_iter, data->dbus_name_to_interface_export_data);
  n = 0;
  while (g_hash_table_iter_next (&iface_iter,
                                 NULL,
                                 (gpointer) &interface_data))
    {
      interface_types[n] = G_TYPE_FROM_INTERFACE (interface_data->g_iface);
      interface_stubs[n] = interface_data->interface_object;
      n++;
    }

 out:
  if (out_interface_types != NULL)
    *out_interface_types = interface_types;

  if (out_interface_stubs != NULL)
    *out_interface_stubs = interface_stubs;

  return num_interfaces;
}


/**
 * egg_dbus_connection_unregister_interface:
 * @connection: A #EggDBusConnection.
 * @object_path: The object path to unregister the interface from.
 * @interface_type: A #GType for the type of #GInterface that represents the D-Bus interface to be unregistered.
 * @...: Zero or more #GType<!-- -->s (like @interface_type), terminated by #G_TYPE_INVALID.
 *
 * Unregisters one or more D-Bus interfaces at @object_path on @connection previously
 * registered with egg_dbus_connection_register_interface(). This method is only useful
 * when exporting a D-Bus service.
 **/
void
egg_dbus_connection_unregister_interface (EggDBusConnection   *connection,
                                          const gchar         *object_path,
                                          GType                interface_type,
                                          ...)
{
  va_list va_args;

  va_start (va_args, interface_type);
  egg_dbus_connection_unregister_interface_valist (connection,
                                                   object_path,
                                                   interface_type,
                                                   va_args);
  va_end (va_args);
}

/**
 * egg_dbus_connection_unregister_interface_valist:
 * @connection: A #EggDBusConnection.
 * @object_path: The object path to unregister the interface from.
 * @interface_type: A #GType for the type of #GInterface that represents the D-Bus interface to be unregistered.
 * @var_args: A #va_list with zero or more #GType<!-- -->s (like @interface_type), terminated by #G_TYPE_INVALID.
 *
 * Like egg_dbus_connection_unregister_interface() but intended for
 * language bindings.
 **/
void
egg_dbus_connection_unregister_interface_valist (EggDBusConnection   *connection,
                                                 const gchar         *object_path,
                                                 GType                interface_type,
                                                 va_list              var_args)
{
  EggDBusConnectionPrivate *priv;
  ExportData *data;

  g_return_if_fail (EGG_DBUS_IS_CONNECTION (connection));

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  data = g_hash_table_lookup (priv->hash_from_object_path_to_export_data, object_path);
  if (data == NULL)
    goto out;

  for (; interface_type != G_TYPE_INVALID; interface_type = va_arg (var_args, GType))
    {
      EggDBusInterfaceIface *g_iface;
      const EggDBusInterfaceInfo *interface_info;
      InterfaceExportData *interface_data;

      /* if type isn't currently in use, it can't be exported anywhere */
      g_iface = g_type_default_interface_peek (interface_type);
      if (g_iface == NULL)
        continue;

      interface_info = g_iface->get_interface_info ();

      interface_data = g_hash_table_lookup (data->dbus_name_to_interface_export_data,
                                            interface_info->name);

      if (interface_data == NULL)
        continue;

      g_hash_table_remove (data->dbus_name_to_interface_export_data,
                           interface_info->name);
      remove_export_data_if_empty (data);
    }

 out:
  ;
}

/**
 * egg_dbus_connection_register_interface:
 * @connection: A #EggDBusConnection.
 * @object_path: The object path to register the interface on.
 * @interface_type: A #GType for the type of #GInterface that represents the D-Bus interface to be registered.
 * @...: A #GObject derived instance implementing @interface_type, followed by more <literal>(type, instance pairs)</literal>, terminated by #G_TYPE_INVALID.
 *
 * Registers one or more D-Bus interfaces at @object_path on @connection. This function
 * is only useful when exporting a D-Bus service.
 *
 * This function may be called multiple times for the same @object_path. If an existing
 * #GType for an interface is already registered, it will be replaced.
 *
 * Only a weak reference to the given #GObject instances will be taken; if a registered object is
 * finalized it will automatically be unregistered. Use egg_dbus_connection_unregister_interface() to
 * manually unregister the interface.
 *
 * Note that the #EggDBusProperties, #EggDBusIntrospectable and #EggDBusPeer interfaces will be
 * automatically handled for @object_path unless they specifically registered.
 *
 * For objects with relatively simple D-Bus interfaces (where there are no name clashes on property
 * and signal names), a single #GObject derived class implementing multiple #GInterface<!-- -->s (each
 * corresponding to a D-Bus interface) can be used. For more complex objects (with name clashes),
 * separate #GObject derived classes (typically one for each D-Bus interface) must be used for
 * disambiguation.
 */
void
egg_dbus_connection_register_interface (EggDBusConnection   *connection,
                                        const gchar         *object_path,
                                        GType                interface_type,
                                        ...)
{
  va_list va_args;

  va_start (va_args, interface_type);
  egg_dbus_connection_register_interface_valist (connection,
                                                 object_path,
                                                 interface_type,
                                                 va_args);
  va_end (va_args);
}

/**
 * egg_dbus_connection_register_interface_valist:
 * @connection: A #EggDBusConnection.
 * @object_path: The object path to register the interface on.
 * @interface_type: A #GType for the type of #GInterface that represents the D-Bus interface to be registered.
 * @var_args: A #va_list containing a #GObject derived instance implementing @interface_type, followed by more <literal>(type, instance pairs)</literal>, terminated by #G_TYPE_INVALID.
 *
 * Like egg_dbus_connection_register_interface() but intended for
 * language bindings.
 **/
void
egg_dbus_connection_register_interface_valist (EggDBusConnection   *connection,
                                               const gchar         *object_path,
                                               GType                interface_type,
                                               va_list              var_args)
{
  EggDBusConnectionPrivate *priv;
  ExportData *data;

  g_return_if_fail (EGG_DBUS_IS_CONNECTION (connection));

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  //g_debug ("exporting object %p at %s for connection with unique name %s",
  //         object,
  //         object_path,
  //         egg_dbus_connection_get_unique_name (connection));

  data = g_hash_table_lookup (priv->hash_from_object_path_to_export_data, object_path);
  if (data == NULL)
    {
      data = g_new0 (ExportData, 1);
      data->connection = connection; /* no ref */
      data->object_path = g_strdup (object_path);
      data->dbus_name_to_interface_export_data = g_hash_table_new_full (g_str_hash,
                                                                        g_str_equal,
                                                                        NULL,
                                                                        (GDestroyNotify) interface_export_data_free);
      g_hash_table_insert (priv->hash_from_object_path_to_export_data,
                           data->object_path,
                           data);
    }


  for (; interface_type != G_TYPE_INVALID; interface_type = va_arg (var_args, GType))
    {
      GObject *interface_object;
      EggDBusInterfaceIface *g_iface;
      const EggDBusInterfaceInfo *interface_info;
      InterfaceExportData *interface_data;
      guint *signal_ids;
      guint num_signal_ids;
      guint n;

      interface_object = va_arg (var_args, GObject*);
      g_assert (interface_object != NULL);

      g_iface = g_type_interface_peek (G_OBJECT_GET_CLASS (interface_object), interface_type);

      interface_info = g_iface->get_interface_info ();

      interface_data = g_hash_table_lookup (data->dbus_name_to_interface_export_data,
                                            interface_info->name);

      if (interface_data != NULL)
        {
          /* we're already exporting this interface at @object_path; remove the existing one */
          egg_dbus_connection_unregister_interface (connection,
                                                    object_path,
                                                    interface_type,
                                                    G_TYPE_INVALID);
        }

      interface_data = g_new0 (InterfaceExportData, 1);
      interface_data->interface_object = interface_object;
      interface_data->interface_info = interface_info;
      interface_data->g_iface = g_iface;
      interface_data->export_data = data;

      g_object_weak_ref (interface_object,
                         exported_interface_finalized,
                         interface_data);

      /* connect to all signals on interface so we can marshal those onto the bus */
      signal_ids = g_signal_list_ids (interface_type, &num_signal_ids);
      for (n = 0; n < num_signal_ids; n++)
        {
          GSignalQuery query;
          EggDBusClosure *closure;

          g_signal_query (signal_ids[n], &query);

          closure = (EggDBusClosure *) g_closure_new_simple (sizeof (EggDBusClosure), interface_data);
          closure->signal_info = egg_dbus_interface_info_lookup_signal_for_g_name (interface_info, query.signal_name);
          if (closure->signal_info == NULL)
            {
              g_warning ("Couldn't find signal info for signal %s on interface %s",
                         query.signal_name,
                         interface_info->name);
            }
          closure->interface_export_data = interface_data;

          g_closure_set_marshal ((GClosure *) closure, marshal_signal_onto_dbus);

          closure->signal_handler_id = g_signal_connect_closure_by_id (interface_data->interface_object,
                                                                       signal_ids[n],
                                                                       0,
                                                                       (GClosure *) closure,
                                                                       TRUE);

          /* for later cleanup */
          interface_data->closures = g_slist_prepend (interface_data->closures, closure);
        }

      /* connect to the the GObject::notify signal to catch all property changes
       * so we can generate EggDBusChanged() signals
       */
      interface_data->notify_handler_id = g_signal_connect (interface_data->interface_object,
                                                            "notify",
                                                            (GCallback) marshal_property_change_onto_dbus,
                                                            interface_data);

      g_hash_table_insert (data->dbus_name_to_interface_export_data,
                           (gpointer) interface_info->name,
                           interface_data);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
append_introspection_xml_for_interface_type (GString *s,
                                             GType    interface_type)
{
  const EggDBusInterfaceInfo *interface_info;
  EggDBusInterfaceIface *g_iface;

  g_iface = g_type_default_interface_peek (interface_type);
  if (g_iface == NULL)
    g_iface = g_type_default_interface_ref (interface_type);

  interface_info = g_iface->get_interface_info ();

  egg_dbus_interface_info_to_xml (interface_info, 2, s);
}


static gchar *
compute_introspection_xml (EggDBusConnection *connection,
                           ExportData      *export_data,
                           const gchar     *object_path)
{
  GString *s;
  GHashTableIter object_iter;
  const gchar *iter_object_path;
  guint object_path_len;
  gboolean object_path_is_root;
  EggDBusConnectionPrivate *priv;
  GHashTable *nodes_seen;

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  s = g_string_new ("<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
                    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n");

  g_string_append_printf (s,
                          "<node name=\"%s\">\n",
                          object_path);

  object_path_is_root = (strcmp (object_path, "/") == 0);
  object_path_len = strlen (object_path);

  /* print information about this node */
  if (export_data != NULL)
    {
      GHashTableIter iface_iter;
      InterfaceExportData *interface_data;
      gboolean has_properties_interface;
      gboolean has_introspectable_interface;
      gboolean has_peer_interface;

      has_properties_interface = FALSE;
      has_introspectable_interface = FALSE;
      has_peer_interface = FALSE;

      g_hash_table_iter_init (&iface_iter, export_data->dbus_name_to_interface_export_data);
      while (g_hash_table_iter_next (&iface_iter,
                                     NULL,
                                     (gpointer) &interface_data))
        {
          if (strcmp (interface_data->interface_info->name, "org.freedesktop.DBus.Properties") == 0)
            has_properties_interface = TRUE;

          if (strcmp (interface_data->interface_info->name, "org.freedesktop.DBus.Introspectable") == 0)
            has_introspectable_interface = TRUE;

          if (strcmp (interface_data->interface_info->name, "org.freedesktop.DBus.Peer") == 0)
            has_peer_interface = TRUE;

          egg_dbus_interface_info_to_xml (interface_data->interface_info, 2, s);
        }

      if (!has_properties_interface)
        append_introspection_xml_for_interface_type (s, EGG_DBUS_TYPE_PROPERTIES);

      if (!has_introspectable_interface)
        append_introspection_xml_for_interface_type (s, EGG_DBUS_TYPE_INTROSPECTABLE);

      if (!has_peer_interface)
        append_introspection_xml_for_interface_type (s, EGG_DBUS_TYPE_PEER);

    } /* for all interfaces */


  /* print all nodes below. If this turns out to be slow, we can construct
   * a lookaside tree or something.
   *
   * nodes_seen is used as a poor mans set data structureto avoid printing
   * the same path out twice
   */
  nodes_seen = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_iter_init (&object_iter, priv->hash_from_object_path_to_export_data);
  while (g_hash_table_iter_next (&object_iter,
                                 (gpointer) &iter_object_path,
                                 NULL))
    {

      if (g_str_has_prefix (iter_object_path, object_path) &&
          (iter_object_path[object_path_len] == '/' || object_path_is_root))
        {
          const gchar *begin;
          const gchar *end;
          gchar *node_name;

          if (object_path_is_root)
            begin = iter_object_path + 1;
          else
            begin = iter_object_path + object_path_len + 1;

          end = strchr (begin, '/');

          if (end == NULL)
            node_name = g_strdup (begin);
          else
            node_name = g_strndup (begin, end - begin);

          if (g_hash_table_lookup (nodes_seen, node_name) == NULL)
            {
              g_string_append_printf (s, "  <node name=\"%s\"/>\n", node_name);
              g_hash_table_insert (nodes_seen, node_name, node_name);
            }

          g_free (node_name);
        }

    }
  g_hash_table_unref (nodes_seen);

  g_string_append (s, "</node>\n");

  return g_string_free (s, FALSE);
}

static DBusHandlerResult
handle_introspection (EggDBusConnection *connection,
                      ExportData        *export_data,
                      EggDBusMessage    *message,
                      DBusMessage       *dmessage,
                      const gchar       *object_path)
{
  EggDBusMessage *reply;
  gchar *xml_data;
  DBusHandlerResult result;
  GError *error;

  reply = NULL;
  xml_data = NULL;
  result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (strcmp (egg_dbus_message_get_method_name (message), "Introspect") != 0 ||
      strcmp (egg_dbus_message_get_signature (message), "") != 0)
    goto out;

  reply = egg_dbus_message_new_for_method_reply (message);

  xml_data = compute_introspection_xml (connection,
                                        export_data,
                                        object_path);

  error = NULL;
  if (!egg_dbus_message_append_string (reply, xml_data, &error))
    {
      g_warning ("%s: Error appending XML introspection data: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

  egg_dbus_connection_send_message (connection, reply);

  result = DBUS_HANDLER_RESULT_HANDLED;

 out:
  g_free (xml_data);
  if (reply != NULL)
    g_object_unref (reply);
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

static DBusHandlerResult
handle_set_property (EggDBusConnection *connection,
                     ExportData        *export_data,
                     EggDBusMessage    *message)
{
  EggDBusMessage *reply;
  DBusHandlerResult result;
  GError *error;
  gchar *interface_name;
  gchar *property_name;
  EggDBusVariant *variant;
  InterfaceExportData *interface_data;
  const EggDBusInterfacePropertyInfo *property_info;
  GParamSpec *pspec;
  GValue value = {0};

  reply = NULL;
  error = NULL;
  interface_name = NULL;
  property_name = NULL;
  variant = NULL;
  result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (!egg_dbus_message_extract_string (message, &interface_name, &error))
    {
      g_warning ("%s: Cannot extract interface name: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

  if (!egg_dbus_message_extract_string (message, &property_name, &error))
    {
      g_warning ("%s: Cannot extract property name: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

  if (!egg_dbus_message_extract_variant (message, &variant, &error))
    {
      g_warning ("%s: Cannot extract property value: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

  interface_data = g_hash_table_lookup (export_data->dbus_name_to_interface_export_data, interface_name);

  /* if object doesn't implement the interface asked for, return error */
  if (interface_data == NULL)
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Object does not implement given interface");
      goto send_message;
    }

  /* TODO: handle interface_name being the empty string (spec requires handling this) */

  property_info = egg_dbus_interface_info_lookup_property_for_name (interface_data->interface_info,
                                                                    property_name);

  /* if interface doesn't have the property asked for, return error */
  if (property_info == NULL)
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Given property does not exist on the given interface");
      goto send_message;
    }

  /* property must be writable */
  if (! (property_info->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE))
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Given property is not writable");
      goto send_message;
    }

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (interface_data->interface_object),
                                        property_info->g_name);

  /* if property isn't on the object, return error */
  if (pspec == NULL)
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Error finding requested property");
      goto send_message;
    }

  g_value_init (&value, pspec->value_type);
  g_object_set_property (interface_data->interface_object,
                         property_info->g_name,
                         egg_dbus_variant_get_gvalue (variant));

  reply = egg_dbus_message_new_for_method_reply (message);

 send_message:

  egg_dbus_connection_send_message (connection, reply);

  result = DBUS_HANDLER_RESULT_HANDLED;

 out:
  g_free (interface_name);
  g_free (property_name);
  if (variant != NULL)
    g_object_unref (variant);
  if (reply != NULL)
    g_object_unref (reply);
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

static DBusHandlerResult
handle_get_property (EggDBusConnection *connection,
                     ExportData      *export_data,
                     EggDBusMessage    *message)
{
  EggDBusMessage *reply;
  DBusHandlerResult result;
  GError *error;
  gchar *interface_name;
  gchar *property_name;
  EggDBusVariant *variant;
  InterfaceExportData *interface_data;
  const EggDBusInterfacePropertyInfo *property_info;
  GParamSpec *pspec;
  GValue value = {0};

  reply = NULL;
  error = NULL;
  interface_name = NULL;
  property_name = NULL;
  variant = NULL;
  result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (!egg_dbus_message_extract_string (message, &interface_name, &error))
    {
      g_warning ("%s: Cannot extract interface name: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

  if (!egg_dbus_message_extract_string (message, &property_name, &error))
    {
      g_warning ("%s: Cannot extract property name: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

  interface_data = g_hash_table_lookup (export_data->dbus_name_to_interface_export_data, interface_name);

  /* if object doesn't implement the interface asked for, return error */
  if (interface_data == NULL)
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Object does not implement given interface");
      goto send_message;
    }

  /* TODO: handle interface_name being the empty string (spec requires handling this) */

  property_info = egg_dbus_interface_info_lookup_property_for_name (interface_data->interface_info,
                                                                    property_name);

  /* if interface doesn't have the property asked for, return error */
  if (property_info == NULL)
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Given property does not exist on the given interface");
      goto send_message;
    }

  /* property must be readable */
  if (! (property_info->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE))
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Given property is not readable");
      goto send_message;
    }

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (interface_data->interface_object),
                                        property_info->g_name);

  /* if property isn't on the object, return error */
  if (pspec == NULL)
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Error finding requested property");
      goto send_message;
    }

  g_value_init (&value, pspec->value_type);
  g_object_get_property (interface_data->interface_object,
                         property_info->g_name,
                         &value);

  variant = egg_dbus_variant_new_for_gvalue (&value, property_info->signature);
  g_value_unset (&value);

  reply = egg_dbus_message_new_for_method_reply (message);

  if (!egg_dbus_message_append_variant (reply, variant, &error))
    {
      g_warning ("%s: Error appending variant: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

 send_message:

  egg_dbus_connection_send_message (connection, reply);

  result = DBUS_HANDLER_RESULT_HANDLED;

 out:
  g_free (interface_name);
  g_free (property_name);
  if (variant != NULL)
    g_object_unref (variant);
  if (reply != NULL)
    g_object_unref (reply);
  return result;
}

static DBusHandlerResult
handle_get_all_properties (EggDBusConnection *connection,
                           ExportData      *export_data,
                           EggDBusMessage    *message)
{
  EggDBusMessage *reply;
  DBusHandlerResult result;
  GError *error;
  gchar *interface_name;
  InterfaceExportData *interface_data;
  EggDBusHashMap *hash_from_string_to_variant;
  guint n;

  reply = NULL;
  error = NULL;
  interface_name = NULL;
  hash_from_string_to_variant = NULL;
  result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (!egg_dbus_message_extract_string (message, &interface_name, &error))
    {
      g_warning ("%s: Cannot extract interface name: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

  interface_data = g_hash_table_lookup (export_data->dbus_name_to_interface_export_data, interface_name);

  /* if object doesn't implement the interface asked for, return error */
  if (interface_data == NULL)
    {
      reply = egg_dbus_message_new_for_method_error_reply (message,
                                                           "org.gtk.EggDBus.Error.Failed",
                                                           "Object does not implement given interface");
      goto send_message;
    }

  /* TODO: handle interface_name being the empty string (spec requires handling this) */

  hash_from_string_to_variant = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                                                       EGG_DBUS_TYPE_VARIANT, g_object_unref);

  for (n = 0; n < interface_data->interface_info->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *property_info = interface_data->interface_info->properties + n;
      GParamSpec *pspec;
      EggDBusVariant *variant;
      GValue value = {0};

      /* property must be readable */
      if (! (property_info->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE))
        continue;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (interface_data->interface_object),
                                            property_info->g_name);

      /* if property isn't on the object, log an error */
      if (pspec == NULL)
        {
          g_warning ("%s: no pspec for property %s", G_STRFUNC, property_info->g_name);
          continue;
        }

      g_value_init (&value, pspec->value_type);
      g_object_get_property (interface_data->interface_object,
                             property_info->g_name,
                             &value);

      variant = egg_dbus_variant_new_for_gvalue (&value, property_info->signature);
      g_value_unset (&value);

      egg_dbus_hash_map_insert (hash_from_string_to_variant, (gpointer) property_info->name, variant);
    }


  reply = egg_dbus_message_new_for_method_reply (message);

  if (!egg_dbus_message_append_map (reply, hash_from_string_to_variant, "s", "v", &error))
    {
      g_warning ("%s: Error appending hash table: %s", G_STRFUNC, error->message);
      g_error_free (error);
      goto out;
    }

 send_message:

  egg_dbus_connection_send_message (connection, reply);

  result = DBUS_HANDLER_RESULT_HANDLED;

 out:
  g_free (interface_name);
  if (hash_from_string_to_variant != NULL)
    g_object_unref (hash_from_string_to_variant);
  if (reply != NULL)
    g_object_unref (reply);
  return result;
}


static DBusHandlerResult
handle_properties (EggDBusConnection *connection,
                   ExportData      *export_data,
                   EggDBusMessage    *message)
{
  if (strcmp (egg_dbus_message_get_method_name (message), "Get") == 0 &&
      strcmp (egg_dbus_message_get_signature (message), "ss") == 0)
    {
      return handle_get_property (connection, export_data, message);
    }
  else if (strcmp (egg_dbus_message_get_method_name (message), "GetAll") == 0 &&
           strcmp (egg_dbus_message_get_signature (message), "s") == 0)
    {
      return handle_get_all_properties (connection, export_data, message);
    }
  else if (strcmp (egg_dbus_message_get_method_name (message), "Set") == 0 &&
           strcmp (egg_dbus_message_get_signature (message), "ssv") == 0)
    {
      return handle_set_property (connection, export_data, message);
    }
  else
    {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static DBusHandlerResult
filter_function_handle_method_call (DBusConnection *dconnection,
                                    DBusMessage    *dmessage,
                                    void           *user_data)
{
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;
  DBusHandlerResult result;
  const char *objpath;
  const char *sender;
  const char *interface_name;
  const char *method_name;
  ExportData *data;
  EggDBusMessage *message;

  connection = EGG_DBUS_CONNECTION (user_data);
  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  message = NULL;

  objpath = dbus_message_get_path (dmessage);
  sender = dbus_message_get_sender (dmessage);
  interface_name = dbus_message_get_interface (dmessage);
  method_name = dbus_message_get_member (dmessage);

  //g_debug ("filter_function_handle_method_call: objpath='%s', sender='%s' if='%s' member='%s'", objpath, sender, interface_name, dbus_message_get_member (dmessage));

  if (sender == NULL)
    {
      g_warning ("no sender");
      goto out;
    }

  if (objpath == NULL)
    {
      g_warning ("no objpath");
      goto out;
    }

  if (interface_name == NULL)
    {
      g_warning ("no interface");
      goto out;
    }

  if (method_name == NULL)
    {
      g_warning ("no method_name");
      goto out;
    }

  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             dbus_message_get_sender (dmessage),
                                                             NULL,
                                                             objpath,
                                                             interface_name,
                                                             method_name);

  /* overwrites the DBusMessage that EggDBusMessage creates */
  g_object_set_data_full (G_OBJECT (message),
                          "dbus-1-message",
                          dbus_message_ref (dmessage),
                          (GDestroyNotify) dbus_message_unref);

  data = g_hash_table_lookup (priv->hash_from_object_path_to_export_data, objpath);
  if (data == NULL)
    {
      /* handle introspection if object is not known */
      if (strcmp (interface_name, "org.freedesktop.DBus.Introspectable") == 0)
        result = handle_introspection (connection, data, message, dmessage, objpath);
    }
  else
    {
      InterfaceExportData *interface_data;

      interface_data = g_hash_table_lookup (data->dbus_name_to_interface_export_data,
                                            interface_name);

      if (interface_data == NULL)
        {
          if (strcmp (interface_name, "org.freedesktop.DBus.Introspectable") == 0)
            {
              /* handle introspection if the object itself does not
               * implement org.freedesktop.DBus.Introspectable
               */
              result = handle_introspection (connection, data, message, dmessage, objpath);
            }
          else if (strcmp (interface_name, "org.freedesktop.DBus.Properties") == 0)
            {
              /* handle properties if the object itself does not
               * implement org.freedesktop.DBus.Properties
               */
              result = handle_properties (connection, data, message);
            }
        }
      else
        {
          /* TODO: should probably make handle_message return a bool whether the message is
           * handled (entry in vtable may be NULL)
           */

          interface_data->g_iface->handle_message ((gpointer) interface_data->interface_object,
                                                   message);
          result = DBUS_HANDLER_RESULT_HANDLED;
        }
    }

 out:
  if (message != NULL)
    g_object_unref (message);
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_connection_send_message:
 * @connection: A #EggDBusConnection.
 * @message: A #EggDBusMessage.
 *
 * Sends @message on @connection without waiting for a reply. This method is
 * only useful for language bindings.
 **/
void
egg_dbus_connection_send_message (EggDBusConnection       *connection,
                                  EggDBusMessage          *message)
{
  DBusMessage *dmessage;
  EggDBusConnectionPrivate *priv;

  g_return_if_fail (EGG_DBUS_IS_CONNECTION (connection));
  g_return_if_fail (message != NULL);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  dmessage = g_object_get_data (G_OBJECT (message), "dbus-1-message");

  dbus_connection_send (priv->connection,
                        dmessage,
                        NULL);
}

static void
egg_dbus_connection_send_message_with_reply_sync_cb (EggDBusConnection *connection,
                                                     GAsyncResult      *res,
                                                     GAsyncResult     **data)
{
  *data = g_object_ref (res);
}

/**
 * egg_dbus_connection_send_message_with_reply_sync:
 * @connection: A #EggDBusConnection.
 * @call_flags: Flags from #EggDBusCallFlags detailing how the message should be sent.
 * @message: The message to send.
 * @error_types: Either %NULL or a #G_TYPE_INVALID terminated list of #GType<!-- -->s for #GError error domains to use
 * when processing an error reply.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error.
 *
 * Queues @message to be sent and waits until a reply arrives. This method is
 * only useful for language bindings.
 *
 * Returns: The reply or %NULL if @error is set.
 **/
EggDBusMessage *
egg_dbus_connection_send_message_with_reply_sync (EggDBusConnection       *connection,
                                                  EggDBusCallFlags         call_flags,
                                                  EggDBusMessage          *message,
                                                  GType                   *error_types,
                                                  GCancellable            *cancellable,
                                                  GError                 **error)
{
  EggDBusMessage *reply;
  GAsyncResult *res;
  guint pending_call_id;

  g_return_val_if_fail (EGG_DBUS_IS_CONNECTION (connection), NULL);
  g_return_val_if_fail (message != NULL, NULL);

  res = NULL;

  pending_call_id = egg_dbus_connection_send_message_with_reply (connection,
                                                                 call_flags,
                                                                 message,
                                                                 error_types,
                                                                 cancellable,
                                                                 (GAsyncReadyCallback) egg_dbus_connection_send_message_with_reply_sync_cb,
                                                                 &res);

  egg_dbus_connection_pending_call_block (connection, pending_call_id);

  g_assert (res != NULL);

  reply = egg_dbus_connection_send_message_with_reply_finish (connection,
                                                              res,
                                                              error);

  g_object_unref (res);
  return reply;
}

static void
egg_dbus_connection_send_message_with_reply_cancelled_cb (GCancellable *cancellable,
                                                          GSimpleAsyncResult *simple)
{
  EggDBusConnection *connection;
  guint pending_call_id;

  connection = EGG_DBUS_CONNECTION (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
  pending_call_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (simple), "egg-dbus-pending-call-id"));
  egg_dbus_connection_pending_call_cancel (connection, pending_call_id);
  g_object_unref (connection);
}

static void
egg_dbus_connection_send_message_with_reply_cb (DBusPendingCall    *pending_call,
                                                GSimpleAsyncResult *simple)
{
  DBusMessage *dreply;
  DBusError derror;
  EggDBusConnection *connection;
  EggDBusConnectionPrivate *priv;
  EggDBusMessage *message;
  guint pending_call_id;
  GMainLoop *loop;
  GCancellable *cancellable;

  connection = EGG_DBUS_CONNECTION (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  message = g_object_get_data (G_OBJECT (simple), "egg-dbus-message");

  /* see egg_dbus_connection_pending_call_cancel(), it invokes this function on cancellation */
  if (pending_call == NULL)
    {
      g_simple_async_result_set_error (simple,
                                       EGG_DBUS_ERROR,
                                       EGG_DBUS_ERROR_CANCELLED,
                                       "Method call %s.%s() was cancelled",
                                       egg_dbus_message_get_interface_name (message),
                                       egg_dbus_message_get_method_name (message));
    }
  else
    {
      dreply = dbus_pending_call_steal_reply (pending_call);
      dbus_error_init (&derror);
      if (dbus_set_error_from_message (&derror, dreply))
        {
          GError *error;
          GType *error_types;

          error_types = g_object_get_data (G_OBJECT (simple), "egg-dbus-error-types");

          error = _egg_dbus_error_new_remote_exception (derror.name,
                                                        derror.message,
                                                        error_types,
                                                        "Remote Exception invoking %s.%s() on %s at name %s: %s: %s",
                                                        egg_dbus_message_get_interface_name (message),
                                                        egg_dbus_message_get_method_name (message),
                                                        egg_dbus_message_get_object_path (message),
                                                        egg_dbus_message_get_destination (message),
                                                        derror.name,
                                                        derror.message);

          /* TODO: would be nice to avoid copying the error here */
          g_simple_async_result_set_from_error (simple, error);
          g_error_free (error);

          dbus_error_free (&derror);
          dbus_message_unref (dreply);
        }
      else
        {
          EggDBusMessage *reply;

          reply = egg_dbus_message_new_for_method_reply (message);

          /* overwrites the DBusMessage that EggDBusMessage creates */
          g_object_set_data_full (G_OBJECT (reply),
                                  "dbus-1-message",
                                  dreply,
                                  (GDestroyNotify) dbus_message_unref);


          g_simple_async_result_set_op_res_gpointer (simple, reply, (GDestroyNotify) g_object_unref);
        }
    }

  pending_call_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (simple), "egg-dbus-pending-call-id"));
  g_hash_table_remove (priv->hash_pending_call_id_to_async_result, GUINT_TO_POINTER (pending_call_id));

  g_simple_async_result_complete (simple);

  /* quit the mainloop if someone is blocking on us - see egg_dbus_connection_pending_call_block() */
  loop = g_object_get_data (G_OBJECT (simple), "egg-dbus-main-loop");
  if (loop != NULL)
    g_main_loop_quit (loop);

  /* disconnect the "cancelled" signal handler if we have a GCancellable */
  cancellable = g_object_get_data (G_OBJECT (simple), "egg-dbus-cancellable");
  if (cancellable != NULL)
    g_signal_handlers_disconnect_by_func (cancellable,
                                          egg_dbus_connection_send_message_with_reply_cancelled_cb,
                                          simple);

  if (pending_call != NULL)
    dbus_pending_call_unref (pending_call);
  g_object_unref (connection);
}

/**
 * egg_dbus_connection_pending_call_cancel:
 * @connection: A #EggDBusConnection.
 * @pending_call_id: A valid pending call id obtained from egg_dbus_connection_send_message_with_reply()
 * or similar wrapper functions (such as egg_dbus_bus_request_name()).
 *
 * Cancels an asynchronous method invocation expecting a reply. The
 * #GAsyncReadyCallback callback given to
 * egg_dbus_connection_send_message_with_reply() will be invoked
 * before this function returns.
 **/
void
egg_dbus_connection_pending_call_cancel (EggDBusConnection *connection,
                                         guint              pending_call_id)
{
  EggDBusConnectionPrivate *priv;
  GSimpleAsyncResult *simple;
  DBusPendingCall *pending_call;

  g_return_if_fail (EGG_DBUS_IS_CONNECTION (connection));

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  simple = g_hash_table_lookup (priv->hash_pending_call_id_to_async_result, GUINT_TO_POINTER (pending_call_id));
  if (simple == NULL)
    {
      g_warning ("No pending call with id %u", pending_call_id);
      return;
    }

  pending_call = (DBusPendingCall *) g_object_get_data (G_OBJECT (simple), "dbus-1-pending-call");

  g_assert (pending_call != NULL);

  /* this won't trigger a callback */
  dbus_pending_call_cancel (pending_call);

  /* generate a callback with a EGG_DBUS_ERROR_CANCELLED error, then do away with the pending call */
  egg_dbus_connection_send_message_with_reply_cb (NULL, simple);
  dbus_pending_call_unref (pending_call);
}

/**
 * egg_dbus_connection_pending_call_block:
 * @connection: A #EggDBusConnection.
 * @pending_call_id: A valid pending call id obtained from egg_dbus_connection_send_message_with_reply()
 * or similar wrapper functions (such as egg_dbus_bus_request_name()).
 *
 * Blocks until the asynchronous method invocation with
 * @pending_call_id completes. The #GAsyncReadyCallback callback given to
 * egg_dbus_connection_send_message_with_reply() will be invoked
 * before this function returns.
 *
 * Depending on how the call was issued (e.g. what set of #EggDBusCallFlags was
 * passed), this function may use a recursive #GMainLoop for blocking.
 **/
void
egg_dbus_connection_pending_call_block (EggDBusConnection *connection,
                                        guint              pending_call_id)
{
  EggDBusConnectionPrivate *priv;
  GSimpleAsyncResult *simple;
  EggDBusCallFlags call_flags;
  DBusPendingCall *pending_call;
  GMainLoop *loop;

  g_return_if_fail (EGG_DBUS_IS_CONNECTION (connection));

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  simple = g_hash_table_lookup (priv->hash_pending_call_id_to_async_result, GUINT_TO_POINTER (pending_call_id));
  if (simple == NULL)
    {
      g_warning ("No pending call with id %u", pending_call_id);
      return;
    }

  call_flags = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (simple), "egg-dbus-call-flags"));
  pending_call = (DBusPendingCall *) g_object_get_data (G_OBJECT (simple), "dbus-1-pending-call");

  g_assert (pending_call != NULL);

  /* now block depending on the mode */
  if (call_flags & EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP)
    {
      /* creates a new main loop;
       * egg_dbus_connection_send_message_with_reply_cb() will
       * quit it *after* it has completed the operation.
       */
      loop = g_main_loop_new (NULL, FALSE);
      g_object_set_data_full (G_OBJECT (simple),
                              "egg-dbus-main-loop",
                              loop,
                              (GDestroyNotify) g_main_loop_unref);
      g_main_loop_run (loop);
    }
  else
    {
      dbus_pending_call_block (pending_call);
    }
}

/**
 * egg_dbus_connection_send_message_with_reply:
 * @connection: A #EggDBusConnection.
 * @call_flags: Flags from #EggDBusCallFlags detailing how the message should be sent.
 * @message: The message to send.
 * @error_types: Either %NULL or a #G_TYPE_INVALID terminated list of #GType<!-- -->s for #GError error domains to use
 * when processing an error reply.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: Callback when the asynchronous operation finishes.
 * @user_data: User data to pass to @callback.
 *
 * Queues @message to be sent and invokes @callback (on the main thread) when there is a reply. This method is
 * only useful for language bindings.
 *
 * In @callback, egg_dbus_connection_send_message_with_reply_finish() should be used to
 * extract the reply or error.
 *
 * Returns: A pending call id (never zero) that can be used with egg_dbus_connection_pending_call_cancel() or
 * egg_dbus_connection_pending_call_block().
 **/
guint
egg_dbus_connection_send_message_with_reply (EggDBusConnection       *connection,
                                             EggDBusCallFlags         call_flags,
                                             EggDBusMessage          *message,
                                             GType                   *error_types,
                                             GCancellable            *cancellable,
                                             GAsyncReadyCallback      callback,
                                             gpointer                 user_data)
{
  guint pending_call_id;
  DBusPendingCall *pending_call;
  GSimpleAsyncResult *simple;
  DBusMessage *dmessage;
  EggDBusConnectionPrivate *priv;
  gint timeout;
  GType *error_types_copy;

  g_return_val_if_fail (EGG_DBUS_IS_CONNECTION (connection), 0);
  g_return_val_if_fail (message != NULL, 0);

  priv = EGG_DBUS_CONNECTION_GET_PRIVATE (connection);

  simple = g_simple_async_result_new (G_OBJECT (connection),
                                      callback,
                                      user_data,
                                      egg_dbus_connection_send_message_with_reply);

  dmessage = g_object_get_data (G_OBJECT (message), "dbus-1-message");

  g_object_set_data_full (G_OBJECT (simple),
                          "egg-dbus-message",
                          g_object_ref (message),
                          (GDestroyNotify) g_object_unref);

  /* Sort out timeout.
   *
   * TODO: maybe we want to provide more finetuned control of the timeout; we
   *       could either have more flags or just perhaps just use the top 8-16
   *       bits for specifying the timeout in seconds.
   */
  timeout = -1;
  if (call_flags & EGG_DBUS_CALL_FLAGS_TIMEOUT_NONE)
    timeout = G_MAXINT;

  dbus_connection_send_with_reply (priv->connection,
                                   dmessage,
                                   &pending_call,
                                   timeout);

  dbus_pending_call_set_notify (pending_call,
                                (DBusPendingCallNotifyFunction) egg_dbus_connection_send_message_with_reply_cb,
                                (void *) simple,
                                (DBusFreeFunction) g_object_unref);

  /* never return zero as pending_call_id */
  if (priv->pending_call_next == 0)
    priv->pending_call_next++;
  pending_call_id = priv->pending_call_next++;

  if (error_types == NULL)
    {
      error_types_copy = NULL;
    }
  else
    {
      guint n;
      for (n = 0; error_types[n] != G_TYPE_INVALID; n++)
        ;
      error_types_copy = g_memdup (error_types, n * sizeof (GType));
    }

  g_object_set_data (G_OBJECT (simple), "dbus-1-pending-call", pending_call);
  g_object_set_data (G_OBJECT (simple), "egg-dbus-pending-call-id", GUINT_TO_POINTER (pending_call_id));
  g_object_set_data (G_OBJECT (simple), "egg-dbus-call-flags", GUINT_TO_POINTER (call_flags));
  if (cancellable != NULL)
    g_object_set_data_full (G_OBJECT (simple), "egg-dbus-cancellable", g_object_ref (cancellable), g_object_unref);
  g_object_set_data_full (G_OBJECT (simple), "egg-dbus-error-types", error_types_copy, (GDestroyNotify) g_free);

  g_hash_table_insert (priv->hash_pending_call_id_to_async_result,
                       GUINT_TO_POINTER (pending_call_id),
                       simple);

  if (cancellable != NULL)
    {
      g_signal_connect_data (cancellable,
                             "cancelled",
                             G_CALLBACK (egg_dbus_connection_send_message_with_reply_cancelled_cb),
                             g_object_ref (simple),
                             (GClosureNotify) g_object_unref,
                             0);
      /* TODO: hmm, some other thread could have cancelled us before we connect to the signal... Fix this. */
    }

  return pending_call_id;
}

/**
 * egg_dbus_connection_send_message_with_reply_finish:
 * @connection: A #EggDBusConnection.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed
 * to egg_dbus_connection_send_message_with_reply().
 * @error: Return location for error.
 *
 * Finishes sending a message where a reply is requested.
 *
 * Returns: A #EggDBusMessage or %NULL if @error is set.
 **/
EggDBusMessage *
egg_dbus_connection_send_message_with_reply_finish (EggDBusConnection     *connection,
                                                    GAsyncResult          *res,
                                                    GError               **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  EggDBusMessage *reply;

  g_return_val_if_fail (EGG_DBUS_IS_CONNECTION (connection), NULL);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == egg_dbus_connection_send_message_with_reply);

  reply = NULL;

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  reply = g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));

out:
  return reply;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_connection_new_message_for_signal:
 * @connection: A #EggDBusConnection.
 * @sender: Sender of the signal or %NULL to use unique name for @connection.
 * @destination: Destination of signal or %NULL to multi-cast to all recipients on the bus.
 * @object_path: The object path to broadcast the signal on.
 * @interface_name: The interface name to which @signal_name belongs.
 * @signal_name: Name of the signal to broadcast.
 *
 * Creates a new #EggDBusMessage for sending a signal. This method is
 * only useful for language bindings.
 *
 * Returns: A new #EggDBusMessage. Free with g_object_unref().
 **/
EggDBusMessage *
egg_dbus_connection_new_message_for_signal (EggDBusConnection       *connection,
                                            const gchar             *sender,
                                            const gchar             *destination,
                                            const gchar             *object_path,
                                            const gchar             *interface_name,
                                            const gchar             *signal_name)
{
  return EGG_DBUS_MESSAGE (g_object_new (EGG_DBUS_TYPE_MESSAGE,
                                       "connection",     connection,
                                       "sender",         sender,
                                       "destination",    destination,
                                       "message-type",   EGG_DBUS_MESSAGE_TYPE_SIGNAL,
                                       "object-path",    object_path,
                                       "interface-name", interface_name,
                                       "method-name",    NULL,
                                       "signal-name",    signal_name,
                                       "in-reply-to",    NULL,
                                       "error-name",     NULL,
                                       "error-message",  NULL,
                                       NULL));
}

/**
 * egg_dbus_connection_new_message_for_method_call:
 * @connection: A #EggDBusConnection.
 * @sender: Sender of the message or %NULL to use unique name for @connection.
 * @destination: Destination of name to invoke method on.
 * @object_path: The object to invoke the method on.
 * @interface_name: The interface name to which @method_name belongs.
 * @method_name: The name of the method to invoke.
 *
 * Creates a new #EggDBusMessage for invoking a method. This method is
 * only useful for language bindings.
 *
 * Returns: A new #EggDBusMessage. Free with g_object_unref().
 **/
EggDBusMessage *
egg_dbus_connection_new_message_for_method_call (EggDBusConnection       *connection,
                                                 const gchar             *sender,
                                                 const gchar             *destination,
                                                 const gchar             *object_path,
                                                 const gchar             *interface_name,
                                                 const gchar             *method_name)
{
  return EGG_DBUS_MESSAGE (g_object_new (EGG_DBUS_TYPE_MESSAGE,
                                       "connection",     connection,
                                       "sender",         sender,
                                       "destination",    destination,
                                       "message-type",   EGG_DBUS_MESSAGE_TYPE_METHOD_CALL,
                                       "object-path",    object_path,
                                       "interface-name", interface_name,
                                       "method-name",    method_name,
                                       "signal-name",    NULL,
                                       "in-reply-to",    NULL,
                                       "error-name",     NULL,
                                       "error-message",  NULL,
                                       NULL));
}
