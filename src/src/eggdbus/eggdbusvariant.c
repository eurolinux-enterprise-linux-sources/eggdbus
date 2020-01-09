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
#include <gobject/gvaluecollector.h>
#include <eggdbus/eggdbusmisctypes.h>
#include <eggdbus/eggdbusobjectpath.h>
#include <eggdbus/eggdbussignature.h>
#include <eggdbus/eggdbusstructure.h>
#include <eggdbus/eggdbusutils.h>
#include <eggdbus/eggdbustypes.h>
#include <eggdbus/eggdbusarrayseq.h>
#include <eggdbus/eggdbushashmap.h>
#include <eggdbus/eggdbusvariant.h>
#include <eggdbus/eggdbusprivate.h>

/**
 * SECTION:eggdbusvariant
 * @title: EggDBusVariant
 * @short_description: Holds a value and a type
 *
 * The #EggDBusVariant type is used to represent D-Bus variants. It is a polymorphic type
 * in the sense that it can hold any values of any type used in D-Bus. It is similar to
 * #GValue but provides a simpler API in addition to knowledge about the D-Bus signature.
 */

typedef struct {
  gchar *signature;
  GValue value;
} EggDBusVariantPrivate;


enum
{
    PROP_0,
    PROP_SIGNATURE,
};

#define EGG_DBUS_VARIANT_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_VARIANT, EggDBusVariantPrivate))

G_DEFINE_TYPE (EggDBusVariant, egg_dbus_variant, G_TYPE_OBJECT);

static void
egg_dbus_variant_init (EggDBusVariant *variant)
{
}

static void
egg_dbus_variant_finalize (GObject *object)
{
  EggDBusVariantPrivate *priv;

  priv = EGG_DBUS_VARIANT_GET_PRIVATE (object);

  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_free (priv->signature);

  G_OBJECT_CLASS (egg_dbus_variant_parent_class)->finalize (object);
}

static void
egg_dbus_variant_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EggDBusVariantPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (object,
                                      EGG_DBUS_TYPE_VARIANT,
                                      EggDBusVariantPrivate);
  switch (prop_id)
    {
    case PROP_SIGNATURE:
      priv->signature = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_variant_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EggDBusVariantPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (object,
                                      EGG_DBUS_TYPE_VARIANT,
                                      EggDBusVariantPrivate);

  switch (prop_id)
    {
    case PROP_SIGNATURE:
      g_value_set_boxed (value, priv->signature);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
egg_dbus_variant_class_init (EggDBusVariantClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_variant_finalize;
  gobject_class->set_property = egg_dbus_variant_set_property;
  gobject_class->get_property = egg_dbus_variant_get_property;

  g_type_class_add_private (klass, sizeof (EggDBusVariantPrivate));

  g_object_class_install_property (gobject_class,
                                   PROP_SIGNATURE,
                                   g_param_spec_boxed ("signature",
                                                       "Signature",
                                                       "The signature of the variant",
                                                       EGG_DBUS_TYPE_SIGNATURE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK |
                                                       G_PARAM_STATIC_BLURB));
}

/**
 * egg_dbus_variant_get_variant_signature:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the signature of the type that @variant holds.
 * 
 * Returns: A D-Bus signature. Do not free, the value is owned by @variant.
 **/
const gchar *
egg_dbus_variant_get_variant_signature (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (variant,
                                      EGG_DBUS_TYPE_VARIANT,
                                      EggDBusVariantPrivate);

  return priv->signature;
}

#if 0
void
egg_dbus_variant_print (EggDBusVariant   *variant,
                      guint           indent)
{
  EggDBusVariantPrivate *priv;
  gboolean simple;

  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));

  priv = G_TYPE_INSTANCE_GET_PRIVATE (variant,
                                      EGG_DBUS_TYPE_VARIANT,
                                      EggDBusVariantPrivate);

  g_print ("Variant:\n");
  g_print ("%*swrapping:  signature %s\n", indent + 2, "", priv->signature);
  g_print ("%*svalue:", indent + 2, "");

  /* simple types can be printed on the same line */
  simple = TRUE;
  if (G_VALUE_HOLDS (&priv->value, EGG_DBUS_TYPE_ARRAY_SEQ) ||
      G_VALUE_HOLDS (&priv->value, EGG_DBUS_TYPE_HASH_MAP) ||
      G_VALUE_HOLDS (&priv->value, EGG_DBUS_TYPE_STRUCTURE) ||
      G_VALUE_HOLDS (&priv->value, EGG_DBUS_TYPE_VARIANT))
    simple = FALSE;

  if (simple)
    g_print ("     ");
  else
    g_print ("\n%*s", indent + 4, "");

  egg_dbus_utils_print_gvalue (&priv->value,
                               priv->signature,
                               indent + 4);
}
#endif

