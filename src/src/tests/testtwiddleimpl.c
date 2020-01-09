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
#include "testtweakimpl.h"
#include "testtwiddleimpl.h"
#include "testbindings.h"
#include "testsubject.h"

typedef struct
{
  TestSubject *six;
  TestSubject *david;
  TestSubject *divad;
  TestSubject *god;
} TestTwiddleImplPrivate;

#define TEST_TWIDDLE_IMPL_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_TWIDDLE_IMPL, TestTwiddleImplPrivate))

static void test_twiddle_impl_twiddle_iface_init (TestTwiddleIface *iface);

G_DEFINE_TYPE_WITH_CODE (TestTwiddleImpl, test_twiddle_impl, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TEST_TYPE_TWIDDLE,
                                                test_twiddle_impl_twiddle_iface_init)
                         );

static void
test_twiddle_impl_init (TestTwiddleImpl *twiddle_impl)
{
  TestTwiddleImplPrivate *priv;

  priv = TEST_TWIDDLE_IMPL_GET_PRIVATE (twiddle_impl);

  priv->god   = test_subject_new (TEST_SUBJECT_KIND_DEITY, "God",         "deity-snacks",          "infrared");
  priv->david = test_subject_new (TEST_SUBJECT_KIND_HUMAN, "David",       "buffalo chicken pizza", "blue");
  priv->six   = test_subject_new (TEST_SUBJECT_KIND_CYLON, "Caprica-Six", "baltar-snacks",         "red");
  priv->divad = test_subject_new (TEST_SUBJECT_KIND_HUMAN, "Divad",       "oysters",               "black");
}

static void
test_twiddle_impl_finalize (GObject *object)
{
  TestTwiddleImpl *twiddle_impl;
  TestTwiddleImplPrivate *priv;

  twiddle_impl = TEST_TWIDDLE_IMPL (object);
  priv = TEST_TWIDDLE_IMPL_GET_PRIVATE (twiddle_impl);

  g_object_unref (priv->god);
  g_object_unref (priv->david);
  g_object_unref (priv->six);
  g_object_unref (priv->divad);
}

static void
test_twiddle_impl_class_init (TestTwiddleImplClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = test_twiddle_impl_finalize;

  g_type_class_add_private (klass, sizeof (TestTwiddleImplPrivate));
}

TestTwiddleImpl *
test_twiddle_impl_new (void)
{
  return TEST_TWIDDLE_IMPL (g_object_new (TEST_TYPE_TWIDDLE_IMPL, NULL));
}

static void
twiddle_iface_handle_broadcastz_newz (TestTwiddle *instance,
                                      const gchar *newz,
                                      EggDBusMethodInvocation *method_invocation)
{
  gchar *s;

  s = g_strdup_printf ("Sez '%s'. KTHXBYE!", newz);
  test_twiddle_emit_signal_newz_notifz (instance,
                                        NULL,
                                        s);

  test_twiddle_handle_broadcastz_newz_finish (method_invocation);

  g_free (s);
}

static void
twiddle_iface_handle_get_most_powerful_subject (TestTwiddle *instance,
                                                EggDBusMethodInvocation *method_invocation)
{
  TestTwiddleImpl *twiddle_impl;
  TestTwiddleImplPrivate *priv;

  twiddle_impl = TEST_TWIDDLE_IMPL (instance);
  priv = TEST_TWIDDLE_IMPL_GET_PRIVATE (twiddle_impl);

  test_twiddle_handle_get_most_powerful_subject_finish (method_invocation,
                                                        priv->god);
}

static void
twiddle_iface_handle_get_all_subjects (TestTwiddle *instance,
                                       EggDBusMethodInvocation *method_invocation)
{
  TestTwiddleImpl *twiddle_impl;
  TestTwiddleImplPrivate *priv;
  EggDBusArraySeq *seq;

  twiddle_impl = TEST_TWIDDLE_IMPL (instance);
  priv = TEST_TWIDDLE_IMPL_GET_PRIVATE (twiddle_impl);

  seq = egg_dbus_array_seq_new (TEST_TYPE_SUBJECT, NULL, NULL, NULL);
  egg_dbus_array_seq_add (seq, priv->god);
  egg_dbus_array_seq_add (seq, priv->david);
  egg_dbus_array_seq_add (seq, priv->six);
  egg_dbus_array_seq_add (seq, priv->divad);

  test_twiddle_handle_get_all_subjects_finish (method_invocation,
                                               seq);

  g_object_unref (seq);
}

