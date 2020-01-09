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
#include "testtweakimpl.h"
#include "testbindings.h"
#include "testtypes.h"

typedef struct
{
  gchar *some_read_write_property;
  gchar *new_foo;

  TestSomeExampleCType property_with_ctype;

  TestVehicle escape_vehicle;
  TestCreateFlags default_create_flags;

} TestTweakImplPrivate;

#define TEST_TWEAK_IMPL_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_TWEAK_IMPL, TestTweakImplPrivate))

enum
{
  PROP_0,

  /* Properties from the TestTweak interface */
  PROP_FOO,
  PROP_BAR,
  PROP_BAZ,
  PROP_BAZ_FORCED_TO_USE_PAIR,
  PROP_SOME_READ_WRITE_PROPERTY,
  PROP_PROPERTY_WITH_CTYPE,
  PROP_ESCAPE_VEHICLE,
  PROP_DEFAULT_CREATE_FLAGS,
};

static void test_tweak_impl_tweak_iface_init (TestTweakIface *iface);

G_DEFINE_TYPE_WITH_CODE (TestTweakImpl, test_tweak_impl, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TEST_TYPE_TWEAK,
                                                test_tweak_impl_tweak_iface_init)
                         );

static void
test_tweak_impl_init (TestTweakImpl *tweak_impl)
{
  TestTweakImplPrivate *priv;

  priv = TEST_TWEAK_IMPL_GET_PRIVATE (tweak_impl);

  priv->some_read_write_property = g_strdup ("Some initial property value");
}

static void
test_tweak_impl_finalize (GObject *object)
{
  TestTweakImpl *tweak_impl;
  TestTweakImplPrivate *priv;

  tweak_impl = TEST_TWEAK_IMPL (object);
  priv = TEST_TWEAK_IMPL_GET_PRIVATE (tweak_impl);

  g_free (priv->some_read_write_property);
  g_free (priv->new_foo);
}

