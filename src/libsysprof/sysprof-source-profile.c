/* sysprof-source-profile.c
 *
 * Copyright 2026 Christian Hergert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "sysprof-document-frame-private.h"
#include "sysprof-elf-loader-private.h"
#include "sysprof-source-file-private.h"
#include "sysprof-source-profile-private.h"

/**
 * SysprofSourceProfile:
 *
 * An immutable index of sampled source lines, built asynchronously by
 * [method@Document.source_profile_async]. File lookup uses [method@Gio.File.equal];
 * it does not resolve symlinks or match basenames. Each file can outlive the
 * profile and the capture document.
 *
 * Since: 52
 */
struct _SysprofSourceProfile
{
  GObject     parent_instance;
  GHashTable *files;
  guint64     n_samples;
  guint64     n_resolved_samples;
};

typedef struct
{
  SysprofDocument  *document;
  GCancellable     *cancellable;
  char            **debug_dirs;
  char            **external_debug_dirs;
} Build;

typedef struct
{
  SysprofProcessInfo *process;
  guint64             address;
} Address;

typedef struct
{
  SysprofSourceFile *file;
  guint              line;
} Location;

G_DEFINE_FINAL_TYPE (SysprofSourceProfile, sysprof_source_profile, G_TYPE_OBJECT)

static void
sysprof_source_profile_finalize (GObject *object)
{
  SysprofSourceProfile *self = SYSPROF_SOURCE_PROFILE (object);

  g_clear_pointer (&self->files, g_hash_table_unref);

  G_OBJECT_CLASS (sysprof_source_profile_parent_class)->finalize (object);
}

static void
sysprof_source_profile_class_init (SysprofSourceProfileClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = sysprof_source_profile_finalize;
}

static void
sysprof_source_profile_init (SysprofSourceProfile *self)
{
  self->files = g_hash_table_new_full (g_file_hash,
                                       (GEqualFunc)g_file_equal,
                                       g_object_unref,
                                       g_object_unref);
}

static void
build_free (gpointer data)
{
  Build *build = data;

  g_clear_object (&build->document);
  g_clear_object (&build->cancellable);
  g_strfreev (build->debug_dirs);
  g_strfreev (build->external_debug_dirs);
  g_free (build);
}

static guint
address_hash (gconstpointer data)
{
  const Address *address = data;

  return g_direct_hash (address->process) ^ g_int64_hash (&address->address);
}

static gboolean
address_equal (gconstpointer a,
               gconstpointer b)
{
  const Address *left = a;
  const Address *right = b;

  return left->process == right->process && left->address == right->address;
}

static Location *
resolve_address (SysprofSourceProfile *self,
                 SysprofElfLoader     *loader,
                 SysprofProcessInfo   *process,
                 guint64               address)
{
  g_autoptr(SysprofElf) elf = NULL;
  g_autoptr(SysprofSourceLocation) source = NULL;
  g_autoptr(GFile) file = NULL;
  SysprofDocumentMmap *map;
  SysprofSourceFile *source_file;
  Location *location;
  guint64 offset;
  guint line;

  g_assert (SYSPROF_IS_SOURCE_PROFILE (self));
  g_assert (SYSPROF_IS_ELF_LOADER (loader));

  if (process == NULL ||
      process->address_layout == NULL ||
      process->mount_namespace == NULL)
    return NULL;

  if ((address & G_GUINT64_CONSTANT (0xFFFFFFFF00000000)) ==
      G_GUINT64_CONSTANT (0xE000000000000000))
    return NULL;

  if (!(map = sysprof_address_layout_lookup (process->address_layout, address)))
    return NULL;

  offset = address - sysprof_document_mmap_get_start_address (map);
  if (offset > G_MAXUINT64 - sysprof_document_mmap_get_file_offset (map))
    return NULL;
  offset += sysprof_document_mmap_get_file_offset (map);

  if (!(elf = sysprof_elf_loader_load (loader,
                                       process->mount_namespace,
                                       sysprof_document_mmap_get_file (map),
                                       sysprof_document_mmap_get_build_id (map),
                                       sysprof_document_mmap_get_file_inode (map),
                                       NULL)))
    return NULL;

  if (!(source = sysprof_elf_resolve_source_location (elf, process->mount_namespace, offset)))
    return NULL;

  line = sysprof_source_location_get_line (source);
  if (line == 0)
    return NULL;

  file = sysprof_source_location_dup_file (source);
  if (!(source_file = g_hash_table_lookup (self->files, file)))
    {
      source_file = _sysprof_source_file_new (file);
      g_hash_table_insert (self->files, g_object_ref (file), source_file);
    }

  location = g_new0 (Location, 1);
  location->file = source_file;
  location->line = line;

  return location;
}