static void
set_signature (EggDBusVariant *variant,
               const gchar  *signature)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (signature == NULL)
    {
      if (priv->signature != NULL)
        g_value_unset (&priv->value);
      priv->signature = NULL;
      g_free (priv->signature);
    }
  else
    {
      g_free (priv->signature);
      priv->signature = g_strdup (signature);
    }
}

static void
set_signature_for_array (EggDBusVariant *variant,
                         const gchar  *elem_signature)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (elem_signature == NULL)
    {
      if (priv->signature != NULL)
        g_value_unset (&priv->value);
      priv->signature = NULL;
      g_free (priv->signature);
    }
  else
    {
      g_free (priv->signature);
      priv->signature = g_strdup_printf ("a%s", elem_signature);
    }
}

static void
set_signature_for_hash_table (EggDBusVariant *variant,
                              const gchar  *key_signature,
                              const gchar  *value_signature)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (key_signature == NULL || value_signature == NULL)
    {
      if (priv->signature != NULL)
        g_value_unset (&priv->value);
      priv->signature = NULL;
      g_free (priv->signature);
    }
  else
    {
      g_free (priv->signature);
      priv->signature = g_strdup_printf ("a{%s%s}", key_signature, value_signature);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_variant_new:
 * 
 * Creates a new #EggDBusVariant that doesn't hold any value.
 * 
 * Returns: A #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new (void)
{
  EggDBusVariant *variant;
  variant = EGG_DBUS_VARIANT (g_object_new (EGG_DBUS_TYPE_VARIANT, NULL));
  return variant;
}

/**
 * egg_dbus_variant_new_for_gvalue:
 * @value: A #GValue.
 * @signature: D-Bus signature for @value.
 * 
 * Creates a new #EggDBusVariant from @value and @signature.
 * 
 * Returns: A #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_gvalue (const GValue *value,
                                 const gchar  *signature)
{
  EggDBusVariant *variant;
  EggDBusVariantPrivate *priv;

  variant = egg_dbus_variant_new ();
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);

  g_value_init (&priv->value, G_VALUE_TYPE (value));
  g_value_copy (value, &priv->value);
  set_signature (variant, signature);

  return variant;
}

/**
 * egg_dbus_variant_get_gvalue:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the value of @variant as a #GValue.
 * 
 * Returns: A pointer to a #GValue owned by @variant. Do not free.
 **/
const GValue *
egg_dbus_variant_get_gvalue (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return &priv->value;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_variant_new_for_string:
 * @value: A string.
 * 
 * Creates a new variant that holds a copy of @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_string (const gchar *value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_string (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_object_path:
 * @value: An object path.
 * 
 * Creates a new variant that holds a copy of @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_object_path (const gchar *value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_object_path (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_signature:
 * @value: A D-Bus signature.
 * 
 * Creates a new variant that holds a copy of @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_signature (const gchar *value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_signature (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_string_array:
 * @value: A string array.
 * 
 * Creates a new variant that holds a copy of @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_string_array (gchar **value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_string_array (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_object_path_array:
 * @value: An object path array.
 * 
 * Creates a new variant that holds a copy of @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_object_path_array (gchar **value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_object_path_array (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_signature_array:
 * @value: A signature array.
 * 
 * Creates a new variant that holds a copy of @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_signature_array (gchar **value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_signature_array (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_byte:
 * @value: A #guchar.
 * 
 * Creates a new variant that holds a #guchar equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_byte (guchar value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_byte (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_int16:
 * @value: A #gint16.
 * 
 * Creates a new variant that holds a #gint16 equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_int16 (gint16 value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_int16 (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_uint16:
 * @value: A #guint16.
 * 
 * Creates a new variant that holds a #guint16 equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_uint16 (guint16 value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_uint16 (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_int:
 * @value: A #gint.
 * 
 * Creates a new variant that holds a #gint equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_int (gint value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_int (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_uint:
 * @value: A #guint.
 * 
 * Creates a new variant that holds a #guint equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_uint (guint value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_uint (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_int64:
 * @value: A #gint64.
 * 
 * Creates a new variant that holds a #gint64 equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_int64 (gint64 value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_int64 (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_uint64:
 * @value: A #guint64.
 * 
 * Creates a new variant that holds a #guint64 equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_uint64 (guint64 value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_uint64 (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_boolean:
 * @value: A #gboolean.
 * 
 * Creates a new variant that holds a #gboolean equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_boolean (gboolean value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_boolean (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_double:
 * @value: A #gdouble.
 * 
 * Creates a new variant that holds a #gdouble equal to @value.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_double (gdouble value)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_double (variant, value);
  return variant;
}

/**
 * egg_dbus_variant_new_for_seq:
 * @seq: A #EggDBusArraySeq.
 * @element_signature: D-Bus signature of the elements stored in @seq.
 * 
 * Creates a new variant that holds a reference to @seq.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_seq (EggDBusArraySeq  *seq,
                              const gchar      *element_signature)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_seq (variant, seq, element_signature);
  return variant;
}

/**
 * egg_dbus_variant_new_for_map:
 * @map: A #EggDBusHashMap.
 * @key_signature: D-Bus signature of the keys stored in @map.
 * @value_signature: D-Bus signature of the values stored in @map.
 * 
 * Creates a new variant that holds a reference to @map.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_map (EggDBusHashMap   *map,
                              const gchar      *key_signature,
                              const gchar      *value_signature)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_map (variant, map, key_signature, value_signature);
  return variant;
}

/**
 * egg_dbus_variant_new_for_structure:
 * @structure: A #EggDBusStructure.
 * 
 * Creates a new variant that holds a reference to @structure.
 * 
 * Returns: A new #EggDBusVariant. Free with g_object_unref().
 **/
EggDBusVariant *
egg_dbus_variant_new_for_structure (EggDBusStructure *structure)
{
  EggDBusVariant *variant;
  variant = egg_dbus_variant_new ();
  egg_dbus_variant_set_structure (variant, structure);
  return variant;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_variant_set_string:
 * @variant: A #EggDBusVariant.
 * @value: A string.
 * 
 * Makes @variant hold a copy of @value.
 **/
void
egg_dbus_variant_set_string (EggDBusVariant *variant,
                           const gchar  *value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_STRING);
  g_value_set_string (&priv->value, value);
  set_signature (variant, "s");
}

/**
 * egg_dbus_variant_set_object_path:
 * @variant: A #EggDBusVariant.
 * @value: An object path.
 * 
  * Makes @variant hold a copy of @value.
 **/
void
egg_dbus_variant_set_object_path (EggDBusVariant *variant,
                                  const gchar    *value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_OBJECT_PATH);
  g_value_set_boxed (&priv->value, value);
  set_signature (variant, "o");
}

/**
 * egg_dbus_variant_set_signature:
 * @variant: A #EggDBusVariant.
 * @value: A signature.
 * 
  * Makes @variant hold a copy of @value.
 **/
void
egg_dbus_variant_set_signature (EggDBusVariant *variant,
                                const gchar    *value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_OBJECT_PATH);
  g_value_set_boxed (&priv->value, value);
  set_signature (variant, "o");
}

/**
 * egg_dbus_variant_set_string_array:
 * @variant: A #EggDBusVariant.
 * @value: A %NULL-terminated string array.
 * 
  * Makes @variant hold a copy of @value.
 **/
void
egg_dbus_variant_set_string_array (EggDBusVariant  *variant,
                                   gchar          **value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_STRV);
  g_value_set_boxed (&priv->value, value);
  set_signature (variant, "as");
}

/**
 * egg_dbus_variant_set_object_path_array:
 * @variant: A #EggDBusVariant.
 * @value: A %NULL-terminated object path array.
 * 
  * Makes @variant hold a copy of @value.
 **/
void
egg_dbus_variant_set_object_path_array (EggDBusVariant  *variant,
                                        gchar          **value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_OBJECT_PATH);
  g_value_set_boxed (&priv->value, value);
  set_signature (variant, "ao");
}

/**
 * egg_dbus_variant_set_signature_array:
 * @variant: A #EggDBusVariant.
 * @value: A %NULL-terminated signature array.
 * 
  * Makes @variant hold a copy of @value.
 **/
void
egg_dbus_variant_set_signature_array (EggDBusVariant  *variant,
                                      gchar          **value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_SIGNATURE);
  g_value_set_boxed (&priv->value, value);
  set_signature (variant, "ao");
}

/**
 * egg_dbus_variant_set_byte:
 * @variant: A #EggDBusVariant.
 * @value: A #guchar.
 * 
  * Makes @variant hold a #guchar equal to @value.
 **/
void
egg_dbus_variant_set_byte (EggDBusVariant *variant,
                         guchar        value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_UCHAR);
  g_value_set_uchar (&priv->value, value);
  set_signature (variant, "y");
}

/**
 * egg_dbus_variant_set_int16:
 * @variant: A #EggDBusVariant.
 * @value: A #gint16.
 * 
  * Makes @variant hold a #gint16 equal to @value.
 **/
void
egg_dbus_variant_set_int16 (EggDBusVariant *variant,
                          gint16        value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_INT16);
  egg_dbus_value_set_int16 (&priv->value, value);
  set_signature (variant, "n");
}

/**
 * egg_dbus_variant_set_uint16:
 * @variant: A #EggDBusVariant.
 * @value: A #guint16.
 * 
  * Makes @variant hold a #guint64 equal to @value.
 **/
void
egg_dbus_variant_set_uint16 (EggDBusVariant *variant,
                           guint16        value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_UINT16);
  egg_dbus_value_set_uint16 (&priv->value, value);
  set_signature (variant, "q");
}

/**
 * egg_dbus_variant_set_int:
 * @variant: A #EggDBusVariant.
 * @value: A #gint.
 * 
  * Makes @variant hold a #gint equal to @value.
 **/
void
egg_dbus_variant_set_int (EggDBusVariant *variant,
                          gint          value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_INT);
  g_value_set_int (&priv->value, value);
  set_signature (variant, "i");
}

/**
 * egg_dbus_variant_set_uint:
 * @variant: A #EggDBusVariant.
 * @value: A #guint.
 * 
  * Makes @variant hold a #guint equal to @value.
 **/
void
egg_dbus_variant_set_uint (EggDBusVariant *variant,
                           guint         value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_UINT);
  g_value_set_uint (&priv->value, value);
  set_signature (variant, "u");
}

/**
 * egg_dbus_variant_set_int64:
 * @variant: A #EggDBusVariant.
 * @value: A #gint64.
 * 
  * Makes @variant hold a #gint64 equal to @value.
 **/
void
egg_dbus_variant_set_int64 (EggDBusVariant *variant,
                          gint64        value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_INT64);
  g_value_set_int64 (&priv->value, value);
  set_signature (variant, "x");
}

/**
 * egg_dbus_variant_set_uint64:
 * @variant: A #EggDBusVariant.
 * @value: A #guint64.
 * 
  * Makes @variant hold a #guint64 equal to @value.
 **/
void
egg_dbus_variant_set_uint64 (EggDBusVariant *variant,
                           guint64       value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_UINT64);
  g_value_set_uint64 (&priv->value, value);
  set_signature (variant, "t");
}

/**
 * egg_dbus_variant_set_boolean:
 * @variant: A #EggDBusVariant.
 * @value: A #gboolean.
 * 
  * Makes @variant hold a #gboolean equal to @value.
 **/
void
egg_dbus_variant_set_boolean (EggDBusVariant *variant,
                            gboolean      value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&priv->value, value);
  set_signature (variant, "b");
}

