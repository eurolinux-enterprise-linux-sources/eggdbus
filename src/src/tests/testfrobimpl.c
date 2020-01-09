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
#include "testfrobimpl.h"
#include "testbindings.h"

enum
{
  PROP_0,

  /* Properties from the TestFrob interface */
  PROP_Y,
  PROP_B,
  PROP_N,
  PROP_Q,
  PROP_I,
  PROP_U,
  PROP_X,
  PROP_T,
  PROP_D,
  PROP_S,
  PROP_O,
  PROP_G,
  PROP_AY,
  PROP_AB,
  PROP_AN,
  PROP_AQ,
  PROP_AI,
  PROP_AU,
  PROP_AX,
  PROP_AT,
  PROP_AD,
  PROP_AS,
  PROP_AO,
  PROP_AG,
  PROP_FOO,
};

static void test_frob_impl_frob_iface_init (TestFrobIface *iface);

G_DEFINE_TYPE_WITH_CODE (TestFrobImpl, test_frob_impl, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TEST_TYPE_FROB,
                                                test_frob_impl_frob_iface_init)
                         );

static void
test_frob_impl_init (TestFrobImpl *frob_impl)
{
}

static void
test_frob_impl_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  TestFrobImpl *frob_impl;
  EggDBusArraySeq *array;

  frob_impl = TEST_FROB_IMPL (object);

  switch (prop_id)
    {
    case PROP_Y:
      g_value_set_uchar (value, 1);
      break;

    case PROP_B:
      g_value_set_boolean (value, TRUE);
      break;

    case PROP_N:
      egg_dbus_value_set_int16 (value, 2);
      break;

    case PROP_Q:
      egg_dbus_value_set_uint16 (value, 3);
      break;

    case PROP_I:
      g_value_set_int (value, 4);
      break;

    case PROP_U:
      g_value_set_uint (value, 5);
      break;

    case PROP_X:
      g_value_set_int64 (value, 6);
      break;

    case PROP_T:
      g_value_set_uint64 (value, 7);
      break;

    case PROP_D:
      g_value_set_double (value, 7.5);
      break;

    case PROP_S:
      g_value_set_string (value, "a string");
      break;

    case PROP_O:
      g_value_set_boxed (value, "/some/path");
      break;

    case PROP_G:
      g_value_set_boxed (value, "(sig)");
      break;

    case PROP_AY:
        array = egg_dbus_array_seq_new (G_TYPE_UCHAR, NULL, NULL, NULL);
        egg_dbus_array_seq_add_fixed (array, 1);
        egg_dbus_array_seq_add_fixed (array, 11);
        g_value_take_object (value, array);
      break;

    case PROP_AB:
      array = egg_dbus_array_seq_new (G_TYPE_BOOLEAN, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, TRUE);
      egg_dbus_array_seq_add_fixed (array, FALSE);
      g_value_take_object (value, array);
      break;

    case PROP_AN:
      array = egg_dbus_array_seq_new (EGG_DBUS_TYPE_INT16, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, 2);
      egg_dbus_array_seq_add_fixed (array, 12);
      g_value_take_object (value, array);
      break;

    case PROP_AQ:
      array = egg_dbus_array_seq_new (EGG_DBUS_TYPE_UINT16, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, 3);
      egg_dbus_array_seq_add_fixed (array, 13);
      g_value_take_object (value, array);
      break;

    case PROP_AI:
      array = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, 4);
      egg_dbus_array_seq_add_fixed (array, 14);
      g_value_take_object (value, array);
      break;

    case PROP_AU:
      array = egg_dbus_array_seq_new (G_TYPE_UINT, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, 5);
      egg_dbus_array_seq_add_fixed (array, 15);
      g_value_take_object (value, array);
      break;

    case PROP_AX:
      array = egg_dbus_array_seq_new (G_TYPE_INT64, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, 6);
      egg_dbus_array_seq_add_fixed (array, 16);
      g_value_take_object (value, array);
      break;

    case PROP_AT:
      array = egg_dbus_array_seq_new (G_TYPE_UINT64, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, 7);
      egg_dbus_array_seq_add_fixed (array, 17);
      g_value_take_object (value, array);
      break;

    case PROP_AD:
      array = egg_dbus_array_seq_new (G_TYPE_DOUBLE, NULL, NULL, NULL);
      egg_dbus_array_seq_add_float (array, 7.5);
      egg_dbus_array_seq_add_float (array, 17.5);
      g_value_take_object (value, array);
      break;

    case PROP_AS:
      {
        const gchar *strs[] = {"a string", "another string", NULL};
        g_value_set_boxed (value, strs);
      }
      break;

    case PROP_AO:
      {
        const gchar *strs[] = {"/some/path", "/another/path", NULL};
        g_value_set_boxed (value, strs);
      }
      break;

    case PROP_AG:
      {
        const gchar *strs[] = {"(sig)", "((sig))", NULL};
        g_value_set_boxed (value, strs);
      }
      break;

    case PROP_FOO:
      g_value_set_string (value, "a frobbed string");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
test_frob_impl_class_init (TestFrobImplClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = test_frob_impl_get_property;

  g_assert (test_frob_override_properties (gobject_class, PROP_Y) == PROP_FOO);
}

