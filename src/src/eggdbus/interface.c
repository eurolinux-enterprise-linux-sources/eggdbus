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
#include "interface.h"

typedef enum {
  METHOD_TYPE_SYNC,
  METHOD_TYPE_ASYNC,
  METHOD_TYPE_ASYNC_FINISH,
  METHOD_TYPE_SERVER,
  METHOD_TYPE_SERVER_FINISH
} MethodType;

static gchar *
get_doc_for_arg (const EggDBusInterfaceArgInfo *arg)
{
  return get_doc (arg->annotations, DOC_TYPE_GTKDOC);
}

static gboolean
str_ends_with_period_or_similar (const gchar *str)
{
  gsize len;

  len = strlen (str);

  return str[len-1] == '.' || str[len-1] == '!';
}

static gboolean
print_gtkdoc_for_args (const EggDBusInterfaceArgInfo  *args,
                       guint                           num_args,
                       const gchar                    *prefix,
                       gboolean                        include_free_info,
                       guint                           indent,
                       GError                        **error)
{
  guint n;

  for (n = 0; n < num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = args + n;
      gchar *arg_name_uscore;
      gchar *arg_doc_string;
      gchar *free_info;

      arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

      arg_doc_string = get_doc_for_arg (arg);

      free_info = NULL;

      if (include_free_info)
        {
          gchar *arg_g_type_name;
          const gchar *free_function_name;

          arg_g_type_name = get_type_names_for_signature (arg->signature,
                                                          arg->annotations,
                                                          FALSE,
                                                          FALSE,
                                                          NULL,
                                                          &free_function_name,
                                                          NULL,
                                                          NULL,
                                                          error);

          if (free_function_name != NULL)
            free_info = g_strdup_printf ("%sFree with %s().",
                                         str_ends_with_period_or_similar (arg_doc_string) ? " " : ". ",
                                         free_function_name);

          g_free (arg_g_type_name);
        }

      g_print ("%*s * @%s%s: %s%s\n",
               indent, "",
               prefix,
               arg_name_uscore,
               arg_doc_string,
               free_info != NULL ? free_info : "");

      g_free (arg_doc_string);
      g_free (arg_name_uscore);
      g_free (free_info);
    }

  return TRUE;
}

static gboolean
print_method_doc (const EggDBusInterfaceMethodInfo  *method,
                  const EggDBusInterfaceInfo        *interface,
                  const gchar                       *name_space_uscore,
                  const gchar                       *iface_name_uscore,
                  const gchar                       *full_instance_name,
                  MethodType                         method_type,
                  GError                           **error)
{
  const gchar *name_suffix;
  const gchar *method_name_qualifier;
  gchar *method_name_uscore;
  gchar *method_doc_string;
  gboolean ret;

  ret = FALSE;
  method_doc_string = NULL;

  method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

  switch (method_type)
    {
    case METHOD_TYPE_SYNC:
      name_suffix = "_sync";
      method_name_qualifier = "";
      break;

    case METHOD_TYPE_ASYNC:
      name_suffix = "";
      method_name_qualifier = "";
      break;

    case METHOD_TYPE_ASYNC_FINISH:
      name_suffix = "_finish";
      method_name_qualifier = "";
      break;

    case METHOD_TYPE_SERVER_FINISH:
      name_suffix = "_finish";
      method_name_qualifier = "handle_";
      break;

    default:
      g_assert_not_reached ();
    }

  g_print ("/**\n"
           " * %s_%s_%s%s%s:\n",
           name_space_uscore,
           iface_name_uscore,
           method_name_qualifier,
           method_name_uscore,
           name_suffix);


  if (method_type == METHOD_TYPE_SYNC)
    {
      g_print (" * @instance: A #%s.\n"
               " * @call_flags: Flags from #EggDBusCallFlags detailing how the method should be invoked.\n",
               full_instance_name);

      if (!print_gtkdoc_for_args (method->in_args,
                                  method->in_num_args,
                                  "",
                                  FALSE,
                                  0,
                                  error))
        goto out;

      if (!print_gtkdoc_for_args (method->out_args,
                                  method->out_num_args,
                                  "out_",
                                  TRUE,
                                  0,
                                  error))
        goto out;

      g_print (" * @cancellable: A #GCancellable or %%NULL.\n"
               " * @error: Return location for error.\n");

      method_doc_string = get_doc (method->annotations, DOC_TYPE_GTKDOC);
      g_print (" *\n"
               " * %s\n",
               method_doc_string);

      g_print (" *\n"
               " * This function synchronously invokes the <link linkend=\"eggdbus-method-%s.%s\">%s<!-- -->()</link> method on the <link linkend=\"eggdbus-interface-%s\">%s</link> interface on the object represented by @instance.\n"
               " * See %s_%s_%s() for the asynchronous version of this function.\n",
               interface->name, method->name, method->name,
               interface->name, interface->name,
               name_space_uscore, iface_name_uscore, method_name_uscore);

    }
  else if (method_type == METHOD_TYPE_ASYNC)
    {
      g_print (" * @instance: A #%s.\n"
               " * @call_flags: Flags from #EggDBusCallFlags detailing how the method should be invoked.\n",
               full_instance_name);

      if (!print_gtkdoc_for_args (method->in_args,
                                  method->in_num_args,
                                  "",
                                  FALSE,
                                  0,
                                  error))
        goto out;

      g_print (" * @cancellable: A #GCancellable or %%NULL.\n"
               " * @callback: Callback to invoke when the reply is ready.\n"
               " * @user_data: User data to pass to @callback.\n");

      method_doc_string = get_doc (method->annotations, DOC_TYPE_GTKDOC);
      g_print (" *\n"
               " * %s\n",
               method_doc_string);

      g_print (" *\n"
               " * This function asynchronously invokes the <link linkend=\"eggdbus-method-%s.%s\">%s<!-- -->()</link> method\n"
               " * on the <link linkend=\"eggdbus-interface-%s\">%s</link> interface\n"
               " * on the object represented by @instance.\n"
               " * When the reply is ready, @callback will be called (on the main thread).\n"
               " * You can then call %s_%s_%s_finish() to get the result.\n"
               " * See %s_%s_%s_sync() for the synchronous version of this function.\n",
               interface->name, method->name, method->name,
               interface->name, interface->name,
               name_space_uscore, iface_name_uscore, method_name_uscore,
               name_space_uscore, iface_name_uscore, method_name_uscore);

    }
  else if (method_type == METHOD_TYPE_ASYNC_FINISH)
    {
      g_print (" * @instance: A #%s.\n",
               full_instance_name);

      if (!print_gtkdoc_for_args (method->out_args,
                                  method->out_num_args,
                                  "out_",
                                  TRUE,
                                  0,
                                  error))
        goto out;

      g_print (" * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed to %s_%s_%s().\n"
               " * @error: Return location for error.\n",
               name_space_uscore, iface_name_uscore, method_name_uscore);

      g_print (" *\n"
               " * Finishes an asynchronous method invocation started with %s_%s_%s().\n",
               name_space_uscore, iface_name_uscore, method_name_uscore);

    }
  else if (method_type == METHOD_TYPE_SERVER_FINISH)
    {

      if (!print_gtkdoc_for_args (method->out_args,
                                  method->out_num_args,
                                  "",
                                  FALSE,
                                  0,
                                  error))
        goto out;

      g_print (" * @method_invocation: A #EggDBusMethodInvocation.\n");

      g_print (" *\n"
               " * Function to be called by implementers of the\n"
               " * <link linkend=\"eggdbus-interface-%s\">%s</link>\n"
               " * D-Bus interface to finish handling the\n"
               " * <link linkend=\"eggdbus-method-%s.%s\">%s<!-- -->()</link> method.\n",
               interface->name, interface->name,
               interface->name, method->name, method->name);
    }

  g_print (" *\n");

  if (method_type == METHOD_TYPE_SYNC)
    {
      g_print (" * Returns: %%TRUE if the method call succeeded, %%FALSE if @error is set.\n");
    }
  else if (method_type == METHOD_TYPE_ASYNC)
    {
      g_print (" * Returns: A pending call id (never zero) that can be used with egg_dbus_connection_pending_call_cancel() or egg_dbus_connection_pending_call_block().\n");
    }
  else if (method_type == METHOD_TYPE_ASYNC_FINISH)
    {
      g_print (" * Returns: %%TRUE if the method call succeeded, %%FALSE if @error is set.\n");
    }
  else if (method_type == METHOD_TYPE_SERVER_FINISH)
    {
      /* none */
    }

  g_print (" */\n");

  ret = TRUE;

 out:
  g_free (method_name_uscore);
  g_free (method_doc_string);
  return ret;
}

static gboolean
print_method_prototype (const EggDBusInterfaceMethodInfo  *method,
                        const gchar                       *name_space_uscore,
                        const gchar                       *iface_name_uscore,
                        const gchar                       *full_instance_name,
                        MethodType                         method_type,
                        guint                              indent,
                        const gchar                       *arg_prefix,
                        gboolean                           use_name_space_and_iface_name_prefix,
                        gboolean                           ret_type_on_separate_line,
                        gboolean                           use_function_pointer,
                        gboolean                           terminate_with_semicolon,
                        GError                           **error)
{
  gboolean ret;
  const char *name_suffix;
  const char *return_ctype;
  char *method_name_prefix;
  char *method_name_qualifier;
  guint n;
  guint m;
  gchar *method_name_uscore;

  method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

  ret = FALSE;

  if (use_name_space_and_iface_name_prefix)
    {
      method_name_prefix = g_strdup_printf ("%s_%s_",
                                            name_space_uscore,
                                            iface_name_uscore);
    }
  else
    {
      method_name_prefix = g_strdup ("");
    }

  switch (method_type)
    {
    case METHOD_TYPE_SYNC:
      name_suffix = "_sync";
      return_ctype = "gboolean";
      method_name_qualifier = "";
      break;

    case METHOD_TYPE_ASYNC:
      name_suffix = "";
      return_ctype = "guint";
      method_name_qualifier = "";
      break;

    case METHOD_TYPE_ASYNC_FINISH:
      name_suffix = "_finish";
      return_ctype = "gboolean";
      method_name_qualifier = "";
      break;

    case METHOD_TYPE_SERVER:
      name_suffix = "";
      return_ctype = "void";
      method_name_qualifier = "handle_";
      break;

    case METHOD_TYPE_SERVER_FINISH:
      name_suffix = "_finish";
      return_ctype = "void";
      method_name_qualifier = "handle_";
      break;

    default:
      g_assert_not_reached ();
    }

  g_print ("%*s%s%s%s%s%s%s%s%s (\n"
           "%*s    ",
           indent, "",
           return_ctype,
           ret_type_on_separate_line ? "\n" : " ",
           use_function_pointer ? "(* " : "",
           method_name_prefix,
           method_name_qualifier,
           method_name_uscore,
           name_suffix,
           use_function_pointer ? ")" : "",
           indent, "");

  if (method_type == METHOD_TYPE_SERVER_FINISH)
    {
      g_print ("EggDBusMethodInvocation *method_invocation");
    }
  else
    {
      g_print ("%s *instance",
               full_instance_name);
    }


  if (method_type == METHOD_TYPE_SYNC ||
      method_type == METHOD_TYPE_ASYNC)
    {
      g_print (",\n"
               "%*s    EggDBusCallFlags call_flags",
               indent, "");
    }

  for (n = 0; n < 2; n++)
    {
      guint num_args;
      const EggDBusInterfaceArgInfo *args;
      gboolean is_in;

      if (n == 0)
        {
          num_args = method->in_num_args;
          args = method->in_args;
          is_in = TRUE;
        }
      else
        {
          num_args = method->out_num_args;
          args = method->out_args;
          is_in = FALSE;
        }

      for (m = 0; m < num_args; m++)
        {
          const EggDBusInterfaceArgInfo *arg = args + m;
          char *arg_g_type_name;
          gchar *arg_name_uscore;
          gchar *req_c_type_name;

          arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

          switch (method_type)
            {
            case METHOD_TYPE_SYNC:
              break;

            case METHOD_TYPE_ASYNC:
              if (!is_in)
                continue;
              break;

            case METHOD_TYPE_ASYNC_FINISH:
              if (is_in)
                continue;
              break;

            case METHOD_TYPE_SERVER:
              if (!is_in)
                continue;
              break;

            case METHOD_TYPE_SERVER_FINISH:
              if (is_in)
                continue;
              break;

            default:
              g_assert_not_reached ();
            }

          arg_g_type_name = get_type_names_for_signature (arg->signature,
                                                          arg->annotations,
                                                          is_in,
                                                          (is_in || method_type == METHOD_TYPE_SERVER_FINISH),
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          &req_c_type_name,
                                                          error);
          if (arg_g_type_name == NULL)
            goto out;

          g_print (",\n"
                   "%*s    ",
                   indent, "");

          if (is_in)
            {
              g_print ("%s%s%s",
                       req_c_type_name != NULL ? req_c_type_name : arg_g_type_name,
                       arg_prefix,
                       arg_name_uscore);
            }
          else
            {
              if (method_type == METHOD_TYPE_SERVER_FINISH)
                {
                  g_print ("%s%sout_%s",
                           req_c_type_name != NULL ? req_c_type_name : arg_g_type_name,
                           arg_prefix,
                           arg_name_uscore);
                }
              else
                {
                  g_print ("%s*%sout_%s",
                           req_c_type_name != NULL ? req_c_type_name : arg_g_type_name,
                           arg_prefix,
                           arg_name_uscore);
                }
            }

          g_free (arg_name_uscore);
          g_free (req_c_type_name);

        } /* foreach arg */
    } /* foreach {in, out} */

  switch (method_type)
    {
    case METHOD_TYPE_SYNC:
      g_print (",\n"
               "%*s    GCancellable *cancellable,\n"
               "%*s    GError **error)",
               indent, "",
               indent, "");
      break;

    case METHOD_TYPE_ASYNC:
      g_print (",\n"
               "%*s    GCancellable *cancellable,\n"
               "%*s    GAsyncReadyCallback callback,\n"
               "%*s    gpointer user_data)",
               indent, "",
               indent, "",
               indent, "");
      break;

    case METHOD_TYPE_ASYNC_FINISH:
      g_print (",\n"
               "%*s    GAsyncResult *res,\n"
               "%*s    GError **error)",
               indent, "",
               indent, "");
      break;

    case METHOD_TYPE_SERVER:
      g_print (",\n"
               "%*s    EggDBusMethodInvocation *method_invocation)",
               indent, "");
      break;

    case METHOD_TYPE_SERVER_FINISH:
      g_print (")");
      break;

    default:
      g_assert_not_reached ();
    }

  g_print ("%s\n",
           terminate_with_semicolon ? ";" : "");

  ret = TRUE;

 out:
  g_free (method_name_prefix);
  g_free (method_name_uscore);

  return ret;
}

