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
#include <eggdbus/eggdbusarrayseq.h>
#include <eggdbus/eggdbusmisctypes.h>
#include <eggdbus/eggdbusprivate.h>

/**
 * SECTION:eggdbusarrayseq
 * @title: EggDBusArraySeq
 * @short_description: Arrays
 *
 * An array type that can store elements of a given #GType. See egg_dbus_array_seq_new() for details.
 *
 * The array will automatically grow when elements are added and all accessor functions
 * dealing with index-based access checks if the index is within bounds. If an index
 * is not within the bounds of the array a warning is issued using g_error() (causing
 * program termination).
 *
 * When possible (when the elements are derived from #GObject) type checking will be performed to
 * ensure an inserted element is compatible with the element type of the array. If the check fails,
 * a warning is issued using g_error() (causing program termination).
 *
 * For implementing operations that involve comparing elements (such as egg_dbus_array_seq_contains()),
 * a #GEqualFunc is needed. For most types (such as #G_TYPE_STRING and #G_TYPE_INT), the natural
 * #GEqualFunc is used but for e.g. #G_TYPE_OBJECT derived types one will need to be provided.
 * Note that said operations are <emphasis>optional</emphasis> in the sense that if a #GEqualFunc
 * is not provided other operations will still work; the operations needing the equal function
 * will just fail and call g_error() (causing program termination).
 *
 * By default, the array takes ownership when inserting elements meaning that the programmer gives
 * up his reference. Elements extracted from the array are owned by the array. There is also convenience
 * API to get a copy of the item, see egg_dbus_array_seq_get_copy().
 *
 * Note that this class exposes a number of implementation details directly in the class
 * instance structure for efficient and convenient access when used from the C programming
 * language. Use with caution. For the same reasons, this class also provides a number of
 * convenience functions for dealing with fixed-size integral and floating point numbers.
 */


typedef struct
{
  /* if element_type is a fixed-size type, these are both NULL */
  gpointer        (*copy_func) (EggDBusArraySeq *array_seq, gconstpointer element);
  GDestroyNotify  free_func;
  GEqualFunc      equal_func;
  GBoxedCopyFunc  user_copy_func;

  guint capacity;

  /* cached value of G_IS_OBJECT_TYPE() to avoid overhead on every insertion */
  gboolean element_type_is_gobject_derived;

  /* cached value to determine if it's a fixed size array */
  gboolean element_type_is_fixed_size;

  /* cached value for C convenience functions */
  GType element_fundamental_type;
} EggDBusArraySeqPrivate;

#define EGG_DBUS_ARRAY_SEQ_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EGG_DBUS_TYPE_ARRAY_SEQ, EggDBusArraySeqPrivate))

G_DEFINE_TYPE (EggDBusArraySeq, egg_dbus_array_seq, G_TYPE_OBJECT);

static void
egg_dbus_array_seq_init (EggDBusArraySeq *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);
}

static void
egg_dbus_array_seq_finalize (GObject *object)
{
  EggDBusArraySeq *array_seq;
  EggDBusArraySeqPrivate *priv;
  guint n;

  array_seq = EGG_DBUS_ARRAY_SEQ (object);
  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  if (priv->free_func != NULL)
    for (n = 0; n < array_seq->size; n++)
      if (array_seq->data.v_ptr[n] != NULL)
        priv->free_func (array_seq->data.v_ptr[n]);
  g_free (array_seq->data.data);

  G_OBJECT_CLASS (egg_dbus_array_seq_parent_class)->finalize (object);
}

/* ---------------------------------------------------------------------------------------------------- */

static gpointer
copy_via_user_copy_func (EggDBusArraySeq *array_seq,
                         gconstpointer    element)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  return element == NULL ? NULL : priv->user_copy_func ((gpointer) element);
}

static gpointer
copy_elem_object (EggDBusArraySeq *array_seq,
                  gconstpointer    element)
{
  return element == NULL ? NULL : g_object_ref ((gpointer) element);
}

static gpointer
copy_elem_string (EggDBusArraySeq *array_seq,
                  gconstpointer    element)
{
  return g_strdup (element);
}

static gpointer
copy_elem_boxed (EggDBusArraySeq *array_seq,
                 gconstpointer    element)
{
  return element == NULL ? NULL : g_boxed_copy (array_seq->element_type, element);
}

static gpointer
copy_elem_param_spec (EggDBusArraySeq *array_seq,
                      gconstpointer    element)
{
  return element == NULL ? NULL : g_param_spec_ref ((gpointer) element);
}

/* ---------------------------------------------------------------------------------------------------- */

#define IMPL_EQUAL_FUNC(type)                          \
  static gboolean                                      \
  _##type##_equal (gconstpointer v1, gconstpointer v2) \
  {                                                    \
    const type *a = v1;                                \
    const type *b = v2;                                \
    return *a == *b;                                   \
  }

IMPL_EQUAL_FUNC (guchar);
IMPL_EQUAL_FUNC (gint);
IMPL_EQUAL_FUNC (gint64);
IMPL_EQUAL_FUNC (gboolean);
IMPL_EQUAL_FUNC (gdouble);
IMPL_EQUAL_FUNC (glong);
IMPL_EQUAL_FUNC (gfloat);
IMPL_EQUAL_FUNC (gint16);

