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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include <string.h>
#include "testbindings.h"
#include "testsubject.h"

/**
 * SECTION:testsubject
 * @title: TestSubject
 * @short_description: Example of user-supplied structure wrapper
 *
 * The #TestSubject interface is a user-supplied wrapper for accessing
 * and creating #EggDBusStructure instances with signature (sa{sv}).
 */

G_DEFINE_TYPE (TestSubject, test_subject, EGG_DBUS_TYPE_STRUCTURE);

static void
test_subject_init (TestSubject *instance)
{
}

static void
test_subject_class_init (TestSubjectClass *klass)
{
}

/**
 * test_subject_new:
 * @kind: A TestSubjectKind.
 * @name: Name of subject.
 * @favorite_food: The favorite food of the subject.
 * @favorite_color: The favorite color of the subject.
 *
 * Constructs a new #TestSubject.
 *
 * Returns: A #TestSubject.
 */
TestSubject *
test_subject_new (TestSubjectKind  kind,
                  const gchar     *name,
                  const gchar     *favorite_food,
                  const gchar     *favorite_color)
{
  GValue *values;
  const gchar *kind_str;
  EggDBusHashMap *properties;

  switch (kind)
    {
    case TEST_SUBJECT_KIND_HUMAN:
      kind_str = "human";
      break;

    case TEST_SUBJECT_KIND_CYLON:
      kind_str = "cylon";
      break;

    case TEST_SUBJECT_KIND_DEITY:
      kind_str = "deity";
      break;

    default:
      g_warning ("Unknown kind passed");
      return NULL;
    }

  properties = egg_dbus_hash_map_new (G_TYPE_STRING,         (GDestroyNotify) g_free,
                                      EGG_DBUS_TYPE_VARIANT, (GDestroyNotify) g_object_unref);

  egg_dbus_hash_map_insert (properties,
                            g_strdup ("name"),
                            egg_dbus_variant_new_for_string (name));

  egg_dbus_hash_map_insert (properties,
                            g_strdup ("favorite-food"),
                            egg_dbus_variant_new_for_string (favorite_food));

  egg_dbus_hash_map_insert (properties,
                            g_strdup ("favorite-color"),
                            egg_dbus_variant_new_for_string (favorite_color));

  values = g_new0 (GValue, 2);

  g_value_init (&(values[0]), G_TYPE_STRING);
  g_value_set_string (&(values[0]), kind_str);

  g_value_init (&(values[1]), EGG_DBUS_TYPE_HASH_MAP);
  g_value_take_object (&(values[1]), properties);

  return TEST_SUBJECT (g_object_new (TEST_TYPE_SUBJECT,
                                     "signature", "(sa{sv})",
                                     /* Note: the superclass steals the 'values' parameter */
                                     "elements", values,
                                     NULL));
}

/**
 * test_subject_get_kind:
 * @subject: A #TestSubject.
 *
 * Gets the kind of @subject.
 *
 * Returns: A #TestSubjectKind.
 */
TestSubjectKind
test_subject_get_kind (TestSubject *subject)
{
  const gchar *kind_str;
  TestSubjectKind kind;

  g_return_val_if_fail (TEST_IS_SUBJECT (subject), TEST_SUBJECT_KIND_UNKNOWN);

  egg_dbus_structure_get_element (EGG_DBUS_STRUCTURE (subject),
                                  0, &kind_str,
                                  -1);

  if (strcmp (kind_str, "human") == 0)
    kind = TEST_SUBJECT_KIND_HUMAN;
  else if (strcmp (kind_str, "cylon") == 0)
    kind = TEST_SUBJECT_KIND_CYLON;
  else if (strcmp (kind_str, "deity") == 0)
    kind = TEST_SUBJECT_KIND_DEITY;
  else
    {
      g_warning ("unknown kind str '%s'", kind_str);
      kind = TEST_SUBJECT_KIND_UNKNOWN;
    }

  return kind;
}

/**
 * test_subject_get_name:
 * @subject: A #TestSubject.
 *
 * Gets name of @subject.
 *
 * Returns: Name of @subject.
 **/
const gchar *
test_subject_get_name (TestSubject *subject)
{
  EggDBusHashMap *value;
  EggDBusVariant *variant;
  const gchar *result;

  g_return_val_if_fail (TEST_IS_SUBJECT (subject), NULL);

  egg_dbus_structure_get_element (EGG_DBUS_STRUCTURE (subject),
                1, &value,
                -1);

  variant = egg_dbus_hash_map_lookup (value, "name");
  result = egg_dbus_variant_get_string (variant);

  return result;
}

/**
 * test_subject_get_favorite_food:
 * @subject: A #TestSubject.
 *
 * Gets favorite food of @subject.
 *
 * Returns: Favorite food of @subject.
 **/
const gchar *
test_subject_get_favorite_food (TestSubject *subject)
{
  EggDBusHashMap *value;
  EggDBusVariant *variant;
  const gchar *result;

  g_return_val_if_fail (TEST_IS_SUBJECT (subject), NULL);

  egg_dbus_structure_get_element (EGG_DBUS_STRUCTURE (subject),
                1, &value,
                -1);

  variant = egg_dbus_hash_map_lookup (value, "favorite-food");
  result = egg_dbus_variant_get_string (variant);

  return result;
}

/**
 * test_subject_get_favorite_color:
 * @subject: A #TestSubject.
 *
 * Gets favorite color of @subject.
 *
 * Returns: Favorite color of @subject.
 **/
const gchar *
test_subject_get_favorite_color (TestSubject *subject)
{
  EggDBusHashMap *value;
  EggDBusVariant *variant;
  const gchar *result;

  g_return_val_if_fail (TEST_IS_SUBJECT (subject), NULL);

  egg_dbus_structure_get_element (EGG_DBUS_STRUCTURE (subject),
                1, &value,
                -1);

  variant = egg_dbus_hash_map_lookup (value, "favorite-color");
  result = egg_dbus_variant_get_string (variant);

  return result;
}
