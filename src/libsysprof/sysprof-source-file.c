/* sysprof-source-file.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "sysprof-source-file-private.h"

#define LINES_PER_PAGE 256

typedef struct
{
  guint64 self_count;
  guint64 total_count;
  guint64 last_sample;
} Line;

/**
 * SysprofSourceFile:
 *
 * Immutable sample counts for a source file. Line numbers are 1-based.
 * Sparse pages provide expected constant-time lookup without allocating memory
 * proportional to the largest line number. Lookups allocate nothing and may be
 * used from any thread after the profile has been built.
 *
 * Since: 52
 */
struct _SysprofSourceFile
{
  GObject     parent_instance;
  GFile      *file;
  GHashTable *pages;
  guint64     max_self;
  guint64     max_total;
};

G_DEFINE_FINAL_TYPE (SysprofSourceFile, sysprof_source_file, G_TYPE_OBJECT)

static void
sysprof_source_file_finalize (GObject *object)
{
  SysprofSourceFile *self = SYSPROF_SOURCE_FILE (object);

  g_clear_object (&self->file);
  g_clear_pointer (&self->pages, g_hash_table_unref);

  G_OBJECT_CLASS (sysprof_source_file_parent_class)->finalize (object);
}

static void
sysprof_source_file_class_init (SysprofSourceFileClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = sysprof_source_file_finalize;
}

static void
sysprof_source_file_init (SysprofSourceFile *self)
{
  self->pages = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

SysprofSourceFile *
_sysprof_source_file_new (GFile *file)
{
  SysprofSourceFile *self;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  self = g_object_new (SYSPROF_TYPE_SOURCE_FILE, NULL);
  self->file = g_object_ref (file);

  return self;
}

void
_sysprof_source_file_add (SysprofSourceFile *self,
                          guint              line,
                          guint64            sample_id,
                          gboolean           is_self)
{
  Line *page;
  Line *entry;
  gpointer key = GUINT_TO_POINTER (line / LINES_PER_PAGE);

  g_return_if_fail (SYSPROF_IS_SOURCE_FILE (self));
  g_return_if_fail (line > 0);
  g_return_if_fail (sample_id > 0);

  if (!(page = g_hash_table_lookup (self->pages, key)))
    {
      page = g_new0 (Line, LINES_PER_PAGE);
      g_hash_table_insert (self->pages, key, page);
    }

  entry = &page[line % LINES_PER_PAGE];

  if (entry->last_sample != sample_id)
    {
      entry->last_sample = sample_id;
      entry->total_count++;
      self->max_total = MAX (self->max_total, entry->total_count);
    }

  if (is_self)
    {
      entry->self_count++;
      self->max_self = MAX (self->max_self, entry->self_count);
    }
}

/**
 * sysprof_source_file_dup_file:
 * @self: a source file profile
 *
 * Gets the source file identified by the debug information, translated through
 * the capture's mount namespace where possible. The file need not exist.
 *
 * Returns: (transfer full): the source file
 * Since: 52
 */
GFile *
sysprof_source_file_dup_file (SysprofSourceFile *self)
{
  g_return_val_if_fail (SYSPROF_IS_SOURCE_FILE (self), NULL);

  return g_object_ref (self->file);
}

/**
 * sysprof_source_file_get_line:
 * @self: a source file profile
 * @line: a 1-based source line
 * @self_count: (out) (optional): samples executing on this line
 * @total_count: (out) (optional): samples whose stacks include this line
 *
 * Looks up sample counts without allocation. Inclusive counts include self
 * counts and count recursive appearances of a line only once per sample.
 * Unknown lines, including line 0, return zero counts.
 *
 * Returns: whether the line has any samples
 * Since: 52
 */
gboolean
sysprof_source_file_get_line (SysprofSourceFile *self,
                              guint              line,
                              guint64           *self_count,
                              guint64           *total_count)
{
  const Line *page;
  const Line *entry;

  g_return_val_if_fail (SYSPROF_IS_SOURCE_FILE (self), FALSE);

  page = g_hash_table_lookup (self->pages, GUINT_TO_POINTER (line / LINES_PER_PAGE));
  entry = page != NULL ? &page[line % LINES_PER_PAGE] : NULL;

  if (self_count != NULL)
    *self_count = entry != NULL ? entry->self_count : 0;

  if (total_count != NULL)
    *total_count = entry != NULL ? entry->total_count : 0;

  return entry != NULL && entry->total_count > 0;
}

/**
 * sysprof_source_file_get_max_count:
 * @self: a source file profile
 * @inclusive: whether to use inclusive counts instead of self counts
 *
 * Returns: the largest line count in this file, or zero if there are no counts
 * Since: 52
 */
guint64
sysprof_source_file_get_max_count (SysprofSourceFile *self,
                                   gboolean           inclusive)
{
  g_return_val_if_fail (SYSPROF_IS_SOURCE_FILE (self), 0);

  return inclusive ? self->max_total : self->max_self;
}

/**
 * sysprof_source_file_get_line_heat:
 * @self: a source file profile
 * @line: a 1-based source line
 * @inclusive: whether to use inclusive counts instead of self counts
 *
 * Normalizes a line count against the hottest line in this file. This is a
 * linear intensity suitable for mapping onto an application's color palette,
 * not a fraction of capture duration. Unknown and unsampled lines return zero.
 *
 * Returns: a value between 0 and 1
 * Since: 52
 */
double
sysprof_source_file_get_line_heat (SysprofSourceFile *self,
                                   guint              line,
                                   gboolean           inclusive)
{
  guint64 self_count;
  guint64 total_count;
  guint64 maximum;

  g_return_val_if_fail (SYSPROF_IS_SOURCE_FILE (self), 0);

  maximum = inclusive ? self->max_total : self->max_self;
  if (maximum == 0)
    return 0;

  sysprof_source_file_get_line (self, line, &self_count, &total_count);

  return (double)(inclusive ? total_count : self_count) / maximum;
}
