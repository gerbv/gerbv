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

/** \file main.h
    \brief Header info for common structs and functions used for the GUI application
    \ingroup gerbv
*/

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {GERBV_MILS, GERBV_MMS, GERBV_INS} gerbv_gui_unit_t;
typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, ZOOM_SET } gerbv_zoom_dir_t;
typedef enum {NORMAL, IN_MOVE, IN_ZOOM_OUTLINE, IN_MEASURE, ALT_PRESSED,
		IN_SELECTION_DRAG, SCROLLBAR} gerbv_state_t;
typedef enum {POINTER, PAN, ZOOM, MEASURE} gerbv_tool_t;

typedef struct {
    GtkWidget *drawing_area;
    GdkPixmap *pixmap;
    GdkColor  zoom_outline_color;
    GdkColor  dist_measure_color;
    GdkColor  selection_color;

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
	GtkWidget *toolButtonPan;
	GtkWidget *toolButtonZoom;
	GtkWidget *toolButtonMeasure;
	gboolean updatingTools;
	GtkWidget *layerTreePopupMenu;
	GtkWidget *drawWindowPopupMenu;
	gdouble lastMeasuredX;
	gdouble lastMeasuredY;
    } win;
    
    gpointer windowSurface;
    gpointer bufferSurface;
    gpointer selectionRenderData;

    GtkTooltips *tooltips;
    GtkWidget *popup_menu;
    struct {
	GtkWidget *msg;
	char msgstr[MAX_STATUSMSGLEN];
	char coordstr[MAX_COORDLEN];
	char diststr[MAX_DISTLEN];
    } statusbar;

    gboolean centered_outline_zoom;

    int selected_layer;         /* Selected layer by Alt+keypad */
    gerbv_selection_info_t selectionInfo;
    gerbv_state_t state;
    gerbv_tool_t tool;
    gerbv_gui_unit_t unit;
           
    gint last_x;
    gint last_y;
    gint start_x;		/* Zoom box/measure start coordinates */
    gint start_y;

    gint off_x;			/* Offset current pixmap when panning */
    gint off_y;

    int dump_parsed_image;
} gerbv_screen_t;

extern gerbv_screen_t screen;
extern gerbv_project_t *mainProject;

void 
main_save_as_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename);

void 
main_save_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename);

void 
main_open_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename);
#endif /* GERBV_H */

