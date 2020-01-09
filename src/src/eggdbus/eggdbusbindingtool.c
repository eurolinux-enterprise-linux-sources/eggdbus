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
#include <eggdbus/eggdbuserror.h>
#include <eggdbus/eggdbusutils.h>
#include <eggdbus/eggdbusinterface.h>
#include <dbus/dbus.h>

#include "eggdbusprivate.h"

#include "eggdbusbindingtool.h"
#include "interface.h"
#include "struct.h"
#include "enum.h"
#include "docbook.h"
#include "completetype.h"

static char **opt_xml = NULL;
static gboolean opt_iface_only = FALSE;
static gboolean opt_no_types_header = FALSE;
static char **opt_extra_header_files = NULL;

static char *dbus_name_space;
static char *name_space;

static char *opt_stamp_file;

static GOptionEntry  entries []     = {
  { "dbus-namespace", 'd', 0, G_OPTION_ARG_STRING, &dbus_name_space, "Namespace to strip from D-Bus interface names", NULL},
  { "namespace", 'n', 0, G_OPTION_ARG_STRING, &name_space, "Namespace for generated code", NULL},
  { "introspection-xml", 'x', 0, G_OPTION_ARG_FILENAME_ARRAY, &opt_xml, "Introspection XML files", NULL},
  { "interfaces-only", 'i', 0, G_OPTION_ARG_NONE, &opt_iface_only, "Only generate interfaces and marshallers", NULL},
  { "no-types-header", 't', 0, G_OPTION_ARG_NONE, &opt_no_types_header, "Don't generate types header file", NULL},
  { "include-header", 'h', 0, G_OPTION_ARG_STRING_ARRAY, &opt_extra_header_files, "Extra header files to includes", NULL},
  { "stamp-file", 's', 0, G_OPTION_ARG_STRING, &opt_stamp_file, "Generate stamp file with generated files", NULL},
  { NULL }
};

/* ---------------------------------------------------------------------------------------------------- */

/* per invocation data structures */

/* List of the names of generated files */
static GSList *generated_files;

/* hash from GType name used (e.g. TestTweak) to a const EggDBusInterfaceInfo pointer */
static GHashTable *all_dbus_interfaces;

/* a list of StructData for the structure GInterfaces we generate */
static GSList *struct_data_list;

/* a list of EnumData objects for enumerations (e.g. error domains, flags, enums) */
static GSList *enum_data_list;

/* List of marshallers to generate; consecutive pairs in the list is of the form
 * "INT16,UINT,OBJECT,BOXED", "<namespace>_cclosure_marshal_VOID_INT16_UINT_OBJECT_BOXED"
 */
static GSList *marshallers_to_generate = NULL;

/* ---------------------------------------------------------------------------------------------------- */

EnumData *
find_enum_by_name (const gchar *name)
{
  GSList *l;
  EnumData *ret;

  ret = NULL;
  for (l = enum_data_list; l != NULL; l = l->next)
    {
      EnumData *s = (EnumData *) l->data;

      if (strcmp (s->name, name) == 0)
        {
          ret = s;
          goto out;
        }
    }

 out:
  return ret;
}

StructData *
find_struct_by_name (const gchar *name)
{
  GSList *l;
  StructData *ret;

  ret = NULL;
  for (l = struct_data_list; l != NULL; l = l->next)
    {
      StructData *s = (StructData *) l->data;

      if (strcmp (s->name, name) == 0)
        {
          ret = s;
          goto out;
        }
    }

 out:
  return ret;
}

static GSList *
find_struct_by_signature (const gchar *signature)
{
  GSList *ret;
  GSList *l;

  ret = NULL;

  for (l = struct_data_list; l != NULL; l = l->next)
    {
      StructData *s = (StructData *) l->data;

      if (strcmp (s->signature, signature) == 0)
        ret = g_slist_prepend (ret, s);
    }

  return ret;
}


/* ---------------------------------------------------------------------------------------------------- */

GSList *
get_enums_declared_in_interface (const EggDBusInterfaceInfo *interface)
{
  GSList *ret;
  GSList *l;

  ret = NULL;

  for (l = enum_data_list; l != NULL; l = l->next)
    {
      EnumData *data = l->data;

      if (data->interface == interface)
        ret = g_slist_append (ret, data);
    }

  return ret;
}

