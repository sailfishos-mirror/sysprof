/* sysprof-document-fork.h
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

#include "sysprof-document-frame.h"

G_BEGIN_DECLS

#define SYSPROF_TYPE_DOCUMENT_FORK         (sysprof_document_fork_get_type())
#define SYSPROF_IS_DOCUMENT_FORK(obj)      G_TYPE_CHECK_INSTANCE_TYPE(obj, SYSPROF_TYPE_DOCUMENT_FORK)
#define SYSPROF_DOCUMENT_FORK(obj)         G_TYPE_CHECK_INSTANCE_CAST(obj, SYSPROF_TYPE_DOCUMENT_FORK, SysprofDocumentFork)
#define SYSPROF_DOCUMENT_FORK_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, SYSPROF_TYPE_DOCUMENT_FORK, SysprofDocumentForkClass)

typedef struct _SysprofDocumentFork      SysprofDocumentFork;
typedef struct _SysprofDocumentForkClass SysprofDocumentForkClass;

SYSPROF_AVAILABLE_IN_ALL
GType sysprof_document_fork_get_type      (void) G_GNUC_CONST;
SYSPROF_AVAILABLE_IN_ALL
int   sysprof_document_fork_get_child_pid (SysprofDocumentFork *self);

G_END_DECLS

