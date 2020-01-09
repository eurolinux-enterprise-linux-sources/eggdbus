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
#include <string.h>
#include <dbus/dbus.h>
#include <eggdbus/eggdbuserror.h>
#include <eggdbus/eggdbusutils.h>

#include "eggdbusbindingtool.h"
#include "struct.h"

static void
struct_elem_data_free (StructElemData *struct_elem_data)
{
  g_free (struct_elem_data->name);
  g_free (struct_elem_data->type_string);
  if (struct_elem_data->type != NULL)
    complete_type_free (struct_elem_data->type);
  g_free (struct_elem_data->signature);
  g_free (struct_elem_data);
}

void
struct_data_free (StructData *struct_data)
{
  guint n;

  g_free (struct_data->name);
  g_free (struct_data->name_uscore);
  g_free (struct_data->name_uscore_upper);
  g_free (struct_data->signature);

  for (n = 0; n < struct_data->num_elements; n++)
    struct_elem_data_free (struct_data->elements[n]);
  g_free (struct_data->elements);

  g_free (struct_data);
}

static gchar *
break_down_type_cb (const gchar  *user_type,
                    gpointer      user_data,
                    GError      **error)
{
  StructData *struct_data;
  EnumData *enum_data;
  gchar *ret;

  ret = NULL;

  /* user type may be a struct */
  struct_data = find_struct_by_name (user_type);
  if (struct_data != NULL)
    {
      ret = g_strdup (struct_data->type_string);
      goto out;
    }

  /* ... or an enumeration */
  enum_data = find_enum_by_name (user_type);
  if (enum_data != NULL)
    {
      /* error domains can't be used */
      if (enum_data->type == ENUM_DATA_TYPE_FLAGS ||
          enum_data->type == ENUM_DATA_TYPE_ENUM)
        {
          ret = g_strdup ("UInt32");
          goto out;
        }
    }


  if (ret == NULL)
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Unknown type %s",
                   user_type);
    }

 out:
  return ret;
}

gboolean
struct_data_compute_types_and_signatures (StructData  *struct_data,
                                          GError     **error)
{
  gboolean ret;

  ret = FALSE;

  if (struct_data->signature != NULL)
    {
      ret = TRUE;
      goto out;
    }

  if (struct_data->user_supplied)
    {
      CompleteType *type;

      type = complete_type_from_string (struct_data->type_string,
                                        break_down_type_cb,
                                        NULL,
                                        error);

      if (type == NULL)
        goto out;

      struct_data->signature = g_strdup (type->signature);
      complete_type_free (type);
    }
  else
    {
      guint n;
      GString *signature;
      GString *type_string;

      type_string = g_string_new ("Struct<");
      signature = g_string_new ("(");

      for (n = 0; n < struct_data->num_elements; n++)
        {
          StructElemData *elem = struct_data->elements[n];

          elem->type = complete_type_from_string (elem->type_string,
                                                  break_down_type_cb,
                                                  NULL,
                                                  error);

          if (elem->type == NULL)
            {
              g_string_free (signature, TRUE);
              goto out;
            }

          /* this is a little redundant */
          elem->signature = g_strdup (elem->type->signature);

          if (n > 0)
            g_string_append_c (type_string, ',');
          g_string_append (type_string, elem->type_string);
          g_string_append (signature, elem->type->signature);
        }

      g_string_append_c (type_string, '>');
      g_string_append_c (signature, ')');
      struct_data->signature = g_string_free (signature, FALSE);
      struct_data->type_string = g_string_free (type_string, FALSE);
    }

  ret = TRUE;

 out:
  return ret;
}

/* Structs specified through annotations are of this form
 *
 *  <annotation name="org.gtk.EggDBus.DeclareStruct" value="DescribedPoint">
 *    <annotation name="org.gtk.EggDBus.Struct.Member" value="s:desc">
 *      <annotation name="org.gtk.EggDBus.DocString" value="A description of the described point"/>
 *    </annotation>
 *    <annotation name="org.gtk.EggDBus.Struct.Member" value="(ii):point">
 *      <annotation name="org.gtk.EggDBus.DocString" value="The point being described"/>
 *      <annotation name="org.gtk.EggDBus.StructType"  value="Point"/>
 *    </annotation>
 *  </annotation>
 *
 * of the user can supply the GInterface himself if this form is used
 *
 *  <annotation name="org.gtk.EggDBus.DeclareStruct"       value="TestSubject">
 *    <annotation name="org.gtk.EggDBus.Struct.Signature"  value="(sa{sv})"/>
 *  </annotation>
 *
 */

