/* sysprof-line-visualizer-row.c
 *
 * Copyright 2016-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "sysprof-line-visualizer-row"

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sysprof.h>

#include "pointcache.h"
#include "sysprof-line-visualizer-row.h"

typedef struct
{
  /*
   * Our reader as assigned by the visualizer system.
   */
  SysprofCaptureReader *reader;

  /*
   * An array of LineInfo which contains information about the counters
   * we need to render.
   */
  GArray *lines;

  /*
   * This is our set of cached points to render. Once it is assigned here,
   * it is immutable (and therefore may be shared with worker processes
   * that are rendering the points).
   */
  PointCache *cache;

  /*
   * Child widget to display the label in the upper corner.
   */
  GtkLabel *label;

  /*
   * Range of the scale for lower and upper.
   */
  gdouble y_lower;
  gdouble y_upper;

  /*
   * If we have a new counter discovered or the reader is set, we might
   * want to delay loading until we return to the main loop. This can
   * help us avoid doing duplicate work.
   */
  guint queued_load;

  guint y_lower_set : 1;
  guint y_upper_set : 1;
} SysprofLineVisualizerRowPrivate;

typedef struct
{
  guint id;
  gdouble line_width;
  GdkRGBA foreground;
  GdkRGBA background;
  guint use_default_style : 1;
  guint fill : 1;
} LineInfo;

typedef struct
{
  SysprofCaptureCursor *cursor;
  GArray *lines;
  PointCache *cache;
  gint64 begin_time;
  gint64 end_time;
  gdouble y_lower;
  gdouble y_upper;
  guint y_lower_set : 1;
  guint y_upper_set : 1;
} LoadData;

G_DEFINE_TYPE_WITH_PRIVATE (SysprofLineVisualizerRow, sysprof_line_visualizer_row, SYSPROF_TYPE_VISUALIZER_ROW)

static void       sysprof_line_visualizer_row_load_data_async   (SysprofLineVisualizerRow  *self,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
static PointCache *sysprof_line_visualizer_row_load_data_finish (SysprofLineVisualizerRow  *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);

enum {
  PROP_0,
  PROP_TITLE,
  PROP_Y_LOWER,
  PROP_Y_UPPER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
load_data_free (gpointer data)
{
  LoadData *load = data;

  if (load != NULL)
    {
      g_clear_pointer (&load->lines, g_array_unref);
      g_clear_pointer (&load->cursor, sysprof_capture_cursor_unref);
      g_clear_pointer (&load->cache, point_cache_unref);
      g_slice_free (LoadData, load);
    }
}

static GArray *
copy_array (GArray *ar)
{
  GArray *ret;

  ret = g_array_sized_new (FALSE, FALSE, g_array_get_element_size (ar), ar->len);
  g_array_set_size (ret, ar->len);
  memcpy (ret->data, ar->data, ar->len * g_array_get_element_size (ret));

  return ret;
}

static gboolean
sysprof_line_visualizer_row_draw (GtkWidget *widget,
                                  cairo_t   *cr)
{
  SysprofLineVisualizerRow *self = (SysprofLineVisualizerRow *)widget;
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);
  GtkStyleContext *style_context;
  GtkStateFlags flags;
  GtkAllocation alloc;
  GdkRGBA foreground;
  gboolean ret;

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (widget));
  g_assert (cr != NULL);

  gtk_widget_get_allocation (widget, &alloc);

  ret = GTK_WIDGET_CLASS (sysprof_line_visualizer_row_parent_class)->draw (widget, cr);

  if (priv->cache == NULL)
    return ret;

  style_context = gtk_widget_get_style_context (widget);
  flags = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_color (style_context, flags, &foreground);

  for (guint line = 0; line < priv->lines->len; line++)
    {
      g_autofree SysprofVisualizerRowAbsolutePoint *points = NULL;
      const LineInfo *line_info = &g_array_index (priv->lines, LineInfo, line);
      const Point *fpoints;
      guint n_fpoints = 0;
      GdkRGBA color;

      fpoints = point_cache_get_points (priv->cache, line_info->id, &n_fpoints);

      if (n_fpoints > 0)
        {
          gdouble last_x;
          gdouble last_y;

          points = g_new0 (SysprofVisualizerRowAbsolutePoint, n_fpoints);

          sysprof_visualizer_row_translate_points (SYSPROF_VISUALIZER_ROW (self),
                                                   (const SysprofVisualizerRowRelativePoint *)fpoints,
                                                   n_fpoints,
                                                   points,
                                                   n_fpoints);

          last_x = points[0].x;
          last_y = points[0].y;

          if (line_info->fill)
            {
              cairo_move_to (cr, last_x, alloc.height);
              cairo_line_to (cr, last_x, last_y);
            }
          else
            {
              cairo_move_to (cr, last_x, last_y);
            }

          for (guint i = 1; i < n_fpoints; i++)
            {
              cairo_curve_to (cr,
                              last_x + ((points[i].x - last_x) / 2),
                              last_y,
                              last_x + ((points[i].x - last_x) / 2),
                              points[i].y,
                              points[i].x,
                              points[i].y);
              last_x = points[i].x;
              last_y = points[i].y;
            }

          if (line_info->fill)
            {
              cairo_line_to (cr, last_x, alloc.height);
              cairo_close_path (cr);
            }

          cairo_set_line_width (cr, line_info->line_width);

          if (line_info->fill)
            {
              gdk_cairo_set_source_rgba (cr, &line_info->background);
              cairo_fill_preserve (cr);
            }

          if (line_info->use_default_style)
            color = foreground;
          else
            color = line_info->foreground;

          gdk_cairo_set_source_rgba (cr, &color);
          cairo_stroke (cr);
        }
    }

  return ret;
}

