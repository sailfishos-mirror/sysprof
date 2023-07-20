/* sysprof-callgraph-private.h
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

#include "sysprof-callgraph.h"
#include "sysprof-document.h"

#include "eggbitset.h"

G_BEGIN_DECLS

typedef struct _SysprofCallgraphSummary
{
  SysprofSymbol *symbol;
  EggBitset     *traceables;
  GPtrArray     *callers;
  gpointer       augment[2];
} SysprofCallgraphSummary;

struct _SysprofCallgraphNode
{
  SysprofCallgraphNode     *parent;
  SysprofCallgraphNode     *prev;
  SysprofCallgraphNode     *next;
  SysprofCallgraphNode     *children;
  SysprofCallgraphSummary  *summary;
  gpointer                  augment[2];
  SysprofCallgraphCategory  category;
};

struct _SysprofCallgraph
{
  GObject                  parent_instance;

  SysprofDocument         *document;
  GListModel              *traceables;

  GHashTable              *symbol_to_summary;
  GPtrArray               *symbols;

  SysprofCallgraphFlags    flags;

  gsize                    augment_size;
  SysprofAugmentationFunc  augment_func;
  gpointer                 augment_func_data;
  GDestroyNotify           augment_func_data_destroy;

  SysprofCallgraphNode     root;
};

void                      _sysprof_callgraph_new_async          (SysprofDocument          *document,
                                                                 SysprofCallgraphFlags     flags,
                                                                 GListModel               *traceables,
                                                                 gsize                     augment_size,
                                                                 SysprofAugmentationFunc   augment_func,
                                                                 gpointer                  augment_func_data,
                                                                 GDestroyNotify            augment_func_data_destroy,
                                                                 GCancellable             *cancellable,
                                                                 GAsyncReadyCallback       callback,
                                                                 gpointer                  user_data);
SysprofCallgraph         *_sysprof_callgraph_new_finish         (GAsyncResult             *result,
                                                                 GError                  **error);
gpointer                  _sysprof_callgraph_get_symbol_augment (SysprofCallgraph         *self,
                                                                 SysprofSymbol            *symbol);
void                      _sysprof_callgraph_node_free          (SysprofCallgraphNode     *self,
                                                                 gboolean                  free_self);
SysprofCallgraphCategory  _sysprof_callgraph_node_categorize    (SysprofCallgraphNode     *node);

G_END_DECLS
