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


void
on_new_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_open_project_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_open_layer_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_revert_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save_as_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_export_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_postscript_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_png_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_pdf_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_svg_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_print_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_zoom_in_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_zoom_out_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_fit_to_window_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_analyze_activelayers_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_validate_active_layers_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_control_gerber_options_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_pointer_tool_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_zoom_tool_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_measure_tool_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_online_manual_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
callback_window_scroll_event(GtkWidget *widget, GdkEventScroll *event);

gboolean
callback_window_key_release_event (GtkWidget *widget, GdkEventKey *event);

gboolean
callback_window_key_press_event (GtkWidget *widget, GdkEventKey *event);

gboolean
callback_drawingarea_button_release_event (GtkWidget *widget, GdkEventButton *event);

gboolean
callback_drawingarea_button_press_event (GtkWidget *widget, GdkEventButton *event);

gboolean
callback_drawingarea_motion_notify_event (GtkWidget *widget, GdkEventMotion *event);

gboolean
callback_drawingarea_configure_event (GtkWidget *widget, GdkEventConfigure *event);

gboolean
callback_drawingarea_expose_event (GtkWidget *widget, GdkEventExpose *event);

void
cb_ok_project(GtkWidget *widget, gpointer data);

void
project_save_cb(GtkWidget *widget, gpointer data);

void
unload_file(GtkWidget *widget, gpointer data);

void
reload_files(GtkWidget *widget, gpointer data);

void 
update_statusbar(gerbv_screen_t *scr);

void
export_png_popup(GtkWidget *widget, gpointer data);

void
cb_ok_export_png(GtkWidget *widget, GtkFileSelection *fs);

void
cb_cancel_load_file(GtkWidget *widget, gpointer data);

void
load_file_popup(GtkWidget *widget, gpointer data);

void 
update_statusbar(gerbv_screen_t *scr);

