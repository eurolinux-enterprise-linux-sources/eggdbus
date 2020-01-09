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

#ifndef __TEST_SUBJECT_H
#define __TEST_SUBJECT_H

#include <glib-object.h>
#include <gio/gio.h>
#include "testbindingstypes.h"

G_BEGIN_DECLS

#define TEST_TYPE_SUBJECT          (test_subject_get_type())
#define TEST_SUBJECT(o)            (EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_CAST (o, "(sa{sv})", TestSubject))
#define TEST_SUBJECT_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), TEST_TYPE_SUBJECT, TestSubjectClass))
#define TEST_SUBJECT_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), TEST_TYPE_SUBJECT, TestSubjectClass))
#define TEST_IS_SUBJECT(o)         (EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_TYPE (o, "(sa{sv})", TestSubject))
#define TEST_IS_SUBJECT_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), TEST_TYPE_SUBJECT))

#if 0
typedef struct _TestSubject TestSubject;
#endif
typedef struct _TestSubjectClass TestSubjectClass;

struct _TestSubject
{
  EggDBusStructure parent_instance;
};

struct _TestSubjectClass
{
  EggDBusStructureClass parent_class;
};

GType test_subject_get_type (void) G_GNUC_CONST;

typedef enum
{
  TEST_SUBJECT_KIND_UNKNOWN,
  TEST_SUBJECT_KIND_HUMAN,
  TEST_SUBJECT_KIND_CYLON,
  TEST_SUBJECT_KIND_DEITY,
} TestSubjectKind;

TestSubject    *test_subject_new                (TestSubjectKind  kind,
                                                 const gchar     *name,
                                                 const gchar     *favorite_food,
                                                 const gchar     *favorite_color);
TestSubjectKind test_subject_get_kind           (TestSubject     *subject);
const gchar *   test_subject_get_name           (TestSubject     *subject);
const gchar *   test_subject_get_favorite_food  (TestSubject     *subject);
const gchar *   test_subject_get_favorite_color (TestSubject     *subject);


G_END_DECLS

#endif /* __TEST_SUBJECT_H */
