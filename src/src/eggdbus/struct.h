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

#ifndef __STRUCT_H
#define __STRUCT_H

#include <glib-object.h>
#include "eggdbusinterface.h"
#include "completetype.h"

G_BEGIN_DECLS

struct _StructData;
struct _StructElemData;
typedef struct _StructData StructData;
typedef struct _StructElemData StructElemData;

struct _StructData
{
  gchar           *name;
  gchar           *name_uscore;
  gchar           *name_uscore_upper;
  gboolean         user_supplied;

  /* these members are only set if user_supplied == FALSE */
  guint            num_elements;
  StructElemData **elements;

  const EggDBusInterfaceAnnotationInfo *annotations;

  /* the interface the struct was declared in or NULL if not declared in an interface */
  const EggDBusInterfaceInfo *interface;

  gchar        *signature;
  CompleteType *type;
  gchar *type_string;
};

struct _StructElemData
{
  gchar *name;
  gchar *type_string;

  const EggDBusInterfaceAnnotationInfo *annotations;

  gchar        *signature;
  CompleteType *type;
};

StructData *struct_data_new_from_annotation (const EggDBusInterfaceAnnotationInfo *annotation, GError **error);

gboolean struct_data_compute_types_and_signatures (StructData  *struct_data,
                                                   GError     **error);


void struct_data_free (StructData *struct_data);

gboolean struct_generate_h_file (StructData    *struct_data,
                                 const char    *name_space,
                                 const char    *output_name,
                                 const char    *class_name,
                                 GError       **error);

gboolean struct_generate_c_file (StructData    *struct_data,
                                 const char    *name_space,
                                 const char    *output_name,
                                 const char    *h_file_name,
                                 const char    *class_name,
                                 GError       **error);

G_END_DECLS

#endif /* __STRUCT_H */
