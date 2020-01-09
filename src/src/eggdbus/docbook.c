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
#include <eggdbus/eggdbusinterface.h>

#include "eggdbusbindingtool.h"
#include "docbook.h"
#include "completetype.h"

static void
pretty_print_type_name (CompleteType *type,
                        GString      *name,
                        GString      *link)
{
  guint n;
  StructData *struct_data;
  EnumData *enum_data;

  if (type->user_type != NULL)
    {
      struct_data = find_struct_by_name (type->user_type);
      if (struct_data != NULL)
        {
          g_string_append_printf (name, "%s", struct_data->name);
          g_string_append_printf (link, "<link linkend=\"eggdbus-struct-%s\">%s</link>",
                                  struct_data->name,
                                  struct_data->name);
        }
      else
        {
          enum_data = find_enum_by_name (type->user_type);
          if (enum_data != NULL)
            {
              g_string_append_printf (name, "%s", enum_data->name);
              g_string_append_printf (link, "<link linkend=\"eggdbus-enum-%s\">%s</link>",
                                      enum_data->name,
                                      enum_data->name);
            }
          else
            {
              g_string_append (name, type->user_type);
              g_string_append (link, type->user_type);
            }
        }
      goto out;
    }

  switch (type->signature[0])
    {
    case DBUS_TYPE_BYTE:
      g_string_append (name, "Byte");
      g_string_append (link, "Byte");
      break;

    case DBUS_TYPE_BOOLEAN:
      g_string_append (name, "Boolean");
      g_string_append (link, "Boolean");
      break;

    case DBUS_TYPE_INT16:
      g_string_append (name, "Int16");
      g_string_append (link, "Int16");
      break;

    case DBUS_TYPE_UINT16:
      g_string_append (name, "UInt16");
      g_string_append (link, "UInt16");
      break;

    case DBUS_TYPE_INT32:
      g_string_append (name, "Int32");
      g_string_append (link, "Int32");
      break;

    case DBUS_TYPE_UINT32:
      g_string_append (name, "UInt32");
      g_string_append (link, "UInt32");
      break;

    case DBUS_TYPE_INT64:
      g_string_append (name, "Int64");
      g_string_append (link, "Int64");
      break;

    case DBUS_TYPE_UINT64:
      g_string_append (name, "UInt64");
      g_string_append (link, "UInt64");
      break;

    case DBUS_TYPE_DOUBLE:
      g_string_append (name, "Double");
      g_string_append (link, "Double");
      break;

    case DBUS_TYPE_STRING:
      g_string_append (name, "String");
      g_string_append (link, "String");
      break;

    case DBUS_TYPE_OBJECT_PATH:
      g_string_append (name, "ObjectPath");
      g_string_append (link, "ObjectPath");
      break;

    case DBUS_TYPE_SIGNATURE:
      g_string_append (name, "Signature");
      g_string_append (link, "Signature");
      break;

    case DBUS_TYPE_VARIANT:
      g_string_append (name, "Variant");
      g_string_append (link, "Variant");
      break;

    case DBUS_TYPE_ARRAY:
      if (type->signature[1] == '{')
        {
          g_string_append (name, "Dict<");
          g_string_append (link, "Dict&lt;");
          pretty_print_type_name (type->contained_types[0], name, link);
          g_string_append (name, ",");
          g_string_append (link, ",");
          pretty_print_type_name (type->contained_types[1], name, link);
          g_string_append (name, ">");
          g_string_append (link, "&gt;");
        }
      else
        {
          g_string_append (name, "Array<");
          g_string_append (link, "Array&lt;");
          pretty_print_type_name (type->contained_types[0], name, link);
          g_string_append (name, ">");
          g_string_append (link, "&gt;");
        }
      break;

    case '(':
      g_string_append (name, "Struct<");
      g_string_append (link, "Struct&lt;");
      for (n = 0; n < type->num_contained_types; n++)
        {
          if (n > 0)
            {
              g_string_append (name, ",");
              g_string_append (link, ",");
            }
          pretty_print_type_name (type->contained_types[n], name, link);
        }
      g_string_append (name, ">");
      g_string_append (link, "&gt;");
      break;

    default:
      g_assert_not_reached ();
      break;
    }
 out:
  ;
}

