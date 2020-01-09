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

/* TODO: figure out how the GTesting framework *really* works and then use it properly */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testbindings.h"

static EggDBusConnection *connection;
static EggDBusObjectProxy *object_proxy;
static GMainLoop *loop;

/* ---------------------------------------------------------------------------------------------------- */

/* Baby sitted spawning */

/* unique for every baby sitter */
static pid_t child_pid;
static const gchar *sitter_command_line;

static void
baby_sitter_signal_handler (int signal)
{
  //fprintf (stderr, "%d: was asked to kill myself and the child I'm baby sitting with pid %d for '%s'\n", getpid (), child_pid, sitter_command_line);
  kill (child_pid, SIGTERM);
  exit (0);
}

static GPid
spawn_with_baby_sitter (const gchar *command_line)
{
  pid_t baby_sitter_pid;
  int baby_sitter_pipe[2];

  pipe (baby_sitter_pipe);

  baby_sitter_pid = fork ();
  g_assert (baby_sitter_pid != -1);

  if (baby_sitter_pid == 0)
    {
      /* child, e.g. baby sitter */

      close (baby_sitter_pipe[1]); /* close unused write end */

      child_pid = fork ();
      g_assert (child_pid != -1);

      if (child_pid != 0)
        {
          GPollFD fds[1];

          sitter_command_line = command_line;

          /* this is for implementing kill_baby_sitted_child() */
          signal (SIGTERM, baby_sitter_signal_handler);

          //fprintf (stderr, "%d: baby sitting pid %d for '%s'\n", getpid (), child_pid, command_line);

          fds[0].fd = baby_sitter_pipe[0];
          fds[0].events = G_IO_HUP;
          fds[0].revents = 0;

          /* babysitter: kill child when parent hangs up, then kill yourself. */
          g_poll (fds, 1, -1);

          //fprintf (stderr, "%d: parent died, now reaping child with pid %d for '%s'\n", getpid (), child_pid, command_line);
          kill (child_pid, SIGTERM);

          exit (0);

        }
      else
        {
          gchar **argv;

          /* child: execute commandline */

          argv = g_strsplit (command_line, " ", 0);
          execvp (argv[0], argv);
        }

    }
  else
    {
      /* parent */

      close (baby_sitter_pipe[0]); /* close unused read end */
    }

  return baby_sitter_pid;
}

static void
kill_baby_sitted_child (GPid pid)
{
  kill (pid, SIGTERM);
}

/* ---------------------------------------------------------------------------------------------------- */
static void
low_level_hello_world_cb (EggDBusConnection *connection,
                          GAsyncResult      *res,
                          gpointer           user_data)
{
  GError *error;
  gboolean expect_failure;
  EggDBusMessage *reply;
  gchar *ret_string;
  gchar *dbus_error_name;
  gchar *dbus_error_message;

  expect_failure = (gboolean) GPOINTER_TO_UINT (user_data);

  error = NULL;
  reply = egg_dbus_connection_send_message_with_reply_finish (connection, res, &error);

  if (expect_failure)
    {
      g_assert (reply == NULL);
      g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_REMOTE_EXCEPTION);

      g_assert (egg_dbus_error_get_remote_exception (error,
                                                     &dbus_error_name,
                                                     &dbus_error_message));
      g_error_free (error);

      g_assert_cmpstr (dbus_error_name, ==, "com.example.Error.FluxCapacitorFailure");
      g_assert_cmpstr (dbus_error_message, ==, "Yo is not a proper greeting");
      g_free (dbus_error_name);
      g_free (dbus_error_message);
    }
  else
    {
      g_assert (reply != NULL);
      g_assert_no_error (error);

      egg_dbus_message_extract_string (reply, &ret_string, &error);
      g_assert_no_error (error);
      g_assert_cmpstr (ret_string, ==, "You greeted me with 'Yupski'. Thanks!");
      g_free (ret_string);
      g_object_unref (reply);
    }

  g_main_loop_quit (loop);
}

static void
test_low_level (void)
{
  GError *error;
  EggDBusMessage *message;
  EggDBusMessage *reply;
  gchar *ret_string;
  gchar *dbus_error_name;
  gchar *dbus_error_message;

  error = NULL;

  /* sync (in mainloop) */
  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             NULL,
                                                             "com.example.TestService",
                                                             "/com/example/TestObject",
                                                             "com.example.Frob",
                                                             "HelloWorld");
  egg_dbus_message_append_string (message, "Yupski", &error);
  g_assert_no_error (error);
  reply = egg_dbus_connection_send_message_with_reply_sync (connection,
                                                            EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                                            message,
                                                            NULL,
                                                            NULL,
                                                            &error);
  g_assert_no_error (error);
  egg_dbus_message_extract_string (reply, &ret_string, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (ret_string, ==, "You greeted me with 'Yupski'. Thanks!");
  g_free (ret_string);
  g_object_unref (message);
  g_object_unref (reply);

  /* sync (in mainloop) with expected error */
  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             NULL,
                                                             "com.example.TestService",
                                                             "/com/example/TestObject",
                                                             "com.example.Frob",
                                                             "HelloWorld");
  egg_dbus_message_append_string (message, "Yo", &error);
  g_assert_no_error (error);
  reply = egg_dbus_connection_send_message_with_reply_sync (connection,
                                                            EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                                            message,
                                                            NULL,
                                                            NULL,
                                                            &error);
  g_assert (reply == NULL);
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_REMOTE_EXCEPTION);
  g_object_unref (message);
  g_assert (egg_dbus_error_get_remote_exception (error,
                                                 &dbus_error_name,
                                                 &dbus_error_message));
  g_error_free (error);
  error = NULL;
  g_assert_cmpstr (dbus_error_name, ==, "com.example.Error.FluxCapacitorFailure");
  g_assert_cmpstr (dbus_error_message, ==, "Yo is not a proper greeting");
  g_free (dbus_error_name);
  g_free (dbus_error_message);

  /* sync (in mainloop) with expected error mapped to proper GError */
  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             NULL,
                                                             "com.example.TestService",
                                                             "/com/example/TestObject",
                                                             "com.example.Frob",
                                                             "HelloWorld");
  egg_dbus_message_append_string (message, "Yo", &error);
  g_assert_no_error (error);
  reply = egg_dbus_connection_send_message_with_reply_sync (connection,
                                                            EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                                            message,
                                                            test_bindings_get_error_domain_types (),
                                                            NULL,
                                                            &error);
  g_object_unref (message);
  g_assert (reply == NULL);
  g_assert_error (error, TEST_ERROR, TEST_ERROR_FLUX_CAPACITOR_FAILURE);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking com.example.Frob.HelloWorld() on /com/example/TestObject at name com.example.TestService: com.example.Error.FluxCapacitorFailure: Yo is not a proper greeting");
  g_error_free (error);
  error = NULL;

  /* sync (without mainloop) */
  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             NULL,
                                                             "com.example.TestService",
                                                             "/com/example/TestObject",
                                                             "com.example.Frob",
                                                             "HelloWorld");
  egg_dbus_message_append_string (message, "Yupski", &error);
  g_assert_no_error (error);
  reply = egg_dbus_connection_send_message_with_reply_sync (connection,
                                                            0, /* flags */
                                                            message,
                                                            NULL,
                                                            NULL,
                                                            &error);
  g_assert_no_error (error);
  egg_dbus_message_extract_string (reply, &ret_string, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (ret_string, ==, "You greeted me with 'Yupski'. Thanks!");
  g_free (ret_string);
  g_object_unref (message);
  g_object_unref (reply);

  /* sync (without mainloop) with expected error */
  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             NULL,
                                                             "com.example.TestService",
                                                             "/com/example/TestObject",
                                                             "com.example.Frob",
                                                             "HelloWorld");
  egg_dbus_message_append_string (message, "Yo", &error);
  g_assert_no_error (error);
  reply = egg_dbus_connection_send_message_with_reply_sync (connection,
                                                            0, /* flags */
                                                            message,
                                                            NULL,
                                                            NULL,
                                                            &error);
  g_assert (reply == NULL);
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_REMOTE_EXCEPTION);
  g_object_unref (message);
  g_assert (egg_dbus_error_get_remote_exception (error,
                                                 &dbus_error_name,
                                                 &dbus_error_message));
  g_error_free (error);
  error = NULL;
  g_assert_cmpstr (dbus_error_name, ==, "com.example.Error.FluxCapacitorFailure");
  g_assert_cmpstr (dbus_error_message, ==, "Yo is not a proper greeting");
  g_free (dbus_error_name);
  g_free (dbus_error_message);

  /* async */
  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             NULL,
                                                             "com.example.TestService",
                                                             "/com/example/TestObject",
                                                             "com.example.Frob",
                                                             "HelloWorld");
  egg_dbus_message_append_string (message, "Yupski", &error);
  g_assert_no_error (error);
  egg_dbus_connection_send_message_with_reply (connection,
                                               0, /* flags */
                                               message,
                                               NULL,
                                               NULL,
                                               (GAsyncReadyCallback) low_level_hello_world_cb,
                                               GUINT_TO_POINTER ((guint) FALSE));
  g_main_loop_run (loop);
  g_object_unref (message);

  /* async with expected error */
  message = egg_dbus_connection_new_message_for_method_call (connection,
                                                             NULL,
                                                             "com.example.TestService",
                                                             "/com/example/TestObject",
                                                             "com.example.Frob",
                                                             "HelloWorld");
  egg_dbus_message_append_string (message, "Yo", &error);
  g_assert_no_error (error);
  egg_dbus_connection_send_message_with_reply (connection,
                                               0, /* flags */
                                               message,
                                               NULL,
                                               NULL,
                                               (GAsyncReadyCallback) low_level_hello_world_cb,
                                               GUINT_TO_POINTER ((guint) TRUE));
  g_main_loop_run (loop);
  g_object_unref (message);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
hello_world_cb (TestFrob *frob,
                GAsyncResult *res,
                gpointer user_data)
{
  char **result = (char **) user_data;
  GError *error;
  char *s;

  error = NULL;
  if (!test_frob_hello_world_finish (frob, &s, res, &error))
    {
      g_assert_error (error, TEST_ERROR, TEST_ERROR_FLUX_CAPACITOR_FAILURE);
      g_assert_cmpstr (error->message, ==, "Remote Exception invoking com.example.Frob.HelloWorld() on /com/example/TestObject at name com.example.TestService: com.example.Error.FluxCapacitorFailure: Yo is not a proper greeting");
      g_error_free (error);

      *result = NULL;
    }
  else
    {
      g_assert_no_error (error);
      *result = s;
    }

  g_main_loop_quit (loop);
}

