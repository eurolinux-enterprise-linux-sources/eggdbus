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
#include <stdlib.h>
#include <string.h>
#include <eggdbus/eggdbuserror.h>
#include <eggdbus/eggdbusprivate.h>

/**
 * SECTION:eggdbuserror
 * @title: EggDBusError
 * @short_description: Error helper functions
 * @see_also: #EggDBusConnection
 *
 * Contains error helper functions for reporting and checking errors.
 */

GQuark
egg_dbus_error_quark (void)
{
  /* instead of using the traditional "<namespace>-<type>-quark" style
   * use the same name as for the registered GEnumClass type so we
   * can easily map back to the quark name.
   */
  return g_quark_from_static_string ("EggDBusError");
}

/**
 * egg_dbus_error_get_remote_exception:
 * @error: A #GError.
 * @out_name: Return location for D-Bus error name or %NULL.
 * @out_message: Return location for D-Bus error message or %NULL.
 *
 * Analyzes @error and if the error domain is #EGG_DBUS_ERROR and
 * error code is #EGG_DBUS_ERROR_REMOTE_EXCEPTION, extracts the D-Bus
 * error name (e.g. <literal>com.example.Acme.Error.Failed</literal>)
 * in @out_name and the D-Bus error message in @out_message and
 * returns %TRUE.
 *
 * Note that this function will not warn if @error isn't a
 * #EGG_DBUS_ERROR_REMOTE_EXCEPTION (it will just return %FALSE) so it
 * can be used to test if @error is really a
 * #EGG_DBUS_ERROR_REMOTE_EXCEPTION.
 *
 * Returns: %TRUE if @out_name and @out_message is set (caller must
 * free these using g_free()), %FALSE if error is not a
 * #EGG_DBUS_ERROR_REMOTE_EXCEPTION.
 **/
gboolean
egg_dbus_error_get_remote_exception (GError  *error,
                                     char   **out_name,
                                     char   **out_message)
{
  char *s;
  char *p;

  g_return_val_if_fail (error != NULL, FALSE);

  if (error->domain != EGG_DBUS_ERROR ||
      error->code != EGG_DBUS_ERROR_REMOTE_EXCEPTION)
    return FALSE;

  if (out_name != NULL)
    *out_name = NULL;
  if (out_message != NULL)
    *out_message = NULL;

  s = strrchr (error->message, ' ');
  if (s == NULL || s == error->message)
    {
      g_warning ("message '%s' is malformed", error->message);
      goto out;
    }
  if (out_message != NULL)
    *out_message = g_uri_unescape_string (s + 1, NULL);

  p = s;
  s--;
  while (*s != ' ')
    {
      if (s < error->message)
        {
          g_warning ("message '%s' is malformed.", error->message);
          goto out;
        }
      s--;
    }
  if (out_name != NULL)
    *out_name = g_uri_unescape_segment (s + 1, p, NULL);

 out:

  return TRUE;
}

gchar *
_egg_dbus_error_encode_gerror (GError *error)
{
  const gchar *domain_as_string;
  gchar *ret;
  GString *s;
  guint n;
  GType enum_type;

  ret = NULL;

  /* We can't assume that two different processes use the same Quark integer
   * value for a given string. So send the string across the wire.
   */
  domain_as_string = g_quark_to_string (error->domain);

  /* Also use the nick-name to produce error names compatible with
   * non-GLib applications; note that EggDBusError may not have
   * been initialized so special case that.
   */
  if (strcmp (domain_as_string, "EggDBusError") == 0)
    enum_type = EGG_TYPE_DBUS_ERROR;
  else
    enum_type = g_type_from_name (domain_as_string);
  if (enum_type != 0)
    {
      GEnumClass *enum_klass;
      GEnumValue *enum_value;

      enum_klass = g_type_class_ref (enum_type);
      enum_value = g_enum_get_value (enum_klass, error->code);
      g_type_class_unref (enum_klass);
      if (enum_value != NULL)
        {
          /* TODO: it's the users own problem if value_nick isn't a proper D-Bus
           * error name. Or is it?
           */
          ret = g_strdup (enum_value->value_nick);
          goto out;
        }
    }

  /* We can't make a lot of assumptions about what domain_as_string
   * looks like and D-Bus is extremely picky about error names so
   * hex-encode it for transport across the wire.
   */

  s = g_string_new ("org.gtk.EggDBus.UnmappedGError.Quark0x");
  for (n = 0; domain_as_string[n] != 0; n++)
    {
      guint nibble_top;
      guint nibble_bottom;

      nibble_top = ((int) domain_as_string[n]) >> 4;
      nibble_bottom = ((int) domain_as_string[n]) & 0x0f;

      if (nibble_top < 10)
        nibble_top += '0';
      else
        nibble_top += 'a' - 10;

      if (nibble_bottom < 10)
        nibble_bottom += '0';
      else
        nibble_bottom += 'a' - 10;

      g_string_append_c (s, nibble_top);
      g_string_append_c (s, nibble_bottom);
    }
  g_string_append_printf (s, ".Code%d", error->code);

  ret = g_string_free (s, FALSE);

 out:
  return ret;
}

