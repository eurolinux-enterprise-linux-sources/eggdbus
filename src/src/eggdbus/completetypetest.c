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

#include "eggdbuserror.h"
#include "completetype.h"

/* ---------------------------------------------------------------------------------------------------- */

static const struct {
  const gchar *string;
  const gchar *signature;
  const gchar *expanded;
  const gchar *expected_error;
} tests[] =
{
  { "Byte", "y", "Byte", NULL },
  { "Boolean", "b", "Boolean", NULL },
  { "Int16", "n", "Int16", NULL },
  { "UInt16", "q", "UInt16", NULL },
  { "Int32", "i", "Int32", NULL },
  { "UInt32", "u", "UInt32", NULL },
  { "Int64", "x", "Int64", NULL },
  { "UInt64", "t", "UInt64", NULL },
  { "Double", "d", "Double", NULL },
  { "String", "s", "String", NULL },
  { "ObjectPath", "o", "ObjectPath", NULL },
  { "Signature", "g", "Signature", NULL },

  { "Array<Byte>", "ay", "Array<Byte>", NULL },
  { "Array<Boolean>", "ab", "Array<Boolean>", NULL },
  { "Array<Int16>", "an", "Array<Int16>", NULL },

  { "Struct<Byte,Boolean,Int16>", "(ybn)", "Struct<Byte,Boolean,Int16>", NULL },
  { "Struct<Array<String>,Array<Int32>>", "(asai)", "Struct<Array<String>,Array<Int32>>", NULL },

  { "Struct<String,Struct<Int32,Int32>>", "(s(ii))", "Struct<String,Struct<Int32,Int32>>", NULL },

  { "Dict<String,String>", "a{ss}", "Dict<String,String>", NULL },

  { "Dict<String,Struct<Int32,Int32,Struct<String,Byte>>>", "a{s(ii(sy))}", "Dict<String,Struct<Int32,Int32,Struct<String,Byte>>>", NULL },

  { "TimeStamp", "i", "Int32", NULL },

  { "Authorization", "(i(sa{sv})(sa{sv}))", "Struct<Int32,Struct<String,Dict<String,Variant>>,Struct<String,Dict<String,Variant>>>", NULL },

  { "Struct<String,Authorization,Array<Identity>>", "(s(i(sa{sv})(sa{sv}))a(sa{sv}))", "Struct<String,Struct<Int32,Struct<String,Dict<String,Variant>>,Struct<String,Dict<String,Variant>>>,Array<Struct<String,Dict<String,Variant>>>>", NULL },

  { "Int323", NULL, NULL, "Unknown type 'Int323'"},

  { "Dict<Int323,String>", NULL, NULL, "Unknown type 'Int323'"},

  { NULL, NULL, NULL, NULL },
};

static gchar *
test_cb (const gchar  *user_type,
         gpointer      user_data,
         GError      **error)
{
  gchar *ret;

  ret = NULL;

  if (strcmp (user_type, "Identity") == 0)
    {
      ret = g_strdup ("Struct<String,Dict<String,Variant>>");
    }
  else if (strcmp (user_type, "Subject") == 0)
    {
      ret = g_strdup ("Struct<String,Dict<String,Variant>>");
    }
  else if (strcmp (user_type, "Authorization") == 0)
    {
      ret = g_strdup ("Struct<TimeStamp,Identity,Subject>");
    }
  else if (strcmp (user_type, "TimeStamp") == 0)
    {
      ret = g_strdup ("Int32");
    }
  else
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Unknown type '%s'",
                   user_type);
    }

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static const struct {
  const gchar *signature;
  const gchar *type_name;
  const gchar *expected_error;
} tests2[] =
{
  { "y", "Byte", NULL },
  { "b", "Boolean", NULL },
  { "n", "Int16", NULL },
  { "q", "UInt16", NULL },
  { "i", "Int32", NULL },
  { "u", "UInt32", NULL },
  { "x", "Int64", NULL },
  { "t", "UInt64", NULL },
  { "d", "Double", NULL },
  { "s", "String", NULL },
  { "o", "ObjectPath", NULL },
  { "g", "Signature", NULL },

  { "ay", "Array<Byte>", NULL },
  { "ab", "Array<Boolean>", NULL },
  { "an", "Array<Int16>", NULL },

  { "(ybn)", "Struct<Byte,Boolean,Int16>", NULL },
  { "(asai)", "Struct<Array<String>,Array<Int32>>", NULL },

  { "(s(ii))", "Struct<String,Struct<Int32,Int32>>", NULL },

  { "a{ss}", "Dict<String,String>", NULL },

  { "a{s(ii(sy))}", "Dict<String,Struct<Int32,Int32,Struct<String,Byte>>>", NULL },

  { "(ssa{sv})", "SomeStruct", NULL },

  { "a(ssa{sv})", "Array<SomeStruct>", NULL },

  { "a{s(ssa{sv})}", "Dict<String,SomeStruct>", NULL },

  { "(ss(ssa{sv}))", "SomeOtherStruct", NULL },

  { "(sa{sv})", NULL, "Cannot disambiguate '(sa{sv})'. Please specify the complete type." },

  { NULL, NULL, NULL },
};


static gchar *
test_sig_cb (const gchar  *signature,
             gpointer      user_data,
             GError      **error)
{
  gchar *ret;

  ret = NULL;

  if (strcmp (signature, "(ssa{sv})") == 0)
    {
      ret = g_strdup ("SomeStruct");
    }
  else if (strcmp (signature, "(ss(ssa{sv}))") == 0)
    {
      ret = g_strdup ("SomeOtherStruct");
    }
  else if (strcmp (signature, "(sa{sv})") == 0)
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Cannot disambiguate '(sa{sv})'. Please specify the complete type.");
    }

  return ret;
}

int
main (int argc, char *argv[])
{
  guint n;

  for (n = 0; tests[n].string != NULL; n++)
    {
      GError *error;
      CompleteType *type;

      error = NULL;
      type = complete_type_from_string (tests[n].string, test_cb, NULL, &error);

      if (tests[n].expected_error != NULL)
        {
          g_assert (type == NULL);
          g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_FAILED);
          g_assert_cmpstr (error->message, ==, tests[n].expected_error);
          g_error_free (error);
        }
      else
        {
          gchar *s;

          g_assert_no_error (error);
          g_assert_cmpstr (type->signature, ==, tests[n].signature);

          s = complete_type_to_string (type, FALSE);
          g_assert_cmpstr (tests[n].string, ==, s);
          g_free (s);

          s = complete_type_to_string (type, TRUE);
          g_assert_cmpstr (tests[n].expanded, ==, s);
          g_free (s);

          complete_type_free (type);
        }
    }

  for (n = 0; tests2[n].signature != NULL; n++)
    {
      GError *error;
      gchar *s;

      error = NULL;
      s = complete_type_name_from_signature (tests2[n].signature,
                                             test_sig_cb,
                                             NULL,
                                             &error);
      if (tests2[n].expected_error != NULL)
        {
          g_assert (s == NULL);
          g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_FAILED);
          g_assert_cmpstr (error->message, ==, tests2[n].expected_error);
          g_error_free (error);
        }
      else
        {
          g_assert_no_error (error);
          g_assert_cmpstr (s, ==, tests2[n].type_name);
          g_free (s);
        }
    }

  return 0;
}
