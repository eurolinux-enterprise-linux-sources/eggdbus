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
#include <eggdbus/eggdbushashmap.h>
#include <eggdbus/eggdbusmisctypes.h>
#include <eggdbus/eggdbusobjectpath.h>
#include <eggdbus/eggdbussignature.h>
#include <eggdbus/eggdbusprivate.h>

/**
 * SECTION:eggdbushashmap
 * @title: EggDBusHashMap
 * @short_description: Hash Maps
 *
 * A map type for that maps keys from one #GType to values of another #GType using hashing.
 * See egg_dbus_hash_map_new() for details.
 *
 * By default, the map takes ownership when inserting key/value pairs meaning that the programmer gives
 * up his references. Values looked up from the map are owned by the array. There is also convenience
 * API to get a copy of the value, see egg_dbus_hash_map_lookup_copy().
 *
 * Note that this class exposes a number of implementation details directly in the class
 * instance structure for efficient and convenient access when used from the C programming
 * language. Use with caution. For the same reasons, this class also provides a number of
 * convenience functions for dealing with fixed-size integral and floating point numbers.
 */

typedef struct
{
  gpointer       (*key_copy_func) (EggDBusHashMap *hash_map, gconstpointer element);
  GDestroyNotify   key_free_func;
  GEqualFunc       key_equal_func;
  GHashFunc        key_hash_func;
  GBoxedCopyFunc   key_user_copy_func;
  gboolean         key_is_gobject_derived;
  gboolean         key_fits_in_pointer;

  gpointer       (*value_copy_func) (EggDBusHashMap *hash_map, gconstpointer element);
  GDestroyNotify   value_free_func;
  GEqualFunc       value_equal_func;
  GBoxedCopyFunc   value_user_copy_func;
  gboolean         value_is_gobject_derived;
  gboolean         value_fits_in_pointer;

} EggDBusHashMapPrivate;

#define EGG_DBUS_HASH_MAP_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_HASH_MAP, EggDBusHashMapPrivate))

G_DEFINE_TYPE (EggDBusHashMap, egg_dbus_hash_map, G_TYPE_OBJECT);

static void
egg_dbus_hash_map_init (EggDBusHashMap *hash_map)
{
  EggDBusHashMapPrivate *priv;

  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
}

static gboolean
remove_func (gpointer key,
             gpointer value,
             EggDBusHashMap *hash_map)
{
  EggDBusHashMapPrivate *priv;

  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);

  if (priv->key_free_func != NULL)
    priv->key_free_func (key);

  if (priv->value_free_func != NULL)
    priv->value_free_func (value);

  return TRUE;
}

static void
egg_dbus_hash_map_finalize (GObject *object)
{
  EggDBusHashMap *hash_map;
  EggDBusHashMapPrivate *priv;

  hash_map = EGG_DBUS_HASH_MAP (object);
  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);

  /* free values */
  g_hash_table_foreach_steal (hash_map->data,
                              (GHRFunc) remove_func,
                              hash_map);
  /* then kill the underlying hash table */
  g_hash_table_unref (hash_map->data);

  G_OBJECT_CLASS (egg_dbus_hash_map_parent_class)->finalize (object);
}

/* ---------------------------------------------------------------------------------------------------- */

static gpointer
key_copy_via_user_copy_func (EggDBusHashMap  *hash_map,
                             gconstpointer    element)
{
  EggDBusHashMapPrivate *priv;

  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);

  return element == NULL ? NULL : priv->key_user_copy_func ((gpointer) element);
}

static gpointer
value_copy_via_user_copy_func (EggDBusHashMap  *hash_map,
                               gconstpointer    element)
{
  EggDBusHashMapPrivate *priv;

  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);

  return element == NULL ? NULL : priv->value_user_copy_func ((gpointer) element);
}

static gpointer
copy_elem_object (EggDBusHashMap  *hash_map,
                  gconstpointer    element)
{
  return element == NULL ? NULL : g_object_ref ((gpointer) element);
}

static gpointer
copy_elem_string (EggDBusHashMap  *hash_map,
                  gconstpointer    element)
{
  return g_strdup (element);
}

static gpointer
key_copy_elem_boxed (EggDBusHashMap  *hash_map,
                     gconstpointer    element)
{
  return element == NULL ? NULL : g_boxed_copy (hash_map->key_type, element);
}

static gpointer
value_copy_elem_boxed (EggDBusHashMap  *hash_map,
                       gconstpointer    element)
{
  return element == NULL ? NULL : g_boxed_copy (hash_map->value_type, element);
}

static gpointer
copy_elem_param_spec (EggDBusHashMap  *hash_map,
                      gconstpointer    element)
{
  return element == NULL ? NULL : g_param_spec_ref ((gpointer) element);
}

/* ---------------------------------------------------------------------------------------------------- */

#define IMPL_EQUAL_FUNC(type)                          \
  static gboolean                                      \
  _##type##_equal (gconstpointer v1, gconstpointer v2) \
  {                                                    \
    const type *a = v1;                                \
    const type *b = v2;                                \
    return *a == *b;                                   \
  }

