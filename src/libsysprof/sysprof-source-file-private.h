/* sysprof-source-file-private.h
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

#pragma once

#include "sysprof-source-file.h"

G_BEGIN_DECLS

SysprofSourceFile *_sysprof_source_file_new (GFile             *file);
void               _sysprof_source_file_add (SysprofSourceFile *self,
                                             guint              line,
                                             guint64            sample_id,
                                             gboolean           is_self);

G_END_DECLS