static void
docbook_get_typename_for_signature (CompleteType  *type,
                                    gchar        **out_name,
                                    gchar        **out_link)
{
  GString *name_str;
  GString *link_str;

  name_str = g_string_new (NULL);
  link_str = g_string_new (NULL);

  pretty_print_type_name (type, name_str, link_str);

  if (out_name != NULL)
    *out_name = g_string_free (name_str, FALSE);
  else
    g_string_free (name_str, TRUE);

  if (out_link != NULL)
    *out_link = g_string_free (link_str, FALSE);
  else
    g_string_free (link_str, TRUE);
}

static void
docbook_print_arg (const EggDBusInterfaceArgInfo  *arg,
                   const gchar                    *arg_prefix,
                   guint                           arg_max_len)
{
  gchar *arg_type_name;
  gchar *arg_type_link;

  docbook_get_typename_for_signature (get_complete_type_for_arg (arg),
                                      &arg_type_name,
                                      &arg_type_link);

  g_print ("%s%s%*s%s",
           arg_prefix,
           arg_type_link,
           (int) (arg_max_len - strlen (arg_type_name)), "",
           arg->name);

  g_free (arg_type_name);
  g_free (arg_type_link);
}

static guint
docbook_get_max_arg_len_for_method (const EggDBusInterfaceMethodInfo    *method)
{
  guint n;
  guint max_arg_len;

  max_arg_len = 0;

  for (n = 0; n < method->in_num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = method->in_args + n;
      gchar *arg_type_name;
      guint arg_len;

      docbook_get_typename_for_signature (get_complete_type_for_arg (arg),
                                          &arg_type_name,
                                          NULL);
      arg_len = strlen (arg_type_name);
      g_free (arg_type_name);

      if (arg_len > max_arg_len)
        max_arg_len = arg_len;
    }

  for (n = 0; n < method->out_num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = method->out_args + n;
      gchar *arg_type_name;
      guint arg_len;

      docbook_get_typename_for_signature (get_complete_type_for_arg (arg),
                                          &arg_type_name,
                                          NULL);
      arg_len = strlen (arg_type_name);
      g_free (arg_type_name);

      if (arg_len > max_arg_len)
        max_arg_len = arg_len;
    }

  return max_arg_len;
}

static guint
docbook_get_max_arg_len_for_signal (const EggDBusInterfaceSignalInfo *signal)
{
  guint n;
  guint max_arg_len;

  max_arg_len = 0;

  for (n = 0; n < signal->num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = signal->args + n;
      gchar *arg_type_name;
      guint arg_len;

      docbook_get_typename_for_signature (get_complete_type_for_arg (arg),
                                          &arg_type_name,
                                          NULL);
      arg_len = strlen (arg_type_name);
      g_free (arg_type_name);

      if (arg_len > max_arg_len)
        max_arg_len = arg_len;
    }

  return max_arg_len;
}

static void
docbook_print_method_prototype (const EggDBusInterfaceInfo          *interface,
                                const EggDBusInterfaceMethodInfo    *method,
                                guint                                indent,
                                guint                                arg_max_len,
                                gboolean                             use_hyperlink)
{
  guint n;
  guint num_printed;
  guint first_indent;

  first_indent = indent - strlen (method->name) - 1;

  if (use_hyperlink)
    {
      g_print ("<link linkend=\"eggdbus-method-%s.%s\">%s</link>%*s(",
               interface->name,
               method->name,
               method->name,
               first_indent, "");
    }
  else
    {
      g_print ("%s%*s(",
               method->name,
               first_indent, "");
    }

  num_printed = 0;
  for (n = 0; n < method->in_num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = method->in_args + n;

      if (num_printed != 0)
        g_print (",\n%*s", indent, "");

      docbook_print_arg (arg, "IN  ", arg_max_len);

      num_printed++;
    }

  for (n = 0; n < method->out_num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = method->out_args + n;

      if (num_printed != 0)
        g_print (",\n%*s", indent, "");

      docbook_print_arg (arg, "OUT ", arg_max_len);

      num_printed++;
    }

  g_print (")\n");
}

static void
docbook_print_signal_prototype (const EggDBusInterfaceInfo          *interface,
                                const EggDBusInterfaceSignalInfo    *signal,
                                guint                                indent,
                                guint                                arg_max_len,
                                gboolean                             use_hyperlink)
{
  guint n;
  guint num_printed;
  guint first_indent;

  first_indent = indent - strlen (signal->name) - 1;

  if (use_hyperlink)
    {
      g_print ("<link linkend=\"eggdbus-signal-%s::%s\">%s</link>%*s(",
               interface->name,
               signal->name,
               signal->name,
               first_indent, "");
    }
  else
    {
      g_print ("%s%*s(",
               signal->name,
               first_indent, "");
    }

  num_printed = 0;
  for (n = 0; n < signal->num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = signal->args + n;

      if (num_printed != 0)
        g_print (",\n%*s", indent, "");

      docbook_print_arg (arg, "", arg_max_len);

      num_printed++;
    }

  g_print (")\n");
}