gboolean
_egg_dbus_error_decode_gerror (const gchar *dbus_name,
                               GQuark      *out_error_domain,
                               gint        *out_error_code)
{
  gboolean ret;
  guint n;
  GString *s;
  gchar *domain_quark_string;

  ret = FALSE;
  s = NULL;

  if (g_str_has_prefix (dbus_name, "org.gtk.EggDBus.UnmappedGError.Quark0x"))
    {
      s = g_string_new (NULL);

      for (n = sizeof "org.gtk.EggDBus.UnmappedGError.Quark0x" - 1;
           dbus_name[n] != '.' && dbus_name[n] != '\0';
           n++)
        {
          guint nibble_top;
          guint nibble_bottom;

          nibble_top = dbus_name[n];
          if (nibble_top >= '0' && nibble_top <= '9')
            nibble_top -= '0';
          else if (nibble_top >= 'a' && nibble_top <= 'f')
            nibble_top -= ('a' - 10);
          else
            goto not_mapped;

          n++;

          nibble_bottom = dbus_name[n];
          if (nibble_bottom >= '0' && nibble_bottom <= '9')
            nibble_bottom -= '0';
          else if (nibble_bottom >= 'a' && nibble_bottom <= 'f')
            nibble_bottom -= ('a' - 10);
          else
            goto not_mapped;

          g_string_append_c (s, (nibble_top<<4) | nibble_bottom);

        }

      if (!g_str_has_prefix (dbus_name + n, ".Code"))
        goto not_mapped;

      domain_quark_string = g_string_free (s, FALSE);
      s = NULL;

      if (out_error_domain != NULL)
        *out_error_domain = g_quark_from_string (domain_quark_string);
      g_free (domain_quark_string);

      if (out_error_code != NULL)
        *out_error_code = atoi (dbus_name + n + sizeof ".Code" - 1);

      ret = TRUE;
    }

 not_mapped:

  if (s != NULL)
    g_string_free (s, TRUE);

  return ret;
}


static GError *
egg_dbus_error_new_remote_exception_valist (const gchar   *name,
                                            const gchar   *message,
                                            GType         *error_types,
                                            const gchar   *format,
                                            va_list        va_args)
{
  GString *s;
  gchar *literal_error;
  gchar *name_escaped;
  gchar *message_escaped;
  GError *error;
  GQuark error_domain;
  gint error_code;
  GEnumClass *enum_klass;
  GEnumValue *enum_value;
  guint n;

  literal_error = g_strdup_vprintf (format, va_args);

  /* Unmapped GError's from GLib peers are encoded in a special format can be mapped
   * back to the right domain and code; see _egg_dbus_error_encode_gerror()
   */
  if (_egg_dbus_error_decode_gerror (name, &error_domain, &error_code))
    goto mapped;

  error_domain = EGG_DBUS_ERROR;

  /* Otherwise lookup our registered error types
   *
   *   TODO: reading the GObject sources, g_enum_get_value_by_nick is O(n); maybe cache in a hash
   *         table if performance is a concern? It probably isn't; errors should be rare...
   *
   * First check our built-in error type...
   */
  enum_klass = g_type_class_ref (EGG_TYPE_DBUS_ERROR);
  enum_value = g_enum_get_value_by_nick (enum_klass, name);
  g_type_class_unref (enum_klass);
  if (enum_value != NULL)
    {
      error_code = enum_value->value;
      goto mapped;
    }
  /* then check all error domains the user passed in (via e.g. egg_dbus_connection_send_message_with_reply()) */
  for (n = 0; error_types != NULL && error_types[n] != G_TYPE_INVALID; n++)
    {
      enum_klass = g_type_class_ref (error_types[n]);
      enum_value = g_enum_get_value_by_nick (enum_klass, name);
      g_type_class_unref (enum_klass);
      if (enum_value != NULL)
        {
          error_domain = g_quark_from_static_string (g_type_name (error_types[n]));
          error_code = enum_value->value;
          goto mapped;
        }
    }

  /* Otherwise we fall back to returning a EGG_DBUS_ERROR_REMOTE_EXCEPTION error with enough
   * detail such that the D-Bus error name and error message can be decoded using the
   * egg_dbus_error_get_remote_exception() function.
   */
  error_code = EGG_DBUS_ERROR_REMOTE_EXCEPTION;

  name_escaped = g_uri_escape_string (name, NULL, TRUE);
  message_escaped = g_uri_escape_string (message, NULL, TRUE);

  s = g_string_new (literal_error);

  g_string_append_c (s, ' ');
  g_string_append (s, name_escaped);
  g_string_append_c (s, ' ');
  g_string_append (s, message_escaped);

  g_free (literal_error);
  literal_error = g_string_free (s, FALSE);

 mapped:

  error = g_error_new_literal (error_domain,
                               error_code,
                               literal_error);

  g_free (literal_error);

  return error;
}


GError *
_egg_dbus_error_new_remote_exception (const gchar   *name,
                                      const gchar   *message,
                                      GType         *error_types,
                                      const gchar   *format,
                                      ...)
{
  va_list va_args;
  GError *new_error;

  va_start (va_args, format);
  new_error = egg_dbus_error_new_remote_exception_valist (name,
                                                          message,
                                                          error_types,
                                                          format,
                                                          va_args);
  va_end (va_args);

  return new_error;
}

void
_egg_dbus_error_set_remote_exception (GError       **error,
                                      const gchar   *name,
                                      const gchar   *message,
                                      GType         *error_types,
                                      const gchar   *format,
                                      ...)
{
  va_list va_args;
  GError *new_error;

  if (error == NULL)
    return;

  va_start (va_args, format);

  new_error = egg_dbus_error_new_remote_exception_valist (name,
                                                          message,
                                                          error_types,
                                                          format,
                                                          va_args);
  *error = new_error;
  va_end (va_args);
}