TestFrobImpl *
test_frob_impl_new (void)
{
  return TEST_FROB_IMPL (g_object_new (TEST_TYPE_FROB_IMPL, NULL));
}

static void
frob_iface_handle_hello_world (TestFrob *instance,
                               const gchar *hello_message,
                               EggDBusMethodInvocation *method_invocation)
{
  if (strcmp (hello_message, "Yo") == 0)
    {
      egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                       TEST_ERROR,
                                                       TEST_ERROR_FLUX_CAPACITOR_FAILURE,
                                                       "Yo is not a proper greeting");
    }
  else
    {
      gchar *ret;

      ret = g_strdup_printf ("You greeted me with '%s'. Thanks!", hello_message);

      test_frob_handle_hello_world_finish (method_invocation,
                                           ret);

      g_free (ret);
    }
}

static void
frob_iface_handle_emit_test_signals (TestFrob *instance,
                                     EggDBusMethodInvocation *method_invocation)
{
  const gchar *str_array[3];
  const gchar *objpath_array[4];
  const gchar *sig_array[4];
  EggDBusArraySeq *array_int32;
  EggDBusArraySeq *array_byte;
  TestPoint *point;
  TestDescribedPoint *dpoint;
  TestPoint *point_in_dpoint;
  EggDBusHashMap *hash_string_to_string;
  EggDBusHashMap *hash_string_to_point;

  /* --- */

  test_frob_emit_signal_signal_with_primitive_types (instance,
                                                     NULL,
                                                     0xfe,
                                                     TRUE,
                                                     2,
                                                     3,
                                                     4,
                                                     5,
                                                     6,
                                                     7,
                                                     1.2,
                                                     "a string",
                                                     "/objpath",
                                                     "(ss)");

  /* --- */

  str_array[0] = "signalfoo";
  str_array[1] = "signalbar";
  str_array[2] = NULL;

  objpath_array[0] = "/signal/foo";
  objpath_array[1] = "/signal/bar";
  objpath_array[2] = "/signal/baz";
  objpath_array[3] = NULL;

  sig_array[0] = "s";
  sig_array[1] = "(ss)";
  sig_array[2] = "(sig)";
  sig_array[3] = NULL;

  array_byte = egg_dbus_array_seq_new (G_TYPE_UCHAR, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_byte, 1);
  egg_dbus_array_seq_add_fixed (array_byte, 11);

  array_int32 = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int32, 4);
  egg_dbus_array_seq_add_fixed (array_int32, 14);

  test_frob_emit_signal_signal_with_array_of_primitive_types (instance,
                                                              NULL,
                                                              array_byte,
                                                              array_int32,
                                                              (gchar **) str_array,
                                                              (gchar **) objpath_array,
                                                              (gchar **) sig_array);

  g_object_unref (array_byte);
  g_object_unref (array_int32);

  /* --- */

  point = test_point_new (40, 41);

  point_in_dpoint = test_point_new (42, 43);
  dpoint = test_described_point_new ("xmas", point_in_dpoint);
  g_object_unref (point_in_dpoint);

  hash_string_to_string = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                                                 G_TYPE_STRING, NULL);
  egg_dbus_hash_map_insert (hash_string_to_string, "secret", "emerald");
  egg_dbus_hash_map_insert (hash_string_to_string, "top-secret", "stuff");
  egg_dbus_hash_map_insert (hash_string_to_string, "ultra-top-secret", "Rupert");

  hash_string_to_point = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                                                TEST_TYPE_POINT, (GDestroyNotify) g_object_unref);
  egg_dbus_hash_map_insert (hash_string_to_point, "site59", test_point_new (20, 21));
  egg_dbus_hash_map_insert (hash_string_to_point, "site60", test_point_new (22, 23));

  test_frob_emit_signal_signal_with_structure_and_hash (instance,
                                                        NULL,
                                                        point,
                                                        dpoint,
                                                        hash_string_to_string,
                                                        hash_string_to_point);
  g_object_unref (point);
  g_object_unref (dpoint);
  g_object_unref (hash_string_to_string);
  g_object_unref (hash_string_to_point);

  test_frob_handle_emit_test_signals_finish (method_invocation);
}

static void
frob_iface_handle_test_primitive_types (TestFrob *instance,
                                        guint8 val_byte,
                                        gboolean val_boolean,
                                        gint16 val_int16,
                                        guint16 val_uint16,
                                        gint val_int32,
                                        guint val_uint32,
                                        gint64 val_int64,
                                        guint64 val_uint64,
                                        double val_double,
                                        const gchar *val_string,
                                        const gchar *val_objpath,
                                        const gchar *val_sig,
                                        EggDBusMethodInvocation *method_invocation)
{
  gchar *str;
  gchar *objpath;
  gchar *sig;

  str = g_strdup_printf ("%s%s", val_string, val_string);
  objpath = g_strdup_printf ("%s/modified", val_objpath);
  sig = g_strdup_printf ("(%s)", val_sig);

  test_frob_handle_test_primitive_types_finish (method_invocation,
                                                val_byte + 1,
                                                ! val_boolean,
                                                val_int16 + 1,
                                                val_uint16 + 1,
                                                val_int32 + 1,
                                                val_uint32 + 1,
                                                val_int64 + 1,
                                                val_uint64 + 1,
                                                -val_double,
                                                str,
                                                objpath,
                                                sig);

  g_free (str);
  g_free (objpath);
  g_free (sig);
}

