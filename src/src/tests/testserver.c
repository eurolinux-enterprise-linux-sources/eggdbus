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
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "testbindings.h"

#include "testfrobimpl.h"
#include "testtweakimpl.h"
#include "testtwiddleimpl.h"

int
main (int argc, char *argv[])
{
  int ret;
  GMainLoop *loop;
  EggDBusConnection *connection;
  GError *error;
  TestFrobImpl *frob_impl;
  TestTweakImpl *tweak_impl;
  TestTwiddleImpl *twiddle_impl;
  EggDBusRequestNameReply request_name_reply;

  ret = 1;

  error = NULL;

  g_type_init ();

  loop = g_main_loop_new (NULL, FALSE);

  connection = egg_dbus_connection_get_for_bus (EGG_DBUS_BUS_TYPE_SESSION);

  if (!egg_dbus_bus_request_name_sync (egg_dbus_connection_get_bus (connection),
                                       0, /* call flags */
                                       "com.example.TestService",
                                       EGG_DBUS_REQUEST_NAME_FLAGS_NONE,
                                       &request_name_reply,
                                       NULL,
                                       &error))
    {
      g_warning ("error: %s", error->message);
      g_error_free (error);
      goto out;
    }

  if (request_name_reply != EGG_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      g_warning ("could not become primary name owner");
      goto out;
    }

  frob_impl = test_frob_impl_new ();
  tweak_impl = test_tweak_impl_new ();
  twiddle_impl = test_twiddle_impl_new ();

  egg_dbus_connection_register_interface (connection,
                                          "/com/example/TestObject",
                                          TEST_TYPE_FROB, frob_impl,
                                          TEST_TYPE_TWEAK, tweak_impl,
                                          TEST_TYPE_TWIDDLE, twiddle_impl,
                                          G_TYPE_INVALID);

  g_main_loop_run (loop);

  g_object_unref (frob_impl);
  g_object_unref (tweak_impl);
  g_object_unref (twiddle_impl);
  g_object_unref (connection);

  ret = 0;

 out:

  return ret;
}