IMPL_EQUAL_FUNC (gint64);
IMPL_EQUAL_FUNC (gdouble);
IMPL_EQUAL_FUNC (gfloat);
IMPL_EQUAL_FUNC (glong);

#define IMPL_COPY_FUNC(type)                           \
  static gpointer                                      \
  _##type##_copy (EggDBusHashMap  *hash_map,           \
                  gconstpointer    element)            \
  {                                                    \
    return g_memdup (element, sizeof (type));          \
  }

IMPL_COPY_FUNC (gint64);
IMPL_COPY_FUNC (gdouble);
IMPL_COPY_FUNC (gfloat);
IMPL_COPY_FUNC (glong);


static guint
_gint64_hash (gconstpointer v)
{
  gint64 value = *((gint64 *) v);
  /* this assumes that a gint64 is at least as big as an gint */
  return g_int_hash (&value);
}

static guint
_glong_hash (gconstpointer v)
{
  glong value = *((glong *) v);
  /* this assumes that a long is at least as big as an gint */
  return g_int_hash (&value);
}

static guint
_gfloat_hash (gconstpointer v)
{
  gint value = *((gint32 *) v);
  /* TODO: this assumes sizeof (gfloat) == sizeof (gint32) */
  return g_int_hash (&value);
}

static guint
_gdouble_hash (gconstpointer v)
{
  gint value = *((gint64 *) v);
  /* TODO: this assumes sizeof (gdouble) == sizeof (gint64) */
  return g_int_hash (&value);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
egg_dbus_hash_map_class_init (EggDBusHashMapClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_hash_map_finalize;

  g_type_class_add_private (klass, sizeof (EggDBusHashMapPrivate));
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_hash_map_new:
 * @key_type: The type of the keys in the map.
 * @key_free_func: Function to be used to free keys or %NULL.
 * @value_type: The type of the values in the map.
 * @value_free_func: Function to be used to free values or %NULL.
 *
 * Creates a new hash map for mapping keys of type @key_type to values of type @value_type.
 *
 * See egg_dbus_hash_map_new_full() for a more complicated version of this function.
 *
 * Returns: A #EggDBusHashMap. Free with g_object_unref().
 **/
EggDBusHashMap *
egg_dbus_hash_map_new (GType            key_type,
                       GDestroyNotify   key_free_func,
                       GType            value_type,
                       GDestroyNotify   value_free_func)
{
  return egg_dbus_hash_map_new_full (key_type,
                                     NULL,
                                     NULL,
                                     key_free_func,
                                     NULL,
                                     value_type,
                                     value_free_func,
                                     NULL,
                                     NULL);
}

/**
 * egg_dbus_hash_map_new_full:
 * @key_type: The type of the keys in the map.
 * @key_hash_func: Hash function for keys or %NULL to use the default hash function if one such exists.
 * @key_equal_func: Function to compare keys or %NULL to use the default equality function if one such exists.
 * @key_free_func: Function to be used to free keys or %NULL.
 * @key_copy_func: Function to copy keys or %NULL to use the default copy function if one such exists.
 * @value_type: The type of the values in the map.
 * @value_free_func: Function to be used to free values or %NULL.
 * @value_copy_func: Function to copy values or %NULL to use the default copy function if one such exists.
 * @value_equal_func: Function to compare values or %NULL to use the default equality function if one such exists.
 *
 * Creates a new hash map for mapping keys of type @key_type to values of type @value_type.
 * The map will use @key_free_func and @value_free_func to free keys and values respectively.
 * If either of these functions are %NULL, it's the responsibility of the owner to make sure
 * any keys or values inserted into the map are freed.
 *
 * If either of @key_hash_func or @key_equal_func is %NULL a default hash or equality function
 * will be used if one such exists (for example one exists for #G_TYPE_STRING, #G_TYPE_INT and
 * so on). If a hash or equality function wasn't given and no default hash function exists, it
 * is a programming error and a warning will be reported using g_error() (causing program
 * termination).
 *
 * If either of @key_copy_func or @value_copy_func is %NULL default copy functions will be used
 * if available (for example there is no default copy function for #G_TYPE_POINTER or
 * derived types). Note that <emphasis>optional</emphasis> methods such as egg_dbus_hash_map_lookup_copy()
 * won't work if there is no value copy function.
 *
 * Normally the hash map stores pointers to both key and values and the methods taking and
 * returning keys or values will also take pointers to keys or values. However, for fixed-size integral
 * types that fits in a pointer (such as #G_TYPE_INT or #G_TYPE_BOOLEAN), the key or value is stored
 * in the pointer to avoid allocating memory. Typically this is not something you need to care about
 * as such implementation details are hidden behind convenience API like egg_dbus_hash_map_insert_fixed_ptr()
 * or egg_dbus_hash_map_lookup_fixed().
 *
 * Note that it is a programming error to pass a free function for fixed-size types (e.g. integral
 * or floating point types); a warning will be reported using g_error() (causing program termination).
 *
 * Returns: A #EggDBusHashMap. Free with g_object_unref().
 **/
EggDBusHashMap *
egg_dbus_hash_map_new_full (GType               key_type,
                            GHashFunc           key_hash_func,
                            GEqualFunc          key_equal_func,
                            GDestroyNotify      key_free_func,
                            GBoxedCopyFunc      key_copy_func,
                            GType               value_type,
                            GDestroyNotify      value_free_func,
                            GBoxedCopyFunc      value_copy_func,
                            GEqualFunc          value_equal_func)
{
  GType key_fundamental_type;
  GType value_fundamental_type;
  EggDBusHashMap *hash_map;
  EggDBusHashMapPrivate *priv;

  hash_map = EGG_DBUS_HASH_MAP (g_object_new (EGG_DBUS_TYPE_HASH_MAP, NULL));

  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);

  key_fundamental_type = G_TYPE_FUNDAMENTAL (key_type);
  value_fundamental_type = G_TYPE_FUNDAMENTAL (value_type);

  /* First, the keys */

  hash_map->key_type = key_type;
  priv->key_hash_func = key_hash_func;
  priv->key_equal_func = key_equal_func;

  switch (key_fundamental_type)
    {
    case G_TYPE_OBJECT:
    case G_TYPE_INTERFACE:
      priv->key_copy_func = copy_elem_object;
      priv->key_is_gobject_derived = TRUE;
      break;

    case G_TYPE_BOXED:
      if (g_type_is_a (key_type, EGG_DBUS_TYPE_OBJECT_PATH) ||
          g_type_is_a (key_type, EGG_DBUS_TYPE_SIGNATURE))
        {
          priv->key_copy_func = copy_elem_string;
          if (priv->key_hash_func == NULL)
            priv->key_hash_func = g_str_hash;
          if (priv->key_equal_func == NULL)
            priv->key_equal_func = g_str_equal;
        }
      else
        {
          priv->key_copy_func = key_copy_elem_boxed;
        }
      break;

    case G_TYPE_PARAM:
      priv->key_copy_func = copy_elem_param_spec;
      break;

    case G_TYPE_STRING:
      priv->key_copy_func = copy_elem_string;
      if (priv->key_hash_func == NULL)
        priv->key_hash_func = g_str_hash;
      if (priv->key_equal_func == NULL)
        priv->key_equal_func = g_str_equal;
      break;

    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
    case G_TYPE_INT:
    case G_TYPE_UINT:
    case G_TYPE_BOOLEAN:
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
      priv->key_fits_in_pointer = TRUE;
      if (priv->key_equal_func == NULL)
        priv->key_equal_func = g_direct_equal;
      if (priv->key_hash_func == NULL)
        priv->key_hash_func = g_direct_hash;
      break;

    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      priv->key_copy_func = _gint64_copy;
      priv->key_free_func = g_free;
      if (priv->key_equal_func == NULL)
        priv->key_equal_func = _gint64_equal;
      if (priv->key_hash_func == NULL)
        priv->key_hash_func = _gint64_hash;
      break;

    case G_TYPE_FLOAT:
      priv->key_copy_func = _gfloat_copy;
      priv->key_free_func = g_free;
      if (priv->key_equal_func == NULL)
        priv->key_equal_func = _gfloat_equal;
      if (priv->key_hash_func == NULL)
        priv->key_hash_func = _gfloat_hash;
      break;

    case G_TYPE_DOUBLE:
      priv->key_copy_func = _gdouble_copy;
      priv->key_free_func = g_free;
      if (priv->key_equal_func == NULL)
        priv->key_equal_func = _gdouble_equal;
      if (priv->key_hash_func == NULL)
        priv->key_hash_func = _gdouble_hash;
      break;

    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      priv->key_copy_func = _glong_copy;
      priv->key_free_func = g_free;
      if (priv->key_equal_func == NULL)
        priv->key_equal_func = _glong_equal;
      if (priv->key_hash_func == NULL)
        priv->key_hash_func = _glong_hash;
      break;

    default:
      if (key_fundamental_type == EGG_DBUS_TYPE_INT16 ||
          key_fundamental_type == EGG_DBUS_TYPE_UINT16)
        {
          priv->key_fits_in_pointer = TRUE;
          if (priv->key_equal_func == NULL)
            priv->key_equal_func = g_direct_equal;
          if (priv->key_hash_func == NULL)
            priv->key_hash_func = g_direct_hash;
        }
      break;
    }

  if ((priv->key_fits_in_pointer || priv->key_free_func != NULL) && key_free_func != NULL)
    {
      g_error ("Meaningless to specify key_free_func for EggDBusHashMap<%s,%s>.",
               g_type_name (hash_map->key_type),
               g_type_name (hash_map->value_type));
    }

  if (priv->key_fits_in_pointer && key_copy_func != NULL)
    {
      g_error ("Meaningless to specify key_copy_func for EggDBusHashMap<%s,%s>.",
               g_type_name (hash_map->key_type),
               g_type_name (hash_map->value_type));
    }

  if (priv->key_hash_func == NULL)
    {
      g_error ("No key_hash_func given for EggDBusHashMap<%s,%s> and unable to infer one.",
               g_type_name (hash_map->key_type),
               g_type_name (hash_map->value_type));
    }

  if (key_copy_func != NULL)
    {
      priv->key_user_copy_func = key_copy_func;
      priv->key_copy_func = key_copy_via_user_copy_func;
    }

  if (key_free_func != NULL)
    priv->key_free_func = key_free_func;

  if (priv->key_equal_func == NULL)
    {
      g_error ("No key_equal_func given for EggDBusHashMap<%s,%s> and unable to infer one.",
               g_type_name (hash_map->key_type),
               g_type_name (hash_map->value_type));
    }

  /* onto the values */

  hash_map->value_type = value_type;
  priv->value_equal_func = value_equal_func;

  switch (value_fundamental_type)
    {
    case G_TYPE_OBJECT:
    case G_TYPE_INTERFACE:
      priv->value_copy_func = copy_elem_object;
      priv->value_is_gobject_derived = TRUE;
      break;

    case G_TYPE_BOXED:
      if (g_type_is_a (value_type, EGG_DBUS_TYPE_OBJECT_PATH) ||
          g_type_is_a (value_type, EGG_DBUS_TYPE_SIGNATURE))
        {
          priv->value_copy_func = copy_elem_string;
          if (priv->value_equal_func == NULL)
            priv->value_equal_func = g_str_equal;
        }
      else
        {
          priv->value_copy_func = value_copy_elem_boxed;
        }
      break;

    case G_TYPE_PARAM:
      priv->value_copy_func = copy_elem_param_spec;
      break;

    case G_TYPE_STRING:
      priv->value_copy_func = copy_elem_string;
      if (priv->value_equal_func == NULL)
        priv->value_equal_func = g_str_equal;
      break;

    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
    case G_TYPE_INT:
    case G_TYPE_UINT:
    case G_TYPE_BOOLEAN:
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
    case G_TYPE_FLOAT: /* we assume a float fits into a int32 */
      priv->value_fits_in_pointer = TRUE;
      if (priv->value_equal_func == NULL)
        priv->value_equal_func = g_direct_equal;
      break;

    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      priv->value_copy_func = _gint64_copy;
      priv->value_free_func = g_free;
      if (priv->value_equal_func == NULL)
        priv->value_equal_func = _gint64_equal;
      break;

    case G_TYPE_DOUBLE:
      priv->value_copy_func = _gdouble_copy;
      priv->value_free_func = g_free;
      if (priv->value_equal_func == NULL)
        priv->value_equal_func = _gdouble_equal;
      break;

    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      priv->value_copy_func = _glong_copy;
      priv->value_free_func = g_free;
      if (priv->value_equal_func == NULL)
        priv->value_equal_func = _glong_equal;
      break;

    default:
      if (value_fundamental_type == EGG_DBUS_TYPE_INT16 ||
          value_fundamental_type == EGG_DBUS_TYPE_UINT16)
        {
          priv->value_fits_in_pointer = TRUE;
          if (priv->value_equal_func == NULL)
            priv->value_equal_func = g_direct_equal;
        }
      break;
    }

  if ((priv->value_fits_in_pointer || priv->value_free_func != NULL) && value_free_func != NULL)
    {
      g_error ("Meaningless to specify value_free_func for EggDBusHashMap<%s,%s>.",
               g_type_name (hash_map->value_type),
               g_type_name (hash_map->value_type));
    }

  if (priv->value_fits_in_pointer && value_copy_func != NULL)
    {
      g_error ("Meaningless to specify value_copy_func for EggDBusHashMap<%s,%s>.",
               g_type_name (hash_map->value_type),
               g_type_name (hash_map->value_type));
    }

  if (value_copy_func != NULL)
    {
      priv->value_user_copy_func = value_copy_func;
      priv->value_copy_func = value_copy_via_user_copy_func;
    }

  if (value_free_func != NULL)
    priv->value_free_func = value_free_func;

  /* now that we've got the functions and types sorted, set up the underlying hash table */

  hash_map->data = g_hash_table_new (priv->key_hash_func,
                                     priv->key_equal_func);

  return hash_map;
}

/**
 * egg_dbus_hash_map_get_size:
 * @hash_map: A #EggDBusHashMap.
 *
 * Gets the number of key/value pairs in @hash_map.
 *
 * Returns: The number of key/value pairs in @hash_map.
 **/
guint
egg_dbus_hash_map_get_size (EggDBusHashMap *hash_map)
{
  return g_hash_table_size (hash_map->data);
}

/**
 * egg_dbus_hash_map_get_key_type:
 * @hash_map: A #EggDBusHashMap.
 *
 * Gets the type of the keys in @hash_map.
 *
 * Returns: The #GType of keys in @hash_map.
 **/
GType
egg_dbus_hash_map_get_key_type (EggDBusHashMap *hash_map)
{
  return hash_map->key_type;
}

/**
 * egg_dbus_hash_map_get_value_type:
 * @hash_map: A #EggDBusHashMap.
 *
 * Gets the type of the values in @hash_map.
 *
 * Returns: The #GType of values in @hash_map.
 **/
GType
egg_dbus_hash_map_get_value_type (EggDBusHashMap *hash_map)
{
  return hash_map->value_type;
}

/**
 * egg_dbus_hash_map_clear:
 * @hash_map: A #EggDBusHashMap.
 *
 * Removes all key/value pairs from @hash_map.
 **/
void
egg_dbus_hash_map_clear (EggDBusHashMap *hash_map)
{
  g_hash_table_foreach_steal (hash_map->data,
                              (GHRFunc) remove_func,
                              hash_map);
}

/**
 * egg_dbus_hash_map_insert:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 * If a @value_free_func was supplied when creating @hash_map, the old value is freed using
 * that function. If a @key_free_func was supplied when creating @hash_map, the passed key
 * is freed using that function.
 **/
void
egg_dbus_hash_map_insert (EggDBusHashMap *hash_map,
                          gconstpointer   key,
                          gconstpointer   value)
{
  /* grr, would be nice if we could lookup GBoxedFreeFunc; then we could just pass
   * that to the hash table and wouldn't need to manage freeing stuff ourselves
   */
  egg_dbus_hash_map_remove (hash_map, key);

  g_hash_table_insert (hash_map->data, (gpointer) key, (gpointer) value);
}

/**
 * egg_dbus_hash_map_contains:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to check for.
 *
 * Checks if @hash_map contains a value for @key.
 *
 * Returns: %TRUE only if @hash_map contains a value for @key.
 **/
gboolean
egg_dbus_hash_map_contains (EggDBusHashMap *hash_map,
                            gconstpointer   key)
{
  return g_hash_table_lookup_extended (hash_map->data, key, NULL, NULL);
}

/**
 * egg_dbus_hash_map_lookup:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value %NULL. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * Note that the returned value is owned by @hash_map and may be invalid if
 * later removed from the map. If you want a copy, use egg_dbus_hash_map_lookup_copy()
 * instead.
 *
 * Returns: The value associated with @key or %NULL.
 **/
gpointer
egg_dbus_hash_map_lookup (EggDBusHashMap *hash_map,
                          gconstpointer   key)
{
  return g_hash_table_lookup (hash_map->data, key);
}

static gboolean
check_have_value_copy_func (EggDBusHashMap *hash_map)
{
  EggDBusHashMapPrivate *priv;

  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);

  if (G_LIKELY (priv->value_copy_func != NULL))
    return TRUE;

  g_error ("no value_copy_func set for EggDBusHashMap<%s,%s>",
           g_type_name (hash_map->key_type),
           g_type_name (hash_map->value_type));

  return FALSE;
}

