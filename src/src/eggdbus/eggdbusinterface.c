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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include <string.h>
#include "eggdbusinterface.h"
#include "eggdbusprivate.h"

/**
 * SECTION:eggdbusinterface
 * @title: EggDBusInterface
 * @short_description: Encapsulates a D-Bus interface
 *
 * The #EggDBusInterface interface is used for describing remote D-Bus interfaces
 * and dispatching messages.
 */

static void
base_init (gpointer g_iface)
{
  static gboolean is_initialized = FALSE;

  if (!is_initialized)
    {

      is_initialized = TRUE;
    }
}

GType
egg_dbus_interface_get_type (void)
{
  static GType iface_type = 0;

  if (iface_type == 0)
    {
      static const GTypeInfo info =
      {
        sizeof (EggDBusInterfaceIface),
        base_init,              /* base_init      */
        NULL,                   /* base_finalize  */
        NULL,                   /* class_init     */
        NULL,                   /* class_finalize */
        NULL,                   /* class_data     */
        0,                      /* instance_size  */
        0,                      /* n_preallocs    */
        NULL,                   /* instance_init  */
        NULL                    /* value_table    */
      };

      iface_type = g_type_register_static (G_TYPE_INTERFACE, "EggDBusInterface", &info, 0);

    }

  return iface_type;
}


/**
 * egg_dbus_interface_annotation_info_lookup:
 * @annotations: An array of annotations.
 * @annotation_name: The name of the annotation to look up.
 *
 * Looks up the value of an annotation.
 *
 * Returns: A string. Do not free, it is owned by @annotations.
 **/
const gchar *
egg_dbus_interface_annotation_info_lookup (const EggDBusInterfaceAnnotationInfo *annotations,
                                           const gchar                        *annotation_name)
{
  guint n;
  const gchar *ret;

  ret = NULL;

  for (n = 0; annotations != NULL && annotations[n].key != NULL; n++)
    {
      if (strcmp (annotations[n].key, annotation_name) == 0)
        {
          ret = annotations[n].value;
          goto out;
        }
    }

 out:
  return ret;
}

void
_egg_dbus_interface_annotation_info_set (EggDBusInterfaceAnnotationInfo **annotations,
                                         const gchar                     *annotation_name,
                                         gpointer                         value)
{
  guint n;

  /* TODO: handle value == NULL by removing the annotation */

  for (n = 0; *annotations != NULL && (*annotations)[n].key != NULL; n++)
    {
      if (strcmp ((*annotations)[n].key, annotation_name) == 0)
        {
          g_free ((gchar *) (*annotations)[n].value);
          (*annotations)[n].value = value;
          goto out;
        }
    }

  *annotations = g_realloc (*annotations, sizeof (EggDBusInterfaceAnnotationInfo) * (n + 2));

  (*annotations)[n].key = g_strdup (annotation_name);
  (*annotations)[n].value = value;
  (*annotations)[n].annotations = NULL;

  (*annotations)[n + 1].key = NULL;
  (*annotations)[n + 1].value = NULL;
  (*annotations)[n + 1].annotations = NULL;

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_interface_info_lookup_signal_for_g_name:
 * @info: A #EggDBusInterfaceInfo.
 * @g_name: A GObject signal name (lower case with hyphens)
 *
 * Looks up information about a signal.
 *
 * Returns: A #EggDBusInterfaceSignalInfo. Do not free, it is owned by @info.
 **/
const EggDBusInterfaceSignalInfo *
egg_dbus_interface_info_lookup_signal_for_g_name   (const EggDBusInterfaceInfo *info,
                                                    const gchar              *g_name)
{
  guint n;
  const EggDBusInterfaceSignalInfo *result;

  /* TODO: we need a hash table somewhere to make lookups O(1) */

  for (n = 0; n < info->num_signals; n++)
    {
      const EggDBusInterfaceSignalInfo *i = info->signals + n;

      if (strcmp (i->g_name, g_name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_interface_info_lookup_property_for_name:
 * @info: A #EggDBusInterfaceInfo.
 * @name: A D-Bus property name (typically in CamelCase).
 *
 * Looks up information about a property.
 *
 * Returns: A #EggDBusInterfacePropertyInfo. Do not free, it is owned by @info.
 **/
const EggDBusInterfacePropertyInfo *
egg_dbus_interface_info_lookup_property_for_name (const EggDBusInterfaceInfo *info,
                                                  const gchar              *name)
{
  guint n;
  const EggDBusInterfacePropertyInfo *result;

  /* TODO: we need a hash table somewhere to make lookups O(1) */

  for (n = 0; n < info->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *i = info->properties + n;

      if (strcmp (i->name, name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_interface_info_lookup_property_for_g_name:
 * @info: A #EggDBusInterfaceInfo.
 * @g_name: A GObject property name (e.g. lower case with hyphens).
 *
 * Looks up information about a property.
 *
 * Returns: A #EggDBusInterfacePropertyInfo. Do not free, it is owned by @info.
 **/
const EggDBusInterfacePropertyInfo *
egg_dbus_interface_info_lookup_property_for_g_name (const EggDBusInterfaceInfo *info,
                                                    const gchar              *g_name)
{
  guint n;
  const EggDBusInterfacePropertyInfo *result;

  /* TODO: we need a hash table somewhere to make lookups O(1) */

  for (n = 0; n < info->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *i = info->properties + n;

      if (strcmp (i->g_name, g_name) == 0)
        {
          result = i;
          goto out;
        }
    }

  result = NULL;

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

GType *
egg_dbus_bindings_get_error_domain_types (void)
{
  return NULL;
}