/**
 * egg_dbus_variant_set_double:
 * @variant: A #EggDBusVariant.
 * @value: A #gdouble.
 * 
  * Makes @variant hold a #gdouble equal to @value.
 **/
void
egg_dbus_variant_set_double (EggDBusVariant *variant,
                           gdouble       value)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_DOUBLE);
  g_value_set_double (&priv->value, value);
  set_signature (variant, "d");
}

/**
 * egg_dbus_variant_set_seq:
 * @variant: A #EggDBusVariant.
 * @seq: A #EggDBusArraySeq.
 * @element_signature: D-Bus signature of the elements stored in @seq.
 * 
  * Makes @variant hold a reference to @seq.
 **/
void
egg_dbus_variant_set_seq (EggDBusVariant   *variant,
                          EggDBusArraySeq  *seq,
                          const gchar      *element_signature)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_ARRAY_SEQ);
  g_value_set_object (&priv->value, seq);
  set_signature_for_array (variant, element_signature);
}

/**
 * egg_dbus_variant_set_map:
 * @variant: A #EggDBusVariant.
 * @map: A #EggDBusHashMap.
 * @key_signature: D-Bus signature of the keys stored in @map.
 * @value_signature: D-Bus signature of the values stored in @map.
 * 
  * Makes @variant hold a reference to @map.
 **/