static void
test_hello_world (void)
{
  GError *error;
  gchar *s;

  error = NULL;

  /* test sync method invocation */
  g_assert (test_frob_hello_world_sync (TEST_QUERY_INTERFACE_FROB (object_proxy), 0, /* flags */
                                        "Hi!",
                                        &s,
                                        NULL,
                                        &error));
  g_assert_no_error (error);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hi!'. Thanks!");
  g_free (s);

  /* test sync method invocation with exception */
  g_assert (!test_frob_hello_world_sync (TEST_QUERY_INTERFACE_FROB (object_proxy), 0, /* flags */
                                         "Yo",
                                         &s,
                                         NULL,
                                         &error));
  g_assert_error (error, TEST_ERROR, TEST_ERROR_FLUX_CAPACITOR_FAILURE);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking com.example.Frob.HelloWorld() on /com/example/TestObject at name com.example.TestService: com.example.Error.FluxCapacitorFailure: Yo is not a proper greeting");
  g_error_free (error);
  error = NULL;

  /* test async method invocation */
  test_frob_hello_world (TEST_QUERY_INTERFACE_FROB (object_proxy), 0, /* flags */
                         "Hi Async!",
                         NULL,
                         (GAsyncReadyCallback) hello_world_cb,
                         &s);
  g_main_loop_run (loop);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hi Async!'. Thanks!");
  g_free (s);

  /* test async method invocation with exception */
  test_frob_hello_world (TEST_QUERY_INTERFACE_FROB (object_proxy), 0, /* flags */
                         "Yo",
                         NULL,
                         (GAsyncReadyCallback) hello_world_cb,
                         &s);
  g_main_loop_run (loop);
  g_assert (s == NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_primitive_types (void)
{
  GError *error;
  guint8 ret_byte;
  gboolean ret_boolean;
  gint16 ret_int16;
  guint16 ret_uint16;
  gint32 ret_int32;
  guint32 ret_uint32;
  gint64 ret_int64;
  guint64 ret_uint64;
  double ret_double;
  gchar *ret_string;
  gchar *ret_objpath;
  gchar *ret_sig;

  error = NULL;

  g_assert (test_frob_test_primitive_types_sync (TEST_QUERY_INTERFACE_FROB (object_proxy), 0, /* flags */
                                                 1,
                                                 FALSE,
                                                 160,
                                                 161,
                                                 320,
                                                 321,
                                                 640,
                                                 641,
                                                 4.5,
                                                 "somestring",
                                                 "/some/objpath",
                                                 "(sig)",
                                                 &ret_byte,
                                                 &ret_boolean,
                                                 &ret_int16,
                                                 &ret_uint16,
                                                 &ret_int32,
                                                 &ret_uint32,
                                                 &ret_int64,
                                                 &ret_uint64,
                                                 &ret_double,
                                                 &ret_string,
                                                 &ret_objpath,
                                                 &ret_sig,
                                                 NULL,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmphex   (ret_byte,    ==, 1 + 1);
  g_assert_cmpint   (ret_boolean, ==, TRUE);
  g_assert_cmpint   (ret_int16,   ==, 160 + 1);
  g_assert_cmpuint  (ret_uint16,  ==, 161 + 1);
  g_assert_cmpint   (ret_int32,   ==, 320 + 1);
  g_assert_cmpuint  (ret_uint32,  ==, 321 + 1);
  g_assert_cmpint   (ret_int64,   ==, 640 + 1);
  g_assert_cmpuint  (ret_uint64,  ==, 641 + 1);
  g_assert_cmpfloat (ret_double,  ==, -4.5);
  g_assert_cmpstr   (ret_string,  ==, "somestringsomestring");
  g_assert_cmpstr   (ret_objpath, ==, "/some/objpath/modified");
  g_assert_cmpstr   (ret_sig,     ==, "((sig))");
  g_free (ret_string);
  g_free (ret_objpath);
  g_free (ret_sig);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_array_of_primitive_types (void)
{
  GError *error;
  EggDBusArraySeq  *array_byte;
  EggDBusArraySeq  *array_boolean;
  EggDBusArraySeq  *array_int16;
  EggDBusArraySeq  *array_uint16;
  EggDBusArraySeq  *array_int32;
  EggDBusArraySeq  *array_uint32;
  EggDBusArraySeq  *array_int64;
  EggDBusArraySeq  *array_uint64;
  EggDBusArraySeq  *array_double;
  gchar   *array_string[] = {"foo", "foo2", NULL};
  gchar   *array_objpath[] = {"/foo", "/foo2", NULL};
  gchar   *array_sig[] = {"(ii)", "s", NULL};
  EggDBusArraySeq  *ret_array_byte;
  EggDBusArraySeq  *ret_array_boolean;
  EggDBusArraySeq  *ret_array_int16;
  EggDBusArraySeq  *ret_array_uint16;
  EggDBusArraySeq  *ret_array_int32;
  EggDBusArraySeq  *ret_array_uint32;
  EggDBusArraySeq  *ret_array_int64;
  EggDBusArraySeq  *ret_array_uint64;
  EggDBusArraySeq  *ret_array_double;
  gchar  **ret_array_string;
  gchar  **ret_array_objpath;
  gchar  **ret_array_sig;

  error = NULL;

  array_byte    = egg_dbus_array_seq_new (G_TYPE_UCHAR, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_byte,    1);
  egg_dbus_array_seq_add_fixed (array_byte,    2);

  array_boolean = egg_dbus_array_seq_new (G_TYPE_BOOLEAN, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_boolean, FALSE);
  egg_dbus_array_seq_add_fixed (array_boolean, TRUE);

  array_int16   = egg_dbus_array_seq_new (EGG_DBUS_TYPE_INT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int16,   160);
  egg_dbus_array_seq_add_fixed (array_int16,   170);

  array_uint16  = egg_dbus_array_seq_new (EGG_DBUS_TYPE_UINT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint16,  161);
  egg_dbus_array_seq_add_fixed (array_uint16,  171);

  array_int32   = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int32,   320);
  egg_dbus_array_seq_add_fixed (array_int32,   330);

  array_uint32  = egg_dbus_array_seq_new (G_TYPE_UINT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint32,  321);
  egg_dbus_array_seq_add_fixed (array_uint32,  331);

  array_int64   = egg_dbus_array_seq_new (G_TYPE_INT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int64,   640);
  egg_dbus_array_seq_add_fixed (array_int64,   650);

  array_uint64  = egg_dbus_array_seq_new (G_TYPE_UINT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint64,  641);
  egg_dbus_array_seq_add_fixed (array_uint64,  651);

  array_double  = egg_dbus_array_seq_new (G_TYPE_DOUBLE, NULL, NULL, NULL);
  egg_dbus_array_seq_add_float (array_double,  4.5);
  egg_dbus_array_seq_add_float (array_double,  5.5);

  g_assert (test_frob_test_array_of_primitive_types_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                          0, /* call flags */
                                                          array_byte,
                                                          array_boolean,
                                                          array_int16,
                                                          array_uint16,
                                                          array_int32,
                                                          array_uint32,
                                                          array_int64,
                                                          array_uint64,
                                                          array_double,
                                                          array_string,
                                                          array_objpath,
                                                          array_sig,
                                                          &ret_array_byte,
                                                          &ret_array_boolean,
                                                          &ret_array_int16,
                                                          &ret_array_uint16,
                                                          &ret_array_int32,
                                                          &ret_array_uint32,
                                                          &ret_array_int64,
                                                          &ret_array_uint64,
                                                          &ret_array_double,
                                                          &ret_array_string,
                                                          &ret_array_objpath,
                                                          &ret_array_sig,
                                                          NULL,
                                                          &error));
  g_assert_no_error (error);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_byte), ==, 4);
  g_assert_cmpint  (array_byte->data.v_byte[0], ==, ret_array_byte->data.v_byte[0]);
  g_assert_cmpint  (array_byte->data.v_byte[1], ==, ret_array_byte->data.v_byte[1]);
  g_assert_cmpint  (array_byte->data.v_byte[0], ==, ret_array_byte->data.v_byte[2]);
  g_assert_cmpint  (array_byte->data.v_byte[1], ==, ret_array_byte->data.v_byte[3]);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_boolean), ==, 4);
  g_assert_cmpint  (array_boolean->data.v_boolean[0], ==, ret_array_boolean->data.v_boolean[0]);
  g_assert_cmpint  (array_boolean->data.v_boolean[1], ==, ret_array_boolean->data.v_boolean[1]);
  g_assert_cmpint  (array_boolean->data.v_boolean[0], ==, ret_array_boolean->data.v_boolean[2]);
  g_assert_cmpint  (array_boolean->data.v_boolean[1], ==, ret_array_boolean->data.v_boolean[3]);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_int16), ==, 4);
  g_assert_cmpint  (array_int16->data.v_int16[0], ==, ret_array_int16->data.v_int16[0]);
  g_assert_cmpint  (array_int16->data.v_int16[1], ==, ret_array_int16->data.v_int16[1]);
  g_assert_cmpint  (array_int16->data.v_int16[0], ==, ret_array_int16->data.v_int16[2]);
  g_assert_cmpint  (array_int16->data.v_int16[1], ==, ret_array_int16->data.v_int16[3]);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_uint16), ==, 4);
  g_assert_cmpint  (array_uint16->data.v_uint16[0], ==, ret_array_uint16->data.v_uint16[0]);
  g_assert_cmpint  (array_uint16->data.v_uint16[1], ==, ret_array_uint16->data.v_uint16[1]);
  g_assert_cmpint  (array_uint16->data.v_uint16[0], ==, ret_array_uint16->data.v_uint16[2]);
  g_assert_cmpint  (array_uint16->data.v_uint16[1], ==, ret_array_uint16->data.v_uint16[3]);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_int32), ==, 4);
  g_assert_cmpint  (array_int32->data.v_int[0], ==, ret_array_int32->data.v_int[0]);
  g_assert_cmpint  (array_int32->data.v_int[1], ==, ret_array_int32->data.v_int[1]);
  g_assert_cmpint  (array_int32->data.v_int[0], ==, ret_array_int32->data.v_int[2]);
  g_assert_cmpint  (array_int32->data.v_int[1], ==, ret_array_int32->data.v_int[3]);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_uint32), ==, 4);
  g_assert_cmpint  (array_uint32->data.v_uint[0], ==, ret_array_uint32->data.v_uint[0]);
  g_assert_cmpint  (array_uint32->data.v_uint[1], ==, ret_array_uint32->data.v_uint[1]);
  g_assert_cmpint  (array_uint32->data.v_uint[0], ==, ret_array_uint32->data.v_uint[2]);
  g_assert_cmpint  (array_uint32->data.v_uint[1], ==, ret_array_uint32->data.v_uint[3]);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_int64), ==, 4);
  g_assert_cmpint  (array_int64->data.v_int64[0], ==, ret_array_int64->data.v_int64[0]);
  g_assert_cmpint  (array_int64->data.v_int64[1], ==, ret_array_int64->data.v_int64[1]);
  g_assert_cmpint  (array_int64->data.v_int64[0], ==, ret_array_int64->data.v_int64[2]);
  g_assert_cmpint  (array_int64->data.v_int64[1], ==, ret_array_int64->data.v_int64[3]);

  g_assert_cmpuint (egg_dbus_array_seq_get_size (ret_array_double), ==, 4);
  g_assert_cmpint  (array_double->data.v_double[0], ==, ret_array_double->data.v_double[0]);
  g_assert_cmpint  (array_double->data.v_double[1], ==, ret_array_double->data.v_double[1]);
  g_assert_cmpint  (array_double->data.v_double[0], ==, ret_array_double->data.v_double[2]);
  g_assert_cmpint  (array_double->data.v_double[1], ==, ret_array_double->data.v_double[3]);

  g_assert_cmpuint (g_strv_length (ret_array_string), ==, 4);
  g_assert_cmpstr  (array_string[0], ==, ret_array_string[0]);
  g_assert_cmpstr  (array_string[1], ==, ret_array_string[1]);
  g_assert_cmpstr  (array_string[0], ==, ret_array_string[2]);
  g_assert_cmpstr  (array_string[1], ==, ret_array_string[3]);

  g_assert_cmpuint (g_strv_length (ret_array_objpath), ==, 4);
  g_assert_cmpstr  (array_objpath[0], ==, ret_array_objpath[0]);
  g_assert_cmpstr  (array_objpath[1], ==, ret_array_objpath[1]);
  g_assert_cmpstr  (array_objpath[0], ==, ret_array_objpath[2]);
  g_assert_cmpstr  (array_objpath[1], ==, ret_array_objpath[3]);

  g_assert_cmpuint (g_strv_length (ret_array_sig), ==, 4);
  g_assert_cmpstr  (array_sig[0], ==, ret_array_sig[0]);
  g_assert_cmpstr  (array_sig[1], ==, ret_array_sig[1]);
  g_assert_cmpstr  (array_sig[0], ==, ret_array_sig[2]);
  g_assert_cmpstr  (array_sig[1], ==, ret_array_sig[3]);

  /* free data passed in */
  g_object_unref (array_byte);
  g_object_unref (array_boolean);
  g_object_unref (array_int16);
  g_object_unref (array_uint16);
  g_object_unref (array_int32);
  g_object_unref (array_uint32);
  g_object_unref (array_int64);
  g_object_unref (array_uint64);
  g_object_unref (array_double);

  /* free returned data */
  g_object_unref (ret_array_byte);
  g_object_unref (ret_array_boolean);
  g_object_unref (ret_array_int16);
  g_object_unref (ret_array_uint16);
  g_object_unref (ret_array_int32);
  g_object_unref (ret_array_uint32);
  g_object_unref (ret_array_int64);
  g_object_unref (ret_array_uint64);
  g_object_unref (ret_array_double);
  g_strfreev (ret_array_string);
  g_strfreev (ret_array_objpath);
  g_strfreev (ret_array_sig);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_structure_types (void)
{
  GError *error;
  TestPoint *point;
  TestPoint *point2;
  TestDescribedPoint *dpoint;
  TestPoint *res_point;
  TestPoint *res_point2;
  TestDescribedPoint *res_dpoint;
  const char *s;

  error = NULL;

  point = test_point_new (10, 11);
  point2 = test_point_new (100, 101);
  dpoint = test_described_point_new ("described point", point2);

  g_assert (test_frob_test_structure_types_sync (TEST_QUERY_INTERFACE_FROB (object_proxy), 0, /* flags */
                                                 point,
                                                 dpoint,
                                                 &res_point,
                                                 &res_dpoint,
                                                 NULL,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmpint (test_point_get_x (point) + 1, ==, test_point_get_x (res_point));
  g_assert_cmpint (test_point_get_y (point) + 1, ==, test_point_get_y (res_point));

  res_point2 = test_described_point_get_point (res_dpoint);
  g_assert_cmpint (test_point_get_x (point2) + 2, ==, test_point_get_x (res_point2));
  g_assert_cmpint (test_point_get_y (point2) + 2, ==, test_point_get_y (res_point2));

  s = test_described_point_get_desc (res_dpoint);
  g_assert_cmpstr (s, ==, "replaced desc");

  g_object_unref (res_dpoint);
  g_object_unref (res_point);
  g_object_unref (dpoint);
  g_object_unref (point2);
  g_object_unref (point);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_array_of_structure_types (void)
{
  GError *error;
  TestPoint *point;
  EggDBusArraySeq *seq;
  EggDBusArraySeq *ret_seq;
  guint n;

  error = NULL;

  seq = egg_dbus_array_seq_new (TEST_TYPE_EXTENDED_DESCRIBED_POINT, g_object_unref, NULL, NULL);

  point = test_point_new (10, 15);
  egg_dbus_array_seq_add (seq, test_extended_described_point_new ("foo", point, "ext"));
  g_object_unref (point);

  point = test_point_new (20, 25);
  egg_dbus_array_seq_add (seq, test_extended_described_point_new ("foo2", point, "ext1"));
  g_object_unref (point);

  point = test_point_new (30, 35);
  egg_dbus_array_seq_add (seq, test_extended_described_point_new ("foo4", point, "ext2"));
  g_object_unref (point);

  g_assert (test_frob_test_array_of_structure_types_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                          0, /* flags */
                                                          seq,
                                                          &ret_seq,
                                                          NULL,
                                                          &error));
  g_assert_no_error (error);

  g_assert_cmpint (egg_dbus_array_seq_get_size (ret_seq), == , 3);

  for (n = 0; n < 3; n++)
    {
      TestPoint *res_point;
      TestExtendedDescribedPoint *edpoint;
      TestExtendedDescribedPoint *res_edpoint;
      const char *res_s;
      char *p;
      const char *s;

      edpoint = TEST_EXTENDED_DESCRIBED_POINT (egg_dbus_array_seq_get (seq, n));
      res_edpoint = TEST_EXTENDED_DESCRIBED_POINT (egg_dbus_array_seq_get (ret_seq, n));

      point = test_extended_described_point_get_point (edpoint);
      res_point = test_extended_described_point_get_point (res_edpoint);
      g_assert_cmpint (test_point_get_x (point) + 1, ==, test_point_get_x (res_point));
      g_assert_cmpint (test_point_get_y (point) + 1, ==, test_point_get_y (res_point));

      s = test_extended_described_point_get_desc (edpoint);
      res_s = test_extended_described_point_get_desc (res_edpoint);
      p = g_strdup_printf ("%s_%d", s, n);
      g_assert_cmpstr (p, ==, res_s);
      g_free (p);

      s = test_extended_described_point_get_ext_desc (edpoint);
      res_s = test_extended_described_point_get_ext_desc (res_edpoint);
      p = g_strdup_printf ("%s_%d", s, n);
      g_assert_cmpstr (p, ==, res_s);
      g_free (p);

    }
  g_object_unref (seq);
  g_object_unref (ret_seq);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_hash_tables (void)
{
  GError *error;
  EggDBusHashMap *hss;
  EggDBusHashMap *hoo;
  EggDBusHashMap *hii;
  EggDBusHashMap *hyy;
  EggDBusHashMap *hnn;
  EggDBusHashMap *hqq;
  EggDBusHashMap *huu;
  EggDBusHashMap *hbb;
  EggDBusHashMap *hxx;
  EggDBusHashMap *htt;
  EggDBusHashMap *hdd;
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

  error = NULL;

  hss = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, G_TYPE_STRING, NULL);
  egg_dbus_hash_map_insert (hss, "foo0", "bar0");
  egg_dbus_hash_map_insert (hss, "foo1", "bar1");

  hoo = egg_dbus_hash_map_new (EGG_DBUS_TYPE_OBJECT_PATH, NULL, EGG_DBUS_TYPE_OBJECT_PATH, NULL);
  egg_dbus_hash_map_insert (hoo, "/foo/0", "/bar/0");
  egg_dbus_hash_map_insert (hoo, "/foo/1", "/bar/1");

  hii = egg_dbus_hash_map_new (G_TYPE_INT, NULL, G_TYPE_INT, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (hii, 10, 100);
  egg_dbus_hash_map_insert_fixed_fixed (hii, 11, 110);
  egg_dbus_hash_map_insert_fixed_fixed (hii, 12, 120);

  hyy = egg_dbus_hash_map_new (G_TYPE_UCHAR, NULL, G_TYPE_UCHAR, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (hyy, 10, 100);
  egg_dbus_hash_map_insert_fixed_fixed (hyy, 11, 110);
  egg_dbus_hash_map_insert_fixed_fixed (hyy, 12, 120);

  hnn = egg_dbus_hash_map_new (EGG_DBUS_TYPE_INT16, NULL, EGG_DBUS_TYPE_INT16, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (hnn, 10, 100);
  egg_dbus_hash_map_insert_fixed_fixed (hnn, 11, 110);
  egg_dbus_hash_map_insert_fixed_fixed (hnn, 12, 120);

  hqq = egg_dbus_hash_map_new (EGG_DBUS_TYPE_UINT16, NULL, EGG_DBUS_TYPE_UINT16, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (hqq, 10, 100);
  egg_dbus_hash_map_insert_fixed_fixed (hqq, 11, 110);
  egg_dbus_hash_map_insert_fixed_fixed (hqq, 12, 120);

  huu = egg_dbus_hash_map_new (G_TYPE_UINT, NULL, G_TYPE_UINT, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (huu, 10, 100);
  egg_dbus_hash_map_insert_fixed_fixed (huu, 11, 110);
  egg_dbus_hash_map_insert_fixed_fixed (huu, 12, 120);

  hbb = egg_dbus_hash_map_new (G_TYPE_BOOLEAN, NULL, G_TYPE_BOOLEAN, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (hbb, TRUE, TRUE);
  egg_dbus_hash_map_insert_fixed_fixed (hbb, FALSE, FALSE);

  hxx = egg_dbus_hash_map_new (G_TYPE_INT64, NULL, G_TYPE_INT64, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (hxx, 100, 200);
  egg_dbus_hash_map_insert_fixed_fixed (hxx, 0x200000000, 0x300000000);

  htt = egg_dbus_hash_map_new (G_TYPE_UINT64, NULL, G_TYPE_UINT64, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (htt, 400, 500);
  egg_dbus_hash_map_insert_fixed_fixed (htt, 0x600000000, 0x700000000);

  hdd = egg_dbus_hash_map_new (G_TYPE_DOUBLE, NULL, G_TYPE_DOUBLE, NULL);
  egg_dbus_hash_map_insert_float_float (hdd, 0.5, 1.0);
  egg_dbus_hash_map_insert_float_float (hdd, 2.5, 2.6);

  g_assert (test_frob_test_hash_tables_sync (TEST_QUERY_INTERFACE_FROB (object_proxy), 0, /* flags */
                                             hss,
                                             hoo,
                                             hii,
                                             hyy,
                                             hnn,
                                             hqq,
                                             huu,
                                             hbb,
                                             hxx,
                                             htt,
                                             hdd,
                                             &ret_hss,
                                             &ret_hoo,
                                             &ret_hii,
                                             &ret_hyy,
                                             &ret_hnn,
                                             &ret_hqq,
                                             &ret_huu,
                                             &ret_hbb,
                                             &ret_hxx,
                                             &ret_htt,
                                             &ret_hdd,
                                             NULL,
                                             &error));
  g_assert_no_error (error);

  g_object_unref (hss);
  g_object_unref (hoo);
  g_object_unref (hii);
  g_object_unref (hyy);
  g_object_unref (hnn);
  g_object_unref (hqq);
  g_object_unref (huu);
  g_object_unref (htt);
  g_object_unref (hxx);
  g_object_unref (hdd);
  g_object_unref (hbb);

  g_assert_cmpstr (egg_dbus_hash_map_lookup (ret_hss, "foo0mod"), ==, "bar0bar0");
  g_assert_cmpstr (egg_dbus_hash_map_lookup (ret_hss, "foo1mod"), ==, "bar1bar1");

  g_assert_cmpstr (egg_dbus_hash_map_lookup (ret_hoo, "/foo/0/mod"), ==, "/bar/0/mod2");
  g_assert_cmpstr (egg_dbus_hash_map_lookup (ret_hoo, "/foo/1/mod"), ==, "/bar/1/mod2");

  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hii, 10*2), ==, 100*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hii, 11*2), ==, 110*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hii, 12*2), ==, 120*3);

  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hyy, 10*2), ==, (100*3 & 255));
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hyy, 11*2), ==, (110*3 & 255));
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hyy, 12*2), ==, (120*3 & 255));

  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hnn, 10*2), ==, 100*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hnn, 11*2), ==, 110*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hnn, 12*2), ==, 120*3);

  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hqq, 10*2), ==, 100*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hqq, 11*2), ==, 110*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hqq, 12*2), ==, 120*3);

  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_huu, 10*2), ==, 100*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_huu, 11*2), ==, 110*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_huu, 12*2), ==, 120*3);

  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hxx, 100*2), ==, 200*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_hxx, 0x200000000*2), ==, 0x300000000*3);

  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_htt, 400*2), ==, 500*3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (ret_htt, 0x600000000*2), ==, 0x700000000*3);

  g_assert_cmpint (egg_dbus_hash_map_lookup_float_float (ret_hdd, 0.5 + 2.5), ==, 1.0 + 5.0);
  g_assert_cmpint (egg_dbus_hash_map_lookup_float_float (ret_hdd, 2.5 + 2.5), ==, 2.6 + 5.0);

  g_assert (egg_dbus_hash_map_lookup_fixed_fixed (ret_hbb, TRUE) == TRUE);
  g_assert (egg_dbus_hash_map_lookup_fixed_fixed (ret_hbb, FALSE) == TRUE);

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

/* ---------------------------------------------------------------------------------------------------- */

