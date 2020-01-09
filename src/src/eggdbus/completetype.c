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

#include <dbus/dbus.h>
#include <string.h>

#include "eggdbuserror.h"
#include "completetype.h"

static gchar **
type_parser_split (const gchar  *list_of_types,
                   GError      **error)
{
  guint start;
  guint n;
  GPtrArray *p;
  gchar *s;
  gboolean ret;
  gint bracket_depth;

  ret = TRUE;

  p = g_ptr_array_new ();
  start = 0;

  bracket_depth = 0;
  for (n = 0; list_of_types[n] != '\0'; n++)
    {
      if (list_of_types[n] == '<')
        bracket_depth++;

      if (list_of_types[n] == '>')
        bracket_depth--;

      if (bracket_depth != 0)
        continue;

      if (list_of_types[n] == ',')
        {
          /* do the split */
          s = g_strndup (list_of_types + start, n - start);
          g_ptr_array_add (p, s);
          start = n + 1;
        }
    }

  /* last one */
  s = g_strndup (list_of_types + start, n - start);
  g_ptr_array_add (p, s);

  if (ret)
    {
      g_ptr_array_add (p, NULL);
      return (gchar **) g_ptr_array_free (p, FALSE);
    }
  else
    {
      g_ptr_array_free (p, TRUE);
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Unable to split '%s': bracket count mismatch",
                   list_of_types);
      return NULL;
    }
}

static CompleteType *
complete_type_from_string_real (const gchar                 *string,
                                CompleteTypeParserCallback   callback,
                                gpointer                     user_data,
                                GError                     **error,
                                gint                         depth)
{
  CompleteType *ret;

  ret = NULL;

  if (depth > 20)
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Max depth reached. Aborting.");
      goto out;
    }

  if (strcmp (string, "Byte") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("y");
      goto out;
    }
  else if (strcmp (string, "Boolean") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("b");
      goto out;
    }
  else if (strcmp (string, "Int16") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("n");
      goto out;
    }
  else if (strcmp (string, "UInt16") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("q");
      goto out;
    }
  else if (strcmp (string, "Int32") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("i");
      goto out;
    }
  else if (strcmp (string, "UInt32") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("u");
      goto out;
    }
  else if (strcmp (string, "Int64") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("x");
      goto out;
    }
  else if (strcmp (string, "UInt64") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("t");
      goto out;
    }
  else if (strcmp (string, "Double") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("d");
      goto out;
    }
  else if (strcmp (string, "String") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("s");
      goto out;
    }
  else if (strcmp (string, "ObjectPath") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("o");
      goto out;
    }
  else if (strcmp (string, "Signature") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("g");
      goto out;
    }
  else if (strcmp (string, "Variant") == 0)
    {
      ret = g_new0 (CompleteType, 1);
      ret->signature = g_strdup ("v");
      goto out;
    }
  else if (g_str_has_prefix (string, "Array<") && g_str_has_suffix (string, ">"))
    {
      gchar *contained_string;

      contained_string = g_strndup (string + 6, strlen (string) - 6 - 1);

      ret = g_new0 (CompleteType, 1);
      ret->contained_types = g_new0 (CompleteType *, 1);
      ret->contained_types[0] = complete_type_from_string_real (contained_string, callback, user_data, error, depth + 1);
      g_free (contained_string);

      if (ret->contained_types[0] == NULL)
        {
          complete_type_free (ret);
          ret = NULL;
          goto out;
        }
      ret->num_contained_types = 1;
      ret->signature = g_strdup_printf ("a%s", ret->contained_types[0]->signature);
      goto out;
    }
  else if (g_str_has_prefix (string, "Struct<") && g_str_has_suffix (string, ">"))
    {
      gchar **elems;
      gchar *contained_string;
      guint n;
      GString *s;

      contained_string = g_strndup (string + 7, strlen (string) - 7 - 1);
      elems = type_parser_split (contained_string, error);
      g_free (contained_string);

      if (elems == NULL)
        goto out;

      ret = g_new0 (CompleteType, 1);
      ret->contained_types = g_new0 (CompleteType *, g_strv_length (elems));

      s = g_string_new ("(");
      for (n = 0; elems[n] != NULL; n++)
        {
          ret->contained_types[n] = complete_type_from_string_real (elems[n], callback, user_data, error, depth + 1);
          if (ret->contained_types[n] == NULL)
            {
              g_string_free (s, TRUE);
              complete_type_free (ret);
              ret = NULL;
              goto out;
            }
          g_string_append (s, ret->contained_types[n]->signature);
          ret->num_contained_types += 1;
        }
      g_string_append_c (s, ')');
      ret->signature = g_string_free (s, FALSE);
      goto out;
    }
  else if (g_str_has_prefix (string, "Dict<") && g_str_has_suffix (string, ">"))
    {
      gchar **elems;
      gchar *contained_string;
      guint n;
      GString *s;

      contained_string = g_strndup (string + 5, strlen (string) - 5 - 1);
      elems = type_parser_split (contained_string, error);
      g_free (contained_string);

      if (elems == NULL)
        goto out;

      if (g_strv_length (elems) != 2)
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "Wrong number of arguments in Dict for '%s'",
                       string);
          g_strfreev (elems);
          goto out;
        }

      ret = g_new0 (CompleteType, 1);
      ret->contained_types = g_new0 (CompleteType *, g_strv_length (elems));

      s = g_string_new ("a{");
      for (n = 0; elems[n] != NULL; n++)
        {
          ret->contained_types[n] = complete_type_from_string_real (elems[n], callback, user_data, error, depth + 1);
          if (ret->contained_types[n] == NULL)
            {
              complete_type_free (ret);
              g_string_free (s, TRUE);
              ret = NULL;
              goto out;
            }
          g_string_append (s, ret->contained_types[n]->signature);
          ret->num_contained_types += 1;
        }
      g_string_append_c (s, '}');
      ret->signature = g_string_free (s, FALSE);
      goto out;
    }
  else
    {
      if (callback != NULL)
        {
          gchar *broken_down;

          broken_down = callback (string, user_data, error);
          if (broken_down == NULL)
            {
              ret = NULL;
              goto out;
            }
          ret = complete_type_from_string_real (broken_down, callback, user_data, error, depth + 1);
          if (ret == NULL)
            goto out;
          ret->user_type = g_strdup (string);
          g_free (broken_down);
          goto out;
        }
    }

  g_set_error (error,
               EGG_DBUS_ERROR,
               EGG_DBUS_ERROR_FAILED,
               "Error parsing '%s'",
               string);

 out:

  return ret;
}