#if 0
/* TODO: use this on G_TYPE_STRV and derived types */
static gboolean
_strv_equal (gchar **a, gchar **b)
{
  guint n;
  guint len;

  if (a == NULL && b == NULL)
    return TRUE;
  if (a == NULL || b == NULL)
    return FALSE;

  len = g_strv_length (a);
  if (g_strv_length (b) != len)
    return FALSE;

  for (n = 0; n < len; n++)
    {
      if (strcmp (a[n], b[n]) != 0)
        return FALSE;
    }

  return TRUE;
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

static void
ensure_size (EggDBusArraySeq *array_seq,
             guint            minimum_size)
{
  EggDBusArraySeqPrivate *priv;
  guint minimum_capacity;
  guint old_capacity;
  guint old_size;

  if (array_seq->size >= minimum_size)
    return;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  /* always allocate 8 elements at a time */
  minimum_capacity = (guint) ((minimum_size + 7) / 8) * 8;

  old_capacity = priv->capacity;
  old_size = array_seq->size;

  priv->capacity = MAX (minimum_capacity, priv->capacity);
  array_seq->size = MAX (minimum_size, array_seq->size);

  /* grow if needed */
  if (priv->capacity > old_capacity)
    {
      array_seq->data.data = g_realloc (array_seq->data.data, array_seq->element_size * priv->capacity);
    }

  /* initialize the elements added */
  if (array_seq->size > old_size)
    {
      memset (array_seq->data.v_byte + old_size * array_seq->element_size,
              0,
              (array_seq->size - old_size) * array_seq->element_size);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
egg_dbus_array_seq_class_init (EggDBusArraySeqClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = egg_dbus_array_seq_finalize;

  g_type_class_add_private (klass, sizeof (EggDBusArraySeqPrivate));
}

/**
 * egg_dbus_array_seq_new:
 * @element_type: The type of the elements in the array.
 * @free_func: Function to be used to free elements or %NULL.
 * @copy_func: Function to be used to copy elements or %NULL to use the default copy function.
 * @equal_func: Function to be used for comparing equality or %NULL to use the default equality function.
 *
 * Creates a new array that holds elements of @element_type and uses @free_func to free
 * elements when they are no longer in use (e.g. when removed from the array either directly
 * by calling e.g. egg_dbus_array_seq_remove_at() or if replaced by another element through
 * egg_dbus_array_seq_set()). If @free_func is %NULL, then it is the responsibility of the owner
 * of the array to free the elements when they are no longer used.
 *
 * If @copy_func is %NULL, the default copy function is used if one exists for @element_type
 * (for example there is no default copy function if @element_type is #G_TYPE_POINTER). Note
 * that <emphasis>optional</emphasis> methods such as egg_dbus_array_seq_get_copy() won't
 * work if there is no copy function.
 *
 * If @equal_func is %NULL, the default equality function is used if one exists for @element_type
 * (for example there is no default equality function if @element_type is a subtype of
 * #G_TYPE_OBJECT, #G_TYPE_INTERFACE or #G_TYPE_BOXED). Note that <emphasis>optional</emphasis>
 * methods such as egg_dbus_array_seq_contains() won't work if there is no equality function.
 *
 * If the type of elements is not a fixed-size type (such as #GObject derived types), the array
 * will store pointers and all value pointers to and from this class are to the actual elements
 * (the array is simply an array of pointers). For example, to implement @equal_func when
 * @element_type is #G_TYPE_FILE, one would do the following:
 *
 * <programlisting>
 * static gboolean
 * my_file_equal_func (gconstpointer _a, gconstpointer _b)
 * {
 *   GVolume *a = G_FILE (_a);
 *   GVolume *b = G_FILE (_b)
 *   gboolean is_equal;
 *
 *   /<!-- -->* compute is_equal by comparing a and b *<!-- -->/
 *
 *   return is_equal;
 * }
 * </programlisting>
 *
 * or, in this specific case, just pass g_file_equal() as @equal_func.
 *
 * If the type of the elements is a fixed-size type (such as #G_TYPE_INT, #G_TYPE_DOUBLE or a
 * #G_TYPE_ENUM derived type), all value pointers used throughout this class is for the address
 * of where the fixed-size value is stored inside the array. This is because the raw value are stored
 * in the array; as such no pointers to elements are ever used (in addition, this means that @free_func
 * and @copy_func are never used on such arrays). For example, for #G_TYPE_DOUBLE, you'd define
 * @equal_func like this:
 *
 * <programlisting>
 * static gboolean
 * my_double_equal_func (gconstpointer _a, gconstpointer _b)
 * {
 *   gdouble a = *((gdouble *) _a);
 *   gdouble b = *((gdouble *) _b);
 *   gboolean is_equal;
 *
 *   /<!-- -->* compute is_equal by comparing a and b *<!-- -->/
 *
 *   return is_equal;
 * }
 * </programlisting>
 *
 * Note that the default equality functions for integral and floating point types should
 * be good enough for all but exotic corner cases.
 *
 * Returns: A new #EggDBusArraySeq. Free with g_object_unref().
 **/
EggDBusArraySeq *
egg_dbus_array_seq_new (GType                 element_type,
                        GDestroyNotify        free_func,
                        GBoxedCopyFunc        copy_func,
                        GEqualFunc            equal_func)
{
  EggDBusArraySeq *array_seq;
  EggDBusArraySeqPrivate *priv;
  gboolean not_supported;

  array_seq = EGG_DBUS_ARRAY_SEQ (g_object_new (EGG_DBUS_TYPE_ARRAY_SEQ, NULL));

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  array_seq->element_type = element_type;

  priv->equal_func = equal_func;
  priv->free_func = free_func;

  not_supported = FALSE;

  priv->element_fundamental_type = G_TYPE_FUNDAMENTAL (array_seq->element_type);

  /* first compute copy_func, free_func, element_size and equal_func */
  switch (priv->element_fundamental_type)
    {
    case G_TYPE_OBJECT:
    case G_TYPE_INTERFACE:
      priv->copy_func = copy_elem_object;
      array_seq->element_size = sizeof (gpointer);
      priv->element_type_is_gobject_derived = TRUE;
      break;

    case G_TYPE_BOXED:
      priv->copy_func = copy_elem_boxed;
      array_seq->element_size = sizeof (gpointer);
      break;

    case G_TYPE_PARAM:
      priv->copy_func = copy_elem_param_spec;
      array_seq->element_size = sizeof (gpointer);
      break;

    case G_TYPE_STRING:
      priv->copy_func = copy_elem_string;
      array_seq->element_size = sizeof (gpointer);

      if (priv->equal_func == NULL)
        priv->equal_func = g_str_equal;

      break;

    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (guchar);
      if (priv->equal_func == NULL)
        priv->equal_func = _guchar_equal;
      break;

    case G_TYPE_INT:
    case G_TYPE_UINT:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (gint);
      if (priv->equal_func == NULL)
        priv->equal_func = _gint_equal;
      break;

    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (gint64);
      if (priv->equal_func == NULL)
        priv->equal_func = _gint64_equal;
      break;

    case G_TYPE_BOOLEAN:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (gboolean);
      if (priv->equal_func == NULL)
        priv->equal_func = _gboolean_equal;
      break;

    case G_TYPE_DOUBLE:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (gdouble);
      if (priv->equal_func == NULL)
        priv->equal_func = _gdouble_equal;
      break;

    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (glong);
      if (priv->equal_func == NULL)
        priv->equal_func = _glong_equal;
      break;

    case G_TYPE_FLOAT:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (gfloat);
      if (priv->equal_func == NULL)
        priv->equal_func = _gfloat_equal;
      break;

    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
      priv->element_type_is_fixed_size = TRUE;
      array_seq->element_size = sizeof (gint);
      if (priv->equal_func == NULL)
        priv->equal_func = _gint_equal;
      break;

    case G_TYPE_POINTER:
      array_seq->element_size = sizeof (gpointer);
      break;

    default:
      /* can't have 16bit types in the switch since they're not constant
       * (Thanks GObject for not having a GType for these)
       */
      if (array_seq->element_type == EGG_DBUS_TYPE_INT16 ||
          array_seq->element_type == EGG_DBUS_TYPE_UINT16)
        {
          priv->element_type_is_fixed_size = TRUE;
          array_seq->element_size = sizeof (gint16);
          if (priv->equal_func == NULL)
            priv->equal_func = _gint16_equal;
        }
      else
        {
          not_supported = TRUE;
        }
      break;
    }

  if (priv->element_type_is_fixed_size && free_func != NULL)
    {
      g_error ("Meaningless to specify free_func for EggDBusArraySeq<%s>.",
               g_type_name (array_seq->element_type));
    }

  if (priv->element_type_is_fixed_size && copy_func != NULL)
    {
      g_error ("Meaningless to specify copy_func for EggDBusArraySeq<%s>.",
               g_type_name (array_seq->element_type));
    }

  if (copy_func != NULL)
    {
      priv->user_copy_func = copy_func;
      priv->copy_func = copy_via_user_copy_func;
    }

  if (not_supported)
    {
      /* didn't recognize type; this is a programmer/user error; contract says we can only manage
       * boxed or GObject-derived types
       */
      g_error ("Unsupported type %s used as element type for EggDBusArraySeq.",
               g_type_name (array_seq->element_type));
    }

  return array_seq;
}

static gboolean
check_same_element_type (EggDBusArraySeq   *array_seq,
                         EggDBusArraySeq   *other_array_seq)
{
  if (G_LIKELY (array_seq->element_type == other_array_seq->element_type))
    return TRUE;

  g_error ("Can't add elements from EggDBusArraySeq<%s> to EggDBusArraySeq<%s>",
           g_type_name (other_array_seq->element_type),
           g_type_name (array_seq->element_type));

  return FALSE;
}

static gboolean
check_have_equal_func (EggDBusArraySeq   *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  if (G_LIKELY (priv->equal_func != NULL))
    return TRUE;

  g_error ("no equal_func set for EggDBusArraySeq<%s>",
           g_type_name (array_seq->element_type));

  return FALSE;
}

static gboolean
check_have_copy_func (EggDBusArraySeq   *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  if (G_LIKELY (priv->element_type_is_fixed_size || priv->copy_func != NULL))
    return TRUE;

  g_error ("no copy_func set for EggDBusArraySeq<%s>",
           g_type_name (array_seq->element_type));

  return FALSE;
}

static gboolean
check_index (EggDBusArraySeq   *array_seq,
             gint               index)
{
  if (G_LIKELY (index >= 0 && index < (gint) array_seq->size))
    return TRUE;

  g_error ("index %d is out of bounds on EggDBusArraySeq<%s> of size %d",
           index,
           g_type_name (array_seq->element_type),
           array_seq->size);
  return FALSE;
}

static gboolean
check_element_type (EggDBusArraySeq   *array_seq,
                    GType              element_type)
{
  /* first the cheap check */
  if (G_LIKELY (array_seq->element_type == element_type))
    return TRUE;

  /* then the more expensive one */
  if (G_LIKELY (g_type_is_a (element_type, array_seq->element_type)))
    return TRUE;

  /* otherwise.. BOOM */

  g_error ("Cannot insert an element of type %s into a EggDBusArraySeq<%s>",
           g_type_name (element_type),
           g_type_name (array_seq->element_type));
  return FALSE;
}

/**
 * egg_dbus_array_seq_get_size:
 * @array_seq: A #EggDBusArraySeq.
 *
 * Gets the size of @array_seq.
 *
 * Returns: The number of elements in @array_seq.
 **/
guint
egg_dbus_array_seq_get_size (EggDBusArraySeq *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  return array_seq->size;
}

/**
 * egg_dbus_array_seq_set_size:
 * @array_seq: A #EggDBusArraySeq.
 * @size: New size of the array.
 *
 * Sets the size of @array_seq to @size.
 *
 * If @size is less than the current size, then elements with indices greater or equal
 * than @size will be freed.
 *
 * If @size is greater than the current size, the newly added elements will be set to
 * either %NULL or 0 depending on the #GType of the elements of @array_seq.
 **/
void
egg_dbus_array_seq_set_size (EggDBusArraySeq  *array_seq,
                             guint             size)
{
  EggDBusArraySeqPrivate *priv;
  guint n;
  guint old_size;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  old_size = array_seq->size;

  if (old_size == size)
    {
      /* do nothing */
    }
  else if (old_size > size)
    {
      /* throw away elements */

      if (priv->free_func != NULL)
        {
          for (n = size; n < old_size; n++)
            if (array_seq->data.v_ptr[n] != NULL)
              priv->free_func (array_seq->data.v_ptr[n]);
        }

      /* set new size */
      array_seq->size = size;
    }
  else
    {
      /* old_size < size  => realloc() + pad with zeros */
      ensure_size (array_seq, size);
    }
}

/**
 * egg_dbus_array_seq_get_element_size:
 * @array_seq: A #EggDBusArraySeq.
 *
 * Gets the size of each element.
 *
 * If @array_seq contains elements on non-fixed size, <literal>sizeof</literal> #gpointer
 * is returned.
 *
 * Returns: The size, in bytes, of each element.
 **/
gsize
egg_dbus_array_seq_get_element_size (EggDBusArraySeq *array_seq)
{
  return array_seq->element_size;
}

/**
 * egg_dbus_array_seq_get_element_type:
 * @array_seq: A #EggDBusArraySeq.
 *
 * Gets the type of the elements stored in @array_seq.
 *
 * Returns: The #GType of for the elements in @array_seq.
 **/
GType
egg_dbus_array_seq_get_element_type (EggDBusArraySeq *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  return array_seq->element_type;
}

/**
 * egg_dbus_array_seq_have_copy_func:
 * @array_seq: A #EggDBusArraySeq.
 *
 * Checks if @array_seq have a copy function.
 *
 * Returns: %TRUE only if there is a copy function for @array_seq.
 **/
gboolean
egg_dbus_array_seq_have_copy_func (EggDBusArraySeq      *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  return priv->copy_func != NULL;
}

/**
 * egg_dbus_array_seq_get_equal_func:
 * @array_seq: A #EggDBusArraySeq.
 *
 * Gets the #GEqualFunc used for comparing equality of elements.
 *
 * Returns: A #GEqualFunc.
 **/
GEqualFunc
egg_dbus_array_seq_get_equal_func (EggDBusArraySeq *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  return priv->equal_func;
}


/**
 * egg_dbus_array_seq_remove_at:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 *
 * Removes the element at @index from @array_seq. All elements following @index will
 * be shifted down by one and the size of @array_seq will shrink by one.
 **/
void
egg_dbus_array_seq_remove_at (EggDBusArraySeq  *array_seq,
                              gint              index)
{
  egg_dbus_array_seq_remove_range_at (array_seq, index, 1);
}

/**
 * egg_dbus_array_seq_remove_range_at:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 * @size: The number of elements to remove starting from @index.
 *
 * Like egg_dbus_array_seq_remove_at() but removes @size consecutive elements starting
 * from @index.
 **/
void
egg_dbus_array_seq_remove_range_at  (EggDBusArraySeq  *array_seq,
                                     gint              index,
                                     guint             size)
{
  EggDBusArraySeqPrivate *priv;
  guint n;

  if (!check_index (array_seq, index))
    return;

  if (!check_index (array_seq, index + size - 1))
    return;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  /* free elements we're removing */
  if (priv->free_func != NULL)
    {
      for (n = index; n < index + size; n++)
        if (array_seq->data.v_ptr[n] != NULL)
          priv->free_func (array_seq->data.v_ptr[n]);
    }

  /* move data (this works for both pointers and primitive types) */
  if (array_seq->size - (index + size) > 0)
    {
      g_memmove (array_seq->data.v_byte + index * array_seq->element_size,          /* dst */
                 array_seq->data.v_byte + (index + size) * array_seq->element_size, /* src */
                 (array_seq->size - (index + size)) * array_seq->element_size);
    }

  /* adjust size */
  array_seq->size -= size;
}

/**
 * egg_dbus_array_seq_clear:
 * @array_seq: A #EggDBusArraySeq.
 *
 * Removes all elements from @array_seq.
 **/
void
egg_dbus_array_seq_clear (EggDBusArraySeq   *array_seq)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  egg_dbus_array_seq_remove_range_at (array_seq,
                                      0,
                                      array_seq->size);
}

/**
 * egg_dbus_array_seq_get:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 *
 * Gets the element at @index from @array_seq.
 *
 * Note that the returned element is owned by @array_seq and may be invalid if
 * later removed from the array. If you want a copy, use egg_dbus_array_seq_get_copy()
 * instead.
 *
 * This is a constant-time operation.
 *
 * Returns: The requested element which is owned by @array_seq.
 **/
gpointer
egg_dbus_array_seq_get (EggDBusArraySeq   *array_seq,
                        gint               index)
{
  EggDBusArraySeqPrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_ARRAY_SEQ (array_seq), NULL);

  if (!check_index (array_seq, index))
    return NULL;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  if (! priv->element_type_is_fixed_size)
    return array_seq->data.v_ptr[index];
  else
    return array_seq->data.v_byte + index * array_seq->element_size;
}

/**
 * egg_dbus_array_seq_get_copy:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 *
 * Gets a copy of the element at @index from @array_seq. If you don't want your
 * own copy use egg_dbus_array_seq_get() instead.
 *
 * This method is <emphasis>optional</emphasis> as some element types (for example #G_TYPE_POINTER
 * and derived types) have no natural copy function and one might not have been set when @array_seq
 * was constructed. It is a programming error to call this method on @array_seq if there
 * is no copy function on @array_seq (a warning will be printed using g_error() causing program
 * termination).
 *
 * Returns: A copy of the requested element. Free with the appropriate
 * function depending on the element type of @array_seq.
 **/
gpointer
egg_dbus_array_seq_get_copy (EggDBusArraySeq   *array_seq,
                            gint               index)
{
  EggDBusArraySeqPrivate *priv;

  g_return_val_if_fail (EGG_DBUS_IS_ARRAY_SEQ (array_seq), NULL);

  if (!check_index (array_seq, index))
    return NULL;

  if (!check_have_copy_func (array_seq))
    return NULL;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  if (! priv->element_type_is_fixed_size)
    {
      return priv->copy_func (array_seq, array_seq->data.v_ptr[index]);
    }
  else
    {
      return g_memdup (array_seq->data.v_byte + index * array_seq->element_size,
                       array_seq->element_size);
    }
}

/**
 * egg_dbus_array_seq_set:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 * @value: The value to insert.
 *
 * Replaces the element at @index in @array_seq with @value.
 *
 * Note that this function <emphasis>steals</emphasis> your reference to @value.
 *
 * This is a constant-time operation.
 **/
void
egg_dbus_array_seq_set (EggDBusArraySeq  *array_seq,
                        gint              index,
                        gconstpointer     value)
{
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  if (!check_index (array_seq, index))
    return;

  if (priv->element_type_is_gobject_derived)
    {
      if (!check_element_type (array_seq, G_OBJECT_TYPE (value)))
        return;
    }

  /* out with the old... */
  if (priv->free_func != NULL)
    if (array_seq->data.v_ptr[index] != NULL)
      priv->free_func (array_seq->data.v_ptr[index]);

  /* and in with the new.. */
  if (! priv->element_type_is_fixed_size)
    {
      array_seq->data.v_ptr[index] = (gpointer) value;
    }
  else
    {
      memcpy (array_seq->data.v_byte + index * array_seq->element_size,
              value,
              array_seq->element_size);
    }
}

/**
 * egg_dbus_array_seq_add:
 * @array_seq: A #EggDBusArraySeq.
 * @value: The value to append.
 *
 * Appends @value to the end of @array_seq. The size of @array_seq will grow by one.
 *
 * Note that this function <emphasis>steals</emphasis> your reference to @value.
 *
 * This is a constant time operation.
 *
 * Returns: Always %TRUE.
 **/
gboolean
egg_dbus_array_seq_add (EggDBusArraySeq  *array_seq,
                        gconstpointer     value)
{
  /* grow, then add a copy of the element */
  ensure_size (array_seq, array_seq->size + 1);
  egg_dbus_array_seq_set (array_seq,
                          array_seq->size - 1,
                          value);
  return TRUE;
}

/**
 * egg_dbus_array_seq_insert:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 * @value: The value to append.
 *
 * Inserts @value at @index of @array_seq. All elements currently at or after @index will
 * be shifted up by one and the size of @array_seq will grow by one.
 *
 * Note that this function <emphasis>steals</emphasis> your reference to @value.
 **/
void
egg_dbus_array_seq_insert (EggDBusArraySeq  *array_seq,
                           gint              index,
                           gconstpointer     value)
{
  EggDBusArraySeqPrivate *priv;
  guint old_size;

  if (!check_index (array_seq, index))
    return;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  old_size = array_seq->size;
  ensure_size (array_seq, array_seq->size + 1);

  if (old_size - index > 0)
    {
      g_memmove (array_seq->data.v_byte + (index + 1) * array_seq->element_size,    /* dst */
                 array_seq->data.v_byte + index * array_seq->element_size,          /* src */
                 (old_size - index) * array_seq->element_size);
    }

  memset (array_seq->data.v_byte + index * array_seq->element_size,
          0,
          array_seq->element_size);

  egg_dbus_array_seq_set (array_seq,
                          index,
                          value);
}

/**
 * egg_dbus_array_seq_index_of:
 * @array_seq: A #EggDBusArraySeq.
 * @value: The value to check for.
 *
 * Find the first occurence of an element equal to @value in @array_seq.
 *
 * This method is <emphasis>optional</emphasis>. It is a programing error to call this
 * method on @array_seq if there is no #GEqualFunc set for @array_seq (a warning will be
 * printed using g_error() causing program termination).
 *
 * Returns: The index of the first occurence of an element equal to @value
 *          in @array_seq or -1 if no such elements exist.
 **/
gint
egg_dbus_array_seq_index_of (EggDBusArraySeq  *array_seq,
                             gconstpointer     value)
{
  EggDBusArraySeqPrivate *priv;
  guint n;

  if (!check_have_equal_func (array_seq))
    return -1;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  for (n = 0; n < array_seq->size; n++)
    {
      gboolean found;

      if (! priv->element_type_is_fixed_size)
        found = priv->equal_func (array_seq->data.v_ptr[n], value);
      else
        found = priv->equal_func (array_seq->data.v_byte + n * array_seq->element_size, value);

      if (found)
        break;
    }

  return n == array_seq->size ? -1 : (gint) n;
}

/**
 * egg_dbus_array_seq_contains:
 * @array_seq: A #EggDBusArraySeq.
 * @value: The value to check for.
 *
 * Check if @array_seq contains an element equal to @value.
 *
 * This method is <emphasis>optional</emphasis>. It is a programing error to call this
 * method on @array_seq if there is no #GEqualFunc set for @array_seq (a warning will be
 * printed using g_error() causing program termination).
 *
 * Returns: %TRUE if @array_seq contains one or more elements equal to @value.
 **/
gboolean
egg_dbus_array_seq_contains (EggDBusArraySeq  *array_seq,
                         gconstpointer     value)
{
  return egg_dbus_array_seq_index_of (array_seq, value) != -1;
}

/**
 * egg_dbus_array_seq_remove:
 * @array_seq: A #EggDBusArraySeq.
 * @value: The value to remove.
 *
 * Remove the first occurence of elements equal to @value from @array_seq.
 *
 * This method is <emphasis>optional</emphasis>. It is a programing error to call this method
 * on @array_seq if there is no #GEqualFunc set for @array_seq (a warning will be
 * printed using g_error() causing program termination).
 *
 * Returns: %TRUE if an element was removed.
 **/
gboolean
egg_dbus_array_seq_remove (EggDBusArraySeq  *array_seq,
                           gconstpointer     value)
{
  gint index;

  index = egg_dbus_array_seq_index_of (array_seq, value);

  if (index == -1)
    return FALSE;

  egg_dbus_array_seq_remove_at (array_seq, index);
  return TRUE;
}


/**
 * egg_dbus_array_seq_add_all:
 * @array_seq: A #EggDBusArraySeq.
 * @other_array_seq: Another #EggDBusArraySeq with elements of the same type as @array_seq or %NULL.
 *
 * Appends a copy of all elements from @other_array_seq to @array_seq. If you don't want to copy
 * the elements, use egg_dbus_array_seq_steal_all() instead.
 *
 * If @array_seq and @other_array_seq does not contain elements of the same type, a warning
 * will be printed using g_error() (causing program termination).
 *
 * This method is <emphasis>optional</emphasis> as some element types (for example #G_TYPE_POINTER
 * and derived types) have no natural copy function and one might not have been set when @array_seq
 * was constructed. It is a programming error to call this method on @array_seq if there
 * is no copy function on @array_seq (a warning will be printed using g_error() causing program
 * termination).
 *
 * It is valid to call this method with @array_seq and @other_array_seq being the same array.
 *
 * Returns: Always %TRUE.
 **/
gboolean
egg_dbus_array_seq_add_all (EggDBusArraySeq      *array_seq,
                            EggDBusArraySeq      *other_array_seq)
{
  guint n;
  guint other_size;

  if (other_array_seq == NULL)
    return TRUE;

  if (!check_have_copy_func (array_seq))
    return FALSE;

  if (!check_same_element_type (array_seq, other_array_seq))
    return FALSE;

  other_size = other_array_seq->size;

  for (n = 0; n < other_size; n++)
    egg_dbus_array_seq_add (array_seq, egg_dbus_array_seq_get_copy (other_array_seq, n));

  return TRUE;
}

/**
 * egg_dbus_array_seq_steal_all:
 * @array_seq: A #EggDBusArraySeq.
 * @other_array_seq: Another #EggDBusArraySeq with elements of the same type as @array_seq or %NULL.
 *
 * Steals all elements from @other_array_seq and appends them to @array_seq. When this method returns
 * there will be no more elements in @other_array_seq. If you only want to copy the elements,
 * use egg_dbus_array_seq_add_all() instead.
 *
 * If @array_seq and @other_array_seq does not contain elements of the same type, a warning
 * will be printed using g_error() (causing program termination).
 *
 * It is an error to call this method if @array_seq and @other_array_seq is equal.
 *
 * Returns: Always %TRUE.
 **/
gboolean
egg_dbus_array_seq_steal_all (EggDBusArraySeq      *array_seq,
                              EggDBusArraySeq      *other_array_seq)
{
  guint old_size;

  if (other_array_seq == NULL)
    return TRUE;

  if (!check_same_element_type (array_seq, other_array_seq))
    return FALSE;

  if (G_UNLIKELY (array_seq == other_array_seq))
    {
      g_error ("Can't steal elements from the same array");
      return FALSE;
    }

  old_size = array_seq->size;
  ensure_size (array_seq, array_seq->size + other_array_seq->size);

  memcpy (array_seq->data.v_byte + old_size * array_seq->element_size,
          other_array_seq->data.data,
          other_array_seq->size * other_array_seq->element_size);

  g_free (other_array_seq->data.data);
  other_array_seq->data.data = NULL;
  other_array_seq->size = 0;

  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */
/* C convenience follows here */
/* ---------------------------------------------------------------------------------------------------- */

/**
 * egg_dbus_array_seq_add_fixed:
 * @array_seq: A #EggDBusArraySeq.
 * @value: The value to append.
 *
 * Appends @value to the end of @array_seq. The size of @array_seq will grow by one.
 *
 * This is a C convenience function for fixed-size integral types such as #G_TYPE_UCHAR, #G_TYPE_INT and so on.
 *
 * Returns: Always %TRUE.
 **/
gboolean
egg_dbus_array_seq_add_fixed (EggDBusArraySeq      *array_seq,
                              guint64               value)
{
  guchar  v_byte;
  guint16 v_int16;
  guint   v_int;
  gulong  v_long;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
      v_byte = value;
      egg_dbus_array_seq_add (array_seq, &v_byte);
      break;

    case G_TYPE_BOOLEAN:
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
    case G_TYPE_INT:
    case G_TYPE_UINT:
      v_int = value;
      egg_dbus_array_seq_add (array_seq, &v_int);
      break;

    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      egg_dbus_array_seq_add (array_seq, &value);
      break;

    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      v_long = value;
      egg_dbus_array_seq_add (array_seq, &v_long);
      break;

    default:
      /* can't have 16bit types in the switch since they're not constant
       */
      if (array_seq->element_type == EGG_DBUS_TYPE_INT16 ||
          array_seq->element_type == EGG_DBUS_TYPE_UINT16)
        {
          v_int16 = value;
          egg_dbus_array_seq_add (array_seq, &v_int16);
        }
      else
        {
          g_error ("Cannot use egg_dbus_array_seq_add_fixed() on EggDBusArraySeq<%s>",
                   g_type_name (array_seq->element_type));
        }
      break;
    }

  return TRUE;
}

/**
 * egg_dbus_array_seq_add_float:
 * @array_seq: A #EggDBusArraySeq.
 * @value: The value to append.
 *
 * Appends @value to the end of @array_seq. The size of @array_seq will grow by one.
 *
 * This is a C convenience function for the floating point types #G_TYPE_FLOAT and #G_TYPE_DOUBLE.
 *
 * Returns: Always %TRUE.
 **/
gboolean
egg_dbus_array_seq_add_float (EggDBusArraySeq      *array_seq,
                              gdouble               value)
{
  gfloat v_float;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_FLOAT:
      v_float = value;
      egg_dbus_array_seq_add (array_seq, &v_float);
      break;

    case G_TYPE_DOUBLE:
      egg_dbus_array_seq_add (array_seq, &value);
      break;

    default:
      g_error ("Cannot use egg_dbus_array_seq_add_float() on EggDBusArraySeq<%s>",
               g_type_name (array_seq->element_type));
      break;
    }

  return TRUE;
}