static void
frob_iface_handle_test_array_of_primitive_types (TestFrob *instance,
                                                 EggDBusArraySeq *array_byte,
                                                 EggDBusArraySeq *array_boolean,
                                                 EggDBusArraySeq *array_int16,
                                                 EggDBusArraySeq *array_uint16,
                                                 EggDBusArraySeq *array_int32,
                                                 EggDBusArraySeq *array_uint32,
                                                 EggDBusArraySeq *array_int64,
                                                 EggDBusArraySeq *array_uint64,
                                                 EggDBusArraySeq *array_double,
                                                 gchar **array_string,
                                                 gchar **array_objpath,
                                                 gchar **array_sig,
                                                 EggDBusMethodInvocation *method_invocation)
{
  gchar **ret_array_string;
  gchar **ret_array_objpath;
  gchar **ret_array_sig;
  guint n;
  guint len;

  /* it's fine to modify incoming args */
  egg_dbus_array_seq_add_all (array_byte,    array_byte);
  egg_dbus_array_seq_add_all (array_boolean, array_boolean);
  egg_dbus_array_seq_add_all (array_int16,   array_int16);
  egg_dbus_array_seq_add_all (array_uint16,  array_uint16);
  egg_dbus_array_seq_add_all (array_int32,   array_int32);
  egg_dbus_array_seq_add_all (array_uint32,  array_uint32);
  egg_dbus_array_seq_add_all (array_int64,   array_int64);
  egg_dbus_array_seq_add_all (array_uint64,  array_uint64);
  egg_dbus_array_seq_add_all (array_double,  array_double);

  len = g_strv_length (array_string);
  ret_array_string = g_new0 (gchar *, len * 2 + 1);
  for (n = 0; n < len; n++)
    ret_array_string[n] = ret_array_string[len + n] = array_string[n];

  len = g_strv_length (array_string);
  ret_array_objpath = g_new0 (gchar *, len * 2 + 1);
  for (n = 0; n < len; n++)
    ret_array_objpath[n] = ret_array_objpath[len + n] = array_objpath[n];

  len = g_strv_length (array_string);
  ret_array_sig = g_new0 (gchar *, len * 2 + 1);
  for (n = 0; n < len; n++)
    ret_array_sig[n] = ret_array_sig[len + n] = array_sig[n];

  test_frob_handle_test_array_of_primitive_types_finish (method_invocation,
                                                         array_byte,
                                                         array_boolean,
                                                         array_int16,
                                                         array_uint16,
                                                         array_int32,
                                                         array_uint32,
                                                         array_int64,
                                                         array_uint64,
                                                         array_double,
                                                         ret_array_string,
                                                         ret_array_objpath,
                                                         ret_array_sig);

  g_free (ret_array_string);
  g_free (ret_array_objpath);
  g_free (ret_array_sig);
}

static void
frob_iface_handle_test_structure_types (TestFrob *instance,
                                        TestPoint *s1,
                                        TestDescribedPoint *s2,
                                        EggDBusMethodInvocation *method_invocation)
{
  TestPoint *p;

  test_point_set_x (s1, test_point_get_x (s1) + 1);
  test_point_set_y (s1, test_point_get_y (s1) + 1);

  test_described_point_set_desc (s2, "replaced desc");
  p = test_described_point_get_point (s2);
  test_point_set_x (p, test_point_get_x (p) + 2);
  test_point_set_y (p, test_point_get_y (p) + 2);

  test_frob_handle_test_structure_types_finish (method_invocation,
                                                s1,
                                                s2);
}

static void
frob_iface_handle_test_array_of_structure_types (TestFrob *instance,
                                                 EggDBusArraySeq *seq,
                                                 EggDBusMethodInvocation *method_invocation)
{
  guint n;

  for (n = 0; n < seq->size; n++)
    {
      TestExtendedDescribedPoint *edpoint;
      TestPoint *p;
      gchar *s;

      edpoint = egg_dbus_array_seq_get (seq, n);

      s = g_strdup_printf ("%s_%d", test_extended_described_point_get_desc (edpoint), n);
      test_extended_described_point_set_desc (edpoint, s);
      g_free (s);

      s = g_strdup_printf ("%s_%d", test_extended_described_point_get_ext_desc (edpoint), n);
      test_extended_described_point_set_ext_desc (edpoint, s);
      g_free (s);

      p = test_extended_described_point_get_point (edpoint);
      test_point_set_x (p, test_point_get_x (p) + 1);
      test_point_set_y (p, test_point_get_y (p) + 1);
    }

  test_frob_handle_test_array_of_structure_types_finish (method_invocation,
                                                         seq);
}

static gboolean
tht_string (EggDBusHashMap *hash_map,
            const gchar *key,
            const gchar *value,
            EggDBusHashMap *new_hash_map)
{
  egg_dbus_hash_map_insert (new_hash_map,
                            g_strdup_printf ("%smod", key),
                            g_strdup_printf ("%s%s", value, value));
  return FALSE;
}

