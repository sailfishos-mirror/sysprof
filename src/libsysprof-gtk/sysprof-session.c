/* sysprof-session.c
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

#include "sysprof-session.h"

struct _SysprofSession
{
  GObject          parent_instance;
  SysprofDocument *document;
  GtkCustomFilter *filter;
};

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_FILTER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (SysprofSession, sysprof_session, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
sysprof_session_dispose (GObject *object)
{
  SysprofSession *self = (SysprofSession *)object;

  g_clear_object (&self->document);
  g_clear_object (&self->filter);

  G_OBJECT_CLASS (sysprof_session_parent_class)->dispose (object);
}

static void
sysprof_session_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  SysprofSession *self = SYSPROF_SESSION (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, sysprof_session_get_document (self));
      break;

    case PROP_FILTER:
      g_value_set_object (value, sysprof_session_get_filter (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_session_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  SysprofSession *self = SYSPROF_SESSION (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      self->document = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_session_class_init (SysprofSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = sysprof_session_dispose;
  object_class->get_property = sysprof_session_get_property;
  object_class->set_property = sysprof_session_set_property;

  properties [PROP_DOCUMENT] =
    g_param_spec_object ("document", NULL, NULL,
                         SYSPROF_TYPE_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILTER] =
    g_param_spec_object ("filter", NULL, NULL,
                         GTK_TYPE_FILTER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
sysprof_session_init (SysprofSession *self)
{
}

SysprofSession *
sysprof_session_new (SysprofDocument *document)
{
  g_return_val_if_fail (SYSPROF_IS_DOCUMENT (document), NULL);

  return g_object_new (SYSPROF_TYPE_SESSION,
                       "document", document,
                       NULL);
}

/**
 * sysprof_session_get_document:
 * @self: a #SysprofSession
 *
 * Returns: (transfer none): a #SysprofDocument
 */
SysprofDocument *
sysprof_session_get_document (SysprofSession *self)
{
  g_return_val_if_fail (SYSPROF_IS_SESSION (self), NULL);

  return self->document;
}

/**
 * sysprof_session_get_filter:
 * @self: a #SysprofSession
 *
 * Gets the filter for the session which can remove #SysprofDocumentFrame
 * which are not matching the current selection filters.
 *
 * Returns: (transfer none) (nullable): a #GtkFilter
 */
GtkFilter *
sysprof_session_get_filter (SysprofSession *self)
{
  g_return_val_if_fail (SYSPROF_IS_SESSION (self), NULL);

  return GTK_FILTER (self->filter);
}