/**
 * egg_dbus_array_seq_set_fixed:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 * @value: The value to insert.
 *
 * Replaces the element at @index in @array_seq with @value.
 *
 * This is a C convenience function for fixed-size integral types such as #G_TYPE_UCHAR, #G_TYPE_INT and so on.
 **/
void
egg_dbus_array_seq_set_fixed (EggDBusArraySeq      *array_seq,
                              gint                  index,
                              guint64               value)
{
  guchar  v_byte;
  guint16 v_int16;
  guint   v_int;
  gulong  v_long;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
      v_byte = value;
      egg_dbus_array_seq_set (array_seq, index, &v_byte);
      break;

    case G_TYPE_BOOLEAN:
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
    case G_TYPE_INT:
    case G_TYPE_UINT:
      v_int = value;
      egg_dbus_array_seq_set (array_seq, index, &v_int);
      break;

    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      egg_dbus_array_seq_set (array_seq, index, &value);
      break;

    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      v_long = value;
      egg_dbus_array_seq_set (array_seq, index, &v_long);
      break;

    default:
      /* can't have 16bit types in the switch since they're not constant
       */
      if (array_seq->element_type == EGG_DBUS_TYPE_INT16 ||
          array_seq->element_type == EGG_DBUS_TYPE_UINT16)
        {
          v_int16 = value;
          egg_dbus_array_seq_set (array_seq, index, &v_int16);
        }
      else
        {
          g_error ("Cannot use egg_dbus_array_seq_set_fixed() on EggDBusArraySeq<%s>",
                   g_type_name (array_seq->element_type));
        }
      break;
    }
}

