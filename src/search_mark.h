/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
 *
 * $Id$
 *  
 *                                      Juergen H. (juergenhaas@gmx.net) and
 *                                      Tomasz M. (T.Motylewski@bfad.de)
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

#ifndef _SEARCH_MARK_H
#define _SEARCH_MARK_H
#ifdef USE_GTK2
#include "search.h"
#include "search_file.h"
#include "search_gui.h"
#endif


/* Keep redraw state when preempting to process certain events */
struct gerbv_redraw_state {
    int valid;			/* Set to nonzero when data is valid */
    int file_index;
    GdkPixmap *curr_pixmap;
    GdkPixmap *clipmask;
    int max_width;
    int max_height;
    int files_loaded;
};

#ifdef USE_GTK2
extern
GtkListStore        *combo_box_model;
#endif

extern 
gint
redraw_pixmap(GtkWidget *widget, int restart);

extern
void create_marked_layer(int idx);
extern
void update_statusbar(gerbv_screen_t *scr);
extern
void autoscale(void);
extern
void invalidate_redraw_state(struct gerbv_redraw_state *state);
extern
void start_idle_redraw_pixmap(GtkWidget *data);
extern
void stop_idle_redraw_pixmap(GtkWidget *data);

#endif