static gboolean
print_signal_emitter_prototype (const EggDBusInterfaceSignalInfo  *signal,
                                const gchar                     *name_space_uscore,
                                const gchar                     *iface_name_uscore,
                                const gchar                     *full_instance_name,
                                gboolean                         ret_type_on_separate_line,
                                gboolean                         terminate_with_semicolon,
                                GError                         **error)
{
  gboolean ret;
  gchar *signal_name_uscore;
  guint n;


  ret = FALSE;

  signal_name_uscore = egg_dbus_utils_camel_case_to_uscore (signal->name);

  g_print ("void%s%s_%s_emit_signal_%s (\n"
           "    %s *instance,\n"
           "    const gchar *destination",
           ret_type_on_separate_line ? "\n" : " ",
           name_space_uscore,
           iface_name_uscore,
           signal_name_uscore,
           full_instance_name);

  for (n = 0; n < signal->num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = signal->args + n;
      char *arg_name_uscore;
      char *arg_g_type_name;
      gchar *req_c_type_name;

      arg_g_type_name = get_type_names_for_signature (arg->signature,
                                                      arg->annotations,
                                                      TRUE,
                                                      TRUE,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      &req_c_type_name,
                                                      error);
      if (arg_g_type_name == NULL)
        goto out;

      arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

      g_print (",\n"
               "    %s%s",
               req_c_type_name != NULL ? req_c_type_name : arg_g_type_name,
               arg_name_uscore);

      g_free (arg_name_uscore);
      g_free (req_c_type_name);
    }

  g_print (")%s\n",
           terminate_with_semicolon ? ";" : "");

  ret = TRUE;

 out:
  g_free (signal_name_uscore);

  return ret;
}


