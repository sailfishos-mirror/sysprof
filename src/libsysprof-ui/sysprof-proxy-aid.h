/* sysprof-proxy-aid.h
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

#include "sysprof-aid.h"

G_BEGIN_DECLS

#define SYSPROF_TYPE_PROXY_AID (sysprof_proxy_aid_get_type())

G_DECLARE_DERIVABLE_TYPE (SysprofProxyAid, sysprof_proxy_aid, SYSPROF, PROXY_AID, SysprofAid)

struct _SysprofProxyAidClass
{
  SysprofAidClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

SYSPROF_AVAILABLE_IN_ALL
void sysprof_proxy_aid_set_bus_type    (SysprofProxyAid *self,
                                        GBusType         bus_type);
SYSPROF_AVAILABLE_IN_ALL
void sysprof_proxy_aid_set_bus_name    (SysprofProxyAid *self,
                                        const gchar     *bus_name);
SYSPROF_AVAILABLE_IN_ALL
void sysprof_proxy_aid_set_object_path (SysprofProxyAid *self,
                                        const gchar     *obj_path);

G_END_DECLS