static void
sysprof_line_visualizer_row_load_data_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  SysprofLineVisualizerRow *self = (SysprofLineVisualizerRow *)object;
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);
  g_autoptr(GError) error = NULL;
  g_autoptr(PointCache) cache = NULL;

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (self));

  cache = sysprof_line_visualizer_row_load_data_finish (self, result, &error);

  if (cache == NULL)
    {
      g_warning ("%s", error->message);
      return;
    }

  g_clear_pointer (&priv->cache, point_cache_unref);
  priv->cache = g_steal_pointer (&cache);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
sysprof_line_visualizer_row_do_reload (gpointer data)
{
  SysprofLineVisualizerRow *self = data;
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (self));

  priv->queued_load = 0;

  if (priv->reader != NULL)
    {
      sysprof_line_visualizer_row_load_data_async (self,
                                              NULL,
                                              sysprof_line_visualizer_row_load_data_cb,
                                              NULL);
    }

  return G_SOURCE_REMOVE;
}

static void
sysprof_line_visualizer_row_queue_reload (SysprofLineVisualizerRow *self)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (self));

  if (priv->queued_load == 0)
    {
      priv->queued_load = gdk_threads_add_idle_full (G_PRIORITY_LOW,
                                                     sysprof_line_visualizer_row_do_reload,
                                                     self,
                                                     NULL);
    }
}

static void
sysprof_line_visualizer_row_set_reader (SysprofVisualizerRow *row,
                                        SysprofCaptureReader *reader)
{
  SysprofLineVisualizerRow *self = (SysprofLineVisualizerRow *)row;
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (self));

  if (priv->reader != reader)
    {
      if (priv->reader != NULL)
        {
          sysprof_capture_reader_unref (priv->reader);
          priv->reader = NULL;
        }

      if (reader != NULL)
        priv->reader = sysprof_capture_reader_ref (reader);

      sysprof_line_visualizer_row_queue_reload (self);
    }
}

static void
sysprof_line_visualizer_row_finalize (GObject *object)
{
  SysprofLineVisualizerRow *self = (SysprofLineVisualizerRow *)object;
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  g_clear_pointer (&priv->lines, g_array_unref);
  g_clear_pointer (&priv->cache, point_cache_unref);
  g_clear_pointer (&priv->reader, sysprof_capture_reader_unref);

  if (priv->queued_load != 0)
    {
      g_source_remove (priv->queued_load);
      priv->queued_load = 0;
    }

  G_OBJECT_CLASS (sysprof_line_visualizer_row_parent_class)->finalize (object);
}