gboolean
interface_generate_iface_h_file (const EggDBusInterfaceInfo   *interface,
                                 const gchar                  *name_space,
                                 const gchar                  *iface_name,
                                 const char                   *output_name,
                                 GError                     **error)
{
  gboolean ret;
  char *name_space_uscore;
  char *name_space_hyphen;
  char *iface_name_uscore;
  char *name_space_uscore_upper;
  char *iface_name_uscore_upper;
  char *full_instance_name;
  char *full_instance_name_uscore;
  char *header_file_protection;
  guint n;
  guint m;

  ret = FALSE;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_hyphen = egg_dbus_utils_camel_case_to_hyphen (name_space);
  iface_name_uscore = egg_dbus_utils_camel_case_to_uscore (iface_name);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);
  iface_name_uscore_upper = g_ascii_strup (iface_name_uscore, -1);

  full_instance_name = g_strdup_printf ("%s%s", name_space, iface_name);
  full_instance_name_uscore = egg_dbus_utils_camel_case_to_uscore (full_instance_name);

  header_file_protection = g_strdup_printf ("__%s_%s_H",
                                            name_space_uscore_upper,
                                            iface_name_uscore_upper);

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

  /* instance / iface macros */
  g_print ("#define %s_TYPE_%s         (%s_%s_get_type())\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore,
           iface_name_uscore);
  g_print ("#define %s_%s(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), %s_TYPE_%s, %s))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper,
           full_instance_name);
  g_print ("#define %s_IS_%s(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), %s_TYPE_%s))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper);
  g_print ("#define %s_%s_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE((o), %s_TYPE_%s, %sIface))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper,
           full_instance_name);
  g_print ("\n");

  g_print ("#if 0\n"
           "typedef struct _%s%s %s%s; /* Dummy typedef */\n"
           "#endif\n"
           "typedef struct _%s%sIface %s%sIface;\n"
           "\n",
           name_space, iface_name, name_space, iface_name,
           name_space, iface_name, name_space, iface_name);

  g_print ("/**\n"
           " * %s_QUERY_INTERFACE_%s:\n"
           " * @object_proxy: A #EggDBusObjectProxy.\n"
           " *\n"
           " * Convenience macro to get an interface proxy for the remote object represented\n"
           " * by @object_proxy. See egg_dbus_object_proxy_query_interface() for details.\n"
           " *\n"
           " * Returns: An instance derived from #EggDBusInterfaceProxy that implements the\n"
           " *          #%s interface. This instance can be used to access the\n"
           " *          <link linkend=\"eggdbus-interface-%s\">%s</link> D-Bus interface on the remote\n"
           " *          object represented by @object_proxy. Do not ref or unref the returned instance,\n"
           " *          it is owned by @object_proxy.\n"
           " */\n"
           "#define %s_QUERY_INTERFACE_%s(object_proxy) (%s_%s (egg_dbus_object_proxy_query_interface (object_proxy, %s_TYPE_%s)))\n"
           "\n",
           name_space_uscore_upper, iface_name_uscore_upper,
           full_instance_name,
           interface->name, interface->name,
           name_space_uscore_upper, iface_name_uscore_upper,
           name_space_uscore_upper, iface_name_uscore_upper,
           name_space_uscore_upper, iface_name_uscore_upper);

  g_print ("/**\n"
           " * %sIface:\n"
           " * @g_iface: The parent interface.\n",
           full_instance_name);
  for (n = 0; n < interface->num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = interface->methods + n;
      gchar *method_name_uscore;
      gchar *method_doc_string;

      method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);
      method_doc_string = get_doc (method->annotations, DOC_TYPE_GTKDOC);

      g_print (" * @handle_%s: %s\n",
               method_name_uscore,
               method_doc_string);

      g_free (method_name_uscore);
      g_free (method_doc_string);
    }
  g_print (" *\n"
           " * Interface VTable for implementing the <link linkend=\"eggdbus-interface-%s\">%s</link> D-Bus interface.\n"
           " */\n",
           interface->name,
           interface->name);

  /* iface vtable */
  g_print ("struct _%sIface\n"
           "{\n"
           "  EggDBusInterfaceIface g_iface;\n"
           "\n",
           full_instance_name);

  for (n = 0; n < interface->num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = interface->methods + n;

      if (!print_method_prototype (method,
                                   name_space_uscore,
                                   iface_name_uscore,
                                   full_instance_name,
                                   METHOD_TYPE_SERVER,
                                   2,
                                   "",
                                   FALSE,
                                   FALSE,
                                   TRUE,
                                   TRUE,
                                   error))
        goto out;
      if (n != interface->num_methods - 1)
        g_print ("\n");
    }
  g_print ("};\n"
           "\n");

  /* GType function */
  g_print ("GType %s_get_type (void) G_GNUC_CONST;\n"
           "\n",
           full_instance_name_uscore);

  if (interface->num_properties > 0)
    {
      /* convenience function to override properties */
      g_print ("guint %s_%s_override_properties (GObjectClass *klass, guint property_id_begin) G_GNUC_WARN_UNUSED_RESULT;\n"
               "\n",
               name_space_uscore,
               iface_name_uscore);
    }

  /* prototypes for property getters */
  for (n = 0; n < interface->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *prop = interface->properties + n;
      char *type_name;
      char *prop_name_uscore;
      gchar *req_c_type_name;

      /* check if property is readable */
      if (! (prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE))
        continue;

      type_name = get_type_names_for_signature (prop->signature,
                                                prop->annotations,
                                                FALSE,
                                                FALSE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
          goto out;

      prop_name_uscore = egg_dbus_utils_camel_case_to_uscore (prop->name);

      g_print ("%s%s_%s_get_%s (%s *instance);\n"
               "\n",
               req_c_type_name != NULL ? req_c_type_name : type_name,
               name_space_uscore,
               iface_name_uscore,
               prop_name_uscore,
               full_instance_name);
      g_print ("\n");

      g_free (type_name);
      g_free (req_c_type_name);
      g_free (prop_name_uscore);
    }

  /* prototypes for property setters */
  for (n = 0; n < interface->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *prop = interface->properties + n;
      char *type_name;
      char *prop_name_uscore;
      gchar *req_c_type_name;

      /* check if property is readable */
      if (! (prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE))
        continue;

      type_name = get_type_names_for_signature (prop->signature,
                                                prop->annotations,
                                                TRUE,
                                                TRUE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        {
          goto out;
        }
      prop_name_uscore = egg_dbus_utils_camel_case_to_uscore (prop->name);

      g_print ("void %s_%s_set_%s (%s *instance, %svalue);\n"
               "\n",
               name_space_uscore,
               iface_name_uscore,
               prop_name_uscore,
               full_instance_name,
               req_c_type_name != NULL ? req_c_type_name : type_name);
      g_print ("\n");

      g_free (type_name);
      g_free (req_c_type_name);
      g_free (prop_name_uscore);
    }

  /* prototypes for sync methods (we want these to appear first in the gtkdoc output) */
  for (n = 0; n < interface->num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = interface->methods + n;
      if (!print_method_prototype (method,
                                   name_space_uscore,
                                   iface_name_uscore,
                                   full_instance_name,
                                   METHOD_TYPE_SYNC,
                                   0,
                                   "",
                                   TRUE,
                                   FALSE,
                                   FALSE,
                                   TRUE,
                                   error))
        goto out;
      g_print ("\n");
    }

  /* prototypes for remaining methods (async, async_finish) */
  for (n = 0; n < interface->num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = interface->methods + n;
      MethodType method_types[2] = {METHOD_TYPE_ASYNC, METHOD_TYPE_ASYNC_FINISH};

      for (m = 0; m < 2; m++)
        {
          if (!print_method_prototype (method,
                                       name_space_uscore,
                                       iface_name_uscore,
                                       full_instance_name,
                                       method_types[m],
                                       0,
                                       "",
                                       TRUE,
                                       FALSE,
                                       FALSE,
                                       TRUE,
                                       error))
            goto out;
          g_print ("\n");
        }
    }

  /* prototype for methods handle_*_finish (used only for implementing servers) */
  for (n = 0; n < interface->num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = interface->methods + n;

      if (!print_method_prototype (method,
                                   name_space_uscore,
                                   iface_name_uscore,
                                   full_instance_name,
                                   METHOD_TYPE_SERVER_FINISH,
                                   0,
                                   "",
                                   TRUE,
                                   FALSE,
                                   FALSE,
                                   TRUE,
                                   error))
        goto out;
      g_print ("\n");
    }

  /* prototypes for type-safe signal emitters */
  for (n = 0; n < interface->num_signals; n++)
    {
      const EggDBusInterfaceSignalInfo *signal = interface->signals + n;

      if (!print_signal_emitter_prototype (signal,
                                           name_space_uscore,
                                           iface_name_uscore,
                                           full_instance_name,
                                           FALSE,
                                           TRUE,
                                           error))
        goto out;
      g_print ("\n");
    }

  g_print ("G_END_DECLS\n");
  g_print ("\n");
  g_print ("#endif /* %s */\n", header_file_protection);

  ret = TRUE;

 out:

  return ret;
}

static gboolean
append_args (const EggDBusInterfaceArgInfo  *args,
             guint                         num_args,
             gboolean                      do_extract,
             gboolean                      indent,
             const gchar                  *arg_prefix_str,
             const gchar                  *error_var_name,
             const gchar                  *message_name,
             const gchar                  *name_space,
             const gchar                  *goto_out_label,
             GError                      **error)
{
  gboolean ret;
  char *name_space_uscore;
  char *name_space_uscore_upper;
  const char *operation_str;
  guint n;

  if (do_extract)
    operation_str = "extract";
  else
    operation_str = "append";

  ret = FALSE;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);

  /* append all in-args */
  for (n = 0; n < num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = args + n;
      gchar *arg_name_uscore;
      gchar *arg_type_name;
      gchar *req_c_type_name;
      gchar *arg_str;

      arg_type_name = get_type_names_for_signature (arg->signature,
                                                    arg->annotations,
                                                    FALSE,
                                                    FALSE,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    &req_c_type_name,
                                                    error);
      if (arg_type_name == NULL)
        goto out;

      arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

      if (req_c_type_name != NULL)
        {
          if (do_extract)
            {
              g_print ("%*s{\n"
                       "%*s  %stemp_value;\n",
                       indent, "",
                       indent, "",
                       arg_type_name);
              arg_str = g_strdup ("&temp_value");

              g_print ("%*sif (!egg_dbus_message_",
                       indent + 2, "");
            }
          else
            {
              arg_str = g_strdup_printf ("(%s) %s%s", arg_type_name, arg_prefix_str, arg_name_uscore);

              g_print ("%*sif (!egg_dbus_message_",
                       indent, "");
            }
        }
      else
        {
          arg_str = g_strdup_printf ("%s%s", arg_prefix_str, arg_name_uscore);

          g_print ("%*sif (!egg_dbus_message_",
                   indent, "");
        }

      if (arg->signature[0] == DBUS_TYPE_STRING)
        {
          g_print ("%s_string (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_OBJECT_PATH)
        {
          g_print ("%s_object_path (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_SIGNATURE)
        {
          g_print ("%s_signature (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_BYTE)
        {
          g_print ("%s_byte (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_INT16)
        {
          g_print ("%s_int16 (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_UINT16)
        {
          g_print ("%s_uint16 (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_INT32)
        {
          g_print ("%s_int (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_UINT32)
        {
          g_print ("%s_uint (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_INT64)
        {
          g_print ("%s_int64 (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_UINT64)
        {
          g_print ("%s_uint64 (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_DOUBLE)
        {
          g_print ("%s_double (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_BOOLEAN)
        {
          g_print ("%s_boolean (%s, %s, %s)",
                   operation_str,
                   message_name,
                   arg_str,
                   error_var_name);
        }
      else if (arg->signature[0] == DBUS_TYPE_ARRAY)
        {
          if (arg->signature[1] == DBUS_TYPE_BYTE   ||
              arg->signature[1] == DBUS_TYPE_INT16  ||
              arg->signature[1] == DBUS_TYPE_UINT16 ||
              arg->signature[1] == DBUS_TYPE_INT32  ||
              arg->signature[1] == DBUS_TYPE_UINT32 ||
              arg->signature[1] == DBUS_TYPE_INT64  ||
              arg->signature[1] == DBUS_TYPE_UINT64 ||
              arg->signature[1] == DBUS_TYPE_DOUBLE ||
              arg->signature[1] == DBUS_TYPE_BOOLEAN)
            {
              if (do_extract)
                {
                  g_print ("%s_seq (%s, %s, %s)",
                           operation_str,
                           message_name,
                           arg_str,
                           error_var_name);
                }
              else
                {
                  g_print ("%s_seq (%s, %s, \"%s\", %s)",
                           operation_str,
                           message_name,
                           arg_str,
                           arg->signature + 1,
                           error_var_name);
                }
            }
          else if (arg->signature[1] == DBUS_TYPE_STRING)
            {
              g_print ("%s_string_array (%s, %s, %s)",
                       operation_str,
                       message_name,
                       arg_str,
                       error_var_name);
            }
          else if (arg->signature[1] == DBUS_TYPE_OBJECT_PATH)
            {
              g_print ("%s_object_path_array (%s, %s, %s)",
                       operation_str,
                       message_name,
                       arg_str,
                       error_var_name);
            }
          else if (arg->signature[1] == DBUS_TYPE_SIGNATURE)
            {
              g_print ("%s_signature_array (%s, %s, %s)",
                       operation_str,
                       message_name,
                       arg_str,
                       error_var_name);
            }
          else if (arg->signature[1] == DBUS_TYPE_ARRAY ||
                   arg->signature[1] == DBUS_TYPE_VARIANT ||
                   arg->signature[1] == DBUS_STRUCT_BEGIN_CHAR)
            {
              if (do_extract)
                {
                  g_print ("%s_seq (%s, %s, %s)",
                           operation_str,
                           message_name,
                           arg_str,
                           error_var_name);
                }
              else
                {
                  g_print ("%s_seq (%s, %s, \"%s\", %s)",
                           operation_str,
                           message_name,
                           arg_str,
                           arg->signature + 1,
                           error_var_name);
                }
            }
          else if (arg->signature[1] == DBUS_DICT_ENTRY_BEGIN_CHAR)
            {
              char *val_sig;

              val_sig = g_strdup (arg->signature + 3);
              val_sig[strlen (val_sig) - 1] = '\0';

              if (do_extract)
                {
                  g_print ("%s_map (%s, %s, %s)",
                           operation_str,
                           message_name,
                           arg_str,
                           error_var_name);
                }
              else
                {
                  g_print ("%s_map (%s, %s, \"%c\", \"%s\", %s)",
                           operation_str,
                           message_name,
                           arg_str,
                           arg->signature[2],
                           val_sig,
                           error_var_name);
                }

              g_free (val_sig);
            }
          else
            {
              g_set_error (error,
                           EGG_DBUS_ERROR,
                           EGG_DBUS_ERROR_FAILED,
                           "Cannot append array signature '%s'. Please add support.",
                           arg->signature + 1);
              goto out;
            }
        }
      else if (arg->signature[0] == DBUS_STRUCT_BEGIN_CHAR)
        {
          if (do_extract)
            {
              g_print ("%s_structure (%s, (EggDBusStructure **) %s, %s)",
                       operation_str,
                       message_name,
                       arg_str,
                       error_var_name);
            }
          else
            {
              g_print ("%s_structure (%s, EGG_DBUS_STRUCTURE (%s), %s)",
                       operation_str,
                       message_name,
                       arg_str,
                       error_var_name);
            }
        }
      else if (arg->signature[0] == DBUS_TYPE_VARIANT)
        {
              g_print ("%s_variant (%s, %s, %s)",
                       operation_str,
                       message_name,
                       arg_str,
                       error_var_name);
        }
      else
        {
          g_set_error (error,
                       EGG_DBUS_ERROR,
                       EGG_DBUS_ERROR_FAILED,
                       "Cannot append signature '%s'. Please add support.",
                       arg->signature);
          goto out;
        }

      if (req_c_type_name != NULL)
        {
          if (do_extract)
            {
              g_print (")\n%*s  goto %s;\n",
                       indent + 2, "",
                       goto_out_label);

              if (arg_prefix_str[0] == '&')
                {
                  /* avoid funny (but working) constructs like
                   *
                   *   if (&_pid != NULL)
                   *     *&_pid = (pid_t ) temp_value;
                   *
                   * just use
                   *
                   *   _pid = (pid_t) temp_value;
                   *
                   * instead.
                   */
                  g_print ("%*s%s%s = (%s) temp_value;\n",
                           indent + 2, "",
                           arg_prefix_str + 1, arg_name_uscore,
                           req_c_type_name);
                }
              else
                {
                  g_print ("%*sif (%s%s != NULL)\n"
                           "%*s  *%s%s = (%s) temp_value;\n",
                           indent + 2, "",
                           arg_prefix_str, arg_name_uscore,
                           indent + 2, "",
                           arg_prefix_str, arg_name_uscore,
                           req_c_type_name);
                }

              g_print ("%*s}\n",
                       indent, "");
            }
          else
            {
              g_print (")\n%*s  goto %s;\n",
                       indent, "",
                       goto_out_label);
            }
        }
      else
        {
          g_print (")\n%*s  goto %s;\n",
                   indent, "",
                   goto_out_label);
        }

      g_free (arg_name_uscore);
      g_free (arg_type_name);
      g_free (req_c_type_name);
      g_free (arg_str);
    } /* append args */

  ret = TRUE;

 out:
  g_free (name_space_uscore);
  g_free (name_space_uscore_upper);
  return ret;
}

static void
introspection_print_args (const EggDBusInterfaceArgInfo *args,
                          guint                        num_args,
                          const gchar                 *name_prefix,
                          const gchar                 *name)
{
  guint n;

  g_print ("static const EggDBusInterfaceArgInfo arg_info_%s%s[] =\n"
           "{\n",
           name_prefix,
           name);

  for (n = 0; n < num_args; n++)
    {
      const EggDBusInterfaceArgInfo *arg = args + n;

      if (arg->name != NULL)
        {
          g_print ("  {\n"
                   "    \"%s\",\n"
                   "    \"%s\",\n",
                   arg->name,
                   arg->signature);
        }
      else
        {
          g_print ("  {\n"
                   "    NULL,\n"
                   "    NULL,\n"
                   "    \"%s\",\n",
                   arg->signature);
        }

      g_print ("    NULL,\n" /* don't print annotations */
               "  }"
               "%s\n",
               (n == num_args - 1) ? "" : ",");
    }

  g_print ("};\n"
           "\n");

}

static void
introspection_print_methods (const EggDBusInterfaceMethodInfo *methods,
                             guint                           num_methods)
{
  guint n;

  if (num_methods == 0)
    return;

  for (n = 0; n < num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = methods + n;
      gchar *method_name_uscore;

      method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

      if (method->in_num_args != 0)
        introspection_print_args (method->in_args,
                                  method->in_num_args,
                                  "method_in_",
                                  method_name_uscore);

      if (method->out_num_args != 0)
        introspection_print_args (method->out_args,
                                  method->out_num_args,
                                  "method_out_",
                                  method_name_uscore);

      g_free (method_name_uscore);
    }

  g_print ("static const EggDBusInterfaceMethodInfo method_info[] =\n"
           "{\n");

  for (n = 0; n < num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = methods + n;
      gchar *method_name_uscore;

      method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

      g_print ("  {\n"
               "    \"%s\",\n",
               method->name);

      g_print ("    \"%s\",\n"
               "    %d,\n",
               method->in_signature,
               method->in_num_args);
      if (method->in_num_args != 0)
        g_print ("    arg_info_method_in_%s,\n",
                 method_name_uscore);
      else
        g_print ("    NULL,\n");

      g_print ("    \"%s\",\n"
               "    %d,\n",
               method->out_signature,
               method->out_num_args);
      if (method->out_num_args != 0)
        g_print ("    arg_info_method_out_%s,\n",
                 method_name_uscore);
      else
        g_print ("    NULL,\n");

      g_print ("    NULL\n"); /* don't print annotations */

      g_print ("  }%s\n",
               (n == num_methods - 1) ? "" : ",");

      g_free (method_name_uscore);
    }

  g_print ("};\n"
           "\n");
}

static void
introspection_print_signals (const EggDBusInterfaceSignalInfo *signals,
                             guint                           num_signals,
                             const gchar                    *full_instance_name_hyphen)
{
  guint n;

  if (num_signals == 0)
    return;

  for (n = 0; n < num_signals; n++)
    {
      const EggDBusInterfaceSignalInfo *signal = signals + n;
      gchar *signal_name_uscore;

      signal_name_uscore = egg_dbus_utils_camel_case_to_uscore (signal->name);

      if (signal->num_args != 0)
        introspection_print_args (signal->args,
                                  signal->num_args,
                                  "signal_",
                                  signal_name_uscore);

      g_free (signal_name_uscore);
    }

  g_print ("static const EggDBusInterfaceSignalInfo signal_info[] =\n"
           "{\n");

  for (n = 0; n < num_signals; n++)
    {
      const EggDBusInterfaceSignalInfo *signal = signals + n;
      gchar *signal_name_uscore;

      signal_name_uscore = egg_dbus_utils_camel_case_to_uscore (signal->name);

      g_print ("  {\n"
               "    \"%s\",\n"
               "    \"%s\",\n",
               signal->name,
               signal->g_name);

      g_print ("    \"%s\",\n"
               "    %d,\n",
               signal->signature,
               signal->num_args);
      if (signal->num_args != 0)
        g_print ("    arg_info_signal_%s,\n",
                 signal_name_uscore);
      else
        g_print ("    NULL,\n");

      g_print ("    NULL\n"); /* don't print annotations */

      g_print ("  }%s\n",
               (n == num_signals - 1) ? "" : ",");

      g_free (signal_name_uscore);
    }

  g_print ("};\n"
           "\n");
}

static void
introspection_print_properties (const EggDBusInterfacePropertyInfo *properties,
                                guint                             num_properties,
                                const gchar                      *full_instance_name_hyphen)
{
  guint n;

  if (num_properties == 0)
    return;

  g_print ("static const EggDBusInterfacePropertyInfo property_info[] =\n"
           "{\n");

  for (n = 0; n < num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *property = properties + n;
      const gchar *flags_str;

      if ((property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE) &&
          (property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE))
        {
          flags_str = "EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE | EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE";
        }
      else if (property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE)
        {
          flags_str = "EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE";
        }
      else if (property->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE)
        {
          flags_str = "EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE";
        }
      else
        {
          g_assert_not_reached ();
        }

      g_print ("  {\n"
               "    \"%s\",\n"
               "    \"%s\",\n"
               "    \"%s\",\n"
               "    %s,\n"
               "    NULL,\n", /* don't print annotations */
               property->name,
               property->g_name,
               property->signature,
               flags_str);

      g_print ("  }%s\n",
               (n == num_properties - 1) ? "" : ",");
    }

  g_print ("};\n"
           "\n");
}


gboolean
interface_generate_iface_c_file (const EggDBusInterfaceInfo  *interface,
                                 const gchar                 *name_space,
                                 const gchar                 *iface_name,
                                 const char                  *output_name,
                                 const char                  *h_file_name,
                                 GError                     **error)
{
  gboolean ret;
  char *name_space_uscore;
  char *name_space_hyphen;
  char *iface_name_uscore;
  char *iface_name_hyphen;
  char *name_space_uscore_upper;
  char *iface_name_uscore_upper;
  char *full_instance_name;
  char *full_instance_name_uscore;
  char *full_instance_name_hyphen;
  gchar *interface_summary_doc_string;
  gchar *interface_doc_string;
  char *file_name;
  guint n;
  guint m;

  ret = FALSE;

  name_space_uscore = egg_dbus_utils_camel_case_to_uscore (name_space);
  name_space_hyphen = egg_dbus_utils_camel_case_to_hyphen (name_space);
  iface_name_uscore = egg_dbus_utils_camel_case_to_uscore (iface_name);
  iface_name_hyphen = egg_dbus_utils_camel_case_to_hyphen (iface_name);
  name_space_uscore_upper = g_ascii_strup (name_space_uscore, -1);
  iface_name_uscore_upper = g_ascii_strup (iface_name_uscore, -1);

  full_instance_name = g_strdup_printf ("%s%s", name_space, iface_name);
  full_instance_name_uscore = egg_dbus_utils_camel_case_to_uscore (full_instance_name);
  full_instance_name_hyphen = egg_dbus_utils_camel_case_to_hyphen (full_instance_name);

  interface_summary_doc_string = get_doc_summary (interface->annotations, DOC_TYPE_GTKDOC);
  interface_doc_string = get_doc (interface->annotations, DOC_TYPE_GTKDOC);

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
           "#include <string.h>\n"
           "#include <eggdbus/eggdbus.h>\n");
  print_includes (name_space, TRUE);
  print_include (name_space, "BindingsMarshal");
  g_print ("#include \"%s\"\n", h_file_name);
  g_print ("\n");

  file_name = compute_file_name (name_space, iface_name, "");
  g_print ("/**\n"
           " * SECTION:%s\n"
           " * @title: %s%s\n"
           " * @short_description: %s\n"
           " *\n"
           " * %s\n"
           " */\n"
           "\n",
           file_name,
           name_space, iface_name,
           interface_summary_doc_string,
           interface_doc_string);
  g_free (file_name);

  /* ---------------------------------------------------------------------------------------------------- */
  /* proxy class */

  g_print ("#define _%s_TYPE_%s_PROXY         (_%s_%s_proxy_get_type())\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore,
           iface_name_uscore);
  g_print ("#define _%s_%s_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), _%s_TYPE_%s_PROXY, _%sProxy))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper,
           full_instance_name);
  g_print ("#define _%s_%s_PROXY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), _%s_TYPE_%s_PROXY, _%sProxy))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper,
           full_instance_name);
  g_print ("#define _%s_%s_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), _%s_TYPE_%s_PROXY, _%sProxy))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper,
           full_instance_name);
  g_print ("#define _%s_IS_%s_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), _%s_TYPE_%s_PROXY))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper);
  g_print ("#define _%s_IS_%s_PROXY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), _%s_TYPE_%s_PROXY))\n",
           name_space_uscore_upper,
           iface_name_uscore_upper,
           name_space_uscore_upper,
           iface_name_uscore_upper);
  g_print ("\n");

  g_print ("typedef struct _%sProxy _%sProxy;\n"
           "typedef struct _%sProxyClass _%sProxyClass;\n"
           "\n",
           full_instance_name,
           full_instance_name,
           full_instance_name,
           full_instance_name);

  g_print ("struct _%sProxy\n"
           "{\n"
           "  EggDBusInterfaceProxy parent_instance;\n"
           "\n"
           "  EggDBusObjectProxy *object_proxy;\n"
           "};\n"
           "\n",
           full_instance_name);

  g_print ("struct _%sProxyClass\n"
           "{\n"
           "  EggDBusInterfaceProxyClass parent_class;\n"
           "};\n"
           "\n",
           full_instance_name);

  g_print ("GType _%s_proxy_get_type (void) G_GNUC_CONST;\n"
           "\n"
           "static void\n"
           "_proxy_interface_init (%sIface *iface)\n"
           "{\n"
           "}\n"
           "\n"
           "G_DEFINE_TYPE_WITH_CODE (_%sProxy, _%s_proxy, EGG_DBUS_TYPE_INTERFACE_PROXY,\n"
           "                         G_IMPLEMENT_INTERFACE (%s_TYPE_%s, _proxy_interface_init));\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name,
           full_instance_name,
           full_instance_name_uscore,
           name_space_uscore_upper,
           iface_name_uscore_upper);

  /* init() */
  g_print ("static void\n"
           "_%s_proxy_init (_%sProxy *interface_proxy)\n"
           "{\n"
           "}\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name);

  /* object_proxy_finalized() - warn if this happens; the user isn't supposed to ref us */
  g_print ("static void\n"
           "_%s_proxy_object_proxy_finalized (gpointer data,\n"
           "                                  GObject  *where_the_object_was)\n"
           "{\n"
           "  _%sProxy *interface_proxy;\n"
           "\n"
           "  interface_proxy = _%s_%s_PROXY (data);\n"
           "\n"
           "  g_warning (\"object_proxy for _%sProxy finalized but interface proxy still alive; you are not supposed to be reffing instances derived from EggDBusInterfaceProxy\");\n"
           "\n"
           "  interface_proxy->object_proxy = NULL;\n"
           "}\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name,
           name_space_uscore_upper, iface_name_uscore_upper,
           full_instance_name);

  /* finalize() */
  g_print ("static void\n"
           "_%s_proxy_finalize (GObject *object)\n"
           "{\n"
           "  _%sProxy *interface_proxy;\n"
           "\n"
           "  interface_proxy = _%s_%s_PROXY (object);\n"
           "\n"
           "  if (interface_proxy->object_proxy != NULL)\n"
           "    g_object_weak_unref (G_OBJECT (interface_proxy->object_proxy), _%s_proxy_object_proxy_finalized, interface_proxy);\n"
           "\n"
           "  G_OBJECT_CLASS (_%s_proxy_parent_class)->finalize (object);\n"
           "}\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name,
           name_space_uscore_upper, iface_name_uscore_upper,
           full_instance_name_uscore,
           full_instance_name_uscore);

  /* get_object_proxy() */
  g_print ("static EggDBusObjectProxy *\n"
           "_%s_proxy_get_object_proxy (EggDBusInterfaceProxy *proxy)\n"
           "{\n"
           "  _%sProxy *interface_proxy;\n"
           "\n"
           "  interface_proxy = _%s_%s_PROXY (proxy);\n"
           "\n"
           "  return interface_proxy->object_proxy;\n"
           "}\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name,
           name_space_uscore_upper, iface_name_uscore_upper);

  /* get_interface_iface() */
  g_print ("static EggDBusInterfaceIface *\n"
           "_%s_proxy_get_interface_iface (EggDBusInterfaceProxy *proxy)\n"
           "{\n"
           "  return (EggDBusInterfaceIface *) (%s_%s_GET_IFACE (proxy));\n"
           "}\n"
           "\n",
           full_instance_name_uscore,
           name_space_uscore_upper, iface_name_uscore_upper);

  /* class init() */
  g_print ("static void\n"
           "_%s_proxy_class_init (_%sProxyClass *klass)\n"
           "{\n"
           "  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);\n"
           "  EggDBusInterfaceProxyClass *interface_proxy_class = EGG_DBUS_INTERFACE_PROXY_CLASS (klass);\n"
           "\n"
           /* use property getters and setters from parent class */
           "  gobject_class->get_property = G_OBJECT_CLASS(g_type_class_peek_parent (klass))->get_property;\n"
           "  gobject_class->set_property = G_OBJECT_CLASS(g_type_class_peek_parent (klass))->set_property;\n"
           "  gobject_class->finalize = _%s_proxy_finalize;\n"
           "\n"
           "  interface_proxy_class->get_object_proxy    = _%s_proxy_get_object_proxy;\n"
           "  interface_proxy_class->get_interface_iface = _%s_proxy_get_interface_iface;\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name,
           full_instance_name_uscore,
           full_instance_name_uscore,
           full_instance_name_uscore);

  /* override properties if we have any */
  if (interface->num_properties != 0)
    {
      g_print ("  g_assert (%s_override_properties (gobject_class, 100) > 100);\n"
               "\n",
               full_instance_name_uscore);
    }

  g_print ("};\n"
           "\n");

  /* constructor (matches ->get_interface_proxy() VFunc on the EggDBusInterfaceIface VTable) */
  g_print ("static EggDBusInterfaceProxy *\n"
           "_%s_proxy_new (EggDBusObjectProxy *object_proxy)\n"
           "{\n"
           "  _%sProxy *interface_proxy;\n"
           "\n"
           "  interface_proxy = _%s_%s_PROXY (g_object_new (_%s_TYPE_%s_PROXY, NULL));\n"
           "\n"
           "  interface_proxy->object_proxy = object_proxy;\n"
           "  g_object_weak_ref (G_OBJECT (object_proxy), _%s_proxy_object_proxy_finalized, interface_proxy);\n"
           "\n"
           "  return EGG_DBUS_INTERFACE_PROXY (interface_proxy);\n"
           "};\n"
           "\n",
           full_instance_name_uscore,
           full_instance_name,
           name_space_uscore_upper, iface_name_uscore_upper,
           name_space_uscore_upper, iface_name_uscore_upper,
           full_instance_name_uscore);

  /* ---------------------------------------------------------------------------------------------------- */

  if (interface->num_signals != 0)
    {
      /* define signals */
      g_print ("enum\n"
               "{\n");
      for (n = 0; n < interface->num_signals; n++)
        {
          const EggDBusInterfaceSignalInfo *signal = interface->signals + n;
          gchar *signal_name_uscore;
          gchar *signal_name_uscore_upper;

          signal_name_uscore = egg_dbus_utils_camel_case_to_uscore (signal->name);
          signal_name_uscore_upper = g_ascii_strup (signal_name_uscore, -1);

          g_print ("  %s_SIGNAL,\n", signal_name_uscore_upper);

          g_free (signal_name_uscore);
          g_free (signal_name_uscore_upper);
        }

      g_print ("  __LAST_SIGNAL\n"
               "};\n"
               "\n"
               "static guint signals[__LAST_SIGNAL] = {0};\n"
               "\n");
    }

  introspection_print_methods (interface->methods, interface->num_methods);
  introspection_print_signals (interface->signals, interface->num_signals, full_instance_name_hyphen);
  introspection_print_properties (interface->properties, interface->num_properties, full_instance_name_hyphen);

  /* interface info */
  g_print ("static const EggDBusInterfaceInfo interface_info =\n"
           "{\n"
           "  \"%s\",\n"
           "  %d,\n"
           "  %s,\n"
           "  %d,\n"
           "  %s,\n"
           "  %d,\n"
           "  %s,\n"
           "  NULL,\n" /* no annotations */
           "};\n"
           "\n",
           interface->name,
           interface->num_methods,
           interface->num_methods == 0 ? "NULL" : "method_info",
           interface->num_signals,
           interface->num_signals == 0 ? "NULL" : "signal_info",
           interface->num_properties,
           interface->num_properties == 0 ? "NULL" : "property_info");

  g_print ("static const EggDBusInterfaceInfo *\n"
           "get_interface_info (void)\n"
           "{\n"
           "  return &interface_info;\n"
           "}\n"
           "\n");

  g_print ("static void handle_message     (EggDBusInterface        *interface,\n"
           "                                EggDBusMessage          *message);\n"
           "\n");

  g_print ("static void\n"
           "base_init (gpointer g_iface)\n"
           "{\n"
           "  static gboolean is_initialized = FALSE;\n"
           "\n"
           "  if (!is_initialized)\n"
           "    {\n"
           "      EggDBusInterfaceIface *gdbus_iface_vtable = (EggDBusInterfaceIface *) g_iface;\n"
           "\n"
           /* make sure our error domain types are registered */
           "      %s_bindings_get_error_domain_types ();\n"
           "\n"
           "      gdbus_iface_vtable->get_interface_info  = get_interface_info;\n"
           "      gdbus_iface_vtable->handle_message      = handle_message;\n"
           "      gdbus_iface_vtable->get_interface_proxy = _%s_proxy_new;\n"
           "\n"
           "\n",
           name_space_uscore,
           full_instance_name_uscore);

  /* register properties */
  for (n = 0; n < interface->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *prop = interface->properties + n;
      const gchar *access_string;
      gchar *prop_doc_string;
      CompleteType *type;

      if ((prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE) &&
          (prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE))
        {
          access_string = "G_PARAM_READWRITE";
        }
      else if (prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE)
        {
          access_string = "G_PARAM_READABLE";
        }
      else if (prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE)
        {
          access_string = "G_PARAM_WRITABLE";
        }
      else
        {
          g_assert_not_reached ();
        }

      prop_doc_string = get_doc (prop->annotations, DOC_TYPE_GTKDOC);

      g_print ("      /**\n"
               "       * %s:%s:\n"
               "       * \n"
               "       * %s\n"
               "       */ \n",
               full_instance_name,
               prop->g_name,
               prop_doc_string);

      type = get_complete_type_for_property (prop);

      if (prop->signature[0] == DBUS_STRUCT_BEGIN_CHAR)
        {
          gchar *type_name;
          gchar *g_type_name;

          /* find the proper GType instead of falling back to EGG_DBUS_TYPE_STRUCTURE */
          type_name = get_type_names_for_signature (prop->signature,
                                                    prop->annotations,
                                                    TRUE,
                                                    TRUE,
                                                    &g_type_name,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    error);
          if (type_name == NULL)
            goto out;

          g_print ("      g_object_interface_install_property (g_iface,\n"
                   "                                           g_param_spec_object (\n"
                   "                                               \"%s\",\n"
                   "                                               \"%s\",\n"
                   "                                               \"%s\",\n"
                   "                                               %s,\n",
                   prop->g_name,
                   prop->name,
                   prop_doc_string,
                   g_type_name);

          g_free (type_name);
          g_free (g_type_name);
        }
      else
        {
          EnumData *enum_data;

          if (type->user_type != NULL)
            enum_data = find_enum_by_name (type->user_type);
          else
            enum_data = NULL;

          if (enum_data != NULL && enum_data->type == ENUM_DATA_TYPE_ENUM)
            {
              g_print ("      g_object_interface_install_property (g_iface,\n"
                       "                                           g_param_spec_enum (\n"
                       "                                               \"%s\",\n"
                       "                                               \"%s\",\n"
                       "                                               \"%s\",\n"
                       "                                               %s_TYPE_%s,\n"
                       "                                               %d,\n",
                       prop->g_name,
                       prop->name,
                       prop_doc_string,
                       name_space_uscore_upper, enum_data->name_uscore_upper,
                       enum_data->elements[0]->value);
            }
          else if (enum_data != NULL && enum_data->type == ENUM_DATA_TYPE_FLAGS)
            {
              g_print ("      g_object_interface_install_property (g_iface,\n"
                       "                                           g_param_spec_flags (\n"
                       "                                               \"%s\",\n"
                       "                                               \"%s\",\n"
                       "                                               \"%s\",\n"
                       "                                               %s_TYPE_%s,\n"
                       "                                               %d,\n",
                       prop->g_name,
                       prop->name,
                       prop_doc_string,
                       name_space_uscore_upper, enum_data->name_uscore_upper,
                       enum_data->elements[0]->value);
            }
          else
            {
              g_print ("      g_object_interface_install_property (g_iface,\n"
                       "                                           egg_dbus_param_spec_for_signature (\n"
                       "                                               \"%s\",\n"
                       "                                               \"%s\",\n"
                       "                                               \"%s\",\n"
                       "                                               \"%s\",\n",
                       prop->g_name,
                       prop->name,
                       prop_doc_string,
                       prop->signature);
            }
        }

      g_print ("                                               %s |\n"
               "                                               G_PARAM_STATIC_NAME |\n"
               "                                               G_PARAM_STATIC_NICK |\n"
               "                                               G_PARAM_STATIC_BLURB));\n"
               "\n",
               access_string);

      g_free (prop_doc_string);
    }

  /* register signals */
  for (n = 0; n < interface->num_signals; n++)
    {
      const EggDBusInterfaceSignalInfo *signal = interface->signals + n;
      gchar *signal_name_uscore;
      gchar *signal_name_uscore_upper;
      const char *c_marshaller_name;
      char *signal_type_string;
      GString *s;
      char **c_type_names;
      gchar *signal_doc_string;

      signal_name_uscore = egg_dbus_utils_camel_case_to_uscore (signal->name);
      signal_name_uscore_upper = g_ascii_strup (signal_name_uscore, -1);

      c_marshaller_name = get_c_marshaller_name_for_args (signal->args, signal->num_args);

      c_type_names = g_new0 (gchar *, signal->num_args + 1);

      s = g_string_new (NULL);
      for (m = 0; m < signal->num_args; m++)
        {
          const EggDBusInterfaceArgInfo *arg = signal->args + m;
          char *gtype_name;

          c_type_names[m] = get_type_names_for_signature (arg->signature,
                                                          arg->annotations,
                                                          FALSE,
                                                          TRUE,
                                                          &gtype_name,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          error);
          if (c_type_names[m] == NULL)
            {
              g_free (signal_name_uscore);
              g_free (signal_name_uscore_upper);
              goto out;
            }

          g_string_append_printf (s,
                                  ",\n"
                                  "                        %s",
                                  gtype_name);

          g_free (gtype_name);
        }
      signal_type_string = g_string_free (s, FALSE);

      g_print ("      /**\n"
               "       * %s::%s:\n"
               "       * @instance: A #%s.\n",
               full_instance_name,
               signal->g_name,
               full_instance_name);

      if (!print_gtkdoc_for_args (signal->args,
                                  signal->num_args,
                                  "",
                                  FALSE,
                                  6,
                                  error))
        goto out;

      signal_doc_string = get_doc (signal->annotations, DOC_TYPE_GTKDOC);
      g_print ("       *\n"
               "       * %s\n",
               signal_doc_string);

      g_print ("       */\n");

      /* TODO: for now use a name of the form InterfaceName-signal until GObject is fixed */
      g_print ("      signals[%s_SIGNAL] = \n"
               "          g_signal_new (\"%s\",\n"
               "                        G_TYPE_FROM_INTERFACE (g_iface),\n"
               "                        G_SIGNAL_RUN_LAST,\n"
               "                        0,                      /* class offset     */\n"
               "                        NULL,                   /* accumulator      */\n"
               "                        NULL,                   /* accumulator data */\n"
               "                        %s,\n"
               "                        G_TYPE_NONE,\n"
               "                        %d%s",
               signal_name_uscore_upper,
               signal->g_name,
               c_marshaller_name,
               signal->num_args,
               signal_type_string);

      g_print (");\n"
               "\n");

      g_strfreev (c_type_names);
      g_free (signal_type_string);
      g_free (signal_name_uscore);
      g_free (signal_name_uscore_upper);
      g_free (signal_doc_string);
    }

  g_print ("\n"
           "      is_initialized = TRUE;\n"
           "    }\n"
           "}\n"
           "\n");

  if (interface->num_properties != 0)
    {
      gchar *first_prop_name_uscore_upper;
      gchar *last_prop_name_uscore_upper;

      first_prop_name_uscore_upper = NULL;
      last_prop_name_uscore_upper = NULL;

      /* convenience function to override properties */
      g_print ("/**\n"
               " * %s_%s_override_properties:\n"
               " * @klass: The class structure for a class deriving from #GObject and implementing #%s.\n"
               " * @property_id_begin: Property ID of first property to override.\n"
               " *\n"
               " * Convenience function to override properties for a #GObject derived class implementing #%s.\n"
               " *\n"
               " * Typical usage of this function is:\n"
               " * |[\n"
               " *   enum\n"
               " *   {\n"
               " *     PROP_0\n"
               " *     PROP_SOME_UNRELATED_PROPERTY,\n"
               " *     PROP_ANOTHER_UNRELATED_PROPERTY,\n"
               " *     ...\n"
               " *\n"
               " *     /<!-- -->* Properties from the %s interface *<!-- -->/\n",
               name_space_uscore, iface_name_uscore,
               full_instance_name,
               full_instance_name,
               full_instance_name);
      for (n = 0; n < interface->num_properties; n++)
        {
          const EggDBusInterfacePropertyInfo *prop = interface->properties + n;
          gchar *prop_name_uscore;
          gchar *prop_name_uscore_upper;

          prop_name_uscore = egg_dbus_utils_camel_case_to_uscore (prop->name);
          prop_name_uscore_upper = g_ascii_strup (prop_name_uscore, -1);

          if (first_prop_name_uscore_upper == NULL)
            first_prop_name_uscore_upper = g_strdup (prop_name_uscore_upper);

          if (n == interface->num_properties - 1)
            last_prop_name_uscore_upper = g_strdup (prop_name_uscore_upper);

          g_print (" *     PROP_%s_%s_%s,\n",
                   name_space_uscore_upper,
                   iface_name_uscore_upper,
                   prop_name_uscore_upper);

          g_free (prop_name_uscore_upper);
          g_free (prop_name_uscore);
        }

      g_print (" *\n"
               " *     ...\n"
               " *   };\n"
               " * ]|\n"
               " *\n"
               " * and then in the <literal>class_init()</literal> function:\n"
               " * |[\n"
               " *   g_assert (%s_%s_override_properties (gobject_class, PROP_%s_%s_%s) == PROP_%s_%s_%s);\n"
               " * ]|\n"
               " *\n"
               " * Returns: Property ID of the last overridden property.\n"
               " **/\n",
               name_space_uscore, iface_name_uscore,
               name_space_uscore_upper, iface_name_uscore_upper, first_prop_name_uscore_upper,
               name_space_uscore_upper, iface_name_uscore_upper, last_prop_name_uscore_upper);

      g_print ("guint\n"
               "%s_%s_override_properties (GObjectClass *klass, guint property_id_begin)\n"
               "{\n"
               "  g_return_val_if_fail (G_IS_OBJECT_CLASS (klass), 0);\n"
               "\n",
               name_space_uscore, iface_name_uscore);

      for (n = 0; n < interface->num_properties; n++)
        {
          const EggDBusInterfacePropertyInfo *prop = interface->properties + n;

          g_print ("  g_object_class_override_property (klass,\n"
                   "                                    property_id_begin%s,\n"
                   "                                    \"%s\");\n",
                   n != interface->num_properties - 1 ? "++" : "",
                   prop->g_name);

          if (n != interface->num_properties)
            g_print ("\n");
        }

      g_print ("  return property_id_begin;\n"
               "}\n"
               "\n");

      g_free (first_prop_name_uscore_upper);
      g_free (last_prop_name_uscore_upper);
    }

  g_print ("GType\n"
           "%s_%s_get_type (void)\n"
           "{\n"
           "  static GType iface_type = 0;\n"
           "\n"
           "  if (iface_type == 0)\n"
           "    {\n"
           "      static const GTypeInfo info =\n"
           "      {\n"
           "        sizeof (%s%sIface),\n"
           "        base_init,              /* base_init      */\n"
           "        NULL,                   /* base_finalize  */\n"
           "        NULL,                   /* class_init     */\n"
           "        NULL,                   /* class_finalize */\n"
           "        NULL,                   /* class_data     */\n"
           "        0,                      /* instance_size  */\n"
           "        0,                      /* n_preallocs    */\n"
           "        NULL,                   /* instance_init  */\n"
           "        NULL                    /* value_table    */\n"
           "      };\n"
           "\n"
           /* Hmm, we really want to derive from EGG_DBUS_TYPE_INTERFACE but the type system won't let us... */
           "      iface_type = g_type_register_static (G_TYPE_INTERFACE, \"%s%s\", &info, 0);\n"
           "\n"
           "      g_type_interface_add_prerequisite (iface_type, G_TYPE_OBJECT);\n"
           "    }\n"
           "\n"
           "  return iface_type;\n"
           "}\n"
           "\n",
           name_space_uscore,
           iface_name_uscore,
           name_space,
           iface_name,
           name_space,
           iface_name);

  g_print ("static void\n"
           "generic_async_callback (GObject *source_object,\n"
           "                        GAsyncResult *res,\n"
           "                        gpointer user_data)\n"
           "{\n"
           "  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);\n"
           "  EggDBusMessage *reply;\n"
           "  GError *error;\n"
           "\n"
           "  error = NULL;\n"
           "  reply = egg_dbus_connection_send_message_with_reply_finish (EGG_DBUS_CONNECTION (source_object),\n"
           "                                                              res,\n"
           "                                                              &error);\n"
           "  if (reply == NULL)\n"
           "    {\n"
           "      g_simple_async_result_set_from_error (simple, error);\n"
           "      g_error_free (error);\n"
           "    }\n"
           "  else\n"
           "    {\n"
           "      g_simple_async_result_set_op_res_gpointer (simple, reply, (GDestroyNotify) g_object_unref);\n"
           "    }\n"
           "\n"
           "  g_simple_async_result_complete (simple);\n"
           "  g_object_unref (simple);\n"
           "}\n"
           "\n");

  /* implementation for methods */
  for (n = 0; n < interface->num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = interface->methods + n;
      gchar *method_name_uscore;

      method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

      if (!print_method_doc (method,
                             interface,
                             name_space_uscore,
                             iface_name_uscore,
                             full_instance_name,
                             METHOD_TYPE_ASYNC,
                             error))
        goto out;

      /* async */
      if (!print_method_prototype (method,
                                   name_space_uscore,
                                   iface_name_uscore,
                                   full_instance_name,
                                   METHOD_TYPE_ASYNC,
                                   0,
                                   "_",
                                   TRUE,
                                   TRUE,
                                   FALSE,
                                   FALSE,
                                   error))
        goto out;

      g_print ("{\n"
               "  EggDBusObjectProxy *object_proxy;\n"
               "  EggDBusMessage *message;\n"
               "  GSimpleAsyncResult *simple;\n"
               "  GError *error;\n"
               "  guint pending_call_id;\n"
               "\n"
               "  g_return_val_if_fail (%s_IS_%s (instance) && EGG_DBUS_IS_INTERFACE_PROXY (instance), 0);\n"
               "\n"
               "  simple = g_simple_async_result_new (G_OBJECT (instance),\n"
               "                                      callback,\n"
               "                                      user_data,\n"
               "                                      %s_%s_%s);\n"
               "\n"
               "  object_proxy = egg_dbus_interface_proxy_get_object_proxy (EGG_DBUS_INTERFACE_PROXY (instance));\n"
               "\n",
               name_space_uscore_upper,
               iface_name_uscore_upper,
               name_space_uscore,
               iface_name_uscore,
               method_name_uscore);

      g_print ("  message = egg_dbus_connection_new_message_for_method_call (egg_dbus_object_proxy_get_connection (object_proxy),\n"
               "                                                             NULL,\n"
               "                                                             egg_dbus_object_proxy_get_name (object_proxy),\n"
               "                                                             egg_dbus_object_proxy_get_object_path (object_proxy),\n"
               "                                                             \"%s\",\n"
               "                                                             \"%s\");\n"
               "\n"
               "  error = NULL;\n"
               "\n",
               interface->name,
               method->name);

      if (!append_args (method->in_args, method->in_num_args, FALSE, 2, "_", "&error", "message", name_space, "out", error))
        goto out;

      g_print ("\n"
               "  pending_call_id = egg_dbus_connection_send_message_with_reply (egg_dbus_object_proxy_get_connection (object_proxy), call_flags, message, %s_bindings_get_error_domain_types (), cancellable, generic_async_callback, simple);\n"
               "\n",
               name_space_uscore);

      g_print ("  g_object_unref (message);\n"
               "  return pending_call_id;\n");

      if (method->in_num_args == 0)
        {
          g_print ("}\n"
                   "\n");
        }
      else
        {
          g_print ("out:\n"
                   "  g_simple_async_result_set_from_error (simple, error);\n"
                   "  g_simple_async_result_complete (simple);\n"
                   "  g_object_unref (simple);\n"
                   "  g_error_free (error);\n"
                   "  g_object_unref (message);\n"
                   "  return 0;\n"
                   "}\n"
                   "\n");
        }


      if (!print_method_doc (method,
                             interface,
                             name_space_uscore,
                             iface_name_uscore,
                             full_instance_name,
                             METHOD_TYPE_ASYNC_FINISH,
                             error))
        goto out;

      /* async finish */
      if (!print_method_prototype (method,
                                   name_space_uscore,
                                   iface_name_uscore,
                                   full_instance_name,
                                   METHOD_TYPE_ASYNC_FINISH,
                                   0,
                                   "_",
                                   TRUE,
                                   TRUE,
                                   FALSE,
                                   FALSE,
                                   error))
        goto out;

      g_print ("{\n"
               "  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);\n"
               "  EggDBusMessage *reply;\n"
               "  gboolean ret;\n"
               "\n"
               "  g_return_val_if_fail (%s_IS_%s (instance) && EGG_DBUS_IS_INTERFACE_PROXY (instance), FALSE);\n"
               "\n"
               "  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == %s_%s_%s);\n"
               "\n"
               "  ret = FALSE;\n"
               "  reply = NULL;\n"
               "\n",
               name_space_uscore_upper,
               iface_name_uscore_upper,
               name_space_uscore,
               iface_name_uscore,
               method_name_uscore);

      g_print ("  if (g_simple_async_result_propagate_error (simple, error))\n"
               "    goto out;\n"
               "\n");

      g_print ("  reply = EGG_DBUS_MESSAGE (g_object_ref (g_simple_async_result_get_op_res_gpointer (simple)));\n"
               "\n"
               "  if (reply == NULL)\n"
               "    {\n"
               "      g_simple_async_result_propagate_error (simple, error);\n"
               "      goto out;\n"
               "    }\n"
               "\n");

      if (!append_args (method->out_args, method->out_num_args, TRUE, 2, "_out_", "error", "reply", name_space, "out", error))
        goto out;

      g_print ("\n"
               "  ret = TRUE;\n"
               "\n"
               "out:\n"
               "  if (reply != NULL)\n"
               "    g_object_unref (reply);\n"
               "  return ret;\n"
               "}\n"
               "\n");

      if (!print_method_doc (method,
                             interface,
                             name_space_uscore,
                             iface_name_uscore,
                             full_instance_name,
                             METHOD_TYPE_SYNC,
                             error))
        goto out;

      /* sync */
      if (!print_method_prototype (method,
                                   name_space_uscore,
                                   iface_name_uscore,
                                   full_instance_name,
                                   METHOD_TYPE_SYNC,
                                   0,
                                   "_",
                                   TRUE,
                                   TRUE,
                                   FALSE,
                                   FALSE,
                                   error))
        goto out;

      g_print ("{\n"
               "  EggDBusObjectProxy *object_proxy;\n"
               "  EggDBusMessage *message;\n"
               "  EggDBusMessage *reply;\n"
               "  gboolean ret;\n"
               "\n"
               "  g_return_val_if_fail (%s_IS_%s (instance) && EGG_DBUS_IS_INTERFACE_PROXY (instance), FALSE);\n"
               "\n"
               "  ret = FALSE;"
               "  reply = NULL;"
               "\n"
               "  object_proxy = egg_dbus_interface_proxy_get_object_proxy (EGG_DBUS_INTERFACE_PROXY (instance));\n"
               "\n"
               "  message = egg_dbus_connection_new_message_for_method_call (egg_dbus_object_proxy_get_connection (object_proxy),\n"
               "                                                             NULL,\n"
               "                                                             egg_dbus_object_proxy_get_name (object_proxy),\n"
               "                                                             egg_dbus_object_proxy_get_object_path (object_proxy),\n"
               "                                                             \"%s\",\n"
               "                                                             \"%s\");\n"
               "\n",
               name_space_uscore_upper,
               iface_name_uscore_upper,
               interface->name,
               method->name);

      if (!append_args (method->in_args, method->in_num_args, FALSE, 2, "_", "error", "message", name_space, "out", error))
        goto out;

      g_print ("\n"
               "  reply = egg_dbus_connection_send_message_with_reply_sync (egg_dbus_object_proxy_get_connection (object_proxy), call_flags, message, %s_bindings_get_error_domain_types (), cancellable, error);\n"
               "  if (reply == NULL)\n"
               "    goto out;\n"
               "\n",
               name_space_uscore);

      if (!append_args (method->out_args, method->out_num_args, TRUE, 2, "_out_", "error", "reply", name_space, "out", error))
        goto out;

      g_print ("\n"
               "  ret = TRUE;\n"
               "\n"
               "out:\n"
               "  if (message != NULL)\n"
               "    g_object_unref (message);\n"
               "  if (reply != NULL)\n"
               "    g_object_unref (reply);\n"
               "  return ret;\n"
               "}\n"
               "\n");

      g_free (method_name_uscore);
    }

  /* implementation for property getters */
  for (n = 0; n < interface->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *prop = interface->properties + n;
      char *type_name;
      char *prop_name_uscore;
      const char *free_function_name;
      gchar *req_c_type_name;

      /* check if property is readable */
      if (! (prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_READABLE))
        continue;

      type_name = get_type_names_for_signature (prop->signature,
                                                prop->annotations,
                                                FALSE,
                                                FALSE,
                                                NULL,
                                                &free_function_name,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      prop_name_uscore = egg_dbus_utils_camel_case_to_uscore (prop->name);

      g_print ("/**\n"
               " * %s_%s_get_%s:\n"
               " * @instance: A #%s.\n"
               " *\n"
               " * C getter for the #%s:%s property.\n"
               " *\n"
               " * Returns: The value of the #%s:%s property.\n"
               " */\n",
               name_space_uscore, iface_name_uscore, prop_name_uscore,
               full_instance_name,
               full_instance_name, prop->g_name,
               full_instance_name, prop->g_name);

      g_print ("%s\n"
               "%s_%s_get_%s (%s *instance)\n",
               req_c_type_name != NULL ? req_c_type_name : type_name,
               name_space_uscore,
               iface_name_uscore,
               prop_name_uscore,
               full_instance_name);

      g_print ("{\n"
               "  %svalue;\n"
               "\n"
               "  g_return_val_if_fail (%s_IS_%s (instance), %s);\n"
               "\n"
               "  g_object_get (instance, \"%s\", &value, NULL);\n"
               "\n"
               "  return (%s) value;\n"
               "}\n"
               "\n",
               type_name,
               name_space_uscore_upper,
               iface_name_uscore_upper,
               free_function_name != NULL ? "NULL" : "0",
               prop->g_name,
               req_c_type_name != NULL ? req_c_type_name : type_name);

      g_free (type_name);
      g_free (req_c_type_name);
      g_free (prop_name_uscore);
    }

  /* implementation for property getters */
  for (n = 0; n < interface->num_properties; n++)
    {
      const EggDBusInterfacePropertyInfo *prop = interface->properties + n;
      char *type_name;
      char *prop_name_uscore;
      gchar *req_c_type_name;

      /* check if property is writable */
      if (! (prop->flags & EGG_DBUS_INTERFACE_PROPERTY_INFO_FLAGS_WRITABLE))
        continue;

      type_name = get_type_names_for_signature (prop->signature,
                                                prop->annotations,
                                                TRUE,
                                                TRUE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &req_c_type_name,
                                                error);
      if (type_name == NULL)
        goto out;

      prop_name_uscore = egg_dbus_utils_camel_case_to_uscore (prop->name);

      g_print ("/**\n"
               " * %s_%s_set_%s:\n"
               " * @instance: A #%s.\n"
               " * @value: New value for the #%s:%s property.\n"
               " *\n"
               " * C setter for the #%s:%s property.\n"
               " */\n",
               name_space_uscore, iface_name_uscore, prop_name_uscore,
               full_instance_name,
               full_instance_name, prop->g_name,
               full_instance_name, prop->g_name);

      g_print ("void\n"
               "%s_%s_set_%s (%s *instance, %svalue)\n",
               name_space_uscore,
               iface_name_uscore,
               prop_name_uscore,
               full_instance_name,
               req_c_type_name != NULL ? req_c_type_name : type_name);

      g_print ("{\n"
               "  g_return_if_fail (%s_IS_%s (instance));\n"
               "\n"
               "  g_object_set (instance, \"%s\", (%s) value, NULL);\n"
               "}\n"
               "\n",
               name_space_uscore_upper,
               iface_name_uscore_upper,
               prop->g_name,
               type_name);

      g_free (type_name);
      g_free (req_c_type_name);
      g_free (prop_name_uscore);
    }

  /* dispatch D-Bus signals via the GObject signal system */
  if (interface->num_signals != 0)
    {
      g_print ("static void\n"
               "handle_signal (EggDBusInterface *interface,\n"
               "               EggDBusMessage   *message)\n"
               "{\n"
               "  guint n;\n"
               "  guint num_args;\n"
               "  guint signal_id;\n"
               "  const gchar *expected_signature;\n"
               "  const gchar *signature;\n"
               "  const gchar *signal_name;\n"
               "  GValue *instance_and_params;\n"
               "\n"
               "  signature = egg_dbus_message_get_signature (message);\n"
               "  signal_name = egg_dbus_message_get_signal_name (message);\n"
               "\n");

      /* handle signals */
      for (n = 0; n < interface->num_signals; n++)
        {
          const EggDBusInterfaceSignalInfo *signal = interface->signals + n;
          gchar *signal_name_uscore;
          gchar *signal_name_uscore_upper;

          signal_name_uscore = egg_dbus_utils_camel_case_to_uscore (signal->name);
          signal_name_uscore_upper = g_ascii_strup (signal_name_uscore, -1);

          g_print ("  %sif (strcmp (signal_name, \"%s\") == 0)\n"
                   "    {\n"
                   "      expected_signature = \"%s\";\n"
                   "      if (strcmp (signature, expected_signature) != 0)\n"
                   "        goto wrong_signature;\n"
                   "      signal_id = signals[%s_SIGNAL];\n"
                   "      num_args = %d;\n"
                   "      instance_and_params = g_new0 (GValue, num_args + 1);\n",
                   n == 0 ? "" : "else ",
                   signal->name,
                   signal->signature != NULL ? signal->signature : "",
                   signal_name_uscore_upper,
                   signal->num_args);

          /* we need to upcast from EggDBusStructure to a possibly derived GType; see
           * egg_dbus_interface_proxy_get_property() for similar treatment for properties
           */
          for (m = 0; m < signal->num_args; m++)
            {
              const EggDBusInterfaceArgInfo *arg = signal->args + m;
              gchar *type_name;
              gchar *gtype_name;

              type_name = get_type_names_for_signature (arg->signature,
                                                        arg->annotations,
                                                        FALSE,
                                                        TRUE,
                                                        &gtype_name,
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        error);

              g_print ("      instance_and_params[%d].g_type = %s;\n",
                       m + 1,
                       gtype_name);

              g_free (type_name);
              g_free (gtype_name);
            }

          g_print ("    }\n");

          g_free (signal_name_uscore);
          g_free (signal_name_uscore_upper);
        }

      g_print ("  else\n"
               "    {\n"
               "      g_warning (\"%%s: Ignoring unknown signal '%%s' on interface '%%s' with signature '%%s'\", G_STRFUNC, signal_name, interface_info.name, signature);\n"
               "     goto out;\n"
               "    }\n"
               "\n"
               "  g_value_init (&(instance_and_params[0]), EGG_DBUS_TYPE_INTERFACE_PROXY);\n"
               "  g_value_set_object (&(instance_and_params[0]), EGG_DBUS_INTERFACE_PROXY (interface));\n"
               "\n"
               "  for (n = 0; n < num_args; n++)\n"
               "    {\n"
               "      GType saved_type = instance_and_params[n + 1].g_type;\n"
               /* Technically this can't fail if the signature checks out. So play hardball and assert() */
               "      instance_and_params[n + 1].g_type = 0;\n"
               "      g_assert (egg_dbus_message_extract_gvalue (message,\n"
               "                                                 &(instance_and_params[n + 1]),\n"
               "                                                 NULL));\n"
               "      instance_and_params[n + 1].g_type = saved_type;\n"
               "    }\n");

      g_print ("\n"
               "  g_signal_emitv (instance_and_params,\n"
               "                  signal_id,\n"
               "                  0,\n"
               "                  NULL);\n"
               "\n"
               "  for (n = 0; n < num_args + 1; n++)\n"
               "    g_value_unset (&(instance_and_params[n]));\n"
               "  g_free (instance_and_params);\n"
               "\n"
               "out:\n"
               "    return;\n"
               "\n"
               "wrong_signature:\n"
               "  g_warning (\"%%s: Ignoring signal '%%s' on interface '%%s' with malformed signature '%%s', expected signature '%%s'\", G_STRFUNC, signal_name, interface_info.name, signature, expected_signature);\n"
               "}\n"
               "\n");
    }
  else
    {
      g_print ("static void\n"
               "handle_signal (EggDBusInterface *interface,\n"
               "               EggDBusMessage   *message)\n"
               "{\n"
               "  g_warning (\"%%s: Ignoring unknown signal '%%s' on interface '%%s' with signature '%%s'\", G_STRFUNC, egg_dbus_message_get_signal_name (message), interface_info.name, egg_dbus_message_get_signature (message));\n"
               "}\n"
               "\n");
    }

  /* dispatch incoming method calls via the interface VTable */
  if (interface->num_methods != 0)
    {
      gboolean has_method_with_in_args;

      has_method_with_in_args = FALSE;

      g_print ("static void\n"
               "handle_method_call (EggDBusInterface  *interface,\n"
               "                    EggDBusMessage    *message)\n"
               "{\n"
               "  GError *error;\n"
               "  const gchar *signature;\n"
               "  const gchar *method_name;\n"
               "  const gchar *expected_signature;\n"
               "  %sIface *iface;\n"
               "  EggDBusMethodInvocation *method_invocation;\n"
               "\n"
               "  error = NULL;\n"
               "\n"
               "  signature = egg_dbus_message_get_signature (message);\n"
               "  method_name = egg_dbus_message_get_method_name (message);\n"
               "  iface = %s_%s_GET_IFACE (interface);\n"
               "\n",
               full_instance_name,
               name_space_uscore_upper,
               iface_name_uscore_upper);

      for (n = 0; n < interface->num_methods; n++)
        {
          const EggDBusInterfaceMethodInfo *method = interface->methods + n;
          gchar *method_name_uscore;

          method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

          g_print ("  %sif (strcmp (method_name, \"%s\") == 0)\n"
                   "    {\n",
                   n == 0 ? "" : "else ",
                   method->name);

          for (m = 0; m < method->in_num_args; m++)
            {
              const EggDBusInterfaceArgInfo *arg = method->in_args + m;
              gchar *arg_g_type_name;
              gchar *arg_name_uscore;
              gchar *req_c_type_name;

              arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

              arg_g_type_name = get_type_names_for_signature (arg->signature,
                                                              arg->annotations,
                                                              FALSE,
                                                              FALSE,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              &req_c_type_name,
                                                              error);
              if (arg_g_type_name == NULL)
                goto out;

              g_print ("      %s_%s;\n",
                       req_c_type_name != NULL ? req_c_type_name : arg_g_type_name,
                       arg_name_uscore);

              g_free (arg_g_type_name);
              g_free (arg_name_uscore);
              g_free (req_c_type_name);
            }

          if (method->in_num_args != 0)
            g_print ("\n");

          g_print ("      expected_signature = \"%s\";\n"
                   "      if (strcmp (signature, expected_signature) != 0)\n"
                   "        goto wrong_signature;\n"
                   "\n",
                   method->in_signature != NULL ? method->in_signature : "");

          g_print ("      if (iface->handle_%s == NULL)\n"
                   "        goto not_implemented;\n"
                   "\n",
                   method_name_uscore);

          if (!append_args (method->in_args, method->in_num_args, TRUE, 6, "&_", "&error", "message", name_space, "extraction_error", error))
            goto out;

          if (method->in_num_args != 0)
            g_print ("\n");

          g_print ("      method_invocation = egg_dbus_method_invocation_new (message,\n"
                   "                                                          %s_%s_handle_%s_finish);\n"
                   "\n",
                   name_space_uscore,
                   iface_name_uscore,
                   method_name_uscore);

          /* add destroy notifiers to free data when method_invocation is finalized */
          for (m = 0; m < method->in_num_args; m++)
            {
              const EggDBusInterfaceArgInfo *arg = method->in_args + m;
              gchar *arg_g_type_name;
              const char *free_function_name;
              gchar *arg_name_uscore;

              arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

              arg_g_type_name = get_type_names_for_signature (arg->signature,
                                                              arg->annotations,
                                                              FALSE,
                                                              FALSE,
                                                              NULL,
                                                              &free_function_name,
                                                              NULL,
                                                              NULL,
                                                              error);
              if (arg_g_type_name == NULL)
                goto out;

              if (free_function_name != NULL)
                {
                  g_print ("      egg_dbus_method_invocation_add_destroy_notify (method_invocation,\n"
                           "                                                   _%s,\n"
                           "                                                   (GDestroyNotify) %s);\n",
                           arg_name_uscore,
                           free_function_name);
                }

              g_free (arg_g_type_name);
              g_free (arg_name_uscore);
            }

          if (method->in_num_args > 0)
            g_print ("\n");

          /* and finally invoke the method */
          g_print ("      iface->handle_%s (%s_%s (interface)",
                   method_name_uscore,
                   name_space_uscore_upper,
                   iface_name_uscore_upper);
          for (m = 0; m < method->in_num_args; m++)
            {
              const EggDBusInterfaceArgInfo *arg = method->in_args + m;
              gchar *arg_name_uscore;

              arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

              g_print (", _%s",
                       arg_name_uscore);

              g_free (arg_name_uscore);
            }
          g_print (", method_invocation);\n\n");

          g_print ("    }\n");

          if (method->in_num_args > 0)
            has_method_with_in_args = TRUE;

          g_free (method_name_uscore);
        }

      g_print ("  else\n"
               "    {\n"
               "      g_warning (\"%%s: Ignoring unknown method call '%%s' on interface '%%s' with signature '%%s'\", G_STRFUNC, method_name, interface_info.name, signature);\n"
               "    }\n"
               "\n"
               "  return;\n"
               "\n"
               "not_implemented:\n"
               "  g_warning (\"%%s: Method call '%%s' on interface '%%s' with signature '%%s' is not implemented on GObject class %%s\", G_STRFUNC, method_name, interface_info.name, signature, g_type_name (G_TYPE_FROM_INSTANCE (interface)));\n"
               "  return;\n"
               "\n"
               "wrong_signature:\n"
               "  g_warning (\"%%s: Ignoring method call '%%s' on interface '%%s' with malformed signature '%%s', expected signature '%%s'\", G_STRFUNC, method_name, interface_info.name, signature, expected_signature);\n");

      if (has_method_with_in_args)
        {
          g_print ("  return;\n"
                   "\n"
                   "extraction_error:\n"
                   "  g_warning (\"%%s: Error extracting arguments for method call '%%s' on interface '%%s' with signature '%%s': %%s\", G_STRFUNC, method_name, interface_info.name, signature, error->message);\n"
                   "  g_error_free (error);\n"
                   "}\n"
                   "\n");
        }
      else
        {
          g_print ("}\n"
                   "\n");
        }
    }
  else
    {
      g_print ("static void\n"
               "handle_method_call (EggDBusInterface *interface,\n"
               "                    EggDBusMessage   *message)\n"
               "\n"
               "{\n"
               "  g_warning (\"%%s: Ignoring unknown method call '%%s' on interface '%%s' with signature '%%s'\", G_STRFUNC, egg_dbus_message_get_method_name (message), interface_info.name, egg_dbus_message_get_signature (message));\n"
               "}\n"
               "\n");
    }

  /* provide the <namespace>_<class>_handle_<method>_finish() functions for completion of
   * server method call invocations
   */
  for (n = 0; n < interface->num_methods; n++)
    {
      const EggDBusInterfaceMethodInfo *method = interface->methods + n;
      gchar *method_name_uscore;

      method_name_uscore = egg_dbus_utils_camel_case_to_uscore (method->name);

      if (!print_method_doc (method,
                             interface,
                             name_space_uscore,
                             iface_name_uscore,
                             full_instance_name,
                             METHOD_TYPE_SERVER_FINISH,
                             error))
        goto out;

      if (!print_method_prototype (method,
                                   name_space_uscore,
                                   iface_name_uscore,
                                   full_instance_name,
                                   METHOD_TYPE_SERVER_FINISH,
                                   0,
                                   "_",
                                   TRUE,
                                   TRUE,
                                   FALSE,
                                   FALSE,
                                   error))
        goto out;

      g_print ("{\n"
               "  GError *error;\n"
               "  EggDBusMessage *reply;\n"
               "\n"
               "  error = NULL;\n"
               "\n"
               "  g_warn_if_fail (egg_dbus_method_invocation_get_source_tag (method_invocation) ==\n"
               "                  %s_%s_handle_%s_finish);\n"
               "\n"
               "  reply = egg_dbus_method_invocation_create_reply_message (method_invocation);\n"
               "\n",
               name_space_uscore,
               iface_name_uscore,
               method_name_uscore);

      if (!append_args (method->out_args, method->out_num_args, FALSE, 2, "_out_", "&error", "reply", name_space, "malformed", error))
        goto out;

      g_print ("\n"
               "  egg_dbus_connection_send_message (egg_dbus_message_get_connection (reply), reply);\n"
               "\n"
               "  g_object_unref (reply);\n"
               "  g_object_unref (method_invocation);\n"
               "\n"
               "  return;\n");

      if (method->out_num_args == 0)
        {
          g_print ("}\n"
                   "\n");
        }
      else
        {
          g_print ("\n"
                   "malformed:\n"
                   "  g_warning (\"%%s: Malformed data passed: %%s\", G_STRFUNC, error->message);\n"
                   "  g_error_free (error);\n"
                   "}\n"
                   "\n");
        }

      g_free (method_name_uscore);
    }

  g_print ("static void\n"
           "handle_message (EggDBusInterface *interface,\n"
           "                EggDBusMessage   *message)\n"
           "{\n"
           "\n"
           "  switch (egg_dbus_message_get_message_type (message))\n"
           "    {\n"
           "    case EGG_DBUS_MESSAGE_TYPE_SIGNAL:\n"
           "      handle_signal (interface, message);\n"
           "      break;\n"
           "\n"
           "    case EGG_DBUS_MESSAGE_TYPE_METHOD_CALL:\n"
           "      handle_method_call (interface, message);\n"
           "      break;\n"
           "\n"
           "    default:\n"
           "      g_assert_not_reached ();\n"
           "      break;\n"
           "    }\n"
           "}\n"
           "\n");

  /* implementation for type-safe signal emitters */
  for (n = 0; n < interface->num_signals; n++)
    {
      const EggDBusInterfaceSignalInfo *signal = interface->signals + n;
      gchar *signal_name_uscore;

      signal_name_uscore = egg_dbus_utils_camel_case_to_uscore (signal->name);

      g_print ("/**\n"
               " * %s_%s_emit_signal_%s:\n"
               " * @instance: A #GObject derived type implementing the #%s interface.\n"
               " * @destination: The destination of the signal or %%NULL to emit signals to all listeners.\n",
               name_space_uscore,
               iface_name_uscore,
               signal_name_uscore,
               full_instance_name);

      if (!print_gtkdoc_for_args (signal->args,
                                  signal->num_args,
                                  "",
                                  FALSE,
                                  0,
                                  error))
        goto out;


      g_print (" *\n"
               " * Type safe wrapper for emitting the #%s::%s signal.\n"
               " *\n"
               " **/\n",
               full_instance_name, signal->g_name);

      if (!print_signal_emitter_prototype (signal,
                                           name_space_uscore,
                                           iface_name_uscore,
                                           full_instance_name,
                                           TRUE,
                                           FALSE,
                                           error))
        goto out;

      g_print ("{\n"
               "  g_return_if_fail (%s_IS_%s (instance));\n"
               "\n",
               name_space_uscore_upper, iface_name_uscore_upper);

      /* TODO: handle destination_name as signal detail? */

      g_print ("  g_signal_emit_by_name (instance,\n"
               "                         \"%s\"",
               signal->g_name);

      for (m = 0; m < signal->num_args; m++)
        {
          const EggDBusInterfaceArgInfo *arg = signal->args + m;
          gchar *arg_name_uscore;
          gchar *arg_type_name;
          gchar *req_c_type_name;

          arg_type_name = get_type_names_for_signature (arg->signature,
                                                        arg->annotations,
                                                        FALSE,
                                                        FALSE,
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        &req_c_type_name,
                                                        error);
          if (arg_type_name == NULL)
            goto out;

          arg_name_uscore = egg_dbus_utils_camel_case_to_uscore (arg->name);

          if (req_c_type_name != NULL)
            {
              g_print (",\n"
                       "                         (%s) %s",
                       arg_type_name,
                       arg_name_uscore);
            }
          else
            {
              g_print (",\n"
                       "                         %s",
                       arg_name_uscore);
            }

          g_free (arg_name_uscore);
          g_free (arg_type_name);
          g_free (req_c_type_name);
        }
      g_print (");\n");

      g_print ("}\n"
               "\n");

      g_free (signal_name_uscore);
    }

  ret = TRUE;
  goto out;

 out:

  g_free (full_instance_name_hyphen);
  g_free (full_instance_name_uscore);
  g_free (full_instance_name);
  g_free (name_space_hyphen);
  g_free (name_space_uscore);
  g_free (iface_name_uscore);
  g_free (name_space_uscore_upper);
  g_free (iface_name_uscore_upper);
  g_free (iface_name_hyphen);
  g_free (interface_summary_doc_string);
  g_free (interface_doc_string);

  return ret;
}
