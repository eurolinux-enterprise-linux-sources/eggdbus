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
#include <dbus/dbus.h>
#include <gobject/gvaluecollector.h>
#include <eggdbus/eggdbusstructure.h>
#include <eggdbus/eggdbusutils.h>
#include <eggdbus/eggdbusvariant.h>
#include <eggdbus/eggdbustypes.h>
#include <eggdbus/eggdbusarrayseq.h>
#include <eggdbus/eggdbushashmap.h>

/**
 * SECTION:eggdbusstructure
 * @title: EggDBusStructure
 * @short_description: D-Bus structures
 *
 * Instances of #EggDBusStructure represents D-Bus structures. Typically, this
 * class isn't used directly, subclasses are used instead.
 *
 * A structure in D-Bus is a container type with a fixed type signature containing
 * a fixed number of elements. Unlike most programming languages, elements in a
 * D-Bus structure does not have identifiers or names. Neither does the structure
 * itself have a name. For example, in C, consider these two data types:
 *
 * <programlisting>
 * typedef struct
 * {
 *   gint x;
 *   gint y;
 * } Point;
 *
 * typedef struct
 * {
 *   gint first;
 *   gint second;
 * } Pair;
 * </programlisting>
 *
 * In C, <literal>Point</literal> and <literal>Pair</literal> are distinct types
 * and cannot be used interchangeably without type punning. In the D-Bus protocol, however,
 * it is not possible to make a distinction between serialized instance of <literal>Point</literal>
 * and <literal>Pair</literal> – both have the signature <literal>(ii)</literal>.
 *
 * In EggDBus, it is possible to declare structures using D-Bus introspection XML. For example
 * consider the following D-Bus introspection XML:
 *
 * <programlisting><![CDATA[
 * <annotation name="org.gtk.EggDBus.DeclareStruct" value="Point">
 *   <annotation name="org.gtk.EggDBus.Struct.Member"  value="i:x">
 *     <annotation name="org.gtk.EggDBus.DocString" value="The X coordinate"/>
 *   </annotation>
 *   <annotation name="org.gtk.EggDBus.Struct.Member" value="i:y">
 *     <annotation name="org.gtk.EggDBus.DocString" value="The Y coordinate"/>
 *   </annotation>
 * </annotation>
 *
 * <annotation name="org.gtk.EggDBus.DeclareStruct" value="Pair">
 *   <annotation name="org.gtk.EggDBus.Struct.Member"  value="i:first">
 *     <annotation name="org.gtk.EggDBus.DocString" value="The first element"/>
 *   </annotation>
 *   <annotation name="org.gtk.EggDBus.Struct.Member" value="i:second">
 *     <annotation name="org.gtk.EggDBus.DocString" value="The second element"/>
 *   </annotation>
 * </annotation>
 *
 * <annotation name="org.gtk.EggDBus.DeclareStruct" value="DescribedPair">
 *   <annotation name="org.gtk.EggDBus.Struct.Member" value="s:desc">
 *     <annotation name="org.gtk.EggDBus.DocString" value="A description of the described pair"/>
 *   </annotation>
 *   <annotation name="org.gtk.EggDBus.Struct.Member" value="(ii):pair">
 *    <annotation name="org.gtk.EggDBus.DocString" value="The pair being described"/>
 *    <annotation name="org.gtk.EggDBus.StructType"  value="Pair"/>
 *   </annotation>
 * </annotation>
 * ]]></programlisting>
 *
 * These declarations makes <xref linkend="eggdbus-binding-tool.1"/> generate (using
 * <literal>--namespace Test</literal>) three #EggDBusStructure derived classes:
 * #TestPoint, #TestPair and #TestDescribedPair.
 *
 * Note that these generated classes are using the EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_CAST()
 * and EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_TYPE() macros for type casting and type checking.
 * This means that it's legal to use #TestPoint and #TestPair interchangeably but it's not
 * legal to cast e.g. an instance of type #TestDescribedPoint to e.g. #TestPoint<!-- -->:
 *
 * <programlisting>
 * TestPoint *point;
 * TestPair *pair;
 * TestDescribedPoint *described_point;
 *
 * point = test_point_new (3, 4);
 *
 * /<!-- -->* legal, TestPoint and TestPair are structurally equivalent types  *<!-- -->/
 * pair = TEST_PAIR (point);
 *
 * /<!-- -->* both of these assertions are true *<!-- -->/
 * g_assert (test_pair_get_first (pair) == 3);
 * g_assert (test_pair_get_second (pair) == 4);
 *
 * /<!-- -->* true assertion, TestDescribedPoint and TestPoint are not structurally equivalent types *<!-- -->/
 * g_assert (!TEST_IS_DESCRIBED_POINT (point));
 *
 * /<!-- -->* will issue a warning *<!-- -->/
 * described_point = TEST_DESCRIBED_POINT (point);
 *
 * /<!-- -->* cleanup *<!-- -->/
 * g_object_unref (point);
 * </programlisting>
 *
 * It is possible to supply structure wrapper types yourself if you want more
 * a more sophisticated API than just raw getters and setters. This is often desirable
 * when using future-proofed (to keep ABI compatibility) D-Bus structures that
 * uses e.g. hash tables to store the values.
 *
 * User defined structure wrapper types has to be declared in the D-Bus introspection XML:
 *
 * <programlisting><![CDATA[
 * <annotation name="org.gtk.EggDBus.DeclareStruct"       value="Subject">
 *   <annotation name="org.gtk.EggDBus.Struct.Signature"  value="(sa{sv})"/>
 * </annotation>
 * ]]></programlisting>
 *
 * such that they can be referenced by name from methods; e.g.
 *
 * <programlisting><![CDATA[
 * <method name="GetMostPowerfulSubject">
 *   <arg name="most_powerful_subject" direction="out" type="(sa{sv})">
 *     <annotation name="org.gtk.EggDBus.StructType" value="Subject"/>
 *   </arg>
 * </method>
 * ]]></programlisting>
 *
 * This annotation is needed to properly disambiguate that the D-Bus signature <literal>(sa{sv})</literal>
 * really refers to a <literal>Subject</literal> structure and not any other random D-Bus structure that
 * happens to have the same signature.
 *
 * Declaring and implementing the <literal>Subject</literal> structure wrapper type is
 * straightforward; first the header file:
 *
 * <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../src/tests/testsubject.h">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting>
 *
 * As with automatically generated #EggDBusStructure subclasses, the EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_CAST()
 * and EGG_DBUS_STRUCTURE_TYPE_CHECK_INSTANCE_TYPE() macros must be used so structural equivalence
 * identification and casting works.
 *
 * The implementation of the <literal>Subject</literal> structure wrapper type looks like this:
 *
 * <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../src/tests/testsubject.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting>
 *
 * It is important, for structural equivalence identification and casting to work, that implementations
 * only use #EggDBusStructure API to construct, store, set and get the element values. While this
 * constraint makes the implementation slightly more convoluted, it allows extending the #TestSubject type
 * (adding new kinds like <literal>Cyborg</literal> and properties like <literal>mood</literal>) without
 * breaking the D-Bus ABI.
 */