/**
 * egg_dbus_hash_map_lookup_copy:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Like egg_dbus_hash_map_lookup() but returns a copy of the value.
 *
 * This method is <emphasis>optional</emphasis> as some value types (for example #G_TYPE_POINTER
 * and derived types) have no natural copy function and one might not have been set when @hash_map
 * was constructed. It is a programming error to call this method on @hash_map if there
 * is no value copy function on @hash_map (a warning will be printed using g_error() causing program
 * termination).
 *
 * Returns: A copy of the value associated with @key or %NULL.
 **/
gpointer
egg_dbus_hash_map_lookup_copy (EggDBusHashMap *hash_map,
                               gconstpointer   key)
{
  gpointer value;
  EggDBusHashMapPrivate *priv;

  if (!check_have_value_copy_func (hash_map))
    return NULL;

  priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);

  value = g_hash_table_lookup (hash_map->data, key);

  return priv->value_copy_func (hash_map, value);
}

/**
 * egg_dbus_hash_map_remove:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to remove.
 *
 * Removes a key and it associated value from @hash_map.
 *
 * If a @value_free_func was supplied when creating @hash_map, the value is freed using
 * that function. If a @key_free_func was supplied when creating @hash_map, the key in
 * the map is freed using that function.
 *
 * Returns: %TRUE if @key was removed from @hash_map.
 **/
