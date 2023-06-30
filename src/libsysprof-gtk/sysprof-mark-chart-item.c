/* sysprof-mark-chart-item.c
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

#include "sysprof-mark-chart-item-private.h"

struct _SysprofMarkChartItem
{
  GObject             parent_instance;
  SysprofSession     *session;
  SysprofMarkCatalog *catalog;
  GtkFilterListModel *filtered;
  SysprofSeries      *series;
};

enum {
  PROP_0,
  PROP_SESSION,
  PROP_CATALOG,
  PROP_SERIES,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (SysprofMarkChartItem, sysprof_mark_chart_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
sysprof_mark_chart_item_constructed (GObject *object)
{
  SysprofMarkChartItem *self = (SysprofMarkChartItem *)object;

  G_OBJECT_CLASS (sysprof_mark_chart_item_parent_class)->constructed (object);

  if (self->catalog == NULL || self->session == NULL)
    g_return_if_reached ();

  g_object_set (self->filtered,
                "model", G_LIST_MODEL (self->catalog),
                "filter", sysprof_session_get_filter (self->session),
                NULL);
}

static void
sysprof_mark_chart_item_dispose (GObject *object)
{
  SysprofMarkChartItem *self = (SysprofMarkChartItem *)object;

  g_clear_object (&self->session);
  g_clear_object (&self->catalog);
  g_clear_object (&self->filtered);
  g_clear_object (&self->series);

  G_OBJECT_CLASS (sysprof_mark_chart_item_parent_class)->dispose (object);
}

static void
sysprof_mark_chart_item_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  SysprofMarkChartItem *self = SYSPROF_MARK_CHART_ITEM (object);

  switch (prop_id)
    {
    case PROP_CATALOG:
      g_value_set_object (value, sysprof_mark_chart_item_get_catalog (self));
      break;

    case PROP_SESSION:
      g_value_set_object (value, sysprof_mark_chart_item_get_session (self));
      break;

    case PROP_SERIES:
      g_value_set_object (value, sysprof_mark_chart_item_get_series (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_mark_chart_item_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  SysprofMarkChartItem *self = SYSPROF_MARK_CHART_ITEM (object);

  switch (prop_id)
    {
    case PROP_CATALOG:
      self->catalog = g_value_dup_object (value);
      break;

    case PROP_SESSION:
      self->session = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_mark_chart_item_class_init (SysprofMarkChartItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = sysprof_mark_chart_item_constructed;
  object_class->dispose = sysprof_mark_chart_item_dispose;
  object_class->get_property = sysprof_mark_chart_item_get_property;
  object_class->set_property = sysprof_mark_chart_item_set_property;

  properties[PROP_CATALOG] =
    g_param_spec_object ("catalog", NULL, NULL,
                         SYSPROF_TYPE_MARK_CATALOG,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SESSION] =
    g_param_spec_object ("session", NULL, NULL,
                         SYSPROF_TYPE_SESSION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SERIES] =
    g_param_spec_object ("series", NULL, NULL,
                         SYSPROF_TYPE_TIME_SERIES,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
sysprof_mark_chart_item_init (SysprofMarkChartItem *self)
{
  self->filtered = gtk_filter_list_model_new (NULL, NULL);
  gtk_filter_list_model_set_incremental (self->filtered, TRUE);

  self->series = sysprof_time_series_new (NULL,
                                          g_object_ref (G_LIST_MODEL (self->filtered)),
                                          gtk_property_expression_new (SYSPROF_TYPE_DOCUMENT_MARK, NULL, "time"),
                                          gtk_property_expression_new (SYSPROF_TYPE_DOCUMENT_MARK, NULL, "duration"),
                                          gtk_property_expression_new (SYSPROF_TYPE_DOCUMENT_MARK, NULL, "message"));
}

SysprofMarkChartItem *
sysprof_mark_chart_item_new (SysprofSession     *session,
                             SysprofMarkCatalog *catalog)
{
  g_return_val_if_fail (SYSPROF_IS_SESSION (session), NULL);
  g_return_val_if_fail (SYSPROF_IS_MARK_CATALOG (catalog), NULL);

  return g_object_new (SYSPROF_TYPE_MARK_CHART_ITEM,
                       "session", session,
                       "catalog", catalog,
                       NULL);
}

SysprofMarkCatalog *
sysprof_mark_chart_item_get_catalog (SysprofMarkChartItem *self)
{
  return self->catalog;
}

SysprofSession *
sysprof_mark_chart_item_get_session (SysprofMarkChartItem *self)
{
  return self->session;
}

SysprofTimeSeries *
sysprof_mark_chart_item_get_series (SysprofMarkChartItem *self)
{
  g_return_val_if_fail (SYSPROF_IS_MARK_CHART_ITEM (self), NULL);

  return SYSPROF_TIME_SERIES (self->series);
}
