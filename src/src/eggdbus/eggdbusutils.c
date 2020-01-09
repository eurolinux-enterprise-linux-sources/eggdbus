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
#include <dbus/dbus.h>
#include <eggdbus/eggdbustypes.h>
#include <eggdbus/eggdbusmisctypes.h>
#include <eggdbus/eggdbusobjectpath.h>
#include <eggdbus/eggdbussignature.h>
#include <eggdbus/eggdbusutils.h>
#include <eggdbus/eggdbusstructure.h>
#include <eggdbus/eggdbusvariant.h>
#include <eggdbus/eggdbusarrayseq.h>
#include <eggdbus/eggdbushashmap.h>

/* ---------------------------------------------------------------------------------------------------- */

gchar *
egg_dbus_utils_uscore_to_camel_case (const gchar *uscore)
{
  const char *p;
  GString *str;
  gboolean last_was_uscore;

  last_was_uscore = TRUE;

  str = g_string_new (NULL);
  p = uscore;
  while (p && *p)
    {
      if (*p == '-' || *p == '_')
        {
          last_was_uscore = TRUE;
        }
      else
        {
          if (last_was_uscore)
            {
              g_string_append_c (str, g_ascii_toupper (*p));
              last_was_uscore = FALSE;
            }
          else
            g_string_append_c (str, *p);
        }
      ++p;
    }

  return g_string_free (str, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

gchar*
egg_dbus_utils_camel_case_to_uscore (const gchar *camel_case)
{
  const char *p;
  GString *str;

  str = g_string_new (NULL);
  p = camel_case;
  while (*p)
    {
      if (g_ascii_isupper (*p))
        {
          if (str->len > 0 &&
              (str->len < 1 || str->str[str->len-1] != '_') &&
              (str->len < 2 || str->str[str->len-2] != '_'))
            g_string_append_c (str, '_');
          g_string_append_c (str, g_ascii_tolower (*p));
        }
      else if (*p == '-')
        {
          g_string_append_c (str, '_');
        }
      else
        {
          g_string_append_c (str, *p);
        }
      ++p;
    }

  return g_string_free (str, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

gchar*
egg_dbus_utils_camel_case_to_hyphen (const gchar *camel_case)
{
  const char *p;
  GString *str;

  str = g_string_new (NULL);
  p = camel_case;
  while (*p)
    {
      if (g_ascii_isupper (*p))
        {
          if (str->len > 0 &&
              (str->len < 1 || str->str[str->len-1] != '-') &&
              (str->len < 2 || str->str[str->len-2] != '-'))
            g_string_append_c (str, '-');
          g_string_append_c (str, g_ascii_tolower (*p));
        }
      else if (*p == '_')
        {
          g_string_append_c (str, '-');
        }
      else
        {
          g_string_append_c (str, *p);
        }
      ++p;
    }

  return g_string_free (str, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_param_spec_for_signature:
 * @name: Canonical name of the property specified.
 * @nick: Nick name for the property specified or %NULL.
 * @blurb: Description of the property specified or %NULL.
 * @signature: A D-Bus signature.
 * @flags: Flags for the property specified.
 *
 * Creates a new #GParamSpec instance specifying a property that can
 * hold @signature. For example, if @signature is
 * <literal>"b"</literal>, this function will return a
 * #GParamSpecBoolean.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: A newly created parameter specification.
 **/
GParamSpec *
egg_dbus_param_spec_for_signature (const gchar  *name,
                                   const gchar  *nick,
                                   const gchar  *blurb,
                                   const gchar  *signature,
                                   GParamFlags   flags)
{
  GParamSpec *pspec;

  g_return_val_if_fail (signature != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  pspec = NULL;

  switch (signature[0])
    {
    case DBUS_TYPE_BYTE:
      pspec = g_param_spec_uchar (name, nick, blurb, 0, G_MAXUINT8, 0, flags);
      break;

    case DBUS_TYPE_BOOLEAN:
      pspec = g_param_spec_boolean (name, nick, blurb, FALSE, flags);
      break;

    case DBUS_TYPE_INT16:
      pspec = egg_dbus_param_spec_int16 (name, nick, blurb, G_MININT16, G_MAXINT16, 0, flags);
      break;

    case DBUS_TYPE_UINT16:
      pspec = egg_dbus_param_spec_uint16 (name, nick, blurb, 0, G_MAXUINT16, 0, flags);
      break;

    case DBUS_TYPE_INT32:
      pspec = g_param_spec_int (name, nick, blurb, G_MININT32, G_MAXINT32, 0, flags);
      break;

    case DBUS_TYPE_UINT32:
      pspec = g_param_spec_uint (name, nick, blurb, 0, G_MAXUINT32, 0, flags);
      break;

    case DBUS_TYPE_INT64:
      pspec = g_param_spec_int64 (name, nick, blurb, G_MININT64, G_MAXINT64, 0, flags);
      break;

    case DBUS_TYPE_UINT64:
      pspec = g_param_spec_uint64 (name, nick, blurb, 0, G_MAXUINT64, 0, flags);
      break;

    case DBUS_TYPE_DOUBLE:
      pspec = g_param_spec_double (name, nick, blurb, -G_MAXDOUBLE, G_MAXDOUBLE, 0.0, flags);
      break;

    case DBUS_TYPE_STRING:
      pspec = g_param_spec_string (name, nick, blurb, NULL, flags);
      break;

    case DBUS_TYPE_OBJECT_PATH:
      pspec = g_param_spec_boxed (name, nick, blurb, EGG_DBUS_TYPE_OBJECT_PATH, flags);
      break;

    case DBUS_TYPE_SIGNATURE:
      pspec = g_param_spec_boxed (name, nick, blurb, EGG_DBUS_TYPE_SIGNATURE, flags);
      break;

    case DBUS_TYPE_ARRAY:
      switch (signature[1])
        {
        case DBUS_TYPE_BYTE:
        case DBUS_TYPE_BOOLEAN:
        case DBUS_TYPE_INT16:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_INT64:
        case DBUS_TYPE_UINT64:
        case DBUS_TYPE_DOUBLE:
          pspec = g_param_spec_object (name, nick, blurb, EGG_DBUS_TYPE_ARRAY_SEQ, flags);
          break;

        case DBUS_TYPE_STRING:
          pspec = g_param_spec_boxed (name, nick, blurb, G_TYPE_STRV, flags);
          break;

        case DBUS_TYPE_OBJECT_PATH:
          pspec = g_param_spec_boxed (name, nick, blurb, EGG_DBUS_TYPE_OBJECT_PATH_ARRAY, flags);
          break;

        case DBUS_TYPE_SIGNATURE:
          pspec = g_param_spec_boxed (name, nick, blurb, EGG_DBUS_TYPE_SIGNATURE_ARRAY, flags);
          break;

        case DBUS_DICT_ENTRY_BEGIN_CHAR:
          pspec = g_param_spec_object (name, nick, blurb, EGG_DBUS_TYPE_HASH_MAP, flags);
          break;

        case DBUS_TYPE_ARRAY:
        case DBUS_STRUCT_BEGIN_CHAR:
          pspec = g_param_spec_object (name, nick, blurb, EGG_DBUS_TYPE_ARRAY_SEQ, flags);
          break;

        default:
          g_warning ("Can't determine type for signature '%s'", signature);
          g_assert_not_reached ();
          break;
        }
      break;

    case DBUS_STRUCT_BEGIN_CHAR:
      pspec = g_param_spec_object (name, nick, blurb, EGG_DBUS_TYPE_STRUCTURE, flags);
      break;

    case DBUS_TYPE_VARIANT:
      pspec = g_param_spec_object (name, nick, blurb, EGG_DBUS_TYPE_VARIANT, flags);
      break;

    default:
      g_warning ("Can't determine type for signature '%s'", signature);
      g_assert_not_reached ();
      break;
    }

  return pspec;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
print_simple (const GValue *value, const gchar *signature, guint indent)
{
  int n;
  GStrv strv;
  char *str_value;

  if (signature == NULL)
    goto print_without_sig;

  switch (signature[0])
    {
    case DBUS_TYPE_BYTE:
      g_print ("0x%02x", g_value_get_uchar (value));
      break;

    case DBUS_TYPE_INT16:
      g_print ("%" G_GINT16_FORMAT, g_value_get_int (value));
      break;

    case DBUS_TYPE_UINT16:
      g_print ("%" G_GUINT16_FORMAT, g_value_get_uint (value));
      break;

    case DBUS_TYPE_INT32:
      g_print ("%" G_GINT32_FORMAT, g_value_get_int (value));
      break;

    case DBUS_TYPE_UINT32:
      g_print ("%" G_GUINT32_FORMAT, g_value_get_uint (value));
      break;

    case DBUS_TYPE_INT64:
      g_print ("%" G_GINT64_FORMAT, g_value_get_int64 (value));
      break;

    case DBUS_TYPE_UINT64:
      g_print ("%" G_GUINT64_FORMAT, g_value_get_uint64 (value));
      break;

    case DBUS_TYPE_DOUBLE:
      g_print ("%g", g_value_get_double (value));
      break;

    case DBUS_TYPE_BOOLEAN:
      g_print ("%s", g_value_get_boolean (value) ? "True" : "False");
      break;

    case DBUS_TYPE_ARRAY:
      switch (signature[1])
        {
        case DBUS_TYPE_STRING:
          strv = g_value_get_boxed (value);
          g_print ("[");
          for (n = 0; strv && strv[n] != NULL; n++)
            {
              if (n != 0)
                g_print (", ");
              g_print ("\"%s\"", strv[n]);
            }
          g_print ("]");
          break;

        default:
          goto print_without_sig;
        }
      break;

    default:
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE:
      goto print_without_sig;
    }

  g_print ("\n");

  return;

 print_without_sig:

  str_value = g_strdup_value_contents (value);
  g_print ("%s\n", str_value);
  g_free (str_value);
}

#if 0
void
egg_dbus_utils_print_gvalue (const GValue  *value,
                           const gchar   *signature,
                           guint          indent)
{
  if (g_type_is_a (G_VALUE_TYPE (value), EGG_DBUS_TYPE_LIST))
    {
      egg_dbus_list_print (g_value_get_boxed (value), indent);
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), EGG_DBUS_TYPE_ARRAY))
    {
      egg_dbus_array_print (g_value_get_boxed (value), indent);
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), EGG_DBUS_TYPE_HASH_TABLE) ||
           g_type_is_a (G_VALUE_TYPE (value), G_TYPE_HASH_TABLE))
    {
      egg_dbus_hash_table_print (g_value_get_boxed (value), indent);
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), EGG_DBUS_TYPE_STRUCTURE))
    {
      egg_dbus_structure_print (g_value_get_object (value), indent);
    }
  else if (g_type_is_a (G_VALUE_TYPE (value), EGG_DBUS_TYPE_VARIANT))
    {
      egg_dbus_variant_print (g_value_get_object (value), indent);
    }
  else
    {
      print_simple (value, signature, indent);
    }
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

GType
egg_dbus_get_type_for_signature (const gchar *signature)
{
  GType type;

  type = G_TYPE_INVALID;

  if (strcmp (signature, DBUS_TYPE_STRING_AS_STRING) == 0)
    type = G_TYPE_STRING;
  else if (strcmp (signature, DBUS_TYPE_OBJECT_PATH_AS_STRING) == 0)
    type = EGG_DBUS_TYPE_OBJECT_PATH;
  else if (strcmp (signature, DBUS_TYPE_SIGNATURE_AS_STRING) == 0)
    type = EGG_DBUS_TYPE_SIGNATURE;
  else if (strcmp (signature, DBUS_TYPE_BYTE_AS_STRING) == 0)
    type = G_TYPE_UCHAR;
  else if (strcmp (signature, DBUS_TYPE_BOOLEAN_AS_STRING) == 0)
    type = G_TYPE_BOOLEAN;
  else if (strcmp (signature, DBUS_TYPE_INT16_AS_STRING) == 0)
    type = G_TYPE_INT;
  else if (strcmp (signature, DBUS_TYPE_UINT16_AS_STRING) == 0)
    type = G_TYPE_UINT;
  else if (strcmp (signature, DBUS_TYPE_INT32_AS_STRING) == 0)
    type = G_TYPE_INT;
  else if (strcmp (signature, DBUS_TYPE_UINT32_AS_STRING) == 0)
    type = G_TYPE_UINT;
  else if (strcmp (signature, DBUS_TYPE_INT64_AS_STRING) == 0)
    type = G_TYPE_INT64;
  else if (strcmp (signature, DBUS_TYPE_UINT64_AS_STRING) == 0)
    type = G_TYPE_UINT64;
  else if (strcmp (signature, DBUS_TYPE_DOUBLE_AS_STRING) == 0)
    type = G_TYPE_DOUBLE;
  else if (strcmp (signature, DBUS_TYPE_VARIANT_AS_STRING) == 0)
    type = EGG_DBUS_TYPE_VARIANT;
  else if (g_str_has_prefix (signature, DBUS_STRUCT_BEGIN_CHAR_AS_STRING))
    type = EGG_DBUS_TYPE_STRUCTURE;
  else if (g_str_has_prefix (signature, DBUS_TYPE_ARRAY_AS_STRING))
    {
      if (signature[1] == DBUS_TYPE_BYTE   ||
          signature[1] == DBUS_TYPE_INT16  ||
          signature[1] == DBUS_TYPE_UINT16 ||
          signature[1] == DBUS_TYPE_INT32  ||
          signature[1] == DBUS_TYPE_UINT32 ||
          signature[1] == DBUS_TYPE_INT64  ||
          signature[1] == DBUS_TYPE_UINT64 ||
          signature[1] == DBUS_TYPE_DOUBLE ||
          signature[1] == DBUS_TYPE_BOOLEAN)
        type = EGG_DBUS_TYPE_ARRAY_SEQ;
      else if (signature[1] == DBUS_TYPE_STRING)
        type = G_TYPE_STRV;
      else if (signature[1] == DBUS_TYPE_OBJECT_PATH)
        type = EGG_DBUS_TYPE_OBJECT_PATH_ARRAY;
      else if (signature[1] == DBUS_TYPE_SIGNATURE)
        type = EGG_DBUS_TYPE_SIGNATURE_ARRAY;
      else if (signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
        type = EGG_DBUS_TYPE_HASH_MAP;
      else
        type = EGG_DBUS_TYPE_ARRAY_SEQ;
    }

  if (type == G_TYPE_INVALID)
    {
      g_warning ("cannot determine GType for signature '%s'", signature);
    }

  return type;
}