gboolean
egg_dbus_hash_map_remove (EggDBusHashMap     *hash_map,
                          gconstpointer       key)
{
  gboolean have_item;
  gpointer orig_key;
  gpointer value;

  have_item = g_hash_table_lookup_extended (hash_map->data, key, &orig_key, &value);
  if (!have_item)
    goto out;

  g_hash_table_remove (hash_map->data, key);
  remove_func (orig_key, value, hash_map);

 out:
  return have_item;
}


/**
 * egg_dbus_hash_map_foreach:
 * @hash_map: A #EggDBusHashMap.
 * @func: Callback function.
 * @user_data: User data to pass to @func.
 *
 * Calls @func for each of the key/value pairs in @hash_map. The function is passed the key and
 * value of each pair, and the given @user_data parameter. The map may not be modified while iterating
 * over it (you can't add/remove items).
 *
 * Returns: %TRUE if the iteration was short-circuited, %FALSE otherwise.
 **/
gboolean
egg_dbus_hash_map_foreach (EggDBusHashMap            *hash_map,
                           EggDBusHashMapForeachFunc  func,
                           gpointer                   user_data)
{
  GHashTableIter hash_iter;
  gpointer key;
  gpointer value;
  gboolean ret;

  ret = TRUE;

  g_hash_table_iter_init (&hash_iter, hash_map->data);
  while (g_hash_table_iter_next (&hash_iter, &key, &value))
    {
      ret = func (hash_map, key, value, user_data);
      if (ret)
        break;
    }

  return ret;
}


