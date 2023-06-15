/* sysprof-time-span.h
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

#include <glib-object.h>

#include <sysprof-capture.h>

G_BEGIN_DECLS

#define SYSPROF_TYPE_TIME_SPAN (sysprof_time_span_get_type())

typedef struct _SysprofTimeSpan
{
  gint64 begin_nsec;
  gint64 end_nsec;
} SysprofTimeSpan;

SYSPROF_AVAILABLE_IN_ALL
GType            sysprof_time_span_get_type (void) G_GNUC_CONST;
SYSPROF_AVAILABLE_IN_ALL
SysprofTimeSpan *sysprof_time_span_copy     (const SysprofTimeSpan *self);
SYSPROF_AVAILABLE_IN_ALL
void             sysprof_time_span_free     (SysprofTimeSpan       *self);

G_END_DECLS
