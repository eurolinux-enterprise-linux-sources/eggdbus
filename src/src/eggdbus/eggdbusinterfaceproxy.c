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
#include <eggdbus/eggdbusstructure.h>
#include <eggdbus/eggdbusintrospectable.h>
#include <eggdbus/eggdbushashmap.h>

/**
 * SECTION:eggdbusinterfaceproxy
 * @title: EggDBusInterfaceProxy
 * @short_description: Abstract base class for interface proxies
 * @see_also: #EggDBusObjectProxy
 *
 * #EggDBusInterfaceProxy is an abstract base class that all interface proxies for concrete
 * D-Bus interfaces are derived from. The base class provides a way to get the #EggDBusObjectProxy
 * and also maps D-Bus properties to GObject properties.
 *
 * You normally get an interface proxy by using the QUERY_INTERFACE() macros in generated
 * glue GInterface code; for example for the interface proxy for the <literal>org.freedesktop.DBus</literal>
 * D-Bus interface, you should use the EGG_DBUS_QUERY_INTERFACE_BUS() macro:
 *
 * <programlisting>
 * EggDBusBus *bus;
 *
 * bus = EGG_DBUS_QUERY_INTERFACE_BUS (object_proxy);
 * </programlisting>
 *
 * In this example you can use the @bus object to invoke methods on the
 * <literal>org.freedesktop.DBus</literal> interface on the remote object represented by
 * @object_proxy. In addition, D-Bus properties and signals are available as GObject properties
 * and signals on interface proxy objects.
 */

typedef struct
{
  GObject parent_instance;

  const EggDBusInterfaceInfo *interface_info;

  /* A property bag is a hash from D-Bus property names into instances
   * of EggDBusVariant; it is retrieved using GetAll() on the interface
   * org.freedesktop.Properties (e.g. egg_dbus_properties_get_all()).
   */
  EggDBusHashMap *property_bag;

} EggDBusInterfaceProxyPrivate;

#define EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_INTERFACE_PROXY, EggDBusInterfaceProxyPrivate))

G_DEFINE_ABSTRACT_TYPE (EggDBusInterfaceProxy, egg_dbus_interface_proxy, G_TYPE_OBJECT);

static void
egg_dbus_interface_proxy_init (EggDBusInterfaceProxy *interface_proxy)
{
  EggDBusInterfaceProxyPrivate *priv;

  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);
}

static void
egg_dbus_interface_proxy_finalize (GObject *object)
{
  EggDBusInterfaceProxy *interface_proxy;
  EggDBusInterfaceProxyPrivate *priv;

  interface_proxy = EGG_DBUS_INTERFACE_PROXY (object);
  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  if (priv->property_bag != NULL)
    g_object_unref (priv->property_bag);

  G_OBJECT_CLASS (egg_dbus_interface_proxy_parent_class)->finalize (object);
}

/* returns a rewritten property_bag (and frees the given property bag) */
static EggDBusHashMap *
rewrite_properties (EggDBusInterfaceProxy *interface_proxy,
                    EggDBusHashMap *property_bag)
{
  EggDBusInterfaceProxyPrivate *priv;
  EggDBusHashMap *rewritten_property_bag;
  GHashTableIter property_bag_iter;
  gchar *camel_cased_name;
  EggDBusVariant *variant;

  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  /* rewrite names in property bag so we store the g-name-of-property intead of GNameOfProperty */
  rewritten_property_bag = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                                                  EGG_DBUS_TYPE_VARIANT, g_object_unref);
  g_hash_table_iter_init (&property_bag_iter, property_bag->data);
  while (g_hash_table_iter_next (&property_bag_iter, (gpointer) &camel_cased_name, (gpointer) &variant))
    {
      const EggDBusInterfacePropertyInfo *property_info;

      /* TODO: this is right now O(n) in number of properties;
       * e.g. this whole rewrite is O(n^2). This _needs_ to be
       * optimized.
       */
      property_info = egg_dbus_interface_info_lookup_property_for_name (priv->interface_info, camel_cased_name);
      if (property_info != NULL)
        {
          egg_dbus_hash_map_insert (rewritten_property_bag,
                                    (gpointer) property_info->g_name,
                                    g_object_ref (variant));
        }
      else
        {
          /* otherwise warn */
          g_warning ("Couldn't find property info for property %s on D-Bus interface %s",
                     camel_cased_name,
                     priv->interface_info->name);
        }
    }

  g_object_unref (property_bag);

  return rewritten_property_bag;
}

/**
 * ensure_properties:
 * @interface_proxy: A #EggDBusInterfaceProxy.
 * @no_block: If %TRUE, don't block until we've received the properties
 *
 * Retrieves all properties for the D-Bus interface.
 *
 * Returns: A #EggDBusHashMap. Returns %NULL only if @no_block is
 * %TRUE and we don't have the properties yet.
 **/
