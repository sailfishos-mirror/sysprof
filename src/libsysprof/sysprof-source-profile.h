/* sysprof-source-profile.h
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

#define SYSPROF_TYPE_SOURCE_PROFILE (sysprof_source_profile_get_type())

SYSPROF_AVAILABLE_IN_52
G_DECLARE_FINAL_TYPE (SysprofSourceProfile, sysprof_source_profile, SYSPROF, SOURCE_PROFILE, GObject)

SYSPROF_AVAILABLE_IN_52
SysprofSourceFile *sysprof_source_profile_dup_file               (SysprofSourceProfile *self,
                                                                  GFile                *file);
SYSPROF_AVAILABLE_IN_52
GPtrArray         *sysprof_source_profile_dup_files              (SysprofSourceProfile *self);
SYSPROF_AVAILABLE_IN_52
guint64            sysprof_source_profile_get_n_samples          (SysprofSourceProfile *self);
SYSPROF_AVAILABLE_IN_52
guint64            sysprof_source_profile_get_n_resolved_samples (SysprofSourceProfile *self);

G_END_DECLS
