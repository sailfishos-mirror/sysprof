/* sysprof-tracepoints.c
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

#include "config.h"

#include "sysprof-instrument-private.h"
#include "sysprof-tracepoints.h"

typedef struct _TracepointInfo
{
  char *path;
  char **fields;
} TracepointInfo;

struct _SysprofTracepoints
{
  SysprofInstrument parent_instance;
  GArray *infos;
};

struct _SysprofTracepointsClass
{
  SysprofInstrumentClass parent_class;
};

G_DEFINE_FINAL_TYPE (SysprofTracepoints, sysprof_tracepoints, SYSPROF_TYPE_INSTRUMENT)

static void
tracepoint_info_clear (gpointer data)
{
  TracepointInfo *info = data;

  g_clear_pointer (&info->path, g_free);
  g_clear_pointer (&info->fields, g_strfreev);
}

static char **
sysprof_tracepoints_list_required_policy (SysprofInstrument *instrument)
{
  static const char *policy[] = {"org.gnome.sysprof3.profile", NULL};

  return g_strdupv ((char **)policy);
}

typedef struct _Prepare
{
  SysprofRecording *recording;
  SysprofTracepoints *tracepoints;
} Prepare;

static void
prepare_free (Prepare *prepare)
{
  g_clear_object (&prepare->recording);
  g_clear_object (&prepare->tracepoints);
  g_free (prepare);
}

static DexFuture *
sysprof_tracepoints_prepare_fiber (gpointer user_data)
{
  Prepare *prepare = user_data;

  g_assert (prepare != NULL);
  g_assert (SYSPROF_IS_RECORDING (prepare->recording));
  g_assert (SYSPROF_IS_TRACEPOINTS (prepare->tracepoints));

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
sysprof_tracepoints_prepare (SysprofInstrument *instrument,
                             SysprofRecording  *recording)
{
  SysprofTracepoints *self = (SysprofTracepoints *)instrument;
  Prepare *prepare;

  g_assert (SYSPROF_IS_TRACEPOINTS (self));
  g_assert (SYSPROF_IS_RECORDING (recording));

  prepare = g_new0 (Prepare, 1);
  prepare->recording = g_object_ref (recording);
  prepare->tracepoints = g_object_ref (self);

  return dex_scheduler_spawn (NULL, 0,
                              sysprof_tracepoints_prepare_fiber,
                              prepare,
                              (GDestroyNotify)prepare_free);
}

static void
sysprof_tracepoints_finalize (GObject *object)
{
  SysprofTracepoints *self = (SysprofTracepoints *)object;

  g_clear_pointer (&self->infos, g_array_unref);

  G_OBJECT_CLASS (sysprof_tracepoints_parent_class)->finalize (object);
}

static void
sysprof_tracepoints_class_init (SysprofTracepointsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SysprofInstrumentClass *instrument_class = SYSPROF_INSTRUMENT_CLASS (klass);

  object_class->finalize = sysprof_tracepoints_finalize;

  instrument_class->list_required_policy = sysprof_tracepoints_list_required_policy;
  instrument_class->prepare = sysprof_tracepoints_prepare;
}

static void
sysprof_tracepoints_init (SysprofTracepoints *self)
{
  self->infos = g_array_new (FALSE, FALSE, sizeof (TracepointInfo));
  g_array_set_clear_func (self->infos, tracepoint_info_clear);
}

SysprofInstrument *
sysprof_tracepoints_new (void)
{
  return g_object_new (SYSPROF_TYPE_TRACEPOINTS, NULL);
}

void
sysprof_tracepoints_add (SysprofTracepoints *self,
                         const char         *tracepoint,
                         const char * const *fields)
{
  TracepointInfo info;

  g_return_if_fail (SYSPROF_IS_TRACEPOINTS (self));
  g_return_if_fail (tracepoint != NULL);
  g_return_if_fail (fields != NULL);

  info.path = g_strdup (tracepoint);
  info.fields = g_strdupv ((char **)fields);

  g_array_append_val (self->infos, info);
}