static gboolean
tht_objpath (EggDBusHashMap *hash_map,
             const gchar *key,
             const gchar *value,
             EggDBusHashMap *new_hash_map)
{
  egg_dbus_hash_map_insert (new_hash_map,
                            g_strdup_printf ("%s/mod", key),
                            g_strdup_printf ("%s/mod2", value));
  return FALSE;
}

static gboolean
tht_fixed (EggDBusHashMap *hash_map,
           gpointer key,
           gpointer value,
           EggDBusHashMap *new_hash_map)
{
  guint64 fixed_key;

  if (hash_map->key_type == G_TYPE_INT64 ||
      hash_map->key_type == G_TYPE_UINT64)
    fixed_key = *((gint64 *) key);
  else
    fixed_key = GPOINTER_TO_INT (key);

  egg_dbus_hash_map_insert_fixed_fixed (new_hash_map,
                                        fixed_key * 2,
                                        3 * egg_dbus_hash_map_lookup_fixed_fixed (hash_map, fixed_key));

  return FALSE;
}

static gboolean
tht_bool (EggDBusHashMap *hash_map,
          gpointer key,
          gpointer value,
          EggDBusHashMap *new_hash_map)
{
  gboolean fixed_key;

  fixed_key = GPOINTER_TO_INT (key);

  egg_dbus_hash_map_insert_fixed_fixed (new_hash_map,
                                        fixed_key * 2,
                                        TRUE);

  return FALSE;
}

static gboolean
tht_double (EggDBusHashMap *hash_map,
            gpointer key,
            gpointer value,
            EggDBusHashMap *new_hash_map)
{
  gdouble float_key;

  float_key = *((gdouble *) key);

  egg_dbus_hash_map_insert_float_float (new_hash_map,
                                        float_key + 2.5,
                                        egg_dbus_hash_map_lookup_float_float (hash_map, float_key) + 5.0);

  return FALSE;
}

static void
frob_iface_handle_test_hash_tables (TestFrob *instance,
                                    EggDBusHashMap *hss,
                                    EggDBusHashMap *hoo,
                                    EggDBusHashMap *hii,
                                    EggDBusHashMap *hyy,
                                    EggDBusHashMap *hnn,
                                    EggDBusHashMap *hqq,
                                    EggDBusHashMap *huu,
                                    EggDBusHashMap *hbb,
                                    EggDBusHashMap *hxx,
                                    EggDBusHashMap *htt,
                                    EggDBusHashMap *hdd,
                                    EggDBusMethodInvocation *method_invocation)
{
  EggDBusHashMap *ret_hss;
  EggDBusHashMap *ret_hoo;
  EggDBusHashMap *ret_hii;
  EggDBusHashMap *ret_hyy;
  EggDBusHashMap *ret_hnn;
  EggDBusHashMap *ret_hqq;
  EggDBusHashMap *ret_huu;
  EggDBusHashMap *ret_hbb;
  EggDBusHashMap *ret_hxx;
  EggDBusHashMap *ret_htt;
  EggDBusHashMap *ret_hdd;

  ret_hss = egg_dbus_hash_map_new (G_TYPE_STRING, g_free, G_TYPE_STRING, g_free);
  ret_hoo = egg_dbus_hash_map_new (EGG_DBUS_TYPE_OBJECT_PATH, g_free, EGG_DBUS_TYPE_OBJECT_PATH, g_free);
  ret_hii = egg_dbus_hash_map_new (G_TYPE_INT, NULL, G_TYPE_INT, NULL);
  ret_hyy = egg_dbus_hash_map_new (G_TYPE_UCHAR, NULL, G_TYPE_UCHAR, NULL);
  ret_hnn = egg_dbus_hash_map_new (EGG_DBUS_TYPE_INT16, NULL, EGG_DBUS_TYPE_INT16, NULL);
  ret_hqq = egg_dbus_hash_map_new (EGG_DBUS_TYPE_UINT16, NULL, EGG_DBUS_TYPE_UINT16, NULL);
  ret_huu = egg_dbus_hash_map_new (G_TYPE_UINT, NULL, G_TYPE_UINT, NULL);
  ret_hbb = egg_dbus_hash_map_new (G_TYPE_BOOLEAN, NULL, G_TYPE_BOOLEAN, NULL);
  ret_hxx = egg_dbus_hash_map_new (G_TYPE_INT64, NULL, G_TYPE_INT64, NULL);
  ret_htt = egg_dbus_hash_map_new (G_TYPE_UINT64, NULL, G_TYPE_UINT64, NULL);
  ret_hdd = egg_dbus_hash_map_new (G_TYPE_DOUBLE, NULL, G_TYPE_DOUBLE, NULL);

  egg_dbus_hash_map_foreach (hss, (EggDBusHashMapForeachFunc) tht_string, ret_hss);
  egg_dbus_hash_map_foreach (hoo, (EggDBusHashMapForeachFunc) tht_objpath, ret_hoo);

  egg_dbus_hash_map_foreach (hii, (EggDBusHashMapForeachFunc) tht_fixed, ret_hii);
  egg_dbus_hash_map_foreach (hyy, (EggDBusHashMapForeachFunc) tht_fixed, ret_hyy);
  egg_dbus_hash_map_foreach (hnn, (EggDBusHashMapForeachFunc) tht_fixed, ret_hnn);
  egg_dbus_hash_map_foreach (hqq, (EggDBusHashMapForeachFunc) tht_fixed, ret_hqq);
  egg_dbus_hash_map_foreach (huu, (EggDBusHashMapForeachFunc) tht_fixed, ret_huu);
  egg_dbus_hash_map_foreach (hxx, (EggDBusHashMapForeachFunc) tht_fixed, ret_hxx);
  egg_dbus_hash_map_foreach (htt, (EggDBusHashMapForeachFunc) tht_fixed, ret_htt);

  egg_dbus_hash_map_foreach (hbb, (EggDBusHashMapForeachFunc) tht_bool, ret_hbb);

  egg_dbus_hash_map_foreach (hdd, (EggDBusHashMapForeachFunc) tht_double, ret_hdd);

  test_frob_handle_test_hash_tables_finish (method_invocation,
                                            ret_hss,
                                            ret_hoo,
                                            ret_hii,
                                            ret_hyy,
                                            ret_hnn,
                                            ret_hqq,
                                            ret_huu,
                                            ret_hbb,
                                            ret_hxx,
                                            ret_htt,
                                            ret_hdd);

  g_object_unref (ret_hss);
  g_object_unref (ret_hoo);
  g_object_unref (ret_hii);
  g_object_unref (ret_hyy);
  g_object_unref (ret_hnn);
  g_object_unref (ret_hqq);
  g_object_unref (ret_huu);
  g_object_unref (ret_hxx);
  g_object_unref (ret_htt);
  g_object_unref (ret_hdd);
  g_object_unref (ret_hbb);
}

