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

#if !defined (_EGG_DBUS_COMPILATION)
#error "This is a private header file. Do not use."
#endif

#ifndef __EGG_DBUS_PRIVATE_H
#define __EGG_DBUS_PRIVATE_H

#include <glib-object.h>
#include <dbus/dbus.h>
#include <eggdbus/eggdbustypes.h>

G_BEGIN_DECLS

const gchar *_get_element_signature (gpointer collection, const gchar *collection_type);

void _ref_element_signature (gpointer collection, const gchar *collection_type);

void _set_element_signature (gpointer collection, const gchar *signature, const gchar *collection_type);

GArray *_egg_dbus_array_copy_without_signature_copy (GArray *array);

EggDBusObjectProxy *_egg_dbus_object_proxy_new (EggDBusConnection *connection,
                                                const gchar       *name,
                                                const gchar       *object_path);

void _egg_dbus_connection_unregister_object_proxy  (EggDBusConnection   *connection,
                                                    EggDBusObjectProxy  *object_proxy);

void _egg_dbus_object_proxy_handle_message  (EggDBusObjectProxy  *proxy,
                                             DBusMessage         *message);

void _egg_dbus_object_proxy_dont_unref_connection_on_finalize  (EggDBusObjectProxy *proxy);

gchar *_egg_dbus_connection_get_owner_for_name (EggDBusConnection *connection,
                                                const gchar     *name);

gchar *_egg_dbus_error_encode_gerror (GError *error);

gboolean _egg_dbus_error_decode_gerror (const gchar *dbus_name,
                                        GQuark      *out_error_domain,
                                        gint        *out_error_code);

GError *_egg_dbus_error_new_remote_exception (const gchar   *name,
                                              const gchar   *message,
                                              GType         *error_types,
                                              const gchar   *format,
                                              ...) G_GNUC_PRINTF (4, 5);

void _egg_dbus_error_set_remote_exception (GError       **error,
                                           const gchar   *name,
                                           const gchar   *message,
                                           GType         *error_types,
                                           const gchar   *format,
                                           ...) G_GNUC_PRINTF (5, 6);

void _egg_dbus_interface_proxy_handle_property_changed (EggDBusInterfaceProxy *interface_proxy,
                                                        EggDBusHashMap        *changed_properties);

void _egg_dbus_interface_proxy_invalidate_properties (EggDBusInterfaceProxy *interface_proxy);

void _egg_dbus_interface_annotation_info_set (EggDBusInterfaceAnnotationInfo **annotations,
                                              const gchar                     *annotation_name,
                                              gpointer                         value);

G_END_DECLS

#endif /* __EGG_DBUS_PRIVATE_H */
