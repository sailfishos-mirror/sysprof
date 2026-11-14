/* sysprof-source-file.h
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

#pragma once

#include <gio/gio.h>

#include "sysprof-types.h"

G_BEGIN_DECLS

#define SYSPROF_TYPE_SOURCE_FILE (sysprof_source_file_get_type())

SYSPROF_AVAILABLE_IN_52
G_DECLARE_FINAL_TYPE (SysprofSourceFile, sysprof_source_file, SYSPROF, SOURCE_FILE, GObject)

SYSPROF_AVAILABLE_IN_52
GFile    *sysprof_source_file_dup_file      (SysprofSourceFile *self);
SYSPROF_AVAILABLE_IN_52
gboolean  sysprof_source_file_get_line      (SysprofSourceFile *self,
                                             guint              line,
                                             guint64           *self_count,
                                             guint64           *total_count);
SYSPROF_AVAILABLE_IN_52
guint64   sysprof_source_file_get_max_count (SysprofSourceFile *self,
                                             gboolean           inclusive);
SYSPROF_AVAILABLE_IN_52
double    sysprof_source_file_get_line_heat (SysprofSourceFile *self,
                                             guint              line,
                                             gboolean           inclusive);

G_END_DECLS