static DexFuture *
build_thread (gpointer data)
{
  Build *build = data;
  g_autoptr(SysprofSourceProfile) self = NULL;
  g_autoptr(SysprofElfLoader) loader = NULL;
  g_autoptr(GListModel) samples = NULL;
  g_autoptr(GHashTable) cache = NULL;
  g_autoptr(GArray) addresses = NULL;
  guint n_samples;

  g_assert (build != NULL);
  g_assert (SYSPROF_IS_DOCUMENT (build->document));

  if (g_cancellable_is_cancelled (build->cancellable))
    goto cancelled;

#if !HAVE_LIBDW
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Source profiling requires libdw support");
#endif

  self = g_object_new (SYSPROF_TYPE_SOURCE_PROFILE, NULL);
  loader = sysprof_elf_loader_new ();
  if (build->debug_dirs != NULL)
    sysprof_elf_loader_set_debug_dirs (loader, (const char * const *)build->debug_dirs);

  if (build->external_debug_dirs != NULL)
    sysprof_elf_loader_set_external_debug_dirs (loader,
                                                (const char * const *)build->external_debug_dirs);

  cache = g_hash_table_new_full (address_hash, address_equal, g_free, g_free);
  addresses = g_array_new (FALSE, FALSE, sizeof (guint64));
  samples = sysprof_document_list_samples (build->document);
  n_samples = g_list_model_get_n_items (samples);
  self->n_samples = n_samples;

  for (guint i = 0; i < n_samples; i++)
    {
      g_autoptr(SysprofDocumentTraceable) sample = g_list_model_get_item (samples, i);
      SysprofProcessInfo *process = SYSPROF_DOCUMENT_FRAME (sample)->process_info;
      SysprofAddressContext context = SYSPROF_ADDRESS_CONTEXT_NONE;
      guint depth = sysprof_document_traceable_get_stack_depth (sample);
      gboolean has_previous_frame = FALSE;
      gboolean seen_frame = FALSE;

      if (g_cancellable_is_cancelled (build->cancellable))
        goto cancelled;

      g_array_set_size (addresses, depth);
      depth = sysprof_document_traceable_get_stack_addresses (sample,
                                                              (guint64 *)addresses->data,
                                                              depth);

      for (guint j = 0; j < depth; j++)
        {
          guint64 address = g_array_index (addresses, guint64, j);
          SysprofAddressContext next_context;
          Address key;
          Location *location;
          gboolean is_self;
          gboolean is_return;

          if (g_cancellable_is_cancelled (build->cancellable))
            goto cancelled;

          if (sysprof_address_is_context_switch (address, &next_context))
            {
              context = next_context;
              has_previous_frame = FALSE;
              continue;
            }

          is_self = !seen_frame;
          is_return = has_previous_frame;
          seen_frame = TRUE;
          has_previous_frame = TRUE;

          if (context != SYSPROF_ADDRESS_CONTEXT_NONE && context != SYSPROF_ADDRESS_CONTEXT_USER)
            continue;

          if (is_return)
            {
              if (address == 0)
                continue;
              address--;
            }

          key.process = process;
          key.address = address;

          if (!g_hash_table_lookup_extended (cache, &key, NULL, (gpointer *)&location))
            {
              location = resolve_address (self, loader, process, address);
              g_hash_table_insert (cache, g_memdup2 (&key, sizeof key), location);
            }

          if (location == NULL)
            continue;

          _sysprof_source_file_add (location->file, location->line, (guint64)i + 1, is_self);
          if (is_self)
            self->n_resolved_samples++;
        }
    }

  if (g_cancellable_is_cancelled (build->cancellable))
    goto cancelled;

  return dex_future_new_take_object (g_steal_pointer (&self));

cancelled:
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_CANCELLED,
                                "Source profiling was cancelled");
}