static void
test_hash_tables_of_arrays (void)
{
  GError *error;
  EggDBusArraySeq  *array_byte;
  EggDBusArraySeq  *array_boolean;
  EggDBusArraySeq  *array_int16;
  EggDBusArraySeq  *array_uint16;
  EggDBusArraySeq  *array_int32;
  EggDBusArraySeq  *array_uint32;
  EggDBusArraySeq  *array_int64;
  EggDBusArraySeq  *array_uint64;
  EggDBusArraySeq  *array_double;
  gchar   *array_string[] = {"foo", "foo2", NULL};
  gchar   *array_objpath[] = {"/foo", "/foo2", NULL};
  gchar   *array_string_other[] = {"bar", "bar2", NULL};
  gchar   *array_objpath_other[] = {"/bar", "/bar2", NULL};
  EggDBusArraySeq *site1_points;
  EggDBusArraySeq *site2_points;
  EggDBusHashMap *hsas;
  EggDBusHashMap *hsao;
  EggDBusHashMap *hsai;
  EggDBusHashMap *hsay;
  EggDBusHashMap *hsan;
  EggDBusHashMap *hsaq;
  EggDBusHashMap *hsau;
  EggDBusHashMap *hsab;
  EggDBusHashMap *hsax;
  EggDBusHashMap *hsat;
  EggDBusHashMap *hsad;
  EggDBusHashMap *hash_of_point_arrays;
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
  gchar   **ret_array_string;
  gchar   **ret_array_objpath;
  EggDBusArraySeq *ret_array;
  TestPoint *point;

  error = NULL;

  array_byte    = egg_dbus_array_seq_new (G_TYPE_UCHAR, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_byte,    1);
  egg_dbus_array_seq_add_fixed (array_byte,    2);

  array_boolean = egg_dbus_array_seq_new (G_TYPE_BOOLEAN, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_boolean, FALSE);
  egg_dbus_array_seq_add_fixed (array_boolean, TRUE);

  array_int16   = egg_dbus_array_seq_new (EGG_DBUS_TYPE_INT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int16,   160);
  egg_dbus_array_seq_add_fixed (array_int16,   170);

  array_uint16  = egg_dbus_array_seq_new (EGG_DBUS_TYPE_UINT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint16,  161);
  egg_dbus_array_seq_add_fixed (array_uint16,  171);

  array_int32   = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int32,   320);
  egg_dbus_array_seq_add_fixed (array_int32,   330);

  array_uint32  = egg_dbus_array_seq_new (G_TYPE_UINT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint32,  321);
  egg_dbus_array_seq_add_fixed (array_uint32,  331);

  array_int64   = egg_dbus_array_seq_new (G_TYPE_INT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int64,   640);
  egg_dbus_array_seq_add_fixed (array_int64,   650);

  array_uint64  = egg_dbus_array_seq_new (G_TYPE_UINT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint64,  641);
  egg_dbus_array_seq_add_fixed (array_uint64,  651);

  array_double  = egg_dbus_array_seq_new (G_TYPE_DOUBLE, NULL, NULL, NULL);
  egg_dbus_array_seq_add_float (array_double,  4.5);
  egg_dbus_array_seq_add_float (array_double,  5.5);

  hsas = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, G_TYPE_STRV, NULL);
  egg_dbus_hash_map_insert (hsas, "key0", array_string);
  egg_dbus_hash_map_insert (hsas, "key1", array_string_other);

  hsao = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_OBJECT_PATH_ARRAY, NULL);
  egg_dbus_hash_map_insert (hsao, "key0", array_objpath);
  egg_dbus_hash_map_insert (hsao, "key1", array_objpath_other);

  hsai = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsai, "key", array_int32);

  hsay = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsay, "key", array_byte);

  hsan = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsan, "key", array_int16);

  hsaq = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsaq, "key", array_uint16);

  hsau = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsau, "key", array_uint32);

  hsab = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsab, "key", array_boolean);

  hsax = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsax, "key", array_int64);

  hsat = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsat, "key", array_uint64);

  hsad = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  egg_dbus_hash_map_insert (hsad, "key", array_double);

  hash_of_point_arrays = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref);
  site1_points = egg_dbus_array_seq_new (TEST_TYPE_POINT, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (site1_points, test_point_new (1000, 1001));
  egg_dbus_array_seq_add (site1_points, test_point_new (1010, 1011));
  egg_dbus_hash_map_insert (hash_of_point_arrays, "site1", site1_points);
  site2_points = egg_dbus_array_seq_new (TEST_TYPE_POINT, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (site2_points, test_point_new (2000, 2001));
  egg_dbus_array_seq_add (site2_points, test_point_new (2010, 2011));
  egg_dbus_array_seq_add (site2_points, test_point_new (2020, 2021));
  egg_dbus_hash_map_insert (hash_of_point_arrays, "site2", site2_points);

  g_assert (test_frob_test_hash_tables_of_arrays_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                       0, /* flags */
                                                       hsas,
                                                       hsao,
                                                       hsai,
                                                       hsay,
                                                       hsan,
                                                       hsaq,
                                                       hsau,
                                                       hsab,
                                                       hsax,
                                                       hsat,
                                                       hsad,
                                                       hash_of_point_arrays,
                                                       &ret_hsas,
                                                       &ret_hsao,
                                                       &ret_hsai,
                                                       &ret_hsay,
                                                       &ret_hsan,
                                                       &ret_hsaq,
                                                       &ret_hsau,
                                                       &ret_hsab,
                                                       &ret_hsax,
                                                       &ret_hsat,
                                                       &ret_hsad,
                                                       &ret_hash_of_point_arrays,
                                                       NULL,
                                                       &error));
  g_assert_no_error (error);
  /* free data passed in */
  g_object_unref (hsas);
  g_object_unref (hsao);
  g_object_unref (hsai);
  g_object_unref (hsay);
  g_object_unref (hsan);
  g_object_unref (hsaq);
  g_object_unref (hsau);
  g_object_unref (hsab);
  g_object_unref (hsax);
  g_object_unref (hsat);
  g_object_unref (hsad);
  g_object_unref (hash_of_point_arrays);

  ret_array_string = egg_dbus_hash_map_lookup (ret_hsas, "key0_mod");
  g_assert (g_strv_length (ret_array_string) == 2);
  g_assert_cmpstr (ret_array_string[0], ==, "foo");
  g_assert_cmpstr (ret_array_string[1], ==, "foo2");
  ret_array_string = egg_dbus_hash_map_lookup (ret_hsas, "key1_mod");
  g_assert (g_strv_length (ret_array_string) == 2);
  g_assert_cmpstr (ret_array_string[0], ==, "bar");
  g_assert_cmpstr (ret_array_string[1], ==, "bar2");

  ret_array_objpath = egg_dbus_hash_map_lookup (ret_hsao, "key0_mod");
  g_assert (g_strv_length (ret_array_objpath) == 2);
  g_assert_cmpstr (ret_array_objpath[0], ==, "/foo");
  g_assert_cmpstr (ret_array_objpath[1], ==, "/foo2");
  ret_array_objpath = egg_dbus_hash_map_lookup (ret_hsao, "key1_mod");
  g_assert (g_strv_length (ret_array_objpath) == 2);
  g_assert_cmpstr (ret_array_objpath[0], ==, "/bar");
  g_assert_cmpstr (ret_array_objpath[1], ==, "/bar2");

  g_assert (egg_dbus_hash_map_lookup (ret_hsai, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsai, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  1 &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  2);

  g_assert (egg_dbus_hash_map_lookup (ret_hsay, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsay, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  3 &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  4);

  g_assert (egg_dbus_hash_map_lookup (ret_hsan, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsan, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  5 &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  6);

  g_assert (egg_dbus_hash_map_lookup (ret_hsaq, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsaq, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  7 &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  8);

  g_assert (egg_dbus_hash_map_lookup (ret_hsau, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsau, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  9 &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  10);

  g_assert (egg_dbus_hash_map_lookup (ret_hsab, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsab, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 3 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  TRUE &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  FALSE &&
            egg_dbus_array_seq_get_fixed (ret_array, 2) ==  TRUE);

  g_assert (egg_dbus_hash_map_lookup (ret_hsax, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsax, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  11 &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  12);

  g_assert (egg_dbus_hash_map_lookup (ret_hsat, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsat, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_fixed (ret_array, 0) ==  13 &&
            egg_dbus_array_seq_get_fixed (ret_array, 1) ==  14);

  g_assert (egg_dbus_hash_map_lookup (ret_hsad, "non-existent-key") == NULL);
  ret_array = egg_dbus_hash_map_lookup (ret_hsad, "k");
  g_assert (egg_dbus_array_seq_get_size (ret_array) == 2 &&
            egg_dbus_array_seq_get_float (ret_array, 0) ==  15.5 &&
            egg_dbus_array_seq_get_float (ret_array, 1) ==  16.5);

  site1_points = egg_dbus_hash_map_lookup (ret_hash_of_point_arrays, "site1_new");
  g_assert (egg_dbus_array_seq_get_size (site1_points) == 2);
  point = TEST_POINT (egg_dbus_array_seq_get (site1_points, 0));
  g_assert_cmpint (test_point_get_x (point), ==, 1000 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 1001 + 200);
  point = TEST_POINT (egg_dbus_array_seq_get (site1_points, 1));
  g_assert_cmpint (test_point_get_x (point), ==, 1010 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 1011 + 200);

  site2_points = egg_dbus_hash_map_lookup (ret_hash_of_point_arrays, "site2_new");
  g_assert (egg_dbus_array_seq_get_size (site2_points) == 3);
  point = TEST_POINT (egg_dbus_array_seq_get (site2_points, 0));
  g_assert_cmpint (test_point_get_x (point), ==, 2000 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 2001 + 200);
  point = TEST_POINT (egg_dbus_array_seq_get (site2_points, 1));
  g_assert_cmpint (test_point_get_x (point), ==, 2010 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 2011 + 200);
  point = TEST_POINT (egg_dbus_array_seq_get (site2_points, 2));
  g_assert_cmpint (test_point_get_x (point), ==, 2020 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 2021 + 200);

  /* free returned data */
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

/* ---------------------------------------------------------------------------------------------------- */

static void
test_hash_table_of_structures (void)
{
  GError *error;
  EggDBusHashMap *hash_of_points;
  EggDBusHashMap *hash_of_described_points;
  TestPoint *point;
  TestDescribedPoint *dpoint;
  const char *s;

  error = NULL;

  hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, TEST_TYPE_POINT, g_object_unref);
  egg_dbus_hash_map_insert (hash_of_points, "alpha", test_point_new (10, 10));
  egg_dbus_hash_map_insert (hash_of_points, "beta",  test_point_new (12, 12));
  egg_dbus_hash_map_insert (hash_of_points, "gamma", test_point_new (14, 14));

  g_assert (test_frob_test_hash_table_of_structures_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                          0, /* flags */
                                                          hash_of_points,
                                                          &hash_of_described_points,
                                                          NULL,
                                                          &error));
  g_assert_no_error (error);

  dpoint = egg_dbus_hash_map_lookup (hash_of_described_points, "alpha_new");
  s = test_described_point_get_desc (dpoint);
  point = test_described_point_get_point (dpoint);
  g_assert_cmpstr (s, ==, "alpha");
  g_assert_cmpint (test_point_get_x (point), ==, 10 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 10 + 200);

  dpoint = egg_dbus_hash_map_lookup (hash_of_described_points, "beta_new");
  s = test_described_point_get_desc (dpoint);
  point = test_described_point_get_point (dpoint);
  g_assert_cmpstr (s, ==, "beta");
  g_assert_cmpint (test_point_get_x (point), ==, 12 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 12 + 200);

  dpoint = egg_dbus_hash_map_lookup (hash_of_described_points, "gamma_new");
  s = test_described_point_get_desc (dpoint);
  point = test_described_point_get_point (dpoint);
  g_assert_cmpstr (s, ==, "gamma");
  g_assert_cmpint (test_point_get_x (point), ==, 14 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 14 + 200);

  g_object_unref (hash_of_points);
  g_object_unref (hash_of_described_points);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_hash_table_of_hash_table_of_structures (void)
{
  GError *error;
  EggDBusHashMap *hash_of_points;
  EggDBusHashMap *hash_of_hash_of_points;
  EggDBusHashMap *ret_hash_of_hash_of_points;
  TestPoint *point;

  error = NULL;

  hash_of_hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, EGG_DBUS_TYPE_HASH_MAP, g_object_unref);

  hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, TEST_TYPE_POINT, g_object_unref);
  egg_dbus_hash_map_insert (hash_of_points, "alpha", test_point_new (10, 10));
  egg_dbus_hash_map_insert (hash_of_points, "beta",  test_point_new (12, 12));
  egg_dbus_hash_map_insert (hash_of_points, "gamma", test_point_new (14, 14));
  egg_dbus_hash_map_insert (hash_of_hash_of_points, "org1", hash_of_points);

  hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, TEST_TYPE_POINT, g_object_unref);
  egg_dbus_hash_map_insert (hash_of_points, "rip", test_point_new (1, 1));
  egg_dbus_hash_map_insert (hash_of_points, "rap", test_point_new (2, 2));
  egg_dbus_hash_map_insert (hash_of_points, "rup", test_point_new (3, 3));
  egg_dbus_hash_map_insert (hash_of_hash_of_points, "org2", hash_of_points);

  g_assert (test_frob_test_hash_table_of_hash_tables_of_structures_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                                         0, /* flags */
                                                                         hash_of_hash_of_points,
                                                                         &ret_hash_of_hash_of_points,
                                                                         NULL,
                                                                         &error));
  g_assert_no_error (error);
  g_object_unref (hash_of_hash_of_points);

  hash_of_points = egg_dbus_hash_map_lookup (ret_hash_of_hash_of_points, "org1_new");
  point = egg_dbus_hash_map_lookup (hash_of_points, "alpha_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 10 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 10 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "beta_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 12 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 12 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "gamma_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 14 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 14 + 200);

  hash_of_points = egg_dbus_hash_map_lookup (ret_hash_of_hash_of_points, "org2_new");
  point = egg_dbus_hash_map_lookup (hash_of_points, "rip_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 1 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 1 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "rap_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 2 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 2 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "rup_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 3 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 3 + 200);

  g_object_unref (ret_hash_of_hash_of_points);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_array_of_hash_table_of_structures (void)
{
  GError *error;
  EggDBusHashMap *hash_of_points;
  EggDBusArraySeq *list_of_hashes;
  EggDBusArraySeq *ret_list_of_hashes;
  TestPoint *point;

  error = NULL;

  list_of_hashes = egg_dbus_array_seq_new (EGG_DBUS_TYPE_HASH_MAP, g_object_unref, NULL, NULL);

  hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, TEST_TYPE_POINT, g_object_unref);
  egg_dbus_hash_map_insert (hash_of_points, "alpha", test_point_new (10, 10));
  egg_dbus_hash_map_insert (hash_of_points, "beta",  test_point_new (12, 12));
  egg_dbus_hash_map_insert (hash_of_points, "gamma", test_point_new (14, 14));
  egg_dbus_array_seq_add (list_of_hashes, hash_of_points);

  hash_of_points = egg_dbus_hash_map_new (G_TYPE_STRING, NULL, TEST_TYPE_POINT, g_object_unref);
  egg_dbus_hash_map_insert (hash_of_points, "rip", test_point_new (1, 1));
  egg_dbus_hash_map_insert (hash_of_points, "rap", test_point_new (2, 2));
  egg_dbus_hash_map_insert (hash_of_points, "rup", test_point_new (3, 3));
  egg_dbus_array_seq_add (list_of_hashes, hash_of_points);

  g_assert (test_frob_test_array_of_hash_tables_of_structures_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                                    0, /* flags */
                                                                    list_of_hashes,
                                                                    &ret_list_of_hashes,
                                                                    NULL,
                                                                    &error));
  g_assert_no_error (error);
  g_object_unref (list_of_hashes);

  g_assert (egg_dbus_array_seq_get_size (ret_list_of_hashes) == 2);

  hash_of_points = egg_dbus_array_seq_get (ret_list_of_hashes, 0);
  point = egg_dbus_hash_map_lookup (hash_of_points, "alpha_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 10 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 10 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "beta_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 12 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 12 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "gamma_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 14 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 14 + 200);

  hash_of_points = egg_dbus_array_seq_get (ret_list_of_hashes, 1);
  point = egg_dbus_hash_map_lookup (hash_of_points, "rip_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 1 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 1 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "rap_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 2 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 2 + 200);
  point = egg_dbus_hash_map_lookup (hash_of_points, "rup_new_new");
  g_assert_cmpint (test_point_get_x (point), ==, 3 + 100);
  g_assert_cmpint (test_point_get_y (point), ==, 3 + 200);

  g_object_unref (ret_list_of_hashes);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_array_of_arrays (void)
{
  GError *error;
  EggDBusArraySeq  *array_byte;
  EggDBusArraySeq  *array_boolean;
  EggDBusArraySeq  *array_int16;
  EggDBusArraySeq  *array_uint16;
  EggDBusArraySeq  *array_int32;
  EggDBusArraySeq  *array_uint32;
  EggDBusArraySeq  *array_int64;
  EggDBusArraySeq  *array_uint64;
  EggDBusArraySeq  *array_double;
  gchar   *array_string[] = {"foo", "foo2", NULL};
  gchar   *array_objpath[] = {"/foo", "/foo2", NULL};
  gchar   *array_string_other[] = {"bar", "bar2", NULL};
  gchar   *array_objpath_other[] = {"/bar", "/bar2", NULL};
  EggDBusArraySeq *array_of_array_of_string;
  EggDBusArraySeq *array_of_array_of_objpath;
  EggDBusArraySeq *array_of_array_of_byte;
  EggDBusArraySeq *array_of_array_of_boolean;
  EggDBusArraySeq *array_of_array_of_int16;
  EggDBusArraySeq *array_of_array_of_uint16;
  EggDBusArraySeq *array_of_array_of_int32;
  EggDBusArraySeq *array_of_array_of_uint32;
  EggDBusArraySeq *array_of_array_of_int64;
  EggDBusArraySeq *array_of_array_of_uint64;
  EggDBusArraySeq *array_of_array_of_double;
  EggDBusArraySeq *array_of_array_of_point;
  EggDBusArraySeq *array_of_array_of_array_of_string;
  EggDBusArraySeq *ret_array_of_array_of_string;
  EggDBusArraySeq *ret_array_of_array_of_objpath;
  EggDBusArraySeq *ret_array_of_array_of_byte;
  EggDBusArraySeq *ret_array_of_array_of_boolean;
  EggDBusArraySeq *ret_array_of_array_of_int16;
  EggDBusArraySeq *ret_array_of_array_of_uint16;
  EggDBusArraySeq *ret_array_of_array_of_int32;
  EggDBusArraySeq *ret_array_of_array_of_uint32;
  EggDBusArraySeq *ret_array_of_array_of_int64;
  EggDBusArraySeq *ret_array_of_array_of_uint64;
  EggDBusArraySeq *ret_array_of_array_of_double;
  EggDBusArraySeq *ret_array_of_array_of_point;
  EggDBusArraySeq *ret_array_of_array_of_array_of_string;
  EggDBusArraySeq *array_of_point;

  error = NULL;

  array_byte    = egg_dbus_array_seq_new (G_TYPE_UCHAR, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_byte,    1);
  egg_dbus_array_seq_add_fixed (array_byte,    2);

  array_boolean = egg_dbus_array_seq_new (G_TYPE_BOOLEAN, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_boolean, FALSE);
  egg_dbus_array_seq_add_fixed (array_boolean, TRUE);

  array_int16   = egg_dbus_array_seq_new (EGG_DBUS_TYPE_INT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int16,   160);
  egg_dbus_array_seq_add_fixed (array_int16,   170);

  array_uint16  = egg_dbus_array_seq_new (EGG_DBUS_TYPE_UINT16, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint16,  161);
  egg_dbus_array_seq_add_fixed (array_uint16,  171);

  array_int32   = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int32,   320);
  egg_dbus_array_seq_add_fixed (array_int32,   330);

  array_uint32  = egg_dbus_array_seq_new (G_TYPE_UINT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint32,  321);
  egg_dbus_array_seq_add_fixed (array_uint32,  331);

  array_int64   = egg_dbus_array_seq_new (G_TYPE_INT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_int64,   640);
  egg_dbus_array_seq_add_fixed (array_int64,   650);

  array_uint64  = egg_dbus_array_seq_new (G_TYPE_UINT64, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (array_uint64,  641);
  egg_dbus_array_seq_add_fixed (array_uint64,  651);

  array_double  = egg_dbus_array_seq_new (G_TYPE_DOUBLE, NULL, NULL, NULL);
  egg_dbus_array_seq_add_float (array_double,  4.5);
  egg_dbus_array_seq_add_float (array_double,  5.5);

  array_of_array_of_string = egg_dbus_array_seq_new (G_TYPE_STRV, NULL, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_string, array_string);
  egg_dbus_array_seq_add (array_of_array_of_string, array_string_other);

  array_of_array_of_objpath = egg_dbus_array_seq_new (EGG_DBUS_TYPE_OBJECT_PATH_ARRAY, NULL, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_objpath, array_objpath);
  egg_dbus_array_seq_add (array_of_array_of_objpath, array_objpath_other);


  array_of_array_of_byte = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_byte, g_object_ref (array_byte));
  egg_dbus_array_seq_add (array_of_array_of_byte, g_object_ref (array_byte));

  array_of_array_of_boolean = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_boolean, g_object_ref (array_boolean));
  egg_dbus_array_seq_add (array_of_array_of_boolean, g_object_ref (array_boolean));

  array_of_array_of_int16 = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_int16, g_object_ref (array_int16));
  egg_dbus_array_seq_add (array_of_array_of_int16, g_object_ref (array_int16));

  array_of_array_of_uint16 = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_uint16, g_object_ref (array_uint16));
  egg_dbus_array_seq_add (array_of_array_of_uint16, g_object_ref (array_uint16));

  array_of_array_of_int32 = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_int32, g_object_ref (array_int32));
  egg_dbus_array_seq_add (array_of_array_of_int32, g_object_ref (array_int32));

  array_of_array_of_uint32 = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_uint32, g_object_ref (array_uint32));
  egg_dbus_array_seq_add (array_of_array_of_uint32, g_object_ref (array_uint32));

  array_of_array_of_int64 = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_int64, g_object_ref (array_int64));
  egg_dbus_array_seq_add (array_of_array_of_int64, g_object_ref (array_int64));

  array_of_array_of_uint64 = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_uint64, g_object_ref (array_uint64));
  egg_dbus_array_seq_add (array_of_array_of_uint64, g_object_ref (array_uint64));

  array_of_array_of_double = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_double, g_object_ref (array_double));
  egg_dbus_array_seq_add (array_of_array_of_double, g_object_ref (array_double));

  array_of_array_of_array_of_string = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_array_of_array_of_string, g_object_ref (array_of_array_of_string));
  egg_dbus_array_seq_add (array_of_array_of_array_of_string, g_object_ref (array_of_array_of_string));

  array_of_array_of_point = egg_dbus_array_seq_new (EGG_DBUS_TYPE_ARRAY_SEQ, g_object_unref, NULL, NULL);
  array_of_point = egg_dbus_array_seq_new (TEST_TYPE_POINT, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (array_of_point, test_point_new (10, 10));
  egg_dbus_array_seq_add (array_of_point, test_point_new (20, 20));
  egg_dbus_array_seq_add (array_of_array_of_point, g_object_ref (array_of_point));
  egg_dbus_array_seq_add (array_of_array_of_point, g_object_ref (array_of_point));
  g_object_unref (array_of_point);

  /* done with the data we passed in */
  g_object_unref (array_byte);
  g_object_unref (array_boolean);
  g_object_unref (array_int16);
  g_object_unref (array_uint16);
  g_object_unref (array_int32);
  g_object_unref (array_uint32);
  g_object_unref (array_int64);
  g_object_unref (array_uint64);
  g_object_unref (array_double);

  g_assert (test_frob_test_array_of_arrays_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                 0, /* flags */
                                                 array_of_array_of_string,
                                                 array_of_array_of_point,
                                                 array_of_array_of_objpath,
                                                 array_of_array_of_int32,
                                                 array_of_array_of_byte,
                                                 array_of_array_of_int16,
                                                 array_of_array_of_uint16,
                                                 array_of_array_of_uint32,
                                                 array_of_array_of_boolean,
                                                 array_of_array_of_int64,
                                                 array_of_array_of_uint64,
                                                 array_of_array_of_double,
                                                 array_of_array_of_array_of_string,
                                                 &ret_array_of_array_of_string,
                                                 &ret_array_of_array_of_point,
                                                 &ret_array_of_array_of_objpath,
                                                 &ret_array_of_array_of_int32,
                                                 &ret_array_of_array_of_byte,
                                                 &ret_array_of_array_of_int16,
                                                 &ret_array_of_array_of_uint16,
                                                 &ret_array_of_array_of_uint32,
                                                 &ret_array_of_array_of_boolean,
                                                 &ret_array_of_array_of_int64,
                                                 &ret_array_of_array_of_uint64,
                                                 &ret_array_of_array_of_double,
                                                 &ret_array_of_array_of_array_of_string,
                                                 NULL,
                                                 &error));
  g_assert_no_error (error);
  g_object_unref (array_of_array_of_string);
  g_object_unref (array_of_array_of_objpath);
  g_object_unref (array_of_array_of_byte);
  g_object_unref (array_of_array_of_boolean);
  g_object_unref (array_of_array_of_int16);
  g_object_unref (array_of_array_of_uint16);
  g_object_unref (array_of_array_of_int32);
  g_object_unref (array_of_array_of_uint32);
  g_object_unref (array_of_array_of_int64);
  g_object_unref (array_of_array_of_uint64);
  g_object_unref (array_of_array_of_double);
  g_object_unref (array_of_array_of_point);
  g_object_unref (array_of_array_of_array_of_string);

  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_string) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_objpath) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_byte) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_boolean) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_int16) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_uint16) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_int32) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_uint32) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_int64) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_uint64) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_double) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_point) == 4);
  g_assert (egg_dbus_array_seq_get_size (ret_array_of_array_of_array_of_string) == 4);

  /* we don't check the data as we've already established that passing arrays back and forth works */

  g_object_unref (ret_array_of_array_of_string);
  g_object_unref (ret_array_of_array_of_objpath);
  g_object_unref (ret_array_of_array_of_byte);
  g_object_unref (ret_array_of_array_of_boolean);
  g_object_unref (ret_array_of_array_of_int16);
  g_object_unref (ret_array_of_array_of_uint16);
  g_object_unref (ret_array_of_array_of_int32);
  g_object_unref (ret_array_of_array_of_uint32);
  g_object_unref (ret_array_of_array_of_int64);
  g_object_unref (ret_array_of_array_of_uint64);
  g_object_unref (ret_array_of_array_of_double);
  g_object_unref (ret_array_of_array_of_point);
  g_object_unref (ret_array_of_array_of_array_of_string);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_variant_return (void)
{
  GError *error;
  EggDBusVariant *variant;
  EggDBusStructure *structure;
  EggDBusArraySeq *my_seq;
  EggDBusArraySeq *seq;
  char **strv;
  TestPoint *point;
  TestDescribedPoint *dpoint;
  EggDBusHashMap *hash_of_points;
  const char *s;
  gint my_int0;
  gint my_int1;

  error = NULL;

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "y",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_byte (variant));
  g_assert_cmpint (egg_dbus_variant_get_byte (variant), ==, 1);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "b",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_boolean (variant));
  g_assert_cmpint (egg_dbus_variant_get_boolean (variant), ==, TRUE);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "n",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_int16 (variant));
  g_assert_cmpint (egg_dbus_variant_get_int16 (variant), ==, 2);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "q",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_uint16 (variant));
  g_assert_cmpuint (egg_dbus_variant_get_uint16 (variant), ==, 3);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "i",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_int (variant));
  g_assert_cmpint (egg_dbus_variant_get_int (variant), ==, 4);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "u",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_uint (variant));
  g_assert_cmpuint (egg_dbus_variant_get_uint (variant), ==, 5);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "x",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_int64 (variant));
  g_assert_cmpint (egg_dbus_variant_get_int64 (variant), ==, 6);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "t",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_uint64 (variant));
  g_assert_cmpuint (egg_dbus_variant_get_uint64 (variant), ==, 7);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "d",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_double (variant));
  g_assert_cmpfloat (egg_dbus_variant_get_double (variant), ==, 7.5);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "s",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_string (variant));
  g_assert_cmpstr (egg_dbus_variant_get_string (variant), ==, "a string");
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "o",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_object_path (variant));
  g_assert_cmpstr (egg_dbus_variant_get_object_path (variant), ==, "/some/object/path");
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "ai",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_seq (variant));
  seq = egg_dbus_variant_get_seq (variant);
  g_assert_cmpuint (seq->size, ==, 2);
  g_assert_cmpint (seq->data.v_int[0], ==, 4);
  g_assert_cmpint (seq->data.v_int[1], ==, 5);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "as",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_string_array (variant));
  strv = egg_dbus_variant_get_string_array (variant);
  g_assert_cmpuint (g_strv_length (strv), ==, 2);
  g_assert_cmpstr (strv[0], ==, "a string");
  g_assert_cmpstr (strv[1], ==, "another string");
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "point",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_structure (variant));
  point = TEST_POINT (egg_dbus_variant_get_structure (variant));
  g_assert_cmpint (test_point_get_x (point), ==, 1);
  g_assert_cmpint (test_point_get_y (point), ==, 2);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "dpoint",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_structure (variant));
  dpoint = TEST_DESCRIBED_POINT (egg_dbus_variant_get_structure (variant));
  s = test_described_point_get_desc (dpoint);
  g_assert_cmpstr (s, ==, "the desc");
  point = test_described_point_get_point (dpoint);
  g_assert_cmpint (test_point_get_x (point), ==, 3);
  g_assert_cmpint (test_point_get_y (point), ==, 4);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "hash_of_points",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert (egg_dbus_variant_is_map (variant));
  hash_of_points = egg_dbus_variant_get_map (variant);
  g_assert (egg_dbus_hash_map_lookup (hash_of_points, "non-existent-key") == NULL);
  g_assert ((point = egg_dbus_hash_map_lookup (hash_of_points, "key0")) != NULL);
  g_assert_cmpint (test_point_get_x (point), ==, 1);
  g_assert_cmpint (test_point_get_y (point), ==, 2);
  g_assert ((point = egg_dbus_hash_map_lookup (hash_of_points, "key1")) != NULL);
  g_assert_cmpint (test_point_get_x (point), ==, 3);
  g_assert_cmpint (test_point_get_y (point), ==, 4);
  g_assert ((point = egg_dbus_hash_map_lookup (hash_of_points, "key2")) != NULL);
  g_assert_cmpint (test_point_get_x (point), ==, 5);
  g_assert_cmpint (test_point_get_y (point), ==, 6);
  g_assert ((point = egg_dbus_hash_map_lookup (hash_of_points, "key3")) != NULL);
  g_assert_cmpint (test_point_get_x (point), ==, 7);
  g_assert_cmpint (test_point_get_y (point), ==, 8);
  g_object_unref (variant);

  g_assert (test_frob_test_variant_return_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                                0, /* flags */
                                                "unregistered_struct",
                                                &variant,
                                                NULL,
                                                &error));
  g_assert_no_error (error);

  structure = egg_dbus_variant_get_structure (variant);

  g_assert_cmpstr (egg_dbus_structure_get_signature (structure), ==, "(iiax)");

  egg_dbus_structure_get_element (structure,
                                  0, &my_int0,
                                  1, &my_int1,
                                  2, &my_seq,
                                  -1);

  g_assert_cmpint (my_int0, ==, 42);
  g_assert_cmpint (my_int1, ==, 43);
  g_assert_cmpuint (my_seq->element_type, ==, G_TYPE_INT64);
  g_assert_cmpuint (my_seq->size, ==, 4);
  g_assert_cmpint (my_seq->data.v_int64[0], ==, 0);
  g_assert_cmpint (my_seq->data.v_int64[1], ==, 1);
  g_assert_cmpint (my_seq->data.v_int64[2], ==, 2);
  g_assert_cmpint (my_seq->data.v_int64[3], ==, 3);

  g_object_unref (variant);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_another_interface (void)
{
  GError *error;
  char *s;

  error = NULL;

  g_assert (test_tweak_i_can_haz_greetingz_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                 0, /* flags */
                                                 "Sup?",
                                                 &s,
                                                 NULL,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmpstr (s, ==, "Word. You haz greetz 'Sup?'. KTHXBYE!");
  g_free (s);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
rw_property_changes_cb (TestTweak *instance,
                        GAsyncResult *res,
                        gboolean *received_async_reply)
{
  GError *error;
  gchar *new_prop_value;

  error = NULL;

  g_assert (test_tweak_block_until_rw_property_changes_finish (instance,
                                                               &new_prop_value,
                                                               res,
                                                               &error));
  g_assert_no_error (error);
  g_assert_cmpstr (new_prop_value, ==, "A new property value");
  *received_async_reply = TRUE;
}

static void
foo_notify_cb (EggDBusObjectProxy *object_proxy,
               GParamSpec *pspec,
               gboolean *got_foo_notify_event)
{
  gchar *s;

  g_assert (pspec->owner_type == TEST_TYPE_TWEAK);

  g_object_get (object_proxy, "foo", &s, NULL);
  g_assert_cmpstr (s, ==, "bar is the new foo");
  g_free (s);

  *got_foo_notify_event = TRUE;
}

static void
escape_vehicle_changed_cb (TestTweak *tweak,
                           TestVehicle new_escape_vehicle,
                           gpointer user_data)
{
  TestVehicle *dest = user_data;

  *dest = new_escape_vehicle;
}

static void
test_properties (void)
{
  GError *error;
  TestPoint *test_point;
  TestPair *test_pair;
  EggDBusArraySeq *array;
  char *s;
  gboolean received_async_reply;
  gboolean got_foo_notify_event;
  EggDBusVariant *variant;
  EggDBusHashMap *property_bag;
  TestVehicle escape_vehicle_signal_value;

  error = NULL;

  g_assert_cmpint (test_frob_get_y (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 1);
  g_assert_cmpint (test_frob_get_b (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, TRUE);
  g_assert_cmpint (test_frob_get_n (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 2);
  g_assert_cmpint (test_frob_get_q (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 3);
  g_assert_cmpint (test_frob_get_i (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 4);
  g_assert_cmpint (test_frob_get_u (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 5);
  g_assert_cmpint (test_frob_get_x (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 6);
  g_assert_cmpint (test_frob_get_t (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 7);
  g_assert_cmpint (test_frob_get_d (TEST_QUERY_INTERFACE_FROB (object_proxy)), ==, 7.5);
  s = test_frob_get_s (TEST_QUERY_INTERFACE_FROB (object_proxy));
  g_assert_cmpstr (s, ==, "a string");
  g_free (s);
  s = test_frob_get_o (TEST_QUERY_INTERFACE_FROB (object_proxy));
  g_assert_cmpstr (s, ==, "/some/path");
  g_free (s);
  s = test_frob_get_g (TEST_QUERY_INTERFACE_FROB (object_proxy));
  g_assert_cmpstr (s, ==, "(sig)");
  g_free (s);

  s = test_tweak_get_foo (TEST_QUERY_INTERFACE_TWEAK (object_proxy));
  g_assert_cmpstr (s, ==, "a tweaked string");
  g_free (s);

  g_object_get (TEST_QUERY_INTERFACE_FROB (object_proxy), "foo", &s, NULL);
  g_assert_cmpstr (s, ==, "a frobbed string");
  g_free (s);

  g_object_get (TEST_QUERY_INTERFACE_TWEAK (object_proxy), "foo", &s, NULL);
  g_assert_cmpstr (s, ==, "a tweaked string");
  g_free (s);

  array = test_tweak_get_bar (TEST_QUERY_INTERFACE_TWEAK (object_proxy));
  g_assert_cmpuint (array->size, ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (array, 0), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (array, 1), ==, 2);
  g_object_unref (array);

  test_point = test_tweak_get_baz (TEST_QUERY_INTERFACE_TWEAK (object_proxy));
  g_assert (TEST_IS_POINT (test_point));
  g_assert_cmpint (test_point_get_x (test_point), ==, 3);
  g_assert_cmpint (test_point_get_y (test_point), ==, 4);
  g_object_unref (test_point);

  test_pair = test_tweak_get_baz_forced_to_use_pair (TEST_QUERY_INTERFACE_TWEAK (object_proxy));
  g_assert (TEST_IS_PAIR (test_pair));
  g_assert_cmpint (test_pair_get_first (test_pair), ==, 30);
  g_assert_cmpint (test_pair_get_second (test_pair), ==, 40);
  /* it's also, by coincidence, a point (see test_egg_dbus_structure() for why); test
   * we can access it as such
   */
  g_assert (TEST_IS_POINT (test_pair));
  g_assert_cmpint (test_point_get_x (TEST_POINT (test_pair)), ==, 30);
  g_assert_cmpint (test_point_get_y (TEST_POINT (test_pair)), ==, 40);
  g_object_unref (test_pair);

  /* check this also works via g_object_get() */
  g_object_get (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                "baz", &test_point,
                "baz-forced-to-use-pair", &test_pair,
                NULL);
  g_assert (TEST_IS_POINT (test_point));
  g_assert_cmpint (test_point_get_x (test_point), ==, 3);
  g_assert_cmpint (test_point_get_y (test_point), ==, 4);
  g_object_unref (test_point);
  g_assert (TEST_IS_PAIR (test_pair));
  g_assert_cmpint (test_pair_get_first (test_pair), ==, 30);
  g_assert_cmpint (test_pair_get_second (test_pair), ==, 40);
  g_object_unref (test_pair);

  /* check that PropertyName <-> property-name conversion works
   *
   * (this property is declared as SomeReadWriteProperty in the com.example.Tweak.xml file)
   */
  g_object_get (TEST_QUERY_INTERFACE_TWEAK (object_proxy), "some-read-write-property", &s, NULL);
  g_assert_cmpstr (s, ==, "Some initial property value");
  g_free (s);

  /* check that property writing works...
   *
   * we also want to check that the server receives notify::test-tweak-some-read-write-property
   * signal so start with invoking an async method that will return only when that happens.. this
   * should happen in the *middle* of setting the property... so block in the mainloop to verify
   * this.
   */
  received_async_reply = FALSE;
  test_tweak_block_until_rw_property_changes (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                              EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                              NULL,
                                              (GAsyncReadyCallback) rw_property_changes_cb,
                                              &received_async_reply);
  g_object_set (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                "some-read-write-property", "A new property value",
                NULL);
  g_assert (received_async_reply == TRUE);

  g_object_get (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                "some-read-write-property", &s,
                NULL);
  g_assert_cmpstr (s, ==, "A new property value");
  g_free (s);

  /* Check that enum and flag properties work; these are using special paramspecs */
  g_signal_connect (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                    "escape-vehicle-changed",
                    (GCallback) escape_vehicle_changed_cb,
                    &escape_vehicle_signal_value);
  escape_vehicle_signal_value = TEST_VEHICLE_SPORT_UTILITY_VEHICLE;
  test_tweak_set_escape_vehicle (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                 TEST_VEHICLE_TRUCK);
  g_assert_cmpint (escape_vehicle_signal_value, ==, TEST_VEHICLE_TRUCK);
  g_assert_cmpint (test_tweak_get_escape_vehicle (TEST_QUERY_INTERFACE_TWEAK (object_proxy)), ==, TEST_VEHICLE_TRUCK);
  test_tweak_set_escape_vehicle (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                 TEST_VEHICLE_PATRIOT);
  g_assert_cmpint (escape_vehicle_signal_value, ==, TEST_VEHICLE_PATRIOT);
  g_assert_cmpint (test_tweak_get_escape_vehicle (TEST_QUERY_INTERFACE_TWEAK (object_proxy)), ==, TEST_VEHICLE_PATRIOT);
  g_signal_handlers_disconnect_by_func (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                        escape_vehicle_changed_cb,
                                        &escape_vehicle_signal_value);

  test_tweak_set_default_create_flags (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                       TEST_CREATE_FLAGS_NONE);
  g_assert_cmpint (test_tweak_get_default_create_flags (TEST_QUERY_INTERFACE_TWEAK (object_proxy)), ==,
                   TEST_CREATE_FLAGS_NONE);
  test_tweak_set_default_create_flags (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                       TEST_CREATE_FLAGS_USE_FROBNICATOR|TEST_CREATE_FLAGS_LOG_ATTEMPT);
  g_assert_cmpint (test_tweak_get_default_create_flags (TEST_QUERY_INTERFACE_TWEAK (object_proxy)), ==,
                   TEST_CREATE_FLAGS_USE_FROBNICATOR|TEST_CREATE_FLAGS_LOG_ATTEMPT);

  /* Check that property Change() signals are generated and received.. again, recurse
   * in the mainloop so we get the signal before the method returns.
  **/
  got_foo_notify_event = FALSE;
  g_signal_connect (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                    "notify::foo",
                    (GCallback) foo_notify_cb,
                    &got_foo_notify_event);
  g_assert (test_tweak_change_readable_property_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                      EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                                      "bar is the new foo",
                                                      NULL,
                                                      &error));
  g_assert_no_error (error);
  g_assert (got_foo_notify_event == TRUE);
  g_signal_handlers_disconnect_by_func (object_proxy, foo_notify_cb, &got_foo_notify_event);

  /* Check that we return the right error on unknown properties on known interfaces */
  variant = NULL;
  g_assert(!egg_dbus_properties_get_sync (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (object_proxy),
                                          0, /* flags */
                                          "com.example.Tweak",
                                          "NonexistantProperty",
                                          &variant,
                                          NULL,
                                          &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking org.freedesktop.DBus.Properties.Get() on /com/example/TestObject at name com.example.TestService: org.gtk.EggDBus.Error.Failed: Given property does not exist on the given interface");
  g_assert (variant == NULL);
  g_error_free (error);
  error = NULL;

  /* Check that we return the right error on unknown properties on unknown interfaces */
  variant = NULL;
  g_assert(!egg_dbus_properties_get_sync (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (object_proxy),
                                          0, /* flags */
                                          "com.example.NonexistantInterface",
                                          "NonexistantProperty",
                                          &variant,
                                          NULL,
                                          &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking org.freedesktop.DBus.Properties.Get() on /com/example/TestObject at name com.example.TestService: org.gtk.EggDBus.Error.Failed: Object does not implement given interface");
  g_assert (variant == NULL);
  g_error_free (error);
  error = NULL;

  /* ditto for GetAll() */
  property_bag = NULL;
  g_assert(!egg_dbus_properties_get_all_sync (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (object_proxy),
                                              0, /* flags */
                                              "com.example.NonexistantInterface",
                                              &property_bag,
                                              NULL,
                                              &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking org.freedesktop.DBus.Properties.GetAll() on /com/example/TestObject at name com.example.TestService: org.gtk.EggDBus.Error.Failed: Object does not implement given interface");
  g_assert (property_bag == NULL);
  g_error_free (error);
  error = NULL;

  /* Check that we return the right error on setting an unknown property on a known interfaces */
  variant = egg_dbus_variant_new_for_byte (42);
  g_assert(!egg_dbus_properties_set_sync (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (object_proxy),
                                          0, /* flags */
                                          "com.example.Tweak",
                                          "NonexistantProperty",
                                          variant,
                                          NULL,
                                          &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking org.freedesktop.DBus.Properties.Set() on /com/example/TestObject at name com.example.TestService: org.gtk.EggDBus.Error.Failed: Given property does not exist on the given interface");
  g_error_free (error);
  error = NULL;
  g_object_unref (variant);

  /* Check that we return the right error on setting an unknown property on a unknown interfaces */
  variant = egg_dbus_variant_new_for_byte (42);
  g_assert(!egg_dbus_properties_set_sync (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (object_proxy),
                                          0, /* flags */
                                          "com.example.NonexistantInterface",
                                          "NonexistantProperty",
                                          variant,
                                          NULL,
                                          &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking org.freedesktop.DBus.Properties.Set() on /com/example/TestObject at name com.example.TestService: org.gtk.EggDBus.Error.Failed: Object does not implement given interface");
  g_error_free (error);
  error = NULL;
  g_object_unref (variant);

  /* TODO: add tests for when interface="" is passed (requrires fixing TODO's in eggdbusconnection.c) */
}

/* ---------------------------------------------------------------------------------------------------- */

static void
frob_signal_with_primitive_types_cb (TestFrob          *instance,
                                     guchar             val_byte,
                                     gboolean           val_boolean,
                                     gint16             val_int16,
                                     guint16            val_uint16,
                                     gint               val_int32,
                                     guint              val_uint32,
                                     gint64             val_int64,
                                     guint64            val_uint64,
                                     gdouble            val_double,
                                     const gchar       *val_string,
                                     const gchar       *val_objpath,
                                     const gchar       *val_sig,
                                     gpointer           user_data)
{
  gboolean *b = user_data;

  g_assert_cmphex (val_byte, ==, 0xfe);
  g_assert (val_boolean == TRUE);
  g_assert_cmpint  (val_int16,    ==, 2);
  g_assert_cmpuint (val_uint16,   ==, 3);
  g_assert_cmpint  (val_int32,    ==, 4);
  g_assert_cmpuint (val_uint32,   ==, 5);
  g_assert_cmpint  (val_int64,    ==, 6);
  g_assert_cmpuint (val_uint64,   ==, 7);
  g_assert_cmpint  (val_double,   ==, 1.2);
  g_assert_cmpstr  (val_string,   ==, "a string");
  g_assert_cmpstr  (val_objpath,  ==, "/objpath");
  g_assert_cmpstr  (val_sig,      ==, "(ss)");

  *b = TRUE;
}

static void
frob_signal_with_array_of_primitive_types_cb (TestFrob               *instance,
                                              EggDBusArraySeq        *array_byte,
                                              EggDBusArraySeq        *array_int32,
                                              gchar                 **array_string,
                                              gchar                 **array_objpath,
                                              gchar                 **array_sig,
                                              gpointer                user_data)
{
  gboolean *b = user_data;

  g_assert_cmpuint (array_byte->size, ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (array_byte, 0), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (array_byte, 1), ==, 11);

  g_assert_cmpuint (array_int32->size, ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (array_int32, 0), ==, 4);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (array_int32, 1), ==, 14);

  g_assert (g_strv_length (array_string) == 2);
  g_assert_cmpstr (array_string[0], ==, "signalfoo");
  g_assert_cmpstr (array_string[1], ==, "signalbar");

  g_assert (g_strv_length (array_objpath) == 3);
  g_assert_cmpstr (array_objpath[0], ==, "/signal/foo");
  g_assert_cmpstr (array_objpath[1], ==, "/signal/bar");
  g_assert_cmpstr (array_objpath[2], ==, "/signal/baz");

  g_assert (g_strv_length (array_sig) == 3);
  g_assert_cmpstr (array_sig[0], ==, "s");
  g_assert_cmpstr (array_sig[1], ==, "(ss)");
  g_assert_cmpstr (array_sig[2], ==, "(sig)");

  *b = TRUE;
}

static void
frob_signal_with_struct_and_hash_cb (TestFrob           *instance,
                                     TestPoint          *point,
                                     TestDescribedPoint *dpoint,
                                     EggDBusHashMap     *hash_of_string_to_string,
                                     EggDBusHashMap     *hash_of_string_to_point,
                                     gpointer            user_data)
{
  gboolean *b = user_data;
  TestPoint *point2;
  const gchar *desc;

  g_assert_cmpint (test_point_get_x (point), ==, 40);
  g_assert_cmpint (test_point_get_y (point), ==, 41);

  desc = test_described_point_get_desc (dpoint);
  g_assert_cmpstr (desc, ==, "xmas");
  point2 = test_described_point_get_point (dpoint);
  g_assert_cmpint (test_point_get_x (point2), ==, 42);
  g_assert_cmpint (test_point_get_y (point2), ==, 43);

  g_assert_cmpint (egg_dbus_hash_map_get_size (hash_of_string_to_string), ==, 3);
  g_assert_cmpstr (egg_dbus_hash_map_lookup (hash_of_string_to_string, "secret"), ==, "emerald");
  g_assert_cmpstr (egg_dbus_hash_map_lookup (hash_of_string_to_string, "top-secret"), ==, "stuff");
  g_assert_cmpstr (egg_dbus_hash_map_lookup (hash_of_string_to_string, "ultra-top-secret"), ==, "Rupert");

  g_assert_cmpint (egg_dbus_hash_map_get_size (hash_of_string_to_point), ==, 2);
  point2 = egg_dbus_hash_map_lookup (hash_of_string_to_point, "site59");
  g_assert_cmpint (test_point_get_x (point2), ==, 20);
  g_assert_cmpint (test_point_get_y (point2), ==, 21);
  point2 = egg_dbus_hash_map_lookup (hash_of_string_to_point, "site60");
  g_assert_cmpint (test_point_get_x (point2), ==, 22);
  g_assert_cmpint (test_point_get_y (point2), ==, 23);

  *b = TRUE;
}

static void
newz_notifz_signal_handler (TestTweak   *tweak,
                            const gchar *newz,
                            gpointer     user_data)
{
  char **signal_ret = user_data;
  g_assert (*signal_ret == NULL);
  *signal_ret = g_strdup (newz);
}

static void
newz_notifz_signal_handler_deadlock_test (TestTweak   *tweak,
                                          const gchar *newz,
                                          gpointer     user_data)
{
  char **signal_ret = user_data;
  GError *error;
  gchar *s;

  g_assert (*signal_ret == NULL);
  *signal_ret = g_strdup (newz);

  /* test sync method invocation in signal handler */
  error = NULL;
  g_assert (test_frob_hello_world_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                        0, /* flags */
                                        "Hi!",
                                        &s,
                                        NULL,
                                        &error));
  g_assert_no_error (error);
  g_assert_cmpstr (s, ==, "You greeted me with 'Hi!'. Thanks!");
  g_free (s);
}

static void
test_signals (void)
{
  GError *error;
  char *tweak_signal_ret;
  char *twiddle_signal_ret;
  gboolean primitive_types_signal_received;
  gboolean array_of_primitive_types_signal_received;
  gboolean struct_and_hash_signal_received;

  error = NULL;

  /* NOTE: For all these tests we want to block in the mainloop since we want the signals
   *       to be received and processed *before* the sync method invocation triggering them
   *       returns.
   */

  /* this also checks that signals with the same names on different interfaces are not conflated
   *
   * Historical note: this test was a lot more interesting back when
   *
   *   a) we didn't prefix stuff with "<name-space>-<interface-name>-"
   *      (see http://bugzilla.gnome.org/show_bug.cgi?id=561737 for details)
   *
   *   b) we slammed all the GInterfaces onto a single EggDBusProxy class
   *
   */

  tweak_signal_ret = NULL;
  twiddle_signal_ret = NULL;
  g_signal_connect (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                    "newz-notifz",
                    (GCallback) newz_notifz_signal_handler,
                    &tweak_signal_ret);
  g_signal_connect (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                    "newz-notifz",
                    (GCallback) newz_notifz_signal_handler,
                    &twiddle_signal_ret);

  g_assert (test_tweak_broadcastz_newz_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                             EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                             "Quick, there's a C compiler fire sale",
                                             NULL,
                                             &error));
  g_assert_no_error (error);
  g_assert (twiddle_signal_ret == NULL);
  g_assert_cmpstr (tweak_signal_ret, ==, "Word. Broadcastz 'Quick, there's a C compiler fire sale'. KTHXBYE!");
  g_free (tweak_signal_ret);
  tweak_signal_ret = NULL;

  g_assert (test_twiddle_broadcastz_newz_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                               EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                               "Diamonds in the mine",
                                               NULL,
                                               &error));
  g_assert_no_error (error);
  g_assert (tweak_signal_ret == NULL);
  g_assert_cmpstr (twiddle_signal_ret, ==, "Sez 'Diamonds in the mine'. KTHXBYE!");
  g_free (twiddle_signal_ret);
  twiddle_signal_ret = NULL;

  g_signal_handlers_disconnect_by_func (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                        newz_notifz_signal_handler,
                                        &tweak_signal_ret);
  g_signal_handlers_disconnect_by_func (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                        newz_notifz_signal_handler,
                                        &twiddle_signal_ret);

  /* check that there's no deadlock when invoking methods in the signal handler
   *
   * TODO: this may need fixing when using threads
   */
  twiddle_signal_ret = NULL;
  g_signal_connect (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                    "newz-notifz",
                    (GCallback) newz_notifz_signal_handler_deadlock_test,
                    &twiddle_signal_ret);
  g_assert (test_twiddle_broadcastz_newz_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                               EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                               "Deadlock test",
                                               NULL,
                                               &error));
  g_assert_no_error (error);
  g_assert_cmpstr (twiddle_signal_ret, ==, "Sez 'Deadlock test'. KTHXBYE!");
  g_free (twiddle_signal_ret);
  twiddle_signal_ret = NULL;
  g_signal_handlers_disconnect_by_func (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                        newz_notifz_signal_handler_deadlock_test,
                                        &twiddle_signal_ret);

  /* check signal emission / reception of various types */

  primitive_types_signal_received = FALSE;
  array_of_primitive_types_signal_received = FALSE;
  struct_and_hash_signal_received = FALSE;
  g_signal_connect (TEST_QUERY_INTERFACE_FROB (object_proxy),
                    "signal-with-primitive-types",
                    (GCallback) frob_signal_with_primitive_types_cb,
                    &primitive_types_signal_received);
  g_signal_connect (TEST_QUERY_INTERFACE_FROB (object_proxy),
                    "signal-with-array-of-primitive-types",
                    (GCallback) frob_signal_with_array_of_primitive_types_cb,
                    &array_of_primitive_types_signal_received);
  g_signal_connect (TEST_QUERY_INTERFACE_FROB (object_proxy),
                    "signal-with-structure-and-hash",
                    (GCallback) frob_signal_with_struct_and_hash_cb,
                    &struct_and_hash_signal_received);

  g_assert (test_frob_emit_test_signals_sync (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                              EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                              NULL,
                                              &error));
  g_assert_no_error (error);

  g_assert (primitive_types_signal_received == TRUE);
  g_assert (array_of_primitive_types_signal_received == TRUE);
  g_assert (struct_and_hash_signal_received == TRUE);

  g_signal_handlers_disconnect_by_func (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                        frob_signal_with_primitive_types_cb,
                                        &primitive_types_signal_received);
  g_signal_handlers_disconnect_by_func (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                        frob_signal_with_array_of_primitive_types_cb,
                                        &array_of_primitive_types_signal_received);
  g_signal_handlers_disconnect_by_func (TEST_QUERY_INTERFACE_FROB (object_proxy),
                                        frob_signal_with_struct_and_hash_cb,
                                        &struct_and_hash_signal_received);

}

/* ---------------------------------------------------------------------------------------------------- */

static gint num_owner_changed_signals_received;

static void
owner_changed_cb (EggDBusObjectProxy *object_proxy,
                  GParamSpec *pspec,
                  gpointer user_data)
{
  num_owner_changed_signals_received++;
  g_main_loop_quit (loop);
}

static void
test_name_owner_changed (void)
{
  GError *error;
  gchar *owner;
  gchar *unique_owner;
  gchar *owner_from_server;
  GPid server_pid;
  EggDBusObjectProxy *object_proxy_for_unique_name;

  error = NULL;

  /* we assume the service is not here already */
  owner = egg_dbus_object_proxy_get_name_owner (object_proxy);
  g_assert (owner == NULL);

  g_signal_connect (object_proxy, "notify::name-owner", (GCallback) owner_changed_cb, NULL);
  num_owner_changed_signals_received = 0;

  /* now, start the service */
  server_pid = spawn_with_baby_sitter ("./testserver");

  /* owner_changed_cb() will break out of the main loop when name-owner changes */
  g_main_loop_run (loop);

  /* check there's an owner now */
  owner = egg_dbus_object_proxy_get_name_owner (object_proxy);
  g_assert (owner != NULL);

  /* check that the server agrees */
  g_assert (test_tweak_get_server_unique_name_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                    0, /* flags */
                                                    &owner_from_server,
                                                    NULL,
                                                    &error));
  g_assert_no_error (error);
  g_assert_cmpstr (owner, ==, owner_from_server);
  g_free (owner);

  /* now kill the server and wait for another name-owner change */
  kill_baby_sitted_child (server_pid);
  g_main_loop_run (loop);

  owner = egg_dbus_object_proxy_get_name_owner (object_proxy);
  g_assert (owner == NULL);

  /* and start it one more time */
  server_pid = spawn_with_baby_sitter ("./testserver");

  /* owner_changed_cb() will break out of the main loop when name-owner changes */
  g_main_loop_run (loop);

  /* check there's an owner now */
  owner = egg_dbus_object_proxy_get_name_owner (object_proxy);
  g_assert (owner != NULL);

  /* disconnect C signal handler and check we've got the right number of signals */
  g_signal_handlers_disconnect_by_func (object_proxy, owner_changed_cb, NULL);
  g_assert (num_owner_changed_signals_received == 3);




  /* create a object_proxy for the unique name */
  object_proxy_for_unique_name = egg_dbus_connection_get_object_proxy (connection,
                                                       owner,
                                                       "/com/example/TestObject");

  /* check that the unique object_proxy has an owner and that it matches what we passed in */
  unique_owner = egg_dbus_object_proxy_get_name_owner (object_proxy_for_unique_name);
  g_assert_cmpstr (unique_owner, ==, owner);
  g_free (unique_owner);
  g_free (owner);

  /* reuse the signal handler from before, just for another object_proxy */
  g_signal_connect (object_proxy_for_unique_name, "notify::name-owner", (GCallback) owner_changed_cb, NULL);
  num_owner_changed_signals_received = 0;

  /* now, bring down the service again and wait for name-owner change */
  kill_baby_sitted_child (server_pid);
  g_main_loop_run (loop);

  /* now unique_owner should be NULL */
  unique_owner = egg_dbus_object_proxy_get_name_owner (object_proxy_for_unique_name);
  g_assert (unique_owner == NULL);

  /* disconnect C signal handler and check we've got the right number of signals */
  g_signal_handlers_disconnect_by_func (object_proxy_for_unique_name, owner_changed_cb, NULL);
  g_assert (num_owner_changed_signals_received == 1);

  /* .. and, btw, object_proxy_for_unique_name is now useless since unique names are not recycled */
  g_object_unref (object_proxy_for_unique_name);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_egg_dbus_structure (void)
{
  EggDBusStructure *as;
  GValue *values;
  const gchar *s1;
  const gchar *s2;
  gint i1;
  gint i2;
  gint16 n;
  double d;
  GValue str_gvalue = {0};
  GValue int32_gvalue = {0};
  TestPoint *point;
  TestPair *pair;
  TestDescribedPoint *dpoint;

  values = g_new0 (GValue, 5);

  g_value_init (&(values[0]), G_TYPE_STRING);
  g_value_set_string (&(values[0]), "a string");

  g_value_init (&(values[1]), G_TYPE_STRING);
  g_value_set_string (&(values[1]), "another string");

  g_value_init (&(values[2]), G_TYPE_INT);
  g_value_set_int (&(values[2]), 42);

  g_value_init (&(values[3]), EGG_DBUS_TYPE_INT16);
  egg_dbus_value_set_int16 (&(values[3]), 43);

  g_value_init (&(values[4]), G_TYPE_DOUBLE);
  g_value_set_double (&(values[4]), 0.5);

  /* this constructor takes ownership of the passed values */
  as = egg_dbus_structure_new ("(ssind)",
                               values);

  /* check we get the right values back */
  egg_dbus_structure_get_element (as,
                                  0, &s1,
                                  1, &s2,
                                  2, &i1,
                                  3, &n,
                                  4, &d,
                                  -1);
  g_assert_cmpstr (s1, ==, "a string");
  g_assert_cmpstr (s2, ==, "another string");
  g_assert_cmpint (i1, ==, 42);
  g_assert_cmpint (n, ==, 43);
  g_assert_cmpint (d, ==, 0.5);

  /* check we can set elements */
  egg_dbus_structure_set_element (as,
                                1, "yet another string",
                                4, 0.6,
                                -1);
  egg_dbus_structure_get_element (as,
                                1, &s2,
                                4, &d,
                                -1);
  g_assert_cmpstr (s2, ==, "yet another string");
  g_assert_cmpint (d, ==, 0.6);

  /* and as GValue */
  egg_dbus_structure_get_element_as_gvalue (as, 0, &str_gvalue);
  egg_dbus_structure_get_element_as_gvalue (as, 2, &int32_gvalue);
  g_assert (G_VALUE_HOLDS (&str_gvalue, G_TYPE_STRING));
  g_assert (G_VALUE_HOLDS (&int32_gvalue, G_TYPE_INT));
  g_assert_cmpstr (g_value_get_string (&str_gvalue), ==, "a string");
  g_assert_cmpint (g_value_get_int (&int32_gvalue), ==, 42);

  /* check we can set things as GValue (reuse the GValue) */
  g_value_set_string (&str_gvalue, "modified string");
  g_value_set_int (&int32_gvalue, 44);
  egg_dbus_structure_set_element_as_gvalue (as, 0, &str_gvalue);
  egg_dbus_structure_set_element_as_gvalue (as, 2, &int32_gvalue);
  g_value_unset (&str_gvalue);
  g_value_unset (&int32_gvalue);

  /* and check that it's properly set */
  egg_dbus_structure_get_element (as,
                                  0, &s1,
                                  2, &i1,
                                  -1);
  g_assert_cmpstr (s1, ==, "modified string");
  g_assert_cmpint (i1, ==, 44);

  g_object_unref (as);


  /* A TestPoint is just a way to access and construct an EggDBusStructure instance
   * with signature (ii); check it can be accessed as a EggDBusStructure.
   */
  point = test_point_new (3, 4);
  g_assert_cmpint (egg_dbus_structure_get_num_elements (EGG_DBUS_STRUCTURE (point)), ==, 2);
  egg_dbus_structure_get_element (EGG_DBUS_STRUCTURE (point),
                                0, &i1,
                                1, &i2,
                                -1);
  g_assert_cmpint (i1, ==, 3);
  g_assert_cmpint (i2, ==, 4);

  /* And TestPair is similar; it's also just a way to easily accessing the struct with
   * signature (ii). So TestPoint and TestPair are structurally equivalent and thus
   * are considered to be equivalent. See
   *
   *  http://en.wikipedia.org/wiki/Structural_type_system
   *
   * Note that this is unlike standard GObject type equivalence / inference; in fact
   * this is a great example of why it's sometimes useful that OO features are not built
   * into the language itself (either that, or a great example of how to subvert the
   * type system).
   *
   * Hence, TEST_IS_PAIR() should return TRUE. Check that.
   */
  g_assert (TEST_IS_PAIR (point));

  /* Also check that casting and access works.
   */
  pair = TEST_PAIR (point);
  g_assert_cmpint (test_pair_get_first (pair), ==, 3);
  g_assert_cmpint (test_pair_get_second (pair), ==, 4);

  /* TestDescribedPoint is a way to access structures with signature (s(ii)). Since
   * the signature is different from (ii), TestDescribedPoint is not structurally
   * equivalent to TestPair.
   *
   * So TEST_IS_DESCRIBED_POINT() should return FALSE.
   */
  g_assert (!TEST_IS_DESCRIBED_POINT (pair));

  /* Also casting from one to another should produce warnings. Verify that.
   */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      dpoint = TEST_DESCRIBED_POINT (pair);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*invalid cast from EggDBusStructure with signature (ii) to TestDescribedPoint with signature (s(ii))*");

  g_object_unref (point);

}

/* ---------------------------------------------------------------------------------------------------- */

static void
long_running_method_expect_cancelled_cb (EggDBusConnection  *connection,
                                         GAsyncResult       *res,
                                         gboolean           *b)
{
  GError *error;

  error = NULL;
  g_assert (!test_tweak_long_running_method_finish (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                    res,
                                                    &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_CANCELLED);
  g_error_free (error);

  *b = TRUE;
}

static gboolean
cancel_through_pending_call_id_timer_cb (gpointer user_data)
{
  guint pending_call_id = GPOINTER_TO_UINT (user_data);

  egg_dbus_connection_pending_call_cancel (connection, pending_call_id);

  g_main_loop_quit (loop);
  return FALSE;
}

static gboolean
cancel_through_g_cancellable_timer_cb (gpointer user_data)
{
  GCancellable *cancellable = G_CANCELLABLE (user_data);

  g_cancellable_cancel (cancellable);

  g_main_loop_quit (loop);
  return FALSE;
}

static void
test_cancellation (void)
{
  gboolean got_callback;
  guint pending_call_id;
  GCancellable *cancellable;

  /* check cancellation works through returned pending_call_id's */
  got_callback = FALSE;
  pending_call_id = test_tweak_long_running_method (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                    0, /* flags */
                                                    1500,
                                                    NULL,
                                                    (GAsyncReadyCallback) long_running_method_expect_cancelled_cb,
                                                    &got_callback);
  g_timeout_add (500,
                 cancel_through_pending_call_id_timer_cb,
                 GUINT_TO_POINTER (pending_call_id));
  g_main_loop_run (loop);
  g_assert (got_callback);

  /* check cancellation works through GCancellable */
  got_callback = FALSE;
  cancellable = g_cancellable_new ();
  test_tweak_long_running_method (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                  0, /* flags */
                                  1500,
                                  cancellable,
                                  (GAsyncReadyCallback) long_running_method_expect_cancelled_cb,
                                  &got_callback);
  g_timeout_add (500,
                 cancel_through_g_cancellable_timer_cb,
                 cancellable);
  g_main_loop_run (loop);
  g_assert (got_callback);
  g_assert (g_cancellable_is_cancelled (cancellable));
  g_object_unref (cancellable);

  /* TODO: check cancellation from another thread works on a sync call (through GCancellable) */

}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_timeouts (void)
{
  GError *error;

  /* this should time out since we ask the server to not reply for 30 seconds
   * and the default time out is 25 seconds.
   */
  error = NULL;
  g_assert (!test_tweak_long_running_method_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                  0, /* flags */
                                                  30 * 1000,
                                                  NULL,
                                                  &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_NO_REPLY);
  g_error_free (error);

  /* this should not time out since we pass EGG_DBUS_CALL_FLAGS_TIMEOUT_NONE
   * so the default timeout should be ignored.
   */
  error = NULL;
  g_assert (test_tweak_long_running_method_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                 EGG_DBUS_CALL_FLAGS_TIMEOUT_NONE,
                                                 30 * 1000,
                                                 NULL,
                                                 &error));
  g_assert_no_error (error);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_error_reporting (void)
{
  GError *error;

  /* Check that arbitrary GError's are transparently mapped back and forth
   * even if the GError is not registered with the EggDBus runtime.
   *
   * See _egg_dbus_error_encode_gerror() and _egg_dbus_error_decode_gerror()
   * in eggdbuserror.c for the wire format used; it boils down to that
   * unregistered error domains (e.g. not registered with TODO)
   * will be encoded as
   *
   *  org.gtk.EggDBus.UnmappedGError.Quark0x<domain-name-hex-encoded>.Code<error-code-in-decimal-encoding>
   *
   * Of course this feature isn't too useful for non-GLib applications but
   * GLib applications planning to interoperate with non-GLib applications
   * can use TODO to register an error.
   *
   * We also check (through error->message) that GError domains
   * registered with the EggDBus runtime using TODO are mapped to the
   * proper D-Bus error names as specified by the nick-name.
   */
  error = NULL;
  g_assert (!test_tweak_return_gerror_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                            0, /* flags */
                                            g_quark_to_string (EGG_DBUS_ERROR),
                                            EGG_DBUS_ERROR_TIMED_OUT,
                                            NULL,
                                            &error));
  g_assert_error (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_TIMED_OUT);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking com.example.Tweak.ReturnGError() on /com/example/TestObject at name com.example.TestService: org.freedesktop.DBus.Error.TimedOut: This is the error you requested (domain='EggDBusError', error_code=23).");
  g_error_free (error);

  error = NULL;
  g_assert (!test_tweak_return_gerror_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                            0, /* flags */
                                            g_quark_to_string (G_KEY_FILE_ERROR),
                                            G_KEY_FILE_ERROR_GROUP_NOT_FOUND, /* Free the group. Nuke the kernel! */
                                            NULL,
                                            &error));
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking com.example.Tweak.ReturnGError() on /com/example/TestObject at name com.example.TestService: org.gtk.EggDBus.UnmappedGError.Quark0x672d6b65792d66696c652d6572726f722d717561726b.Code4: This is the error you requested (domain='g-key-file-error-quark', error_code=4).");
  g_error_free (error);

  error = NULL;
  g_assert (!test_tweak_return_gerror_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                            0, /* flags */
                                            g_quark_to_string (G_IO_ERROR),
                                            G_IO_ERROR_WOULD_RECURSE,
                                            NULL,
                                            &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_RECURSE);
  g_assert_cmpstr (error->message, ==, "Remote Exception invoking com.example.Tweak.ReturnGError() on /com/example/TestObject at name com.example.TestService: org.gtk.EggDBus.UnmappedGError.Quark0x672d696f2d6572726f722d717561726b.Code25: This is the error you requested (domain='g-io-error-quark', error_code=25).");
  g_error_free (error);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
signal_with_ctype_cb (TestTweak *instance,
                      TestSomeExampleCType some_better_value,
                      gboolean *got_signal)
{
  g_assert_cmpint (some_better_value, ==, 42 + 1);
  *got_signal = TRUE;
}

static void
test_ctypes(void)
{
  GError *error;
  TestSomeExampleCType some_value;
  TestSomeExampleCType some_better_value;
  gboolean got_signal;
  TestStructWithVariant *struc;
  EggDBusVariant *variant;

  error = NULL;

  /* Here we test that stuff like
   *
   *   <arg direction="in" type="i" name="value">
   *     <annotation name="org.gtk.EggDBus.CType" value="TestSomeExampleCType"/>
   *   </arg>
   *
   * still works *even* when sizeof(int32) != sizeof (TestSomeExampleCType) as in this
   * example since testtypes.h says
   *
   *  typedef gint16 TestSomeExampleCType;
   *
   * This is important to test as people typically want to use pid_t and uid_t
   * as values for "org.gtk.EggDBus.CType" and these UNIX types *can* and *will*
   * have different sizes depending on the architecture and UNIX flavor / version.
   *
   * We also check signals; MethodWithCTypes will emit SignalWithCType() so recurse
   * in main loop.
   */
  got_signal = FALSE;
  g_signal_connect (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                    "signal-with-ctype",
                    (GCallback) signal_with_ctype_cb,
                    &got_signal);
  some_value = 42;
  g_assert (test_tweak_method_with_ctypes_sync (TEST_QUERY_INTERFACE_TWEAK (object_proxy),
                                                EGG_DBUS_CALL_FLAGS_BLOCK_IN_MAINLOOP,
                                                some_value,
                                                &some_better_value,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert_cmpint (some_better_value, ==, some_value + 1);
  g_assert (got_signal == TRUE);
  g_signal_handlers_disconnect_by_func (object_proxy, signal_with_ctype_cb, &got_signal);

  /* test properties works with CTypes */
  test_tweak_set_property_with_ctype (TEST_QUERY_INTERFACE_TWEAK (object_proxy), some_better_value);
  g_assert_cmpint (test_tweak_get_property_with_ctype (TEST_QUERY_INTERFACE_TWEAK (object_proxy)), ==, some_better_value);
  some_better_value++;
  test_tweak_set_property_with_ctype (TEST_QUERY_INTERFACE_TWEAK (object_proxy), some_better_value);
  g_assert_cmpint (test_tweak_get_property_with_ctype (TEST_QUERY_INTERFACE_TWEAK (object_proxy)), ==, some_better_value);

  /* and finally test structures */
  variant = egg_dbus_variant_new_for_byte (0x2a);
  struc = test_struct_with_variant_new ("foo", 42, 43, variant, some_better_value);
  g_assert_cmpint (test_struct_with_variant_get_override_c_type (struc), ==, some_better_value);
  some_better_value++;
  test_struct_with_variant_set_override_c_type (struc, some_better_value);
  g_assert_cmpint (test_struct_with_variant_get_override_c_type (struc), ==, some_better_value);
  g_object_unref (struc);
  g_object_unref (variant);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_user_supplied_structs (void)
{
  GError *error;
  TestSubject *subject;
  EggDBusArraySeq *list;

  error = NULL;

  /* Note, I'm actually not religious; it's just that this whole
   * god-thing is reasonably well-understood so it's a good example.
   */
  g_assert (test_twiddle_get_most_powerful_subject_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                         0, /* call_flags */
                                                         &subject,
                                                         NULL,
                                                         &error));
  g_assert_no_error (error);
  g_assert (TEST_IS_SUBJECT (subject));
  g_assert_cmpint (test_subject_get_kind (subject), ==, TEST_SUBJECT_KIND_DEITY);
  g_assert_cmpstr (test_subject_get_name (subject), ==, "God");
  g_assert_cmpstr (test_subject_get_favorite_food (subject), ==, "deity-snacks");
  g_assert_cmpstr (test_subject_get_favorite_color (subject), ==, "infrared");
  g_object_unref (subject); /* sacrilegious! sue me! */

  g_assert (test_twiddle_get_all_subjects_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                0, /* call_flags */
                                                &list,
                                                NULL,
                                                &error));
  g_assert_no_error (error);
  g_assert_cmpint (list->size, ==, 4);

  subject = TEST_SUBJECT (egg_dbus_array_seq_get (list, 0));
  g_assert (TEST_IS_SUBJECT (subject));
  g_assert_cmpint (test_subject_get_kind (subject), ==, TEST_SUBJECT_KIND_DEITY);
  g_assert_cmpstr (test_subject_get_name (subject), ==, "God");
  g_assert_cmpstr (test_subject_get_favorite_food (subject), ==, "deity-snacks");
  g_assert_cmpstr (test_subject_get_favorite_color (subject), ==, "infrared");

  subject = TEST_SUBJECT (egg_dbus_array_seq_get (list, 1));
  g_assert (TEST_IS_SUBJECT (subject));
  g_assert_cmpint (test_subject_get_kind (subject), ==, TEST_SUBJECT_KIND_HUMAN);
  g_assert_cmpstr (test_subject_get_name (subject), ==, "David"); /* that's me */
  g_assert_cmpstr (test_subject_get_favorite_food (subject), ==, "buffalo chicken pizza");
  g_assert_cmpstr (test_subject_get_favorite_color (subject), ==, "blue");

  subject = TEST_SUBJECT (egg_dbus_array_seq_get (list, 2));
  g_assert (TEST_IS_SUBJECT (subject));
  g_assert_cmpint (test_subject_get_kind (subject), ==, TEST_SUBJECT_KIND_CYLON);
  g_assert_cmpstr (test_subject_get_name (subject), ==, "Caprica-Six");
  g_assert_cmpstr (test_subject_get_favorite_food (subject), ==, "baltar-snacks"); /* should be davidz-snacks! */
  g_assert_cmpstr (test_subject_get_favorite_color (subject), ==, "red");

  subject = TEST_SUBJECT (egg_dbus_array_seq_get (list, 3));
  g_assert (TEST_IS_SUBJECT (subject));
  g_assert_cmpint (test_subject_get_kind (subject), ==, TEST_SUBJECT_KIND_HUMAN);
  g_assert_cmpstr (test_subject_get_name (subject), ==, "Divad"); /* my evil twin */
  g_assert_cmpstr (test_subject_get_favorite_food (subject), ==, "oysters");
  g_assert_cmpstr (test_subject_get_favorite_color (subject), ==, "black");

  g_object_unref (list); /* deep free FTW! */
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_enums (void)
{
  /* here we basically just check that the generated error domains, flags and value enumerations
   * has the right values according to the definitions in com.example.Twiddle.xml
   */

  g_assert_cmpint (TEST_ERROR_FAILED,                 ==, 0);
  g_assert_cmpint (TEST_ERROR_FLUX_CAPACITOR_FAILURE, ==, 1);
  g_assert_cmpint (TEST_ERROR_WOULD_DESTRUCT,         ==, 100);
  g_assert_cmpint (TEST_ERROR_WOULD_BLOCK,            ==, 101);

  g_assert_cmpint (TEST_DETAILED_ERROR_FAILED,               ==, 0);
  g_assert_cmpint (TEST_DETAILED_ERROR_PERMISSIONS_TOO_COOL, ==, 1);

  g_assert_cmpint (TEST_CREATE_FLAGS_NONE,             ==, 0x0000);
  g_assert_cmpint (TEST_CREATE_FLAGS_USE_FROBNICATOR,  ==, 0x0001);
  g_assert_cmpint (TEST_CREATE_FLAGS_LOG_ATTEMPT,      ==, 0x0002);
  g_assert_cmpint (TEST_CREATE_FLAGS_SOME_OTHER_FLAG,  ==, 0x0080);
  g_assert_cmpint (TEST_CREATE_FLAGS_YET_ANOTHER_FLAG, ==, 0x0100);

  g_assert_cmpint (TEST_DELETE_FLAGS_NO_FLAGS_SET,      ==, 0x0000);
  g_assert_cmpint (TEST_DELETE_FLAGS_USE_ORBITAL_LASER, ==, 0x0001);
  g_assert_cmpint (TEST_DELETE_FLAGS_MEDIA_COVERAGE,    ==, 0x0002);
  g_assert_cmpint (TEST_DELETE_FLAGS_AUDIBLE,           ==, 0x0004);
  g_assert_cmpint (TEST_DELETE_FLAGS_VISIBLE,           ==, 0x0008);

  g_assert_cmpint (TEST_OTHER_FLAGS_NONE,              ==, 0x0040);
  g_assert_cmpint (TEST_OTHER_FLAGS_USE_ORBITAL_LASER, ==, 0x0080);
  g_assert_cmpint (TEST_OTHER_FLAGS_MEDIA_COVERAGE,    ==, 0x0100);

  g_assert_cmpint (TEST_VEHICLE_SPORT_UTILITY_VEHICLE, ==, 100);
  g_assert_cmpint (TEST_VEHICLE_CONVERTIBLE,           ==, 110);
  g_assert_cmpint (TEST_VEHICLE_TRUCK,                 ==, 111);
  g_assert_cmpint (TEST_VEHICLE_PATRIOT,               ==, 112);
  g_assert_cmpint (TEST_VEHICLE_STATION_WAGON,         ==, 120);

}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_register_interface (void)
{
  EggDBusObjectProxy *slash_object_proxy;
  EggDBusObjectProxy *slash_foo_object_proxy;
  EggDBusObjectProxy *slash_foo_bar0_object_proxy;
  EggDBusInterfaceNodeInfo *node_info;
  GError *error;
  gchar *s;

  error = NULL;

  /* the server, in pristine state, only exports the /com/example/TestObject object. We're
   * going to verify that by introspection. Conveniently, this also (sortakinda)
   * verifies that the introspection framework works
   *
   * (Note that we've already established that the non-<node> bits work since we
   *  use it for generating code.)
   */

  /* (NOTE: we create all the proxies in advance; that's perfectly
   * fine; no IO will happen until they are used)
   */
  slash_object_proxy = egg_dbus_connection_get_object_proxy (connection,
                                                             "com.example.TestService",
                                                             "/");

  slash_foo_object_proxy = egg_dbus_connection_get_object_proxy (connection,
                                                                 "com.example.TestService",
                                                                 "/foo");

  slash_foo_bar0_object_proxy = egg_dbus_connection_get_object_proxy (connection,
                                                                      "com.example.TestService",
                                                                      "/foo/bar0");

  node_info = egg_dbus_object_proxy_introspect_sync (slash_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/");
  g_assert_cmpuint (node_info->num_interfaces, ==, 0);
  g_assert_cmpuint (node_info->num_nodes, ==, 1);
  g_assert_cmpstr (node_info->nodes[0].path, ==, "com");
  egg_dbus_interface_node_info_free (node_info);

  node_info = egg_dbus_object_proxy_introspect_sync (slash_foo_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/foo");
  g_assert_cmpuint (node_info->num_interfaces, ==, 0);
  g_assert_cmpuint (node_info->num_nodes, ==, 0);
  egg_dbus_interface_node_info_free (node_info);

  /* now ask the server to register Frob interface at /foo/bar0 */
  g_assert (test_twiddle_register_interface_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                  0, /* call_flags */
                                                  "/foo/bar0",
                                                  TRUE,
                                                  FALSE,
                                                  FALSE,
                                                  NULL,
                                                  &error));
  g_assert_no_error (error);

  /* we should now have a node "bar0" as a child at "/foo"; verify that */
  node_info = egg_dbus_object_proxy_introspect_sync (slash_foo_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/foo");
  g_assert_cmpuint (node_info->num_interfaces, ==, 0);
  g_assert_cmpuint (node_info->num_nodes, ==, 1);
  g_assert_cmpstr (node_info->nodes[0].path, ==, "bar0");
  egg_dbus_interface_node_info_free (node_info);

  /* check that /foo/bar0 implements only the Frob interface and also the standard
   * Properties, Peer and Introspectable interfaces
   */
  node_info = egg_dbus_object_proxy_introspect_sync (slash_foo_bar0_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/foo/bar0");
  g_assert_cmpuint (node_info->num_interfaces, ==, 4); /* Frob + Properties, Peer, Introspectable */
  g_assert_cmpuint (node_info->num_nodes, ==, 0);
  egg_dbus_interface_node_info_free (node_info);

  /* various misc checks that the /foo/bar0 object works */
  g_assert (test_frob_hello_world_sync (TEST_QUERY_INTERFACE_FROB (slash_foo_bar0_object_proxy),
                                        0, /* flags */
                                        "Export Dude",
                                        &s,
                                        NULL,
                                        &error));
  g_assert_no_error (error);
  g_assert_cmpstr (s, ==, "You greeted me with 'Export Dude'. Thanks!");
  g_free (s);
  g_object_get (TEST_QUERY_INTERFACE_FROB (slash_foo_bar0_object_proxy),
                "s", &s,
                NULL);
  g_assert_cmpstr (s, ==, "a string");
  g_free (s);

  /* now ask the server to register an *additional* interface, the com.example.Tweak interface, at the same path */
  g_assert (test_twiddle_register_interface_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                  0, /* call_flags */
                                                  "/foo/bar0",
                                                  FALSE,
                                                  TRUE,
                                                  FALSE,
                                                  NULL,
                                                  &error));
  g_assert_no_error (error);

  /* check there's now 2 + 3 = 5 interfaces registered */
  node_info = egg_dbus_object_proxy_introspect_sync (slash_foo_bar0_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/foo/bar0");
  g_assert_cmpuint (node_info->num_interfaces, ==, 5); /* Frob + Tweak + Properties, Peer, Introspectable */
  g_assert_cmpuint (node_info->num_nodes, ==, 0);
  egg_dbus_interface_node_info_free (node_info);

  /* misc checks on the object */
  g_assert (test_tweak_i_can_haz_greetingz_sync (TEST_QUERY_INTERFACE_TWEAK (slash_foo_bar0_object_proxy),
                                                 0, /* flags */
                                                 "Exporting, Sup?",
                                                 &s,
                                                 NULL,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmpstr (s, ==, "Word. You haz greetz 'Exporting, Sup?'. KTHXBYE!");
  g_free (s);
  g_object_get (TEST_QUERY_INTERFACE_TWEAK (slash_foo_bar0_object_proxy),
                "some-read-write-property", &s,
                NULL);
  g_assert_cmpstr (s, ==, "Some initial property value");
  g_free (s);

  /* change the property value */
  g_object_set (TEST_QUERY_INTERFACE_TWEAK (slash_foo_bar0_object_proxy),
                "some-read-write-property", "teh new value",
                NULL);
  g_object_get (TEST_QUERY_INTERFACE_TWEAK (slash_foo_bar0_object_proxy),
                "some-read-write-property", &s,
                NULL);
  g_assert_cmpstr (s, ==, "teh new value");
  g_free (s);

  /* Now, to check that the guarantee
   *
   *   "This function may be called multiple times for the same @object_path.
   *    If an existing #GType for an interface is already registered, it will
   *    be replaced."
   *
   * of egg_dbus_connection_register_interface() holds, use RegisterInterface()
   * to register a new Tweak interface at this object path.
   *
   * We can verify that it's a new one by looking at the value of the
   * 'some-read-write-property' property.
   *
   * (Obviously we need to manually call egg_dbus_object_proxy_invalidate_properties()
   *  since there's no signals (yet?) for when objects/interfaces gets added/removed.)
   */
  g_assert (test_twiddle_register_interface_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                  0, /* call_flags */
                                                  "/foo/bar0",
                                                  FALSE,
                                                  TRUE,
                                                  FALSE,
                                                  NULL,
                                                  &error));
  g_assert_no_error (error);
  egg_dbus_object_proxy_invalidate_properties (slash_foo_bar0_object_proxy);
  g_object_get (TEST_QUERY_INTERFACE_TWEAK (slash_foo_bar0_object_proxy),
                "some-read-write-property", &s,
                NULL);
  g_assert_cmpstr (s, ==, "Some initial property value");
  g_free (s);

  /* Now add yet another interface, the com.example.Twiddle interface, at the same path */
  g_assert (test_twiddle_register_interface_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                  0, /* call_flags */
                                                  "/foo/bar0",
                                                  FALSE,
                                                  FALSE,
                                                  TRUE,
                                                  NULL,
                                                  &error));
  g_assert_no_error (error);

  /* check there's now 3 + 3 = 6 interfaces registered */
  node_info = egg_dbus_object_proxy_introspect_sync (slash_foo_bar0_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/foo/bar0");
  g_assert_cmpuint (node_info->num_interfaces, ==, 6); /* Frob + Tweak + Twiddle + Properties, Peer, Introspectable */
  g_assert_cmpuint (node_info->num_nodes, ==, 0);
  egg_dbus_interface_node_info_free (node_info);

  /* now ask the server to unregister the Frob interface */
  g_assert (test_twiddle_unregister_interface_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                    0, /* call_flags */
                                                    "/foo/bar0",
                                                    TRUE,
                                                    FALSE,
                                                    FALSE,
                                                    NULL,
                                                    &error));
  g_assert_no_error (error);

  /* check only 2 + 3 = 5 interfaces are registered */
  node_info = egg_dbus_object_proxy_introspect_sync (slash_foo_bar0_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/foo/bar0");
  g_assert_cmpuint (node_info->num_interfaces, ==, 5); /* Tweak + Twiddle + Properties, Peer, Introspectable */
  g_assert_cmpuint (node_info->num_nodes, ==, 0);
  egg_dbus_interface_node_info_free (node_info);

  /* now use another D-Bus method to unregister the remaining interfaces; this will demontrate
   * that a) egg_dbus_connection_lookup_interface(); and b) unreffing registered interface stubs
   * will automatically cause them to be unregistered...
   */
  g_assert (test_twiddle_unregister_all_interfaces_sync (TEST_QUERY_INTERFACE_TWIDDLE (object_proxy),
                                                         0, /* call_flags */
                                                         "/foo/bar0",
                                                         NULL,
                                                         &error));
  g_assert_no_error (error);

  /* check that zero interfaces are now registered */
  node_info = egg_dbus_object_proxy_introspect_sync (slash_foo_bar0_object_proxy,
                                                     0, /* call_flags */
                                                     NULL,
                                                     &error);
  g_assert (node_info != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (node_info->path, ==, "/foo/bar0");
  g_assert_cmpuint (node_info->num_interfaces, ==, 0);
  g_assert_cmpuint (node_info->num_nodes, ==, 0);
  egg_dbus_interface_node_info_free (node_info);

  /* cleanup */
  g_object_unref (slash_object_proxy);
  g_object_unref (slash_foo_object_proxy);
  g_object_unref (slash_foo_bar0_object_proxy);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_egg_dbus_array_seq (void)
{
  EggDBusArraySeq *a;
  EggDBusArraySeq *b;
  const gchar *some_strings[6] = {"zero", "one", "two", "three", "four", NULL};
  gchar **string_array;
  gint some_ints[8] = {0, 1, 2, 3, 2, 4, 2, 5};
  gint int_2 = 2;
  gint int_3 = 3;
  GFile *file_2;
  GFile *file_3;
  guint64 some_uint64_numbers[3] = {G_MAXINT64 - 10, G_MAXINT * G_GINT64_CONSTANT (2), G_GINT64_CONSTANT (42)};
  guchar some_prime_numbers[7] = {1, 2, 3, 5, 7, 11, 13};
  guint n;

  /*
   * First, check that all EggDBusArraySeq operations work
   */
  a = egg_dbus_array_seq_new (G_TYPE_STRING, g_free, NULL, NULL);
  for (n = 0; n < 5; n++)
    egg_dbus_array_seq_add (a, g_strdup (some_strings[n]));
  g_assert_cmpuint (egg_dbus_array_seq_get_element_type (a), ==, G_TYPE_STRING);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 5);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "zero");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "one");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "two");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "three");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 4), ==, "four");
  /* address SHOULDN'T be the same since it's a copy */
  g_assert (egg_dbus_array_seq_get (a, 0) != some_strings[0]);
  /* try inserting elements, that should shift elements up */
  egg_dbus_array_seq_insert (a, 3, g_strdup ("two-and-a-half"));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 6);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "zero");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "one");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "two");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "two-and-a-half");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 4), ==, "three");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 5), ==, "four");
  egg_dbus_array_seq_remove_at (a, 3);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 5);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "zero");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "one");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "two");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "three");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 4), ==, "four");
  /* Try adding elements; this will automatically grow the array */
  egg_dbus_array_seq_add (a, g_strdup ("five"));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 6);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 5), ==, "five");
  /* this would be a buffer overflow; verify that we catch it and warn/abort */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      egg_dbus_array_seq_set (a, 7, g_strdup ("seven"));
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*index 7 is out of bounds on EggDBusArraySeq<gchararray> of size 6*");
  /* the proper way to do this is to a) ensure the array is big enough; and b) then set the item.
   *
   * Note that this creates a hole at index 6. Also check that.
   */
  egg_dbus_array_seq_set_size (a, 8);
  egg_dbus_array_seq_set (a, 7, g_strdup ("seven"));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 8);
  g_assert         (egg_dbus_array_seq_get (a, 6)  ==  NULL);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 7), ==, "seven");
  /* replace elements */
  egg_dbus_array_seq_set (a, 1, g_strdup ("better one")); /* get it? ;-) */
  /* removing elements */
  egg_dbus_array_seq_remove_at (a, 2);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 7);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "zero");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "better one");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "three");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "four");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 4), ==, "five");
  g_assert         (egg_dbus_array_seq_get (a, 5)  ==  NULL);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 6), ==, "seven");
  egg_dbus_array_seq_remove_range_at (a, 1, 3);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 4);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "zero");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "five");
  g_assert         (egg_dbus_array_seq_get (a, 2)  ==  NULL);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "seven");
  egg_dbus_array_seq_remove_at (a, 2);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 3);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "zero");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "five");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "seven");
  /* check that we're getting runtime error / abort for out-of-bounds access */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      const gchar *s;
      s = egg_dbus_array_seq_get (a, 3);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*index 3 is out of bounds on EggDBusArraySeq<gchararray> of size 3*");
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      const gchar *s;
      s = egg_dbus_array_seq_get (a, -2);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*index -2 is out of bounds on EggDBusArraySeq<gchararray> of size 3*");
  /* check we that downsizing the array works */
  egg_dbus_array_seq_set_size (a, 2);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 2);
  /* check that clear works */
  egg_dbus_array_seq_clear (a);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 0);
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* now, check the same without a free function */
  a = egg_dbus_array_seq_new (G_TYPE_STRING, NULL, NULL, NULL);
  for (n = 0; n < 5; n++)
    egg_dbus_array_seq_add (a, some_strings[n]);
  g_assert_cmpuint (egg_dbus_array_seq_get_element_type (a), ==, G_TYPE_STRING);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a),   ==, 5);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "zero");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "one");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "two");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "three");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 4), ==, "four");
  /* address SHOULD be the same since it's not a copy */
  g_assert (egg_dbus_array_seq_get (a, 0) == some_strings[0]);
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* check that fixed size types work (through C convenience interface) */
  a = egg_dbus_array_seq_new (G_TYPE_UCHAR, NULL, NULL, NULL);
  for (n = 0; n < 7; n++)
    egg_dbus_array_seq_add_fixed (a, some_prime_numbers[n]);
  g_assert_cmpuint (egg_dbus_array_seq_get_element_type (a), ==, G_TYPE_UCHAR);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 7);
  /* check that direct access works */
  g_assert_cmpint (a->data.v_byte[0], ==, 1);
  g_assert_cmpint (a->data.v_byte[1], ==, 2);
  g_assert_cmpint (a->data.v_byte[2], ==, 3);
  g_assert_cmpint (a->data.v_byte[3], ==, 5);
  g_assert_cmpint (a->data.v_byte[4], ==, 7);
  g_assert_cmpint (a->data.v_byte[5], ==, 11);
  g_assert_cmpint (a->data.v_byte[6], ==, 13);
  /* check that C convenience getters work */
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 2), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 3), ==, 5);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 4), ==, 7);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 5), ==, 11);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 6), ==, 13);
  /* check we can insert items in the middle */
  egg_dbus_array_seq_insert_fixed (a, 5, 9);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 8);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 2), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 3), ==, 5);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 4), ==, 7);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 5), ==, 9);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 6), ==, 11);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 7), ==, 13);
  egg_dbus_array_seq_remove_at (a, 5);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 7);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 2), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 3), ==, 5);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 4), ==, 7);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 5), ==, 11);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 6), ==, 13);
  /* check that we're getting runtime errors / abort for out-of-bounds access */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      guchar val;
      val = egg_dbus_array_seq_get_fixed (a, 10);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*index 10 is out of bounds on EggDBusArraySeq<guchar> of size 7*");
  /* check we can set elements */
  egg_dbus_array_seq_set_fixed (a, 4, 9); /* prime intruder! */
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 7);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 2), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 3), ==, 5);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 4), ==, 9);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 5), ==, 11);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 6), ==, 13);
  /* check that we can remove elements */
  egg_dbus_array_seq_remove_at (a, 2);
  egg_dbus_array_seq_remove_at (a, 4);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 5);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 2), ==, 5);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 3), ==, 9);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 4), ==, 13);
  /* check we can add stuff */
  egg_dbus_array_seq_add_fixed (a, 0xff);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 6);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 5), ==, 0xff);
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* check another fixed size type just for kicks */
  a = egg_dbus_array_seq_new (G_TYPE_INT64, NULL, NULL, NULL);
  for (n = 0; n < 3; n++)
    egg_dbus_array_seq_add_fixed (a, some_uint64_numbers[n]);
  g_assert_cmpuint (egg_dbus_array_seq_get_element_type (a), ==, G_TYPE_INT64);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 3);
  /* check that direct access works */
  g_assert_cmpint (a->data.v_int64[0], ==, G_MAXINT64 - 10);
  g_assert_cmpint (a->data.v_int64[1], ==, G_MAXINT * G_GINT64_CONSTANT (2));
  g_assert_cmpint (a->data.v_int64[2], ==, 42);
  /* check we can remove stuff */
  egg_dbus_array_seq_remove_at (a, 1);
  /* check that C convenience getters work */
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, G_MAXINT64 - 10);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 42);
  /* check we can add stuff */
  egg_dbus_array_seq_add_fixed (a, G_MAXINT * G_GINT64_CONSTANT (3));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, G_MAXINT64 - 10);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 42);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 2), ==, G_MAXINT * G_GINT64_CONSTANT (3));
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* now check that boxed types works */
  a = egg_dbus_array_seq_new (G_TYPE_STRV, (GDestroyNotify) g_strfreev, NULL, NULL);
  egg_dbus_array_seq_add (a, g_strsplit ("a test string", " ", 0));
  egg_dbus_array_seq_add (a, g_strsplit ("another string", " ", 0));
  egg_dbus_array_seq_add (a, g_strdupv ((gchar **) some_strings));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 3);
  g_assert_cmpuint (g_strv_length (egg_dbus_array_seq_get (a, 0)), ==, 3);
  g_assert_cmpstr (a->data.v_strv[0][0], ==, "a");
  g_assert_cmpstr (a->data.v_strv[0][1], ==, "test");
  g_assert_cmpstr (a->data.v_strv[0][2], ==, "string");
  g_assert_cmpuint (g_strv_length (egg_dbus_array_seq_get (a, 1)), ==, 2);
  g_assert_cmpstr (a->data.v_strv[1][0], ==, "another");
  g_assert_cmpstr (a->data.v_strv[1][1], ==, "string");
  g_assert_cmpuint (g_strv_length (egg_dbus_array_seq_get (a, 2)), ==, 5);
  g_assert_cmpstr (a->data.v_strv[2][0], ==, "zero");
  g_assert_cmpstr (a->data.v_strv[2][1], ==, "one");
  g_assert_cmpstr (a->data.v_strv[2][2], ==, "two");
  g_assert_cmpstr (a->data.v_strv[2][3], ==, "three");
  g_assert_cmpstr (a->data.v_strv[2][4], ==, "four");
  /* adding stuff of a wrong type should cause a run-time warning / abort; check that
   */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      egg_dbus_array_seq_add_fixed (a, 0xcafebabe);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Cannot use egg_dbus_array_seq_add_fixed() on EggDBusArraySeq<GStrv>*");
  /* check we can remove stuff; we only bother check the first element this time */
  egg_dbus_array_seq_remove_at (a, 1);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 2);
  g_assert_cmpuint (g_strv_length (egg_dbus_array_seq_get (a, 0)), ==, 3);
  g_assert_cmpstr (a->data.v_strv[0][0], ==, "a");
  g_assert_cmpuint (g_strv_length (egg_dbus_array_seq_get (a, 1)), ==, 5);
  g_assert_cmpstr (a->data.v_strv[1][0], ==, "zero");
  /* check that copying works */
  string_array = egg_dbus_array_seq_get_copy (a, 1);
  g_assert_cmpuint (g_strv_length (string_array), ==, 5);
  g_assert_cmpstr (string_array[0], ==, "zero");
  g_assert_cmpstr (string_array[1], ==, "one");
  g_assert_cmpstr (string_array[2], ==, "two");
  g_assert_cmpstr (string_array[3], ==, "three");
  g_assert_cmpstr (string_array[4], ==, "four");
  g_strfreev (string_array);
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* check that types deriving from boxed types work */
  a = egg_dbus_array_seq_new (EGG_DBUS_TYPE_OBJECT_PATH_ARRAY, (GDestroyNotify) g_strfreev, NULL, NULL);
  egg_dbus_array_seq_add (a, g_strsplit ("/a/test/path", "/", 0));
  egg_dbus_array_seq_add (a, g_strsplit ("/another/path", "/", 0));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 2);
  g_assert_cmpuint (g_strv_length (egg_dbus_array_seq_get (a, 0)), ==, 4);
  g_assert_cmpstr (a->data.v_strv[0][0], ==, "");
  g_assert_cmpstr (a->data.v_strv[0][1], ==, "a");
  g_assert_cmpstr (a->data.v_strv[0][2], ==, "test");
  g_assert_cmpstr (a->data.v_strv[0][3], ==, "path");
  g_assert_cmpuint (g_strv_length (egg_dbus_array_seq_get (a, 1)), ==, 3);
  g_assert_cmpstr (a->data.v_strv[1][0], ==, "");
  g_assert_cmpstr (a->data.v_strv[1][1], ==, "another");
  g_assert_cmpstr (a->data.v_strv[1][2], ==, "path");
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* Arrays containing GObject derived instances are very similar to arrays containg boxed
   * instances... so we don't need to test them very carefully since we've already tested
   * arrays of boxed instances. However, there's one additional check that is done; namely
   * type checking:
   */
  a = egg_dbus_array_seq_new (TEST_TYPE_SUBJECT, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (a, test_subject_new (TEST_SUBJECT_KIND_HUMAN, "davidz", "eggs", "blue"));
  egg_dbus_array_seq_add (a, test_subject_new (TEST_SUBJECT_KIND_HUMAN, "krh", "<undisclosed>", "<classified>"));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 2);
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      egg_dbus_array_seq_add_fixed (a, 0xcafebabe);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Cannot use egg_dbus_array_seq_add_fixed() on EggDBusArraySeq<TestSubject>*");
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      egg_dbus_array_seq_add (a, g_file_new_for_path ("/somewhere/file.txt"));
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Cannot insert an element of type GLocalFile into a EggDBusArraySeq<TestSubject>*");
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* Check that an array of a type deriving from GInterface works (as long as the
   * instances implenting the type are GObject derived). Since GFile is actually an interface
   * (implemented by GLocalFile and (if gvfs is installed) and GDaemonFile) use that for
   * our test case.
   */
  a = egg_dbus_array_seq_new (G_TYPE_FILE, g_object_unref, NULL, NULL);
  egg_dbus_array_seq_add (a, g_file_new_for_path ("a.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("b.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("c.txt"));
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 3);
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* check that equal_func is properly inferred and works */
  a = egg_dbus_array_seq_new (G_TYPE_STRING, g_free, NULL, NULL);
  g_assert (egg_dbus_array_seq_get_equal_func (a) != NULL);
  egg_dbus_array_seq_add (a, g_strdup ("zero"));
  egg_dbus_array_seq_add (a, g_strdup ("one"));
  egg_dbus_array_seq_add (a, g_strdup ("two"));
  egg_dbus_array_seq_add (a, g_strdup ("three"));
  egg_dbus_array_seq_add (a, g_strdup ("two"));
  egg_dbus_array_seq_add (a, g_strdup ("four"));
  egg_dbus_array_seq_add (a, g_strdup ("two"));
  egg_dbus_array_seq_add (a, g_strdup ("five"));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, "three"), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, "two"), ==, 2);
  g_assert (egg_dbus_array_seq_remove (a, "two"));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, "two"), ==, 3);
  g_assert (egg_dbus_array_seq_remove (a, "two"));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, "two"), ==, 4);
  g_assert (egg_dbus_array_seq_remove (a, "two"));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, "two"), ==, -1);
  g_assert (!egg_dbus_array_seq_remove (a, "two"));
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* try for a fixed-size type as well */
  a = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  for (n = 0; n < 8; n++)
    egg_dbus_array_seq_add_fixed (a, some_ints[n]);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 8);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 0), ==, 0);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 1), ==, 1);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 2), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 3), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 4), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 5), ==, 4);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 6), ==, 2);
  g_assert_cmpint (egg_dbus_array_seq_get_fixed (a, 7), ==, 5);
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, &int_3), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, &int_2), ==, 2);
  g_assert (egg_dbus_array_seq_remove (a, &int_2));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, &int_2), ==, 3);
  g_assert (egg_dbus_array_seq_remove (a, &int_2));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, &int_2), ==, 4);
  g_assert (egg_dbus_array_seq_remove (a, &int_2));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, &int_2), ==, -1);
  g_assert (!egg_dbus_array_seq_remove (a, &int_2));
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* check there is no equal_func by default for GObject types */
  a = egg_dbus_array_seq_new (G_TYPE_FILE, g_object_unref, NULL, NULL);
  g_assert (egg_dbus_array_seq_get_equal_func (a) == NULL);
  egg_dbus_array_seq_add (a, g_file_new_for_path ("0.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("1.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("2.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("3.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("2.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("4.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("2.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("5.txt"));
  /* we only check that index_of don't work (contains() and remove() are trivial when we
   * have index_of() and remove_at() already)
   */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      gint index;
      index = egg_dbus_array_seq_index_of (a, g_file_new_for_path ("/somewhere/file.txt"));
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*no equal_func set for EggDBusArraySeq<GFile>*");
  g_object_unref (a);
  /* now check again with an equal func */
  a = egg_dbus_array_seq_new (G_TYPE_FILE, g_object_unref, NULL, (GEqualFunc) g_file_equal);
  g_assert (egg_dbus_array_seq_get_equal_func (a) != NULL);
  egg_dbus_array_seq_add (a, g_file_new_for_path ("0.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("1.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("2.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("3.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("2.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("4.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("2.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("5.txt"));
  file_2 = g_file_new_for_path ("2.txt");
  file_3 = g_file_new_for_path ("3.txt");
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, file_3), ==, 3);
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, file_2), ==, 2);
  g_assert (egg_dbus_array_seq_remove (a, file_2));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, file_2), ==, 3);
  g_assert (egg_dbus_array_seq_remove (a, file_2));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, file_2), ==, 4);
  g_assert (egg_dbus_array_seq_remove (a, file_2));
  g_assert_cmpint (egg_dbus_array_seq_index_of (a, file_2), ==, -1);
  g_assert (!egg_dbus_array_seq_remove (a, file_2));
  g_object_unref (file_2);
  g_object_unref (file_3);
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* check we can handle raw pointer types */
  a = egg_dbus_array_seq_new (G_TYPE_POINTER, NULL, NULL, NULL);
  g_assert (egg_dbus_array_seq_get_equal_func (a) == NULL);
  egg_dbus_array_seq_add (a, g_file_new_for_path ("0.txt"));
  egg_dbus_array_seq_add (a, g_file_new_for_path ("1.txt"));
  g_object_unref (a->data.v_ptr[0]);
  g_object_unref (a->data.v_ptr[1]);
  /* check that attempting to copy items will cause a runtime error */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      gpointer ptr;
      ptr = egg_dbus_array_seq_get_copy (a, 0);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*no copy_func set for EggDBusArraySeq<gpointer>*");
  g_object_unref (a);

  /* -------------------------------------------------------------------------------- */

  /* check add_all() and steal_all() */
  a = egg_dbus_array_seq_new (G_TYPE_STRING, g_free, NULL, NULL);
  egg_dbus_array_seq_add (a, g_strdup ("1"));
  egg_dbus_array_seq_add (a, g_strdup ("2"));
  egg_dbus_array_seq_add (a, g_strdup ("3"));
  b = egg_dbus_array_seq_new (G_TYPE_STRING, g_free, NULL, NULL);
  egg_dbus_array_seq_add (b, g_strdup ("a"));
  egg_dbus_array_seq_add (b, g_strdup ("b"));
  egg_dbus_array_seq_add_all (a, b);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 5);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "1");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "2");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "3");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "a");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 4), ==, "b");
  g_assert_cmpuint (egg_dbus_array_seq_get_size (b), ==, 2);
  g_assert_cmpstr  (egg_dbus_array_seq_get (b, 0), ==, "a");
  g_assert_cmpstr  (egg_dbus_array_seq_get (b, 1), ==, "b");
  egg_dbus_array_seq_add_all (b, b);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (b), ==, 4);
  g_assert_cmpstr  (egg_dbus_array_seq_get (b, 0), ==, "a");
  g_assert_cmpstr  (egg_dbus_array_seq_get (b, 1), ==, "b");
  g_assert_cmpstr  (egg_dbus_array_seq_get (b, 2), ==, "a");
  g_assert_cmpstr  (egg_dbus_array_seq_get (b, 3), ==, "b");
  /* check we can't steal from ourselves */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      egg_dbus_array_seq_steal_all (b, b);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Can't steal elements from the same array*");
  /* check we can steal from others */
  egg_dbus_array_seq_steal_all (a, b);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 9);
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 0), ==, "1");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 1), ==, "2");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 2), ==, "3");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 3), ==, "a");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 4), ==, "b");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 5), ==, "a");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 6), ==, "b");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 7), ==, "a");
  g_assert_cmpstr  (egg_dbus_array_seq_get (a, 8), ==, "b");
  g_assert_cmpuint (egg_dbus_array_seq_get_size (b), ==, 0);
  g_object_unref (a);
  g_object_unref (b);

  /* also check for fixed-size type */
  a = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (a, 1);
  egg_dbus_array_seq_add_fixed (a, 2);
  egg_dbus_array_seq_add_fixed (a, 3);
  b = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
  egg_dbus_array_seq_add_fixed (b, 10);
  egg_dbus_array_seq_add_fixed (b, 11);
  egg_dbus_array_seq_add_all (a, b);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 5);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 0), ==, 1);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 1), ==, 2);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 2), ==, 3);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 3), ==, 10);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 4), ==, 11);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (b), ==, 2);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (b, 0), ==, 10);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (b, 1), ==, 11);
  egg_dbus_array_seq_add_all (b, b);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (b), ==, 4);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (b, 0), ==, 10);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (b, 1), ==, 11);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (b, 2), ==, 10);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (b, 3), ==, 11);
  /* check we can't steal from ourselves */
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      egg_dbus_array_seq_steal_all (b, b);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Can't steal elements from the same array*");
  /* check we can steal from others */
  egg_dbus_array_seq_steal_all (a, b);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (a), ==, 9);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 0), ==, 1);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 1), ==, 2);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 2), ==, 3);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 3), ==, 10);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 4), ==, 11);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 5), ==, 10);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 6), ==, 11);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 7), ==, 10);
  g_assert_cmpint  (egg_dbus_array_seq_get_fixed (a, 8), ==, 11);
  g_assert_cmpuint (egg_dbus_array_seq_get_size (b), ==, 0);
  g_object_unref (a);
  g_object_unref (b);

}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_egg_dbus_hash_map (void)
{
  EggDBusHashMap *m;
  GFile *f;
  GFile *af;
  gchar *path;

  m = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                             G_TYPE_FILE, g_object_unref);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 0);
  g_assert_cmpuint (egg_dbus_hash_map_get_key_type (m), ==, G_TYPE_STRING);
  g_assert_cmpuint (egg_dbus_hash_map_get_value_type (m), ==, G_TYPE_FILE);
  egg_dbus_hash_map_insert (m, "1", g_file_new_for_path ("/somewhere/1.txt"));
  egg_dbus_hash_map_insert (m, "2", g_file_new_for_path ("/somewhere/2.txt"));
  egg_dbus_hash_map_insert (m, "3", g_file_new_for_path ("/somewhere/3.txt"));
  egg_dbus_hash_map_insert (m, "4", g_file_new_for_path ("/somewhere/4.txt"));
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 4);
  g_assert (egg_dbus_hash_map_contains (m, "1"));
  g_assert (egg_dbus_hash_map_contains (m, "2"));
  g_assert (egg_dbus_hash_map_contains (m, "3"));
  g_assert (egg_dbus_hash_map_contains (m, "4"));
  g_assert (!egg_dbus_hash_map_contains (m, "5"));
  /* lookup value that we know exists */
  f = egg_dbus_hash_map_lookup (m, "2");
  g_assert (f != NULL);
  path = g_file_get_path (f);
  g_assert_cmpstr (path, ==, "/somewhere/2.txt");
  g_free (path);
  /* lookup value that we know doesn't exists */
  f = egg_dbus_hash_map_lookup (m, "5");
  g_assert (f == NULL);
  /* lookup value, get a copy */
  f = egg_dbus_hash_map_lookup_copy (m, "3");
  g_assert (f != NULL);
  path = g_file_get_path (f);
  g_assert_cmpstr (path, ==, "/somewhere/3.txt");
  g_free (path);
  g_object_unref (f);
  /* try removing an item that doesn't exist*/
  g_assert (!egg_dbus_hash_map_remove (m, "5"));
  /* try removing an item that exists */
  g_assert (egg_dbus_hash_map_remove (m, "3"));
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert (egg_dbus_hash_map_contains (m, "1"));
  g_assert (egg_dbus_hash_map_contains (m, "2"));
  g_assert (egg_dbus_hash_map_contains (m, "4"));
  /* try replacing a value */
  egg_dbus_hash_map_insert (m, "2", g_file_new_for_path ("/somewhere/new/2.txt"));
  f = egg_dbus_hash_map_lookup (m, "2");
  g_assert (f != NULL);
  path = g_file_get_path (f);
  g_assert_cmpstr (path, ==, "/somewhere/new/2.txt");
  g_free (path);
  /* try clearing the map */
  egg_dbus_hash_map_clear (m);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 0);
  egg_dbus_hash_map_insert (m, "1", g_file_new_for_path ("/somewhere/else/1.txt"));
  egg_dbus_hash_map_insert (m, "2", g_file_new_for_path ("/somewhere/else/2.txt"));
  g_object_unref (m);

  /* Try using a GObject as the key */
  m = egg_dbus_hash_map_new_full (G_TYPE_FILE, g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, g_object_ref,
                                  G_TYPE_FILE, g_object_unref, g_object_ref, (GEqualFunc) g_file_equal);
  egg_dbus_hash_map_insert (m, g_file_new_for_path ("/1.txt"), g_file_new_for_path ("/icon1.png"));
  egg_dbus_hash_map_insert (m, g_file_new_for_path ("/2.txt"), g_file_new_for_path ("/icon2.png"));
  egg_dbus_hash_map_insert (m, g_file_new_for_path ("/3.txt"), g_file_new_for_path ("/icon3.png"));
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  /* check we can look up stuff */
  af = g_file_new_for_path ("/1.txt"); f = egg_dbus_hash_map_lookup (m, af); g_assert (f != NULL);
  path = g_file_get_path (f); g_assert_cmpstr (path, ==, "/icon1.png"); g_free (path); g_object_unref (af);
  af = g_file_new_for_path ("/2.txt"); f = egg_dbus_hash_map_lookup (m, af); g_assert (f != NULL);
  path = g_file_get_path (f); g_assert_cmpstr (path, ==, "/icon2.png"); g_free (path); g_object_unref (af);
  af = g_file_new_for_path ("/3.txt"); f = egg_dbus_hash_map_lookup (m, af); g_assert (f != NULL);
  path = g_file_get_path (f); g_assert_cmpstr (path, ==, "/icon3.png"); g_free (path); g_object_unref (af);
  /* check we can replace stuff */
  egg_dbus_hash_map_insert (m, g_file_new_for_path ("/3.txt"), g_file_new_for_path ("/modified/icon3.png"));
  af = g_file_new_for_path ("/3.txt"); f = egg_dbus_hash_map_lookup (m, af); g_assert (f != NULL);
  path = g_file_get_path (f); g_assert_cmpstr (path, ==, "/modified/icon3.png"); g_free (path); g_object_unref (af);
  /* and we're done */
  g_object_unref (m);

  /* Now check convenience API. There are 9 combinations of nonfixed/fixed/float:
   *
   *   1. nonfixed -> nonfixed
   *   2. nonfixed ->    fixed
   *   3. nonfixed ->    float
   *   4.    fixed -> nonfixed
   *   5.    fixed -> fixed
   *   6.    fixed -> float
   *   7.    float -> nonfixed
   *   8.    float -> fixed
   *   9.    float -> float
   *
   * We already tested case 1. above. Now test the rest of them.
   *
   * For fixed we want to test both the for gint and gint64 since they are
   * stored differently.
   */

  /*   2. nonfixed ->    fixed */
  m = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                             G_TYPE_INT, NULL);
  egg_dbus_hash_map_insert_ptr_fixed (m, "1", 1);
  egg_dbus_hash_map_insert_ptr_fixed (m, "2", 2);
  egg_dbus_hash_map_insert_ptr_fixed (m, "3", 3);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "1"), ==, 1);
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "2"), ==, 2);
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "3"), ==, 3);
  g_assert (egg_dbus_hash_map_contains (m, "2"));
  g_assert (!egg_dbus_hash_map_contains (m, "4"));
  egg_dbus_hash_map_insert_ptr_fixed (m, "3", 3 + 10);
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "3"), ==, 3 + 10);
  g_object_unref (m);

  m = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                             G_TYPE_INT64, NULL);
  egg_dbus_hash_map_insert_ptr_fixed (m, "1", G_MAXINT + G_GINT64_CONSTANT (1001));
  egg_dbus_hash_map_insert_ptr_fixed (m, "2", G_MAXINT + G_GINT64_CONSTANT (1002));
  egg_dbus_hash_map_insert_ptr_fixed (m, "3", G_MAXINT + G_GINT64_CONSTANT (1003));
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "1"), ==, G_MAXINT + G_GINT64_CONSTANT (1001));
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "2"), ==, G_MAXINT + G_GINT64_CONSTANT (1002));
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "3"), ==, G_MAXINT + G_GINT64_CONSTANT (1003));
  g_assert (egg_dbus_hash_map_contains (m, "2"));
  g_assert (!egg_dbus_hash_map_contains (m, "4"));
  egg_dbus_hash_map_insert_ptr_fixed (m, "3", G_MAXINT + G_GINT64_CONSTANT (1003 + 10));
  g_assert_cmpint (egg_dbus_hash_map_lookup_ptr_fixed (m, "3"), ==, G_MAXINT + G_GINT64_CONSTANT (1003 + 10));
  g_object_unref (m);

  /*   3. nonfixed ->    float */
  m = egg_dbus_hash_map_new (G_TYPE_STRING, NULL,
                             G_TYPE_DOUBLE, NULL);
  egg_dbus_hash_map_insert_ptr_float (m, "1", 0.1);
  egg_dbus_hash_map_insert_ptr_float (m, "2", 0.2);
  egg_dbus_hash_map_insert_ptr_float (m, "3", 0.3);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_ptr_float (m, "1"), ==, 0.1);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_ptr_float (m, "2"), ==, 0.2);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_ptr_float (m, "3"), ==, 0.3);
  g_assert (egg_dbus_hash_map_contains (m, "2"));
  g_assert (!egg_dbus_hash_map_contains (m, "4"));
  egg_dbus_hash_map_insert_ptr_float (m, "3", 0.3 + 10);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_ptr_float (m, "3"), ==, 0.3 + 10);
  g_object_unref (m);

  /*   4.    fixed -> nonfixed */
  m = egg_dbus_hash_map_new (G_TYPE_UCHAR, NULL,
                             G_TYPE_STRING, NULL);
  egg_dbus_hash_map_insert_fixed_ptr (m, 1, "1");
  egg_dbus_hash_map_insert_fixed_ptr (m, 2, "2");
  egg_dbus_hash_map_insert_fixed_ptr (m, 3, "3");
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, 1), ==, "1");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, 2), ==, "2");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, 3), ==, "3");
  g_assert (egg_dbus_hash_map_contains_fixed (m, 2));
  g_assert (!egg_dbus_hash_map_contains_fixed (m, 4));
  egg_dbus_hash_map_insert_fixed_ptr (m, 3, "3a");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, 3), ==, "3a");
  g_object_unref (m);

  m = egg_dbus_hash_map_new (G_TYPE_UINT64, NULL,
                             G_TYPE_STRING, NULL);
  egg_dbus_hash_map_insert_fixed_ptr (m, G_MAXUINT64 - 1, "1");
  egg_dbus_hash_map_insert_fixed_ptr (m, G_MAXUINT64 - 2, "2");
  egg_dbus_hash_map_insert_fixed_ptr (m, G_MAXUINT64 - 3, "3");
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, G_MAXUINT64 - 1), ==, "1");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, G_MAXUINT64 - 2), ==, "2");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, G_MAXUINT64 - 3), ==, "3");
  g_assert (egg_dbus_hash_map_contains_fixed (m, G_MAXUINT64 - 2));
  g_assert (!egg_dbus_hash_map_contains_fixed (m, G_MAXUINT64 - 4));
  egg_dbus_hash_map_insert_fixed_ptr (m, G_MAXUINT64 - 3, "3a");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_fixed (m, G_MAXUINT64 - 3), ==, "3a");
  g_object_unref (m);

  /*   5.    fixed -> fixed */
  m = egg_dbus_hash_map_new (G_TYPE_INT, NULL,
                             G_TYPE_INT, NULL);
  egg_dbus_hash_map_insert_fixed_fixed (m, 1, 1);
  egg_dbus_hash_map_insert_fixed_fixed (m, 2, 2);
  egg_dbus_hash_map_insert_fixed_fixed (m, 3, 3);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (m, 1), ==, 1);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (m, 2), ==, 2);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (m, 3), ==, 3);
  g_assert (egg_dbus_hash_map_contains_fixed (m, 2));
  g_assert (!egg_dbus_hash_map_contains_fixed (m, 4));
  egg_dbus_hash_map_insert_fixed_fixed (m, 3, 13);
  g_assert_cmpint (egg_dbus_hash_map_lookup_fixed_fixed (m, 3), ==, 13);
  g_object_unref (m);

  /*   6.    fixed -> float */
  m = egg_dbus_hash_map_new (G_TYPE_INT, NULL,
                             G_TYPE_FLOAT, NULL);
  egg_dbus_hash_map_insert_fixed_float (m, 1, 1.1f);
  egg_dbus_hash_map_insert_fixed_float (m, 2, 2.1f);
  egg_dbus_hash_map_insert_fixed_float (m, 3, 3.1f);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_fixed_float (m, 1), ==, 1.1f);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_fixed_float (m, 2), ==, 2.1f);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_fixed_float (m, 3), ==, 3.1f);
  g_assert (egg_dbus_hash_map_contains_fixed (m, 2));
  g_assert (!egg_dbus_hash_map_contains_fixed (m, 4));
  egg_dbus_hash_map_insert_fixed_float (m, 3, 3.3f);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_fixed_float (m, 3), ==, 3.3f);
  g_object_unref (m);

  /*   7.    float -> nonfixed */
  m = egg_dbus_hash_map_new (G_TYPE_DOUBLE, NULL,
                             G_TYPE_STRING, NULL);
  egg_dbus_hash_map_insert_float_ptr (m, 1.1, "1");
  egg_dbus_hash_map_insert_float_ptr (m, 2.1, "2");
  egg_dbus_hash_map_insert_float_ptr (m, 3.1, "3");
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpstr (egg_dbus_hash_map_lookup_float (m, 1.1), ==, "1");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_float (m, 2.1), ==, "2");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_float (m, 3.1), ==, "3");
  g_assert (egg_dbus_hash_map_contains_float (m, 2.1));
  g_assert (!egg_dbus_hash_map_contains_float (m, 2.2));
  egg_dbus_hash_map_insert_float_ptr (m, 3.1, "3a");
  g_assert_cmpstr (egg_dbus_hash_map_lookup_float (m, 3.1), ==, "3a");
  g_object_unref (m);

  /*   8.    float -> fixed */
  m = egg_dbus_hash_map_new (G_TYPE_DOUBLE, NULL,
                             G_TYPE_INT, NULL);
  egg_dbus_hash_map_insert_float_fixed (m, 1.1, 1);
  egg_dbus_hash_map_insert_float_fixed (m, 2.1, 2);
  egg_dbus_hash_map_insert_float_fixed (m, 3.1, 3);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpint (egg_dbus_hash_map_lookup_float_fixed (m, 1.1), ==, 1);
  g_assert_cmpint (egg_dbus_hash_map_lookup_float_fixed (m, 2.1), ==, 2);
  g_assert_cmpint (egg_dbus_hash_map_lookup_float_fixed (m, 3.1), ==, 3);
  g_assert (egg_dbus_hash_map_contains_float (m, 2.1));
  g_assert (!egg_dbus_hash_map_contains_float (m, 2.2));
  egg_dbus_hash_map_insert_float_fixed (m, 3.1, 13);
  g_assert_cmpint (egg_dbus_hash_map_lookup_float_fixed (m, 3.1), ==, 13);
  g_object_unref (m);

  /*   9.    float -> float */
  m = egg_dbus_hash_map_new (G_TYPE_DOUBLE, NULL,
                             G_TYPE_DOUBLE, NULL);
  egg_dbus_hash_map_insert_float_float (m, 1.1, 1.2);
  egg_dbus_hash_map_insert_float_float (m, 2.1, 2.2);
  egg_dbus_hash_map_insert_float_float (m, 3.1, 3.2);
  g_assert_cmpuint (egg_dbus_hash_map_get_size (m), ==, 3);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_float_float (m, 1.1), ==, 1.2);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_float_float (m, 2.1), ==, 2.2);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_float_float (m, 3.1), ==, 3.2);
  g_assert (egg_dbus_hash_map_contains_float (m, 2.1));
  g_assert (!egg_dbus_hash_map_contains_float (m, 2.2));
  egg_dbus_hash_map_insert_float_float (m, 3.1, 3.3);
  g_assert_cmpfloat (egg_dbus_hash_map_lookup_float_float (m, 3.1), ==, 3.3);
  g_object_unref (m);


}

