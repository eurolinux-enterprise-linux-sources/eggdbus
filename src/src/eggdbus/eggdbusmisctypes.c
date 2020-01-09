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
#include <gobject/gvaluecollector.h>
#include <eggdbus/eggdbustypes.h>
#include <eggdbus/eggdbusmisctypes.h>

/**
 * SECTION:eggdbusmisctypes
 * @title: Fundamental types
 * @short_description: Types for 16-bit integers
 *
 * This really should be in GObject but it isn't.
 */

/* ---------------------------------------------------------------------------------------------------- */

 /**
 * egg_dbus_param_spec_int16:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #EggDBusParamSpecInt16 instance specifying a %EGG_DBUS_TYPE_INT16 property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: a newly created parameter specification
 */
GParamSpec*
egg_dbus_param_spec_int16 (const gchar *name,
                           const gchar *nick,
                           const gchar *blurb,
                           gint16       minimum,
                           gint16       maximum,
                           gint16       default_value,
                           GParamFlags  flags)
{
  EggDBusParamSpecInt16 *lspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  lspec = g_param_spec_internal (EGG_DBUS_TYPE_PARAM_INT16,
                                name,
                                nick,
                                blurb,
                                flags);

  lspec->minimum = minimum;
  lspec->maximum = maximum;
  lspec->default_value = default_value;

  return G_PARAM_SPEC (lspec);
}

/**
 * egg_dbus_param_spec_uint16:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #EggDBusParamSpecUInt16 instance specifying a %EGG_DBUS_TYPE_UINT16
 * property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: a newly created parameter specification
 */