void
egg_dbus_variant_set_map (EggDBusVariant   *variant,
                          EggDBusHashMap   *map,
                          const gchar      *key_signature,
                          const gchar      *value_signature)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, EGG_DBUS_TYPE_HASH_MAP);
  g_value_set_object (&priv->value, map);
  set_signature_for_hash_table (variant,
                                key_signature,
                                value_signature);
}

/**
 * egg_dbus_variant_set_structure:
 * @variant: A #EggDBusVariant.
 * @structure: A #EggDBusStructure.
 * 
 * Makes @variant hold a reference to @structure.
 **/
void
egg_dbus_variant_set_structure (EggDBusVariant   *variant,
                                EggDBusStructure *structure)
{
  EggDBusVariantPrivate *priv;
  g_return_if_fail (EGG_DBUS_IS_VARIANT (variant));
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  if (priv->signature != NULL)
    g_value_unset (&priv->value);
  g_value_init (&priv->value, G_TYPE_OBJECT);
  g_value_set_object (&priv->value, structure);
  set_signature (variant, egg_dbus_structure_get_signature (structure));
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_variant_get_string:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the string stored in @variant.
 * 
 * Returns: A string. Free with g_free().
 **/
const gchar *
egg_dbus_variant_get_string (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_string (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_string (&priv->value);
}

/**
 * egg_dbus_variant_get_object_path:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the object path stored in @variant.
 * 
 * Returns: An object path. Free with g_free().
 **/
const gchar *
egg_dbus_variant_get_object_path (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_object_path (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_boxed (&priv->value);
}

/**
 * egg_dbus_variant_get_string_array:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the string array stored in @variant.
 * 
 * Returns: A %NULL-terminated string array. Free with g_strfreev().
 **/
gchar **
egg_dbus_variant_get_string_array (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_string_array (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_boxed (&priv->value);
}

/**
 * egg_dbus_variant_get_object_path_array:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the object path array stored in @variant.
 * 
 * Returns: A %NULL-terminated object path array. Free with g_strfreev().
 **/
gchar **
egg_dbus_variant_get_object_path_array (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_object_path_array (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_boxed (&priv->value);
}

/**
 * egg_dbus_variant_get_signature_array:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the signature array stored in @variant.
 * 
 * Returns: A %NULL-terminated signature array. Free with g_strfreev().
 **/
gchar **
egg_dbus_variant_get_signature_array (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_signature_array (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_boxed (&priv->value);
}

/**
 * egg_dbus_variant_get_byte:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #guchar stored in @variant.
 * 
 * Returns: A #guchar.
 **/
guchar
egg_dbus_variant_get_byte (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_byte (variant), 0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_uchar (&priv->value);
}

/**
 * egg_dbus_variant_get_int16:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #gint16 stored in @variant.
 * 
 * Returns: A #gint16.
 **/
gint16
egg_dbus_variant_get_int16 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_int16 (variant), 0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return egg_dbus_value_get_int16 (&priv->value);
}

/**
 * egg_dbus_variant_get_uint16:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #guint16 stored in @variant.
 * 
 * Returns: A #guint16.
 **/
guint16
egg_dbus_variant_get_uint16 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_uint16 (variant), 0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return egg_dbus_value_get_uint16 (&priv->value);
}

/**
 * egg_dbus_variant_get_int:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #gint stored in @variant.
 * 
 * Returns: A #gint.
 **/
gint
egg_dbus_variant_get_int (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_int (variant), 0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_int (&priv->value);
}

/**
 * egg_dbus_variant_get_uint:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #guint stored in @variant.
 * 
 * Returns: A #guint.
 **/
guint
egg_dbus_variant_get_uint (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_uint (variant), 0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_uint (&priv->value);
}

/**
 * egg_dbus_variant_get_int64:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #gint64 stored in @variant.
 * 
 * Returns: A #gint64.
 **/
gint64
egg_dbus_variant_get_int64 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_int64 (variant), 0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_int64 (&priv->value);
}

/**
 * egg_dbus_variant_get_uint64:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #guint64 stored in @variant.
 * 
 * Returns: A #guint64.
 **/
guint64
egg_dbus_variant_get_uint64 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_uint64 (variant), 0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_uint64 (&priv->value);
}

/**
 * egg_dbus_variant_get_boolean:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #gboolean stored in @variant.
 * 
 * Returns: A #gboolean.
 **/
gboolean
egg_dbus_variant_get_boolean (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_boolean (variant), FALSE);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_boolean (&priv->value);
}

/**
 * egg_dbus_variant_get_double:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the #gdouble stored in @variant.
 * 
 * Returns: A #gdouble.
 **/
gdouble
egg_dbus_variant_get_double (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_double (variant), 0.0);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_double (&priv->value);
}

