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

#ifndef __COMPLETE_TYPE_H
#define __COMPLETE_TYPE_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * CompleteTypeParserCallback:
 * @user_type: A user-defined type.
 * @user_data: User data.
 * @error: Return location for error.
 *
 * Callback function used in complete_type_from_string() for breaking @user_type
 * into other types.
 *
 * Returns: %NULL if @error is set, otherwise a broken down type string for @user_type (that
 * must be freeded with g_free()).
 */
typedef gchar *(*CompleteTypeParserCallback) (const gchar  *user_type,
                                              gpointer      user_data,
                                              GError      **error);

/**
 * CompleteTypeSignatureParserCallback:
 * @signature: The signature to resolve.
 * @user_data: User data.
 * @error: Return location for error.
 *
 * Callback function used in complete_type_from_signature() for inferring
 * a user defined type from @signature.
 *
 * Returns: %NULL if @error is set, otherwise a user defined type for @signature (that must
 * be freed with g_free()) or %NULL if there is no such user defined type.
 */
typedef gchar *(*CompleteTypeSignatureParserCallback) (const gchar  *signature,
                                                       gpointer      user_data,
                                                       GError      **error);

struct CompleteType;
typedef struct CompleteType CompleteType;

/**
 * CompleteType:
 * @signature: The D-Bus signature of the type.
 * @user_type: The name of the user supplied type or %NULL.
 * @num_contained_type: Number of elements in @contained_types.
 * @contained_types: Contained types (for structs, arrays and dicts).
 *
 * This structure represents a recursive complete type.
 */
struct CompleteType
{
  gchar *signature;
  gchar *user_type;
  guint  num_contained_types;
  CompleteType **contained_types;
};

/**
 * complete_type_free:
 * @type: The #CompleteType instance to free.
 *
 * Frees @type and all contained types.
 */
void complete_type_free (CompleteType *type);

/**
 * complete_type_from_string:
 * @string: The string to parse.
 * @callback: A #CompleteTypeParserCallback to break down user defined types or %NULL.
 * @user_data: User data to pass to @callback.
 * @error: Return location for error.
 *
 * Parses @string and returns a #CompleteType instance.
 *
 * Returns: A #CompleteType instance or %NULL if @error is set.
 */
CompleteType *complete_type_from_string (const gchar                 *string,
                                         CompleteTypeParserCallback   callback,
                                         gpointer                     user_data,
                                         GError                     **error);

/**
 * complete_type_name_from_signature:
 * @signature: The string to parse.
 * @out_signature: A string builder to append the D-Bus signature to.
 * @callback: A #CompleteTypeSignatureParserCallback to get user defined types or %NULL.
 * @user_data: User data to pass to @callback.
 * @error: Return location for error.
 *
 * Parses @string and returns a complete type name that can be used
 * with complete_type_from_string().
 *
 * Returns: The type name or %NULL if @error is set.
 */
gchar *complete_type_name_from_signature (const gchar                          *signature,
                                          CompleteTypeSignatureParserCallback   callback,
                                          gpointer                              user_data,
                                          GError                              **error);

/**
 * complete_type_to_string:
 * @type: A #CompleteType.
 * @expand_user_types: %TRUE if user defined types should be expanded to native D-Bus types.
 *
 * Constructs a human readable string representation of @type.
 *
 * Returns: The string representation of @type. Free with g_free().
 */
gchar        *complete_type_to_string   (CompleteType                *type,
                                         gboolean                     expand_user_types);

G_END_DECLS

#endif /* __COMPLETE_TYPE_H */