static gboolean
taht_point_cb (EggDBusHashMap  *hash_map,
               const gchar     *key,
               EggDBusArraySeq *value,
               EggDBusHashMap  *ret_hash_map)
{
  EggDBusArraySeq *new_value;
  guint n;

  new_value = egg_dbus_array_seq_new (TEST_TYPE_POINT, g_object_unref, NULL, NULL);
  for (n = 0; n < value->size; n++)
    {
      TestPoint *p = value->data.v_ptr[n];
      egg_dbus_array_seq_add (new_value,
                              test_point_new (test_point_get_x (p) + 100,
                                              test_point_get_y (p) + 200));
    }

  egg_dbus_hash_map_insert (ret_hash_map,
                            g_strdup_printf ("%s_new", key),
                            new_value);

  return FALSE;
}

static void
frob_iface_handle_test_hash_tables_of_arrays (TestFrob *instance,
                                              EggDBusHashMap *hsas,
                                              EggDBusHashMap *hsao,
                                              EggDBusHashMap *hsai,
                                              EggDBusHashMap *hsay,
                                              EggDBusHashMap *hsan,
                                              EggDBusHashMap *hsaq,
                                              EggDBusHashMap *hsau,
                                              EggDBusHashMap *hsab,
                                              EggDBusHashMap *hsax,
                                              EggDBusHashMap *hsat,
                                              EggDBusHashMap *hsad,
                                              EggDBusHashMap *hash_of_point_arrays,
                                              EggDBusMethodInvocation *method_invocation)
{
  EggDBusHashMap *ret_hsas;
  EggDBusHashMap *ret_hsao;
  EggDBusHashMap *ret_hsai;
  EggDBusHashMap *ret_hsay;
  EggDBusHashMap *ret_hsan;
  EggDBusHashMap *ret_hsaq;
  EggDBusHashMap *ret_hsau;
  EggDBusHashMap *ret_hsab;
  EggDBusHashMap *ret_hsax;
  EggDBusHashMap *ret_hsat;
  EggDBusHashMap *ret_hsad;
  EggDBusHashMap *ret_hash_of_point_arrays;
  EggDBusArraySeq *a;

  ret_hsas = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, G_TYPE_STRV, NULL);
  egg_dbus_hash_map_insert (ret_hsas, "key0_mod", egg_dbus_hash_map_lookup (hsas, "key0"));
  egg_dbus_hash_map_insert (ret_hsas, "key1_mod", egg_dbus_hash_map_lookup (hsas, "key1"));

  ret_hsao = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_OBJECT_PATH_ARRAY, NULL);
  egg_dbus_hash_map_insert (ret_hsao, "key0_mod", egg_dbus_hash_map_lookup (hsao, "key0"));
  egg_dbus_hash_map_insert (ret_hsao, "key1_mod", egg_dbus_hash_map_lookup (hsao, "key1"));

  ret_hsai = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 1);
  egg_dbus_array_seq_add_fixed (a, 2);
  egg_dbus_hash_map_insert (ret_hsai, "k", a);

  ret_hsay = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (G_TYPE_UCHAR, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 3);
  egg_dbus_array_seq_add_fixed (a, 4);
  egg_dbus_hash_map_insert (ret_hsay, "k", a);

  ret_hsan = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (EGG_DBUS_TYPE_INT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 5);
  egg_dbus_array_seq_add_fixed (a, 6);
  egg_dbus_hash_map_insert (ret_hsan, "k", a);

  ret_hsaq = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (EGG_DBUS_TYPE_UINT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 7);
  egg_dbus_array_seq_add_fixed (a, 8);
  egg_dbus_hash_map_insert (ret_hsaq, "k", a);

  ret_hsau = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (G_TYPE_UINT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 9);
  egg_dbus_array_seq_add_fixed (a, 10);
  egg_dbus_hash_map_insert (ret_hsau, "k", a);

  ret_hsab = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (G_TYPE_BOOLEAN, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, TRUE);
  egg_dbus_array_seq_add_fixed (a, FALSE);
  egg_dbus_array_seq_add_fixed (a, TRUE);
  egg_dbus_hash_map_insert (ret_hsab, "k", a);

  ret_hsax = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (G_TYPE_INT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 11);
  egg_dbus_array_seq_add_fixed (a, 12);
  egg_dbus_hash_map_insert (ret_hsax, "k", a);

  ret_hsat = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (G_TYPE_UINT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 13);
  egg_dbus_array_seq_add_fixed (a, 14);
  egg_dbus_hash_map_insert (ret_hsat, "k", a);

  ret_hsad = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  a = egg_dbus_array_seq_new (G_TYPE_DOUBLE, NULL, NULL, NULL);
  egg_dbus_array_seq_add_float (a, 15.5);
  egg_dbus_array_seq_add_float (a, 16.5);
  egg_dbus_hash_map_insert (ret_hsad, "k", a);

  ret_hash_of_point_arrays = egg_dbus_hash_map_new (G_TYPE_STRING, g_free, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_foreach (hash_of_point_arrays,
                             (EggDBusHashMapForeachFunc) taht_point_cb,
                             ret_hash_of_point_arrays);

  test_frob_handle_test_hash_tables_of_arrays_finish (method_invocation,
                                                      ret_hsas,
                                                      ret_hsao,
                                                      ret_hsai,
                                                      ret_hsay,
                                                      ret_hsan,
                                                      ret_hsaq,
                                                      ret_hsau,
                                                      ret_hsab,
                                                      ret_hsax,
                                                      ret_hsat,
                                                      ret_hsad,
                                                      ret_hash_of_point_arrays);

  g_object_unref (ret_hsas);
  g_object_unref (ret_hsao);
  g_object_unref (ret_hsai);
  g_object_unref (ret_hsay);
  g_object_unref (ret_hsan);
  g_object_unref (ret_hsaq);
  g_object_unref (ret_hsau);
  g_object_unref (ret_hsab);
  g_object_unref (ret_hsax);
  g_object_unref (ret_hsat);
  g_object_unref (ret_hsad);
  g_object_unref (ret_hash_of_point_arrays);
}