StructData *
struct_data_new_from_annotation (const EggDBusInterfaceAnnotationInfo  *annotation,
                                 GError                               **error)
{
  StructData *struct_data;
  GPtrArray *elems;
  const gchar *user_supplied;
  guint n;

  struct_data = g_new0 (StructData, 1);
  struct_data->name = g_strdup (annotation->value);
  struct_data->name_uscore = egg_dbus_utils_camel_case_to_uscore (struct_data->name);
  struct_data->name_uscore_upper = g_ascii_strup (struct_data->name_uscore, -1);
  struct_data->annotations = annotation->annotations;

  elems = NULL;

  /* check for user supplied structs */
  user_supplied = egg_dbus_interface_annotation_info_lookup (annotation->annotations,
                                                             "org.gtk.EggDBus.Struct.Type");
  if (user_supplied != NULL)
    {
      struct_data->type_string = g_strdup (user_supplied);
      struct_data->user_supplied = TRUE;
      goto out;
    }

  elems = g_ptr_array_new ();

  for (n = 0; annotation->annotations != NULL && annotation->annotations[n].key != NULL; n++)
    {
      const EggDBusInterfaceAnnotationInfo *embedded_annotation = annotation->annotations + n;
      const char *s;
      StructElemData *elem_data;

      elem_data = NULL;

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.Struct.Member") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString") != 0 &&
          strcmp (embedded_annotation->key, "org.gtk.EggDBus.DocString.Summary") != 0)
        {
          g_set_error_literal (error,
                               EGG_DBUS_ERROR,
                               EGG_DBUS_ERROR_FAILED,
                               "Only org.gtk.EggDBus.Struct.Member annotations are allowed inside an "
                               "org.gtk.EggDBus.DeclareStruct annotation");
          struct_elem_data_free (elem_data);
          goto fail;
        }

      if (strcmp (embedded_annotation->key, "org.gtk.EggDBus.Struct.Member") != 0)
        continue;

      elem_data = g_new0 (StructElemData, 1);

      s = strchr (embedded_annotation->value, ':');
      if (s == NULL)
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "No typename:name separator found for value '%s'",
                       embedded_annotation->value);
          struct_elem_data_free (elem_data);
          goto fail;
        }

      elem_data->type_string = g_strndup (embedded_annotation->value, s - embedded_annotation->value);
      elem_data->name = g_strdup (s + 1);

      elem_data->annotations = embedded_annotation->annotations;

      /* TODO: validate name */

      g_ptr_array_add (elems, elem_data);
    }

  struct_data->num_elements = elems->len;
  struct_data->elements = (StructElemData **) g_ptr_array_free (elems, FALSE);

 out:
  return struct_data;

 fail:
  struct_data_free (struct_data);
  if (elems != NULL)
    {
      g_ptr_array_foreach (elems, (GFunc) struct_elem_data_free, NULL);
      g_ptr_array_free (elems, TRUE);
    }
  return NULL;
}