static EggDBusHashMap *
ensure_properties (EggDBusInterfaceProxy *interface_proxy,
                   gboolean no_block)
{
  EggDBusInterfaceProxyPrivate *priv;
  EggDBusHashMap *property_bag;
  GError *error;

  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  /* first see if we've already caching the property bag */

  if (priv->property_bag != NULL)
    goto out;

  if (no_block)
    goto out;

  /* otherwise get it synchronously. (TODO: get async from constructed()) */

  /*g_debug ("Doing org.freedesktop.Properties.GetAll() for if %s", priv->interface_info->name);*/

  /* don't block in mainloop... */
  error = NULL;
  if (!egg_dbus_properties_get_all_sync (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (egg_dbus_interface_proxy_get_object_proxy (interface_proxy)),
                                         EGG_DBUS_CALL_FLAGS_NONE,
                                         priv->interface_info->name,
                                         &property_bag,
                                         NULL,
                                         &error))
    {
      g_warning ("Error getting properties on interface %s: %s",
                 priv->interface_info->name,
                 error->message);
      g_error_free (error);
      goto out;
    }

  priv->property_bag = rewrite_properties (interface_proxy, property_bag);

 out:

  return priv->property_bag;
}


static void
egg_dbus_interface_proxy_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  EggDBusInterfaceProxy *interface_proxy;
  EggDBusInterfaceProxyPrivate *priv;
  EggDBusHashMap *property_bag;
  EggDBusVariant *variant;

  interface_proxy = EGG_DBUS_INTERFACE_PROXY (object);
  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  variant = NULL;
  property_bag = ensure_properties (interface_proxy, FALSE);

  if (property_bag != NULL)
    variant = egg_dbus_hash_map_lookup (property_bag, pspec->name);

  if (variant != NULL)
    {
      /* Since we always construct EggDBusStructure instances up-cast
       * to the proper derived type. This is safe because of the structural
       * equivalence guarantee that all EggDBusStructure derived classes
       * must fulfill.
       */
      if (G_VALUE_HOLDS (value, EGG_DBUS_TYPE_STRUCTURE))
        {
          GValue modified_value;

          modified_value = *(egg_dbus_variant_get_gvalue (variant));
          modified_value.g_type = pspec->value_type;

          g_value_copy (&modified_value, value);
        }
      else
        {
          const GValue *variant_value;

          variant_value = egg_dbus_variant_get_gvalue (variant);

          if (G_VALUE_TYPE (variant_value) == G_TYPE_UINT)
            {
              if (G_TYPE_IS_FLAGS (G_VALUE_TYPE (value)))
                {
                  g_value_set_flags (value, g_value_get_uint (variant_value));
                }
              else if (G_TYPE_IS_ENUM (G_VALUE_TYPE (value)))
                {
                  g_value_set_enum (value, g_value_get_uint (variant_value));
                }
              else
                {
                  g_value_copy (variant_value, value);
                }
            }
          else
            {
              g_value_copy (variant_value, value);
            }
        }
    }
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
egg_dbus_interface_proxy_set_property (GObject       *object,
                                       guint          prop_id,
                                       const GValue  *value,
                                       GParamSpec    *pspec)
{
  EggDBusInterfaceProxy *interface_proxy;
  EggDBusInterfaceProxyPrivate *priv;
  const EggDBusInterfacePropertyInfo *property_info;
  EggDBusVariant *variant;
  GError *error;

  variant = NULL;
  error = NULL;

  interface_proxy = EGG_DBUS_INTERFACE_PROXY (object);
  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  property_info = egg_dbus_interface_info_lookup_property_for_g_name (priv->interface_info, pspec->name);
  if (property_info == NULL)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      goto out;
    }

  //g_debug ("set_property() for prop %s @ type %s, D-Bus property name %s, D-Bus interface %s",
  //         pspec->name,
  //         g_type_name (pspec->owner_type),
  //         property_info->g_name,
  //         priv->interface_info->name);

  variant = egg_dbus_variant_new_for_gvalue (value, property_info->signature);

  /* We do this sync to avoid coherency problems. If this is problematic / slow, we
   * can change it later to be async... However, doing it sync, shouldn't be a problem
   * at all, I haven't see any services in the wild with writable properties...
   *
   * TODO: Right now we block in the mainloop. This is *probably* wrong.
   */
  if (!egg_dbus_properties_set_sync (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (egg_dbus_interface_proxy_get_object_proxy (interface_proxy)),
                                     EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                     priv->interface_info->name,
                                     property_info->name,
                                     variant,
                                     NULL,
                                     &error))
    {
      g_warning ("Error setting property %s (%s) on %s via D-Bus: %s",
                 property_info->g_name,
                 property_info->name,
                 priv->interface_info->name,
                 error->message);
      g_error_free (error);
      goto out;
    }

  /* update local property bag if we've got one (takes ownership of variant)..
   *
   * (If the remote end supports change events, this technically isn't necessary)
   */
  priv->property_bag = ensure_properties (interface_proxy, TRUE);
  if (priv->property_bag != NULL)
    {
      egg_dbus_hash_map_insert (priv->property_bag, (gpointer) property_info->g_name, variant);
      variant = NULL;
    }

 out:
  if (variant != NULL)
    g_object_unref (variant);
}

