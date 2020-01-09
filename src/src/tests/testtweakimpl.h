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

#ifndef __TEST_TWEAK_IMPL_H
#define __TEST_TWEAK_IMPL_H

#include <eggdbus/eggdbus.h>

G_BEGIN_DECLS

#define TEST_TYPE_TWEAK_IMPL         (test_tweak_impl_get_type())
#define TEST_TWEAK_IMPL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_TWEAK_IMPL, TestTweakImpl))
#define TEST_TWEAK_IMPL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TEST_TYPE_TWEAK_IMPL, TestTweakImplClass))
#define TEST_TWEAK_IMPL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TEST_TYPE_TWEAK_IMPL, TestTweakImplClass))
#define TEST_IS_TWEAK_IMPL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TEST_TYPE_TWEAK_IMPL))
#define TEST_IS_TWEAK_IMPL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TEST_TYPE_TWEAK_IMPL))

typedef struct _TestTweakImpl TestTweakImpl;
typedef struct _TestTweakImplClass TestTweakImplClass;

struct _TestTweakImpl
{
  GObject parent_instance;
};

struct _TestTweakImplClass
{
  GObjectClass parent_class;
};

GType          test_tweak_impl_get_type (void) G_GNUC_CONST;
TestTweakImpl *test_tweak_impl_new      (void);

G_END_DECLS

#endif /* __TEST_TWEAK_IMPL_H */