static void
sysprof_line_visualizer_row_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  SysprofLineVisualizerRow *self = SYSPROF_LINE_VISUALIZER_ROW (object);
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_object_get_property (G_OBJECT (priv->label), "label", value);
      break;

    case PROP_Y_LOWER:
      g_value_set_double (value, priv->y_lower);
      break;

    case PROP_Y_UPPER:
      g_value_set_double (value, priv->y_upper);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_line_visualizer_row_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  SysprofLineVisualizerRow *self = SYSPROF_LINE_VISUALIZER_ROW (object);
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_object_set_property (G_OBJECT (priv->label), "label", value);
      break;

    case PROP_Y_LOWER:
      priv->y_lower = g_value_get_double (value);
      priv->y_lower_set = TRUE;
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    case PROP_Y_UPPER:
      priv->y_upper = g_value_get_double (value);
      priv->y_upper_set = TRUE;
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_line_visualizer_row_class_init (SysprofLineVisualizerRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  SysprofVisualizerRowClass *visualizer_class = SYSPROF_VISUALIZER_ROW_CLASS (klass);

  object_class->finalize = sysprof_line_visualizer_row_finalize;
  object_class->get_property = sysprof_line_visualizer_row_get_property;
  object_class->set_property = sysprof_line_visualizer_row_set_property;

  widget_class->draw = sysprof_line_visualizer_row_draw;

  visualizer_class->set_reader = sysprof_line_visualizer_row_set_reader;

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the row",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_Y_LOWER] =
    g_param_spec_double ("y-lower",
                         "Y Lower",
                         "The lowest Y value for the visualizer",
                         -G_MAXDOUBLE,
                         G_MAXDOUBLE,
                         0.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_Y_UPPER] =
    g_param_spec_double ("y-upper",
                         "Y Upper",
                         "The highest Y value for the visualizer",
                         -G_MAXDOUBLE,
                         G_MAXDOUBLE,
                         100.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
sysprof_line_visualizer_row_init (SysprofLineVisualizerRow *self)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);
  PangoAttrList *attrs = pango_attr_list_new ();

  priv->lines = g_array_new (FALSE, FALSE, sizeof (LineInfo));

  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_SMALL * PANGO_SCALE_SMALL));

  priv->label = g_object_new (GTK_TYPE_LABEL,
                              "attributes", attrs,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              "yalign", 0.0f,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (priv->label));

  pango_attr_list_unref (attrs);
}

void
sysprof_line_visualizer_row_add_counter (SysprofLineVisualizerRow *self,
                                         guint                     counter_id,
                                         const GdkRGBA            *color)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);
  LineInfo line_info = { 0 };

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (self));
  g_assert (priv->lines != NULL);

  line_info.id = counter_id;
  line_info.line_width = 1.0;

  if (color != NULL)
    {
      line_info.foreground = *color;
      line_info.use_default_style = FALSE;
    }
  else
    {
      gdk_rgba_parse (&line_info.foreground, "#000");
      line_info.use_default_style = TRUE;
    }

  g_array_append_val (priv->lines, line_info);

  if (SYSPROF_LINE_VISUALIZER_ROW_GET_CLASS (self)->counter_added)
    SYSPROF_LINE_VISUALIZER_ROW_GET_CLASS (self)->counter_added (self, counter_id);

  sysprof_line_visualizer_row_queue_reload (self);
}

void
sysprof_line_visualizer_row_clear (SysprofLineVisualizerRow *self)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  g_return_if_fail (SYSPROF_IS_LINE_VISUALIZER_ROW (self));

  if (priv->lines->len > 0)
    g_array_remove_range (priv->lines, 0, priv->lines->len);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static inline gboolean
contains_id (GArray *ar,
             guint   id)
{
  for (guint i = 0; i < ar->len; i++)
    {
      const LineInfo *info = &g_array_index (ar, LineInfo, i);
      if (info->id == id)
        return TRUE;
    }

  return FALSE;
}

static inline guint8
counter_type (LoadData *load,
              guint     counter_id)
{
  /* TODO: Support other counter types
   *
   * We need to keep some information on the counter (by id) so that we
   * can track the counters type (which is a 1-byte type id).
   */
  return SYSPROF_CAPTURE_COUNTER_DOUBLE;
}