GSList *
get_structs_declared_in_interface (const EggDBusInterfaceInfo *interface)
{
  GSList *ret;
  GSList *l;

  ret = NULL;

  for (l = struct_data_list; l != NULL; l = l->next)
    {
      StructData *data = l->data;

      if (data->interface == interface)
        ret = g_slist_append (ret, data);
    }

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static GRegex *link_regex = NULL;
static GHashTable *gtkdoc_links = NULL;
static GHashTable *docbook_links = NULL;

static gboolean
create_link_regexs (GError **error)
{
  gboolean ret;
  GSList *l;
  guint n;
  gchar *id;
  gchar *gtkdoc_val;
  gchar *docbook_val;
  GHashTableIter hash_iter;
  const gchar *iface_name;
  EggDBusInterfaceInfo *interface;
  char *name_space_uscore;
  char *name_space_uscore_upper;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

  ret = FALSE;

  link_regex = g_regex_new ("("
                             "([\\w.]+)\\s*\\(\\)"       /* method_call() */
                            ")|("
                             "\\@(\\w+((\\.|->)\\w+)*)"  /* @param */
                            ")|("
                             "\\%(-?[\\w.]+)"            /* %constant */
                            ")|("
                             "#([\\w\\.-:]+)"            /* #symbol */
                            ")",
                            G_REGEX_OPTIMIZE,
                            0,
                            error);
  if (link_regex == NULL)
    goto out;

  gtkdoc_links = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        g_free,
                                        g_free);

  docbook_links = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         NULL, //g_free,
                                         g_free);

  /* first, some widely-used constants */
  id = g_strdup ("%TRUE");
  gtkdoc_val = g_strdup ("%TRUE");
  docbook_val = g_strdup ("<literal>TRUE</literal>");
  g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
  g_hash_table_insert (docbook_links, id, docbook_val);
  id = g_strdup ("%TRUE.");
  gtkdoc_val = g_strdup ("%TRUE.");
  docbook_val = g_strdup ("<literal>TRUE</literal>.");
  g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
  g_hash_table_insert (docbook_links, id, docbook_val);

  id = g_strdup ("%FALSE");
  gtkdoc_val = g_strdup ("%FALSE");
  docbook_val = g_strdup ("<literal>FALSE</literal>");
  g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
  g_hash_table_insert (docbook_links, id, docbook_val);
  id = g_strdup ("%FALSE.");
  gtkdoc_val = g_strdup ("%FALSE.");
  docbook_val = g_strdup ("<literal>FALSE</literal>.");
  g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
  g_hash_table_insert (docbook_links, id, docbook_val);

  /* add links for all enums */
  for (l = enum_data_list; l != NULL; l = l->next)
    {
      EnumData *enum_data = l->data;

      /* make links to the enumeration itself */
      if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
        {
          id = g_strdup_printf ("#%s", enum_data->maximal_dbus_prefix);
          id[strlen(id) - 1] = '\0';
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-errordomain-%s\">%s* Error Domain</link>",
                                         enum_data->maximal_dbus_prefix,
                                         enum_data->maximal_dbus_prefix);
        }
      else
        {
          id = g_strdup_printf ("#%s", enum_data->name);
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-enum-%s\">%s</link>.",
                                         enum_data->name,
                                         enum_data->name);
        }
      gtkdoc_val = g_strdup_printf ("#%s%s", name_space, enum_data->name);
      g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
      g_hash_table_insert (docbook_links, id, docbook_val);

      if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
        {
          id = g_strdup_printf ("#%s.", enum_data->maximal_dbus_prefix);
          id[strlen(id) - 1] = '\0';
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-errordomain-%s\">%s* Error Domain</link>.",
                                         enum_data->maximal_dbus_prefix,
                                         enum_data->maximal_dbus_prefix);
        }
      else
        {
          id = g_strdup_printf ("#%s.", enum_data->name);
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-enum-%s\">%s</link>.",
                                         enum_data->name,
                                         enum_data->name);
        }
      gtkdoc_val = g_strdup_printf ("#%s%s.", name_space, enum_data->name);
      g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
      g_hash_table_insert (docbook_links, id, docbook_val);

      for (n = 0; n < enum_data->num_elements; n++)
        {
          EnumElemData *elem = enum_data->elements[n];

          if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
            id = g_strdup_printf ("%%%s", elem->name);
          else
            id = g_strdup_printf ("%%%s.%s", enum_data->name, elem->name);
          gtkdoc_val = g_strdup_printf ("%%%s_%s_%s",
                                        name_space_uscore_upper,
                                        enum_data->name_uscore_upper,
                                        elem->g_name_uscore_upper);
          if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
            docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-constant-%s.%s\">%s</link>",
                                           enum_data->name, elem->name,
                                           elem->name);
          else
            docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-constant-%s.%s\">%s.%s</link>",
                                           enum_data->name, elem->name,
                                           enum_data->name, elem->name);
          g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
          g_hash_table_insert (docbook_links, id, docbook_val);

          if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
            id = g_strdup_printf ("%%%s.", elem->name);
          else
            id = g_strdup_printf ("%%%s.%s.", enum_data->name, elem->name);
          gtkdoc_val = g_strdup_printf ("%%%s_%s_%s.",
                                        name_space_uscore_upper,
                                        enum_data->name_uscore_upper,
                                        elem->g_name_uscore_upper);
          if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
            docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-constant-%s.%s\">%s</link>.",
                                           enum_data->name, elem->name,
                                           elem->name);
          else
            docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-constant-%s.%s\">%s.%s</link>.",
                                           enum_data->name, elem->name,
                                           enum_data->name, elem->name);
          g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
          g_hash_table_insert (docbook_links, id, docbook_val);
        }
    }

  /* add links for all structs */
  for (l = struct_data_list; l != NULL; l = l->next)
    {
      StructData *struct_data = l->data;

      /* only make links for the struct itself, not any of it's members (TODO: should we?) */
      id = g_strdup_printf ("#%s", struct_data->name);
      gtkdoc_val = g_strdup_printf ("#%s%s", name_space, struct_data->name);
      docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-struct-%s\">%s</link>",
                                     struct_data->name,
                                     struct_data->name);
      g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
      g_hash_table_insert (docbook_links, id, docbook_val);

      id = g_strdup_printf ("#%s.", struct_data->name);
      gtkdoc_val = g_strdup_printf ("#%s%s.", name_space, struct_data->name);
      docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-struct-%s\">%s</link>.",
                                     struct_data->name,
                                     struct_data->name);
      g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
      g_hash_table_insert (docbook_links, id, docbook_val);

    }

  /* add links for all interfaces (and methods, signals, properties) */
  g_hash_table_iter_init (&hash_iter, all_dbus_interfaces);
  while (g_hash_table_iter_next (&hash_iter, (gpointer) &iface_name, (gpointer) &interface))
    {
      gchar *iface_uscore;

      iface_uscore = egg_dbus_utils_camel_case_to_uscore (iface_name);

      /* make links to the interface itself */
      id = g_strdup_printf ("#%s", interface->name);
      gtkdoc_val = g_strdup_printf ("#%s%s", name_space, iface_name);
      docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-interface-%s\">%s</link>",
                                     interface->name,
                                     interface->name);
      g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
      g_hash_table_insert (docbook_links, id, docbook_val);

      id = g_strdup_printf ("#%s.", interface->name);
      gtkdoc_val = g_strdup_printf ("#%s%s.", name_space, iface_name);
      docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-interface-%s\">%s</link>.",
                                     interface->name,
                                     interface->name);
      g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
      g_hash_table_insert (docbook_links, id, docbook_val);

      /* foreach method */
      for (n = 0; n < interface->num_methods; n++)
        {
          const EggDBusInterfaceMethodInfo *method = interface->methods + n;
          gchar *method_uscore;

          method_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

          id = g_strdup_printf ("%s.%s()", interface->name, method->name);
          gtkdoc_val = g_strdup_printf ("%s_%s_%s()",
                                        name_space_uscore,
                                        iface_uscore,
                                        method_uscore);
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-method-%s.%s\">%s()</link>",
                                         interface->name, method->name,
                                         method->name);
          g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
          g_hash_table_insert (docbook_links, id, docbook_val);

          g_free (method_uscore);
        }

      /* foreach signal */
      for (n = 0; n < interface->num_signals; n++)
        {
          const EggDBusInterfaceSignalInfo *signal = interface->signals + n;

          id = g_strdup_printf ("#%s::%s", interface->name, signal->name);
          gtkdoc_val = g_strdup_printf ("#%s%s::%s", name_space, iface_name, signal->g_name);
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-signal-%s::%s\"><type>\"%s\"</type></link>",
                                         interface->name, signal->name,
                                         signal->name);
          g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
          g_hash_table_insert (docbook_links, id, docbook_val);

          id = g_strdup_printf ("#%s::%s.", interface->name, signal->name);
          gtkdoc_val = g_strdup_printf ("#%s%s::%s.", name_space, iface_name, signal->g_name);
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-signal-%s::%s\"><type>\"%s\"</type></link>.",
                                         interface->name, signal->name,
                                         signal->name);
          g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
          g_hash_table_insert (docbook_links, id, docbook_val);
        }

      /* foreach property */
      for (n = 0; n < interface->num_properties; n++)
        {
          const EggDBusInterfacePropertyInfo *property = interface->properties + n;

          id = g_strdup_printf ("#%s:%s", interface->name, property->name);
          gtkdoc_val = g_strdup_printf ("#%s%s:%s", name_space, iface_name, property->g_name);
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-property-%s:%s\"><type>\"%s\"</type></link>",
                                         interface->name, property->name,
                                         property->name);
          g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
          g_hash_table_insert (docbook_links, id, docbook_val);

          id = g_strdup_printf ("#%s:%s.", interface->name, property->name);
          gtkdoc_val = g_strdup_printf ("#%s%s:%s.", name_space, iface_name, property->g_name);
          docbook_val = g_strdup_printf ("<link linkend=\"eggdbus-property-%s:%s\"><type>\"%s\"</type></link>.",
                                         interface->name, property->name,
                                         property->name);
          g_hash_table_insert (gtkdoc_links, id, gtkdoc_val);
          g_hash_table_insert (docbook_links, id, docbook_val);
        }

      g_free (iface_uscore);
    }

  ret = TRUE;

 out:
  g_free (name_space_uscore);
  g_free (name_space_uscore_upper);
  return ret;
}

