/* sysprof-empty-state-view.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (SYSPROF_UI_INSIDE) && !defined (SYSPROF_UI_COMPILATION)
# error "Only <sysprof-ui.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <sysprof.h>

G_BEGIN_DECLS

#define SYSPROF_TYPE_EMPTY_STATE_VIEW (sysprof_empty_state_view_get_type())

SYSPROF_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (SysprofEmptyStateView, sysprof_empty_state_view, SYSPROF, EMPTY_STATE_VIEW, GtkBin)

struct _SysprofEmptyStateViewClass
{
  GtkBinClass parent;

  gpointer padding[4];
};

SYSPROF_AVAILABLE_IN_ALL
GtkWidget *sysprof_empty_state_view_new (void);

G_END_DECLS