static inline gdouble
calc_x (gint64 lower,
        gint64 upper,
        gint64 value)
{
  return (gdouble)(value - lower) / (gdouble)(upper - lower);
}

static inline gdouble
calc_y_double (gdouble lower,
               gdouble upper,
               gdouble value)
{
  return (value - lower) / (upper - lower);
}

static inline gdouble
calc_y_int64 (gint64 lower,
              gint64 upper,
              gint64 value)
{
  return (gdouble)(value - lower) / (gdouble)(upper - lower);
}

static gboolean
sysprof_line_visualizer_row_load_data_frame_cb (const SysprofCaptureFrame *frame,
                                                gpointer                   user_data)
{
  LoadData *load = user_data;

  g_assert (frame != NULL);
  g_assert (frame->type == SYSPROF_CAPTURE_FRAME_CTRSET ||
            frame->type == SYSPROF_CAPTURE_FRAME_CTRDEF);
  g_assert (load != NULL);

  if (frame->type == SYSPROF_CAPTURE_FRAME_CTRSET)
    {
      const SysprofCaptureFrameCounterSet *set = (SysprofCaptureFrameCounterSet *)frame;
      gdouble x = calc_x (load->begin_time, load->end_time, frame->time);

      for (guint i = 0; i < set->n_values; i++)
        {
          const SysprofCaptureCounterValues *group = &set->values[i];

          for (guint j = 0; j < G_N_ELEMENTS (group->ids); j++)
            {
              guint counter_id = group->ids[j];

              if (counter_id != 0 && contains_id (load->lines, counter_id))
                {
                  gdouble y;

                  if (counter_type (load, counter_id) == SYSPROF_CAPTURE_COUNTER_DOUBLE)
                    y = calc_y_double (load->y_lower, load->y_upper, group->values[j].vdbl);
                  else
                    y = calc_y_int64 (load->y_lower, load->y_upper, group->values[j].v64);

                  point_cache_add_point_to_set (load->cache, counter_id, x, y);
                }
            }
        }
    }

  return TRUE;
}

static gboolean
sysprof_line_visualizer_row_load_data_range_cb (const SysprofCaptureFrame *frame,
                                                gpointer                   user_data)
{
  LoadData *load = user_data;

  g_assert (frame != NULL);
  g_assert (frame->type == SYSPROF_CAPTURE_FRAME_CTRSET ||
            frame->type == SYSPROF_CAPTURE_FRAME_CTRDEF);
  g_assert (load != NULL);
  g_assert (load->y_upper_set == FALSE ||
            load->y_lower_set == FALSE);

  if (frame->type == SYSPROF_CAPTURE_FRAME_CTRSET)
    {
      const SysprofCaptureFrameCounterSet *set = (SysprofCaptureFrameCounterSet *)frame;

      for (guint i = 0; i < set->n_values; i++)
        {
          const SysprofCaptureCounterValues *group = &set->values[i];

          for (guint j = 0; j < G_N_ELEMENTS (group->ids); j++)
            {
              guint counter_id = group->ids[j];

              if (counter_id != 0 && contains_id (load->lines, counter_id))
                {
                  gdouble y;

                  if (counter_type (load, counter_id) == SYSPROF_CAPTURE_COUNTER_DOUBLE)
                    y = group->values[j].vdbl;
                  else
                    y = group->values[j].v64;

                  if (!load->y_upper_set)
                    load->y_upper = MAX (load->y_upper, y);

                  if (!load->y_lower_set)
                    load->y_lower = MIN (load->y_lower, y);
                }
            }
        }
    }

  return TRUE;
}

