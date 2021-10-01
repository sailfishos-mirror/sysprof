/* sysprof-window.c
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

#define G_LOG_DOMAIN "sysprof-window"

#include "config.h"

#include <glib/gi18n.h>
#include <sysprof-ui.h>

#include "egg-binding-group.h"

#include "sysprof-window.h"

struct _SysprofWindow
{
  AdwApplicationWindow  parent_instance;

  EggBindingGroup      *bindings;

  SysprofNotebook      *notebook;
  GtkButton            *open_button;
  GtkMenuButton        *menu_button;
};

G_DEFINE_TYPE (SysprofWindow, sysprof_window, ADW_TYPE_APPLICATION_WINDOW)

/**
 * sysprof_window_new:
 *
 * Create a new #SysprofWindow.
 *
 * Returns: (transfer full): a newly created #SysprofWindow
 */
GtkWidget *
sysprof_window_new (SysprofApplication *application)
{
  return g_object_new (SYSPROF_TYPE_WINDOW,
                       "application", application,
                       NULL);
}

static void
sysprof_window_notify_can_replay_cb (SysprofWindow   *self,
                                     GParamSpec      *pspec,
                                     SysprofNotebook *notebook)
{
  g_assert (SYSPROF_IS_WINDOW (self));
  g_assert (SYSPROF_IS_NOTEBOOK (notebook));

#if 0
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "replay-capture",
                             "enabled", sysprof_notebook_get_can_replay (notebook),
                             NULL);
#endif
}

static void
sysprof_window_notify_can_save_cb (SysprofWindow   *self,
                                   GParamSpec      *pspec,
                                   SysprofNotebook *notebook)
{
  g_assert (SYSPROF_IS_WINDOW (self));
  g_assert (SYSPROF_IS_NOTEBOOK (notebook));

#if 0
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "save-capture",
                             "enabled", sysprof_notebook_get_can_save (notebook),
                             NULL);
#endif
}

static void
new_tab_cb (GSimpleAction *action,
            GVariant      *param,
            gpointer       user_data)
{
  SysprofWindow *self = user_data;

  g_return_if_fail (SYSPROF_IS_WINDOW (self));

  sysprof_window_new_tab (self);
}

static void
switch_tab_cb (GSimpleAction *action,
               GVariant      *param,
               gpointer       user_data)
{
  SysprofWindow *self = user_data;
  gint page;

  g_return_if_fail (SYSPROF_IS_WINDOW (self));
  g_return_if_fail (g_variant_is_of_type (param, G_VARIANT_TYPE_INT32));

  page = g_variant_get_int32 (param);
  sysprof_notebook_set_current_page (self->notebook, page - 1);
}

static void
close_tab_cb (GSimpleAction *action,
              GVariant      *param,
              gpointer       user_data)
{
  SysprofWindow *self = user_data;

  g_return_if_fail (SYSPROF_IS_WINDOW (self));

  if (sysprof_notebook_get_n_pages (self->notebook) == 1)
    {
      SysprofDisplay *child = sysprof_notebook_get_nth_page (self->notebook, 0);

      if (SYSPROF_IS_DISPLAY (child) &&
          sysprof_display_is_empty (SYSPROF_DISPLAY (child)))
        {
          gtk_window_destroy (GTK_WINDOW (self));
          return;
        }
    }

  sysprof_notebook_close_current (self->notebook);
}

static void
replay_capture_cb (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  SysprofWindow *self = user_data;
  g_return_if_fail (SYSPROF_IS_WINDOW (self));
  sysprof_notebook_replay (self->notebook);
}

static void
save_capture_cb (GSimpleAction *action,
                 GVariant      *param,
                 gpointer       user_data)
{
  SysprofWindow *self = user_data;
  g_return_if_fail (SYSPROF_IS_WINDOW (self));
  sysprof_notebook_save (self->notebook);
}

static void
stop_recording_cb (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  SysprofWindow *self = user_data;
  SysprofDisplay *current;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (SYSPROF_IS_WINDOW (self));

  if ((current = sysprof_notebook_get_current (self->notebook)))
    sysprof_display_stop_recording (current);
}

