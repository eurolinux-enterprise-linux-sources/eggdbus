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

#ifndef __ENUM_H
#define __ENUM_H

#include <glib-object.h>
#include "eggdbusinterface.h"

G_BEGIN_DECLS

struct _EnumData;
struct _EnumElemData;
typedef struct _EnumData EnumData;
typedef struct _EnumElemData EnumElemData;

typedef enum {
  ENUM_DATA_TYPE_ERROR_DOMAIN,
  ENUM_DATA_TYPE_FLAGS,
  ENUM_DATA_TYPE_ENUM,
} EnumDataType;

struct _EnumData
{
  /* the type of enumeration */
  EnumDataType type;

  /* the G name of the enumeration */
  gchar  *name;
  gchar  *name_uscore;
  gchar  *name_uscore_upper;

  /* the maximal D-Bus prefix of all errors in the error domain */
  gchar  *maximal_dbus_prefix;

  guint          num_elements;
  EnumElemData **elements;

  const EggDBusInterfaceAnnotationInfo *annotations;

  /* the interface the enum was declared in or NULL if not declared in an interface */
  const EggDBusInterfaceInfo *interface;
};

struct _EnumElemData
{
  /* the full D-Bus error name */
  gchar *name;

  /* the G name of the value (without enum prefix) */
  gchar *g_name_uscore_upper;

  guint value;

  const EggDBusInterfaceAnnotationInfo *annotations;
};

EnumData *enum_data_new_from_annotation_for_error_domain (const EggDBusInterfaceAnnotationInfo  *annotation,
                                                          GError                               **error);

EnumData *enum_data_new_from_annotation_for_flags (const EggDBusInterfaceAnnotationInfo  *annotation,
                                                   GError                               **error);

EnumData *enum_data_new_from_annotation_for_enum (const EggDBusInterfaceAnnotationInfo  *annotation,
                                                  GError                               **error);


void enum_data_free (EnumData *enum_data);

gboolean enum_generate_h_file (EnumData           *enum_data,
                               const char         *name_space,
                               const char         *output_name,
                               const char         *class_name,
                               GError            **error);

gboolean enum_generate_c_file (EnumData           *enum_data,
                               const char         *name_space,
                               const char         *output_name,
                               const char         *h_file_name,
                               const char         *class_name,
                               GError            **error);

G_END_DECLS

#endif /* __ENUM_H */
