/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
 *
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/** \file callbacks.h
    \brief Header info for the GUI callback functions
    \ingroup gerbv
*/

enum {
	CALLBACKS_SAVE_PROJECT_AS,
	CALLBACKS_SAVE_FILE_PS,
	CALLBACKS_SAVE_FILE_PDF,
	CALLBACKS_SAVE_FILE_SVG,
	CALLBACKS_SAVE_FILE_PNG,
	CALLBACKS_SAVE_FILE_RS274X,
	CALLBACKS_SAVE_FILE_DRILL,
	CALLBACKS_SAVE_FILE_RS274XM,
	CALLBACKS_SAVE_FILE_DRILLM,
	CALLBACKS_SAVE_LAYER_AS,
	
} CALLBACKS_SAVE_FILE_TYPE;

enum {
	LAYER_SELECTED =	-1,
	LAYER_ALL_ON =		-2,
	LAYER_ALL_OFF =		-3,
} toggle_layer;

void
callbacks_new_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_open_project_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_open_layer_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_revert_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_save_layer_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_save_project_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
                                        
void
callbacks_generic_save_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_print_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
callbacks_quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_fullscreen_toggled                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_show_toolbar_toggled                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_show_sidepane_toggled               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_toggle_layer_visibility_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_zoom_in_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_zoom_out_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_fit_to_window_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_analyze_active_gerbers_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_analyze_active_drill_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_control_gerber_options_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_online_manual_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_about_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
callbacks_bugs_activate (GtkMenuItem     *menuitem,
			 gpointer         user_data);

gboolean
callbacks_window_scroll_event(GtkWidget *widget, GdkEventScroll *event);

gboolean
callbacks_window_key_release_event (GtkWidget *widget, GdkEventKey *event);

gboolean
callbacks_window_key_press_event (GtkWidget *widget, GdkEventKey *event);

gboolean
callbacks_drawingarea_button_release_event (GtkWidget *widget, GdkEventButton *event);

gboolean
callbacks_drawingarea_button_press_event (GtkWidget *widget, GdkEventButton *event);

gboolean
callbacks_scrollbar_button_released (GtkWidget *widget, GdkEventButton *event);

gboolean
callbacks_scrollbar_button_pressed (GtkWidget *widget, GdkEventButton *event);

gboolean
callbacks_drawingarea_motion_notify_event (GtkWidget *widget, GdkEventMotion *event);

gboolean
callbacks_drawingarea_configure_event (GtkWidget *widget, GdkEventConfigure *event);

gboolean
callbacks_drawingarea_expose_event (GtkWidget *widget, GdkEventExpose *event);

void
callbacks_handle_log_messages(const gchar *log_domain,
		      GLogLevelFlags log_level,
		      const gchar *message, 
		      gpointer user_data);

void
callbacks_clear_messages_button_clicked  (GtkButton *button, gpointer   user_data);

void
callbacks_statusbar_unit_combo_box_changed (GtkComboBox *widget, gpointer user_data);

void
callbacks_viewmenu_units_changed (GtkCheckMenuItem *widget, gpointer user_data);

void
callbacks_viewmenu_rendertype_changed (GtkCheckMenuItem *widget, gpointer user_data);

void
callbacks_sidepane_render_type_combo_box_changed (GtkComboBox *widget, gpointer user_data);

void
callbacks_layer_tree_visibility_button_toggled (GtkCellRendererToggle *cell_renderer,
                                                        gchar *path,
                                                        gpointer user_data);
gboolean
callbacks_drawingarea_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event,
					gpointer user_data);
gboolean
callbacks_drawingarea_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event,
					gpointer user_data);

void 
callbacks_update_statusbar(void);

void
callbacks_update_statusbar_measured_distance (gdouble dx, gdouble dy);

void
callbacks_update_layer_tree (void);

gboolean
callbacks_layer_tree_key_press (GtkWidget *widget, GdkEventKey *event,
                                   gpointer user_data);

gboolean
callbacks_layer_tree_button_press (GtkWidget *widget, GdkEventButton *event,
                                   gpointer user_data);

void callbacks_add_layer_button_clicked  (GtkButton *button, gpointer   user_data);

void callbacks_remove_layer_button_clicked  (GtkButton *button, gpointer   user_data);

void callbacks_move_layer_down_menu_activate (GtkMenuItem *menuitem, gpointer user_data);

void callbacks_move_layer_down_button_clicked  (GtkButton *button, gpointer   user_data);

void callbacks_move_layer_up_menu_activate (GtkMenuItem *menuitem, gpointer user_data);

void callbacks_move_layer_up_button_clicked  (GtkButton *button, gpointer   user_data);

void callbacks_layer_tree_row_inserted (GtkTreeModel *tree_model, GtkTreePath  *path,
                              GtkTreeIter  *oIter, gpointer      user_data);
void
callbacks_invert_layer_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_display_object_properties_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_benchmark_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_edit_object_properties_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_move_objects_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_reduce_object_area_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_delete_objects_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_change_layer_orientation_clicked (GtkButton *button, gpointer   user_data);

void
callbacks_change_layer_color_clicked  (GtkButton *button, gpointer   user_data);

void
callbacks_change_background_color_clicked  (GtkButton *button, gpointer   user_data);

void
callbacks_reload_layer_clicked  (GtkButton *button, gpointer   user_data);

void
callbacks_change_layer_format_clicked  (GtkButton *button, gpointer   user_data);

void callbacks_update_scrollbar_limits (void);

void callbacks_update_scrollbar_positions (void);

void callbacks_hadjustment_value_changed (GtkAdjustment *adjustment,
			gpointer user_data);

void callbacks_vadjustment_value_changed (GtkAdjustment *adjustment,
			gpointer user_data);

void callbacks_force_expose_event_for_screen (void);

void
callbacks_change_tool  (GtkButton *button, gpointer   user_data);

void
callbacks_switch_to_correct_cursor (void);

void
callbacks_update_selected_object_message (gboolean userTriedToSelect);

cairo_surface_t *
callbacks_create_window_surface (GtkWidget *widget);