static gboolean
thos_cb (EggDBusHashMap *hash_map,
         const gchar    *key,
         TestPoint      *point,
         EggDBusHashMap *new_hash_map)
{
  TestPoint *new_point;

  new_point = test_point_new (test_point_get_x (point) + 100,
                              test_point_get_y (point) + 200);

  egg_dbus_hash_map_insert (new_hash_map,
                            g_strdup_printf ("%s_new", key),
                            test_described_point_new (key,
                                                      new_point));

  g_object_unref (new_point);

  return FALSE;
}

static void
frob_iface_handle_test_hash_table_of_structures (TestFrob *instance,
                                                 EggDBusHashMap *hash_of_points,
                                                 EggDBusMethodInvocation *method_invocation)
{
  EggDBusHashMap *hash_of_dpoints;

  hash_of_dpoints = egg_dbus_hash_map_new (G_TYPE_STRING, g_free, TEST_TYPE_DESCRIBED_POINT, g_object_unref);
  egg_dbus_hash_map_foreach (hash_of_points, (EggDBusHashMapForeachFunc) thos_cb, hash_of_dpoints);

  test_frob_handle_test_hash_table_of_structures_finish (method_invocation,
                                                         hash_of_dpoints);

  g_object_unref (hash_of_dpoints);
}

static gboolean
thtohtos_cb (EggDBusHashMap *hash_map,
             const gchar    *key,
             TestPoint      *point,
             EggDBusHashMap *new_hash_map)
{
  egg_dbus_hash_map_insert (new_hash_map,
                            g_strdup_printf ("%s_new_new", key),
                            test_point_new (test_point_get_x (point) + 100,
                                            test_point_get_y (point) + 200));
  return FALSE;
}

static void
frob_iface_handle_test_hash_table_of_hash_tables_of_structures (TestFrob *instance,
                                                                EggDBusHashMap *hash_of_hash_of_points,
                                                                EggDBusMethodInvocation *method_invocation)
{
  EggDBusHashMap *ret_hash_of_hash_of_points;
  EggDBusHashMap *hash_of_points;
  EggDBusHashMap *new_hash_of_points;

  ret_hash_of_hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_HASH_MAP, g_object_unref);

  hash_of_points = egg_dbus_hash_map_lookup (hash_of_hash_of_points, "org1");
  new_hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, g_free, TEST_TYPE_POINT, g_object_unref);
  egg_dbus_hash_map_foreach (hash_of_points, (EggDBusHashMapForeachFunc) thtohtos_cb, new_hash_of_points);
  egg_dbus_hash_map_insert (ret_hash_of_hash_of_points, "org1_new", new_hash_of_points);

  hash_of_points = egg_dbus_hash_map_lookup (hash_of_hash_of_points, "org2");
  new_hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, TEST_TYPE_POINT, g_object_unref);
  egg_dbus_hash_map_foreach (hash_of_points, (EggDBusHashMapForeachFunc) thtohtos_cb, new_hash_of_points);
  egg_dbus_hash_map_insert (ret_hash_of_hash_of_points, "org2_new", new_hash_of_points);

  test_frob_handle_test_hash_table_of_hash_tables_of_structures_finish (method_invocation,
                                                                        ret_hash_of_hash_of_points);

  g_object_unref (ret_hash_of_hash_of_points);
}