/**
 * egg_dbus_variant_get_seq:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the sequence stored in @variant.
 * 
 * Returns: A #EggDBusArraySeq. Free with g_object_unref().
 **/
EggDBusArraySeq *
egg_dbus_variant_get_seq (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_seq (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_object (&priv->value);
}

/**
 * egg_dbus_variant_get_map:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the map stored in @variant.
 * 
 * Returns: A #EggDBusHashMap. Free with g_object_unref().
 **/
EggDBusHashMap  *
egg_dbus_variant_get_map (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_map (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_object (&priv->value);
}

/**
 * egg_dbus_variant_get_structure:
 * @variant: A #EggDBusVariant.
 * 
 * Gets the structure stored in @variant.
 * 
 * Returns: A #EggDBusStructure. Free with g_object_unref().
 **/
EggDBusStructure *
egg_dbus_variant_get_structure (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant) && egg_dbus_variant_is_structure (variant), NULL);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return g_value_get_object (&priv->value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_variant_is_unset:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a value.
 * 
 * Returns: %TRUE if @variant doesn't hold a value.
 **/
gboolean
egg_dbus_variant_is_unset (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  return priv->signature != NULL;
}

/**
 * egg_dbus_variant_is_string:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a string.
 * 
 * Returns: %TRUE only if @variant holds a string.
 **/
gboolean
egg_dbus_variant_is_string (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 's';
}

/**
 * egg_dbus_variant_is_object_path:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds an object path.
 * 
 * Returns: %TRUE only if @variant holds an object path.
 **/
gboolean
egg_dbus_variant_is_object_path (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'o';
}

/**
 * egg_dbus_variant_is_string_array:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a string array.
 * 
 * Returns: %TRUE only if @variant holds a string array.
 **/
gboolean
egg_dbus_variant_is_string_array (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'a' &&
    priv->signature[1] == 's';
}

/**
 * egg_dbus_variant_is_object_path_array:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds an object path array.
 * 
 * Returns: %TRUE only if @variant holds an object path array.
 **/
gboolean
egg_dbus_variant_is_object_path_array (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'a' &&
    priv->signature[1] == 'o';
}

/**
 * egg_dbus_variant_is_signature_array:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a signature array.
 * 
 * Returns: %TRUE only if @variant holds a signature array.
 **/
gboolean
egg_dbus_variant_is_signature_array (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'a' &&
    priv->signature[1] == 'g';
}

/**
 * egg_dbus_variant_is_byte:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #guchar.
 * 
 * Returns: %TRUE only if @variant holds a #guchar.
 **/
gboolean
egg_dbus_variant_is_byte (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'y';
}

/**
 * egg_dbus_variant_is_int16:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #gint16.
 * 
 * Returns: %TRUE only if @variant holds a #gint16.
 **/
gboolean
egg_dbus_variant_is_int16 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'n';
}

/**
 * egg_dbus_variant_is_uint16:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #guint16.
 * 
 * Returns: %TRUE only if @variant holds a #guint16.
 **/
gboolean
egg_dbus_variant_is_uint16 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'q';
}