static void
free_link_regexs (void)
{
  if (gtkdoc_links != NULL)
    g_hash_table_unref (gtkdoc_links);

  if (docbook_links != NULL)
    g_hash_table_unref (docbook_links);

  if (link_regex != NULL)
    g_regex_unref (link_regex);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
doc_replace_cb (const GMatchInfo *match_info,
                GString          *result,
                gpointer          user_data)
{
  DocType type;
  gchar *match;
  gchar *val;

  type = GPOINTER_TO_INT (user_data);

  match = g_match_info_fetch (match_info, 0);

  val = NULL;
  switch (type)
    {
    case DOC_TYPE_GTKDOC:
      if (match[0] == '@')
        val = g_strdup (match);
      else
        val = g_strdup (g_hash_table_lookup (gtkdoc_links, match));
      break;

    case DOC_TYPE_DOCBOOK:
      if (match[0] == '@')
        val = g_strdup_printf ("<parameter>%s</parameter>", match + 1);
      else
        val = g_strdup (g_hash_table_lookup (docbook_links, match));
      break;
    }

  if (val != NULL)
    {
      g_string_append (result, val);
      g_free (val);
    }
  else
    {
      g_string_append (result, match);
      g_printerr ("Warning: unable to resolve '%s'\n", match);
    }

  g_free (match);

  return FALSE;
}

static gchar *
get_doc_from_key (const EggDBusInterfaceAnnotationInfo *annotations,
                  const gchar                          *key,
                  DocType                               type)
{
  const gchar *s;
  gchar *doc_string;
  GError *error;

  s = egg_dbus_interface_annotation_info_lookup (annotations, key);

  if (s == NULL)
    {
      doc_string = g_strdup_printf ("FIXME: not documented.");
      goto out;
    }

  if (link_regex == NULL)
    {
      doc_string = g_strdup (s);
      goto out;
    }

  error = NULL;
  doc_string = g_regex_replace_eval (link_regex,
                                     s,
                                     -1,
                                     0,
                                     0, /* GRegexMatchFlags */
                                     doc_replace_cb,
                                     GINT_TO_POINTER (type),
                                     &error);
  if (doc_string == NULL)
    {
      g_warning ("Error replacing links: %s", error->message);
      g_error_free (error);
      s = g_strdup (s);
    }

 out:
  return doc_string;
}

gchar *
get_doc (const EggDBusInterfaceAnnotationInfo *annotations,
         DocType                               type)
{
  return get_doc_from_key (annotations, "org.gtk.EggDBus.DocString", type);
}

gchar *
get_doc_summary (const EggDBusInterfaceAnnotationInfo *annotations,
                 DocType                               type)
{
  return get_doc_from_key (annotations, "org.gtk.EggDBus.DocString.Summary", type);
}

/* ---------------------------------------------------------------------------------------------------- */

void
print_includes (const gchar *name_space, gboolean is_c_file)
{
  guint n;

  if (!opt_no_types_header)
    print_include (name_space, "BindingsTypes");

  if (is_c_file && !opt_no_types_header)
    print_include (name_space, "Bindings");

  for (n = 0; opt_extra_header_files != NULL && opt_extra_header_files[n] != NULL; n++)
    g_print ("#include <%s>\n", opt_extra_header_files[n]);
}

void
print_include (const gchar *name_space, const gchar *class_name)
{
  gchar *file_name;
  file_name = compute_file_name (name_space, class_name, ".h");
  g_print ("#include \"%s\"\n", file_name);
  g_free (file_name);
}

gchar*
compute_file_name (const gchar *name_space, const gchar *class_name, const gchar *suffix)
{
  gchar *name_space_lower;
  gchar *class_name_lower;
  gchar *ret;

  name_space_lower = g_ascii_strdown (name_space, -1);
  class_name_lower = g_ascii_strdown (class_name, -1);
  ret = g_strdup_printf ("%s%s%s",
                         name_space_lower,
                         class_name_lower,
                         suffix);
  g_free (name_space_lower);
  g_free (class_name_lower);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

const char *
get_c_marshaller_name_for_args (const EggDBusInterfaceArgInfo *args, guint num_args)
{
  guint n;
  char *name_space_uscore;
  GString *s;
  GString *m;
  char *marshaller_name;
  char *marshaller_string;
  gboolean do_generate;
  GSList *l;

  /* special case the empty signal */
  if (num_args == 0)
    return "g_cclosure_marshal_VOID__VOID";

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);

  s = g_string_new (NULL);
  m = g_string_new ("VOID:");
  g_string_append_printf (s, "_%s_cclosure_marshal_VOID_", name_space_uscore);
  for (n = 0; n < num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = args + n;
      const char *type_name;
      CompleteType *type;

      type = get_complete_type_for_arg (arg);

      switch (arg->signature[0])
        {
        case DBUS_TYPE_BYTE:
          type_name = "UCHAR";
          break;

        case DBUS_TYPE_BOOLEAN:
          type_name = "BOOLEAN";
          break;

        case DBUS_TYPE_INT16:
          type_name = "INT16";
          break;

        case DBUS_TYPE_UINT16:
          type_name = "UINT16";
          break;

        case DBUS_TYPE_INT32:
          type_name = "INT";
          break;

        case DBUS_TYPE_UINT32:
          if (type->user_type != NULL)
            {
              EnumData *enum_data;
              enum_data = find_enum_by_name (type->user_type);
              if (enum_data != NULL && enum_data->type == ENUM_DATA_TYPE_ENUM)
                type_name = "ENUM";
              else if (enum_data != NULL && enum_data->type == ENUM_DATA_TYPE_FLAGS)
                type_name = "FLAGS";
              else
                type_name = "UINT";
            }
          else
            {
              type_name = "UINT";
            }
          break;

        case DBUS_TYPE_INT64:
          type_name = "INT64";
          break;

        case DBUS_TYPE_UINT64:
          type_name = "UINT64";
          break;

        case DBUS_TYPE_DOUBLE:
          type_name = "DOUBLE";
          break;

        case DBUS_TYPE_STRING:
          type_name = "STRING";
          break;

        case DBUS_TYPE_OBJECT_PATH:
          type_name = "BOXED";
          break;

        case DBUS_TYPE_SIGNATURE:
          type_name = "BOXED";
          break;

        case DBUS_TYPE_ARRAY:
          switch (arg->signature[1])
            {
            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
            case DBUS_TYPE_SIGNATURE:
              type_name = "BOXED";
              break;

            default:
              type_name = "OBJECT";
              break;
            }
          break;

        case DBUS_STRUCT_BEGIN_CHAR:
          type_name = "OBJECT";
          break;

        case DBUS_TYPE_VARIANT:
          type_name = "OBJECT";
          break;

        default:
          g_warning ("Cannot generate C marshaler for signature %s", arg->signature);
          g_assert_not_reached ();
        }

      g_string_append_c (s, '_');
      g_string_append (s, type_name);

      g_string_append (m, type_name);
      if (n != num_args - 1)
        g_string_append_c (m, ',');
    }
  marshaller_name = g_string_free (s, FALSE);
  marshaller_string = g_string_free (m, FALSE);

  do_generate = TRUE;

  /* see if we have this marshaller already */
  for (l = marshallers_to_generate; l != NULL; l = l->next)
    {
      char *ms = l->data;

      g_assert (l->next != NULL);

      if (strcmp (ms, marshaller_string) == 0)
        {
          /* yup, got it already, use this one */
          g_free (marshaller_string);
          g_free (marshaller_name);
          marshaller_string = ms;
          marshaller_name = l->next->data;
          goto out;
        }

      l = l->next; /* skip function name */
    }

  marshallers_to_generate = g_slist_append (marshallers_to_generate, marshaller_string);
  marshallers_to_generate = g_slist_append (marshallers_to_generate, marshaller_name);

 out:

  g_free (name_space_uscore);

  return marshaller_name;
}

/* ---------------------------------------------------------------------------------------------------- */

char *
get_type_names_for_signature (const gchar *signature,
                              const EggDBusInterfaceAnnotationInfo *annotations,
                              gboolean is_in,
                              gboolean want_const,
                              char **out_gtype_name,
                              const char **out_free_function_name,
                              const char **out_gvalue_set_func_name,
                              char **out_required_c_type,
                              GError **error)
{
  char *ret;
  const char *free_function_name;
  const char *gvalue_set_func_name;
  const gchar *type;
  gchar *required_c_type;
  char *gtype_name;
  EnumData *enum_data;

  ret = NULL;
  gtype_name = NULL;
  free_function_name = NULL;
  gvalue_set_func_name = NULL;

  type = egg_dbus_interface_annotation_info_lookup (annotations, "org.gtk.EggDBus.Type");

  required_c_type = g_strdup (egg_dbus_interface_annotation_info_lookup (annotations, "org.gtk.EggDBus.CType"));

  if (type != NULL)
    {
      enum_data = find_enum_by_name (type);

      if (enum_data != NULL)
        {
          gchar *name_space_uscore;
          gchar *name_space_uscore_upper;

          name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
          name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

          g_free (required_c_type);
          required_c_type = g_strdup_printf ("%s%s", name_space, type);
          /* TODO: set the correct gtype etc. */

          /* enums only work on uint32 */
          if (signature[0] != DBUS_TYPE_UINT32)
            {
              g_set_error (error,
                           EGG_DBUS_ERROR,
                           EGG_DBUS_ERROR_FAILED,
                           "You can only use enumerations on uint32 types.");
              goto out;
            }

          ret = g_strdup_printf ("%s%s ", name_space, enum_data->name);
          if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
            gvalue_set_func_name = "g_value_set_flags";
          else if (enum_data->type == ENUM_DATA_TYPE_ENUM)
            gvalue_set_func_name = "g_value_set_enum";
          gtype_name = g_strdup_printf ("%s_TYPE_%s", name_space_uscore_upper, enum_data->name_uscore_upper);

          g_free (name_space_uscore);
          g_free (name_space_uscore_upper);
          goto done;
        }
    }

  /* .CType only works on integral types */
  if (required_c_type != NULL)
    {
      switch (signature[0])
        {
        case DBUS_TYPE_BYTE:
        case DBUS_TYPE_INT16:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_INT64:
        case DBUS_TYPE_UINT64:
          break;

        default:
          g_set_error_literal (error,
                               EGG_DBUS_ERROR,
                               EGG_DBUS_ERROR_FAILED,
                               "org.gtk.EggDBus.CType only works on integral types.");
          goto out;
        }
    }

  if (signature[0] == DBUS_TYPE_STRING)
    {
      if (want_const)
        ret = g_strdup ("const gchar *");
      else
        ret = g_strdup ("gchar *");
      free_function_name = "g_free";
      gvalue_set_func_name = "g_value_set_string";
      gtype_name = g_strdup ("G_TYPE_STRING");
    }
  else if (signature[0] == DBUS_TYPE_OBJECT_PATH)
    {
      if (want_const)
        ret = g_strdup ("const gchar *");
      else
        ret = g_strdup ("gchar *");
      free_function_name = "g_free";
      gvalue_set_func_name = "g_value_set_boxed";
      gtype_name = g_strdup ("EGG_DBUS_TYPE_OBJECT_PATH");
    }
  else if (signature[0] == DBUS_TYPE_SIGNATURE)
    {
      if (want_const)
        ret = g_strdup ("const gchar *");
      else
        ret = g_strdup ("gchar *");
      free_function_name = "g_free";
      gvalue_set_func_name = "g_value_set_boxed";
      gtype_name = g_strdup ("EGG_DBUS_TYPE_SIGNATURE");
    }
  else if (signature[0] == DBUS_TYPE_BYTE)
    {
      ret = g_strdup ("guint8 ");
      gvalue_set_func_name = "g_value_set_uchar";
      gtype_name = g_strdup ("G_TYPE_UCHAR");
    }
  else if (signature[0] == DBUS_TYPE_INT16)
    {
      ret = g_strdup ("gint16 ");
      gvalue_set_func_name = "egg_dbus_value_set_int16";
      gtype_name = g_strdup ("EGG_DBUS_TYPE_INT16");
    }
  else if (signature[0] == DBUS_TYPE_UINT16)
    {
      ret = g_strdup ("guint16 ");
      gvalue_set_func_name = "egg_dbus_value_set_uint16";
      gtype_name = g_strdup ("EGG_DBUS_TYPE_UINT16");
    }
  else if (signature[0] == DBUS_TYPE_INT32)
    {
      ret = g_strdup ("gint ");
      gvalue_set_func_name = "g_value_set_int";
      gtype_name = g_strdup ("G_TYPE_INT");
    }
  else if (signature[0] == DBUS_TYPE_UINT32)
    {
      ret = g_strdup ("guint ");
      gvalue_set_func_name = "g_value_set_uint";
      gtype_name = g_strdup ("G_TYPE_UINT");
    }
  else if (signature[0] == DBUS_TYPE_INT64)
    {
      ret = g_strdup ("gint64 ");
      gvalue_set_func_name = "g_value_set_int64";
      gtype_name = g_strdup ("G_TYPE_INT64");
    }
  else if (signature[0] == DBUS_TYPE_UINT64)
    {
      ret = g_strdup ("guint64 ");
      gvalue_set_func_name = "g_value_set_uint64";
      gtype_name = g_strdup ("G_TYPE_UINT64");
    }
  else if (signature[0] == DBUS_TYPE_DOUBLE)
    {
      ret = g_strdup ("double ");
      gvalue_set_func_name = "g_value_set_double";
      gtype_name = g_strdup ("G_TYPE_DOUBLE");
    }
  else if (signature[0] == DBUS_TYPE_BOOLEAN)
    {
      ret = g_strdup ("gboolean ");
      gvalue_set_func_name = "g_value_set_boolean";
      gtype_name = g_strdup ("G_TYPE_BOOLEAN");
    }
  else if (signature[0] == DBUS_STRUCT_BEGIN_CHAR)
    {
      StructData *struct_data;
      char *name_space_uscore;
      char *name_space_uscore_upper;

      /* TODO: support anonymous structs */
      if (type == NULL)
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "No org.gtk.EggDBus.Type annotation.");
          goto out;
        }

      struct_data = find_struct_by_name (type);
      if (struct_data == NULL)
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "No struct with name %s.",
                       type);
          goto out;
        }

      name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
      name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

      ret = g_strdup_printf ("%s%s *", name_space, struct_data->name);
      gtype_name = g_strdup_printf ("%s_TYPE_%s", name_space_uscore_upper, struct_data->name_uscore_upper);
      free_function_name = "g_object_unref";
      gvalue_set_func_name = "g_value_set_object";

      g_free (name_space_uscore);
      g_free (name_space_uscore_upper);
    }
  else if (signature[0] == DBUS_TYPE_ARRAY)
    {
      if (signature[1] == DBUS_STRUCT_BEGIN_CHAR)
        {
          if (is_in)
            ret = g_strdup ("EggDBusArraySeq *");
          else
            ret = g_strdup ("EggDBusArraySeq *");
          free_function_name = "g_object_unref";
          gvalue_set_func_name = "g_value_set_object";
          gtype_name = g_strdup ("EGG_DBUS_TYPE_ARRAY_SEQ");
        }
      else if (signature[1] == DBUS_TYPE_STRING)
        {
          ret = g_strdup ("gchar **");
          free_function_name = "g_strfreev";
          gvalue_set_func_name = "g_value_set_boxed";
          gtype_name = g_strdup ("G_TYPE_STRV");
        }
      else if (signature[1] == DBUS_TYPE_OBJECT_PATH)
        {
          ret = g_strdup ("gchar **");
          free_function_name = "g_strfreev";
          gvalue_set_func_name = "g_value_set_boxed";
          gtype_name = g_strdup ("EGG_DBUS_TYPE_OBJECT_PATH_ARRAY");
        }
      else if (signature[1] == DBUS_TYPE_SIGNATURE)
        {
          ret = g_strdup ("gchar **");
          free_function_name = "g_strfreev";
          gvalue_set_func_name = "g_value_set_boxed";
          gtype_name = g_strdup ("EGG_DBUS_TYPE_SIGNATURE_ARRAY");
        }
      else if (signature[1] == DBUS_TYPE_BYTE   ||
               signature[1] == DBUS_TYPE_INT16  ||
               signature[1] == DBUS_TYPE_UINT16 ||
               signature[1] == DBUS_TYPE_INT32  ||
               signature[1] == DBUS_TYPE_UINT32 ||
               signature[1] == DBUS_TYPE_INT64  ||
               signature[1] == DBUS_TYPE_UINT64 ||
               signature[1] == DBUS_TYPE_DOUBLE ||
               signature[1] == DBUS_TYPE_BOOLEAN)
        {
          if (is_in)
            ret = g_strdup ("EggDBusArraySeq *");
          else
            ret = g_strdup ("EggDBusArraySeq *");
          free_function_name = "g_object_unref";
          gvalue_set_func_name = "g_value_set_object";
          gtype_name = g_strdup ("EGG_DBUS_TYPE_ARRAY_SEQ");
        }
      else if (signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
        {
          if (is_in)
            ret = g_strdup ("EggDBusHashMap *");
          else
            ret = g_strdup ("EggDBusHashMap *");
          free_function_name = "g_object_unref";
          gvalue_set_func_name = "g_value_set_object";
          gtype_name = g_strdup ("EGG_DBUS_TYPE_HASH_MAP");
        }
      else if (signature[1] == DBUS_TYPE_ARRAY)
        {
          if (is_in)
            ret = g_strdup ("EggDBusArraySeq *");
          else
            ret = g_strdup ("EggDBusArraySeq *");
          free_function_name = "g_object_unref";
          gvalue_set_func_name = "g_value_set_object";
          gtype_name = g_strdup ("EGG_DBUS_TYPE_ARRAY_SEQ");
        }
      else if (signature[1] == DBUS_TYPE_VARIANT)
        {
          if (is_in)
            ret = g_strdup ("EggDBusArraySeq *");
          else
            ret = g_strdup ("EggDBusArraySeq *");
          free_function_name = "g_object_unref";
          gvalue_set_func_name = "g_value_set_object";
          gtype_name = g_strdup ("EGG_DBUS_TYPE_ARRAY_SEQ");
        }
      else
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "Cannot figure out C type name for signature '%s'. Please add support.",
                       signature);
          goto out;
        }
    }
  else if (signature[0] == DBUS_TYPE_VARIANT)
    {
      ret = g_strdup ("EggDBusVariant *");
      free_function_name = "g_object_unref";
      gvalue_set_func_name = "g_value_set_object";
      gtype_name = g_strdup ("EGG_DBUS_TYPE_VARIANT");
    }
  else
    {
      g_set_error (error,
                   EGG_DBUS_ERROR,
                   EGG_DBUS_ERROR_FAILED,
                   "Cannot figure out C type name for signature '%s'. Please add support.",
                   signature);
      goto out;
    }

 done:
  if (out_free_function_name != NULL)
    *out_free_function_name = free_function_name;

  if (out_gvalue_set_func_name != NULL)
    *out_gvalue_set_func_name = gvalue_set_func_name;

  if (gtype_name != NULL && out_gtype_name != NULL)
    *out_gtype_name = gtype_name;
  else
    g_free (gtype_name);

  if (out_required_c_type != NULL)
    *out_required_c_type = (required_c_type != NULL ? g_strdup_printf ("%s ", required_c_type) : NULL);

 out:
  g_free (required_c_type);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static char *file_print_name = NULL;