static gboolean
taohtof_cb (EggDBusHashMap *hash_map,
            const gchar    *key,
            TestPoint      *point,
            EggDBusHashMap *new_hash_map)
{
  egg_dbus_hash_map_insert (new_hash_map,
                            g_strdup_printf ("%s_new_new", key),
                            test_point_new (test_point_get_x (point) + 100,
                                            test_point_get_y (point) + 200));
  return FALSE;
}

static void
frob_iface_handle_test_array_of_hash_tables_of_structures (TestFrob *instance,
                                                           EggDBusArraySeq *seq_of_map_of_structures,
                                                           EggDBusMethodInvocation *method_invocation)
{
  EggDBusArraySeq *new_seq;
  guint n;

  new_seq = egg_dbus_array_seq_new (EGG_DBUS_TYPE_HASH_MAP, g_object_unref, NULL, NULL);
  for (n = 0; n < seq_of_map_of_structures->size; n++)
    {
      EggDBusHashMap *map = seq_of_map_of_structures->data.v_ptr[n];
      EggDBusHashMap *new_map;

      new_map = egg_dbus_hash_map_new (G_TYPE_STRING, g_free,
                                       TEST_TYPE_POINT, g_object_unref);

      egg_dbus_hash_map_foreach (map, (EggDBusHashMapForeachFunc) taohtof_cb, new_map);

      egg_dbus_array_seq_add (new_seq, new_map);
    }

  test_frob_handle_test_array_of_hash_tables_of_structures_finish (method_invocation,
                                                                   new_seq);
}

static void
frob_iface_handle_test_array_of_arrays (TestFrob *instance,
                                        EggDBusArraySeq *aas,
                                        EggDBusArraySeq *aastruct,
                                        EggDBusArraySeq *aao,
                                        EggDBusArraySeq *aai,
                                        EggDBusArraySeq *aay,
                                        EggDBusArraySeq *aan,
                                        EggDBusArraySeq *aaq,
                                        EggDBusArraySeq *aau,
                                        EggDBusArraySeq *aab,
                                        EggDBusArraySeq *aax,
                                        EggDBusArraySeq *aat,
                                        EggDBusArraySeq *aad,
                                        EggDBusArraySeq *aaas,
                                        EggDBusMethodInvocation *method_invocation)
{
  egg_dbus_array_seq_add_all (aas, aas);
  egg_dbus_array_seq_add_all (aastruct, aastruct);
  egg_dbus_array_seq_add_all (aao, aao);
  egg_dbus_array_seq_add_all (aai, aai);
  egg_dbus_array_seq_add_all (aay, aay);
  egg_dbus_array_seq_add_all (aan, aan);
  egg_dbus_array_seq_add_all (aaq, aaq);
  egg_dbus_array_seq_add_all (aau, aau);
  egg_dbus_array_seq_add_all (aab, aab);
  egg_dbus_array_seq_add_all (aax, aax);
  egg_dbus_array_seq_add_all (aat, aat);
  egg_dbus_array_seq_add_all (aad, aad);
  egg_dbus_array_seq_add_all (aaas, aaas);

  test_frob_handle_test_array_of_arrays_finish (method_invocation,
                                                aas,
                                                aastruct,
                                                aao,
                                                aai,
                                                aay,
                                                aan,
                                                aaq,
                                                aau,
                                                aab,
                                                aax,
                                                aat,
                                                aad,
                                                aaas);
}

