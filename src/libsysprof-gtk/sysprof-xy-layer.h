/* sysprof-xy-layer.h
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

#include <sysprof-analyze.h>

#include "sysprof-axis.h"
#include "sysprof-chart-layer.h"
#include "sysprof-xy-series.h"

G_BEGIN_DECLS

#define SYSPROF_TYPE_XY_LAYER (sysprof_xy_layer_get_type())

SYSPROF_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (SysprofXYLayer, sysprof_xy_layer, SYSPROF, XY_LAYER, SysprofChartLayer)

SYSPROF_AVAILABLE_IN_ALL
SysprofChartLayer *sysprof_xy_layer_new        (void);
SYSPROF_AVAILABLE_IN_ALL
SysprofXYSeries   *sysprof_xy_layer_get_series (SysprofXYLayer  *self);
SYSPROF_AVAILABLE_IN_ALL
void               sysprof_xy_layer_set_series (SysprofXYLayer  *self,
                                                SysprofXYSeries *series);
SYSPROF_AVAILABLE_IN_ALL
SysprofAxis       *sysprof_xy_layer_get_x_axis (SysprofXYLayer  *self);
SYSPROF_AVAILABLE_IN_ALL
void               sysprof_xy_layer_set_x_axis (SysprofXYLayer  *self,
                                                SysprofAxis     *x_axis);
SYSPROF_AVAILABLE_IN_ALL
SysprofAxis       *sysprof_xy_layer_get_y_axis (SysprofXYLayer  *self);
SYSPROF_AVAILABLE_IN_ALL
void               sysprof_xy_layer_set_y_axis (SysprofXYLayer  *self,
                                                SysprofAxis     *y_axis);

G_END_DECLS
