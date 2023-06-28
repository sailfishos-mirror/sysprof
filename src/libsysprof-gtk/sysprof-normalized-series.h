/* sysprof-normalized-series.h
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

#include <gtk/gtk.h>

#include "sysprof-axis.h"
#include "sysprof-series.h"

G_BEGIN_DECLS

#define SYSPROF_TYPE_NORMALIZED_SERIES         (sysprof_normalized_series_get_type())
#define SYSPROF_IS_NORMALIZED_SERIES(obj)      (G_TYPE_CHECK_INSTANCE_TYPE(obj, SYSPROF_TYPE_NORMALIZED_SERIES))
#define SYSPROF_NORMALIZED_SERIES(obj)         (G_TYPE_CHECK_INSTANCE_CAST(obj, SYSPROF_TYPE_NORMALIZED_SERIES, SysprofNormalizedSeries))
#define SYSPROF_NORMALIZED_SERIES_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, SYSPROF_TYPE_NORMALIZED_SERIES, SysprofNormalizedSeriesClass))

typedef struct _SysprofNormalizedSeries SysprofNormalizedSeries;
typedef struct _SysprofNormalizedSeriesClass SysprofNormalizedSeriesClass;

SYSPROF_AVAILABLE_IN_ALL
GType          sysprof_normalized_series_get_type       (void) G_GNUC_CONST;
SYSPROF_AVAILABLE_IN_ALL
SysprofSeries *sysprof_normalized_series_new            (SysprofSeries           *series,
                                                         SysprofAxis             *axis,
                                                         GtkExpression           *expression);
SYSPROF_AVAILABLE_IN_ALL
gboolean       sysprof_normalized_series_get_inverted   (SysprofNormalizedSeries *self);
SYSPROF_AVAILABLE_IN_ALL
void           sysprof_normalized_series_set_inverted   (SysprofNormalizedSeries *self,
                                                         gboolean                 inverted);
SYSPROF_AVAILABLE_IN_ALL
GtkExpression *sysprof_normalized_series_get_expression (SysprofNormalizedSeries *self);
SYSPROF_AVAILABLE_IN_ALL
void           sysprof_normalized_series_set_expression (SysprofNormalizedSeries *self,
                                                         GtkExpression           *expression);
SYSPROF_AVAILABLE_IN_ALL
SysprofAxis   *sysprof_normalized_series_get_axis       (SysprofNormalizedSeries *self);
SYSPROF_AVAILABLE_IN_ALL
void           sysprof_normalized_series_set_axis       (SysprofNormalizedSeries *self,
                                                         SysprofAxis             *axis);
SYSPROF_AVAILABLE_IN_ALL
SysprofSeries *sysprof_normalized_series_get_series     (SysprofNormalizedSeries *self);
SYSPROF_AVAILABLE_IN_ALL
void           sysprof_normalized_series_set_series     (SysprofNormalizedSeries *self,
                                                         SysprofSeries           *series);
SYSPROF_AVAILABLE_IN_ALL
float          sysprof_normalized_series_value_at       (SysprofNormalizedSeries *self,
                                                         guint                    position);
SYSPROF_AVAILABLE_IN_ALL
const float   *sysprof_normalized_series_get_values     (SysprofNormalizedSeries *self,
                                                         guint                   *n_values);

G_END_DECLS
