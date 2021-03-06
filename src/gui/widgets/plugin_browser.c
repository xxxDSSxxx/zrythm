/*
 * Copyright (C) 2018-2020 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "zrythm-config.h"

#include "actions/create_tracks_action.h"
#include "audio/engine.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/dialogs/string_entry_dialog.h"
#include "gui/widgets/expander_box.h"
#include "gui/widgets/plugin_browser.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/right_dock_edge.h"
#include "plugins/carla/carla_discovery.h"
#include "plugins/collections.h"
#include "plugins/plugin.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/err_codes.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/resources.h"
#include "utils/string.h"
#include "utils/ui.h"
#include "zrythm_app.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (
  PluginBrowserWidget,
  plugin_browser_widget,
  GTK_TYPE_PANED)

enum
{
  CAT_COLUMN_NAME,
  CAT_NUM_COLUMNS,
};

enum
{
  PL_COLUMN_ICON,
  PL_COLUMN_NAME,
  PL_COLUMN_DESCR,
  PL_NUM_COLUMNS
};

void
plugin_browser_widget_refresh_collections (
  PluginBrowserWidget * self);

static void
save_tree_view_selection (
  PluginBrowserWidget * self,
  GtkTreeView *         tree_view,
  const char *          key)
{
  GtkTreeSelection * selection;
  GList * selected_rows;
  char * path_strings[60000];
  int num_path_strings = 0;

  selection =
    gtk_tree_view_get_selection (
      GTK_TREE_VIEW (tree_view));
  selected_rows =
    gtk_tree_selection_get_selected_rows (
      selection, NULL);
  for (GList * l = selected_rows; l != NULL;
       l = g_list_next (l))
    {
      GtkTreePath * tp =
        (GtkTreePath *) l->data;
      char * path_str =
        gtk_tree_path_to_string (tp);
      path_strings[num_path_strings++] = path_str;
    }
  path_strings[num_path_strings] = NULL;
  g_settings_set_strv (
    S_UI, key,
    (const char * const *) path_strings);
  for (int i = 0; i < num_path_strings; i++)
    {
      g_free (path_strings[i]);
      path_strings[i] = NULL;
    }
}

/**
 * Save (remember) the current selections.
 */
static void
save_tree_view_selections (
  PluginBrowserWidget * self)
{
  save_tree_view_selection (
    self, self->collection_tree_view,
    "plugin-browser-collections");
  save_tree_view_selection (
    self, self->category_tree_view,
    "plugin-browser-categories");
  save_tree_view_selection (
    self, self->protocol_tree_view,
    "plugin-browser-protocols");
}

/**
 * Restores a selection from the gsettings.
 */
static void
restore_tree_view_selection (
  PluginBrowserWidget * self,
  GtkTreeView *         tree_view,
  const char *          key)
{
  char ** selection_paths =
    g_settings_get_strv (S_UI, key);

  GtkTreeSelection * selection =
    gtk_tree_view_get_selection (
      GTK_TREE_VIEW (tree_view));
  int i = 1;
  for (char * selection_path = selection_paths[0];
       selection_path != NULL;
       selection_path = selection_paths[i++])
    {
      GtkTreePath * tp =
        gtk_tree_path_new_from_string (
          selection_path);
      gtk_tree_selection_select_path (
        selection, tp);
    }

  g_strfreev (selection_paths);
}

static void
restore_tree_view_selections (
  PluginBrowserWidget * self)
{
  restore_tree_view_selection (
    self, self->collection_tree_view,
    "plugin-browser-collections");
  restore_tree_view_selection (
    self, self->category_tree_view,
    "plugin-browser-categories");
  restore_tree_view_selection (
    self, self->protocol_tree_view,
    "plugin-browser-protocols");
}

static void
activate_plugin_descr (
  PluginDescriptor * descr)
{
  TrackType tt =
    track_get_type_from_plugin_descriptor (descr);

  UndoableAction * ua =
    create_tracks_action_new (
      tt, descr, NULL, TRACKLIST->num_tracks,
      PLAYHEAD, 1);

  int err = undo_manager_perform (UNDO_MANAGER, ua);
  if (err)
    {
      ui_show_error_message (
        MAIN_WINDOW,
        error_code_get_message (err));
    }
}

/**
 * Called when row is double clicked.
 */
static void
on_row_activated (
  GtkTreeView       *tree_view,
  GtkTreePath       *tp,
  GtkTreeViewColumn *column,
  gpointer           user_data)
{
  GtkTreeModel * model = GTK_TREE_MODEL (user_data);
  GtkTreeIter iter;
  gtk_tree_model_get_iter (
    model,
    &iter,
    tp);
  GValue value = G_VALUE_INIT;
  gtk_tree_model_get_value (
    model, &iter, PL_COLUMN_DESCR, &value);
  PluginDescriptor * descr =
    g_value_get_pointer (&value);

  activate_plugin_descr (descr);
}

/**
 * Visible function for plugin tree model.
 *
 * Used for filtering based on selected category.
 */
static gboolean
visible_func (
  GtkTreeModel *model,
  GtkTreeIter  *iter,
  PluginBrowserWidget * self)
{
  // Visible if row is non-empty and category
  // matches selected
  PluginDescriptor *descr;

  gtk_tree_model_get (
    model, iter, PL_COLUMN_DESCR, &descr, -1);

  int instruments_active, effects_active,
      modulators_active, midi_modifiers_active;
  instruments_active =
    gtk_toggle_tool_button_get_active (
      self->toggle_instruments);
  effects_active =
    gtk_toggle_tool_button_get_active (
      self->toggle_effects);
  modulators_active =
    gtk_toggle_tool_button_get_active (
      self->toggle_modulators);
  midi_modifiers_active =
    gtk_toggle_tool_button_get_active (
      self->toggle_midi_modifiers);

  /* no filter, all visible */
  if (self->num_selected_categories == 0 &&
      self->num_selected_protocols == 0 &&
      !self->selected_collection &&
      !instruments_active &&
      !effects_active &&
      !modulators_active &&
      !midi_modifiers_active)
    {
      return true;
    }

  int visible = false;

  /* not visible if category selected and plugin
   * doesn't match */
  if (self->num_selected_categories > 0)
    {
      for (int i = 0;
           i < self->num_selected_categories; i++)
        {
          if (descr->category ==
                self->selected_categories[i])
            {
              visible = true;
              break;
            }
        }

      /* not visible if the category is not one
       * of the selected categories */
      if (!visible)
        return false;
    }

  /* not visible if plugin is not part of
   * selected collection */
  if (self->selected_collection)
    {
      if (plugin_collection_contains_descriptor (
            self->selected_collection, descr,
            false))
        {
          visible = true;
        }
      else
        {
          return false;
        }
    }

  /* not visible if plugin protocol is not one of
   * selected protocols */
  if (self->num_selected_protocols > 0)
    {
      for (int i = 0;
           i < self->num_selected_protocols; i++)
        {
          if (descr->protocol ==
                self->selected_protocols[i])
            {
              visible = true;
              break;
            }
        }

      /* not visible if the category is not one
       * of the selected categories */
      if (!visible)
        return false;
    }

  /* not visibile if plugin type doesn't match */
  if (instruments_active &&
      !plugin_descriptor_is_instrument (descr))
    return false;
  if (effects_active &&
      !plugin_descriptor_is_effect (descr))
    return false;
  if (modulators_active &&
      !plugin_descriptor_is_modulator (descr))
    return false;
  if (midi_modifiers_active &&
      !plugin_descriptor_is_midi_modifier (descr))
    return false;

  return true;
}