static void
frob_iface_handle_test_variant_return (TestFrob *instance,
                                       const gchar *desired_variant,
                                       EggDBusMethodInvocation *method_invocation)
{
  EggDBusVariant *variant;

  if (strcmp (desired_variant, "y") == 0)
    {
      variant = egg_dbus_variant_new_for_byte (1);
    }
  else if (strcmp (desired_variant, "b") == 0)
    {
      variant = egg_dbus_variant_new_for_boolean (TRUE);
    }
  else if (strcmp (desired_variant, "n") == 0)
    {
      variant = egg_dbus_variant_new_for_int16 (2);
    }
  else if (strcmp (desired_variant, "q") == 0)
    {
      variant = egg_dbus_variant_new_for_uint16 (3);
    }
  else if (strcmp (desired_variant, "i") == 0)
    {
      variant = egg_dbus_variant_new_for_int (4);
    }
  else if (strcmp (desired_variant, "u") == 0)
    {
      variant = egg_dbus_variant_new_for_uint (5);
    }
  else if (strcmp (desired_variant, "x") == 0)
    {
      variant = egg_dbus_variant_new_for_int64 (6);
    }
  else if (strcmp (desired_variant, "t") == 0)
    {
      variant = egg_dbus_variant_new_for_uint64 (7);
    }
  else if (strcmp (desired_variant, "d") == 0)
    {
      variant = egg_dbus_variant_new_for_double (7.5);
    }
  else if (strcmp (desired_variant, "s") == 0)
    {
      variant = egg_dbus_variant_new_for_string ("a string");
    }
  else if (strcmp (desired_variant, "o") == 0)
    {
      variant = egg_dbus_variant_new_for_object_path ("/some/object/path");
    }
  else if (strcmp (desired_variant, "ai") == 0)
    {
      EggDBusArraySeq *a;

      a = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (a, 4);
      egg_dbus_array_seq_add_fixed (a, 5);
      variant = egg_dbus_variant_new_for_seq (a, "i");
      g_object_unref (a);
    }
  else if (strcmp (desired_variant, "as") == 0)
    {
      gchar *strv[] = {"a string", "another string", NULL};

      variant = egg_dbus_variant_new_for_string_array (strv);
    }
  else if (strcmp (desired_variant, "point") == 0)
    {
      TestPoint *point;

      point = test_point_new (1, 2);
      variant = egg_dbus_variant_new_for_structure (EGG_DBUS_STRUCTURE (point));
      g_object_unref (point);
    }
  else if (strcmp (desired_variant, "dpoint") == 0)
    {
      TestDescribedPoint *dpoint;
      TestPoint *point;

      point = test_point_new (3, 4);
      dpoint = test_described_point_new ("the desc", point);
      g_object_unref (point);
      variant = egg_dbus_variant_new_for_structure (EGG_DBUS_STRUCTURE (dpoint));
      g_object_unref (dpoint);
    }
  else if (strcmp (desired_variant, "hash_of_points") == 0)
    {
      EggDBusHashMap *hash_of_points;

      hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, TEST_TYPE_POINT, g_object_unref);
      egg_dbus_hash_map_insert (hash_of_points, "key0", test_point_new (1, 2));
      egg_dbus_hash_map_insert (hash_of_points, "key1", test_point_new (3, 4));
      egg_dbus_hash_map_insert (hash_of_points, "key2", test_point_new (5, 6));
      egg_dbus_hash_map_insert (hash_of_points, "key3", test_point_new (7, 8));
      variant = egg_dbus_variant_new_for_map (hash_of_points, "s", "(ii)");
      g_object_unref (hash_of_points);
    }
  else if (strcmp (desired_variant, "unregistered_struct") == 0)
    {
      EggDBusStructure *s;
      EggDBusArraySeq *a;
      GValue *elements;

      a = egg_dbus_array_seq_new (G_TYPE_INT64, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (a, 0);
      egg_dbus_array_seq_add_fixed (a, 1);
      egg_dbus_array_seq_add_fixed (a, 2);
      egg_dbus_array_seq_add_fixed (a, 3);

      elements = g_new0 (GValue, 3);
      g_value_init (&elements[0], G_TYPE_INT);
      g_value_set_int (&elements[0], 42);

      g_value_init (&elements[1], G_TYPE_INT);
      g_value_set_int (&elements[1], 43);

      g_value_init (&elements[2], EGG_DBUS_TYPE_ARRAY_SEQ);
      g_value_take_object (&elements[2], a);

      s = egg_dbus_structure_new ("(iiax)", elements);

      variant = egg_dbus_variant_new_for_structure (s);
      g_object_unref (s);
    }
  else
    {
      egg_dbus_method_invocation_return_error (method_invocation,
                                               TEST_ERROR,
                                               TEST_ERROR_FLUX_CAPACITOR_FAILURE,
                                               "desired_variant '%s' not supported",
                                               desired_variant);
      return;
    }

  test_frob_handle_test_variant_return_finish (method_invocation,
                                               variant);

  g_object_unref (variant);
}

static void
test_frob_impl_frob_iface_init (TestFrobIface *iface)
{
  iface->handle_hello_world                                  = frob_iface_handle_hello_world;
  iface->handle_test_primitive_types                         = frob_iface_handle_test_primitive_types;
  iface->handle_test_array_of_primitive_types                = frob_iface_handle_test_array_of_primitive_types;
  iface->handle_emit_test_signals                            = frob_iface_handle_emit_test_signals;
  iface->handle_test_structure_types                         = frob_iface_handle_test_structure_types;
  iface->handle_test_array_of_structure_types                = frob_iface_handle_test_array_of_structure_types;
  iface->handle_test_hash_tables                             = frob_iface_handle_test_hash_tables;
  iface->handle_test_hash_tables_of_arrays                   = frob_iface_handle_test_hash_tables_of_arrays;
  iface->handle_test_hash_table_of_structures                = frob_iface_handle_test_hash_table_of_structures;
  iface->handle_test_hash_table_of_hash_tables_of_structures = frob_iface_handle_test_hash_table_of_hash_tables_of_structures;
  iface->handle_test_array_of_hash_tables_of_structures      = frob_iface_handle_test_array_of_hash_tables_of_structures;
  iface->handle_test_array_of_arrays                         = frob_iface_handle_test_array_of_arrays;
  iface->handle_test_variant_return                          = frob_iface_handle_test_variant_return;
}
