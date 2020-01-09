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
#include <eggdbus/eggdbustypes.h>
#include <eggdbus/eggdbusprivate.h>
#include <eggdbus/eggdbusvariant.h>

/* ---------------------------------------------------------------------------------------------------- */

/* hash table from collection instance (e.g. GList *) to ref counted signature (of the form "<refcount>:<sig>") */
static GHashTable *collection_elem_signature_hash = NULL;

static const gchar *
_element_signature_get_sig (const gchar *sig_with_refcount, int *num_refs)
{
  const char *s;

  if (num_refs != NULL)
    *num_refs = atoi (sig_with_refcount);

  s = (const char *) strchr (sig_with_refcount, ':');

  return s + 1;
}

const gchar *
_get_element_signature (gpointer collection, const gchar *collection_type)
{
  gpointer orig_key;
  gpointer value;

  if (collection_elem_signature_hash == NULL)
    goto fail;

  if (!g_hash_table_lookup_extended (collection_elem_signature_hash, collection, &orig_key, &value))
    goto fail;

  return _element_signature_get_sig ((const gchar *) value, NULL);

 fail:
  g_warning ("Unable to determine element signature for %s %p", collection_type, collection);
  return NULL;
}

void
_ref_element_signature (gpointer collection, const gchar *collection_type)
{
  gpointer orig_key;
  gpointer value;

  if (g_hash_table_lookup_extended (collection_elem_signature_hash, collection, &orig_key, &value))
    {
      int num_refs;
      const gchar *sig;

      sig = _element_signature_get_sig ((char *) value, &num_refs);
      //g_warning ("reffed to ref_count %d signature %s for %s %p", num_refs + 1, sig, collection_type, collection);
      g_hash_table_insert (collection_elem_signature_hash,
                           collection,
                           g_strdup_printf ("%d:%s", num_refs + 1, sig));
    }
  else
    {
      g_warning ("Unable to ref element signature for %s %p", collection_type, collection);
    }
}

void
_set_element_signature (gpointer collection, const gchar *signature, const gchar *collection_type)
{
  gpointer orig_key;
  gpointer value;
  int num_refs;
  const gchar *sig;

  if (signature == NULL)
    {
      if (collection_elem_signature_hash == NULL)
        goto fail;

      if (!g_hash_table_lookup_extended (collection_elem_signature_hash, collection, &orig_key, &value))
        goto fail;

      sig = _element_signature_get_sig ((char *) value, &num_refs);
      if (num_refs > 1)
        {
          //g_warning ("unreffed to ref_count %d signature %s for %s %p", num_refs - 1, sig, collection_type, collection);
          g_hash_table_insert (collection_elem_signature_hash,
                               collection,
                               g_strdup_printf ("%d:%s", num_refs - 1, sig));
        }
      else
        {
          //g_warning ("removed signature %s for %s %p", sig, collection_type, collection);
          g_hash_table_remove (collection_elem_signature_hash, collection);

          /* free the hash table if it's empty */
          if (g_hash_table_size (collection_elem_signature_hash) == 0)
            {
              //g_warning ("  freeing hash table");
              g_hash_table_destroy (collection_elem_signature_hash);
              collection_elem_signature_hash = NULL;
            }
        }
    }
  else
    {
      if (collection_elem_signature_hash == NULL)
        collection_elem_signature_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

      /* if we already have this signature, "ref" it */
      if (g_hash_table_lookup_extended (collection_elem_signature_hash, collection, &orig_key, &value))
        {
          sig = _element_signature_get_sig ((char *) value, &num_refs);
          //g_warning ("reffed to ref_count %d signature %s for %s %p", num_refs + 1, sig, collection_type, collection);
          g_hash_table_insert (collection_elem_signature_hash,
                               collection,
                               g_strdup_printf ("%d:%s", num_refs + 1, sig));
        }
      else
        {
          //g_warning ("added signature %s for %s %p", signature, collection_type, collection);
          g_hash_table_insert (collection_elem_signature_hash,
                               collection,
                               g_strdup_printf ("1:%s", signature));
        }
    }

  //if (collection_elem_signature_hash != NULL)
  //  g_warning (" hash table size %d", g_hash_table_size (collection_elem_signature_hash));

 fail:
  return;
}

/* ---------------------------------------------------------------------------------------------------- */