/**
 * egg_dbus_array_seq_set_float:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 * @value: The value to insert.
 *
 * Replaces the element at @index in @array_seq with @value.
 *
 * This is a C convenience function for the floating point types #G_TYPE_FLOAT and #G_TYPE_DOUBLE.
 **/
void
egg_dbus_array_seq_set_float (EggDBusArraySeq      *array_seq,
                              gint                  index,
                              gdouble               value)
{
  gfloat v_float;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_FLOAT:
      v_float = value;
      egg_dbus_array_seq_set (array_seq, index, &v_float);
      break;

    case G_TYPE_DOUBLE:
      egg_dbus_array_seq_set (array_seq, index, &value);
      break;

    default:
      g_error ("Cannot use egg_dbus_array_seq_set_float() on EggDBusArraySeq<%s>",
               g_type_name (array_seq->element_type));
      break;
    }
}

/**
 * egg_dbus_array_seq_insert_fixed:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 * @value: The value to append.
 *
 * Inserts @value at @index of @array_seq. All elements currently at or after @index will
 * be shifted up by one and the size of @array_seq will grow by one.
 *
 * This is a C convenience function for fixed-size integral types such as #G_TYPE_UCHAR, #G_TYPE_INT and so on.
 **/
void
egg_dbus_array_seq_insert_fixed (EggDBusArraySeq      *array_seq,
                                 gint                  index,
                                 guint64               value)
{
  guchar  v_byte;
  guint16 v_int16;
  guint   v_int;
  gulong  v_long;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
      v_byte = value;
      egg_dbus_array_seq_insert (array_seq, index, &v_byte);
      break;

    case G_TYPE_BOOLEAN:
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
    case G_TYPE_INT:
    case G_TYPE_UINT:
      v_int = value;
      egg_dbus_array_seq_insert (array_seq, index, &v_int);
      break;

    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      egg_dbus_array_seq_insert (array_seq, index, &value);
      break;

    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      v_long = value;
      egg_dbus_array_seq_insert (array_seq, index, &v_long);
      break;

    default:
      /* can't have 16bit types in the switch since they're not constant
       */
      if (array_seq->element_type == EGG_DBUS_TYPE_INT16 ||
          array_seq->element_type == EGG_DBUS_TYPE_UINT16)
        {
          v_int16 = value;
          egg_dbus_array_seq_insert (array_seq, index, &v_int16);
        }
      else
        {
          g_error ("Cannot use egg_dbus_array_seq_insert_fixed() on EggDBusArraySeq<%s>",
                   g_type_name (array_seq->element_type));
        }
      break;
    }
}