/* ---------------------------------------------------------------------------------------------------- */
/* C convenience follows here */
/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_hash_map_contains_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to check for.
 *
 * Checks if @hash_map contains a value for @key.
 *
 * This is a C convenience function for when the key type is an integral
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 *
 * Returns: %TRUE only if @hash_map contains a value for @key.
 **/
gboolean
egg_dbus_hash_map_contains_fixed     (EggDBusHashMap     *hash_map,
                                      guint64             key)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  if (priv->key_fits_in_pointer)
    return egg_dbus_hash_map_contains (hash_map, GUINT_TO_POINTER ((guint) key));
  else
    return egg_dbus_hash_map_contains (hash_map, &key);
}

/**
 * egg_dbus_hash_map_contains_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to check for.
 *
 * Checks if @hash_map contains a value for @key.
 *
 * This is a C convenience function for when the key type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 *
 * Returns: %TRUE only if @hash_map contains a value for @key.
 **/
gboolean
egg_dbus_hash_map_contains_float     (EggDBusHashMap     *hash_map,
                                      gdouble             key)
{
  gfloat key_float;
  if (hash_map->key_type == G_TYPE_DOUBLE)
    return egg_dbus_hash_map_contains (hash_map, &key);
  key_float = key;
  return egg_dbus_hash_map_contains (hash_map, &key_float);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_hash_map_lookup_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value %NULL. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when the key type is an integral
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 *
 * Returns: The value associated with @key or %NULL.
 **/
gpointer
egg_dbus_hash_map_lookup_fixed       (EggDBusHashMap     *hash_map,
                                      guint64             key)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  if (priv->key_fits_in_pointer)
    return egg_dbus_hash_map_lookup (hash_map, GUINT_TO_POINTER ((guint) key));
  else
    return egg_dbus_hash_map_lookup (hash_map, &key);
}

