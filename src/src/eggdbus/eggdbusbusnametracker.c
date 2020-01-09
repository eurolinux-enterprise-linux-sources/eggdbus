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
#include <eggdbus/eggdbusbusnametracker.h>
#include <eggdbus/eggdbusconnection.h>
#include <eggdbus/eggdbusbus.h>
#include <eggdbus/eggdbustypes.h>
#include "eggdbusmarshal.h"

/**
 * SECTION:eggdbusbusnametracker
 * @title: EggDBusBusNameTracker
 * @short_description: Track bus names
 *
 * The #EggDBusBusNameTracker class is used for tracking bus names on a
 * message bus.
 *
 * If you are writing a D-Bus client there's no need to use this class as it's used
 * internally by #EggDBusConnection to maintain the #EggDBusObjectProxy:name-owner property
 * on object proxies.
 */

typedef struct
{
  /* ref count of this structure */
  volatile gint ref_count;

  /* number of API users watching the name (not the same as ref_count) */
  gint num_watchers;

  /* a reference to the bus proxy */
  EggDBusBus *bus;

  /* The name we are watching */
  gchar *name;

  /* The match rule passed to AddMatch() */
  gchar *match_rule;

  /* The owner for the name */
  gchar *owner;

  /* TRUE only if AddMatch has run */
  gboolean has_added_match_rule;

  /* if not NULL, we have an outstanding GetNameOwner() request */
  gboolean get_name_owner_is_pending;

  /* a main loop; set by egg_dbus_bus_name_tracker_get_owner_for_bus_name() if blocking for
   * GetNameOwner() to complete.
   */
  GMainLoop *loop;

} NameData;

static NameData *
name_data_new (EggDBusBus *bus,
               const gchar *name)
{
  NameData *data;

  data = g_new0 (NameData, 1);
  data->bus = g_object_ref (bus);
  data->ref_count = 1;
  data->num_watchers = 1;
  data->name = g_strdup (name);
  return data;
}

static NameData *
name_data_ref (NameData *data)
{
  g_atomic_int_inc (&data->ref_count);
  return data;
}

static void
remove_match_rule_cb (EggDBusBus     *bus,
                      GAsyncResult *res,
                      gchar        *match_rule)
{
  GError *error;

  error = NULL;
  if (!egg_dbus_bus_remove_match_finish (bus,
                                         res,
                                         &error))
    {
      g_warning ("Error removing match rule \"%s\": %s", match_rule, error->message);
      g_error_free (error);
    }
  else
    {
      /*g_debug ("Success removing match rule \"%s\"", match_rule);*/
    }

  g_free (match_rule);
}

static void
name_data_unref (NameData *data)
{
  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      egg_dbus_bus_remove_match (data->bus,
                                 0,
                                 data->match_rule,
                                 NULL,
                                 (GAsyncReadyCallback) remove_match_rule_cb,
                                 data->match_rule); /* steal, remove_match_rule_cb() will free it */
      g_object_unref (data->bus);
      g_free (data->name);
      g_free (data->owner);
      g_free (data);
    }
}


typedef struct
{
  EggDBusBus *bus;
  guint name_owner_changed_handler_id;

  /* maps from names being watched to NameData */
  GHashTable *hash_from_name_to_data;

} EggDBusBusNameTrackerPrivate;

enum
{
  PROP_0,
  PROP_BUS,
};

enum
{
  BUS_NAME_HAS_INFO,
  BUS_NAME_LOST_OWNER_SIGNAL,
  BUS_NAME_GAINED_OWNER_SIGNAL,
  LAST_SIGNAL,
};

static void name_owner_changed (EggDBusBus            *bus_proxy,
                                const gchar         *name,
                                const gchar         *old_owner,
                                const gchar         *new_owner,
                                EggDBusBusNameTracker *bus_name_tracker);

guint signals[LAST_SIGNAL] = {0};

#define EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_BUS_NAME_TRACKER, EggDBusBusNameTrackerPrivate))

G_DEFINE_TYPE (EggDBusBusNameTracker, egg_dbus_bus_name_tracker, G_TYPE_OBJECT);