GParamSpec*
egg_dbus_param_spec_uint16 (const gchar *name,
                            const gchar *nick,
                            const gchar *blurb,
                            guint16      minimum,
                            guint16      maximum,
                            guint16      default_value,
                            GParamFlags  flags)
{
  EggDBusParamSpecUInt16 *uspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  uspec = g_param_spec_internal (EGG_DBUS_TYPE_PARAM_UINT16,
                                name,
                                nick,
                                blurb,
                                flags);

  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;

  return G_PARAM_SPEC (uspec);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
param_int16_init (GParamSpec *pspec)
{
  EggDBusParamSpecInt16 *lspec = EGG_DBUS_PARAM_SPEC_INT16 (pspec);

  lspec->minimum = G_MININT16;
  lspec->maximum = G_MAXINT16;
  lspec->default_value = 0;
}

static void
param_int16_set_default (GParamSpec *pspec,
                         GValue     *value)
{
  value->data[0].v_int = EGG_DBUS_PARAM_SPEC_INT16 (pspec)->default_value;
}

static gboolean
param_int16_validate (GParamSpec *pspec,
                      GValue     *value)
{
  EggDBusParamSpecInt16 *lspec = EGG_DBUS_PARAM_SPEC_INT16 (pspec);
  gint16 oval = value->data[0].v_int;

  value->data[0].v_int = CLAMP (value->data[0].v_int, lspec->minimum, lspec->maximum);

  return value->data[0].v_int != oval;
}

static gint
param_int16_values_cmp (GParamSpec   *pspec,
                        const GValue *value1,
                        const GValue *value2)
{
  if (value1->data[0].v_int < value2->data[0].v_int)
    return -1;
  else
    return value1->data[0].v_int > value2->data[0].v_int;
}

static void
param_uint16_init (GParamSpec *pspec)
{
  EggDBusParamSpecUInt16 *uspec = EGG_DBUS_PARAM_SPEC_UINT16 (pspec);

  uspec->minimum = 0;
  uspec->maximum = G_MAXUINT16;
  uspec->default_value = 0;
}

static void
param_uint16_set_default (GParamSpec *pspec,
                          GValue     *value)
{
  value->data[0].v_uint = EGG_DBUS_PARAM_SPEC_UINT16 (pspec)->default_value;
}

static gboolean
param_uint16_validate (GParamSpec *pspec,
                       GValue     *value)
{
  EggDBusParamSpecUInt16 *uspec = EGG_DBUS_PARAM_SPEC_UINT16 (pspec);
  guint16 oval = value->data[0].v_uint;

  value->data[0].v_uint = CLAMP (value->data[0].v_uint, uspec->minimum, uspec->maximum);

  return value->data[0].v_uint != oval;
}

static gint
param_uint16_values_cmp (GParamSpec   *pspec,
                         const GValue *value1,
                         const GValue *value2)
{
  if (value1->data[0].v_uint < value2->data[0].v_uint)
    return -1;
  else
    return value1->data[0].v_uint > value2->data[0].v_uint;
}

GType
egg_dbus_param_int16_get_type (void)
{
  static GParamSpecTypeInfo pspec_info =
    {
      sizeof (EggDBusParamSpecInt16), /* instance_size */
      16,                             /* n_preallocs */
      param_int16_init,               /* instance_init */
      0,                              /* value_type */
      NULL,                           /* finalize */
      param_int16_set_default,        /* value_set_default */
      param_int16_validate,           /* value_validate */
      param_int16_values_cmp,         /* values_cmp */
    };
  static GType type = G_TYPE_INVALID;

  if (type != G_TYPE_INVALID)
    return type;

  pspec_info.value_type = EGG_DBUS_TYPE_INT16;

  type = g_param_type_register_static (g_intern_static_string ("EggDBusParamInt16"), &pspec_info);

  return type;
}

GType
egg_dbus_param_uint16_get_type (void)
{
  static GParamSpecTypeInfo pspec_info =
    {
      sizeof (EggDBusParamSpecUInt16), /* instance_size */
      16,                              /* n_preallocs */
      param_uint16_init,               /* instance_init */
      0,                               /* value_type */
      NULL,                            /* finalize */
      param_uint16_set_default,        /* value_set_default */
      param_uint16_validate,           /* value_validate */
      param_uint16_values_cmp,         /* values_cmp */
    };
  static GType type = G_TYPE_INVALID;

  if (type != G_TYPE_INVALID)
    return type;

  pspec_info.value_type = EGG_DBUS_TYPE_UINT16;

  type = g_param_type_register_static (g_intern_static_string ("EggDBusParamUInt16"), &pspec_info);

  return type;
}

/* ---------------------------------------------------------------------------------------------------- */


static void
value_init_int16 (GValue *value)
{
  value->data[0].v_int = 0;
}

static void
value_copy_int16 (const GValue *src_value,
                 GValue       *dest_value)
{
  dest_value->data[0].v_int = src_value->data[0].v_int;
}

 static gchar*
value_collect_int16 (GValue      *value,
                    guint        n_collect_values,
                    GTypeCValue *collect_values,
                    guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;

  return NULL;
}

static gchar*
value_lcopy_int16 (const GValue *value,
                  guint         n_collect_values,
                  GTypeCValue  *collect_values,
                  guint         collect_flags)
{
  gint16 *int16_p = collect_values[0].v_pointer;

  if (!int16_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  *int16_p = value->data[0].v_int;

  return NULL;
}

static const GTypeValueTable value_table_int16 = {
  value_init_int16,             /* value_init */
  NULL,                 /* value_free */
  value_copy_int16,             /* value_copy */
  NULL,                     /* value_peek_pointer */
  "q",                  /* collect_format */
  value_collect_int16,  /* collect_value */
  "p",                  /* lcopy_format */
  value_lcopy_int16,    /* lcopy_value */
};

GType
egg_dbus_int16_get_type (void)
{
  GTypeInfo info = {
    0,                          /* class_size */
    NULL,                       /* base_init */
    NULL,                       /* base_destroy */
    NULL,                       /* class_init */
    NULL,                       /* class_destroy */
    NULL,                       /* class_data */
    0,                          /* instance_size */
    0,                          /* n_preallocs */
    NULL,                       /* instance_init */
    &value_table_int16,         /* value_table */
  };
  const GTypeFundamentalInfo finfo = { G_TYPE_FLAG_DERIVABLE, };
  static GType type = G_TYPE_INVALID;

  if (type != G_TYPE_INVALID)
    return type;

  type = g_type_register_fundamental (g_type_fundamental_next (), g_intern_static_string ("eggdbusint16"), &info, &finfo, 0);

  return type;
}

GType
egg_dbus_uint16_get_type (void)
{
  GTypeInfo info = {
    0,                          /* class_size */
    NULL,                       /* base_init */
    NULL,                       /* base_destroy */
    NULL,                       /* class_init */
    NULL,                       /* class_destroy */
    NULL,                       /* class_data */
    0,                          /* instance_size */
    0,                          /* n_preallocs */
    NULL,                       /* instance_init */
    &value_table_int16,         /* value_table */
  };
  const GTypeFundamentalInfo finfo = { G_TYPE_FLAG_DERIVABLE, };
  static GType type = G_TYPE_INVALID;

  if (type != G_TYPE_INVALID)
    return type;

  type = g_type_register_fundamental (g_type_fundamental_next (), g_intern_static_string ("eggdbusuint16"), &info, &finfo, 0);

  return type;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_value_set_int16:
 * @value: a valid #GValue of type %EGG_DBUS_TYPE_INT16
 * @v_int16: 16bit integer value to be set
 *
 * Set the contents of a %EGG_DBUS_TYPE_INT16 #GValue to @v_int16.
 */
void
egg_dbus_value_set_int16 (GValue *value,
                  gint16  v_int16)
{
  g_return_if_fail (EGG_DBUS_VALUE_HOLDS_INT16 (value));

  value->data[0].v_int = v_int16;
}

/**
 * egg_dbus_value_get_int16:
 * @value: a valid #GValue of type %EGG_DBUS_TYPE_INT16
 *
 * Get the contents of a %EGG_DBUS_TYPE_INT16 #GValue.
 *
 * Returns: 16bit integer contents of @value
 */
gint16
egg_dbus_value_get_int16 (const GValue *value)
{
  g_return_val_if_fail (EGG_DBUS_VALUE_HOLDS_INT16 (value), 0);

  return value->data[0].v_int;
}

/* -- */

/**
 * egg_dbus_value_set_uint16:
 * @value: a valid #GValue of type %EGG_DBUS_TYPE_UINT16
 * @v_uint16: unsigned 16bit integer value to be set
 *
 * Set the contents of a %EGG_DBUS_TYPE_UINT16 #GValue to @v_uint16.
 */
void
egg_dbus_value_set_uint16 (GValue *value,
                   guint16 v_uint16)
{
  g_return_if_fail (EGG_DBUS_VALUE_HOLDS_UINT16 (value));

  value->data[0].v_uint = v_uint16;
}

/**
 * egg_dbus_value_get_uint16:
 * @value: a valid #GValue of type %EGG_DBUS_TYPE_UINT16
 *
 * Get the contents of a %EGG_DBUS_TYPE_UINT16 #GValue.
 *
 * Returns: unsigned 16bit integer contents of @value
 */
guint16
egg_dbus_value_get_uint16 (const GValue *value)
{
  g_return_val_if_fail (EGG_DBUS_VALUE_HOLDS_UINT16 (value), 0);

  return value->data[0].v_uint;
}

/* ---------------------------------------------------------------------------------------------------- */