/* ---------------------------------------------------------------------------------------------------- */


static void
main_owner_changed_cb (EggDBusObjectProxy *object_proxy,
                       GParamSpec *pspec,
                       GMainLoop  *loop)
{
  g_main_loop_quit (loop);
}

int
main (int argc, char *argv[])
{
  GPid server_pid;

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  /* TODO: launch our own session bus */

  connection = egg_dbus_connection_get_for_bus (EGG_DBUS_BUS_TYPE_SESSION);

  object_proxy = egg_dbus_connection_get_object_proxy (connection,
                                                       "com.example.TestService",
                                                       "/com/example/TestObject");

  loop = g_main_loop_new (NULL, FALSE);

  /* all tests depend on these global variables:
   *
   * - connection: A EggDBusConnection to the session bus
   *
   * - object_proxy:      a EggDBusObjectProxy for the object /com/example/TestObject
   *                      provided by the name com.example.TestService
   *
   * - loop:       A GMainLoop
   */

  /* Test collection classes */
  test_egg_dbus_array_seq ();
  test_egg_dbus_hash_map ();
  test_egg_dbus_structure ();

  /* Tests for code generator
   *
   */
  test_enums ();

  /* The life-cycle test suite requires that none of our test services
   * are on the bus. So we start with that.
   */
  test_name_owner_changed ();


  /* The marshalling test suite requires the com.example.TestService to
   * be available on the bus.. so start the service and wait for it
   * to come up
   */
  //server_pid = spawn_with_baby_sitter ("./exampleservice.py");
  server_pid = spawn_with_baby_sitter ("./testserver");
  g_signal_connect (object_proxy, "notify::name-owner", (GCallback) main_owner_changed_cb, loop);
  g_main_loop_run (loop);
  g_signal_handlers_disconnect_by_func (object_proxy, main_owner_changed_cb, loop);

  test_low_level ();
  test_hello_world ();
  test_primitive_types ();
  test_array_of_primitive_types ();
  test_structure_types ();
  test_array_of_structure_types ();
  test_hash_tables ();
  test_hash_tables_of_arrays ();
  test_hash_table_of_structures ();
  test_hash_table_of_hash_table_of_structures ();
  test_array_of_hash_table_of_structures ();
  test_array_of_arrays ();
  test_variant_return ();
  test_another_interface ();
  test_ctypes ();
  test_user_supplied_structs ();

  /* the following tests requires the server is in a pristine state; it will change
   * properties / objects in various ways and leave the server tainted
   */
  test_properties ();
  test_signals ();
  test_error_reporting ();
  test_register_interface ();
  test_cancellation (); /* do cancellation as late since it takes up wall-clock time */
  test_timeouts ();     /* this test takes at least 50 secs to run so make sure it's the last one */

  /* done with the service */
  kill_baby_sitted_child (server_pid);

  g_object_unref (object_proxy);
  g_object_unref (connection);
  g_main_loop_unref (loop);

  g_print ("\n"
           "Test suite completed successfully.\n");

  return 0;
}
