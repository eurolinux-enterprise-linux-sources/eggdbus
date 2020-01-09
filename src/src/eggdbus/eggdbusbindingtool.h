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

#ifndef __DBUS2GOBJECT_H
#define __DBUS2GOBJECT_H

#include <glib-object.h>
#include <eggdbus/eggdbusinterface.h>
#include "completetype.h"
#include "struct.h"
#include "enum.h"

G_BEGIN_DECLS

char *get_type_names_for_signature (const gchar *signature,
                                    const EggDBusInterfaceAnnotationInfo *annotations,
                                    gboolean is_in,
                                    gboolean want_const,
                                    char **out_gtype_name,
                                    const char **out_free_function_name,
                                    const char **out_gvalue_set_func_name,
                                    char **out_required_c_type,
                                    GError **error);

const char *get_c_marshaller_name_for_args (const EggDBusInterfaceArgInfo *args, guint num_args);

void print_include (const gchar *name_space, const gchar *class_name);

void print_includes (const gchar *name_space, gboolean is_c_file);

gchar *compute_file_name (const gchar *name_space, const gchar *class_name, const gchar *suffix);

/**
 * DocType:
 * @DOC_TYPE_GTKDOC:
 * @DOC_TYPE_DOCBOOK:
 *
 * The target consuming the documentation; used for translating method_calls(),
 * #Type::signal, #Type:property and %CONSTANTS to a suitable representation.
 */
typedef enum
{
  DOC_TYPE_GTKDOC,
  DOC_TYPE_DOCBOOK
} DocType;

gchar *get_doc         (const EggDBusInterfaceAnnotationInfo *annotations,
                        DocType                               type);
gchar *get_doc_summary (const EggDBusInterfaceAnnotationInfo *annotations,
                        DocType                               type);

GSList *get_enums_declared_in_interface (const EggDBusInterfaceInfo *interface);

GSList *get_structs_declared_in_interface (const EggDBusInterfaceInfo *interface);

CompleteType *get_complete_type_for_arg (const EggDBusInterfaceArgInfo *arg);

CompleteType *get_complete_type_for_property (const EggDBusInterfacePropertyInfo *property);

StructData *find_struct_by_name (const gchar *name);

EnumData *find_enum_by_name (const gchar *name);

G_END_DECLS

#endif /* __DBUS2GOBJECT_H */