static void
docbook_print_property_prototype (const EggDBusInterfaceInfo          *interface,
                                  const EggDBusInterfacePropertyInfo  *property,
                                  guint                                indent,
                                  gboolean                             use_hyperlink)
{
  guint first_indent;
  gchar *arg_type_link;

  first_indent = indent - strlen (property->name) - 1;

  if (use_hyperlink)
    {
      g_print ("<link linkend=\"eggdbus-property-%s:%s\">%s</link>%*s    ",
               interface->name,
               property->name,
               property->name,
               first_indent, "");
    }
  else
    {
      g_print ("%s%*s    ",
               property->name,
               first_indent, "");
    }

  if ((property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE) &&
      (property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE))
    {
      g_print ("readwrite ");
    }
  else if (property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE)
    {
      g_print ("readable  ");
    }
  else if (property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE)
    {
      g_print ("writable  ");
    }
  else
    {
      g_print ("          ");
    }

  docbook_get_typename_for_signature (get_complete_type_for_property (property),
                                      NULL,
                                      &arg_type_link);

  g_print ("   %s\n", arg_type_link);

  g_free (arg_type_link);
}

static void
docbook_print_arg_in_list (const gchar                   *prefix,
                           const EggDBusInterfaceArgInfo *arg)
{
  gchar *arg_doc_string;
  gchar *arg_type_link;

  arg_doc_string = get_doc (arg->annotations, DOC_TYPE_DOCBOOK);

  docbook_get_typename_for_signature (get_complete_type_for_arg (arg),
                                      NULL,
                                      &arg_type_link);

  g_print ("  <varlistentry>\n");
  g_print ("    <term><literal>%s%s <parameter>%s</parameter></literal>:</term>\n",
           prefix,
           arg_type_link,
           arg->name);
  g_print ("    <listitem>\n");
  g_print ("      <para>\n");
  g_print ("%s\n", arg_doc_string);
  g_print ("      </para>\n");
  g_print ("    </listitem>\n");
  g_print ("  </varlistentry>\n");

  g_free (arg_doc_string);

  g_free (arg_type_link);
}

static void
docbook_print_args (const EggDBusInterfaceInfo     *interface,
                    const gchar                    *prefix0,
                    const EggDBusInterfaceArgInfo  *args0,
                    guint                           num_args0,
                    const gchar                    *prefix1,
                    const EggDBusInterfaceArgInfo  *args1,
                    guint                           num_args1)
{
  guint n;

  g_print ("<variablelist role=\"params\">\n");

  for (n = 0; n < num_args0; n++)
    {
      const EggDBusInterfaceArgInfo *arg = args0 + n;
      docbook_print_arg_in_list (prefix0, arg);
    }

  for (n = 0; n < num_args1; n++)
    {
      const EggDBusInterfaceArgInfo *arg = args1 + n;
      docbook_print_arg_in_list (prefix1, arg);
    }

  g_print ("</variablelist>\n");
}