static void
test_tweak_impl_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  TestTweakImpl *tweak_impl;
  TestTweakImplPrivate *priv;

  tweak_impl = TEST_TWEAK_IMPL (object);
  priv = TEST_TWEAK_IMPL_GET_PRIVATE (tweak_impl);

  switch (prop_id)
    {
    case PROP_SOME_READ_WRITE_PROPERTY:
      priv->some_read_write_property = g_value_dup_string (value);
      break;

    case PROP_PROPERTY_WITH_CTYPE:
      priv->property_with_ctype = (TestSomeExampleCType) g_value_get_int (value);
      break;

    case PROP_ESCAPE_VEHICLE:
      priv->escape_vehicle = g_value_get_enum (value);
      test_tweak_emit_signal_escape_vehicle_changed (TEST_TWEAK (object),
                                                     NULL,
                                                     priv->escape_vehicle);
      break;

    case PROP_DEFAULT_CREATE_FLAGS:
      priv->default_create_flags = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
test_tweak_impl_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  TestTweakImpl *tweak_impl;
  TestTweakImplPrivate *priv;
  EggDBusArraySeq *array;

  tweak_impl = TEST_TWEAK_IMPL (object);
  priv = TEST_TWEAK_IMPL_GET_PRIVATE (tweak_impl);

  switch (prop_id)
    {
    case PROP_FOO:
      if (priv->new_foo != NULL)
        g_value_set_string (value, priv->new_foo);
      else
        g_value_set_string (value, "a tweaked string");
      break;

    case PROP_BAR:
      array = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
      egg_dbus_array_seq_add_fixed (array, 1);
      egg_dbus_array_seq_add_fixed (array, 2);
      g_value_take_object (value, array);
      break;

    case PROP_BAZ:
      {
        TestPoint *point;
        point = test_point_new (3, 4);
        g_value_take_object (value, point);
      }
      break;

    case PROP_BAZ_FORCED_TO_USE_PAIR:
      {
        TestPair *pair;
        pair = test_pair_new (30, 40);
        g_value_take_object (value, pair);
      }
      break;

    case PROP_SOME_READ_WRITE_PROPERTY:
      g_value_set_string (value, priv->some_read_write_property);
      break;

    case PROP_PROPERTY_WITH_CTYPE:
      g_value_set_int (value, (gint) priv->property_with_ctype);
      break;

    case PROP_ESCAPE_VEHICLE:
      g_value_set_enum (value, priv->escape_vehicle);
      break;

    case PROP_DEFAULT_CREATE_FLAGS:
      g_value_set_flags (value, priv->default_create_flags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
test_tweak_impl_class_init (TestTweakImplClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = test_tweak_impl_finalize;
  gobject_class->set_property = test_tweak_impl_set_property;
  gobject_class->get_property = test_tweak_impl_get_property;

  g_assert (test_tweak_override_properties (gobject_class, PROP_FOO) == PROP_DEFAULT_CREATE_FLAGS);

  g_type_class_add_private (klass, sizeof (TestTweakImplPrivate));
}

TestTweakImpl *
test_tweak_impl_new (void)
{
  return TEST_TWEAK_IMPL (g_object_new (TEST_TYPE_TWEAK_IMPL, NULL));
}

static void
tweak_iface_handle_i_can_haz_greetingz (TestTweak *instance,
                                        const gchar *greetz,
                                        EggDBusMethodInvocation *method_invocation)
{
  gchar *word;

  word = g_strdup_printf ("Word. You haz greetz '%s'. KTHXBYE!", greetz);

  test_tweak_handle_i_can_haz_greetingz_finish (method_invocation,
                                                word);

  g_free (word);
}

static void
tweak_iface_handle_get_server_unique_name (TestTweak *instance,
                                           EggDBusMethodInvocation *method_invocation)
{
  EggDBusConnection *connection;
  const gchar *unique_name;

  connection = egg_dbus_method_invocation_get_connection (method_invocation);
  unique_name = egg_dbus_connection_get_unique_name (connection);

  test_tweak_handle_get_server_unique_name_finish (method_invocation,
                                                   unique_name);
}

static void
tweak_iface_handle_broadcastz_newz (TestTweak *instance,
                                    const gchar *newz,
                                    EggDBusMethodInvocation *method_invocation)
{
  gchar *s;

  s = g_strdup_printf ("Word. Broadcastz '%s'. KTHXBYE!", newz);
  test_tweak_emit_signal_newz_notifz (instance,
                                      NULL,
                                      s);

  test_tweak_handle_broadcastz_newz_finish (method_invocation);

  g_free (s);
}

static void
rw_property_changed (TestTweak *instance,
                     GParamSpec *pspec,
                     gpointer user_data)
{
  TestTweakImpl *tweak_impl;
  TestTweakImplPrivate *priv;
  EggDBusMethodInvocation *method_invocation;

  tweak_impl = TEST_TWEAK_IMPL (instance);
  priv = TEST_TWEAK_IMPL_GET_PRIVATE (tweak_impl);
  method_invocation = EGG_DBUS_METHOD_INVOCATION (user_data);

  g_signal_handlers_disconnect_by_func (instance,
                                        (GCallback) rw_property_changed,
                                        method_invocation);

  test_tweak_handle_block_until_rw_property_changes_finish (method_invocation,
                                                            priv->some_read_write_property);
}

static void
tweak_iface_handle_block_until_rw_property_changes (TestTweak *instance,
                                                    EggDBusMethodInvocation *method_invocation)
{
  g_signal_connect (instance,
                    "notify::some-read-write-property",
                    (GCallback) rw_property_changed,
                    method_invocation);
}

static void
tweak_iface_handle_change_readable_property (TestTweak *instance,
                                             const gchar *new_value,
                                             EggDBusMethodInvocation *method_invocation)
{
  TestTweakImpl *tweak_impl;
  TestTweakImplPrivate *priv;

  tweak_impl = TEST_TWEAK_IMPL (instance);
  priv = TEST_TWEAK_IMPL_GET_PRIVATE (tweak_impl);

  priv->new_foo = g_strdup (new_value);
  g_object_notify (G_OBJECT (tweak_impl), "foo");

  test_tweak_handle_change_readable_property_finish (method_invocation);
}

typedef struct {
  TestTweak               *instance;
  EggDBusMethodInvocation *method_invocation;
} LongRunningData;

static gboolean
long_running_method_timeout_cb (gpointer user_data)
{
  LongRunningData *data = user_data;

  test_tweak_handle_long_running_method_finish (data->method_invocation);

  g_object_unref (data->instance);
  g_free (data);

  return FALSE;
}

static void
tweak_iface_handle_long_running_method (TestTweak               *instance,
                                        gint                     msec_to_run,
                                        EggDBusMethodInvocation *method_invocation)
{
  LongRunningData *data;

  /* TODO: Hmm, would be useful to get the instance from method_invocation */
  data = g_new0 (LongRunningData, 1);
  data->instance = g_object_ref (instance);
  data->method_invocation = method_invocation;

  g_timeout_add (msec_to_run,
                 long_running_method_timeout_cb,
                 data);
}

static void
tweak_iface_handle_return_gerror (TestTweak               *instance,
                                  const gchar             *error_domain,
                                  gint                     error_code,
                                  EggDBusMethodInvocation *method_invocation)
{
  egg_dbus_method_invocation_return_error (method_invocation,
                                           g_quark_from_string (error_domain),
                                           error_code,
                                           "This is the error you requested (domain='%s', error_code=%d).",
                                           error_domain,
                                           error_code);
}

static void
tweak_iface_handle_method_with_ctypes (TestTweak                *instance,
                                       TestSomeExampleCType      value,
                                       EggDBusMethodInvocation  *method_invocation)
{
  test_tweak_emit_signal_signal_with_ctype (instance,
                                            NULL,
                                            value + 1);

  test_tweak_handle_method_with_ctypes_finish (method_invocation,
                                               value + 1);
}

static void
test_tweak_impl_tweak_iface_init (TestTweakIface *iface)
{
  iface->handle_i_can_haz_greetingz             = tweak_iface_handle_i_can_haz_greetingz;
  iface->handle_get_server_unique_name          = tweak_iface_handle_get_server_unique_name;
  iface->handle_broadcastz_newz                 = tweak_iface_handle_broadcastz_newz;
  iface->handle_block_until_rw_property_changes = tweak_iface_handle_block_until_rw_property_changes;
  iface->handle_change_readable_property        = tweak_iface_handle_change_readable_property;
  iface->handle_long_running_method             = tweak_iface_handle_long_running_method;
  iface->handle_return_gerror                   = tweak_iface_handle_return_gerror;
  iface->handle_method_with_ctypes              = tweak_iface_handle_method_with_ctypes;
}
