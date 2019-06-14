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

/**
 * \file
 *
 * PianoRoll selection info.
 */

#ifndef __GUI_WIDGETS_PIANO_ROLL_SELECTION_INFO_H__
#define __GUI_WIDGETS_PIANO_ROLL_SELECTION_INFO_H__

#include "audio/region.h"
#include "gui/widgets/region.h"
#include "utils/ui.h"

#include <gtk/gtk.h>

#define PIANO_ROLL_SELECTION_INFO_WIDGET_TYPE \
  (piano_roll_selection_info_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  PianoRollSelectionInfoWidget,
  piano_roll_selection_info_widget,
  Z, PIANO_ROLL_SELECTION_INFO_WIDGET,
  GtkStack);

#define MW_MAS_INFO \
  MW_CLIP_EDITOR->piano_roll_selections

typedef struct _SelectionInfoWidget
  SelectionInfoWidget;
typedef struct MidiArrangerSelections   \
  MidiArrangerSelections;

/**
 * A widget for showing info about the current
 * PianoRollSelections.
 */
typedef struct _PianoRollSelectionInfoWidget
{
  GtkStack              parent_instance;
  GtkLabel *            no_selection_label;
  SelectionInfoWidget * selection_info;
} PianoRollSelectionInfoWidget;

/**
 * Populates the SelectionInfoWidget based on the
 * leftmost object selected.
 */
void
piano_roll_selection_info_widget_refresh (
  PianoRollSelectionInfoWidget * self,
  MidiArrangerSelections * mas);

#endif