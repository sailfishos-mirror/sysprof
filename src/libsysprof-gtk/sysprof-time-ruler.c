/* sysprof-time-ruler.c
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

#include "sysprof-time-ruler.h"

#define GROUP_SIZE 100

struct _SysprofTimeRuler
{
  GtkWidget       parent_instance;
  SysprofSession *session;
  GSignalGroup   *session_signals;
  gint64          tick_interval;
};

enum {
  PROP_0,
  PROP_SESSION,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (SysprofTimeRuler, sysprof_time_ruler, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
sysprof_time_ruler_update (SysprofTimeRuler *self)
{


  g_assert (SYSPROF_IS_TIME_RULER (self));
  g_assert (!self->session || SYSPROF_IS_SESSION (self->session));

  if (self->session == NULL)
    {
      self->tick_interval = 0;
      return;
    }
}

static void
sysprof_time_ruler_notify_selected_time_cb (SysprofTimeRuler *self,
                                            GParamSpec       *pspec,
                                            SysprofSession   *session)
{
  g_assert (SYSPROF_IS_TIME_RULER (self));
  g_assert (SYSPROF_IS_SESSION (session));

  sysprof_time_ruler_update (self);
}

static void
sysprof_time_ruler_notify_visible_time_cb (SysprofTimeRuler *self,
                                           GParamSpec       *pspec,
                                           SysprofSession   *session)
{
  g_assert (SYSPROF_IS_TIME_RULER (self));
  g_assert (SYSPROF_IS_SESSION (session));

  sysprof_time_ruler_update (self);
}

static void
sysprof_time_ruler_bind_session_cb (SysprofTimeRuler *self,
                                    SysprofSession   *session,
                                    GSignalGroup     *signals)
{
  g_assert (SYSPROF_IS_TIME_RULER (self));
  g_assert (SYSPROF_IS_SESSION (session));
  g_assert (G_IS_SIGNAL_GROUP (signals));

  sysprof_time_ruler_update (self);
}

static void
sysprof_time_ruler_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  SysprofTimeRuler *self = (SysprofTimeRuler *)widget;
  const SysprofTimeSpan *visible_time;
  PangoLayout *layout;
  gint64 tick_interval;
  gint64 time_range;
  guint n_groups;
  int width;
  int height;

  g_assert (SYSPROF_IS_TIME_RULER (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));
  g_assert (!self->session || SYSPROF_IS_SESSION (self->session));

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (self->session == NULL || width == 0 || height == 0)
    return;

  visible_time = sysprof_session_get_visible_time (self->session);
  n_groups = MAX (width / GROUP_SIZE, 1);
  time_range = visible_time->end_nsec - visible_time->begin_nsec;
  tick_interval = time_range / n_groups;

  layout = gtk_widget_create_pango_layout (widget, NULL);

  for (gint64 t = visible_time->begin_nsec - (visible_time->begin_nsec % tick_interval);
       t < visible_time->end_nsec;
       t += tick_interval)
    {
      static const GdkRGBA black = {0,0,0,1};
      gint64 o = t - visible_time->begin_nsec;
      double x = (o / (double)time_range) * width;
      int pw, ph;
      char str[32];

      if (x < 0)
        continue;

      if (o == 0)
        g_snprintf (str, sizeof str, "%.3lfs", .0);
      else if (o < 1000000)
        g_snprintf (str, sizeof str, "%.3lfμs", o/1000.);
      else if (o < SYSPROF_NSEC_PER_SEC)
        g_snprintf (str, sizeof str, "%.3lfms", o/1000000.);
      else
        g_snprintf (str, sizeof str, "%.3lfs", o/(double)SYSPROF_NSEC_PER_SEC);

      pango_layout_set_text (layout, str, -1);
      pango_layout_get_pixel_size (layout, &pw, &ph);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x-pw-6, (height-ph)/2));
      gtk_snapshot_append_layout (snapshot, layout, &black);
      gtk_snapshot_restore (snapshot);

      gtk_snapshot_append_color (snapshot,
                                 &black,
                                 &GRAPHENE_RECT_INIT (x, 0, 1, height));
    }

  g_object_unref (layout);
}

static void
sysprof_time_ruler_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  GTK_WIDGET_CLASS (sysprof_time_ruler_parent_class)->measure (widget,
                                                               orientation,
                                                               for_size,
                                                               minimum,
                                                               natural,
                                                               minimum_baseline,
                                                               natural_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = MAX (*minimum, GROUP_SIZE);
      *natural = MAX (*natural, *minimum);
    }
}

static void
sysprof_time_ruler_dispose (GObject *object)
{
  SysprofTimeRuler *self = (SysprofTimeRuler *)object;

  g_signal_group_set_target (self->session_signals, NULL);
  g_clear_object (&self->session);

  G_OBJECT_CLASS (sysprof_time_ruler_parent_class)->dispose (object);
}

static void
sysprof_time_ruler_finalize (GObject *object)
{
  SysprofTimeRuler *self = (SysprofTimeRuler *)object;

  g_clear_object (&self->session_signals);

  G_OBJECT_CLASS (sysprof_time_ruler_parent_class)->finalize (object);
}

static void
sysprof_time_ruler_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  SysprofTimeRuler *self = SYSPROF_TIME_RULER (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      g_value_set_object (value, sysprof_time_ruler_get_session (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_time_ruler_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  SysprofTimeRuler *self = SYSPROF_TIME_RULER (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      sysprof_time_ruler_set_session (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_time_ruler_class_init (SysprofTimeRulerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = sysprof_time_ruler_dispose;
  object_class->finalize = sysprof_time_ruler_finalize;
  object_class->get_property = sysprof_time_ruler_get_property;
  object_class->set_property = sysprof_time_ruler_set_property;

  widget_class->snapshot = sysprof_time_ruler_snapshot;
  widget_class->measure = sysprof_time_ruler_measure;

  properties [PROP_SESSION] =
    g_param_spec_object ("session", NULL, NULL,
                         SYSPROF_TYPE_SESSION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "timeruler");
}

static void
sysprof_time_ruler_init (SysprofTimeRuler *self)
{
  self->session_signals = g_signal_group_new (SYSPROF_TYPE_SESSION);
  g_signal_group_connect_object (self->session_signals,
                                 "notify::selected-time",
                                 G_CALLBACK (sysprof_time_ruler_notify_selected_time_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->session_signals,
                                 "notify::visible-time",
                                 G_CALLBACK (sysprof_time_ruler_notify_visible_time_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_connect_object (self->session_signals,
                           "bind",
                           G_CALLBACK (sysprof_time_ruler_bind_session_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
sysprof_time_ruler_new (void)
{
  return g_object_new (SYSPROF_TYPE_TIME_RULER, NULL);
}

/**
 * sysprof_time_ruler_get_session:
 * @self: a #SysprofTimeRuler
 *
 * Returns: (transfer none) (nullable): a #SysprofSession or %NULL
 */
SysprofSession *
sysprof_time_ruler_get_session (SysprofTimeRuler *self)
{
  g_return_val_if_fail (SYSPROF_IS_TIME_RULER (self), NULL);

  return self->session;
}

void
sysprof_time_ruler_set_session (SysprofTimeRuler *self,
                                SysprofSession   *session)
{
  g_return_if_fail (SYSPROF_IS_TIME_RULER (self));

  if (g_set_object (&self->session, session))
    {
      g_signal_group_set_target (self->session_signals, session);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SESSION]);
    }
}