/**
 * egg_dbus_hash_map_lookup_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value %NULL. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when the key type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 *
 * Returns: The value associated with @key or %NULL.
 **/
gpointer
egg_dbus_hash_map_lookup_float       (EggDBusHashMap     *hash_map,
                                      gdouble             key)
{
  gfloat key_float;
  if (hash_map->key_type == G_TYPE_DOUBLE)
    return egg_dbus_hash_map_lookup (hash_map, &key);
  key_float = key;
  return egg_dbus_hash_map_lookup (hash_map, &key_float);
}

/**
 * egg_dbus_hash_map_lookup_fixed_copy:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Like egg_dbus_hash_map_lookup_fixed() but returns a copy of the value.
 *
 * This method is <emphasis>optional</emphasis> as some value types (for example #G_TYPE_POINTER
 * and derived types) have no natural copy function and one might not have been set when @hash_map
 * was constructed. It is a programming error to call this method on @hash_map if there
 * is no value copy function on @hash_map (a warning will be printed using g_error() causing program
 * termination).
 *
 * This is a C convenience function for when the key type is an integral
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 *
 * Returns: A copy of the value associated with @key or %NULL.
 **/
gpointer
egg_dbus_hash_map_lookup_fixed_copy  (EggDBusHashMap     *hash_map,
                                      guint64             key)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  if (priv->key_fits_in_pointer)
    return egg_dbus_hash_map_lookup_copy (hash_map, GUINT_TO_POINTER ((guint) key));
  else
    return egg_dbus_hash_map_lookup_copy (hash_map, &key);
}

/**
 * egg_dbus_hash_map_lookup_float_copy:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Like egg_dbus_hash_map_lookup_float() but returns a copy of the value.
 *
 * This method is <emphasis>optional</emphasis> as some value types (for example #G_TYPE_POINTER
 * and derived types) have no natural copy function and one might not have been set when @hash_map
 * was constructed. It is a programming error to call this method on @hash_map if there
 * is no value copy function on @hash_map (a warning will be printed using g_error() causing program
 * termination).
 *
 * This is a C convenience function for when the key type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 *
 * Returns: A copy of the value associated with @key or %NULL.
 **/
gpointer
egg_dbus_hash_map_lookup_float_copy  (EggDBusHashMap     *hash_map,
                                      gdouble             key)
{
  gfloat key_float;
  if (hash_map->key_type == G_TYPE_DOUBLE)
    return egg_dbus_hash_map_lookup (hash_map, &key);
  key_float = key;
  return egg_dbus_hash_map_lookup_copy (hash_map, &key_float);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_hash_map_lookup_ptr_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value 0. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when the value type is an integral
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 *
 * Returns: The value associated with @key or 0.
 **/
guint64
egg_dbus_hash_map_lookup_ptr_fixed   (EggDBusHashMap     *hash_map,
                                      gconstpointer       key)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  guint64 *val = (guint64 *) egg_dbus_hash_map_lookup (hash_map, key);
  if (priv->value_fits_in_pointer)
    return (guint64) GPOINTER_TO_UINT (val);
  else
    return val != NULL ? *val : 0;
}

/**
 * egg_dbus_hash_map_lookup_ptr_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value 0.0. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when the value type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 *
 * Returns: The value associated with @key or 0.0.
 **/
gdouble
egg_dbus_hash_map_lookup_ptr_float   (EggDBusHashMap     *hash_map,
                                      gconstpointer       key)
{
  gpointer val = egg_dbus_hash_map_lookup (hash_map, key);
  if (hash_map->value_type == G_TYPE_DOUBLE)
    return val != NULL ? *((gdouble *) val) : 0.0;
  else
    return val != NULL ? *((gfloat *) val) : 0.0;
}

