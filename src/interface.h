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

/** \file interface.h
    \brief Header info for the GUI building functions for Gerber Viewer
    \ingroup gerbv
*/


void
interface_create_gui (int req_width, int req_height);

void
interface_set_render_type (int);

void rename_main_window(char const* filename, GtkWidget* main_win);

void
set_window_icon (GtkWidget * this_window);

gboolean
interface_get_alert_dialog_response (gchar *primaryText, 
				     gchar *secondaryText,
                                     gboolean show_checkbox, 
				     gboolean *ask_to_show_again );


