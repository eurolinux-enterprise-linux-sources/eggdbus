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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <eggdbus/eggdbuserror.h>
#include <eggdbus/eggdbusutils.h>

#include "eggdbusbindingtool.h"
#include "enum.h"

static void
enum_elem_data_free (EnumElemData *enum_elem_data)
{
  g_free (enum_elem_data->name);
  g_free (enum_elem_data->g_name_uscore_upper);
  g_free (enum_elem_data);
}

void
enum_data_free (EnumData *enum_data)
{
  guint n;

  g_free (enum_data->name);
  g_free (enum_data->name_uscore);
  g_free (enum_data->name_uscore_upper);
  g_free (enum_data->maximal_dbus_prefix);

  for (n = 0; n < enum_data->num_elements; n++)
    enum_elem_data_free (enum_data->elements[n]);
  g_free (enum_data->elements);

  g_free (enum_data);
}

/* Error Domains specified through annotations are of this form
 *
 *  <annotation name="org.gtk.EggDBus.DeclareErrorDomain" value="Error">
 *    <annotation name="org.gtk.EggDBus.ErrorDomain.Member" value="com.example.Error.Failed">
 *      <annotation name="org.gtk.EggDBus.DocString" value="Something went wrong"/>
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.ErrorDomain.Member" value="com.example.Error.FluxCapacitorFailure">
 *      <annotation name="org.gtk.EggDBus.DocString" value="The Flux Capacitor failed"/>
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.ErrorDomain.Member" value="com.example.Error.WouldDestruct">
 *      <annotation name="org.gtk.EggDBus.DocString" value="Operation would destruct the TwiddleFrobber"/>
 *    </annotation>
 *  </annotation>
 *
 */
EnumData *
enum_data_new_from_annotation_for_error_domain (const EggDBusInterfaceAnnotationInfo  *annotation,
                                                GError                               **error)
{
  EnumData *enum_data;
  GPtrArray *elems;
  guint n;
  gchar *maximal_dbus_prefix;
  guint next_value;

  enum_data = g_new0 (EnumData, 1);
  enum_data->type = ENUM_DATA_TYPE_ERROR_DOMAIN;
  enum_data->name = g_strdup (annotation->value);
  enum_data->name_uscore = egg_dbus_utils_camel_case_to_uscore (enum_data->name);
  enum_data->name_uscore_upper = g_ascii_strup (enum_data->name_uscore, -1);
  enum_data->annotations = annotation->annotations;

  elems = g_ptr_array_new ();

  maximal_dbus_prefix = NULL;

  next_value = 0;
  for (n = 0; annotation->annotations != NULL && annotation->annotations[n].key != NULL; n++)
    {
      const EggDBusInterfaceAnnotationInfo *embedded_annotation = annotation->annotations + n;
      EnumElemData *elem_data;
      const gchar *value_str;

      elem_data = NULL;

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.ErrorDomain.Member") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString.Summary") != 0)
        {
          g_set_error_literal (error,
                               EGG_DBUS_ERROR,
                               EGG_DBUS_ERROR_FAILED,
                               "Only org.gtk.EggDBus.ErrorDomain.Member annotations are allowed inside an "
                               "org.gtk.EggDBus.DeclareErrorDomain annotation");
          goto fail;
        }

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.ErrorDomain.Member") != 0)
        continue;

      elem_data = g_new0 (EnumElemData, 1);

      elem_data->name = g_strdup (embedded_annotation->value);
      elem_data->annotations = embedded_annotation->annotations;

      /* TODO: check the name is a valid D-Bus error name */

      value_str = egg_dbus_interface_annotation_info_lookup (embedded_annotation->annotations,
                                                             "org.gtk.EggDBus.ErrorDomain.Member.Value");
      if (value_str != NULL)
        {
          char *endp;
          elem_data->value = strtol (value_str, &endp, 0);
          if (*endp != '\0')
            {
              g_set_error (error,
                           EGG_DBUS_ERROR,
                           EGG_DBUS_ERROR_FAILED,
                           "Value '%s' of org.gtk.EggDBus.ErrorDomain.Member.Value is malformed",
                           value_str);
              goto fail;
            }
          next_value = elem_data->value;
        }
      else
        {
          elem_data->value = next_value;
        }
      next_value += 1;

      if (maximal_dbus_prefix == NULL)
        {
          maximal_dbus_prefix = g_strdup (elem_data->name);
        }
      else
        {
          /* make maximal_dbus_prefix the maximal prefix of both */
          if (g_str_has_prefix (elem_data->name, maximal_dbus_prefix))
            {
              /* good */
            }
          else
            {
              guint m;

              for (m = strlen (maximal_dbus_prefix) - 1; m > 0; m--)
                {
                  //g_printerr ("comparing %d chars of '%s' and '%s'\n", m, maximal_dbus_prefix, elem_data->name);
                  if (strncmp (maximal_dbus_prefix, elem_data->name, m) == 0)
                    {
                      //g_printerr ("gotcha\n");
                      g_free (maximal_dbus_prefix);
                      maximal_dbus_prefix = g_strndup (elem_data->name, m);
                      break;
                    }
                }

              if (m == 0)
                {
                  g_free (maximal_dbus_prefix);
                  maximal_dbus_prefix = g_strdup ("");
                }

            }
        }
      //g_printerr (" cur maximal_dbus_prefix='%s'\n", maximal_dbus_prefix);

      g_ptr_array_add (elems, elem_data);

    }

  //g_printerr ("maximal_dbus_prefix='%s'\n", maximal_dbus_prefix);

  /* now that we've established the prefix, compute g_name_uscore_upper for each element */
  for (n = 0; n < elems->len; n++)
    {
      EnumElemData *elem = (elems->pdata)[n];
      gchar *name;
      gchar *name_uscore;
      guint m;

      name = g_strdup (elem->name + strlen (maximal_dbus_prefix));
      /* replace dots with underscores */
      for (m = 0; name[m] != '\0'; m++)
        if (name[m] == '.')
          name[m] = '_';
      name_uscore = egg_dbus_utils_camel_case_to_uscore (name);
      elem->g_name_uscore_upper = g_ascii_strup (name_uscore, -1);
      g_free (name_uscore);
      g_free (name);
    }
  enum_data->maximal_dbus_prefix = maximal_dbus_prefix;

  enum_data->num_elements = elems->len;

  if (enum_data->num_elements == 0)
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "No members in error domain %s",
                   enum_data->name);
      goto fail;
    }

  enum_data->elements = (EnumElemData **) g_ptr_array_free (elems, FALSE);

  return enum_data;

 fail:
  enum_data_free (enum_data);
  g_free (maximal_dbus_prefix);
  if (elems != NULL)
    {
      g_ptr_array_foreach (elems, (GFunc) enum_elem_data_free, NULL);
      g_ptr_array_free (elems, TRUE);
    }
  return NULL;
}