static void
sysprof_line_visualizer_row_load_data_worker (GTask        *task,
                                              gpointer      source_object,
                                              gpointer      task_data,
                                              GCancellable *cancellable)
{
  LoadData *load = task_data;
  g_autoptr(GArray) counter_ids = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  counter_ids = g_array_new (FALSE, FALSE, sizeof (guint));

  for (guint i = 0; i < load->lines->len; i++)
    {
      const LineInfo *line_info = &g_array_index (load->lines, LineInfo, i);
      g_array_append_val (counter_ids, line_info->id);
    }

  sysprof_capture_cursor_add_condition (load->cursor,
                                        sysprof_capture_condition_new_where_counter_in (counter_ids->len,
                                                                                        (guint *)(gpointer)counter_ids->data));

  /* If y boundaries are not set, we need to discover them by scaning the data. */
  if (!load->y_lower_set || !load->y_upper_set)
    {
      sysprof_capture_cursor_foreach (load->cursor, sysprof_line_visualizer_row_load_data_range_cb, load);
      sysprof_capture_cursor_reset (load->cursor);

      /* Add extra boundary for some space above the graph line */
      if (G_MAXDOUBLE - load->y_upper > (load->y_upper * .25))
        load->y_upper *= 1.25;
    }

  sysprof_capture_cursor_foreach (load->cursor, sysprof_line_visualizer_row_load_data_frame_cb, load);
  g_task_return_pointer (task, g_steal_pointer (&load->cache), (GDestroyNotify)point_cache_unref);
}

static void
sysprof_line_visualizer_row_load_data_async (SysprofLineVisualizerRow *self,
                                             GCancellable             *cancellable,
                                             GAsyncReadyCallback       callback,
                                             gpointer                  user_data)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  LoadData *load;

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, sysprof_line_visualizer_row_load_data_async);

  if (priv->reader == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "No data loaded");
      return;
    }

  load = g_slice_new0 (LoadData);
  load->cache = point_cache_new ();
  load->y_lower = priv->y_lower;
  load->y_upper = priv->y_upper;
  load->y_lower_set = priv->y_lower_set;
  load->y_upper_set = priv->y_upper_set;
  load->begin_time = sysprof_capture_reader_get_start_time (priv->reader);
  load->end_time = sysprof_capture_reader_get_end_time (priv->reader);
  load->cursor = sysprof_capture_cursor_new (priv->reader);
  load->lines = copy_array (priv->lines);

  for (guint i = 0; i < load->lines->len; i++)
    {
      const LineInfo *line_info = &g_array_index (load->lines, LineInfo, i);

      point_cache_add_set (load->cache, line_info->id);
    }

  g_task_set_task_data  (task, load, load_data_free);
  g_task_run_in_thread (task, sysprof_line_visualizer_row_load_data_worker);
}

static PointCache *
sysprof_line_visualizer_row_load_data_finish (SysprofLineVisualizerRow  *self,
                                              GAsyncResult              *result,
                                              GError                   **error)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);
  LoadData *state;

  g_assert (SYSPROF_IS_LINE_VISUALIZER_ROW (self));
  g_assert (G_IS_TASK (result));

  state = g_task_get_task_data (G_TASK (result));

  if (!priv->y_lower_set && priv->y_lower != state->y_lower)
    {
      priv->y_lower = state->y_lower;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_Y_LOWER]);
    }

  if (!priv->y_upper_set && priv->y_upper != state->y_upper)
    {
      priv->y_upper = state->y_upper;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_Y_UPPER]);
    }

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
sysprof_line_visualizer_row_set_line_width (SysprofLineVisualizerRow *self,
                                            guint                     counter_id,
                                            gdouble                   width)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  g_return_if_fail (SYSPROF_IS_LINE_VISUALIZER_ROW (self));

  for (guint i = 0; i < priv->lines->len; i++)
    {
      LineInfo *info = &g_array_index (priv->lines, LineInfo, i);

      if (info->id == counter_id)
        {
          info->line_width = width;
          sysprof_line_visualizer_row_queue_reload (self);
          break;
        }
    }
}

void
sysprof_line_visualizer_row_set_fill (SysprofLineVisualizerRow *self,
                                      guint                     counter_id,
                                      const GdkRGBA            *color)
{
  SysprofLineVisualizerRowPrivate *priv = sysprof_line_visualizer_row_get_instance_private (self);

  g_return_if_fail (SYSPROF_IS_LINE_VISUALIZER_ROW (self));

  for (guint i = 0; i < priv->lines->len; i++)
    {
      LineInfo *info = &g_array_index (priv->lines, LineInfo, i);

      if (info->id == counter_id)
        {
          info->fill = !!color;
          if (color != NULL)
            info->background = *color;
          sysprof_line_visualizer_row_queue_reload (self);
          break;
        }
    }
}