static void
on_plugin_descr_activate (
  GtkMenuItem *      menuitem,
  PluginDescriptor * descr)
{
  g_message ("%s: activated", __func__);
  activate_plugin_descr (descr);
}

static void
on_plugin_descr_add_to_collection (
  GtkMenuItem *      menuitem,
  PluginCollection * collection)
{
  g_return_if_fail (
    MW_PLUGIN_BROWSER->num_current_descriptors > 0);

  plugin_collection_add_descriptor (
    collection,
    MW_PLUGIN_BROWSER->current_descriptors[0]);

  plugin_collections_serialize_to_file (
    PLUGIN_MANAGER->collections);

  EVENTS_PUSH (ET_PLUGIN_COLLETIONS_CHANGED, NULL);
}

static void
on_plugin_descr_remove_from_collection (
  GtkMenuItem *      menuitem,
  PluginCollection * collection)
{
  g_return_if_fail (
    MW_PLUGIN_BROWSER->num_current_descriptors > 0);

  plugin_collection_remove_descriptor (
    collection,
    MW_PLUGIN_BROWSER->current_descriptors[0]);

  plugin_collections_serialize_to_file (
    PLUGIN_MANAGER->collections);

  EVENTS_PUSH (ET_PLUGIN_COLLETIONS_CHANGED, NULL);
}

static void
delete_plugin_descr (
  PluginDescriptor * descr,
  GClosure *         closure)
{
  plugin_descriptor_free (descr);
}