/* Flags specified through annotations are of this form
 *
 *  <annotation name="org.gtk.EggDBus.DeclareFlags" value="CreateFlags">
 *    <annotation name="org.gtk.EggDBus.Flags.Member" value="UseFrobnicator">
 *      <annotation name="org.gtk.EggDBus.DocString" value="When creating, use the frobnicator to do so"/>
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.Flags.Member" value="LOG_ATTEMPT">
 *      <annotation name="org.gtk.EggDBus.DocString" value="Log any create attemps to the system log"/>
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.Flags.Member" value="some_other_flag">
 *      <!-- docstring explicitly left out -->
 *    </annotation>
 *  </annotation>
 *
 */
EnumData *
enum_data_new_from_annotation_for_flags (const EggDBusInterfaceAnnotationInfo  *annotation,
                                         GError                               **error)
{
  EnumData *enum_data;
  GPtrArray *elems;
  guint n;
  guint next_value;

  enum_data = g_new0 (EnumData, 1);
  enum_data->type = ENUM_DATA_TYPE_FLAGS;
  enum_data->name = g_strdup (annotation->value);
  enum_data->name_uscore = egg_dbus_utils_camel_case_to_uscore (enum_data->name);
  enum_data->name_uscore_upper = g_ascii_strup (enum_data->name_uscore, -1);
  enum_data->annotations = annotation->annotations;

  elems = g_ptr_array_new ();

  next_value = 1;
  for (n = 0; annotation->annotations != NULL && annotation->annotations[n].key != NULL; n++)
    {
      const EggDBusInterfaceAnnotationInfo *embedded_annotation = annotation->annotations + n;
      EnumElemData *elem_data;
      gchar *name_uscore;
      const gchar *value_str;

      elem_data = NULL;

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.Flags.Member") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString.Summary") != 0)
        {
          g_set_error_literal (error,
                               EGG_DBUS_ERROR,
                               EGG_DBUS_ERROR_FAILED,
                               "Only org.gtk.EggDBus.Flags.Member annotations are allowed inside an "
                               "org.gtk.EggDBus.Flags annotation");
          goto fail;
        }

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.Flags.Member") != 0)
        continue;

      elem_data = g_new0 (EnumElemData, 1);

      elem_data->name = g_strdup (embedded_annotation->value);
      elem_data->annotations = embedded_annotation->annotations;

      value_str = egg_dbus_interface_annotation_info_lookup (embedded_annotation->annotations,
                                                             "org.gtk.EggDBus.Flags.Member.Value");
      if (value_str != NULL)
        {
          char *endp;
          elem_data->value = strtol (value_str, &endp, 0);
          if (*endp != '\0')
            {
              g_set_error (error,
                           EGG_DBUS_ERROR,
                           EGG_DBUS_ERROR_FAILED,
                           "Value '%s' of org.gtk.EggDBus.Flags.Member.Value is malformed",
                           value_str);
              goto fail;
            }
          next_value = elem_data->value;
        }
      else
        {
          elem_data->value = next_value;
        }
      if (next_value == 0)
        next_value = 1;
      else
        next_value *= 2;

      name_uscore = egg_dbus_utils_camel_case_to_uscore (elem_data->name);
      elem_data->g_name_uscore_upper = g_ascii_strup (name_uscore, -1);
      g_free (name_uscore);

      g_ptr_array_add (elems, elem_data);

    }

  enum_data->num_elements = elems->len;

  if (enum_data->num_elements == 0)
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "No members in flag enumeration %s",
                   enum_data->name);
      goto fail;
    }

  enum_data->elements = (EnumElemData **) g_ptr_array_free (elems, FALSE);

  return enum_data;

 fail:
  enum_data_free (enum_data);
  if (elems != NULL)
    {
      g_ptr_array_foreach (elems, (GFunc) enum_elem_data_free, NULL);
      g_ptr_array_free (elems, TRUE);
    }
  return NULL;
}

