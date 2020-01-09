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
#include <eggdbus/eggdbusobjectpath.h>

/**
 * SECTION:eggdbusobjectpath
 * @title: EggDBusObjectPath
 * @short_description: Object path types
 *
 * Types for working with D-Bus object paths.
 */

GType
egg_dbus_object_path_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (type != G_TYPE_INVALID)
    return type;

  type = g_boxed_type_register_static ("EggDBusObjectPath",
                                       (GBoxedCopyFunc) g_strdup,
                                       (GBoxedFreeFunc) g_free);
  return type;
}

GType
egg_dbus_object_path_array_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (type != G_TYPE_INVALID)
    return type;

  type = g_boxed_type_register_static ("EggDBusObjectPathArray",
                                       (GBoxedCopyFunc) g_strdupv,
                                       (GBoxedFreeFunc) g_strfreev);
  return type;
}