static void
show_plugin_context_menu (
  PluginBrowserWidget * self,
  PluginDescriptor *    descr)
{
  g_return_if_fail (self && descr);
  self->current_descriptors[0] = descr;
  self->num_current_descriptors = 1;

  GtkMenuItem * menuitem;
  GtkWidget * menu = gtk_menu_new();
  PluginDescriptor * new_descr;

  /* FIXME this is allocating memory every time */

#define CREATE_WITH_LBL(lbl) \
  menuitem = \
    GTK_MENU_ITEM ( \
      gtk_menu_item_new_with_label (lbl)); \
  gtk_widget_set_visible ( \
    GTK_WIDGET (menuitem), true); \
  APPEND; \
  new_descr = plugin_descriptor_clone (descr)

#define CONNECT_SIGNAL \
  g_signal_connect_data ( \
    G_OBJECT (menuitem), "activate", \
    G_CALLBACK (on_plugin_descr_activate), \
    new_descr, \
    (GClosureNotify) delete_plugin_descr, 0)

#define APPEND \
  gtk_menu_shell_append ( \
    GTK_MENU_SHELL (menu), \
    GTK_WIDGET (menuitem));

  CREATE_WITH_LBL (_("Add to project"));
  new_descr->open_with_carla = false;
  CONNECT_SIGNAL;

#ifdef HAVE_CARLA
  /* carla doesn't work with cv ports */
  if (descr->num_cv_ins == 0 &&
      descr->num_cv_outs == 0)
    {
      CREATE_WITH_LBL (
        _("Add to project (carla)"));
      new_descr->open_with_carla = true;
      new_descr->bridge_mode = CARLA_BRIDGE_NONE;
      CONNECT_SIGNAL;

      if (plugin_descriptor_has_custom_ui (descr) &&
          z_carla_discovery_get_bridge_mode (descr) ==
            CARLA_BRIDGE_NONE)
        {
          CREATE_WITH_LBL (
            _("Add to project (bridged UI)"));
          new_descr->open_with_carla = true;
          new_descr->bridge_mode = CARLA_BRIDGE_UI;
          CONNECT_SIGNAL;
        }

      CREATE_WITH_LBL (
        _("Add to project (bridged full)"));
      new_descr->open_with_carla = true;
      new_descr->bridge_mode = CARLA_BRIDGE_FULL;
      CONNECT_SIGNAL;
    }
#endif

  /* add to collection */
  menuitem =
    z_gtk_create_menu_item (
      _("Add to collection"), "favorite",
      false, NULL);
  gtk_widget_set_visible (
    GTK_WIDGET (menuitem), true);
  GtkWidget * submenu = gtk_menu_new ();
  int num_added = 0;
  for (int i = 0;
       i < PLUGIN_MANAGER->collections->
         num_collections;
       i++)
    {
      PluginCollection * coll =
        PLUGIN_MANAGER->collections->collections[i];
      if (plugin_collection_contains_descriptor (
            coll, descr, false))
        {
          continue;
        }

      GtkWidget * submenu_item =
        gtk_menu_item_new_with_label (coll->name);
      gtk_widget_set_visible (
        GTK_WIDGET (submenu_item), true);
      gtk_menu_shell_append (
        GTK_MENU_SHELL (submenu),
        GTK_WIDGET (submenu_item));
      g_signal_connect (
        G_OBJECT (submenu_item), "activate",
        G_CALLBACK (
          on_plugin_descr_add_to_collection),
        coll);
      num_added++;
    }
  gtk_menu_item_set_submenu (
    GTK_MENU_ITEM (menuitem),
    GTK_WIDGET (submenu));
  if (num_added > 0)
    {
      APPEND;
    }
  else
    {
      g_object_ref_sink (menuitem);
      g_object_unref (menuitem);
    }

  /* remove from collection */
  menuitem =
    z_gtk_create_menu_item (
      _("Remove from collection"), "unfavorite",
      false, NULL);
  gtk_widget_set_visible (
    GTK_WIDGET (menuitem), true);
  submenu = gtk_menu_new ();
  num_added = 0;
  for (int i = 0;
       i < PLUGIN_MANAGER->collections->
         num_collections;
       i++)
    {
      PluginCollection * coll =
        PLUGIN_MANAGER->collections->collections[i];
      if (!plugin_collection_contains_descriptor (
            coll, descr, false))
        {
          continue;
        }

      GtkWidget * submenu_item =
        gtk_menu_item_new_with_label (coll->name);
      gtk_widget_set_visible (
        GTK_WIDGET (submenu_item), true);
      gtk_menu_shell_append (
        GTK_MENU_SHELL (submenu),
        GTK_WIDGET (submenu_item));
      g_signal_connect (
        G_OBJECT (submenu_item), "activate",
        G_CALLBACK (
          on_plugin_descr_remove_from_collection),
        coll);
      num_added++;
    }
  gtk_menu_item_set_submenu (
    GTK_MENU_ITEM (menuitem),
    GTK_WIDGET (submenu));
  if (num_added > 0)
    {
      APPEND;
    }
  else
    {
      g_object_ref_sink (menuitem);
      g_object_unref (menuitem);
    }

#undef APPEND

  gtk_menu_attach_to_widget (
    GTK_MENU (menu),
    GTK_WIDGET (self), NULL);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void
on_plugin_right_click (
  GtkGestureMultiPress * gesture,
  gint                   n_press,
  gdouble                x_dbl,
  gdouble                y_dbl,
  PluginBrowserWidget *  self)
{
  if (n_press != 1)
    return;

  GtkTreePath *path;
  GtkTreeViewColumn *column;

  int x, y;
  gtk_tree_view_convert_widget_to_bin_window_coords (
    GTK_TREE_VIEW (self->plugin_tree_view),
    (int) x_dbl, (int) y_dbl, &x, &y);

  GtkTreeSelection * selection =
    gtk_tree_view_get_selection (
      (self->plugin_tree_view));
  if (!gtk_tree_view_get_path_at_pos (
        GTK_TREE_VIEW (self->plugin_tree_view),
        x, y,
        &path, &column, NULL, NULL))
    {
      g_message ("no path at position %d %d", x, y);
      // if we can't find path at pos, we surely don't
      // want to pop up the menu
      return;
    }

  gtk_tree_selection_unselect_all(selection);
  gtk_tree_selection_select_path(selection, path);
  GtkTreeIter iter;
  gtk_tree_model_get_iter (
    GTK_TREE_MODEL (self->plugin_tree_model),
    &iter, path);
  GValue value = G_VALUE_INIT;
  gtk_tree_model_get_value (
    GTK_TREE_MODEL (self->plugin_tree_model),
    &iter,
    PL_COLUMN_DESCR,
    &value);
  gtk_tree_path_free(path);

  PluginDescriptor * descr =
    g_value_get_pointer (&value);

  show_plugin_context_menu (self, descr);
}

static void
on_collection_add_activate (
  GtkMenuItem *         menuitem,
  PluginBrowserWidget * self)
{
  PluginCollection * collection =
    plugin_collection_new ();

  StringEntryDialogWidget * dialog =
    string_entry_dialog_widget_new (
      _("Collection name"), collection,
      (GenericStringGetter)
        plugin_collection_get_name,
      (GenericStringSetter)
        plugin_collection_set_name);
  gtk_widget_show_all (GTK_WIDGET (dialog));
  gtk_dialog_run (GTK_DIALOG (dialog));

  plugin_collections_add (
    PLUGIN_MANAGER->collections, collection,
    F_SERIALIZE);

  plugin_browser_widget_refresh_collections (self);

  plugin_collection_free (collection);
}

static void
on_collection_rename_activate (
  GtkMenuItem *         menuitem,
  PluginBrowserWidget * self)
{
  g_return_if_fail (
    self->num_current_collections > 0);
  PluginCollection * collection =
    self->current_collections[0];

  StringEntryDialogWidget * dialog =
    string_entry_dialog_widget_new (
      _("Collection name"), collection,
      (GenericStringGetter)
        plugin_collection_get_name,
      (GenericStringSetter)
        plugin_collection_set_name);
  gtk_widget_show_all (GTK_WIDGET (dialog));
  gtk_dialog_run (GTK_DIALOG (dialog));

  plugin_collections_serialize_to_file (
    PLUGIN_MANAGER->collections);

  EVENTS_PUSH (ET_PLUGIN_COLLETIONS_CHANGED, NULL);
}

static void
on_collection_remove_activate (
  GtkMenuItem *      menuitem,
  PluginBrowserWidget * self)
{
  g_return_if_fail (
    self->num_current_collections > 0);
  int result = GTK_RESPONSE_YES;
  PluginCollection * collection =
    self->current_collections[0];

  if (collection->num_descriptors > 0)
    {
      GtkWidget * dialog =
        gtk_message_dialog_new (
          GTK_WINDOW (MAIN_WINDOW),
          GTK_DIALOG_MODAL |
            GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_MESSAGE_QUESTION,
          GTK_BUTTONS_YES_NO,
          _("This collection contains %d plugins. "
          "Are you sure you want to remove it?"),
          collection->num_descriptors);
      gtk_widget_show_all (GTK_WIDGET (dialog));
      result =
        gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }

  if (result == GTK_RESPONSE_YES)
    {
      plugin_collections_remove (
        PLUGIN_MANAGER->collections,
        self->current_collections[0],
        F_SERIALIZE);

      EVENTS_PUSH (
        ET_PLUGIN_COLLETIONS_CHANGED, NULL);
    }
}

static void
show_collection_context_menu (
  PluginBrowserWidget * self,
  PluginCollection *    collection)
{
  GtkMenuItem * menuitem;
  GtkWidget * menu = gtk_menu_new();

#define APPEND \
  gtk_menu_shell_append ( \
    GTK_MENU_SHELL (menu), \
    GTK_WIDGET (menuitem));

  if (collection)
    {
      self->current_collections[0] = collection;
      self->num_current_collections = 1;

      menuitem =
        z_gtk_create_menu_item (
          _("Rename"), "edit-rename", false, NULL);
      gtk_widget_set_visible (
        GTK_WIDGET (menuitem), true);
      APPEND;
      g_signal_connect (
        G_OBJECT (menuitem), "activate",
        G_CALLBACK (on_collection_rename_activate),
        self);

      menuitem =
        z_gtk_create_menu_item (
          _("Delete"), "edit-delete", false, NULL);
      gtk_widget_set_visible (
        GTK_WIDGET (menuitem), true);
      APPEND;
      g_signal_connect (
        G_OBJECT (menuitem), "activate",
        G_CALLBACK (on_collection_remove_activate),
        self);
    }
  else
    {
      self->num_current_collections = 0;

      menuitem =
        z_gtk_create_menu_item (
          _("Add"), "add", false, NULL);
      gtk_widget_set_visible (
        GTK_WIDGET (menuitem), true);
      APPEND;
      g_signal_connect (
        G_OBJECT (menuitem), "activate",
        G_CALLBACK (on_collection_add_activate),
        self);
    }

#undef APPEND

  gtk_menu_attach_to_widget (
    GTK_MENU (menu),
    GTK_WIDGET (self), NULL);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void
on_collection_right_click (
  GtkGestureMultiPress * gesture,
  gint                   n_press,
  gdouble                x_dbl,
  gdouble                y_dbl,
  PluginBrowserWidget *  self)
{
  if (n_press != 1)
    return;

  GtkTreePath *path;
  GtkTreeViewColumn *column;

  int x, y;
  gtk_tree_view_convert_widget_to_bin_window_coords (
    GTK_TREE_VIEW (self->collection_tree_view),
    (int) x_dbl, (int) y_dbl, &x, &y);

  GtkTreeSelection * selection =
    gtk_tree_view_get_selection (
      (self->collection_tree_view));
  bool have_selection = true;
  if (!gtk_tree_view_get_path_at_pos (
        GTK_TREE_VIEW (self->collection_tree_view),
        x, y,
        &path, &column, NULL, NULL))
    {
      /* no collection selected */
      have_selection = false;
    }

  PluginCollection * collection = NULL;

  gtk_tree_selection_unselect_all (selection);
  if (have_selection)
    {
      gtk_tree_selection_select_path (
        selection, path);
      GtkTreeIter iter;
      gtk_tree_model_get_iter (
        GTK_TREE_MODEL (
          self->collection_tree_model),
        &iter, path);
      int collection_idx = 0;
      gtk_tree_model_get (
        GTK_TREE_MODEL (
          self->collection_tree_model),
        &iter, 1, &collection_idx, -1);
      gtk_tree_path_free (path);

      collection =
        PLUGIN_MANAGER->collections->collections[
          collection_idx];
    }

  show_collection_context_menu (self, collection);
}

static void
on_category_reset_activate (
  GtkMenuItem *         menuitem,
  PluginBrowserWidget * self)
{
  GtkTreeSelection * selection =
    gtk_tree_view_get_selection (
      (self->category_tree_view));
  gtk_tree_selection_unselect_all (selection);
}

static void
show_category_context_menu (
  PluginBrowserWidget * self)
{
  GtkWidget *menuitem;
  GtkWidget * menu = gtk_menu_new();

  /* FIXME this is allocating memory every time */

  menuitem =
    gtk_menu_item_new_with_label (_("Reset"));
  gtk_widget_set_visible (menuitem, true);
  gtk_menu_shell_append (
    GTK_MENU_SHELL (menu),
    GTK_WIDGET (menuitem));

  g_signal_connect (
    G_OBJECT (menuitem), "activate",
    G_CALLBACK (on_category_reset_activate),
    self);

  gtk_menu_attach_to_widget (
    GTK_MENU (menu),
    GTK_WIDGET (self), NULL);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void
on_category_right_click (
  GtkGestureMultiPress * gesture,
  gint                   n_press,
  gdouble                x_dbl,
  gdouble                y_dbl,
  PluginBrowserWidget *  self)
{
  if (n_press != 1)
    return;

  GtkTreePath *path;
  GtkTreeViewColumn *column;

  int x, y;
  gtk_tree_view_convert_widget_to_bin_window_coords (
    GTK_TREE_VIEW (self->category_tree_view),
    (int) x_dbl, (int) y_dbl, &x, &y);

  if (!gtk_tree_view_get_path_at_pos (
        GTK_TREE_VIEW (self->category_tree_view),
        x, y,
        &path, &column, NULL, NULL))
    {
      g_message ("no path at position %d %d", x, y);
      return;
    }

  show_category_context_menu (self);
}

/**
 * Updates the label below the list of plugins with
 * the plugin info.
 */
static int
update_plugin_info_label (
  PluginBrowserWidget * self,
  gpointer user_data)
{
  char * label = (char *) user_data;

  gtk_label_set_text (self->plugin_info, label);

  return G_SOURCE_REMOVE;
}

static void
cat_selected_foreach (
  GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  PluginBrowserWidget * self)
{
  char * str;
  gtk_tree_model_get (
    model, iter, 0, &str, -1);

  self->selected_categories[
    self->num_selected_categories++] =
      plugin_descriptor_string_to_category (
        str);

  g_free (str);
}

static void
protocol_selected_foreach (
  GtkTreeModel *model,
  GtkTreePath *path,
  GtkTreeIter *iter,
  PluginBrowserWidget * self)
{
  PluginProtocol protocol;
  gtk_tree_model_get (
    model, iter, 2, &protocol, -1);

  self->selected_protocols[
    self->num_selected_protocols++] =
      protocol;
}

static void
on_selection_changed (
  GtkTreeSelection * ts,
  PluginBrowserWidget * self)
{
  GtkTreeView * tv =
    gtk_tree_selection_get_tree_view (ts);
  GtkTreeModel * model =
    gtk_tree_view_get_model (tv);
  GList * selected_rows =
    gtk_tree_selection_get_selected_rows (
      ts, NULL);

  if (model == self->category_tree_model)
    {
      self->num_selected_categories = 0;

      gtk_tree_selection_selected_foreach (
        ts,
        (GtkTreeSelectionForeachFunc)
          cat_selected_foreach,
        self);

      gtk_tree_model_filter_refilter (
        self->plugin_tree_model);
    }
  else if (model ==
             GTK_TREE_MODEL (
               self->protocol_tree_model))
    {
      self->num_selected_protocols = 0;

      gtk_tree_selection_selected_foreach (
        ts,
        (GtkTreeSelectionForeachFunc)
          protocol_selected_foreach,
        self);

      gtk_tree_model_filter_refilter (
        self->plugin_tree_model);
    }
  else if (model ==
             GTK_TREE_MODEL (
               self->plugin_tree_model))
    {
      if (selected_rows)
        {
          GtkTreePath * tp =
            (GtkTreePath *)
            g_list_first (selected_rows)->data;
          GtkTreeIter iter;
          gtk_tree_model_get_iter (
            model, &iter, tp);
          PluginDescriptor * descr;
          gtk_tree_model_get (
            model, &iter, PL_COLUMN_DESCR, &descr, -1);
          char * label = g_strdup_printf (
            "%s\n%s, %s%s\nAudio: %d, %d\nMidi: %d, "
            "%d\nControls: %d, %d\nCV: %d, %d",
            descr->author,
            descr->category_str,
            plugin_protocol_to_str (
              descr->protocol),
            descr->arch == ARCH_32 ? " (32-bit)" : "",
            descr->num_audio_ins,
            descr->num_audio_outs,
            descr->num_midi_ins,
            descr->num_midi_outs,
            descr->num_ctrl_ins,
            descr->num_ctrl_outs,
            descr->num_cv_ins,
            descr->num_cv_outs);
          update_plugin_info_label (
            self, label);
        }
    }
  else if (model ==
             GTK_TREE_MODEL (
               self->collection_tree_model))
    {
      if (selected_rows)
        {
          GtkTreeIter iter;
          gtk_tree_selection_get_selected (
            ts, NULL, &iter);
          int collection_idx = 0;
          gtk_tree_model_get (
            GTK_TREE_MODEL (
              self->collection_tree_model),
            &iter, 1, &collection_idx, -1);

          self->selected_collection =
            PLUGIN_MANAGER->collections->
              collections[collection_idx];
        }
      else
        {
          self->selected_collection = NULL;
        }
      g_message (
        "selected collection: %s",
        self->selected_collection ?
          self->selected_collection->name :
          "none");

      gtk_tree_model_filter_refilter (
        self->plugin_tree_model);
    }

  g_list_free_full (
    selected_rows,
    (GDestroyNotify) gtk_tree_path_free);

  save_tree_view_selections (self);
}

static void
on_drag_data_get (
  GtkWidget        *widget,
  GdkDragContext   *context,
  GtkSelectionData *data,
  guint             info,
  guint             time,
  gpointer          user_data)
{
  GtkTreeView * tv = GTK_TREE_VIEW (user_data);
  PluginDescriptor * descr =
    z_gtk_get_single_selection_pointer (
      tv, PL_COLUMN_DESCR);

  gtk_selection_data_set (data,
    gdk_atom_intern_static_string (
      TARGET_ENTRY_PLUGIN_DESCR),
    32,
    (const guchar *)&descr,
    sizeof (descr));
}

static GtkTreeModel *
create_model_for_favorites ()
{
  /* list name, list index */
  GtkListStore * list_store =
    gtk_list_store_new (
      2, G_TYPE_STRING, G_TYPE_INT);

  PluginCollections * collections =
    PLUGIN_MANAGER->collections;
  g_return_val_if_fail (collections, NULL);

  for (int i = 0;
       i < collections->num_collections; i++)
    {
      PluginCollection * collection =
        collections->collections[i];

      /* add row to model */
      GtkTreeIter iter;
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (
        list_store, &iter,
        0, collection->name,
        1, i,
        -1);
    }

  return GTK_TREE_MODEL (list_store);
}

static GtkTreeModel *
create_model_for_categories ()
{
  /* plugin category */
  GtkListStore * list_store =
    gtk_list_store_new (
      1, G_TYPE_STRING);

  GtkTreeIter iter;

  for (int i = 0;
       i < PLUGIN_MANAGER->num_plugin_categories;
       i++)
    {
      const gchar * name =
        PLUGIN_MANAGER->plugin_categories[i];

      // Add a new row to the model
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (
        list_store, &iter, 0, name, -1);
    }

  return GTK_TREE_MODEL (list_store);
}

static GtkTreeModel *
create_model_for_protocols ()
{
  /* protocol icon, string, enum */
  GtkListStore * list_store =
    gtk_list_store_new (
      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

  GtkTreeIter iter;

  for (PluginProtocol prot = PROT_LV2;
       prot <= PROT_SF2; prot++)
    {
      const char * name =
        plugin_protocol_to_str (prot);

      const char * icon = NULL;
      switch (prot)
        {
        case PROT_LV2:
          icon = "logo-lv2";
          break;
        case PROT_LADSPA:
          icon = "logo-ladspa";
          break;
        case PROT_AU:
          icon = "logo-au";
          break;
        case PROT_VST:
        case PROT_VST3:
          icon = "logo-vst";
          break;
        default:
          icon = "plug";
          break;
        }

      /* Add a new row to the model */
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (
        list_store, &iter,
        0, icon,
        1, name,
        2, prot,
        -1);
    }

  GtkTreeModel * model =
    gtk_tree_model_sort_new_with_model (
      GTK_TREE_MODEL (list_store));

  return model;
}

static GtkTreeModel *
create_model_for_plugins (
  PluginBrowserWidget * self)
{
  GtkListStore *list_store;
  /*GtkTreePath *path;*/
  GtkTreeIter iter;
  gint i;

  /* plugin name, index */
  list_store =
    gtk_list_store_new (
      PL_NUM_COLUMNS, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_POINTER);

  for (i = 0; i < PLUGIN_MANAGER->num_plugins; i++)
    {
      PluginDescriptor * descr =
        PLUGIN_MANAGER->plugin_descriptors[i];
      const char * icon_name = NULL;
      if (plugin_descriptor_is_instrument (descr))
        {
          icon_name = "instrument";
        }
      else if (plugin_descriptor_is_modulator (
                 descr))
        {
          icon_name = "modulator";
        }
      else if (plugin_descriptor_is_midi_modifier (
                 descr))
        {
          icon_name = "signal-midi";
        }
      else if (plugin_descriptor_is_effect (
                 descr))
        {
          icon_name = "bars";
        }
      else
        {
          icon_name = "plug";
        }

      /*else if (!strcmp (descr->category, "Distortion"))*/
        /*icon_name = "distortionfx";*/

      // Add a new row to the model
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (
        list_store, &iter,
        PL_COLUMN_ICON, icon_name,
        PL_COLUMN_NAME, descr->name,
        PL_COLUMN_DESCR, descr,
        -1);
    }

  GtkTreeModel * model =
    gtk_tree_model_filter_new (
      GTK_TREE_MODEL (list_store),
      NULL);
  gtk_tree_model_filter_set_visible_func (
    GTK_TREE_MODEL_FILTER (model),
    (GtkTreeModelFilterVisibleFunc) visible_func,
    self, NULL);

  return model;
}

static gboolean
plugin_search_equal_func (
  GtkTreeModel *model,
  gint column,
  const gchar *key,
  GtkTreeIter *iter,
  PluginBrowserWidget * self)
{
  char * str;
  gtk_tree_model_get (
    model, iter, column, &str, -1);

  char * down_key =
    g_utf8_strdown (key, -1);
  char * down_str =
    g_utf8_strdown (str, -1);

  int match =
    g_strrstr (down_str, down_key) != NULL;

  g_free (str);
  g_free (down_key);
  g_free (down_str);

  return !match;
}

static void
tree_view_setup (
  PluginBrowserWidget * self,
  GtkTreeView *         tree_view,
  GtkTreeModel *        model,
  bool                  allow_multi,
  bool                  dnd)
{
  gtk_tree_view_set_model (tree_view, model);

  z_gtk_tree_view_remove_all_columns (tree_view);

  /* init tree view */
  GtkCellRenderer * renderer;
  GtkTreeViewColumn * column;
  if (model ==
        GTK_TREE_MODEL (self->plugin_tree_model))
    {
      /* column for icon */
      renderer =
        gtk_cell_renderer_pixbuf_new ();
      column =
        gtk_tree_view_column_new_with_attributes (
          "icon", renderer,
          "icon-name", PL_COLUMN_ICON,
          NULL);
      gtk_tree_view_append_column (
        GTK_TREE_VIEW (tree_view),
        column);

      /* column for name */
      renderer =
        gtk_cell_renderer_text_new ();
      column =
        gtk_tree_view_column_new_with_attributes (
          "name", renderer,
          "text", PL_COLUMN_NAME,
          NULL);
      gtk_tree_view_append_column (
        GTK_TREE_VIEW (tree_view),
        column);

      /* set search column */
      gtk_tree_view_set_search_column (
        GTK_TREE_VIEW (tree_view),
        PL_COLUMN_NAME);

      /* set search func */
      gtk_tree_view_set_search_equal_func (
        GTK_TREE_VIEW (tree_view),
        (GtkTreeViewSearchEqualFunc)
          plugin_search_equal_func,
        self, NULL);

      /* connect right click handler */
      GtkGestureMultiPress * mp =
        GTK_GESTURE_MULTI_PRESS (
          gtk_gesture_multi_press_new (
            GTK_WIDGET (tree_view)));
      gtk_gesture_single_set_button (
        GTK_GESTURE_SINGLE (mp),
        GDK_BUTTON_SECONDARY);
      g_signal_connect (
        G_OBJECT (mp), "pressed",
        G_CALLBACK (on_plugin_right_click), self);
    }
  else if (model ==
             GTK_TREE_MODEL (
               self->protocol_tree_model))
    {
      /* column for icon */
      renderer =
        gtk_cell_renderer_pixbuf_new ();
      column =
        gtk_tree_view_column_new_with_attributes (
          "icon", renderer,
          "icon-name", 0,
          NULL);
      gtk_tree_view_append_column (
        GTK_TREE_VIEW (tree_view), column);

      /* column for name */
      renderer =
        gtk_cell_renderer_text_new ();
      column =
        gtk_tree_view_column_new_with_attributes (
          "name", renderer,
          "text", 1,
          NULL);
      gtk_tree_view_append_column (
        GTK_TREE_VIEW (tree_view),
        column);

      /* set search column */
      gtk_tree_view_set_search_column (
        GTK_TREE_VIEW (tree_view), 1);

      gtk_tree_sortable_set_sort_column_id (
        GTK_TREE_SORTABLE (model), 1,
        GTK_SORT_ASCENDING);
    }
  else
    {
      /* column for name */
      renderer =
        gtk_cell_renderer_text_new ();
      column =
        gtk_tree_view_column_new_with_attributes (
          "name", renderer,
          "text", 0,
          NULL);
      gtk_tree_view_append_column (
        GTK_TREE_VIEW (tree_view),
        column);

      /* connect right click handler */
      GtkGestureMultiPress * mp =
        GTK_GESTURE_MULTI_PRESS (
          gtk_gesture_multi_press_new (
            GTK_WIDGET (tree_view)));
      gtk_gesture_single_set_button (
        GTK_GESTURE_SINGLE (mp),
        GDK_BUTTON_SECONDARY);
      if (model ==
            GTK_TREE_MODEL (
              self->category_tree_model))
        {
          g_signal_connect (
            G_OBJECT (mp), "pressed",
            G_CALLBACK (on_category_right_click),
            self);
        }
      else if (model ==
            GTK_TREE_MODEL (
              self->collection_tree_model))
        {
          g_signal_connect (
            G_OBJECT (mp), "pressed",
            G_CALLBACK (on_collection_right_click),
            self);
        }
      else
        {
          g_object_unref (mp);
        }
    }

  /* hide headers and allow multi-selection */
  gtk_tree_view_set_headers_visible (
    GTK_TREE_VIEW (tree_view), false);

  if (allow_multi)
    {
      gtk_tree_selection_set_mode (
        gtk_tree_view_get_selection (
          GTK_TREE_VIEW (tree_view)),
        GTK_SELECTION_MULTIPLE);
    }

  if (dnd)
    {
      char * entry_plugin_descr =
        g_strdup (TARGET_ENTRY_PLUGIN_DESCR);
      GtkTargetEntry entries[] = {
        {
          entry_plugin_descr, GTK_TARGET_SAME_APP,
          symap_map (ZSYMAP, TARGET_ENTRY_PLUGIN_DESCR),
        },
      };
      gtk_tree_view_enable_model_drag_source (
        GTK_TREE_VIEW (tree_view),
        GDK_BUTTON1_MASK,
        entries, G_N_ELEMENTS (entries),
        GDK_ACTION_COPY);
      g_free (entry_plugin_descr);

      g_signal_connect (
        GTK_WIDGET (tree_view), "drag-data-get",
        G_CALLBACK (on_drag_data_get), tree_view);
    }

  GtkTreeSelection * sel =
    gtk_tree_view_get_selection (
      GTK_TREE_VIEW (tree_view));
  g_signal_connect (
    G_OBJECT (sel), "changed",
    G_CALLBACK (on_selection_changed), self);
}

void
plugin_browser_widget_refresh_collections (
  PluginBrowserWidget * self)
{
  self->collection_tree_model =
    create_model_for_favorites ();
  tree_view_setup (
    self, self->collection_tree_view,
    self->collection_tree_model,
    F_NO_MULTI_SELECT, F_NO_DND);
}

static void
on_visible_child_changed (
  GtkStack * stack,
  GParamSpec * pspec,
  PluginBrowserWidget * self)
{
  GtkWidget * child =
    gtk_stack_get_visible_child (stack);

  if (child == GTK_WIDGET (self->collection_scroll))
    {
      S_UI_SET_ENUM (
        "plugin-browser-tab",
        PLUGIN_BROWSER_TAB_COLLECTION);
    }
  else if (child ==
           GTK_WIDGET (self->category_scroll))
    {
      S_UI_SET_ENUM (
        "plugin-browser-tab",
        PLUGIN_BROWSER_TAB_CATEGORY);
    }
  else if (child ==
           GTK_WIDGET (self->protocol_scroll))
    {
      S_UI_SET_ENUM (
        "plugin-browser-tab",
        PLUGIN_BROWSER_TAB_PROTOCOL);
    }
}

static void
toggles_changed (
  GtkToggleToolButton * btn,
  PluginBrowserWidget * self)
{
  if (gtk_toggle_tool_button_get_active (btn))
    {
      /* block signals, unset all, unblock */
      g_signal_handlers_block_by_func (
        self->toggle_instruments,
        toggles_changed, self);
      g_signal_handlers_block_by_func (
        self->toggle_effects,
        toggles_changed, self);
      g_signal_handlers_block_by_func (
        self->toggle_modulators,
        toggles_changed, self);
      g_signal_handlers_block_by_func (
        self->toggle_midi_modifiers,
        toggles_changed, self);

      if (btn == self->toggle_instruments)
        {
          S_UI_SET_ENUM (
            "plugin-browser-filter",
            PLUGIN_BROWSER_FILTER_INSTRUMENT);
          gtk_toggle_tool_button_set_active (
            self->toggle_effects, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_modulators, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_midi_modifiers, 0);
        }
      else if (btn == self->toggle_effects)
        {
          S_UI_SET_ENUM (
            "plugin-browser-filter",
            PLUGIN_BROWSER_FILTER_EFFECT);
          gtk_toggle_tool_button_set_active (
            self->toggle_instruments, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_modulators, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_midi_modifiers, 0);
        }
      else if (btn == self->toggle_modulators)
        {
          S_UI_SET_ENUM (
            "plugin-browser-filter",
            PLUGIN_BROWSER_FILTER_MODULATOR);
          gtk_toggle_tool_button_set_active (
            self->toggle_instruments, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_effects, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_midi_modifiers, 0);
        }
      else if (btn == self->toggle_midi_modifiers)
        {
          S_UI_SET_ENUM (
            "plugin-browser-filter",
            PLUGIN_BROWSER_FILTER_MIDI_EFFECT);
          gtk_toggle_tool_button_set_active (
            self->toggle_instruments, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_effects, 0);
          gtk_toggle_tool_button_set_active (
            self->toggle_modulators, 0);
        }

      g_signal_handlers_unblock_by_func (
        self->toggle_instruments,
        toggles_changed, self);
      g_signal_handlers_unblock_by_func (
        self->toggle_effects,
        toggles_changed, self);
      g_signal_handlers_unblock_by_func (
        self->toggle_modulators,
        toggles_changed, self);
      g_signal_handlers_unblock_by_func (
        self->toggle_midi_modifiers,
        toggles_changed, self);
    }
  else
    {
      S_UI_SET_ENUM (
        "plugin-browser-filter",
        PLUGIN_BROWSER_FILTER_NONE);
    }
  gtk_tree_model_filter_refilter (
    self->plugin_tree_model);
}

static int
on_map_event (
  GtkStack * stack,
  GdkEvent * event,
  PluginBrowserWidget * self)
{
  /*g_message ("PLUGIN MAP EVENT");*/
  if (gtk_widget_get_mapped (
        GTK_WIDGET (self)))
    {
      self->start_saving_pos = 1;
      self->first_time_position_set_time =
        g_get_monotonic_time ();

      /* set divider position */
      int divider_pos =
        g_settings_get_int (
          S_UI,
          "browser-divider-position");
      gtk_paned_set_position (
        GTK_PANED (self), divider_pos);
      self->first_time_position_set = 1;
    }

  return false;
}

static void
on_position_change (
  GtkStack * stack,
  GParamSpec * pspec,
  PluginBrowserWidget * self)
{
  int divider_pos;
  if (!self->start_saving_pos)
    return;

  gint64 curr_time = g_get_monotonic_time ();

  if (self->first_time_position_set ||
      curr_time -
        self->first_time_position_set_time < 400000)
    {
      /* get divider position */
      divider_pos =
        g_settings_get_int (
          S_UI,
          "browser-divider-position");
      gtk_paned_set_position (
        GTK_PANED (self),
        divider_pos);

      self->first_time_position_set = 0;
      /*g_message ("***************************got plugin position %d",*/
                 /*divider_pos);*/
    }
  else
    {
      /* save the divide position */
      divider_pos =
        gtk_paned_get_position (
          GTK_PANED (self));
      g_settings_set_int (
        S_UI,
        "browser-divider-position",
        divider_pos);
      /*g_message ("***************************set plugin position to %d",*/
                 /*divider_pos);*/
    }
}

PluginBrowserWidget *
plugin_browser_widget_new ()
{
  PluginBrowserWidget * self =
    g_object_new (
      PLUGIN_BROWSER_WIDGET_TYPE, NULL);

  gtk_label_set_xalign (self->plugin_info, 0);

  /* setup collections */
  plugin_browser_widget_refresh_collections (self);

  /* setup protocols */
  self->protocol_tree_model =
   GTK_TREE_MODEL_SORT (
     create_model_for_protocols ());
  tree_view_setup (
    self, self->protocol_tree_view,
    GTK_TREE_MODEL (self->protocol_tree_model),
    F_MULTI_SELECT, F_NO_DND);

  /* setup categories */
  self->category_tree_model =
    create_model_for_categories ();
  tree_view_setup (
    self, self->category_tree_view,
    self->category_tree_model,
    F_MULTI_SELECT, F_NO_DND);

  /* populate plugins */
  self->plugin_tree_model =
    GTK_TREE_MODEL_FILTER (
      create_model_for_plugins (self));
  tree_view_setup (
    self, self->plugin_tree_view,
    GTK_TREE_MODEL (
      self->plugin_tree_model),
    F_NO_MULTI_SELECT, F_DND);
  g_signal_connect (
    G_OBJECT (self->plugin_tree_view),
    "row-activated",
    G_CALLBACK (on_row_activated),
    self->plugin_tree_model);

  /* set the selected values */
  PluginBrowserTab tab =
    S_UI_GET_ENUM ("plugin-browser-tab");
  PluginBrowserFilter filter =
    S_UI_GET_ENUM ("plugin-browser-filter");
  restore_tree_view_selections (self);

  switch (tab)
    {
    case PLUGIN_BROWSER_TAB_COLLECTION:
      gtk_stack_set_visible_child (
        self->stack,
        GTK_WIDGET (self->collection_scroll));
      break;
    case PLUGIN_BROWSER_TAB_CATEGORY:
      gtk_stack_set_visible_child (
        self->stack,
        GTK_WIDGET (self->category_scroll));
      break;
    case PLUGIN_BROWSER_TAB_PROTOCOL:
      gtk_stack_set_visible_child (
        self->stack,
        GTK_WIDGET (self->protocol_scroll));
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  switch (filter)
    {
    case PLUGIN_BROWSER_FILTER_NONE:
      break;
    case PLUGIN_BROWSER_FILTER_INSTRUMENT:
      gtk_toggle_tool_button_set_active (
        self->toggle_instruments, 1);
      break;
    case PLUGIN_BROWSER_FILTER_EFFECT:
      gtk_toggle_tool_button_set_active (
        self->toggle_effects, 1);
      break;
    case PLUGIN_BROWSER_FILTER_MODULATOR:
      gtk_toggle_tool_button_set_active (
        self->toggle_modulators, 1);
      break;
    case PLUGIN_BROWSER_FILTER_MIDI_EFFECT:
      gtk_toggle_tool_button_set_active (
        self->toggle_midi_modifiers, 1);
      break;
    }

  /* set divider position */
  int divider_pos =
    g_settings_get_int (
      S_UI,
      "browser-divider-position");
  gtk_paned_set_position (
    GTK_PANED (self),
    divider_pos);
  self->first_time_position_set = 1;
  g_message (
    "setting plugin browser divider pos to %d",
    divider_pos);

  gtk_widget_add_events (
    GTK_WIDGET (self), GDK_STRUCTURE_MASK);

  /* notify when tab changes */
  g_signal_connect (
    G_OBJECT (self->stack), "notify::visible-child",
    G_CALLBACK (on_visible_child_changed), self);
  g_signal_connect (
    G_OBJECT (self), "notify::position",
    G_CALLBACK (on_position_change), self);
  g_signal_connect (
    G_OBJECT (self), "map-event",
    G_CALLBACK (on_map_event), self);

  return self;
}

static void
plugin_browser_widget_class_init (
  PluginBrowserWidgetClass * _klass)
{
  GtkWidgetClass * klass =
    GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "plugin_browser.ui");

  gtk_widget_class_set_css_name (
    klass, "browser");

#define BIND_CHILD(name) \
  gtk_widget_class_bind_template_child ( \
    klass, \
    PluginBrowserWidget, \
    name)

  BIND_CHILD (collection_scroll);
  BIND_CHILD (protocol_scroll);
  BIND_CHILD (category_scroll);
  BIND_CHILD (plugin_scroll);
  BIND_CHILD (collection_tree_view);
  BIND_CHILD (protocol_tree_view);
  BIND_CHILD (category_tree_view);
  BIND_CHILD (plugin_tree_view);
  BIND_CHILD (browser_bot);
  BIND_CHILD (plugin_info);
  BIND_CHILD (stack_switcher_box);
  BIND_CHILD (stack);
  BIND_CHILD (plugin_toolbar);
  BIND_CHILD (toggle_instruments);
  BIND_CHILD (toggle_effects);
  BIND_CHILD (toggle_modulators);
  BIND_CHILD (toggle_midi_modifiers);

#undef BIND_CHILD

#define BIND_SIGNAL(sig) \
  gtk_widget_class_bind_template_callback ( \
    klass, sig)

  BIND_SIGNAL (toggles_changed);

#undef BIND_SIGNAL
}

static void
plugin_browser_widget_init (
  PluginBrowserWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->stack_switcher =
    GTK_STACK_SWITCHER (gtk_stack_switcher_new ());
  gtk_stack_switcher_set_stack (
    self->stack_switcher,
    self->stack);
  gtk_widget_show_all (
    GTK_WIDGET (self->stack_switcher));
  gtk_box_pack_start (
    self->stack_switcher_box,
    GTK_WIDGET (self->stack_switcher),
    true, true, 0);

  /* set stackswitcher icons */
  GValue iconval1 = G_VALUE_INIT;
  GValue iconval2 = G_VALUE_INIT;
  GValue iconval3 = G_VALUE_INIT;
  g_value_init (&iconval1, G_TYPE_STRING);
  g_value_init (&iconval2, G_TYPE_STRING);
  g_value_init (&iconval3, G_TYPE_STRING);
  g_value_set_string (
    &iconval1, "favorite");
  g_value_set_string(
    &iconval2, "category");
  g_value_set_string(
    &iconval3, "protocol");

  gtk_container_child_set_property (
    GTK_CONTAINER (self->stack),
    GTK_WIDGET (self->collection_scroll),
    "icon-name", &iconval1);
  g_value_set_string (
    &iconval1, _("Collection"));
  gtk_container_child_set_property (
    GTK_CONTAINER (self->stack),
    GTK_WIDGET (self->collection_scroll),
    "title", &iconval1);
  gtk_container_child_set_property (
    GTK_CONTAINER (self->stack),
    GTK_WIDGET (self->category_scroll),
    "icon-name", &iconval2);
  g_value_set_string (
    &iconval2, _("Category"));
  gtk_container_child_set_property (
    GTK_CONTAINER (self->stack),
    GTK_WIDGET (self->category_scroll),
    "title", &iconval2);
  gtk_container_child_set_property (
    GTK_CONTAINER (self->stack),
    GTK_WIDGET (self->protocol_scroll),
    "icon-name", &iconval3);
  g_value_set_string (
    &iconval3, _("Protocol"));
  gtk_container_child_set_property (
    GTK_CONTAINER (self->stack),
    GTK_WIDGET (self->protocol_scroll),
    "title", &iconval3);
  g_value_unset (&iconval1);
  g_value_unset (&iconval2);
  g_value_unset (&iconval3);

  GList *children, *iter;
  children =
    gtk_container_get_children (
      GTK_CONTAINER (self->stack_switcher));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (!GTK_IS_RADIO_BUTTON (iter->data))
        continue;

      GtkWidget * radio = GTK_WIDGET (iter->data);
      g_object_set (
        radio, "hexpand", true, NULL);
    }
  g_list_free (children);

  self->current_collections =
    calloc (40000, sizeof (PluginCollection *));
  self->current_descriptors =
    calloc (40000, sizeof (PluginDescriptor *));
}