/* Enumerations specified through annotations are of this form
 *
 *  <annotation name="org.gtk.EggDBus.DeclareEnum" value="Vehicle">
 *    <annotation name="org.gtk.EggDBus.Enum.Member" value="SportUtilityVehicle">
 *      <annotation name="org.gtk.EggDBus.DocString" value="A Sport Utility Vehicle"/>
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.Enum.Member" value="Convertible">
 *      <annotation name="org.gtk.EggDBus.DocString" value="A car without a top"/>
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.Enum.Member" value="Truck">
 *      <!-- docstring explicitly left out -->
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.Enum.Member" value="Patriot">
 *      <annotation name="org.gtk.EggDBus.DocString" value="As known from the GTA games"/>
 *    </annotation>
 *  </annotation>
 *
 */
EnumData *
enum_data_new_from_annotation_for_enum (const EggDBusInterfaceAnnotationInfo  *annotation,
                                        GError                               **error)
{
  EnumData *enum_data;
  GPtrArray *elems;
  guint n;
  guint next_value;

  enum_data = g_new0 (EnumData, 1);
  enum_data->type = ENUM_DATA_TYPE_ENUM;
  enum_data->name = g_strdup (annotation->value);
  enum_data->name_uscore = egg_dbus_utils_camel_case_to_uscore (enum_data->name);
  enum_data->name_uscore_upper = g_ascii_strup (enum_data->name_uscore, -1);
  enum_data->annotations = annotation->annotations;

  elems = g_ptr_array_new ();

  next_value = 0;
  for (n = 0; annotation->annotations != NULL && annotation->annotations[n].key != NULL; n++)
    {
      const EggDBusInterfaceAnnotationInfo *embedded_annotation = annotation->annotations + n;
      EnumElemData *elem_data;
      gchar *name_uscore;
      const gchar *value_str;

      elem_data = NULL;

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.Enum.Member") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString.Summary") != 0)
        {
          g_set_error_literal (error,
                               EGG_DBUS_ERROR,
                               EGG_DBUS_ERROR_FAILED,
                               "Only org.gtk.EggDBus.Enum.Member annotations are allowed inside an "
                               "org.gtk.EggDBus.Enum annotation");
          goto fail;
        }

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.Enum.Member") != 0)
        continue;

      elem_data = g_new0 (EnumElemData, 1);

      elem_data->name = g_strdup (embedded_annotation->value);
      elem_data->annotations = embedded_annotation->annotations;

      value_str = egg_dbus_interface_annotation_info_lookup (embedded_annotation->annotations,
                                                             "org.gtk.EggDBus.Enum.Member.Value");
      if (value_str != NULL)
        {
          char *endp;
          elem_data->value = strtol (value_str, &endp, 0);
          if (*endp != '\0')
            {
              g_set_error (error,
                           EGG_DBUS_ERROR,
                           EGG_DBUS_ERROR_FAILED,
                           "Value '%s' of org.gtk.EggDBus.Flags.Enum.Value is malformed",
                           value_str);
              goto fail;
            }
          next_value = elem_data->value;
        }
      else
        {
          elem_data->value = next_value;
        }
      next_value += 1;

      name_uscore = egg_dbus_utils_camel_case_to_uscore (elem_data->name);
      elem_data->g_name_uscore_upper = g_ascii_strup (name_uscore, -1);
      g_free (name_uscore);

      g_ptr_array_add (elems, elem_data);

    }

  enum_data->num_elements = elems->len;

  if (enum_data->num_elements == 0)
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "No members in enumeration %s",
                   enum_data->name);
      goto fail;
    }

  enum_data->elements = (EnumElemData **) g_ptr_array_free (elems, FALSE);

  return enum_data;

 fail:
  enum_data_free (enum_data);
  if (elems != NULL)
    {
      g_ptr_array_foreach (elems, (GFunc) enum_elem_data_free, NULL);
      g_ptr_array_free (elems, TRUE);
    }
  return NULL;
}

