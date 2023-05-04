/* sysprof-bundled-symbolizer.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "sysprof-bundled-symbolizer.h"
#include "sysprof-document-private.h"
#include "sysprof-symbolizer-private.h"

struct _SysprofBundledSymbolizer
{
  SysprofSymbolizer parent_instance;
};

struct _SysprofBundledSymbolizerClass
{
  SysprofSymbolizerClass parent_class;
};

G_DEFINE_FINAL_TYPE (SysprofBundledSymbolizer, sysprof_bundled_symbolizer, SYSPROF_TYPE_SYMBOLIZER)

static void
sysprof_bundled_symbolizer_decode (SysprofBundledSymbolizer *self,
                                   GBytes                   *bytes,
                                   gboolean                  is_native)
{
  g_assert (SYSPROF_IS_BUNDLED_SYMBOLIZER (self));
  g_assert (bytes != NULL);

}

static void
sysprof_bundled_symbolizer_prepare_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  SysprofDocument *document = (SysprofDocument *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;

  g_assert (SYSPROF_IS_DOCUMENT (document));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if ((bytes = sysprof_document_lookup_file_finish (document, result, &error)))
    sysprof_bundled_symbolizer_decode (g_task_get_source_object (task),
                                       bytes,
                                       _sysprof_document_is_native (document));

  g_task_return_boolean (task, TRUE);
}

static void
sysprof_bundled_symbolizer_prepare_async (SysprofSymbolizer   *symbolizer,
                                          SysprofDocument     *document,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (SYSPROF_IS_BUNDLED_SYMBOLIZER (symbolizer));
  g_assert (SYSPROF_IS_DOCUMENT (document));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (symbolizer, cancellable, callback, user_data);
  g_task_set_source_tag (task, sysprof_bundled_symbolizer_prepare_async);

  sysprof_document_lookup_file_async (document,
                                      "__symbols__",
                                      cancellable,
                                      sysprof_bundled_symbolizer_prepare_cb,
                                      g_steal_pointer (&task));

}

static gboolean
sysprof_bundled_symbolizer_prepare_finish (SysprofSymbolizer  *symbolizer,
                                           GAsyncResult       *result,
                                           GError            **error)
{
  g_assert (SYSPROF_IS_BUNDLED_SYMBOLIZER (symbolizer));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (result, symbolizer));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
sysprof_bundled_symbolizer_finalize (GObject *object)
{
  G_OBJECT_CLASS (sysprof_bundled_symbolizer_parent_class)->finalize (object);
}

static void
sysprof_bundled_symbolizer_class_init (SysprofBundledSymbolizerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SysprofSymbolizerClass *symbolizer_class = SYSPROF_SYMBOLIZER_CLASS (klass);

  object_class->finalize = sysprof_bundled_symbolizer_finalize;

  symbolizer_class->prepare_async = sysprof_bundled_symbolizer_prepare_async;
  symbolizer_class->prepare_finish = sysprof_bundled_symbolizer_prepare_finish;
}

static void
sysprof_bundled_symbolizer_init (SysprofBundledSymbolizer *self)
{
}

SysprofSymbolizer *
sysprof_bundled_symbolizer_new (void)
{
  return g_object_new (SYSPROF_TYPE_BUNDLED_SYMBOLIZER, NULL);
}