/**
 * egg_dbus_variant_is_int:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #gint.
 * 
 * Returns: %TRUE only if @variant holds a #gint.
 **/
gboolean
egg_dbus_variant_is_int (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'i';
}

/**
 * egg_dbus_variant_is_uint:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #guint.
 * 
 * Returns: %TRUE only if @variant holds a #guint.
 **/
gboolean
egg_dbus_variant_is_uint (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'u';
}

/**
 * egg_dbus_variant_is_int64:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #gint64.
 * 
 * Returns: %TRUE only if @variant holds a #gint64.
 **/
gboolean
egg_dbus_variant_is_int64 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'x';
}

/**
 * egg_dbus_variant_is_uint64:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #guint64.
 * 
 * Returns: %TRUE only if @variant holds a #guint64.
 **/
gboolean
egg_dbus_variant_is_uint64 (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 't';
}

/**
 * egg_dbus_variant_is_boolean:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #gboolean.
 * 
 * Returns: %TRUE only if @variant holds a #gboolean.
 **/
gboolean
egg_dbus_variant_is_boolean (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'b';
}

/**
 * egg_dbus_variant_is_double:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a #gdouble.
 * 
 * Returns: %TRUE only if @variant holds a #gdouble.
 **/
gboolean
egg_dbus_variant_is_double (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'd';
}