static void
twiddle_iface_handle_register_interface (TestTwiddle              *instance,
                                         const gchar              *object_path,
                                         gboolean                  impl_frob,
                                         gboolean                  impl_tweak,
                                         gboolean                  impl_twiddle,
                                         EggDBusMethodInvocation  *method_invocation)
{
  EggDBusConnection *connection;

  connection = egg_dbus_method_invocation_get_connection (method_invocation);

  if (impl_frob)
    egg_dbus_connection_register_interface (connection,
                                            object_path,
                                            TEST_TYPE_FROB, test_frob_impl_new (),
                                            G_TYPE_INVALID);

  if (impl_tweak)
    egg_dbus_connection_register_interface (connection,
                                            object_path,
                                            TEST_TYPE_TWEAK, test_tweak_impl_new (),
                                            G_TYPE_INVALID);

  if (impl_twiddle)
    egg_dbus_connection_register_interface (connection,
                                            object_path,
                                            TEST_TYPE_TWIDDLE, test_twiddle_impl_new (),
                                            G_TYPE_INVALID);

  test_twiddle_handle_register_interface_finish (method_invocation);
}

static void
twiddle_iface_handle_unregister_interface (TestTwiddle              *instance,
                                           const gchar              *object_path,
                                           gboolean                  impl_frob,
                                           gboolean                  impl_tweak,
                                           gboolean                  impl_twiddle,
                                           EggDBusMethodInvocation  *method_invocation)
{
  EggDBusConnection *connection;

  connection = egg_dbus_method_invocation_get_connection (method_invocation);

  if (impl_frob)
    egg_dbus_connection_unregister_interface (connection,
                                              object_path,
                                              TEST_TYPE_FROB,
                                              G_TYPE_INVALID);

  if (impl_tweak)
    egg_dbus_connection_unregister_interface (connection,
                                              object_path,
                                              TEST_TYPE_TWEAK,
                                              G_TYPE_INVALID);

  if (impl_twiddle)
    egg_dbus_connection_unregister_interface (connection,
                                              object_path,
                                              TEST_TYPE_TWIDDLE,
                                              G_TYPE_INVALID);

  test_twiddle_handle_unregister_interface_finish (method_invocation);
}

static void
twiddle_iface_handle_unregister_all_interfaces (TestTwiddle              *instance,
                                                const gchar              *object_path,
                                                EggDBusMethodInvocation  *method_invocation)
{
  EggDBusConnection *connection;
  guint n;
  guint num_interfaces;
  GType *interface_types;
  GObject **interface_stubs;

  connection = egg_dbus_method_invocation_get_connection (method_invocation);

  num_interfaces = egg_dbus_connection_lookup_interface (connection,
                                                         object_path,
                                                         &interface_types,
                                                         &interface_stubs);
  for (n = 0; n < num_interfaces; n++)
    {
      /*g_debug ("%p %s %s", interface_stubs[n], g_type_name (interface_types[n]), g_type_name (G_TYPE_FROM_INSTANCE (interface_stubs[n])));*/
      g_object_unref (interface_stubs[n]);
    }

  g_free (interface_types);
  g_free (interface_stubs);

  test_twiddle_handle_unregister_all_interfaces_finish (method_invocation);
}

static void
test_twiddle_impl_twiddle_iface_init (TestTwiddleIface *iface)
{
  iface->handle_broadcastz_newz           = twiddle_iface_handle_broadcastz_newz;
  iface->handle_get_most_powerful_subject = twiddle_iface_handle_get_most_powerful_subject;
  iface->handle_get_all_subjects          = twiddle_iface_handle_get_all_subjects;
  iface->handle_register_interface        = twiddle_iface_handle_register_interface;
  iface->handle_unregister_interface      = twiddle_iface_handle_unregister_interface;
  iface->handle_unregister_all_interfaces = twiddle_iface_handle_unregister_all_interfaces;
}