/**
 * egg_dbus_array_seq_insert_float:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 * @value: The value to append.
 *
 * Inserts @value at @index of @array_seq. All elements currently at or after @index will
 * be shifted up by one and the size of @array_seq will grow by one.
 *
 * This is a C convenience function for the floating point types #G_TYPE_FLOAT and #G_TYPE_DOUBLE.
 **/
void
egg_dbus_array_seq_insert_float (EggDBusArraySeq      *array_seq,
                                 gint                  index,
                                 gdouble               value)
{
  gfloat v_float;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_FLOAT:
      v_float = value;
      egg_dbus_array_seq_insert (array_seq, index, &v_float);
      break;

    case G_TYPE_DOUBLE:
      egg_dbus_array_seq_insert (array_seq, index, &value);
      break;

    default:
      g_error ("Cannot use egg_dbus_array_seq_insert_float() on EggDBusArraySeq<%s>",
               g_type_name (array_seq->element_type));
      break;
    }
}


/**
 * egg_dbus_array_seq_get_fixed:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 *
 * Gets the element at @index from @array_seq.
 *
 * This is a C convenience function for fixed-size integral types such as #G_TYPE_UCHAR, #G_TYPE_INT and so on.
 *
 * Returns: The requested element.
 **/
guint64
egg_dbus_array_seq_get_fixed (EggDBusArraySeq      *array_seq,
                              gint                  index)
{
  gpointer elem;
  guint64 val;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  elem = egg_dbus_array_seq_get (array_seq, index);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
      val = *((guchar *) elem);
      break;

    case G_TYPE_BOOLEAN:
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
    case G_TYPE_INT:
    case G_TYPE_UINT:
      val = *((gint *) elem);
      break;

    case G_TYPE_INT64:
    case G_TYPE_UINT64:
      val = *((gint64 *) elem);
      break;

    case G_TYPE_LONG:
    case G_TYPE_ULONG:
      val = *((glong *) elem);
      break;

    default:
      /* can't have 16bit types in the switch since they're not constant
       */
      if (array_seq->element_type == EGG_DBUS_TYPE_INT16 ||
          array_seq->element_type == EGG_DBUS_TYPE_UINT16)
        {
          val = *((gint16 *) elem);
        }
      else
        {
          g_error ("Cannot use egg_dbus_array_seq_get_fixed() on EggDBusArraySeq<%s>",
                   g_type_name (array_seq->element_type));
        }
      break;
    }

  return val;
}

/**
 * egg_dbus_array_seq_get_float:
 * @array_seq: A #EggDBusArraySeq.
 * @index: Zero-based index of element.
 *
 * Gets the element at @index from @array_seq.
 *
 * This is a C convenience function for the floating point types #G_TYPE_FLOAT and #G_TYPE_DOUBLE.
 *
 * Returns: The requested element.
 **/
gdouble
egg_dbus_array_seq_get_float (EggDBusArraySeq      *array_seq,
                              gint                  index)
{
  gpointer elem;
  gdouble val;
  EggDBusArraySeqPrivate *priv;

  priv = EGG_DBUS_ARRAY_SEQ_GET_PRIVATE (array_seq);

  elem = egg_dbus_array_seq_get (array_seq, index);

  switch (priv->element_fundamental_type)
    {
    case G_TYPE_FLOAT:
      val = *((gfloat *) elem);
      break;

    case G_TYPE_DOUBLE:
      val = *((gdouble *) elem);
      break;

    default:
      g_error ("Cannot use egg_dbus_array_seq_get_float() on EggDBusArraySeq<%s>",
               g_type_name (array_seq->element_type));
      break;
    }

  return val;
}
