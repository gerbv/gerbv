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
#include "gerber.h"

#define INITIAL_SCALE 200
#define MAX_ERRMSGLEN 25
#define MAX_COORDLEN 28
#define MAX_DISTLEN 90
#define MAX_STATUSMSGLEN (MAX_ERRMSGLEN+MAX_COORDLEN+MAX_DISTLEN)

/* Macros to convert between unscaled gerber coordinates and other units */
/* XXX NOTE: Currently unscaled units are assumed as inch, this is not
   XXX necessarily true for all files */
#define COORD2MILS(c) ((c)*1000.0)
#define COORD2MMS(c) ((c)*25.4)

typedef enum {NORMAL, IN_MOVE, IN_ZOOM_OUTLINE, IN_MEASURE, ALT_PRESSED, SCROLLBAR} gerbv_state_t;
typedef enum {POINTER, ZOOM, MEASURE} gerbv_tool_t;
typedef enum {GERBV_MILS, GERBV_MMS, GERBV_INS} gerbv_unit_t;

typedef struct {
    gerb_image_t *image;
    GdkColor color;
    guint16 alpha;
    gboolean isVisible;
    gpointer privateRenderData;
    char *name; /* this should be the full pathname to the file */
    char inverted;
} gerbv_fileinfo_t;

typedef struct {
	double x1, y1;
	double x2, y2;
} gerbv_bbox_t;


typedef struct {
    GtkWidget *drawing_area;
    GdkPixmap *pixmap;
    GdkColor  background;
    GdkColor  zoom_outline_color;
    GdkColor  dist_measure_color;
    gerbv_unit_t unit;

    struct {
	GtkWidget *log;
	GtkWidget *topLevelWindow;
	GtkWidget *messageTextView;
	GtkWidget *statusMessageLeft;
	GtkWidget *statusMessageRight;
	GtkWidget *statusUnitComboBox;
	GtkWidget *layerTree;
	gboolean treeIsUpdating;
	GtkWidget *colorSelectionDialog;
	gint colorSelectionIndex;
	GtkWidget *hAdjustment;
	GtkWidget *vAdjustment;
	GtkWidget *hRuler;
	GtkWidget *vRuler;
	GtkWidget *sidepane_notebook;
	GtkWidget *project;
	GtkWidget *gerber;
	GtkWidget *about_dialog;
	GtkWidget *toolButtonPointer;
	GtkWidget *toolButtonZoom;
	GtkWidget *toolButtonMeasure;
	gboolean updatingTools;
    } win;
    gpointer windowSurface;
    gpointer bufferSurface;
    gerbv_fileinfo_t *file[MAX_FILES];
    int curr_index;
    int last_loaded;

    gchar *path;
    gchar *execpath;    /* Path to executed version of gerbv */
    gchar *project;     /* Current project to simplify save next time */

    GtkTooltips *tooltips;
    GtkWidget *popup_menu;
    struct {
	GtkWidget *msg;
	char msgstr[MAX_STATUSMSGLEN];
	char coordstr[MAX_COORDLEN];
	char diststr[MAX_DISTLEN];
    } statusbar;

    gerbv_state_t state;
    gerbv_tool_t tool;
    gboolean centered_outline_zoom;

    int selected_layer;         /* Selected layer by Alt+keypad */

    gint last_x;
    gint last_y;
    gint start_x;		/* Zoom box/measure start coordinates */
    gint start_y;

    gint off_x;			/* Offset current pixmap when panning */
    gint off_y;

    int dump_parsed_image;
} gerbv_screen_t;

typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, ZOOM_SET } gerbv_zoom_dir_t;
typedef struct {
    gerbv_zoom_dir_t z_dir;
    GdkEventButton *z_event;
    int scale;
} gerbv_zoom_data_t;

extern gerbv_screen_t screen;

#endif /* GERBV_SCREEN_H */