gboolean
interface_generate_docbook (const EggDBusInterfaceInfo  *interface,
                            GError                     **error)
{
  gboolean ret;
  gchar *interface_summary_doc_string;
  gchar *interface_doc_string;
  guint n;
  guint indent;
  guint arg_max_len_for_all;
  GSList *enums_to_include;
  GSList *structs_to_include;
  GSList *l;

  ret = FALSE;

  interface_summary_doc_string = get_doc_summary (interface->annotations, DOC_TYPE_DOCBOOK);
  interface_doc_string = get_doc (interface->annotations, DOC_TYPE_DOCBOOK);

  enums_to_include = get_enums_declared_in_interface (interface);
  structs_to_include = get_structs_declared_in_interface (interface);

  g_print ("<?xml version=\"1.0\"?>\n"
           "<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2 //EN\"\n"
           "\"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\">\n");

  g_print ("<refentry id=\"eggdbus-interface-%s\">\n", interface->name);
  g_print ("  <refmeta>\n");
  g_print ("    <refentrytitle role=\"top_of_page\">%s Interface</refentrytitle>\n", interface->name);
  g_print ("  </refmeta>\n");
  g_print ("  <refnamediv>\n");
  g_print ("    <refname>%s Interface</refname>\n", interface->name);
  g_print ("    <refpurpose>%s</refpurpose>\n", interface_summary_doc_string);
  g_print ("  </refnamediv>\n");

  /* Synopsis for methods */
  if (interface->num_methods > 0 || enums_to_include != NULL || structs_to_include != NULL)
    {
      g_print ("  <refsynopsisdiv role=\"synopsis\">\n");
      g_print ("    <title role=\"synopsis.title\">Methods</title>\n");
      g_print ("    <synopsis>\n");

      for (l = enums_to_include; l != NULL; l = l->next)
        {
          EnumData *enum_data = l->data;

          if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
            {
              g_print ("ErrorDomain  <link linkend=\"eggdbus-errordomain-%s\">%s*</link>\n",
                       enum_data->maximal_dbus_prefix,
                       enum_data->maximal_dbus_prefix);
            }
          else if (enum_data->type == ENUM_DATA_TYPE_ENUM)
            {
              g_print ("Enumeration  <link linkend=\"eggdbus-enum-%s\">%s</link>\n",
                       enum_data->name,
                       enum_data->name);
            }
          else if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
            {
              g_print ("Flags        <link linkend=\"eggdbus-enum-%s\">%s</link>\n",
                       enum_data->name,
                       enum_data->name);
            }
          else
            {
              g_assert_not_reached ();
            }
        }

      for (l = structs_to_include; l != NULL; l = l->next)
        {
          StructData *struct_data = l->data;

          g_print ("Structure    <link linkend=\"eggdbus-struct-%s\">%s</link>\n",
                   struct_data->name,
                   struct_data->name);
        }

      if (enums_to_include != NULL || structs_to_include != NULL)
        g_print ("\n");

      indent = 0;
      arg_max_len_for_all = 0;
      for (n = 0; n < interface->num_methods; n++)
        {
          const EggDBusInterfaceMethodInfo *method = interface->methods + n;
          guint method_name_len;
          guint arg_max_len;

          method_name_len = strlen (method->name);
          if (method_name_len > indent)
            indent = method_name_len;

          arg_max_len = docbook_get_max_arg_len_for_method (method);
          if (arg_max_len > arg_max_len_for_all)
            arg_max_len_for_all = arg_max_len;
        }
      for (n = 0; n < interface->num_methods; n++)
        {
          const EggDBusInterfaceMethodInfo *method = interface->methods + n;

          docbook_print_method_prototype (interface,
                                          method,
                                          indent + 2,
                                          arg_max_len_for_all + 2,
                                          TRUE);
        }
      g_print ("    </synopsis>\n");
      g_print ("  </refsynopsisdiv>\n");
    }

  /* Synopsis for signals */
  if (interface->num_signals > 0)
    {
      g_print ("  <refsect1 role=\"signal_proto\" id=\"eggdbus-if-signals-%s\">\n",
               interface->name);
      g_print ("    <title role=\"signal_proto.title\">Signals</title>\n");
      g_print ("    <synopsis>\n");
      indent = 0;
      arg_max_len_for_all = 0;
      for (n = 0; n < interface->num_signals; n++)
        {
          const EggDBusInterfaceSignalInfo *signal = interface->signals + n;
          guint signal_name_len;
          guint arg_max_len;

          signal_name_len = strlen (signal->name);
          if (signal_name_len > indent)
            indent = signal_name_len;

          arg_max_len = docbook_get_max_arg_len_for_signal (signal);
          if (arg_max_len > arg_max_len_for_all)
            arg_max_len_for_all = arg_max_len;
        }
      for (n = 0; n < interface->num_signals; n++)
        {
          const EggDBusInterfaceSignalInfo *signal = interface->signals + n;

          docbook_print_signal_prototype (interface,
                                          signal,
                                          indent + 2,
                                          arg_max_len_for_all + 2,
                                          TRUE);
        }
      g_print ("    </synopsis>\n");
      g_print ("  </refsect1>\n");
    }

  /* Synopsis for properties */
  if (interface->num_properties > 0)
    {
      g_print ("  <refsect1 role=\"properties\" id=\"eggdbus-if-properties-%s\">\n",
               interface->name);
      g_print ("    <title role=\"properties.title\">Properties</title>\n");
      g_print ("    <synopsis>\n");
      indent = 0;
      arg_max_len_for_all = 0;
      for (n = 0; n < interface->num_properties; n++)
        {
          const EggDBusInterfacePropertyInfo *property = interface->properties + n;
          guint property_name_len;

          property_name_len = strlen (property->name);
          if (property_name_len > indent)
            indent = property_name_len;
        }
      for (n = 0; n < interface->num_properties; n++)
        {
          const EggDBusInterfacePropertyInfo *property = interface->properties + n;

          docbook_print_property_prototype (interface,
                                            property,
                                            indent + 2,
                                            TRUE);
        }
      g_print ("    </synopsis>\n");
      g_print ("  </refsect1>\n");
    }

  /* Description */
  g_print ("  <refsect1 role=\"desc\" id=\"eggdbus-if-description-%s\">\n",
           interface->name);
  g_print ("    <title role=\"desc.title\">Description</title>\n");
  g_print ("      <para>\n");
  g_print ("%s\n", interface_doc_string);
  g_print ("      </para>\n");
  g_print ("  </refsect1>\n");

  /* Enumerations and Structs associated with this interface */
  if (enums_to_include != NULL)
    {
      g_print ("  <refsect1 role=\"desc\" id=\"eggdbus-if-enumerations-%s\">\n",
               interface->name);
      g_print ("    <title role=\"desc.title\">Enumerations</title>\n");
      for (l = enums_to_include; l != NULL; l = l->next)
        {
          EnumData *enum_data = l->data;
          if (!enum_generate_docbook (enum_data,
                                      TRUE,
                                      error))
            goto out;
        }
      g_print ("  </refsect1>\n");
    }
  if (structs_to_include != NULL)
    {
      g_print ("  <refsect1 role=\"desc\" id=\"eggdbus-if-structures-%s\">\n",
               interface->name);
      g_print ("    <title role=\"desc.title\">Structures</title>\n");
      for (l = structs_to_include; l != NULL; l = l->next)
        {
          StructData *struct_data = l->data;
          if (!struct_generate_docbook (struct_data,
                                        TRUE,
                                        error))
            goto out;
        }
      g_print ("  </refsect1>\n");
    }

  /* Details for each method */
  if (interface->num_methods > 0)
    {
      g_print ("  <refsect1 role=\"details\" id=\"eggdbus-if-method-details-%s\">\n",
               interface->name);
      g_print ("    <title role=\"details.title\">Method Details</title>\n");
      for (n = 0; n < interface->num_methods; n++)
        {
          const EggDBusInterfaceMethodInfo *method = interface->methods + n;
          guint method_name_len;
          guint arg_max_len;
          gchar *doc_string;

          method_name_len = strlen (method->name);

          arg_max_len = docbook_get_max_arg_len_for_method (method);

          g_print ("    <refsect2 role=\"function\" id=\"eggdbus-method-%s.%s\">\n"
                   "      <title>%s ()</title>\n",
                   interface->name,
                   method->name,
                   method->name);
          g_print ("    <programlisting>\n");

          docbook_print_method_prototype (interface,
                                          method,
                                          method_name_len + 2,
                                          arg_max_len + 2,
                                          FALSE);

          g_print ("    </programlisting>\n");
          g_print ("    <para>\n");
          doc_string = get_doc (method->annotations, DOC_TYPE_DOCBOOK);
          g_print ("%s\n", doc_string);
          g_free (doc_string);
          g_print ("    </para>\n");

          docbook_print_args (interface,
                              "IN  ",
                              method->in_args,
                              method->in_num_args,
                              "OUT ",
                              method->out_args,
                              method->out_num_args);

          g_print ("    </refsect2>\n");
        }
      g_print ("  </refsect1>\n");
    }

  /* Details for each signal */
  if (interface->num_signals > 0)
    {
      g_print ("  <refsect1 role=\"signals\" id=\"eggdbus-if-signal-details-%s\">\n",
               interface->name);
      g_print ("    <title role=\"signals.title\">Signal Details</title>\n");
      for (n = 0; n < interface->num_signals; n++)
        {
          const EggDBusInterfaceSignalInfo *signal = interface->signals + n;
          guint signal_name_len;
          guint arg_max_len;
          gchar *doc_string;

          signal_name_len = strlen (signal->name);

          arg_max_len = docbook_get_max_arg_len_for_signal (signal);

          g_print ("    <refsect2 role=\"signal\" id=\"eggdbus-signal-%s::%s\">\n"
                   "      <title>The \"%s\" signal</title>\n",
                   interface->name,
                   signal->name,
                   signal->name);
          g_print ("    <programlisting>\n");

          docbook_print_signal_prototype (interface,
                                          signal,
                                          signal_name_len + 2,
                                          arg_max_len + 2,
                                          FALSE);

          g_print ("    </programlisting>\n");
          g_print ("    <para>\n");
          doc_string = get_doc (signal->annotations, DOC_TYPE_DOCBOOK);
          g_print ("%s\n", doc_string);
          g_free (doc_string);
          g_print ("    </para>\n");

          docbook_print_args (interface,
                              "",
                              signal->args,
                              signal->num_args,
                              NULL,
                              NULL,
                              0);

          g_print ("    </refsect2>\n");
        }
      g_print ("  </refsect1>\n");
    }

  /* Details for each property */
  if (interface->num_properties > 0)
    {
      g_print ("  <refsect1 role=\"property_details\" id=\"eggdbus-if-property-details-%s\">\n",
               interface->name);
      g_print ("    <title role=\"property_details.title\">Property Details</title>\n");
      for (n = 0; n < interface->num_properties; n++)
        {
          const EggDBusInterfacePropertyInfo *property = interface->properties + n;
          guint property_name_len;
          gchar *doc_string;

          property_name_len = strlen (property->name);

          g_print ("    <refsect2 role=\"property\" id=\"eggdbus-property-%s:%s\">\n"
                   "      <title>The \"%s\" property</title>\n",
                   interface->name,
                   property->name,
                   property->name);
          g_print ("    <programlisting>\n");

          docbook_print_property_prototype (interface,
                                            property,
                                            property_name_len + 2,
                                            FALSE);

          g_print ("    </programlisting>\n");
          g_print ("    <para>\n");
          doc_string = get_doc (property->annotations, DOC_TYPE_DOCBOOK);
          g_print ("%s\n", doc_string);
          g_free (doc_string);
          g_print ("    </para>\n");

          g_print ("    </refsect2>\n");
        }
      g_print ("  </refsect1>\n");
    }

  g_print ("</refentry>\n");

  ret = TRUE;

 out:
  g_free (interface_summary_doc_string);
  g_free (interface_doc_string);
  g_slist_free (enums_to_include);
  g_slist_free (structs_to_include);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

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
enum_generate_docbook (EnumData  *enum_data,
                       gboolean   only_sect2,
                       GError   **error)
{
  gboolean ret;
  gchar *enum_summary_doc_string;
  gchar *enum_doc_string;
  const gchar *type_string;
  guint n;
  guint len;
  guint max_len;
  gboolean has_none_elem;
  gchar *title;

  ret = FALSE;
  has_none_elem = TRUE;

  switch (enum_data->type)
    {
    case ENUM_DATA_TYPE_ERROR_DOMAIN:
      type_string = "Error Domain";
      title = g_strdup_printf ("%s* Error Domain", enum_data->maximal_dbus_prefix);
      break;

    case ENUM_DATA_TYPE_FLAGS:
      type_string = "Flags";
      title = g_strdup_printf ("%s Flag Enumeration", enum_data->name);
      break;

    case ENUM_DATA_TYPE_ENUM:
      type_string = "Enumeration";
      title = g_strdup_printf ("%s Enumeration", enum_data->name);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  enum_summary_doc_string = get_doc_summary (enum_data->annotations, DOC_TYPE_DOCBOOK);
  enum_doc_string = get_doc (enum_data->annotations, DOC_TYPE_DOCBOOK);

  if (!only_sect2)
    {
      g_print ("<?xml version=\"1.0\"?>\n"
               "<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2 //EN\"\n"
               "\"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\">\n");

      g_print ("<refentry id=\"eggdbus-enummain-%s\">\n", enum_data->name);
      g_print ("  <refmeta>\n");
      g_print ("    <refentrytitle role=\"top_of_page\">%s</refentrytitle>\n", title);
      g_print ("  </refmeta>\n");
      g_print ("  <refnamediv>\n");
      g_print ("    <refname>%s</refname>\n", title);
      g_print ("    <refpurpose>%s</refpurpose>\n", enum_summary_doc_string);
      g_print ("  </refnamediv>\n");

      g_print ("  <refsect1>\n");
    }

  if (enum_data->type == ENUM_DATA_TYPE_ERROR_DOMAIN)
    {
      g_print ("    <refsect2 role=\"enum\" id=\"eggdbus-errordomain-%s\">"
               "      <title>The %s* Error Domain</title>\n",
               enum_data->maximal_dbus_prefix,
               enum_data->maximal_dbus_prefix);
    }
  else
    {
      g_print ("    <refsect2 role=\"enum\" id=\"eggdbus-enum-%s\">\n"
               "      <title>The %s %s</title>\n",
               enum_data->name,
               enum_data->name,
               type_string);
    }
  g_print ("        <para>\n");
  g_print ("          <programlisting>\n");
  switch (enum_data->type)
    {
    case ENUM_DATA_TYPE_ERROR_DOMAIN:
      g_print ("{\n");
      for (n = 0; n < enum_data->num_elements; n++)
        {
          EnumElemData *elem = enum_data->elements[n];

          if (n != 0)
            g_print (",\n");

          g_print ("  %s", elem->name);
        }
      g_print ("\n"
               "}\n");
      break;

    case ENUM_DATA_TYPE_FLAGS:
      max_len = 4;
      for (n = 0; n < enum_data->num_elements; n++)
        {
          EnumElemData *elem = enum_data->elements[n];
          len = strlen (elem->name);
          if (len > max_len)
            max_len = len;
        }

      g_print ("{\n");

      if (!flags_has_none_value_already (enum_data))
        {
          g_print ("  None %*s= 0x00000000", max_len - 4, "");
          has_none_elem = FALSE;
        }

      for (n = 0; n < enum_data->num_elements; n++)
        {
          EnumElemData *elem = enum_data->elements[n];
          if (n != 0 || !has_none_elem)
            g_print (",\n");

          len = strlen (elem->name);
          g_print ("  %s %*s= 0x%08x", elem->name, max_len - len, "", elem->value);
        }
      g_print ("\n"
               "}\n");
      break;

    case ENUM_DATA_TYPE_ENUM:
      max_len = 0;
      for (n = 0; n < enum_data->num_elements; n++)
        {
          EnumElemData *elem = enum_data->elements[n];
          len = strlen (elem->name);
          if (len > max_len)
            max_len = len;
        }

      g_print ("{\n");
      for (n = 0; n < enum_data->num_elements; n++)
        {
          EnumElemData *elem = enum_data->elements[n];
          if (n != 0)
            g_print (",\n");

          len = strlen (elem->name);
          g_print ("  %s %*s= %d", elem->name, max_len - len, "", elem->value);
        }
      g_print ("\n"
               "}\n");
      break;
    }
  g_print ("          </programlisting>\n");
  g_print ("          <para>\n");
  g_print ("%s\n", enum_doc_string);
  g_print ("          </para>\n");

  g_print ("          <variablelist role=\"constant\">\n");

  if (enum_data->type == ENUM_DATA_TYPE_FLAGS)
    {
      if (!has_none_elem)
        {
          g_print ("  <varlistentry id=\"eggdbus-constant-%s.None\" role=\"constant\">\n", enum_data->name);
          g_print ("    <term><literal>None</literal></term>\n");
          g_print ("    <listitem>\n");
          g_print ("      <para>\n");
          g_print ("No flags set.\n");
          g_print ("      </para>\n");
          g_print ("    </listitem>\n");
          g_print ("  </varlistentry>\n");
        }
    }

  for (n = 0; n < enum_data->num_elements; n++)
    {
      EnumElemData *elem = enum_data->elements[n];
      gchar *enum_doc_string;

      enum_doc_string = get_doc (elem->annotations, DOC_TYPE_DOCBOOK);

      switch (enum_data->type)
        {
        case ENUM_DATA_TYPE_ERROR_DOMAIN:
          g_print ("  <varlistentry id=\"eggdbus-constant-%s.%s\" role=\"constant\">\n", enum_data->name, elem->name);
          break;

        case ENUM_DATA_TYPE_FLAGS:
          g_print ("  <varlistentry id=\"eggdbus-constant-%s.%s\" role=\"constant\">\n", enum_data->name, elem->name);
          break;

        case ENUM_DATA_TYPE_ENUM:
          g_print ("  <varlistentry id=\"eggdbus-constant-%s.%s\" role=\"constant\">\n", enum_data->name, elem->name);
          break;
        }
      g_print ("    <term><literal>%s</literal></term>\n", elem->name);
      g_print ("    <listitem>\n");
      g_print ("      <para>\n");
      g_print ("%s\n", enum_doc_string);
      g_print ("      </para>\n");
      g_print ("    </listitem>\n");
      g_print ("  </varlistentry>\n");

      g_free (enum_doc_string);
    }
  g_print ("          </variablelist>\n");
  g_print ("        </para>\n");
  g_print ("    </refsect2>\n");

  if (!only_sect2)
    {
      g_print ("  </refsect1>\n");
      g_print ("</refentry>\n");
    }

  ret = TRUE;

  g_free (enum_doc_string);
  g_free (enum_summary_doc_string);
  g_free (title);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

gboolean
struct_generate_docbook (StructData  *struct_data,
                         gboolean     only_sect2,
                         GError     **error)
{
  gboolean ret;
  gchar *struct_summary_doc_string;
  gchar *struct_doc_string;
  guint n;
  gboolean has_none_elem;
  guint max_type_len;

  ret = FALSE;
  has_none_elem = TRUE;

  struct_summary_doc_string = get_doc_summary (struct_data->annotations, DOC_TYPE_DOCBOOK);
  struct_doc_string = get_doc (struct_data->annotations, DOC_TYPE_DOCBOOK);

  if (!only_sect2)
    {
      g_print ("<?xml version=\"1.0\"?>\n"
               "<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.1.2 //EN\"\n"
               "\"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd\">\n");

      g_print ("<refentry id=\"eggdbus-structmain-%s\">\n", struct_data->name);
      g_print ("  <refmeta>\n");
      g_print ("    <refentrytitle role=\"top_of_page\">%s Structure</refentrytitle>\n", struct_data->name);
      g_print ("  </refmeta>\n");
      g_print ("  <refnamediv>\n");
      g_print ("    <refname>%s Structure</refname>\n", struct_data->name);
      g_print ("    <refpurpose>%s</refpurpose>\n", struct_summary_doc_string);
      g_print ("  </refnamediv>\n");

      g_print ("  <refsect1>\n");
    }

  g_print ("    <refsect2 role=\"struct\" id=\"eggdbus-struct-%s\">\n"
           "      <title>The %s Structure</title>\n",
           struct_data->name,
           struct_data->name);
  g_print ("        <para>\n");
  g_print ("          <programlisting>\n");
  max_type_len = 0;
  for (n = 0; n < struct_data->num_elements; n++)
    {
      StructElemData *elem = struct_data->elements[n];
      guint type_len;
      gchar *type_name;

      docbook_get_typename_for_signature (elem->type,
                                          &type_name,
                                          NULL);
      type_len = strlen (type_name);
      g_free (type_name);

      if (type_len > max_type_len)
        max_type_len = type_len;
    }
  g_print ("{\n");
  for (n = 0; n < struct_data->num_elements; n++)
    {
      StructElemData *elem = struct_data->elements[n];
      gchar *type_name;
      gchar *type_name_link;
      guint type_len;

      if (n > 0)
        g_print (",\n");

      docbook_get_typename_for_signature (elem->type,
                                          &type_name,
                                          &type_name_link);
      type_len = strlen (type_name);

      g_print ("  %s%*s %s",
               type_name_link,
               max_type_len - type_len, "",
               elem->name);

      g_free (type_name);
      g_free (type_name_link);
    }
  g_print ("\n"
           "}\n");
  g_print ("          </programlisting>\n");
  g_print ("          <para>\n");
  g_print ("%s\n", struct_doc_string);
  g_print ("          </para>\n");

  g_print ("          <variablelist role=\"struct\">\n");

  for (n = 0; n < struct_data->num_elements; n++)
    {
      StructElemData *elem = struct_data->elements[n];
      gchar *struct_doc_string;
      gchar *type_name_link;

      struct_doc_string = get_doc (elem->annotations, DOC_TYPE_DOCBOOK);
      docbook_get_typename_for_signature (elem->type,
                                          NULL,
                                          &type_name_link);

      g_print ("  <varlistentry>\n");
      g_print ("    <term><literal>%s <structfield>%s</structfield></literal></term>\n", type_name_link, elem->name);
      g_print ("    <listitem>\n");
      g_print ("      <para>\n");
      g_print ("%s\n", struct_doc_string);
      g_print ("      </para>\n");
      g_print ("    </listitem>\n");
      g_print ("  </varlistentry>\n");

      g_free (type_name_link);
      g_free (struct_doc_string);
    }
  g_print ("          </variablelist>\n");
  g_print ("        </para>\n");
  g_print ("    </refsect2>\n");

  if (!only_sect2)
    {
      g_print ("  </refsect1>\n");
      g_print ("</refentry>\n");
    }

  ret = TRUE;

  g_free (struct_doc_string);
  g_free (struct_summary_doc_string);

  return ret;
}