static void
sysprof_window_finalize (GObject *object)
{
  SysprofWindow *self = (SysprofWindow *)object;

  egg_binding_group_set_source (self->bindings, NULL);
  g_clear_object (&self->bindings);

  G_OBJECT_CLASS (sysprof_window_parent_class)->finalize (object);
}

static void
sysprof_window_class_init (SysprofWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = sysprof_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/sysprof/ui/sysprof-window.ui");
  gtk_widget_class_bind_template_child (widget_class, SysprofWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, SysprofWindow, open_button);
  gtk_widget_class_bind_template_child (widget_class, SysprofWindow, notebook);

#if 0
  /* Switch to GtkShortcutController class bindings */
  DzlShortcutController *controller;
  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));
  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.sysprof3.stop-recording",
                                              "Escape",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "win.stop-recording");
#endif

  g_type_ensure (SYSPROF_TYPE_NOTEBOOK);
  g_type_ensure (SYSPROF_TYPE_DISPLAY);
}

static void
sysprof_window_init (SysprofWindow *self)
{
  static GActionEntry actions[] = {
    { "close-tab", close_tab_cb },
    { "new-tab", new_tab_cb },
    { "switch-tab", switch_tab_cb, "i" },
    { "replay-capture", replay_capture_cb },
    { "save-capture", save_capture_cb },
    { "stop-recording", stop_recording_cb },
  };
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (g_application_get_default ()), "win-menu");
  gtk_menu_button_set_menu_model (self->menu_button, G_MENU_MODEL (menu));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  g_signal_connect_object (self->notebook,
                           "notify::can-replay",
                           G_CALLBACK (sysprof_window_notify_can_replay_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->notebook,
                           "notify::can-save",
                           G_CALLBACK (sysprof_window_notify_can_save_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->bindings = egg_binding_group_new ();
  egg_binding_group_bind (self->bindings, "title", self, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->notebook, "current", self->bindings, "source",
                          G_BINDING_SYNC_CREATE);

#if 0
  /* Switch to using gtk_widget_action_set_enabled() */
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "save-capture",
                             "enabled", FALSE,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "replay-capture",
                             "enabled", FALSE,
                             NULL);
#endif
}

void
sysprof_window_open (SysprofWindow *self,
                     GFile         *file)
{
  g_return_if_fail (SYSPROF_IS_WINDOW (self));
  g_return_if_fail (G_IS_FILE (file));

  sysprof_notebook_open (self->notebook, file);
}

static void
sysprof_window_open_from_dialog_cb (SysprofWindow        *self,
                                    int                   response,
                                    GtkFileChooserNative *dialog)
{
  g_assert (SYSPROF_IS_WINDOW (self));
  g_assert (GTK_IS_FILE_CHOOSER_NATIVE (dialog));

  if (response == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      if (g_file_is_native (file))
        sysprof_window_open (self, file);
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));
}

void
sysprof_window_open_from_dialog (SysprofWindow *self)
{
  GtkFileChooserNative *dialog;
  GtkFileFilter *filter;

  g_return_if_fail (SYSPROF_IS_WINDOW (self));

  /* Translators: This is a window title. */
  dialog = gtk_file_chooser_native_new (_("Open Capture…"),
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        /* Translators: This is a button. */
                                        _("Open"),
                                        /* Translators: This is a button. */
                                        _("Cancel"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Sysprof Captures"));
  gtk_file_filter_add_pattern (filter, "*.syscap");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (sysprof_window_open_from_dialog_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

void
sysprof_window_new_tab (SysprofWindow *self)
{
  GtkWidget *display;
  gint page;

  g_return_if_fail (SYSPROF_IS_WINDOW (self));

  display = sysprof_display_new ();
  page = sysprof_notebook_append (self->notebook, SYSPROF_DISPLAY (display));
  sysprof_notebook_set_current_page (self->notebook, page);
}
