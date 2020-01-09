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

#ifndef __TEST_TWIDDLE_IMPL_H
#define __TEST_TWIDDLE_IMPL_H

#include <eggdbus/eggdbus.h>

G_BEGIN_DECLS

#define TEST_TYPE_TWIDDLE_IMPL         (test_twiddle_impl_get_type())
#define TEST_TWIDDLE_IMPL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_TWIDDLE_IMPL, TestTwiddleImpl))
#define TEST_TWIDDLE_IMPL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TEST_TYPE_TWIDDLE_IMPL, TestTwiddleImplClass))
#define TEST_TWIDDLE_IMPL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TEST_TYPE_TWIDDLE_IMPL, TestTwiddleImplClass))
#define TEST_IS_TWIDDLE_IMPL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TEST_TYPE_TWIDDLE_IMPL))
#define TEST_IS_TWIDDLE_IMPL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TEST_TYPE_TWIDDLE_IMPL))

typedef struct _TestTwiddleImpl TestTwiddleImpl;
typedef struct _TestTwiddleImplClass TestTwiddleImplClass;

struct _TestTwiddleImpl
{
  GObject parent_instance;
};

struct _TestTwiddleImplClass
{
  GObjectClass parent_class;
};

GType            test_twiddle_impl_get_type (void) G_GNUC_CONST;
TestTwiddleImpl *test_twiddle_impl_new      (void);

G_END_DECLS

#endif /* __TEST_TWIDDLE_IMPL_H */
