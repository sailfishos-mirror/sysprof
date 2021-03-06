/* sysprof-symbol-map.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <sysprof-capture.h>

#include "sysprof-symbol-resolver.h"

G_BEGIN_DECLS

typedef struct _SysprofSymbolMap SysprofSymbolMap;

SysprofSymbolMap *sysprof_symbol_map_new          (void);
void              sysprof_symbol_map_add_resolver (SysprofSymbolMap           *self,
                                                   SysprofSymbolResolver      *resolver);
void              sysprof_symbol_map_resolve      (SysprofSymbolMap           *self,
                                                   SysprofCaptureReader       *reader);
const gchar      *sysprof_symbol_map_lookup       (SysprofSymbolMap           *self,
                                                   gint64                      time,
                                                   gint32                      pid,
                                                   SysprofCaptureAddress       addr,
                                                   GQuark                     *tag);
void              sysprof_symbol_map_printf       (SysprofSymbolMap           *self);
gboolean          sysprof_symbol_map_serialize    (SysprofSymbolMap           *self,
                                                   gint                        fd);
gboolean          sysprof_symbol_map_deserialize  (SysprofSymbolMap           *self,
                                                   gint                        byte_order,
                                                   gint                        fd);
void              sysprof_symbol_map_free         (SysprofSymbolMap           *self);

G_END_DECLS