/**
 * egg_dbus_variant_is_seq:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a sequence.
 * 
 * Returns: %TRUE only if @variant holds a sequence.
 **/
gboolean
egg_dbus_variant_is_seq (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'a' &&
    (priv->signature[1] == 'y' ||
     priv->signature[1] == 'n' ||
     priv->signature[1] == 'q' ||
     priv->signature[1] == 'i' ||
     priv->signature[1] == 'x' ||
     priv->signature[1] == 't' ||
     priv->signature[1] == 'd' ||
     priv->signature[1] == 'b' ||
     priv->signature[1] == 'a' ||
     priv->signature[1] == 'v' ||
     priv->signature[1] == '(');
}

/**
 * egg_dbus_variant_is_map:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a map.
 * 
 * Returns: %TRUE only if @variant holds a map.
 **/
gboolean
egg_dbus_variant_is_map (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == 'a' &&
    priv->signature[1] == '{';
}

/**
 * egg_dbus_variant_is_structure:
 * @variant: A #EggDBusVariant.
 * 
 * Checks if @variant holds a structure.
 * 
 * Returns: %TRUE only if @variant holds a structure.
 **/
gboolean
egg_dbus_variant_is_structure (EggDBusVariant *variant)
{
  EggDBusVariantPrivate *priv;
  priv = EGG_DBUS_VARIANT_GET_PRIVATE (variant);
  g_return_val_if_fail (EGG_DBUS_IS_VARIANT (variant), FALSE);
  return
    priv->signature != NULL &&
    priv->signature[0] == '(';
}

/* ---------------------------------------------------------------------------------------------------- */