static GPrintFunc old_print_func;
static GString *print_str;

static void
my_print_func (const char *str)
{
  g_string_append (print_str, str);
}

static void
file_print_func_begin (const char *output_name)
{
  file_print_name = g_strdup (output_name);
  print_str = g_string_new (NULL);
  old_print_func = g_set_print_handler (my_print_func);
}

static gboolean
file_print_func_end (gboolean   success,
                     GError   **error)
{
  char *str;
  gboolean ret;

  ret = FALSE;

  g_set_print_handler (old_print_func);

  str = g_string_free (print_str, FALSE);
  if (success)
    {
      ret = g_file_set_contents (file_print_name, str, -1, error);
    }
  else
    {
      ret = TRUE;
    }

  g_free (str);

  g_free (file_print_name);
  file_print_name = NULL;

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
generate_main_files (GError **error)
{
  gboolean ret;
  char *c_file_name;
  char *h_file_name;
  char *name_space_uscore;
  char *name_space_uscore_upper;
  char *name_space_hyphen;
  char *header_file_protection;
  GSList *l;
  GHashTableIter hash_iter;
  const gchar *iface_name;
  const EggDBusInterfaceInfo *interface;
  guint n;
  guint num_error_domains;

  ret = FALSE;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);
  name_space_hyphen = egg_dbus_utils_camel_case_to_hyphen (name_space);

  if (!opt_no_types_header)
    {
      /* generate the <namespace>-bindings-types.h file */
      h_file_name = compute_file_name (name_space, "BindingsTypes", ".h");
      file_print_func_begin (h_file_name);

      g_print ("\n"
               "/* File: %s\n"
               " *\n"
               " * Generated by eggdbus-binding-tool %s. Do not edit.\n"
               " */\n"
               "\n",
               h_file_name,
               PACKAGE_VERSION);

      header_file_protection = g_strdup_printf ("__%s_BINDINGS_TYPES_H",
                                                name_space_uscore_upper);
      g_print ("#ifndef %s\n", header_file_protection);
      g_print ("#define %s\n", header_file_protection);
      g_print ("\n");
      g_print ("#include <glib-object.h>\n");
      g_print ("#include <gio/gio.h>\n");
      g_print ("#include <eggdbus/eggdbus.h>\n");

      for (n = 0; opt_extra_header_files != NULL && opt_extra_header_files[n] != NULL; n++)
        g_print ("#include <%s>\n", opt_extra_header_files[n]);

      for (l = enum_data_list; l != NULL; l = l->next)
        {
          EnumData *enum_data = (EnumData *) l->data;
          print_include (name_space, enum_data->name);
        }

      g_print ("\n");
      g_print ("G_BEGIN_DECLS\n");
      g_print ("\n");

      g_hash_table_iter_init (&hash_iter, all_dbus_interfaces);
      while (g_hash_table_iter_next (&hash_iter,
                                     (gpointer) &iface_name,
                                     (gpointer) &interface))
        {
          g_print ("typedef struct _%s%s %s%s; /* Dummy typedef */\n"
                   "\n",
                   name_space, iface_name,
                   name_space, iface_name);
        }

      for (l = struct_data_list; l != NULL; l = l->next)
        {
          StructData *struct_data = (StructData *) l->data;

          g_print ("typedef struct _%s%s %s%s; /* Dummy typedef */\n"
                   "\n",
                   name_space, struct_data->name,
                   name_space, struct_data->name);
        }

      g_print ("\n");
      g_print ("G_END_DECLS\n");
      g_print ("\n");
      g_print ("#endif /* %s */\n", header_file_protection);

      g_free (header_file_protection);

      if (!file_print_func_end (TRUE, error))
        {
          g_free (h_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", h_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (h_file_name));
      g_free (h_file_name);
    }

  /* generate the <namespace>-bindings.h file (the main header file) */
  h_file_name = compute_file_name (name_space, "Bindings", ".h");
  file_print_func_begin (h_file_name);

  g_print ("\n"
           "/* File: %s\n"
           " *\n"
           " * Generated by eggdbus-binding-tool %s. Do not edit.\n"
           " */\n"
           "\n",
           h_file_name,
           PACKAGE_VERSION);

  header_file_protection = g_strdup_printf ("__%s_BINDINGS_H",
                                            name_space_uscore_upper);
  g_print ("#ifndef %s\n", header_file_protection);
  g_print ("#define %s\n", header_file_protection);
  g_print ("\n");
  g_print ("#include <glib-object.h>\n");
  g_print ("#include <gio/gio.h>\n");
  g_print ("#include <eggdbus/eggdbus.h>\n");
  g_print ("\n");
  g_print ("G_BEGIN_DECLS\n");
  g_print ("\n");

  print_include (name_space, "Bindings");

  g_hash_table_iter_init (&hash_iter, all_dbus_interfaces);
  while (g_hash_table_iter_next (&hash_iter,
                                 (gpointer) &iface_name,
                                 (gpointer) &interface))
    {
      print_include (name_space, iface_name);
    }

  for (l = struct_data_list; l != NULL; l = l->next)
    {
      StructData *struct_data = (StructData *) l->data;

      print_include (name_space, struct_data->name);
    }

  g_print ("\n");

  g_print ("GType *%s_bindings_get_error_domain_types (void);\n"
           "\n",
           name_space_uscore);

  g_print ("\n");
  g_print ("G_END_DECLS\n");
  g_print ("\n");
  g_print ("#endif /* %s */\n", header_file_protection);

  g_free (header_file_protection);

  if (!file_print_func_end (TRUE, error))
    {
      g_free (h_file_name);
      goto out;
    }
  g_printerr ("Wrote %s\n", h_file_name);
  generated_files = g_slist_prepend (generated_files, g_strdup (h_file_name));
  g_free (h_file_name);

  /* generate the <namespace>-bindings.c file */
  c_file_name = compute_file_name (name_space, "Bindings", ".c");
  file_print_func_begin (c_file_name);

  g_print ("\n"
           "/* File: %s\n"
           " *\n"
           " * Generated by eggdbus-binding-tool %s. Do not edit.\n"
           " */\n"
           "\n",
           c_file_name,
           PACKAGE_VERSION);

  g_print ("#include <string.h>\n");
  print_include (name_space, "Bindings");
  g_print ("\n");

  num_error_domains = 0;
  for (l = enum_data_list; l != NULL; l = l->next)
    {
      EnumData *enum_data = (EnumData *) l->data;
      if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
        num_error_domains++;
    }

  if (num_error_domains == 0)
    {
      g_print ("GType *\n"
               "%s_bindings_get_error_domain_types (void)\n"
               "{\n"
               "  return NULL;\n"
               "}\n"
               "\n",
               name_space_uscore);
    }
  else
    {
      g_print ("GType *\n"
               "%s_bindings_get_error_domain_types (void)\n"
               "{\n"
               "  guint n;\n"
               "  static gboolean is_set = FALSE;\n"
               "  static GType error_domain_types[%d];\n"
               "\n"
               "  if (is_set)\n"
               "    return error_domain_types;\n"
               "\n",
               name_space_uscore,
               num_error_domains + 1);

      g_print ("  n = 0;\n");
      for (l = enum_data_list; l != NULL; l = l->next)
        {
          EnumData *enum_data = (EnumData *) l->data;
          if (enum_data->type != ENUM_DATA_TYPE_ERROR_DOMAIN)
            continue;
          g_print ("  error_domain_types[n++] = %s_TYPE_%s;\n",
                   name_space_uscore_upper, enum_data->name_uscore_upper);
        }
      g_print ("  error_domain_types[n] = G_TYPE_INVALID;\n"
               "  is_set = TRUE;\n"
               "\n"
               "  return error_domain_types;\n"
               "}\n"
               "\n");
    }

  if (!file_print_func_end (TRUE, error))
    {
      g_free (c_file_name);
      goto out;
    }
  g_printerr ("Wrote %s\n", c_file_name);
  generated_files = g_slist_prepend (generated_files, g_strdup (c_file_name));
  g_free (c_file_name);

  ret = TRUE;

 out:
  g_free (name_space_uscore);
  g_free (name_space_uscore_upper);
  g_free (name_space_hyphen);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
generate_marshallers (GError **error)
{
  GSList *l;
  char *list_file_name;
  char *name_space_uscore;
  gboolean ret;
  int n;

  ret = FALSE;
  list_file_name = NULL;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);

  /* generate .list file */
  list_file_name = compute_file_name (name_space, "BindingsMarshal", ".list");
  file_print_func_begin (list_file_name);
  for (l = marshallers_to_generate; l != NULL; l = l->next)
    {
      const char *marshal_string = l->data;

      g_print ("%s\n", marshal_string);

      /* skip C function name */
      g_assert (l->next != NULL);
      l = l->next;
    }
  if (!file_print_func_end (TRUE, error))
    {
      g_free (list_file_name);
      goto out;
    }
  g_printerr ("Wrote %s\n", list_file_name);
  generated_files = g_slist_prepend (generated_files, g_strdup (list_file_name));


  for (n = 0; n < 2; n++)
    {
      char *standard_output;
      char *file_name;
      char *command_line;
      int exit_status;
      const gchar *genmarshal_prog;

      genmarshal_prog = g_getenv ("EGG_DBUS_GENMARSHAL");
      if (genmarshal_prog == NULL)
        genmarshal_prog = "eggdbus-glib-genmarshal";

      command_line = g_strdup_printf ("%s %s --prefix=_%s_cclosure_marshal %s",
                                      genmarshal_prog,
                                      n == 0 ? "--header" : "--body",
                                      name_space_uscore,
                                      list_file_name);

      if (!g_spawn_command_line_sync (command_line,
                                      &standard_output,
                                      NULL,
                                      &exit_status,
                                      error))
        {
          g_free (command_line);
          goto out;
        }

      if (exit_status != 0)
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "Got exit status %d when invoking %s",
                       exit_status, command_line);
          g_free (command_line);
          g_free (standard_output);
          goto out;
        }
      g_free (command_line);

      /* no option to glib-genmarshal(1) to specify that the generated
       * C file should include the H file. Thanks GLib.
       */
      if (n == 1)
        {
          char *s;

          file_name = compute_file_name (name_space, "BindingsMarshal", ".h");

          s = g_strdup_printf ("#include \"%s\"\n"
                               "\n"
                               "%s",
                               file_name,
                               standard_output);
          g_free (file_name);
          g_free (standard_output);
          standard_output = s;
        }

      file_name = compute_file_name (name_space,
                                     "BindingsMarshal",
                                     n == 0 ? ".h" : ".c");
      if (!g_file_set_contents (file_name, standard_output, -1, error))
        {
          g_free (file_name);
          g_free (standard_output);
          goto out;
        }
      g_printerr ("Wrote %s\n", file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (file_name));
      g_free (file_name);
      g_free (standard_output);

    }

  ret = TRUE;

 out:
  g_free (list_file_name);
  g_free (name_space_uscore);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
parse_interfaces_enums_and_structs (GSList   *nodes,
                                    GError **error)
{
  guint n;
  guint m;
  gchar *name_space_hyphen;
  gboolean ret;
  GSList *l;
  EnumData *enum_data;

  ret = FALSE;
  enum_data = NULL;
  name_space_hyphen = egg_dbus_utils_camel_case_to_hyphen (name_space);

  /* look for org.gtk.EggDBus.DeclareStruct annotations in the top node
   * and in interface nodes.
   */
  for (l = nodes; l != NULL; l = l->next)
    {
      EggDBusInterfaceNodeInfo *node = l->data;

      for (m = 0; m <= node->num_interfaces; m++)
        {
          const EggDBusInterfaceInfo *interface;
          const EggDBusInterfaceAnnotationInfo *annotation_base;

          if (m < node->num_interfaces)
            {
              annotation_base = (node->interfaces + m)->annotations;
              interface = node->interfaces + m;
            }
          else
            {
              annotation_base = node->annotations;
              interface = NULL;
            }

          for (n = 0; annotation_base != NULL && annotation_base[n].key != NULL; n++)
            {
              const EggDBusInterfaceAnnotationInfo *annotation = annotation_base + n;

              if (strcmp (annotation->key, "org.gtk.EggDBus.DeclareStruct") == 0)
                {
                  StructData *struct_data;

                  struct_data = struct_data_new_from_annotation (annotation, error);
                  if (struct_data == NULL)
                    goto out;

                  struct_data->interface = interface;
                  struct_data_list = g_slist_append (struct_data_list, struct_data);
                }
            }
        }
    }

  /* look for annotations
   *
   *  org.gtk.EggDBus.DeclareErrorDomain
   *  org.gtk.EggDBus.DeclareFlags
   *  org.gtk.EggDBus.DeclareEnum
   *
   * in the top nodes and in interface nodes.
   */
  for (l = nodes; l != NULL; l = l->next)
    {
      EggDBusInterfaceNodeInfo *node = l->data;

      for (m = 0; m <= node->num_interfaces; m++)
        {
          const EggDBusInterfaceInfo *interface;
          const EggDBusInterfaceAnnotationInfo *annotation_base;

          if (m < node->num_interfaces)
            {
              annotation_base = (node->interfaces + m)->annotations;
              interface = node->interfaces + m;
            }
          else
            {
              annotation_base = node->annotations;
              interface = NULL;
            }

          for (n = 0; annotation_base != NULL && annotation_base[n].key != NULL; n++)
            {
              const EggDBusInterfaceAnnotationInfo *annotation = annotation_base + n;

              enum_data = NULL;

              if (strcmp (annotation->key, "org.gtk.EggDBus.DeclareErrorDomain") == 0)
                {
                  enum_data = enum_data_new_from_annotation_for_error_domain (annotation, error);
                  if (enum_data == NULL)
                    goto out;
                }
              else if (strcmp (annotation->key, "org.gtk.EggDBus.DeclareFlags") == 0)
                {
                  enum_data = enum_data_new_from_annotation_for_flags (annotation, error);
                  if (enum_data == NULL)
                    goto out;
                }
              else if (strcmp (annotation->key, "org.gtk.EggDBus.DeclareEnum") == 0)
                {
                  enum_data = enum_data_new_from_annotation_for_enum (annotation, error);
                  if (enum_data == NULL)
                    goto out;
                }

              if (enum_data != NULL)
                {
                  enum_data->interface = interface;
                  enum_data_list = g_slist_append (enum_data_list, enum_data);
                  enum_data = NULL;
                }

            } /* for all annotations for node/interface */

        } /* for all interfaces */

    } /* for all nodes */


  for (l = nodes; l != NULL; l = l->next)
    {
      const EggDBusInterfaceNodeInfo *node = l->data;

#if 0
      GString *s;
      s = g_string_new (NULL);
      egg_dbus_interface_node_info_to_xml (node, 2, s);
      g_printerr ("node:\n%s", s->str);
      g_string_free (s, TRUE);
#endif

      for (n = 0; n < node->num_interfaces; n++)
        {
          const EggDBusInterfaceInfo *interface = node->interfaces + n;
          gchar *iface_name;
          gchar *iface_name_hyphen;

          iface_name = g_strdup (egg_dbus_interface_annotation_info_lookup (interface->annotations,
                                                                          "org.gtk.EggDBus.Name"));
          /* infer class name if not specified */
          if (iface_name == NULL)
            {
              if (g_str_has_prefix (interface->name, dbus_name_space) &&
                  interface->name[strlen (dbus_name_space)] == '.')
                {

                  iface_name = g_strdup (interface->name + strlen (dbus_name_space) + 1);

                  g_printerr ("Inferred GInterface name %s for D-Bus interface %s\n",
                              iface_name,
                              interface->name);
                }
              else
                {
                  g_set_error (error,
                               EGG_DBUS_ERROR,
                               EGG_DBUS_ERROR_FAILED,
                               "Cannot infer GInterface name for D-Bus interface %s",
                               interface->name);
                  goto out;
                }
            }

          /* TODO: check this interface name isn't already in use */

          iface_name_hyphen = egg_dbus_utils_camel_case_to_hyphen (iface_name);

          /* set g_name for all properties and signals */
          for (m = 0; m < interface->num_properties; m++)
            {
              EggDBusInterfacePropertyInfo *prop = (EggDBusInterfacePropertyInfo *) interface->properties + m;
              prop->g_name = egg_dbus_utils_camel_case_to_hyphen (prop->name);
            }
          for (m = 0; m < interface->num_signals; m++)
            {
              EggDBusInterfaceSignalInfo *signal = (EggDBusInterfaceSignalInfo *) interface->signals + m;
              signal->g_name = egg_dbus_utils_camel_case_to_hyphen (signal->name);
            }

          g_free (iface_name_hyphen);

          g_hash_table_insert (all_dbus_interfaces,
                               g_strdup (iface_name),
                               (gpointer) interface);
        }
    }

  ret = TRUE;

 out:
  g_free (name_space_hyphen);
  if (enum_data != NULL)
    enum_data_free (enum_data);
  return ret;
}
/* ---------------------------------------------------------------------------------------------------- */

static gboolean
generate_enums (GError **error)
{
  GSList *l;
  gboolean ret;

  ret = FALSE;

  for (l = enum_data_list; l != NULL; l = l->next)
    {
      EnumData *enum_data = l->data;
      gchar *h_file_name;
      gchar *c_file_name;

      /* now generate Docbook, H and C files for enum_data */

      /* generate docbook file unless enum belongs to an interface */
      if (enum_data->interface == NULL)
        {
          gchar *docbook_file_name;

          docbook_file_name = g_strdup_printf ("docbook-enum-%s.xml", enum_data->name);
          file_print_func_begin (docbook_file_name);
          if (!enum_generate_docbook (enum_data, FALSE, error))
            {
              g_free (docbook_file_name);
              file_print_func_end (FALSE, NULL);
              goto out;
            }
          if (!file_print_func_end (TRUE, error))
            {
              g_free (docbook_file_name);
              goto out;
            }
          g_printerr ("Wrote %s\n", docbook_file_name);
          generated_files = g_slist_prepend (generated_files, g_strdup (docbook_file_name));
          g_free (docbook_file_name);
        }

      /* generate header file */
      h_file_name = compute_file_name (name_space, enum_data->name, ".h");
      file_print_func_begin (h_file_name);

      if (!enum_generate_h_file (enum_data,
                                 name_space,
                                 h_file_name,
                                 enum_data->name,
                                 error))
        {
          g_free (h_file_name);
          file_print_func_end (FALSE, NULL);
          goto out;
        }
      if (!file_print_func_end (TRUE, error))
        {
          g_free (h_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", h_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (h_file_name));

      /* generate c file */
      c_file_name = compute_file_name (name_space, enum_data->name, ".c");
      file_print_func_begin (c_file_name);

      if (!enum_generate_c_file (enum_data,
                                 name_space,
                                 c_file_name,
                                 h_file_name,
                                 enum_data->name,
                                 error))
        {
          g_free (c_file_name);
          g_free (h_file_name);
          file_print_func_end (FALSE, NULL);
          goto out;
        }
      if (!file_print_func_end (TRUE, error))
        {
          g_free (c_file_name);
          g_free (h_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", c_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (c_file_name));

      g_free (h_file_name);
      g_free (c_file_name);

    } /* for all enums */

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
generate_struct_interfaces (GError **error)
{
  gboolean ret;
  GSList *l;

  ret = FALSE;

  for (l = struct_data_list; l != NULL; l = l->next)
    {
      StructData *struct_data = l->data;
      gchar *h_file_name;
      gchar *c_file_name;

      /* now generate Docbook, H and C files for struct_interface unless they are user supplied */

      if (struct_data->user_supplied)
        continue;

      /* generate docbook file unless struct belongs to an interface */
      if (struct_data->interface == NULL)
        {
          gchar *docbook_file_name;

          docbook_file_name = g_strdup_printf ("docbook-struct-%s.xml", struct_data->name);
          file_print_func_begin (docbook_file_name);
          if (!struct_generate_docbook (struct_data, FALSE, error))
            {
              g_free (docbook_file_name);
              file_print_func_end (FALSE, NULL);
              goto out;
            }
          if (!file_print_func_end (TRUE, error))
            {
              g_free (docbook_file_name);
              goto out;
            }
          g_printerr ("Wrote %s\n", docbook_file_name);
          generated_files = g_slist_prepend (generated_files, g_strdup (docbook_file_name));
          g_free (docbook_file_name);
        }

      /* generate header file */
      h_file_name = compute_file_name (name_space, struct_data->name, ".h");
      file_print_func_begin (h_file_name);

      if (!struct_generate_h_file (struct_data,
                                   name_space,
                                   h_file_name,
                                   struct_data->name,
                                   error))
        {
          g_free (h_file_name);
          file_print_func_end (FALSE, NULL);
          goto out;
        }
      if (!file_print_func_end (TRUE, error))
        {
          g_free (h_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", h_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (h_file_name));

      /* generate c file */
      c_file_name = compute_file_name (name_space, struct_data->name, ".c");
      file_print_func_begin (c_file_name);

      if (!struct_generate_c_file (struct_data,
                                   name_space,
                                   c_file_name,
                                   h_file_name,
                                   struct_data->name,
                                   error))
        {
          g_free (c_file_name);
          g_free (h_file_name);
          file_print_func_end (FALSE, NULL);
          goto out;
        }
      if (!file_print_func_end (TRUE, error))
        {
          g_free (c_file_name);
          g_free (h_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", c_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (c_file_name));

      g_free (h_file_name);
      g_free (c_file_name);

    } /* foreach struct_interface to generate */

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
generate_dbus_interfaces (GError  **error)
{
  gboolean ret;
  GHashTableIter hash_iter;
  const gchar *iface_name;
  EggDBusInterfaceInfo *interface;

  ret = FALSE;

  g_hash_table_iter_init (&hash_iter, all_dbus_interfaces);
  while (g_hash_table_iter_next (&hash_iter, (gpointer) &iface_name, (gpointer) &interface))
    {
      gchar *h_file_name;
      gchar *c_file_name;
      gchar *docbook_file_name;

      docbook_file_name = g_strdup_printf ("docbook-interface-%s.xml", interface->name);
      file_print_func_begin (docbook_file_name);
      if (!interface_generate_docbook (interface, error))
        {
          g_free (docbook_file_name);
          file_print_func_end (FALSE, NULL);
          goto out;
        }
      if (!file_print_func_end (TRUE, error))
        {
          g_free (docbook_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", docbook_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (docbook_file_name));
      g_free (docbook_file_name);

      h_file_name = compute_file_name (name_space, iface_name, ".h");
      file_print_func_begin (h_file_name);
      if (!interface_generate_iface_h_file (interface,
                                            name_space,
                                            iface_name,
                                            h_file_name,
                                            error))
        {
          g_free (h_file_name);
          file_print_func_end (FALSE, NULL);
          goto out;
        }
      if (!file_print_func_end (TRUE, error))
        {
          g_free (h_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", h_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (h_file_name));

      c_file_name = compute_file_name (name_space, iface_name, ".c");
      file_print_func_begin (c_file_name);
      if (!interface_generate_iface_c_file (interface,
                                            name_space,
                                            iface_name,
                                            c_file_name,
                                            h_file_name,
                                            error))
        {
          g_free (c_file_name);
          g_free (h_file_name);
          file_print_func_end (FALSE, NULL);
          goto out;
        }
      if (!file_print_func_end (TRUE, error))
        {
          g_free (c_file_name);
          g_free (h_file_name);
          goto out;
        }
      g_printerr ("Wrote %s\n", c_file_name);
      generated_files = g_slist_prepend (generated_files, g_strdup (c_file_name));
      g_free (c_file_name);
      g_free (h_file_name);
    }

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static GSList *complete_types = NULL;

CompleteType *
get_complete_type_for_arg (const EggDBusInterfaceArgInfo *arg)
{
  return (CompleteType *) g_dataset_get_data (arg, "complete-type");
}

CompleteType *
get_complete_type_for_property (const EggDBusInterfacePropertyInfo *property)
{
  return (CompleteType *) g_dataset_get_data (property, "complete-type");
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

static gchar *
infer_complete_type_cb (const gchar  *signature,
                        gpointer      user_data,
                        GError      **error)
{
  gchar *ret;

  ret = NULL;

  if (signature[0] == '(')
    {
      GSList *matches;
      StructData *struct_data;

      matches = find_struct_by_signature (signature);
      if (matches == NULL)
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "No declared structs with signature '%s'. If you really want an anonymous structure, "
                       "please specify it in a org.gtk.EggDBus.Type annotation.",
                       signature);
          goto out;
        }

      if (g_slist_length (matches) > 1)
        {
          g_slist_free (matches);
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "Multiple structs with signature '%s' exists. Please use org.gtk.EggDBus.Type "
                       "annotation to disambiguate.",
                       signature);
          goto out;
        }

      struct_data = matches->data;
      ret = g_strdup (struct_data->name);
      g_slist_free (matches);
    }

 out:
  return ret;
}

static CompleteType *
determine_and_validate_type (EggDBusInterfaceAnnotationInfo  **annotations,
                             const gchar                      *expected_signature,
                             GError                          **error)
{
  const gchar *type_string;
  CompleteType *type;

  type = NULL;

  /* if the user specified the complete type, verify that it matches the signature */
  type_string = egg_dbus_interface_annotation_info_lookup (*annotations, "org.gtk.EggDBus.Type");
  if (type_string != NULL)
    {
      type = complete_type_from_string (type_string,
                                        break_down_type_cb,
                                        NULL,
                                        error);
      if (type == NULL)
        goto out;

      if (strcmp (type->signature, expected_signature) != 0)
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "Signature of the complete type %s is %s and it doesn't match the given signature %s",
                       type_string,
                       type->signature,
                       expected_signature);
          complete_type_free (type);
          type = NULL;
          goto out;
        }
    }
  else
    {
      gchar *s;
      /* user didn't specify complete type, let's see if we can infer it */
      s = complete_type_name_from_signature (expected_signature,
                                             infer_complete_type_cb,
                                             NULL,
                                             error);
      if (s == NULL)
        goto out;

      _egg_dbus_interface_annotation_info_set ((EggDBusInterfaceAnnotationInfo **) annotations,
                                               "org.gtk.EggDBus.Type",
                                               (gpointer) s);

      type = complete_type_from_string (s,
                                        break_down_type_cb,
                                        NULL,
                                        error);
      if (type == NULL)
        goto out;

    }

 out:
  return type;
}

static gboolean
determine_and_validate_arg (const EggDBusInterfaceArgInfo  *arg,
                            GError                        **error)
{
  CompleteType *type;

  type = determine_and_validate_type ((EggDBusInterfaceAnnotationInfo **) &(arg->annotations),
                                      arg->signature,
                                      error);

  if (type != NULL)
    {
      g_dataset_set_data (arg, "complete-type", type);
      complete_types = g_slist_prepend (complete_types, type);
    }

  return type != NULL;
}

static gboolean
determine_and_validate_property (const EggDBusInterfacePropertyInfo  *property,
                                 GError                             **error)
{
  CompleteType *type;

  type = determine_and_validate_type ((EggDBusInterfaceAnnotationInfo **) &(property->annotations),
                                      property->signature,
                                      error);

  if (type != NULL)
    {
      g_dataset_set_data (property, "complete-type", type);
      complete_types = g_slist_prepend (complete_types, type);
    }

  return type != NULL;
}

static gboolean
determine_and_validate_complete_types (GError **error)
{
  gboolean ret;
  const gchar *iface_name;
  EggDBusInterfaceInfo *interface;
  GHashTableIter hash_iter;
  guint n;
  guint m;
  GSList *l;

  ret = FALSE;

  /* foreach struct */
  for (l = struct_data_list; l != NULL; l = l->next)
    {
      StructData *struct_data = l->data;
      if (!struct_data_compute_types_and_signatures (struct_data, error))
        goto out;
    }

  /* foreach interface */
  g_hash_table_iter_init (&hash_iter, all_dbus_interfaces);
  while (g_hash_table_iter_next (&hash_iter, (gpointer) &iface_name, (gpointer) &interface))
    {
      /* foreach method */
      for (n = 0; n < interface->num_methods; n++)
        {
          const EggDBusInterfaceMethodInfo *method = interface->methods + n;
          for (m = 0; m < method->in_num_args; m++)
            {
              const EggDBusInterfaceArgInfo *arg = method->in_args + m;
              if (!determine_and_validate_arg (arg, error))
                {
                  g_prefix_error (error,
                                  "When handling in-arg %d in on method %s.%s: ",
                                  m,
                                  interface->name,
                                  method->name);
                  goto out;
                }
            }
          for (m = 0; m < method->out_num_args; m++)
            {
              const EggDBusInterfaceArgInfo *arg = method->out_args + m;
              if (!determine_and_validate_arg (arg, error))
                {
                  g_prefix_error (error,
                                  "When handling out-arg %d in on method %s.%s: ",
                                  m,
                                  interface->name,
                                  method->name);
                  goto out;
                }
            }
        }

      /* foreach signal */
      for (n = 0; n < interface->num_signals; n++)
        {
          const EggDBusInterfaceSignalInfo *signal = interface->signals + n;
          for (m = 0; m < signal->num_args; m++)
            {
              const EggDBusInterfaceArgInfo *arg = signal->args + m;
              if (!determine_and_validate_arg (arg, error))
                {
                  g_prefix_error (error,
                                  "When handling arg %d in on signal %s::%s: ",
                                  m,
                                  interface->name,
                                  signal->name);
                  goto out;
                }
            }
        }

      /* foreach property */
      for (n = 0; n < interface->num_properties; n++)
        {
          const EggDBusInterfacePropertyInfo *property = interface->properties + n;
          if (!determine_and_validate_property (property, error))
            {
              g_prefix_error (error,
                              "When handling property %s:%s: ",
                              interface->name,
                              property->name);
              goto out;
            }
        }
    }

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
parse (char       **xml_files,
       GError     **error)
{
  gboolean ret;
  GSList *nodes;
  guint n;

  ret = FALSE;

  all_dbus_interfaces = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  struct_data_list = NULL;
  nodes = NULL;

  /* parse all XML files */
  for (n = 0; xml_files[n] != NULL; n++)
    {
      EggDBusInterfaceNodeInfo *node;
      gchar *xml_data;

      if (!g_file_get_contents (xml_files[n],
                                &xml_data,
                                NULL,
                                error))
        goto out;

      node = egg_dbus_interface_new_node_info_from_xml (xml_data, error);
      if (node == NULL)
        {
          /* this makes error reading in emacs work nice */
          g_prefix_error (error, "%s:", xml_files[n]);
          g_free (xml_data);
          goto out;
        }

      nodes = g_slist_append (nodes, node);
    }

  /* then extract enums and structs */
  if (!parse_interfaces_enums_and_structs (nodes, error))
    goto out;

  /* now that we have enums/structs in place, we can determine/validate the complete type of each signature */
  if (!determine_and_validate_complete_types (error))
    goto out;

  /* then generate... */

  /* ... regular expression for Docbook/gtkdoc links */
  if (!create_link_regexs (error))
    goto out;

  /* ... GObjects for structs */
  if (!generate_struct_interfaces (error))
    goto out;

  /* ... Enums for error domains, enums and flags */
  if (!generate_enums (error))
    goto out;

  /* ... GInterfaces for all D-Bus interfaces */
  if (!generate_dbus_interfaces (error))
    goto out;

  /* ... <namespace>bindings.[ch] and <namespace>bindingstypes.h */
  if (!opt_iface_only)
    {
      if (!generate_main_files (error))
        goto out;
    }

  /* ... <namespace>bindingmarshal.list, then <namespace>bindingsmarshal.[ch] */
  if (!generate_marshallers (error))
    goto out;

  /* finally generate a stamp file if requested */
  if (opt_stamp_file != NULL)
    {
      GSList *l;

      file_print_func_begin (opt_stamp_file);
      for (l = generated_files; l != NULL; l = l->next)
        {
          const gchar *file_name = l->data;
          g_print ("%s ", file_name);
        }
      g_print ("\n");
      if (!file_print_func_end (TRUE, error))
        {
          goto out;
        }
    }

  /* and we're done... */

  ret = TRUE;

 out:

  free_link_regexs ();

  g_hash_table_unref (all_dbus_interfaces);

  g_slist_foreach (marshallers_to_generate, (GFunc) g_free, NULL);
  g_slist_free (marshallers_to_generate);

  g_slist_foreach (struct_data_list, (GFunc) struct_data_free, NULL);
  g_slist_free (struct_data_list);

  g_slist_foreach (enum_data_list, (GFunc) enum_data_free, NULL);
  g_slist_free (enum_data_list);

  g_slist_foreach (nodes, (GFunc) egg_dbus_interface_node_info_free, NULL);
  g_slist_free (nodes);

  g_slist_foreach (generated_files, (GFunc) g_free, NULL);
  g_slist_free (generated_files);

  g_slist_foreach (complete_types, (GFunc) complete_type_free, NULL);
  g_slist_free (complete_types);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
usage (void)
{
  g_printerr ("eggdbus-binding-tool -x <introspection-xml>\n");
  g_printerr ("\n");
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int argc, char *argv[])
{
  int ret;
  GOptionContext *context;
  GError *error;

  ret = 1;

  error = NULL;

  g_type_init ();

  context = g_option_context_new ("D-Bus Introspection XML to GObject code generator");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_parse (context, &argc, &argv, NULL);

  if (opt_xml == NULL)
    {
      usage ();
      goto out;
    }

  if (name_space == NULL)
    {
      usage ();
      goto out;
    }

  if (dbus_name_space == NULL)
    {
      usage ();
      goto out;
    }

  if (!parse (opt_xml, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      goto out;
    }

  ret = 0;

 out:
  g_option_context_free (context);
  return ret;
}