CompleteType *
complete_type_from_string (const gchar                 *string,
                           CompleteTypeParserCallback   callback,
                           gpointer                     user_data,
                           GError                     **error)
{
  return complete_type_from_string_real (string,
                                         callback,
                                         user_data,
                                         error,
                                         0);
}

gchar *
complete_type_to_string (CompleteType *type,
                         gboolean      expand_user_types)
{
  gchar *ret;

  ret = NULL;

  if (!expand_user_types)
    {
      if (type->user_type != NULL)
        {
          ret = g_strdup (type->user_type);
          goto out;
        }
    }

  switch (type->signature[0])
    {
    case 'y':
      ret = g_strdup ("Byte");
      break;

    case 'b':
      ret = g_strdup ("Boolean");
      break;

    case 'n':
      ret = g_strdup ("Int16");
      break;

    case 'q':
      ret = g_strdup ("UInt16");
      break;

    case 'i':
      ret = g_strdup ("Int32");
      break;

    case 'u':
      ret = g_strdup ("UInt32");
      break;

    case 'x':
      ret = g_strdup ("Int64");
      break;

    case 't':
      ret = g_strdup ("UInt64");
      break;

    case 'd':
      ret = g_strdup ("Double");
      break;

    case 's':
      ret = g_strdup ("String");
      break;

    case 'o':
      ret = g_strdup ("ObjectPath");
      break;

    case 'g':
      ret = g_strdup ("Signature");
      break;

    case 'v':
      ret = g_strdup ("Variant");
      break;

    case 'a':
      if (type->signature[1] == '{')
        {
          ret = g_strdup_printf ("Dict<%s,%s>",
                                 complete_type_to_string (type->contained_types[0], expand_user_types),
                                 complete_type_to_string (type->contained_types[1], expand_user_types));
        }
      else
        {
          ret = g_strdup_printf ("Array<%s>",
                                 complete_type_to_string (type->contained_types[0], expand_user_types));
        }
      break;

    case '(':
      {
        GString *s;
        guint n;

        s = g_string_new ("Struct<");
        for (n = 0; n < type->num_contained_types; n++)
          {
            if (n > 0)
              g_string_append_c (s, ',');
            g_string_append (s, complete_type_to_string (type->contained_types[n], expand_user_types));
          }
        g_string_append_c (s, '>');
        ret = g_string_free (s, FALSE);
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

 out:
  return ret;
}

gchar *
complete_type_name_from_signature (const gchar                          *signature,
                                   CompleteTypeSignatureParserCallback   callback,
                                   gpointer                              user_data,
                                   GError                              **error)
{
  gchar *ret;
  DBusError derror;
  DBusSignatureIter iter;
  int type;

  ret = NULL;

  dbus_error_init (&derror);
  if (!dbus_signature_validate (signature, &derror))
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Signature '%s' not valid: %s: %s",
                   signature,
                   derror.name,
                   derror.message);
      dbus_error_free (&derror);
      goto out;
    }

  if (callback != NULL)
    {
      GError *local_error;

      local_error = NULL;
      ret = callback (signature, user_data, &local_error);

      if (ret != NULL)
        goto out;

      if (local_error != NULL)
        {
          g_propagate_error (error, local_error);
          goto out;
        }
    }

  dbus_signature_iter_init (&iter, signature);

  type = dbus_signature_iter_get_current_type (&iter);

  switch (type)
    {
    case 'y':
      ret = g_strdup ("Byte");
      break;

    case 'b':
      ret = g_strdup ("Boolean");
      break;

    case 'n':
      ret = g_strdup ("Int16");
      break;

    case 'q':
      ret = g_strdup ("UInt16");
      break;

    case 'i':
      ret = g_strdup ("Int32");
      break;

    case 'u':
      ret = g_strdup ("UInt32");
      break;

    case 'x':
      ret = g_strdup ("Int64");
      break;

    case 't':
      ret = g_strdup ("UInt64");
      break;

    case 'd':
      ret = g_strdup ("Double");
      break;

    case 's':
      ret = g_strdup ("String");
      break;

    case 'o':
      ret = g_strdup ("ObjectPath");
      break;

    case 'g':
      ret = g_strdup ("Signature");
      break;

    case 'v':
      ret = g_strdup ("Variant");
      break;

    case DBUS_TYPE_ARRAY:
      if (signature[1] == '{')
        {
          guint n;
          GString *s;
          DBusSignatureIter sub_iter;
          DBusSignatureIter sub_sub_iter;

          dbus_signature_iter_recurse (&iter, &sub_iter);
          dbus_signature_iter_recurse (&sub_iter, &sub_sub_iter);

          n = 0;
          s = g_string_new ("Dict<");
          do
            {
              char *contained;
              gchar *name;

              if (n > 0)
                g_string_append_c (s, ',');

              contained = dbus_signature_iter_get_signature (&sub_sub_iter);
              name = complete_type_name_from_signature (contained, callback, user_data, error);
              dbus_free (contained);
              if (name == NULL)
                {
                  g_string_free (s, TRUE);
                  goto out;
                }

              g_string_append (s, name);
              g_free (name);

              n++;
            }
          while (dbus_signature_iter_next (&sub_sub_iter));

          g_string_append_c (s, '>');

          ret = g_string_free (s, FALSE);
        }
      else
        {
          gchar *name;

          name = complete_type_name_from_signature (signature + 1, callback, user_data, error);
          if (name == NULL)
            goto out;

          ret = g_strdup_printf ("Array<%s>", name);

          g_free (name);
        }
      break;

    case DBUS_TYPE_STRUCT:
      {
        guint n;
        GString *s;
        DBusSignatureIter sub_iter;

        dbus_signature_iter_recurse (&iter, &sub_iter);

        n = 0;
        s = g_string_new ("Struct<");
        do
          {
            char *contained;
            gchar *name;

            if (n > 0)
              g_string_append_c (s, ',');

            contained = dbus_signature_iter_get_signature (&sub_iter);
            name = complete_type_name_from_signature (contained, callback, user_data, error);
            dbus_free (contained);
            if (name == NULL)
              {
                g_string_free (s, TRUE);
                goto out;
              }

            g_string_append (s, name);
            g_free (name);

            n++;
          }
        while (dbus_signature_iter_next (&sub_iter));

        g_string_append_c (s, '>');

        ret = g_string_free (s, FALSE);
      }
      break;

    default:
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Don't know how to parse signature '%s'",
                   signature);
      break;
    }

 out:
  return ret;
}


void
complete_type_free (CompleteType *type)
{
  guint n;

  g_free (type->signature);
  g_free (type->user_type);
  for (n = 0; n < type->num_contained_types; n++)
    complete_type_free (type->contained_types[n]);
  g_free (type->contained_types);
  g_free (type);
}
