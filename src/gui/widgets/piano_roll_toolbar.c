/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "actions/undo_manager.h"
#include "actions/undoable_action.h"
#include "gui/accel.h"
#include "gui/backend/events.h"
#include "gui/widgets/piano_roll_toolbar.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/snap_grid.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/gtk.h"
#include "utils/resources.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (PianoRollToolbarWidget,
               piano_roll_toolbar_widget,
               GTK_TYPE_TOOLBAR)

static void
on_highlighting_changed (
  GtkComboBox *widget,
  PianoRollToolbarWidget * self)
{
  piano_roll_set_highlighting (
    PIANO_ROLL,
    gtk_combo_box_get_active (widget));
}

void
piano_roll_toolbar_widget_setup (
  PianoRollToolbarWidget * self)
{
  /* setup bot toolbar */
  snap_grid_widget_setup (
    self->snap_grid_midi,
    &PROJECT->snap_grid_midi);

  /* setup highlighting */
  gtk_combo_box_set_active (
    GTK_COMBO_BOX (
      self->chord_highlighting),
    PIANO_ROLL->highlighting);

  /* setup signals */
  g_signal_connect (
    G_OBJECT(self->chord_highlighting),
    "changed",
    G_CALLBACK (on_highlighting_changed),  self);
}

static void
piano_roll_toolbar_widget_init (
  PianoRollToolbarWidget * self)
{
  g_type_ensure (SNAP_GRID_WIDGET_TYPE);

  gtk_widget_init_template (GTK_WIDGET (self));

#define SET_TOOLTIP(x, tooltip) \
  z_gtk_set_tooltip_for_actionable ( \
    GTK_ACTIONABLE (self->x), \
    _(tooltip))
  /*SET_TOOLTIP (loop_selection, "Loop Selection");*/
#undef SET_TOOLTIP
}

static void
piano_roll_toolbar_widget_class_init (PianoRollToolbarWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "piano_roll_toolbar.ui");

  gtk_widget_class_set_css_name (
    klass, "piano-roll-toolbar");

  gtk_widget_class_bind_template_child (
    klass,
    PianoRollToolbarWidget,
    chord_highlighting);
  gtk_widget_class_bind_template_child (
    klass,
    PianoRollToolbarWidget,
    snap_grid_midi);
}