typedef struct
{
  gchar *signature;

  guint    num_elems;
  char   **elem_signatures;
  GValue  *elem_values;

} EggDBusStructurePrivate;

enum
{
  PROP_0,
  PROP_SIGNATURE,
  PROP_ELEMENTS,
  PROP_NUM_ELEMENTS,
  PROP_ELEMENT_SIGNATURES,
};

#define EGG_DBUS_STRUCTURE_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_STRUCTURE, EggDBusStructurePrivate))

G_DEFINE_TYPE (EggDBusStructure, egg_dbus_structure, G_TYPE_OBJECT);

static void
egg_dbus_structure_init (EggDBusStructure *structure)
{
}

static void
egg_dbus_structure_finalize (GObject *object)
{
  EggDBusStructurePrivate *priv;
  guint n;

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (object);

  for (n = 0; n < priv->num_elems; n++)
    {
      g_value_unset (&(priv->elem_values[n]));
      dbus_free (priv->elem_signatures[n]);
    }
  g_free (priv->elem_signatures);
  g_free (priv->elem_values);

  g_free (priv->signature);

  G_OBJECT_CLASS (egg_dbus_structure_parent_class)->finalize (object);
}

static void
egg_dbus_structure_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  EggDBusStructure *structure;
  EggDBusStructurePrivate *priv;

  structure = EGG_DBUS_STRUCTURE (object);
  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  switch (prop_id)
    {
    case PROP_SIGNATURE:
      g_value_set_string (value, priv->signature);
      break;

    case PROP_NUM_ELEMENTS:
      g_value_set_int (value, priv->num_elems);
      break;

    case PROP_ELEMENT_SIGNATURES:
      g_value_set_boxed (value, priv->elem_signatures);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_structure_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  EggDBusStructure *structure;
  EggDBusStructurePrivate *priv;

  structure = EGG_DBUS_STRUCTURE (object);
  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  switch (prop_id)
    {
    case PROP_ELEMENTS:
      /* we steal the pointer; that's part of the contract */
      priv->elem_values = g_value_get_pointer (value);
      break;

    case PROP_SIGNATURE:
      priv->signature = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_dbus_structure_constructed (GObject *object)
{
  EggDBusStructure *structure;
  EggDBusStructurePrivate *priv;
  DBusSignatureIter sig_iter;
  DBusSignatureIter sig_struct_iter;
  GPtrArray *elem_sigs;
  guint n;

  structure = EGG_DBUS_STRUCTURE (object);
  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  elem_sigs = g_ptr_array_new ();

  /* decompose signature */
  dbus_signature_iter_init (&sig_iter, priv->signature);
  g_assert (dbus_signature_iter_get_current_type (&sig_iter) == DBUS_TYPE_STRUCT);

  dbus_signature_iter_recurse (&sig_iter, &sig_struct_iter);
  n = 0;
  do
    {
      g_ptr_array_add (elem_sigs, dbus_signature_iter_get_signature (&sig_struct_iter));
      n++;
    }
  while (dbus_signature_iter_next (&sig_struct_iter));

  priv->num_elems = n;

  g_ptr_array_add (elem_sigs, NULL);
  priv->elem_signatures = (char **) g_ptr_array_free (elem_sigs, FALSE);

  if (G_OBJECT_CLASS (egg_dbus_structure_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (egg_dbus_structure_parent_class)->constructed (object);
}

static void
egg_dbus_structure_class_init (EggDBusStructureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = egg_dbus_structure_set_property;
  gobject_class->get_property = egg_dbus_structure_get_property;
  gobject_class->finalize     = egg_dbus_structure_finalize;
  gobject_class->constructed  = egg_dbus_structure_constructed;

  g_object_class_install_property (gobject_class,
                                   PROP_SIGNATURE,
                                   g_param_spec_string ("signature",
                                                        "Signature",
                                                        "The signature of the structure",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ELEMENTS,
                                   g_param_spec_pointer ("elements",
                                                         "Elements",
                                                         "The elements of the structure as an array of GValue. Takes ownership.",
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_NUM_ELEMENTS,
                                   g_param_spec_int ("num-elemements",
                                                     "Number of elements",
                                                     "The number of elements in the structure",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ELEMENT_SIGNATURES,
                                   g_param_spec_boxed ("elemement-signatures",
                                                       "Element Signatures",
                                                       "The signatures of the elements of the structure",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK |
                                                       G_PARAM_STATIC_BLURB));

  g_type_class_add_private (klass, sizeof (EggDBusStructurePrivate));
}

EggDBusStructure *
egg_dbus_structure_new (const gchar *signature,
                        GValue      *elements)
{
  return EGG_DBUS_STRUCTURE (g_object_new (EGG_DBUS_TYPE_STRUCTURE,
                                           "signature", signature,
                                           "elements", elements,
                                           NULL));
}


guint
egg_dbus_structure_get_num_elements (EggDBusStructure *structure)
{
  EggDBusStructurePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_STRUCTURE (structure), 0);

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  return priv->num_elems;
}

/**
 * egg_dbus_structure_get_element:
 * @structure: A #EggDBusStructure.
 * @first_structure_element_number: Element number to get.
 * @...: Return location for the first element, followed optionally by
 * more element number / return location pairs, followed by -1.
 *
 * Gets element values in a #EggDBusStructure. The returned values
 * should not be freed; @structure owns the reference.
 **/
void
egg_dbus_structure_get_element (EggDBusStructure *structure,
                                guint           first_structure_element_number,
                                ...)
{
  va_list var_args;

  g_return_if_fail (EGG_DBUS_IS_STRUCTURE (structure));

  va_start (var_args, first_structure_element_number);
  egg_dbus_structure_get_element_valist (structure, first_structure_element_number, var_args);
  va_end (var_args);
}

/**
 * egg_dbus_structure_get_element_valist:
 * @structure: A #EggDBusStructure.
 * @first_structure_element_number: Element number.
 * @var_args: Return location for the first element, followed optionally by
 * more element number / return location pairs, followed by -1.
 *
 * Like egg_dbus_structure_get_element() but intended for use by language
 * bindings.
 **/
void
egg_dbus_structure_get_element_valist (EggDBusStructure *structure,
                                       guint           first_structure_elem_number,
                                       va_list         var_args)
{
  EggDBusStructurePrivate *priv;
  guint elem_number;

  g_return_if_fail (EGG_DBUS_IS_STRUCTURE (structure));

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  elem_number = first_structure_elem_number;
  while (elem_number != (guint) -1)
    {
      gchar *error;

      if (elem_number >= priv->num_elems)
        {
          g_warning ("%s: elem number %u is out of bounds", G_STRFUNC, elem_number);
          break;
        }

      G_VALUE_LCOPY (&(priv->elem_values[elem_number]), var_args, G_VALUE_NOCOPY_CONTENTS, &error);
      if (error != NULL)
        {
          g_warning ("%s: %s", G_STRFUNC, error);
          g_free (error);
          break;
        }

      elem_number = va_arg (var_args, guint);
    }
}

/**
 * egg_dbus_structure_get_element_as_gvalue:
 * @structure: A #EggDBusStructure.
 * @element_number: Element number.
 * @value: Return location for #GValue.
 *
 * Sets @value to the contents of the value of @element_number. This
 * will also initalize @value so it needs to be uninitialized
 * (e.g. set to zeroes). Unlike egg_dbus_structure_get_element(), note
 * that the value is copied; use g_value_unset() to free it.
 **/
void
egg_dbus_structure_get_element_as_gvalue (EggDBusStructure *structure,
                                          guint           elem_number,
                                          GValue         *value)
{
  EggDBusStructurePrivate *priv;

  g_return_if_fail (EGG_DBUS_IS_STRUCTURE (structure));
  g_return_if_fail (value != NULL);

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  if (elem_number >= priv->num_elems)
    {
      g_warning ("%s: elem number %u is out of bounds", G_STRFUNC, elem_number);
      return;
    }

  g_value_init (value, G_VALUE_TYPE (&(priv->elem_values[elem_number])));
  g_value_copy (&(priv->elem_values[elem_number]), value);
}

/**
 * egg_dbus_structure_set_element:
 * @structure: A #EggDBusStructure.
 * @first_structure_element_number: Element number to set.
 * @...: First element to set, followed optionally by
 * more element number / return location pairs, followed by -1.
 *
 * Sets element values in a #EggDBusStructure. Similar to g_object_set().
 **/
void
egg_dbus_structure_set_element (EggDBusStructure *structure,
                              guint           first_structure_element_number,
                              ...)
{
  va_list var_args;

  g_return_if_fail (EGG_DBUS_IS_STRUCTURE (structure));

  va_start (var_args, first_structure_element_number);
  egg_dbus_structure_set_element_valist (structure, first_structure_element_number, var_args);
  va_end (var_args);
}

/**
 * egg_dbus_structure_set_element_valist:
 * @structure: A #EggDBusStructure.
 * @first_structure_element_number: Element number to set.
 * @var_args: First element to set, followed optionally by
 * more element number / return location pairs, followed by -1.
 *
 * Like egg_dbus_structure_set_element() but intended for use by language
 * bindings.
 **/
void
egg_dbus_structure_set_element_valist (EggDBusStructure *structure,
                                     guint           first_structure_elem_number,
                                     va_list         var_args)
{
  EggDBusStructurePrivate *priv;
  guint elem_number;

  g_return_if_fail (EGG_DBUS_IS_STRUCTURE (structure));

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  elem_number = first_structure_elem_number;
  while (elem_number != (guint) -1)
    {
      gchar *error;

      if (elem_number >= priv->num_elems)
        {
          g_warning ("%s: elem number %u is out of bounds", G_STRFUNC, elem_number);
          break;
        }

      G_VALUE_COLLECT (&(priv->elem_values[elem_number]), var_args, 0, &error);
      if (error != NULL)
        {
          g_warning ("%s: %s", G_STRFUNC, error);
          g_free (error);
          break;
        }

      elem_number = va_arg (var_args, guint);
    }
}

/**
 * egg_dbus_structure_set_element_valist:
 * @structure: A #EggDBusStructure.
 * @first_structure_element_number: Element number.
 * @var_args: First element to set, followed optionally by
 * more element number / element pairs, followed by -1.
 *
 * Like egg_dbus_structure_set_element() but intended for use by language
 * bindings.
 **/
void
egg_dbus_structure_set_element_as_gvalue (EggDBusStructure *structure,
                                        guint           elem_number,
                                        const GValue   *value)
{
  EggDBusStructurePrivate *priv;

  g_return_if_fail (EGG_DBUS_IS_STRUCTURE (structure));
  g_return_if_fail (value != NULL);

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  if (elem_number >= priv->num_elems)
    {
      g_warning ("%s: elem number %u is out of bounds", G_STRFUNC, elem_number);
      return;
    }

  /* TODO: check value has the correct type type */
  if (G_VALUE_TYPE (value) != G_VALUE_TYPE (&(priv->elem_values[elem_number])))
    {
      g_warning ("%s: Attempting to set elem number %d of type %s with a value of type %s",
                 G_STRFUNC,
                 elem_number,
                 g_type_name (G_VALUE_TYPE (&(priv->elem_values[elem_number]))),
                 g_type_name (G_VALUE_TYPE (value)));
      return;
    }

  g_value_unset (&(priv->elem_values[elem_number]));
  g_value_init (&(priv->elem_values[elem_number]), G_VALUE_TYPE (value));
  g_value_copy (value, &(priv->elem_values[elem_number]));
}

/**
 * egg_dbus_structure_set_element_as_gvalue:
 * @structure: A #EggDBusStructure.
 * @element_number: Element number.
 * @value: Return location for #GValue.
 *
 * Sets @value to the contents of the value of @element_number. This
 * will also initalize @value so it needs to be uninitialized
 * (e.g. set to zeroes).
 **/
const gchar *
egg_dbus_structure_get_signature (EggDBusStructure *structure)
{
  EggDBusStructurePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_STRUCTURE (structure), 0);

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  return priv->signature;
}

const gchar *
egg_dbus_structure_get_signature_for_element (EggDBusStructure *structure,
                                            guint           elem_number)
{
  EggDBusStructurePrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_STRUCTURE (structure), 0);

  priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

  if (elem_number >= priv->num_elems)
    {
      g_warning ("%s: elem number %u is out of bounds", G_STRFUNC, elem_number);
      return NULL;
    }

  return priv->elem_signatures[elem_number];
}

#if 0
void
egg_dbus_structure_print (EggDBusStructure *structure,
                        guint           indent)
{
  guint n;
  guint num_elements;

  g_return_if_fail (EGG_DBUS_IS_STRUCTURE (structure));

  g_print ("%s:\n", g_type_name (G_TYPE_FROM_INSTANCE (structure)));
  g_print ("%*swrapping:  struct %s\n", indent + 2, "", egg_dbus_structure_get_signature (structure));
  g_print ("%*selements:\n",
           indent + 2, "");

  num_elements = egg_dbus_structure_get_num_elements (structure);

  for (n = 0; n < num_elements; n++)
    {
      gchar *name;
      GValue value = {0};
      const gchar *signature;
      gboolean simple;

      name = g_strdup_printf ("element%d", n);

      signature = egg_dbus_structure_get_signature_for_element (structure, n);

      egg_dbus_structure_get_element_as_gvalue (structure, n, &value);

      g_print ("%*s%s:", indent + 4, "", name);

      /* simple types can be printed on the same line */
      simple = TRUE;
      if (G_VALUE_HOLDS (&value, EGG_DBUS_TYPE_ARRAY_SEQ) ||
          G_VALUE_HOLDS (&value, EGG_DBUS_TYPE_HASH_MAP) ||
          G_VALUE_HOLDS (&value, EGG_DBUS_TYPE_STRUCTURE) ||
          G_VALUE_HOLDS (&value, EGG_DBUS_TYPE_VARIANT))
        simple = FALSE;

      if (simple)
        g_print (" ");
      else
        g_print ("\n%*s", indent + 6, "");

      egg_dbus_utils_print_gvalue (&value, signature, indent + 6);

      g_value_unset (&value);

      g_free (name);
    }
}
#endif

gpointer
egg_dbus_structure_type_check_instance_cast (gpointer      instance,
                                             const gchar  *signature,
                                             const gchar  *c_type_name)
{
  if (!EGG_DBUS_IS_STRUCTURE (instance))
    {
      g_warning ("invalid cast to %s", c_type_name);
    }
  else
    {
      EggDBusStructure *structure = EGG_DBUS_STRUCTURE (instance);
      EggDBusStructurePrivate *priv;

      priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

      if (strcmp (priv->signature, signature) != 0)
        {
          g_warning ("invalid cast from EggDBusStructure with signature %s to %s with signature %s",
                     priv->signature,
                     c_type_name,
                     signature);
        }
    }

  return instance;
}

gboolean
egg_dbus_structure_type_check_instance_type (gpointer      instance,
                                             const gchar  *signature,
                                             const gchar  *c_type_name)
{
  gboolean ret;

  ret = FALSE;

  if (!EGG_DBUS_IS_STRUCTURE (instance))
    {
      goto out;
    }
  else
    {
      EggDBusStructure *structure = EGG_DBUS_STRUCTURE (instance);
      EggDBusStructurePrivate *priv;

      priv = EGG_DBUS_STRUCTURE_GET_PRIVATE (structure);

      if (strcmp (priv->signature, signature) != 0)
        goto out;

      ret = TRUE;
    }

 out:
  return ret;
}