gboolean
struct_generate_h_file (StructData    *struct_data,
                        const char    *name_space,
                        const char    *output_name,
                        const char    *class_name,
                        GError       **error)
{
  gboolean ret;
  char *name_space_uscore;
  char *name_space_uscore_upper;
  char *full_instance_name;
  char *full_instance_name_uscore;
  char *header_file_protection;
  int indent;
  guint n;

  ret = FALSE;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

  full_instance_name = g_strdup_printf ("%s%s", name_space, struct_data->name);
  full_instance_name_uscore = egg_dbus_utils_camel_case_to_uscore (full_instance_name);

  header_file_protection = g_strdup_printf ("__%s_%s_H",
                                            name_space_uscore_upper,
                                            struct_data->name_uscore_upper);

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

  /* instance / class macros */
  g_print ("#define %s_TYPE_%s          (%s_get_type())\n",
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           full_instance_name_uscore);
  g_print ("#define %s_%s(o)            (EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_CAST ((o), \"%s\", %s))\n",
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           struct_data->signature,
           full_instance_name);
  g_print ("#define %s_%s_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), %s_TYPE_%s, %sClass))\n",
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           full_instance_name);
  g_print ("#define %s_%s_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), %s_TYPE_%s, %sClass))\n",
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           full_instance_name);
  g_print ("#define %s_IS_%s(o)         (EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_TYPE ((o), \"%s\", %s))\n",
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           struct_data->signature,
           full_instance_name);
  g_print ("#define %s_IS_%s_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), %s_TYPE_%s))\n",
           name_space_uscore_upper,
           struct_data->name_uscore_upper,
           name_space_uscore_upper,
           struct_data->name_uscore_upper);
  g_print ("\n");

  g_print ("#if 0\n"
           "typedef struct _%s %s;\n"
           "#endif\n"
           "typedef struct _%sClass %sClass;\n"
           "\n",
           full_instance_name, full_instance_name,
           full_instance_name, full_instance_name);

  g_print ("struct _%s\n"
           "{\n"
           "  EggDBusStructure parent_instance;\n"
           "};\n"
           "\n",
           full_instance_name);

  g_print ("struct _%sClass\n"
           "{\n"
           "  EggDBusStructureClass parent_class;\n"
           "};\n"
           "\n",
           full_instance_name);

  /* GType function */
  g_print ("GType %s_get_type (void) G_GNUC_CONST;\n\n",
           full_instance_name_uscore);

  /* constructor */
  indent = (int) strlen (full_instance_name) + strlen (full_instance_name_uscore) - 1;
  g_print ("%s *%s_new (",
           full_instance_name,
           full_instance_name_uscore);

  for (n = 0; n < struct_data->num_elements; n++)
    {
      char *type_name;
      gchar *req_c_type_name;

      type_name = get_type_names_for_signature (struct_data->elements[n]->signature,
                                                struct_data->elements[n]->annotations,
                                                TRUE,
                                                TRUE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      g_print ("%s%s",
               req_c_type_name != NULL ? req_c_type_name : type_name,
               struct_data->elements[n]->name);
      if (n == struct_data->num_elements - 1)
        g_print (");\n\n");
      else
        g_print (", ");

      g_free (type_name);
      g_free (req_c_type_name);
    }

  /* prototypes for element getters */
  for (n = 0; n < struct_data->num_elements; n++)
    {
      char *type_name;
      gchar *req_c_type_name;

      type_name = get_type_names_for_signature (struct_data->elements[n]->signature,
                                                struct_data->elements[n]->annotations,
                                                FALSE,
                                                TRUE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      g_print ("%s %s_%s_get_%s (%s *instance);\n"
               "\n",
               req_c_type_name != NULL ? req_c_type_name : type_name,
               name_space_uscore,
               struct_data->name_uscore,
               struct_data->elements[n]->name,
               full_instance_name);

      g_free (type_name);
      g_free (req_c_type_name);
    }
  g_print ("\n");

  /* prototype for element setters */
  for (n = 0; n < struct_data->num_elements; n++)
    {
      char *type_name;
      gchar *req_c_type_name;

      type_name = get_type_names_for_signature (struct_data->elements[n]->signature,
                                                struct_data->elements[n]->annotations,
                                                TRUE,
                                                TRUE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      g_print ("void %s_%s_set_%s (%s *instance, %svalue);\n"
               "\n",
               name_space_uscore,
               struct_data->name_uscore,
               struct_data->elements[n]->name,
               full_instance_name,
               req_c_type_name != NULL ? req_c_type_name : type_name);

      g_free (type_name);
      g_free (req_c_type_name);
    }
  g_print ("\n");

  g_print ("G_END_DECLS\n");
  g_print ("\n");
  g_print ("#endif /* %s */\n", header_file_protection);

  ret = TRUE;

 out:

  g_free (header_file_protection);
  g_free (full_instance_name_uscore);
  g_free (full_instance_name);
  g_free (name_space_uscore);
  g_free (name_space_uscore_upper);

  return ret;
}

gboolean
struct_generate_c_file (StructData    *struct_data,
                        const char    *name_space,
                        const char    *output_name,
                        const char    *h_file_name,
                        const char    *class_name,
                        GError       **error)
{
  gboolean ret;
  char *name_space_uscore;
  char *name_space_uscore_upper;
  char *full_instance_name;
  char *full_instance_name_uscore;
  char *full_instance_name_uscore_upper;
  gchar *struct_summary_doc_string;
  gchar *struct_doc_string;
  guint n;
  gchar *file_name;

  ret = FALSE;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

  full_instance_name = g_strdup_printf ("%s%s", name_space, struct_data->name);
  full_instance_name_uscore = egg_dbus_utils_camel_case_to_uscore (full_instance_name);
  full_instance_name_uscore_upper = g_ascii_strup (full_instance_name_uscore, -1);

  struct_summary_doc_string = get_doc_summary (struct_data->annotations, DOC_TYPE_GTKDOC);
  struct_doc_string = get_doc (struct_data->annotations, DOC_TYPE_GTKDOC);

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

  file_name = compute_file_name (name_space, struct_data->name, "");
  g_print ("/**\n"
           " * SECTION:%s\n"
           " * @title: %s%s\n"
           " * @short_description: %s\n"
           " *\n"
           " * %s\n"
           " */\n"
           "\n",
           file_name,
           name_space, struct_data->name,
           struct_summary_doc_string,
           struct_doc_string);
  g_free (file_name);

  g_print ("G_DEFINE_TYPE (%s, %s, EGG_DBUS_TYPE_STRUCTURE);\n"
           "\n",
           full_instance_name,
           full_instance_name_uscore);

  g_print ("static void\n"
           "%s_init (%s *instance)\n"
           "{\n"
           "}\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name);

  g_print ("static void\n"
           "%s_class_init (%sClass *klass)\n"
           "{\n"
           "}\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name);

  /* implementation for element getters */
  for (n = 0; n < struct_data->num_elements; n++)
    {
      char *type_name;
      const char *free_function_name;
      gchar *req_c_type_name;
      gchar *doc_string;

      type_name = get_type_names_for_signature (struct_data->elements[n]->signature,
                                                struct_data->elements[n]->annotations,
                                                FALSE,
                                                TRUE,
                                                NULL,
                                                &free_function_name,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      doc_string = get_doc (struct_data->elements[n]->annotations, DOC_TYPE_GTKDOC);

      g_print ("/**\n"
               " * %s_%s_get_%s:\n"
               " * @instance: A #%s.\n"
               " *\n"
               " * Gets element %d of the #EggDBusStructure wrapped by #%s.\n"
               " *\n"
               " * Returns: %s\n"
               " */\n",
               name_space_uscore,
               struct_data->name_uscore,
               struct_data->elements[n]->name,
               full_instance_name,
               n,
               full_instance_name,
               doc_string);
      g_print ("%s\n"
               "%s_%s_get_%s (%s *instance)\n"
               "{\n",
               req_c_type_name != NULL ? req_c_type_name : type_name,
               name_space_uscore,
               struct_data->name_uscore,
               struct_data->elements[n]->name,
               full_instance_name);

      g_print ("  %svalue;\n"
               "\n",
               type_name);

      g_print ("  g_return_val_if_fail (%s_IS_%s (instance), %s);\n"
               "\n",
               name_space_uscore_upper,
               struct_data->name_uscore_upper,
               free_function_name != NULL ? "NULL" : "0");

      g_print ("  egg_dbus_structure_get_element (EGG_DBUS_STRUCTURE (instance),\n"
               "                %d, &value,\n"
               "                -1);\n"
               "\n",
               n);

      if (req_c_type_name == NULL)
        {
          g_print ("  return value;\n");
        }
      else
        {
          g_print ("  return (%s) value;\n",
                   req_c_type_name);
        }
      g_print ("}\n"
               "\n");


      g_free (type_name);
      g_free (req_c_type_name);
      g_free (doc_string);
    }
  g_print ("\n");

  for (n = 0; n < struct_data->num_elements; n++)
    {
      char *type_name;
      gchar *req_c_type_name;
      gchar *doc_string;

      type_name = get_type_names_for_signature (struct_data->elements[n]->signature,
                                                struct_data->elements[n]->annotations,
                                                TRUE,
                                                TRUE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      doc_string = get_doc (struct_data->elements[n]->annotations, DOC_TYPE_GTKDOC);

      g_print ("/**\n"
               " * %s_%s_set_%s:\n"
               " * @instance: A #%s.\n"
               " * @value: %s\n"
               " *\n"
               " * Sets element %d of the #EggDBusStructure wrapped by #%s.\n"
               " */\n",
               name_space_uscore,
               struct_data->name_uscore,
               struct_data->elements[n]->name,
               full_instance_name,
               doc_string,
               n,
               full_instance_name);
      g_print ("void\n"
               "%s_%s_set_%s (%s *instance, %svalue)\n"
               "{\n",
               name_space_uscore,
               struct_data->name_uscore,
               struct_data->elements[n]->name,
               full_instance_name,
               req_c_type_name != NULL ? req_c_type_name : type_name);

      g_print ("  g_return_if_fail (%s_IS_%s (instance));\n"
               "\n",
               name_space_uscore_upper,
               struct_data->name_uscore_upper);

      if (req_c_type_name == NULL)
        {
          g_print ("  egg_dbus_structure_set_element (EGG_DBUS_STRUCTURE (instance),\n"
                   "                %d, value,\n"
                   "                -1);\n"
                   "}\n"
                   "\n",
                   n);
        }
      else
        {
          g_print ("  egg_dbus_structure_set_element (EGG_DBUS_STRUCTURE (instance),\n"
                   "                %d, (%s) value,\n"
                   "                -1);\n"
                   "}\n"
                   "\n",
                   n,
                   type_name);
        }

      g_free (type_name);
      g_free (req_c_type_name);
      g_free (doc_string);
    }
  g_print ("\n");


  g_print ("/**\n"
           " * %s_%s_new:\n",
           name_space_uscore,
           struct_data->name_uscore);
  for (n = 0; n < struct_data->num_elements; n++)
    {
      gchar *doc_string;

      doc_string = get_doc (struct_data->elements[n]->annotations, DOC_TYPE_GTKDOC);

      g_print (" * @%s: %s\n",
               struct_data->elements[n]->name,
               doc_string);

      g_free (doc_string);
    }
  g_print (" *\n"
           " * Constructs a new #%s.\n"
           " *\n"
           " * Returns: A #%s.\n"
           " */\n",
           full_instance_name,
           full_instance_name);
  g_print ("%s *\n"
           "%s_new (",
           full_instance_name,
           full_instance_name_uscore);

  for (n = 0; n < struct_data->num_elements; n++)
    {
      char *type_name;
      gchar *req_c_type_name;

      type_name = get_type_names_for_signature (struct_data->elements[n]->signature,
                                                struct_data->elements[n]->annotations,
                                                TRUE,
                                                TRUE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      g_print ("%s%s",
               req_c_type_name != NULL ? req_c_type_name : type_name,
               struct_data->elements[n]->name);
      if (n == struct_data->num_elements - 1)
        g_print (")\n");
      else
        g_print (", ");

      g_free (type_name);
      g_free (req_c_type_name);
    }

  g_print ("{\n"
           "  GValue *values;\n"
           "  %s *structure;\n"
           "\n"
           "  values = g_new0 (GValue, %d);\n"
           "\n",
           full_instance_name,
           struct_data->num_elements);

  for (n = 0; n < struct_data->num_elements; n++)
    {
      char *type_name;
      gchar *gtype_name;
      const gchar *gvalue_set_func_name;
      gchar *req_c_type_name;

      //g_printerr ("sig = '%s'\n", struct_data->elements[n]->signature);
      type_name = get_type_names_for_signature (struct_data->elements[n]->signature,
                                                struct_data->elements[n]->annotations,
                                                TRUE,
                                                TRUE,
                                                &gtype_name,
                                                NULL,
                                                &gvalue_set_func_name,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      if (req_c_type_name == NULL)
        {
          g_print ("  g_value_init (&(values[%d]), %s);\n"
                   "  %s (&(values[%d]), %s);\n"
                   "\n",
                   n,
                   gtype_name,
                   gvalue_set_func_name,
                   n,
                   struct_data->elements[n]->name);
        }
      else
        {
          g_print ("  g_value_init (&(values[%d]), %s);\n"
                   "  %s (&(values[%d]), (%s) %s);\n"
                   "\n",
                   n,
                   gtype_name,
                   gvalue_set_func_name,
                   n,
                   type_name,
                   struct_data->elements[n]->name);
        }

      g_free (gtype_name);
      g_free (req_c_type_name);
    }

  g_print ("  structure = %s_%s (g_object_new (%s_TYPE_%s, \"signature\", \"%s\", \"elements\", values, NULL));\n"
           "\n"
           "  return structure;\n"
           "}\n"
           "\n",
           name_space_uscore_upper, struct_data->name_uscore_upper,
           name_space_uscore_upper, struct_data->name_uscore_upper,
           struct_data->signature);

  ret = TRUE;

 out:
  g_free (full_instance_name_uscore_upper);
  g_free (full_instance_name_uscore);
  g_free (full_instance_name);
  g_free (name_space_uscore);
  g_free (name_space_uscore_upper);
  g_free (struct_summary_doc_string);
  g_free (struct_doc_string);

  return ret;
}