DexFuture *
_sysprof_source_profile_new (SysprofDocument    *document,
                             const char * const *debug_dirs,
                             const char * const *external_debug_dirs,
                             GCancellable       *cancellable)
{
  static DexThreadPool *pool;
  static gsize pool_initialized;
  Build *build;

  dex_return_error_if_fail (SYSPROF_IS_DOCUMENT (document));
  dex_return_error_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (g_once_init_enter (&pool_initialized))
    {
      pool = dex_thread_pool_new (MIN (4, MAX (1, g_get_num_processors ())));
      g_once_init_leave (&pool_initialized, 1);
    }

  if (pool == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Could not create source profile worker pool");

  build = g_new0 (Build, 1);
  build->document = g_object_ref (document);
  build->cancellable = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  build->debug_dirs = g_strdupv ((char **)debug_dirs);
  build->external_debug_dirs = g_strdupv ((char **)external_debug_dirs);

  return dex_thread_pool_submit (pool,
                                 "sysprof-source-profile",
                                 build_thread,
                                 build,
                                 build_free);
}

/**
 * sysprof_source_profile_dup_file:
 * @self: a source profile
 * @file: a source file
 *
 * Looks up a file in expected constant time. Keep the returned object in the
 * gutter renderer to avoid looking up the filename for each line.
 *
 * Returns: (transfer full) (nullable): the file's profile, or %NULL if absent
 * Since: 52
 */
SysprofSourceFile *
sysprof_source_profile_dup_file (SysprofSourceProfile *self,
                                 GFile                *file)
{
  SysprofSourceFile *source_file;

  g_return_val_if_fail (SYSPROF_IS_SOURCE_PROFILE (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  source_file = g_hash_table_lookup (self->files, file);

  return source_file != NULL ? g_object_ref (source_file) : NULL;
}

/**
 * sysprof_source_profile_dup_files:
 * @self: a source profile
 *
 * Returns: (transfer full) (element-type SysprofSourceFile): the files with
 *   resolved samples, in unspecified order
 * Since: 52
 */
GPtrArray *
sysprof_source_profile_dup_files (SysprofSourceProfile *self)
{
  GPtrArray *files;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (SYSPROF_IS_SOURCE_PROFILE (self), NULL);

  files = g_ptr_array_new_with_free_func (g_object_unref);
  g_hash_table_iter_init (&iter, self->files);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    g_ptr_array_add (files, g_object_ref (value));

  return files;
}

/**
 * sysprof_source_profile_get_n_samples:
 * @self: a source profile
 *
 * Returns: the number of samples, including empty and unresolved stacks
 * Since: 52
 */
guint64
sysprof_source_profile_get_n_samples (SysprofSourceProfile *self)
{
  g_return_val_if_fail (SYSPROF_IS_SOURCE_PROFILE (self), 0);

  return self->n_samples;
}

/**
 * sysprof_source_profile_get_n_resolved_samples:
 * @self: a source profile
 *
 * Returns: the number of samples whose executing instruction resolved to a
 *   source line; caller-only matches do not contribute to this count
 * Since: 52
 */
guint64
sysprof_source_profile_get_n_resolved_samples (SysprofSourceProfile *self)
{
  g_return_val_if_fail (SYSPROF_IS_SOURCE_PROFILE (self), 0);

  return self->n_resolved_samples;
}