void
_egg_dbus_interface_proxy_invalidate_properties (EggDBusInterfaceProxy *interface_proxy)
{
  EggDBusInterfaceProxyPrivate *priv;

  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  if (priv->property_bag != NULL)
    {
      g_object_unref (priv->property_bag);
      priv->property_bag = NULL;
    }

}

void
_egg_dbus_interface_proxy_handle_property_changed (EggDBusInterfaceProxy *interface_proxy,
                                                   EggDBusHashMap        *changed_properties)
{
  EggDBusInterfaceProxyPrivate *priv;
  GHashTableIter change_property_bag_iter;
  const gchar *prop_g_name;
  EggDBusVariant *variant;
  EggDBusHashMap *existing_property_bag;
  EggDBusHashMap *rewritten_properties;
  GObjectClass *gobject_class;

  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  rewritten_properties = rewrite_properties (interface_proxy,
                                             g_object_ref (changed_properties));

  existing_property_bag = ensure_properties (interface_proxy, TRUE);

  gobject_class = G_OBJECT_GET_CLASS (interface_proxy);

  g_hash_table_iter_init (&change_property_bag_iter, rewritten_properties->data);
  while (g_hash_table_iter_next (&change_property_bag_iter, (gpointer) &prop_g_name, (gpointer) &variant))
    {
      GParamSpec *pspec;

      pspec = g_object_class_find_property (gobject_class, prop_g_name);
      if (pspec == NULL)
        {
          g_warning ("No pspec for property %s", prop_g_name);
          continue;
        }

      /*g_debug ("handling EggDBusChanged() for property %s on D-Bus interface %s", prop_g_name, priv->interface_info->name);*/

      /* TODO: maybe it would be useful to check if the value
       * actually changed before emitting notify?
       *
       * For that we need an equal() method on a EggDBusVariant...
       */

      /* update the existing property bag if we have one */
      if (existing_property_bag != NULL)
        {
          //g_debug ("updating %s", prop_g_name);
          egg_dbus_hash_map_insert (existing_property_bag,
                                    (gpointer) prop_g_name,
                                    g_object_ref (variant));
        }

      g_object_notify (G_OBJECT (interface_proxy), prop_g_name);
      //g_debug ("emitted notify for %s", prop_g_name);
    }
}

static void
egg_dbus_interface_proxy_constructed (GObject *object)
{
  EggDBusInterfaceProxy *interface_proxy;
  EggDBusInterfaceProxyPrivate *priv;
  EggDBusInterfaceIface *g_iface;

  interface_proxy = EGG_DBUS_INTERFACE_PROXY (object);
  priv = EGG_DBUS_INTERFACE_PROXY_GET_PRIVATE (interface_proxy);

  g_iface = egg_dbus_interface_proxy_get_interface_iface (interface_proxy);
  priv->interface_info = g_iface->get_interface_info ();

  /* TODO: initiate property fetching async */

  if (G_OBJECT_CLASS (egg_dbus_interface_proxy_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (egg_dbus_interface_proxy_parent_class)->constructed (object);
}

static void
egg_dbus_interface_proxy_class_init (EggDBusInterfaceProxyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_interface_proxy_finalize;
  gobject_class->set_property = egg_dbus_interface_proxy_set_property;
  gobject_class->get_property = egg_dbus_interface_proxy_get_property;
  gobject_class->constructed  = egg_dbus_interface_proxy_constructed;

  g_type_class_add_private (klass, sizeof (EggDBusInterfaceProxyPrivate));
}

/**
 * egg_dbus_interface_proxy_get_object_proxy:
 * @interface_proxy: A #EggDBusInterfaceProxy.
 *
 * Gets the object proxy that @interface_proxy is associated with.
 *
 * Returns: A #EggDBusObjectProxy. Do not free, the returned object is owned by @interface_proxy.
 **/
EggDBusObjectProxy *
egg_dbus_interface_proxy_get_object_proxy (EggDBusInterfaceProxy *interface_proxy)
{
  EggDBusInterfaceProxyClass *klass;

  g_return_val_if_fail (EGG_DBUS_IS_INTERFACE_PROXY (interface_proxy), NULL);

  klass = EGG_DBUS_INTERFACE_PROXY_GET_CLASS (interface_proxy);

  return klass->get_object_proxy (interface_proxy);
}

/**
 * egg_dbus_interface_proxy_get_interface_iface:
 * @interface_proxy: A #EggDBusInterfaceProxy.
 *
 * Gets the D-Bus interface VTable for @interface_proxy.
 *
 * Returns: A #EggDBusInterfaceIface.
 **/
EggDBusInterfaceIface *
egg_dbus_interface_proxy_get_interface_iface (EggDBusInterfaceProxy *interface_proxy)
{
  EggDBusInterfaceProxyClass *klass;

  g_return_val_if_fail (EGG_DBUS_IS_INTERFACE_PROXY (interface_proxy), NULL);

  klass = EGG_DBUS_INTERFACE_PROXY_GET_CLASS (interface_proxy);
  return klass->get_interface_iface (interface_proxy);
}