static gboolean
flags_has_none_value_already (EnumData *enum_data)
{
  gboolean ret;
  guint n;

  ret = TRUE;

  /* this can happen if either a) the value 0 is defined; or b) the name NONE exists */
  for (n = 0; n < enum_data->num_elements; n++)
    {
      EnumElemData *elem = enum_data->elements[n];

      if (elem->value == 0)
        goto out;

      if (strcmp (elem->g_name_uscore_upper, "NONE") == 0)
        goto out;
    }

  ret = FALSE;

 out:
  return ret;
}

gboolean
enum_generate_h_file (EnumData         *enum_data,
                      const char       *name_space,
                      const char       *output_name,
                      const char       *class_name,
                      GError          **error)
{
  gboolean ret;
  char *name_space_uscore;
  char *name_space_uscore_upper;
  char *full_instance_name;
  char *full_instance_name_uscore;
  char *header_file_protection;
  gchar *enum_summary_doc_string;
  gchar *enum_doc_string;
  guint n;

  ret = FALSE;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

  full_instance_name = g_strdup_printf ("%s%s", name_space, enum_data->name);
  full_instance_name_uscore = egg_dbus_utils_camel_case_to_uscore (full_instance_name);

  enum_summary_doc_string = get_doc_summary (enum_data->annotations, DOC_TYPE_GTKDOC);
  enum_doc_string = get_doc (enum_data->annotations, DOC_TYPE_GTKDOC);

  header_file_protection = g_strdup_printf ("__%s_%s_H",
                                            name_space_uscore_upper,
                                            enum_data->name_uscore_upper);

  g_print ("\n"
           "/* File: %s\n"
           " *\n"
           " * Generated by eggdbus-binding-tool %s. Do not edit.\n"
           " */\n"
           "\n",
           output_name,
           PACKAGE_VERSION);

  g_print ("#ifndef %s\n", header_file_protection);
  g_print ("#define %s\n", header_file_protection);
  g_print ("\n");
  g_print ("#include <glib-object.h>\n");
  g_print ("#include <gio/gio.h>\n");
  print_includes (name_space, FALSE);
  g_print ("\n");
  g_print ("G_BEGIN_DECLS\n");
  g_print ("\n");

  if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
    {
      g_print ("/**\n"
               " * %s_%s:\n"
               " *\n"
               " * Error domain for <literal>%s*</literal> D-Bus errors.\n"
               " * Errors in this domain will be from the #%s \n"
               " * enumeration. See #GError for information on error domains\n"
               " */\n"
               "#define %s_%s %s_%s_quark()\n"
               "\n"
               "GQuark %s_%s_quark (void);\n"
               "\n",
               name_space_uscore_upper, enum_data->name_uscore_upper,
               enum_data->maximal_dbus_prefix,
               full_instance_name,
               name_space_uscore_upper, enum_data->name_uscore_upper,
               name_space_uscore, enum_data->name_uscore,
               name_space_uscore, enum_data->name_uscore);
    }

  g_print ("GType %s_%s_get_type (void) G_GNUC_CONST;\n"
           "\n"
           "#define %s_TYPE_%s (%s_%s_get_type ())\n"
           "\n",
           name_space_uscore, enum_data->name_uscore,
           name_space_uscore_upper, enum_data->name_uscore_upper,
           name_space_uscore, enum_data->name_uscore);

  g_print ("/**\n"
           " * %s:\n",
           full_instance_name);

  if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
    {
      if (!flags_has_none_value_already (enum_data))
        {
          g_print (" * @%s_%s_NONE: No flags set.\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper);
        }
    }

  for (n = 0; n < enum_data->num_elements; n++)
    {
      EnumElemData *elem = enum_data->elements[n];
      gchar *doc_string;

      g_print (" * @%s_%s_%s: ",
               name_space_uscore_upper,
               enum_data->name_uscore_upper,
               elem->g_name_uscore_upper);

      doc_string = get_doc (elem->annotations, DOC_TYPE_GTKDOC);
      g_print ("%s\n", doc_string);
      g_free (doc_string);
    }

  g_print (" *\n"
           " * %s\n"
           " */\n"
           "typedef enum\n"
           "{\n",
           enum_doc_string);

  if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
    {
      if (!flags_has_none_value_already (enum_data))
        {
          g_print ("  %s_%s_NONE = 0x0000, /*< nick=none >*/\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper);
        }
    }

  for (n = 0; n < enum_data->num_elements; n++)
    {
      EnumElemData *elem = enum_data->elements[n];

      if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
        {
          g_print ("  %s_%s_%s = %d,   /*< nick=%s >*/\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   elem->g_name_uscore_upper,
                   elem->value,
                   elem->name);
        }
      else if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
        {
          g_print ("  %s_%s_%s = 0x%04x,\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   elem->g_name_uscore_upper,
                   elem->value);
        }
      else if (enum_data->type == ENUM_DATA_TYPE_ENUM)
        {
          g_print ("  %s_%s_%s = %d,\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   elem->g_name_uscore_upper,
                   elem->value);
        }
    }

  g_print ("} %s;\n"
           "\n",
           full_instance_name);

  g_print ("G_END_DECLS\n");
  g_print ("\n");
  g_print ("#endif /* %s */\n", header_file_protection);

  ret = TRUE;

  //out:

  g_free (header_file_protection);
  g_free (full_instance_name_uscore);
  g_free (full_instance_name);
  g_free (name_space_uscore);
  g_free (name_space_uscore_upper);
  g_free (enum_summary_doc_string);
  g_free (enum_doc_string);

  return ret;
}

gboolean
enum_generate_c_file (EnumData      *enum_data,
                      const char    *name_space,
                      const char    *output_name,
                      const char    *h_file_name,
                      const char    *class_name,
                      GError       **error)
{
  gboolean ret;
  char *name_space_hyphen;
  char *name_space_hyphen_lower;
  char *name_space_uscore;
  char *name_space_uscore_upper;
  char *full_instance_name;
  char *full_instance_name_uscore;
  char *full_instance_name_uscore_upper;
  gchar *enum_summary_doc_string;
  gchar *enum_doc_string;
  guint n;
  gchar *file_name;

  ret = FALSE;

  name_space_hyphen = egg_dbus_utils_camel_case_to_hyphen (name_space);
  name_space_hyphen_lower = g_ascii_strdown (name_space_hyphen, -1);

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

  full_instance_name = g_strdup_printf ("%s%s", name_space, enum_data->name);
  full_instance_name_uscore = egg_dbus_utils_camel_case_to_uscore (full_instance_name);
  full_instance_name_uscore_upper = g_ascii_strup (full_instance_name_uscore, -1);

  enum_summary_doc_string = get_doc_summary (enum_data->annotations, DOC_TYPE_GTKDOC);
  enum_doc_string = get_doc (enum_data->annotations, DOC_TYPE_GTKDOC);

  g_print ("\n"
           "/* File: %s\n"
           " *\n"
           " * Generated by eggdbus-binding-tool %s. Do not edit.\n"
           " */\n"
           "\n",
           output_name,
           PACKAGE_VERSION);

  g_print ("#ifdef HAVE_CONFIG_H\n"
           "#  include \"config.h\"\n"
           "#endif\n"
           "#include <string.h>\n");
  print_includes (name_space, TRUE);
  g_print ("#include \"%s\"\n"
           "\n",
           h_file_name);

  file_name = compute_file_name (name_space, enum_data->name, "");
  g_print ("/**\n"
           " * SECTION:%s\n"
           " * @title: %s%s\n"
           " * @short_description: %s\n"
           " *\n"
           " * %s\n"
           " */\n"
           "\n",
           file_name,
           name_space, enum_data->name,
           enum_summary_doc_string,
           enum_doc_string);
  g_free (file_name);

  if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
    {
      /* instead of using the traditional "<namespace>-<type>-quark" style
       * use the same name as for the registered GEnumClass type so we
       * can easily map back to the quark name.
       */
      g_print ("GQuark\n"
               "%s_%s_quark (void)\n"
               "{\n"
               "  return g_quark_from_static_string (\"%s%s\");\n"
               "}\n"
               "\n",
               name_space_uscore, enum_data->name_uscore,
               name_space, enum_data->name);
    }

  g_print ("GType %s_%s_get_type (void)\n"
           "{\n"
           "  static volatile gsize g_define_type_id__volatile = 0;\n"
           "\n"
           "  if (g_once_init_enter (&g_define_type_id__volatile))\n"
           "    {\n"
           "      GType g_define_type_id;\n",
           name_space_uscore, enum_data->name_uscore);

  if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN ||
    enum_data->type == ENUM_DATA_TYPE_ENUM)
    g_print ("      static const GEnumValue values[] = {\n");
  else if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
    g_print ("      static const GFlagsValue values[] = {\n");

  if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
    {
      if (!flags_has_none_value_already (enum_data))
        {
          g_print ("        {%s_%s_NONE, \"%s_%s_NONE\", \"none\"},\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper);
        }
    }

  for (n = 0; n < enum_data->num_elements; n++)
    {
      EnumElemData *elem = enum_data->elements[n];

      if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
        {
          g_print ("        {%s_%s_%s, \"%s_%s_%s\", \"%s\"},\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   elem->g_name_uscore_upper,
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   elem->g_name_uscore_upper,
                   elem->name);
        }
      else if (enum_data->type == ENUM_DATA_TYPE_FLAGS ||
               enum_data->type == ENUM_DATA_TYPE_ENUM)
        {
          gchar *name_hyphen;
          gchar *name_hyphen_lower;

          name_hyphen = egg_dbus_utils_camel_case_to_hyphen (elem->name);
          name_hyphen_lower = g_ascii_strdown (name_hyphen, -1);

          g_print ("        {%s_%s_%s, \"%s_%s_%s\", \"%s\"},\n",
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   elem->g_name_uscore_upper,
                   name_space_uscore_upper,
                   enum_data->name_uscore_upper,
                   elem->g_name_uscore_upper,
                   name_hyphen_lower);

          g_free (name_hyphen_lower);
          g_free (name_hyphen);
        }
    }
  g_print ("        };\n"
           "\n");

  if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN ||
      enum_data->type == ENUM_DATA_TYPE_ENUM)
    g_print ("      g_define_type_id = g_enum_register_static (g_intern_static_string (\"%s\"), values);\n",
             full_instance_name);
  else if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
    g_print ("      g_define_type_id = g_flags_register_static (g_intern_static_string (\"%s\"), values);\n",
             full_instance_name);


  g_print ("      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n"
           "    }\n"
           "\n"
           "  return g_define_type_id__volatile;\n"
           "}\n"
           "\n");

  ret = TRUE;

  g_free (name_space_hyphen);
  g_free (name_space_hyphen_lower);
  g_free (full_instance_name_uscore_upper);
  g_free (full_instance_name_uscore);
  g_free (full_instance_name);
  g_free (name_space_uscore);
  g_free (name_space_uscore_upper);
  g_free (enum_summary_doc_string);
  g_free (enum_doc_string);

  return ret;
}
