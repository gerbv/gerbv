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

#ifndef GERBV_SCREEN_H
#define GERBV_SCREEN_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gerb_image.h"

#define INITIAL_SCALE 200
#define MAX_ERRMSGLEN 25
#define MAX_COORDLEN 28
#define MAX_DISTLEN 45
#define MAX_STATUSMSGLEN (MAX_ERRMSGLEN+MAX_COORDLEN+MAX_DISTLEN)

/* Macros to convert between unscaled gerber coordinates and other units */
/* XXX NOTE: Currently unscaled units are assumed as inch, this is not
   XXX necessarily true for all files */
#define COORD2MILS(c) ((c)*1000.0)
#define COORD2MMS(c) ((c)*25.4)

typedef enum {NORMAL, MOVE, ZOOM_OUTLINE, MEASURE, ALT_PRESSED} gerbv_state_t;

typedef enum {GERBV_MILS, GERBV_MMS} gerbv_unit_t;

typedef struct {
    gerb_image_t *image;
    GdkColor *color;
    char *name;
    char *basename;
} gerbv_fileinfo_t;

typedef struct {
	double x1, y1;
	double x2, y2;
} gerbv_bbox_t;


typedef struct {
    GtkWidget *drawing_area;
    GdkPixmap *pixmap;
    GdkColor  *background;
    GdkFunction si_func; /* Function used for superimposing layers */
    GdkColor  *zoom_outline_color;
    GdkColor  *dist_measure_color;
    gerbv_unit_t unit;

    struct {
	GtkWidget *load_file;
	GtkWidget *color_selection;
	GtkWidget *export_png;
	GtkWidget *scale;
	GtkWidget *log;
	GtkWidget *project;
    } win;

    gerbv_fileinfo_t *file[MAX_FILES];
    int curr_index;
    char *path;
    char *execpath;
    
    /* Bounding box for all loaded gerber images. Initialized by autoscale() */
    gerbv_bbox_t gerber_bbox;

    GtkTooltips *tooltips;
    GtkWidget *layer_button[MAX_FILES];
    GtkWidget *popup_menu;
    struct {
	GtkWidget *msg;
	char msgstr[MAX_ERRMSGLEN];
	char coordstr[MAX_COORDLEN];
	char diststr[MAX_DISTLEN];
    } statusbar;

    gerbv_state_t state;
    gboolean centered_outline_zoom;

    int scale;

    int selected_layer;         /* Selected layer by Alt+keypad */

    gint last_x;
    gint last_y;
    gint start_x;		/* Zoom box/measure start coordinates */
    gint start_y;

    int trans_x; /* Translate offset */
    int trans_y;

    gint off_x;			/* Offset current pixmap when panning */
    gint off_y;
    gerbv_bbox_t clip_bbox;	/* Clipping bounding box */
} gerbv_screen_t;

extern gerbv_screen_t screen;

#endif /* GERBV_SCREEN_H */