static void
egg_dbus_bus_name_tracker_init (EggDBusBusNameTracker *bus_name_tracker)
{
  EggDBusBusNameTrackerPrivate *priv;

  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  priv->hash_from_name_to_data = g_hash_table_new_full (g_str_hash,
                                                        g_str_equal,
                                                        NULL,
                                                        (GDestroyNotify) name_data_unref);
}

static void
egg_dbus_bus_name_tracker_finalize (GObject *object)
{
  EggDBusBusNameTracker *bus_name_tracker;
  EggDBusBusNameTrackerPrivate *priv;

  bus_name_tracker = EGG_DBUS_BUS_NAME_TRACKER (object);
  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  g_signal_handler_disconnect (priv->bus,
                               priv->name_owner_changed_handler_id);

  g_hash_table_unref (priv->hash_from_name_to_data);

  if (priv->bus != NULL)
    g_object_unref (priv->bus);

  G_OBJECT_CLASS (egg_dbus_bus_name_tracker_parent_class)->finalize (object);
}

static void
egg_dbus_bus_name_tracker_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  EggDBusBusNameTracker *bus_name_tracker;
  EggDBusBusNameTrackerPrivate *priv;

  bus_name_tracker = EGG_DBUS_BUS_NAME_TRACKER (object);
  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  switch (prop_id)
    {
    case PROP_BUS:
      priv->bus = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_bus_name_tracker_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  EggDBusBusNameTracker *bus_name_tracker;
  EggDBusBusNameTrackerPrivate *priv;

  bus_name_tracker = EGG_DBUS_BUS_NAME_TRACKER (object);
  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  switch (prop_id)
    {
    case PROP_BUS:
      g_value_set_object (value, priv->bus);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_bus_name_tracker_constructed (GObject    *object)
{
  EggDBusBusNameTracker *bus_name_tracker;
  EggDBusBusNameTrackerPrivate *priv;

  bus_name_tracker = EGG_DBUS_BUS_NAME_TRACKER (object);
  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  priv->name_owner_changed_handler_id = g_signal_connect (priv->bus,
                                                          "name-owner-changed",
                                                          (GCallback) name_owner_changed,
                                                          bus_name_tracker);

  if (G_OBJECT_CLASS (egg_dbus_bus_name_tracker_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (egg_dbus_bus_name_tracker_parent_class)->constructed (object);
}

static void
egg_dbus_bus_name_tracker_class_init (EggDBusBusNameTrackerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_bus_name_tracker_finalize;
  gobject_class->set_property = egg_dbus_bus_name_tracker_set_property;
  gobject_class->get_property = egg_dbus_bus_name_tracker_get_property;
  gobject_class->constructed  = egg_dbus_bus_name_tracker_constructed;

  g_type_class_add_private (klass, sizeof (EggDBusBusNameTracker));

  g_object_class_install_property (gobject_class,
                                   PROP_BUS,
                                   g_param_spec_object ("bus",
                                                        "Bus",
                                                        "The bus we're tracking names for",
                                                        EGG_DBUS_TYPE_BUS,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EggDBusBusNameTracker::bus-name-has-info:
   * @bus_name_tracker: A #EggDBusBusNameTracker.
   * @bus_name: A bus name being watched.
   *
   * Emitted when @bus_name_tracker has information about @bus_name.
   */
  signals[BUS_NAME_HAS_INFO] =
    g_signal_new ("bus-name-has-info",
                  EGG_DBUS_TYPE_BUS_NAME_TRACKER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EggDBusBusNameTrackerClass, bus_name_has_info),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  /**
   * EggDBusBusNameTracker::bus-name-lost-owner:
   * @bus_name_tracker: A #EggDBusBusNameTracker.
   * @bus_name: A bus name being watched.
   * @old_owner: A unique bus name.
   *
   * Emitted when @old_owner loses ownership of @bus_name.
   */
  signals[BUS_NAME_LOST_OWNER_SIGNAL] =
    g_signal_new ("bus-name-lost-owner",
                  EGG_DBUS_TYPE_BUS_NAME_TRACKER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EggDBusBusNameTrackerClass, bus_name_lost_owner),
                  NULL,
                  NULL,
                  eggdbus_marshal_VOID__STRING_STRING,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  /**
   * EggDBusBusNameTracker::bus-name-gained-owner:
   * @bus_name_tracker: A #EggDBusBusNameTracker.
   * @bus_name: A bus name being watched.
   * @new_owner: A unique bus name.
   *
   * Emitted when @new_owner gains ownership of @bus_name.
   */
  signals[BUS_NAME_GAINED_OWNER_SIGNAL] =
    g_signal_new ("bus-name-gained-owner",
                  EGG_DBUS_TYPE_BUS_NAME_TRACKER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EggDBusBusNameTrackerClass, bus_name_gained_owner),
                  NULL,
                  NULL,
                  eggdbus_marshal_VOID__STRING_STRING,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

}

/**
 * egg_dbus_bus_name_tracker_new:
 * @bus: A #EggDBusBus object that represents a message bus daemon.
 *
 * Creates a new bus name tracker for @bus.
 *
 * Returns: A #EggDBusBusNameTracker.
 **/
EggDBusBusNameTracker *
egg_dbus_bus_name_tracker_new (EggDBusBus *bus)
{
  EggDBusBusNameTracker *bus_name_tracker;

  bus_name_tracker = EGG_DBUS_BUS_NAME_TRACKER (g_object_new (EGG_DBUS_TYPE_BUS_NAME_TRACKER,
                                                            "bus", bus,
                                                            NULL));

  return bus_name_tracker;
}

static void
name_owner_changed (EggDBusBus            *bus,
                    const gchar         *bus_name,
                    const gchar         *old_owner,
                    const gchar         *new_owner,
                    EggDBusBusNameTracker *bus_name_tracker)
{
  EggDBusBusNameTrackerPrivate *priv;
  NameData *data;

  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  //g_debug ("in name_owner_changed: bus_name=%s old_owner=%s new_owner=%s", bus_name, old_owner, new_owner);

  data = g_hash_table_lookup (priv->hash_from_name_to_data, bus_name);
  if (data == NULL)
    {
      /* no need to warn, someone else may have added match rules on the bus */
      /*g_debug ("no data for bus_name=%s", bus_name);*/
      return;
    }

  g_free (data->owner);
  if (strlen (new_owner) > 0)
    data->owner = g_strdup (new_owner);
  else
    data->owner = NULL;

  if (strlen (old_owner) > 0)
    {
      /*g_debug ("emitting lost-owner for bus_name=%s, old_owner=%s", bus_name, old_owner);*/
      g_signal_emit (bus_name_tracker,
                     signals[BUS_NAME_LOST_OWNER_SIGNAL],
                     0,
                     bus_name,
                     old_owner);
    }

  if (strlen (new_owner) > 0)
    {
      /*g_debug ("emitting gained-owner for bus_name=%s, old_owner=%s", bus_name, old_owner);*/
      g_signal_emit (bus_name_tracker,
                     signals[BUS_NAME_GAINED_OWNER_SIGNAL],
                     0,
                     bus_name,
                     new_owner);
    }

}

static void
get_name_owner_cb (EggDBusBus      *bus,
                   GAsyncResult  *res,
                   NameData      *data)
{
  GError *error;
  char *owner;

  /*g_debug ("get_name_owner_cb for %s", data->name);*/

  error = NULL;
  if (!egg_dbus_bus_get_name_owner_finish (bus,
                                           &owner,
                                           res,
                                           &error))
    {
      /* this is actually not fatal or weird; it's perfectly fine for a well-known name not to have an owner */

      /* TODO: check that the error is actually org.freedesktop.DBus.Error.NameHasNoOwner */

      //g_warning ("Error getting owner for name %s: %s", data->name, error->message);
    }
  else
    {
      /* alrighty, we've got a unique name for the well known name */

      /*g_debug ("  got name %s", owner);*/

      g_warn_if_fail (data->owner == NULL);

      data->owner = owner; /* steals string */
    }

  data->get_name_owner_is_pending = FALSE;

  /* see egg_dbus_bus_name_tracker_get_owner_for_bus_name() */
  if (data->loop != NULL)
    {
      g_main_loop_quit (data->loop);
      g_main_loop_unref (data->loop);
      data->loop = NULL;
    }

  name_data_unref (data);
}

static void
add_match_rule_cb (EggDBusBus     *bus,
                   GAsyncResult *res,
                   NameData     *data)
{
  GError *error;

  error = NULL;
  if (!egg_dbus_bus_add_match_finish (bus,
                                      res,
                                      &error))
    {
      /* TODO: handle cancelled */
      g_warning ("Error adding match rule \"%s\": %s", data->match_rule, error->message);
      g_error_free (error);
    }

  /*g_debug ("Success adding match rule \"%s\"", data->match_rule);*/

  name_data_unref (data);
}

/**
 * egg_dbus_bus_name_tracker_watch_bus_name:
 * @bus_name_tracker: A #EggDBusBusNameTracker.
 * @bus_name: A unique or well-known name.
 *
 * Starts watching @bus_name.
 *
 * This function can be called multiple times for the same name;
 * @bus_name_tracker internally maintains a count of watchers for each
 * watched name.
 *
 * TODO: it would be useful to pass callback functions as well.
 **/
void
egg_dbus_bus_name_tracker_watch_bus_name (EggDBusBusNameTracker *bus_name_tracker,
                                        const gchar         *bus_name)
{
  EggDBusBusNameTrackerPrivate *priv;
  NameData *data;

  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  data = g_hash_table_lookup (priv->hash_from_name_to_data, bus_name);
  if (data != NULL)
    {
      data->num_watchers++;
      return;
    }

  data = name_data_new (priv->bus, bus_name);

  g_hash_table_insert (priv->hash_from_name_to_data, data->name, data);

  data->match_rule = g_strdup_printf ("type='signal',"
                                      "sender='org.freedesktop.DBus',"
                                      "member='NameOwnerChanged',arg0='%s'",
                                      bus_name);
  /* Watch for changes */
  egg_dbus_bus_add_match (priv->bus,
                          0,
                          data->match_rule,
                          NULL,
                          (GAsyncReadyCallback) add_match_rule_cb,
                          name_data_ref (data));

  data->get_name_owner_is_pending = TRUE;

  /* Get the name */
  egg_dbus_bus_get_name_owner (priv->bus,
                               0,
                               data->name,
                               NULL,
                               (GAsyncReadyCallback) get_name_owner_cb,
                               name_data_ref (data));

  /*g_debug ("started watching bus name %s", bus_name);*/
}

/**
 * egg_dbus_bus_name_tracker_stop_watching_bus_name:
 * @bus_name_tracker: A #EggDBusBusNameTracker.
 * @bus_name: A name being watched.
 *
 * Stops watching @bus_name.
 *
 * This function can be called multiple times for the same name;
 * @bus_name_tracker internally maintains a count of watchers for each
 * watched name.
 **/
void
egg_dbus_bus_name_tracker_stop_watching_bus_name (EggDBusBusNameTracker *bus_name_tracker,
                                                const gchar         *bus_name)
{
  EggDBusBusNameTrackerPrivate *priv;
  NameData *data;

  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  data = g_hash_table_lookup (priv->hash_from_name_to_data, bus_name);
  if (data == NULL)
    {
      g_warning ("bus name %s is not being watched", bus_name);
      return;
    }

  data->num_watchers--;

  if (data->num_watchers == 0)
    {
      /*g_debug ("stopped watching bus name %s", bus_name);*/
      /* this will trigger RemoveMatchRule in name_data_unref() */
      g_hash_table_remove (priv->hash_from_name_to_data, bus_name);
    }
}

/**
 * egg_dbus_bus_name_tracker_has_info_for_bus_name:
 * @bus_name_tracker: A #EggDBusBusNameTracker.
 * @bus_name: A name being watched.
 *
 * Checks if information has been retrieved for @bus_name. If this function returns %FALSE, you can
 * connect to the #EggDBusBusNameTracker::bus-name-has-info signal and use
 * egg_dbus_bus_name_tracker_get_owner_for_bus_name() to retrieve the owner when information is
 * available.
 *
 * Returns: %TRUE if @bus_name_tracker has information about @bus_name.
 **/
gboolean
egg_dbus_bus_name_tracker_has_info_for_bus_name (EggDBusBusNameTracker  *bus_name_tracker,
                                               const gchar          *bus_name)
{
  EggDBusBusNameTrackerPrivate *priv;
  NameData *data;

  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  data = g_hash_table_lookup (priv->hash_from_name_to_data, bus_name);
  if (data == NULL)
    {
      g_warning ("bus name %s is not being watched", bus_name);
      return FALSE;
    }

  return !data->get_name_owner_is_pending;
}

/**
 * egg_dbus_bus_name_tracker_get_owner_for_bus_name:
 * @bus_name_tracker: A #EggDBusBusNameTracker.
 * @bus_name: A name being watched.
 *
 * Gets owner of @bus_name.
 *
 * If @bus_name_tracker recently started watching @bus_name, a call to egg_dbus_bus_get_name_owner()
 * call may be still be pending. This function will block in a main loop until the owner is resolved.
 *
 * If this synchronous behavior is not desired, use call egg_dbus_bus_name_tracker_has_info_for_bus_name()
 * and connect to the #EggDBusBusNameTracker::bus-name-has-info signal to get informed when
 * owner information is available.
 *
 * Returns: The owner of @bus_name or %NULL if there is no owner. Free
 * with g_free().
 **/
gchar *
egg_dbus_bus_name_tracker_get_owner_for_bus_name (EggDBusBusNameTracker  *bus_name_tracker,
                                                const gchar          *bus_name)
{
  EggDBusBusNameTrackerPrivate *priv;
  NameData *data;

  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  data = g_hash_table_lookup (priv->hash_from_name_to_data, bus_name);
  if (data == NULL)
    {
      g_warning ("bus name %s is not being watched", bus_name);
      return NULL;
    }

  if (data->get_name_owner_is_pending)
    {
      /* block until GetNameOwner() returns; get_name_owner_cb() will quit + free this loop */
      data->loop = g_main_loop_new (NULL, FALSE);

      g_object_ref (bus_name_tracker);

      g_main_loop_run (data->loop);

      g_object_unref (bus_name_tracker);
    }

  return g_strdup (data->owner);
}

/**
 * egg_dbus_bus_name_tracker_get_known_well_known_bus_names_for_unique_bus_name:
 * @bus_name_tracker: A #EggDBusBusNameTracker.
 * @unique_bus_name: A unique or well-known bus name earlier passed to
 * egg_dbus_bus_name_tracker_watch_bus_name().
 *
 * Returns the known set (e.g. only the set of well-known bus names
 * currently being watched) of well-known bus names that
 * @unique_bus_name owns.
 *
 * This function is used to dispatch signals to proxies in
 * #EggDBusConnection; it is probably not of much general use.
 *
 * Returns: An %NULL terminated array of well-known bus names or %NULL
 * if there are no known well-known bus names owned by
 * @unique_bus_name. Free with g_strfreev().
 **/
gchar **
egg_dbus_bus_name_tracker_get_known_well_known_bus_names_for_unique_bus_name (EggDBusBusNameTracker  *bus_name_tracker,
                                                                            const gchar          *unique_bus_name)
{
  EggDBusBusNameTrackerPrivate *priv;
  GHashTableIter iter;
  NameData *data;
  GPtrArray *p;

  /* TODO: if this turns out to be slow, we can have some kind of look-aside hash table etc. */

  priv = EGG_DBUS_BUS_NAME_TRACKER_GET_PRIVATE (bus_name_tracker);

  p = NULL;

  g_hash_table_iter_init (&iter, priv->hash_from_name_to_data);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &data))
    {
      /* we're only interested in well-known names */
      if (data->name[0] == ':')
        continue;

      /* a well-known name may not have an owner */
      if (data->owner == NULL)
        continue;

      if (strcmp (data->owner, unique_bus_name) == 0)
        {
          if (p == NULL)
            p = g_ptr_array_new ();

          g_ptr_array_add (p, g_strdup (data->name));
        }
    }

  if (p != NULL)
    {
      g_ptr_array_add (p, NULL);
      return (gchar **) g_ptr_array_free (p, FALSE);
    }
  else
    {
      return NULL;
    }
}
