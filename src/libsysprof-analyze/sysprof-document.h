/* sysprof-document.h
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

#pragma once

#include <gio/gio.h>

#include <sysprof-capture.h>

#include "sysprof-callgraph.h"
#include "sysprof-document-file.h"
#include "sysprof-document-traceable.h"
#include "sysprof-symbol.h"

G_BEGIN_DECLS

#define SYSPROF_TYPE_DOCUMENT (sysprof_document_get_type())

SYSPROF_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (SysprofDocument, sysprof_document, SYSPROF, DOCUMENT, GObject)

SYSPROF_AVAILABLE_IN_ALL
SysprofDocumentFile    *sysprof_document_lookup_file         (SysprofDocument           *self,
                                                              const char                *path);
SYSPROF_AVAILABLE_IN_ALL
GListModel             *sysprof_document_list_files          (SysprofDocument           *self);
SYSPROF_AVAILABLE_IN_ALL
GListModel             *sysprof_document_list_traceables     (SysprofDocument           *self);
SYSPROF_AVAILABLE_IN_ALL
GListModel             *sysprof_document_list_samples        (SysprofDocument           *self);
SYSPROF_AVAILABLE_IN_ALL
GListModel             *sysprof_document_list_processes      (SysprofDocument           *self);
SYSPROF_AVAILABLE_IN_ALL
GListModel             *sysprof_document_list_jitmaps        (SysprofDocument           *self);
SYSPROF_AVAILABLE_IN_ALL
GListModel             *sysprof_document_list_counters       (SysprofDocument           *self);
SYSPROF_AVAILABLE_IN_ALL
guint                   sysprof_document_symbolize_traceable (SysprofDocument           *self,
                                                              SysprofDocumentTraceable  *traceable,
                                                              SysprofSymbol            **symbols,
                                                              guint                      n_symbols);
SYSPROF_AVAILABLE_IN_ALL
void                    sysprof_document_callgraph_async     (SysprofDocument             *self,
                                                              GListModel                  *traceables,
                                                              gsize                        augment_size,
                                                              SysprofAugmentationFunc      augment_func,
                                                              gpointer                     augment_func_data,
                                                              GDestroyNotify               augment_func_data_destroy,
                                                              GCancellable                *cancellable,
                                                              GAsyncReadyCallback          callback,
                                                              gpointer                     user_data);
SYSPROF_AVAILABLE_IN_ALL
SysprofCallgraph       *sysprof_document_callgraph_finish    (SysprofDocument           *self,
                                                              GAsyncResult              *result,
                                                              GError                   **error);

G_END_DECLS