/**
 * egg_dbus_hash_map_lookup_fixed_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value 0. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when both the key and value types are integral
 * types (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 *
 * Returns: The value associated with @key or 0.
 **/
guint64
egg_dbus_hash_map_lookup_fixed_fixed (EggDBusHashMap     *hash_map,
                                      guint64             key)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  guint64 *val = egg_dbus_hash_map_lookup_fixed (hash_map, key);
  if (priv->value_fits_in_pointer)
    return (guint64) GPOINTER_TO_UINT (val);
  else
    return val != NULL ? *val : 0;
}

/**
 * egg_dbus_hash_map_lookup_fixed_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value 0.0. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when the key type is an integral type
 * (#G_TYPE_INT, #G_TYPE_UINT64 and so on) and value type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 *
 * Returns: The value associated with @key or 0.0.
 **/
gdouble
egg_dbus_hash_map_lookup_fixed_float (EggDBusHashMap     *hash_map,
                                      guint64             key)
{
  gpointer val = egg_dbus_hash_map_lookup_fixed (hash_map, key);
  if (hash_map->value_type == G_TYPE_DOUBLE)
    return val != NULL ? *((gdouble *) val) : 0.0;
  else
    return val != NULL ? *((gfloat *) val) : 0.0;
}

/**
 * egg_dbus_hash_map_lookup_float_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value 0. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when the key type is a floating point type
 * (#G_TYPE_FLOAT and #G_TYPE_DOUBLE) and the value type is an integral type
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 *
 * Returns:  The value associated with @key or 0.
 **/
guint64
egg_dbus_hash_map_lookup_float_fixed (EggDBusHashMap     *hash_map,
                                      gdouble             key)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  guint64 *val = egg_dbus_hash_map_lookup_float (hash_map, key);
  if (priv->value_fits_in_pointer)
    return (guint64) GPOINTER_TO_UINT (val);
  else
    return val != NULL ? *val : 0;
}

/**
 * egg_dbus_hash_map_lookup_float_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to look up value for.
 *
 * Looks up the value associated with @key in @hash_map. Note that this function
 * cannot distinguish between a key that is not present and one which is present
 * but has the value 0.0. If you need this distinction, use egg_dbus_hash_map_contains().
 *
 * This is a C convenience function for when both the key and value type are floating point
 * types (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 *
 * Returns: The value associated with @key or 0.0.
 **/
gdouble
egg_dbus_hash_map_lookup_float_float (EggDBusHashMap     *hash_map,
                                      gdouble             key)
{
  gpointer val = egg_dbus_hash_map_lookup_float (hash_map, key);
  if (hash_map->value_type == G_TYPE_DOUBLE)
    return val != NULL ? *((gdouble *) val) : 0.0;
  else
    return val != NULL ? *((gfloat *) val) : 0.0;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_hash_map_remove_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to remove.
 *
 * Removes a key and it associated value from @hash_map.
 *
 * If a @value_free_func was supplied when creating @hash_map, the value is freed using
 * that function.
 *
 * This is a C convenience function for when the key type is an integral
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 *
 * Returns: %TRUE if @key was removed from @hash_map.
 **/
gboolean
egg_dbus_hash_map_remove_fixed       (EggDBusHashMap     *hash_map,
                                      guint64             key)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  if (priv->key_fits_in_pointer)
    return egg_dbus_hash_map_remove (hash_map, GUINT_TO_POINTER ((guint) key));
  else
    return egg_dbus_hash_map_remove (hash_map, &key);
}

/**
 * egg_dbus_hash_map_remove_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to remove.
 *
 * Removes a key and it associated value from @hash_map.
 *
 * If a @value_free_func was supplied when creating @hash_map, the value is freed using
 * that function.
 *
 * This is a C convenience function for when the key type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 *
 * Returns: %TRUE if @key was removed from @hash_map.
 **/
gboolean
egg_dbus_hash_map_remove_float       (EggDBusHashMap     *hash_map,
                                      gdouble             key)
{
  gfloat key_float;
  if (hash_map->key_type == G_TYPE_DOUBLE)
    return egg_dbus_hash_map_remove (hash_map, &key);
  key_float = key;
  return egg_dbus_hash_map_remove (hash_map, &key_float);
}

/* ---------------------------------------------------------------------------------------------------- */

static gpointer
key_fixed_to_ptr (EggDBusHashMap     *hash_map,
                  guint64             value)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  if (priv->key_fits_in_pointer)
    return GUINT_TO_POINTER ((guint) value);
  else
    return g_memdup (&value, sizeof (guint64));
}

static gpointer
val_fixed_to_ptr (EggDBusHashMap     *hash_map,
                  guint64             value)
{
  EggDBusHashMapPrivate *priv = EGG_DBUS_HASH_MAP_GET_PRIVATE (hash_map);
  if (priv->value_fits_in_pointer)
    return GUINT_TO_POINTER ((guint) value);
  else
    return g_memdup (&value, sizeof (guint64));
}

static gpointer
key_float_to_ptr (EggDBusHashMap     *hash_map,
                  gdouble             value)
{
  gfloat value_gfloat;
  if (hash_map->key_type == G_TYPE_DOUBLE)
    return g_memdup (&value, sizeof (gdouble));
  value_gfloat = value;
  return g_memdup (&value_gfloat, sizeof (gfloat));
}

static gpointer
val_float_to_ptr (EggDBusHashMap     *hash_map,
                  gdouble             value)
{
  gfloat value_gfloat;
  if (hash_map->value_type == G_TYPE_DOUBLE)
    return g_memdup (&value, sizeof (gdouble));
  value_gfloat = value;
  return g_memdup (&value_gfloat, sizeof (gfloat));
}

/**
 * egg_dbus_hash_map_insert_ptr_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 * If a @key_free_func was supplied when creating @hash_map, the passed key
 * is freed using that function.
 *
 * This is a C convenience function for when the value type is an integral
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 **/
void
egg_dbus_hash_map_insert_ptr_fixed   (EggDBusHashMap     *hash_map,
                                      gconstpointer       key,
                                      guint64             value)
{
  egg_dbus_hash_map_insert (hash_map, key, val_fixed_to_ptr (hash_map, value));
}

/**
 * egg_dbus_hash_map_insert_ptr_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 * If a @key_free_func was supplied when creating @hash_map, the passed key
 * is freed using that function.
 *
 * This is a C convenience function for when the value type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 **/
void
egg_dbus_hash_map_insert_ptr_float   (EggDBusHashMap     *hash_map,
                                      gconstpointer       key,
                                      gdouble             value)
{
  egg_dbus_hash_map_insert (hash_map, key, val_float_to_ptr (hash_map, value));
}

/**
 * egg_dbus_hash_map_insert_fixed_ptr:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 * If a @value_free_func was supplied when creating @hash_map, the old value is freed using
 * that function.
 *
 * This is a C convenience function for when the key type is an integral
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 **/
void
egg_dbus_hash_map_insert_fixed_ptr   (EggDBusHashMap     *hash_map,
                                      guint64             key,
                                      gconstpointer       value)
{
  egg_dbus_hash_map_insert (hash_map, key_fixed_to_ptr (hash_map, key), value);
}

/**
 * egg_dbus_hash_map_insert_fixed_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 *
 * This is a C convenience function for when both the key and value types are integral
 * types (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 **/
void
egg_dbus_hash_map_insert_fixed_fixed (EggDBusHashMap     *hash_map,
                                      guint64             key,
                                      guint64             value)
{
  egg_dbus_hash_map_insert (hash_map, key_fixed_to_ptr (hash_map, key), val_fixed_to_ptr (hash_map, value));
}

/**
 * egg_dbus_hash_map_insert_fixed_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 *
 * This is a C convenience function for when the key type is an integral type
 * (#G_TYPE_INT, #G_TYPE_UINT64 and so on) and value type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 **/
void
egg_dbus_hash_map_insert_fixed_float (EggDBusHashMap     *hash_map,
                                      guint64             key,
                                      gdouble             value)
{
  egg_dbus_hash_map_insert (hash_map, key_fixed_to_ptr (hash_map, key), val_float_to_ptr (hash_map, value));
}

/**
 * egg_dbus_hash_map_insert_float_ptr:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 * If a @value_free_func was supplied when creating @hash_map, the old value is freed using
 * that function.
 *
 * This is a C convenience function for when the key type is a floating point
 * type (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 **/
void
egg_dbus_hash_map_insert_float_ptr   (EggDBusHashMap     *hash_map,
                                      gdouble             key,
                                      gconstpointer       value)
{
  egg_dbus_hash_map_insert (hash_map, key_float_to_ptr (hash_map, key), value);
}

/**
 * egg_dbus_hash_map_insert_float_fixed:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 *
 * This is a C convenience function for when the key type is a floating point type
 * (#G_TYPE_FLOAT and #G_TYPE_DOUBLE) and the value type is an integral type
 * type (#G_TYPE_INT, #G_TYPE_UINT64 and so on).
 **/
void
egg_dbus_hash_map_insert_float_fixed (EggDBusHashMap     *hash_map,
                                      gdouble             key,
                                      guint64             value)
{
  egg_dbus_hash_map_insert (hash_map, key_float_to_ptr (hash_map, key), val_fixed_to_ptr (hash_map, value));
}

/**
 * egg_dbus_hash_map_insert_float_float:
 * @hash_map: A #EggDBusHashMap.
 * @key: Key to insert.
 * @value: Value to insert.
 *
 * Inserts a new key and value into @hash_map.
 *
 * If the key already exists in the @hash_map its current value is replaced with the new value.
 *
 * This is a C convenience function for when both the key and value type are floating point
 * types (#G_TYPE_FLOAT and #G_TYPE_DOUBLE).
 **/
void
egg_dbus_hash_map_insert_float_float (EggDBusHashMap     *hash_map,
                                      gdouble             key,
                                      gdouble             value)
{
  egg_dbus_hash_map_insert (hash_map, key_float_to_ptr (hash_map, key), val_float_to_ptr (hash_map, value));
}

/* ---------------------------------------------------------------------------------------------------- */
