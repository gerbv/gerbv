/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <libgen.h> /* dirname */

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif


#include "gerber.h"
#include "drill.h"
#ifdef GUILE_IN_USE
#include "scm_gerber.h"
#endif /* GUILE_IN_USE */
#include "draw.h"
#include "color.h"
#include "gerbv_screen.h"
#ifdef EXPORT_PNG
#include "exportimage.h"
#endif /* EXPORT_PNG */

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif

typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE} gerbv_zoom_dir_t;
typedef struct {
    gerbv_zoom_dir_t z_dir;
    GdkEventButton *z_event;
} gerbv_zoom_data_t;


/*
 * Declared extern in gerbv_screen.h. Global state variable to keep track
 * of what's happening on the screen.
 */
gerbv_screen_t screen;


static gint expose_event (GtkWidget *widget, GdkEventExpose *event);
static void draw_zoom_outline(gboolean centered);
static void draw_measure_distance();
static void color_selection_popup(GtkWidget *widget, gpointer data);
static void load_file_popup(GtkWidget *widget, gpointer data);
#ifdef EXPORT_PNG
static void export_png_popup(GtkWidget *widget, gpointer data);
#endif /* EXPORT_PNG */
static void unload_file(GtkWidget *widget, gpointer data);
static void reload_files(GtkWidget *widget, gpointer data);
static void menu_zoom(GtkWidget *widget, gpointer data);
static void zoom(GtkWidget *widget, gpointer data);
static void zoom_outline(GtkWidget *widget, GdkEventButton *event);
static gint redraw_pixmap(GtkWidget *widget);
static void open_image(char *filename, int idx, int reload);


void
destroy(GtkWidget *widget, gpointer data)
{
    int i;

    for (i = 0; i < MAX_FILES; i++) {
	if (screen.file[i] != NULL && 
	    screen.file[i]->image != NULL) {
	    free_gerb_image(screen.file[i]->image);
	    free(screen.file[i]->name);
	    free(screen.file[i]);
	}
    }
    gtk_main_quit();
} /* destroy */


static GtkItemFactoryEntry menu_items[] = {
    {"/_File",               NULL,     NULL,             0, "<Branch>"},
#ifdef EXPORT_PNG
    {"/File/_Export",        NULL,     NULL,             0, "<Branch>"},
    {"/File/_Export/PNG...", NULL,     export_png_popup, 0, NULL},
    {"/File/sep1",           NULL,     NULL,             0, "<Separator>"},
#endif /* EXPORT_PNG */
    {"/File/Reload",         "<alt>R", reload_files,     0, NULL},
    {"/File/sep2",           NULL,     NULL,             0, "<Separator>"},
    {"/File/_Quit",          "<alt>Q", destroy,          0, NULL},
    {"/_Zoom",               NULL,     NULL,             0, "<Branch>"},
    {"/Zoom/_Out",           "<alt>O", menu_zoom,        ZOOM_OUT, NULL},
    {"/Zoom/_In",            "<alt>I", menu_zoom,        ZOOM_IN, NULL},
    {"/Zoom/sep3",           NULL,     NULL,             0,  "<Separator>"},
    {"/Zoom/_Fit",           NULL,     menu_zoom,        ZOOM_FIT, NULL},
    {"/_Setup",              NULL,     NULL,             0, "<Branch>"},
    {"/Setup/_Background Color", NULL, color_selection_popup, 1, NULL},
};

static GtkItemFactoryEntry popup_menu_items[] = {
    {"/Layer Color...", NULL, color_selection_popup, 0, NULL},
    {"/Load File...", NULL, load_file_popup, 0, NULL},
    {"/Unload File...", NULL, unload_file, 0, NULL},
};


void
create_menubar(GtkWidget *window, GtkWidget **menubar)
{
    int nmenu_items = sizeof(menu_items) / sizeof(menu_items[0]);
    GtkItemFactory *item_factory;
    GtkAccelGroup  *accel_group;
    
    accel_group = gtk_accel_group_new();

    /* This function initializes the item factory.
       Param 1: The type of menu - can be GTK_TYPE_MENU_BAR, GTK_TYPE_MENU,
       or GTK_TYPE_OPTION_MENU.
       Param 2: The path of the menu.
       Param 3: A pointer to a gtk_accel_group. The item factory sets up
       the accelerator table while generating menus.
    */
    
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", 
					accel_group);
    /* This function generates the menu items. Pass the item factory,
       the number of items in the array, the array itself, and any
       callback data for the menu items. */
    gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
    
    /* Attach the new accelerator group to the window */
    gtk_accel_group_attach(accel_group, GTK_OBJECT(window));
    
    if(menubar)
	/* Finally, return the actual menu bar created by the item factory. */
	*menubar = gtk_item_factory_get_widget(item_factory, "<main>");
} /* create_menubar */


/*
 * By redesigning create_menubar we can probably do without this function
 */
static void
create_popupmenu(GtkWidget **popupmenu)
{
    int nmenu_items = sizeof(popup_menu_items) / sizeof(popup_menu_items[0]);
    GtkItemFactory *item_factory;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<popup>", NULL);
    gtk_item_factory_create_items(item_factory, nmenu_items, popup_menu_items,
				  NULL);
    
    if(popupmenu)
	/* Finally, return the actual menu bar created by the item factory. */
	*popupmenu = gtk_item_factory_get_widget(item_factory, "<popup>");

    return;
} /* create_popupmenu */


static void
cb_layer_button(GtkWidget *widget, gpointer data)
{
    
    screen.curr_index = (long int)data;

    /* Redraw the image(s) */
    redraw_pixmap(screen.drawing_area);

} /* cb_layer_button */


static gint
layer_button_press_event(GtkWidget *widget, GdkEventButton *event, 
			 gpointer button)
{
    GdkEventButton *event_button;

    if (event->type != GDK_BUTTON_PRESS) {
	return FALSE;
    }

    event_button = (GdkEventButton *) event;

    /*
     * Only button 3 accepted
     */
    if (event_button->button != 3) {
	return FALSE;
    }

    gtk_menu_popup(GTK_MENU(screen.popup_menu), NULL, NULL, NULL, NULL, 
		   event_button->button, event_button->time);
    
    screen.curr_index = (long int)button;

    return TRUE;
} /*layer_button_press_event */


static GtkWidget *
create_layer_buttons(int nuf_buttons)
{
    GtkWidget *button = NULL;
    GtkWidget *box = NULL;
    char      info[5];
    long int  bi;

    box = gtk_vbox_new(TRUE, 0);

    for (bi = 0; bi < nuf_buttons; bi++) {
	sprintf(info, "%ld", bi);
	button = gtk_toggle_button_new_with_label(info);

	gtk_signal_connect(GTK_OBJECT(button), "toggled", 
			   GTK_SIGNAL_FUNC(cb_layer_button),
			   (gpointer)bi);
	gtk_signal_connect(GTK_OBJECT(button), "button_press_event",
			   GTK_SIGNAL_FUNC(layer_button_press_event), 
			   (gpointer)bi);

	gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
	screen.layer_button[bi] = button;
    }

    return box;
} /* create_layer_buttons */


static void
color_selection_destroy(GtkWidget *widget, gpointer data)
{
    gtk_grab_remove(widget);
} /* color_selection_destroy */


static void
color_selection_ok(GtkWidget *widget, gpointer data)
{
    int background = (int)data;
    GtkColorSelection *colorsel;
    gdouble color[4];
    GtkStyle *oldstyle, *newstyle;

    /* Get selected color */
    colorsel = GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG
				   (screen.color_selection_popup)->colorsel);
    gtk_color_selection_get_color(colorsel, color);

    /* Allocate new color  */
    if (background)
	screen.background = 
	    alloc_color((int)(color[0] * MAX_COLOR_RESOLUTION),
			(int)(color[1] * MAX_COLOR_RESOLUTION),
			(int)(color[2] * MAX_COLOR_RESOLUTION),
			NULL);
    else
	screen.file[screen.curr_index]->color = 
	    alloc_color((int)(color[0] * MAX_COLOR_RESOLUTION), 
			(int)(color[1] * MAX_COLOR_RESOLUTION), 
			(int)(color[2] * MAX_COLOR_RESOLUTION), 
			NULL);

    /* Redraw image on screen */
    redraw_pixmap(screen.drawing_area);

    /* Change color on button too */
    if (!background) {
	oldstyle = gtk_widget_get_style(screen.layer_button[screen.curr_index]);
	newstyle = gtk_style_copy(oldstyle);
	newstyle->bg[GTK_STATE_NORMAL] = *(screen.file[screen.curr_index]->color);
	newstyle->bg[GTK_STATE_ACTIVE] = *(screen.file[screen.curr_index]->color);
	newstyle->bg[GTK_STATE_PRELIGHT] = *(screen.file[screen.curr_index]->color);
	gtk_widget_set_style(screen.layer_button[screen.curr_index], newstyle);
    }

    /* Remove modal grab and destroy color selection dialog */
    gtk_grab_remove(screen.color_selection_popup);
    gtk_widget_destroy(screen.color_selection_popup);
    screen.color_selection_popup = NULL;

    return;
} /* cb_ok_color_selection */


static void
color_selection_popup(GtkWidget *widget, gpointer data)
{
    gdouble curr_color[4];
    int background = (int)data;

    if (!background && (screen.file[screen.curr_index] == NULL))
	return;

    screen.color_selection_popup = gtk_color_selection_dialog_new("Color Selection");

    gtk_color_selection_set_update_policy
	(GTK_COLOR_SELECTION
	 (GTK_COLOR_SELECTION_DIALOG(screen.color_selection_popup)->colorsel),
	 GTK_UPDATE_CONTINUOUS);

    gtk_signal_connect_object
	(GTK_OBJECT
	 (GTK_COLOR_SELECTION_DIALOG(screen.color_selection_popup)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 GTK_OBJECT(screen.color_selection_popup));

    gtk_signal_connect
	(GTK_OBJECT
	 (GTK_COLOR_SELECTION_DIALOG(screen.color_selection_popup)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(color_selection_ok), data);

    gtk_signal_connect(GTK_OBJECT(screen.color_selection_popup), "destroy",
		       GTK_SIGNAL_FUNC(color_selection_destroy),
		       NULL);

    /* Get current color and use it as a start in color selection dialog */
    if (background) {
	curr_color[0] = (gdouble)screen.background->red / 
	    (gdouble)MAX_COLOR_RESOLUTION;
	curr_color[1] = (gdouble)screen.background->green / 
	    (gdouble)MAX_COLOR_RESOLUTION;
	curr_color[2] = (gdouble)screen.background->blue / 
	    (gdouble)MAX_COLOR_RESOLUTION;
	curr_color[3] = 0.0; /* Actually don't know how to get this value */
    }else {
	curr_color[0] = (gdouble)screen.file[screen.curr_index]->color->red / 
	    (gdouble)MAX_COLOR_RESOLUTION;
	curr_color[1] = (gdouble)screen.file[screen.curr_index]->color->green /
	    (gdouble)MAX_COLOR_RESOLUTION;
	curr_color[2] = (gdouble)screen.file[screen.curr_index]->color->blue / 
	    (gdouble)MAX_COLOR_RESOLUTION;
	curr_color[3] = 0.0; /* Actually don't know how to get this value */
    }

    /* Now set this color in color selection dialog */
    gtk_color_selection_set_color
	(GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG
			     (screen.color_selection_popup)->colorsel),
	 curr_color);

    /* Display widget */
    gtk_widget_show(screen.color_selection_popup);

    /* Make widget modal */
    gtk_grab_add(screen.color_selection_popup);

    return;
} /* color_selection_popup */


static void
cb_ok_load_file(GtkWidget *widget, GtkFileSelection *fs)
{
    char *filename;

    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
    open_image(filename, screen.curr_index, FALSE);

    /*
     * Remember where we loaded file from last time
     */
    filename = dirname(filename);
    if (screen.path)
	free(screen.path);
    screen.path = (char *)malloc(strlen(filename) + 1);
    strcpy(screen.path, filename);
    screen.path = strncat(screen.path, "/", 1);
    
    /* Make loaded image appear on screen */
    redraw_pixmap(screen.drawing_area);

    gtk_grab_remove(screen.load_file_popup);
    
    screen.load_file_popup = NULL;

    return;
} /* cb_ok_load_file */


static void
load_file_popup(GtkWidget *widget, gpointer data)
{
    screen.load_file_popup = gtk_file_selection_new("Select File To View");

    if (screen.path)
	gtk_file_selection_set_filename
	    (GTK_FILE_SELECTION(screen.load_file_popup), screen.path);

    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.load_file_popup)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_load_file), 
	 (gpointer)screen.load_file_popup);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.load_file_popup)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.load_file_popup);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.load_file_popup)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.load_file_popup);
    
    gtk_widget_show(screen.load_file_popup);

    gtk_grab_add(screen.load_file_popup);
    
    return;
} /* load_file_popup */


#ifdef EXPORT_PNG
static void
cb_ok_export_png(GtkWidget *widget, GtkFileSelection *fs)
{
    char *filename;
    gboolean result;
    GdkWindow *window;

    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

    /* This might be lengthy, show that we're busy by changing the pointer */
    window = gtk_widget_get_parent_window(widget);
    if (window) {
	GdkCursor *cursor;

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
    }

    /* Export PNG */
    result = png_export(screen.pixmap, filename);
    if (!result) {
	fprintf(stderr, "Failed to save PNG at %s\n", filename);
    }

    /* Return default pointer shape */
    if (window) {
	gdk_window_set_cursor(window, GERBV_DEF_CURSOR);
    }

    /*
     * Remember where we loaded file from last time
     */
    filename = dirname(filename);
    if (screen.path)
	free(screen.path);
    screen.path = (char *)malloc(strlen(filename) + 1);
    strcpy(screen.path, filename);
    screen.path = strncat(screen.path, "/", 1);
   
    gtk_grab_remove(screen.export_png_popup);
    
    screen.export_png_popup = NULL;

    return;
} /* cb_ok_export_png */


static void
export_png_popup(GtkWidget *widget, gpointer data)
{
    screen.export_png_popup = gtk_file_selection_new("Save PNG filename");

    if (screen.path)
	gtk_file_selection_set_filename
	    (GTK_FILE_SELECTION(screen.export_png_popup), screen.path);

    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.export_png_popup)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_export_png), 
	 (gpointer)screen.export_png_popup);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.export_png_popup)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.export_png_popup);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.export_png_popup)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.export_png_popup);
    
    gtk_widget_show(screen.export_png_popup);

    gtk_grab_add(screen.export_png_popup);
    
    return;
} /* export_png_popup */

#endif /* EXPORT_PNG */

static void
unload_file(GtkWidget *widget, gpointer data)
{
    int idx = screen.curr_index;
    GtkStyle *defstyle;

    if (screen.file[idx] == NULL)
	return;

    /*
     * Deselect the layer we're unloading so it's not left in the pixmap
     */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(screen.layer_button[idx]),
				 FALSE);

    /* 
     * Remove color on layer button (set default style) 
     */
    defstyle = gtk_widget_get_default_style();
    gtk_widget_set_style(screen.layer_button[idx], defstyle);

    /* 
     * Remove tool tips 
     */
    gtk_tooltips_set_tip(screen.tooltips, screen.layer_button[idx], 
			 NULL, NULL); 

    /* 
     * Remove data struct 
     */
    free_gerb_image(screen.file[idx]->image);  screen.file[idx]->image = NULL;
    free(screen.file[idx]->color);  screen.file[idx]->color = NULL;
    free(screen.file[idx]->name);  screen.file[idx]->name = NULL;
    free(screen.file[idx]);  screen.file[idx] = NULL;

    return;
} /* unload_file */


static void
reload_files(GtkWidget *widget, gpointer data)
{
    int idx;

    for (idx = 0; idx < MAX_FILES; idx++) {
	if (screen.file[idx] && screen.file[idx]->name) {
	    open_image(screen.file[idx]->name, idx, TRUE);
	}
    }
    
    redraw_pixmap(screen.drawing_area);

    return;
} /* reload_files */


static void
autoscale()
{
    double max_width = LONG_MIN, max_height = LONG_MIN;
    double x_scale, y_scale;
    int i, first = 1;
    
    if (screen.drawing_area == NULL)
	return;

    for(i = 0; i < MAX_FILES; i++) {
        if (screen.file[i]) {
            
            /* 
             * Find the biggest image and use as a size reference
             */
	    if (first) {
		screen.gerber_bbox.x1 = screen.file[i]->image->info->min_x;
		screen.gerber_bbox.y1 = screen.file[i]->image->info->min_y;
		screen.gerber_bbox.x2 = screen.file[i]->image->info->max_x;
		screen.gerber_bbox.y2 = screen.file[i]->image->info->max_y;
		first = 0;
	    } else {
		screen.gerber_bbox.x1 = MIN(screen.gerber_bbox.x1,
					    screen.file[i]->image->info->min_x);
		screen.gerber_bbox.y1 = MIN(screen.gerber_bbox.y1,
					    screen.file[i]->image->info->min_y);
		screen.gerber_bbox.x2 = MAX(screen.gerber_bbox.x2,
					    screen.file[i]->image->info->max_x);
		screen.gerber_bbox.y2 = MAX(screen.gerber_bbox.y2,
					    screen.file[i]->image->info->max_y);
	    }
        }
    }

    max_width = screen.gerber_bbox.x2 - screen.gerber_bbox.x1;
    max_height = screen.gerber_bbox.y2 - screen.gerber_bbox.y1;

    /*
     * Calculate scale for both x axis and y axis
     */
    x_scale = screen.drawing_area->allocation.width / max_width;
    y_scale = screen.drawing_area->allocation.height / max_height;

    /*
     * Take the scale that fits both directions with some extra checks
     */
    screen.scale = (int)ceil(MIN(x_scale, y_scale));
    if (screen.scale < 1)
	screen.scale = 1;
    if (screen.scale > 10)
	screen.scale = (screen.scale / 10) * 10;

    /*
     * "Calculate" translation
     */
    screen.trans_x = 0;
    screen.trans_y = 0;

    return;
} /* autoscale */


static gboolean idle_redraw_pixmap_active = FALSE;
/*
 * On idle callback to ensure a zoomed image is properly redrawn
 */
gboolean
idle_redraw_pixmap(gpointer data)
{
    idle_redraw_pixmap_active = FALSE;
    redraw_pixmap((GtkWidget *) data);
    return FALSE;
} /* idle_redraw_pixmap */


void start_idle_redraw_pixmap(GtkWidget *data)
{
    if (!idle_redraw_pixmap_active) {
	gtk_idle_add(idle_redraw_pixmap, (gpointer) data);
	idle_redraw_pixmap_active = TRUE;
    }
} /* start_idle_redraw_pixmap */


void stop_idle_redraw_pixmap(GtkWidget *data)
{
    if (idle_redraw_pixmap_active) {
	gtk_idle_remove_by_data ((gpointer)data);
	idle_redraw_pixmap_active = FALSE;
    }
} /* stop_idle_redraw_pixmap */


static void
menu_zoom(GtkWidget *widget, gpointer data)
{
    gerbv_zoom_data_t z_data = { (gerbv_zoom_dir_t)data, NULL };

    zoom(widget, &z_data);
} /* menu_zoom */


static void
zoom(GtkWidget *widget, gpointer data)
{
    double us_midx, us_midy;	/* unscaled translation for screen center */
    int half_w, half_h;		/* cache for half window dimensions */
    int mouse_delta_x = 0;	/* Delta for mouse to window center */
    int mouse_delta_y = 0;
    gerbv_zoom_data_t *z_data = (gerbv_zoom_data_t *) data;

    if (screen.file[screen.curr_index] == NULL)
	return;

    half_w = screen.drawing_area->allocation.width / 2;
    half_h = screen.drawing_area->allocation.height / 2;

    if (z_data->z_dir == ZOOM_IN_CMOUSE ||
	z_data->z_dir == ZOOM_OUT_CMOUSE) {
	mouse_delta_x = half_w - z_data->z_event->x;
	mouse_delta_y = half_h - z_data->z_event->y;
	screen.trans_x -= mouse_delta_x;
	screen.trans_y -= mouse_delta_y;
    }

    us_midx = (screen.trans_x + half_w)/(double) screen.scale;
    us_midy = (screen.trans_y + half_h)/(double) screen.scale;

    switch(z_data->z_dir) {
    case ZOOM_IN : /* Zoom In */
    case ZOOM_IN_CMOUSE : /* Zoom In Around Mouse Pointer */
	screen.scale += screen.scale/10;
	screen.trans_x = screen.scale * us_midx - half_w;
	screen.trans_y = screen.scale * us_midy - half_h;
	break;
    case ZOOM_OUT :  /* Zoom Out */
    case ZOOM_OUT_CMOUSE : /* Zoom Out Around Mouse Pointer */
	if (screen.scale > 10) {
	    screen.scale -= screen.scale/10;
	    screen.trans_x = screen.scale * us_midx - half_w;
	    screen.trans_y = screen.scale * us_midy - half_h;
	}
	break;
    case ZOOM_FIT : /* Zoom Fit */
	autoscale();
	break;
    default :
	fprintf(stderr, "Illegal zoom direction %ld\n", (long int)data);
    }

    if (z_data->z_dir == ZOOM_IN_CMOUSE ||
	z_data->z_dir == ZOOM_OUT_CMOUSE) {
	screen.trans_x += mouse_delta_x;
	screen.trans_y += mouse_delta_y;
    }

    /* Redraw screen unless we have more events to process */
    if (!g_main_pending()) {
	stop_idle_redraw_pixmap(screen.drawing_area);
	redraw_pixmap(screen.drawing_area);
    } else {
	/* Set idle function to ensure we wont miss to redraw */
	start_idle_redraw_pixmap(screen.drawing_area);
    }
    
    return;
} /* zoom */


static void
zoom_outline(GtkWidget *widget, GdkEventButton *event)
{
    int x1, y1, x2, y2, dx, dy;	/* Zoom outline (UR and LL corners) */
    double us_x1, us_y1, us_x2, us_y2;
    int half_w, half_h;		/* cache for half window dimensions */

    if (screen.file[screen.curr_index] == NULL)
	return;

    half_w = screen.drawing_area->allocation.width / 2;
    half_h = screen.drawing_area->allocation.height / 2;

    x1 = MIN(screen.start_x, event->x);
    y1 = MIN(screen.start_y, event->y);
    x2 = MAX(screen.start_x, event->x);
    y2 = MAX(screen.start_y, event->y);
    dx = x2-x1;
    dy = y2-y1;

    if (dx < 4 && dy < 4) {
	    fprintf(stderr, "Warning: Zoom area too small, bailing out!\n");
	    goto zoom_outline_end;
    }

    if (screen.centered_outline_zoom) {
	/* Centered outline mode */
	x1 = screen.start_x - dx;
	y1 = screen.start_y - dy;
	dx *= 2;
	dy *= 2;
    }

    us_x1 = (screen.trans_x + x1)/(double) screen.scale;
    us_y1 = (screen.trans_y + y1)/(double) screen.scale;
    us_x2 = (screen.trans_x + x2)/(double) screen.scale;
    us_y2 = (screen.trans_y + y2)/(double) screen.scale;

    screen.scale = MIN(screen.drawing_area->allocation.width/(double)(us_x2 - us_x1),
		       screen.drawing_area->allocation.height/(double)(us_y2 - us_y1));
    screen.trans_x = screen.scale * (us_x1 + (us_x2 - us_x1)/2) - half_w;
    screen.trans_y = screen.scale * (us_y1 + (us_y2 - us_y1)/2) - half_h;;

zoom_outline_end:
    /* Redraw screen unless we have more events to process */
    if (!g_main_pending()) {
	stop_idle_redraw_pixmap(screen.drawing_area);
	redraw_pixmap(screen.drawing_area);
    } else {
	/* Set idle function to ensure we wont miss to redraw */
	start_idle_redraw_pixmap(screen.drawing_area);
    }
    
} /* zoom_outline */


GtkWidget *
create_drawing_area(gint win_width, gint win_height)
{
    GtkWidget *drawing_area;
    
    drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area), win_width, win_height);
    
    return drawing_area;
} /* create_drawing_area */


/* Create a new backing pixmap of the appropriate size */
static gint
redraw_pixmap(GtkWidget *widget)
{
    int i;
    int background_polarity = POSITIVE;
    int last_negative = 0;
    double dmax_x = LONG_MIN, dmax_y = LONG_MIN;
    double dmin_x = LONG_MAX, dmin_y = LONG_MAX;
    int max_width = 0, max_height = 0;
    GdkGC *gc = gdk_gc_new(widget->window);
    GdkRectangle update_rect;
    int file_loaded = 0;
    GdkWindow *window;
    int retval = TRUE;

    window = gtk_widget_get_parent_window(widget);
    /* This might be lengthy, show that we're busy by changing the pointer */
    if (window) {
	GdkCursor *cursor;

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
    }

    /* Called first when opening window and then when resizing window */

    for(i = 0; i < MAX_FILES; i++) {
	if (g_main_pending()) {
	    /* Set idle function to ensure we wont miss to redraw */
	    start_idle_redraw_pixmap(screen.drawing_area);
	    goto redraw_pixmap_end;
	}
	if (screen.file[i]) {
	    file_loaded = 1;
	    /* 
	     * Find the biggest image and use as a size reference
	     */
	    dmax_x  = MAX(screen.file[i]->image->info->max_x, dmax_x);
	    dmax_y = MAX(screen.file[i]->image->info->max_y, dmax_y);
	    
	    /*
	     * Also find the smallest coordinates to see if we have negative
	     * ones that must be compensated for.
	     */
	    dmin_x  = MIN(screen.file[i]->image->info->min_x, dmin_x);
	    dmin_y = MIN(screen.file[i]->image->info->min_y, dmin_y);

	    /* 
	     * Find out if any active layer is negative and 
	     * what the last negative layer is 
	     */
	    if (screen.file[i]->image->info->polarity == NEGATIVE &&
		GTK_TOGGLE_BUTTON(screen.layer_button[i])->active) {
		last_negative = i;
		background_polarity = NEGATIVE;
	    }
	}
    }

    /*
     * Paranoia check; size in width or height is zero
     */
    if ((abs(dmax_x - dmin_x) < 0.0001) || 
	(abs(dmax_y - dmin_y) < 0.0001)) {
	retval = FALSE;
	goto redraw_pixmap_end;
    }

    /*
     * Setup scale etc first time we load a file
     */
    if (file_loaded && screen.scale == 0) autoscale();

    /*
     * Scale width to actual windows size. Make picture a little bit 
     * bigger so things on the edges comes inside. Actual width is
     * abs(max) + abs(min) -> max - min. Same with height.
     */
    max_width = (int)floor((dmax_x - dmin_x + 0.1) * 
			   (double)screen.scale);
    max_height = (int)floor((dmax_y - dmin_y + 0.1) * 
			    (double)screen.scale);

    if (background_polarity == NEGATIVE) {
	/* Set background to normal color for the last negative layer */
	gdk_gc_set_foreground(gc, screen.file[last_negative]->color);
    } else {
	/* Background to black */
	gdk_gc_set_foreground(gc, screen.background);
    }

    /* 
     * Remove old pixmap, allocate a new one and draw the background
     */
    if (screen.pixmap) 
	gdk_pixmap_unref(screen.pixmap);
    screen.pixmap = gdk_pixmap_new(widget->window, max_width, max_height,  -1);
    gdk_draw_rectangle(screen.pixmap, gc, TRUE, 0, 0, max_width, max_height);
    
    /* 
     * This now allows drawing several layers on top of each other.
     * Higher layer numbers have higher priority in the Z-order. 
     */
    for(i = 0; i < MAX_FILES; i++) {
	/* Do some events so we don't lag to much behind */
	gtk_main_iteration_do(FALSE);

	if (g_main_pending()) {
	    /* Set idle function to ensure we wont miss to redraw */
	    start_idle_redraw_pixmap(screen.drawing_area);
	    goto redraw_pixmap_end;
	}
	if (GTK_TOGGLE_BUTTON(screen.layer_button[i])->active &&
	    screen.file[i]) {
	    /*
	     * Translation is to get it inside the allocated pixmap,
	     * which is not always centered perfectly for GTK/X.
	     */
	    image2pixmap(&screen.pixmap, screen.file[i]->image, screen.scale, 
			 -dmin_x * screen.scale,dmax_y * screen.scale,
			 screen.file[i]->image->info->polarity,
			 screen.file[i]->color,
			 screen.background, screen.err_color);
	}
    }

    update_rect.x = 0, update_rect.y = 0;
    update_rect.width =	widget->allocation.width;
    update_rect.height = widget->allocation.height;

    /*
     * Calls expose_event
     */
    gtk_widget_draw(widget, &update_rect);

redraw_pixmap_end:
    /* Return default pointer shape */
    if (window) {
	gdk_window_set_cursor(window, GERBV_DEF_CURSOR);
    }

    gdk_gc_unref(gc);

    return retval;
} /* redraw_pixmap */


static void
open_image(char *filename, int idx, int reload)
{
    gerb_file_t *fd;
    int r, g, b;
    GtkStyle *defstyle, *newstyle;
    gerb_verify_error_t error = GERB_IMAGE_OK;

    if (idx >= MAX_FILES) {
	fprintf(stderr, "Couldn't open %s. Maximum number of files opened.\n",
		filename);
	return;
    }

    fd = gerb_fopen(filename);
    if (fd == NULL) {
	fprintf(stderr, "Trying to open %s: ", filename);
	perror("");
	return;
    }
    
    if (reload && screen.file[idx]) {
	free_gerb_image(screen.file[idx]->image);  
	screen.file[idx]->image = NULL;
    } else {
	screen.file[idx] = (gerbv_fileinfo_t *)malloc(sizeof(gerbv_fileinfo_t));
    }
    
    if(drill_file_p(fd))
	screen.file[idx]->image = parse_drillfile(fd);
    else 
	screen.file[idx]->image = parse_gerb(fd);

    gerb_fclose(fd);
    
    /*
     * Do error check before continuing
     */
    error = gerb_image_verify(screen.file[idx]->image);
    if (error) {
	fprintf(stderr, "%s: Parse error: ", filename);
	if (error & GERB_IMAGE_MISSING_NETLIST)
	    fprintf(stderr, "Missing netlist ");
	if (error & GERB_IMAGE_MISSING_FORMAT)
	    fprintf(stderr, "Missing format ");
	if (error & GERB_IMAGE_MISSING_APERTURES) 
	    fprintf(stderr, "Missing apertures/drill sizes ");
	if (error & GERB_IMAGE_MISSING_INFO)
	    fprintf(stderr, "Missing info ");
	fprintf(stderr, "\n");
	exit(1);
    }

    if (reload) return; /* Stop here if we do a reload of files */

    /*
     * Store filename for eventual reload
     */
    screen.file[idx]->name = (char *)malloc(strlen(filename) + 1);
    strcpy(screen.file[idx]->name, filename);

    /*
     * Calculate a "clever" random color based on index.
     */
    r = (12341 + 657371 * idx) % (int)(MAX_COLOR_RESOLUTION);
    g = (23473 + 434382 * idx) % (int)(MAX_COLOR_RESOLUTION);
    b = (90341 + 123393 * idx) % (int)(MAX_COLOR_RESOLUTION);

    screen.file[idx]->color = alloc_color(r, g, b, NULL);

    /* 
     * Set color on layer button
     */
    defstyle = gtk_widget_get_default_style();
    newstyle = gtk_style_copy(defstyle);
    newstyle->bg[GTK_STATE_NORMAL] = *(screen.file[idx]->color);
    newstyle->bg[GTK_STATE_ACTIVE] = *(screen.file[idx]->color);
    newstyle->bg[GTK_STATE_PRELIGHT] = *(screen.file[idx]->color);
    gtk_widget_set_style(screen.layer_button[idx], newstyle);

    /* 
     * Tool tips on button is the file name 
     */
    gtk_tooltips_set_tip(screen.tooltips, screen.layer_button[idx],
			 filename, NULL); 

    return;
} /* open_image */


static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
    return redraw_pixmap(widget);
} /* configure_event */


static gint
button_press_event (GtkWidget *widget, GdkEventButton *event)
{
    gerbv_zoom_data_t data;
    gboolean do_zoom = FALSE;

    switch (event->button) {
    case 1 :
	if((event->state & GDK_SHIFT_MASK) == 0) {
	    /* Plain panning */
	    screen.state = MOVE;
	    screen.last_x = widget->allocation.height - event->x;
	    screen.last_y = widget->allocation.width  - event->y;
	} else {
	    GdkCursor *cursor;

	    screen.state = MEASURE;
	    screen.start_x = event->x;
	    screen.start_y = event->y;

	    cursor = gdk_cursor_new(GDK_CROSSHAIR);
	    gdk_window_set_cursor(gtk_widget_get_parent_window(widget),
				  cursor);
	    gdk_cursor_destroy(cursor);
	    
	}
	break;
    case 2 :
	/* And now, some Veribest-like mouse commands for
	   all us who dislike scroll wheels ;) */
	do_zoom = TRUE;
	if((event->state & GDK_SHIFT_MASK) != 0) {
	    /* Middle button + shift == zoom in */
	    data.z_dir = ZOOM_IN_CMOUSE;
	} else {
	    /* Only middle button == zoom out */
	    data.z_dir = ZOOM_OUT_CMOUSE;
	}
	break;
    case 3 :
	/* Zoom outline mode initiated */
	screen.state = ZOOM_OUTLINE;
	screen.start_x = event->x;
	screen.start_y = event->y;
	screen.centered_outline_zoom = event->state & GDK_SHIFT_MASK;
	break;
    case 4 :
	data.z_dir = ZOOM_IN_CMOUSE;
	do_zoom = TRUE;
	break;
    case 5 : 
	data.z_dir = ZOOM_OUT_CMOUSE;
	do_zoom = TRUE;
	break;
    default:
    }

    if (do_zoom) {
	data.z_event = event;
	zoom(widget, &data);
    }

    return TRUE;
} /* button_press_event */


static gint
button_release_event (GtkWidget *widget, GdkEventButton *event)
{
    if (event->type == GDK_BUTTON_RELEASE) {
	if (screen.state == ZOOM_OUTLINE) {
	    screen.state = NORMAL;
	    zoom_outline(widget, event);
	} else if (!(event->state & GDK_SHIFT_MASK)) {
	    gdk_window_set_cursor(gtk_widget_get_parent_window(widget),
				  GERBV_DEF_CURSOR);
	}
	screen.last_x = screen.last_y = 0;
	screen.state = NORMAL;
    }

    return TRUE;
} /* button_release_event */


static gint
key_press_event (GtkWidget *widget, GdkEventKey *event)
{
    if (screen.state == NORMAL) {
	if ((event->state & GDK_SHIFT_MASK) ||
	    (event->keyval == GDK_Shift_L) ||
	    (event->keyval == GDK_Shift_R)) {
	    GdkCursor *cursor;

	    cursor = gdk_cursor_new(GDK_CROSSHAIR);
	    gdk_window_set_cursor(gtk_widget_get_parent_window(screen.drawing_area),
				  cursor);
	    gdk_cursor_destroy(cursor);
	}
    }

    /* Escape may be used to abort outline zoom and just plain repaint */
    if (event->keyval == GDK_Escape) {
	GdkRectangle update_rect;

	screen.state = NORMAL;

	update_rect.x = 0, update_rect.y = 0;
	update_rect.width =	widget->allocation.width;
	update_rect.height = widget->allocation.height;

	/*
	 * Calls expose_event
	 */
	gtk_widget_draw(widget, &update_rect);
    }

    return TRUE;
} /* key_press_event */


static gint
key_release_event (GtkWidget *widget, GdkEventKey *event)
{
    if (screen.state == NORMAL) {
	if((event->state & GDK_SHIFT_MASK) ||
	   (event->keyval == GDK_Shift_L) ||
	   (event->keyval == GDK_Shift_R)) {
	    gdk_window_set_cursor(gtk_widget_get_parent_window(screen.drawing_area),
				  GERBV_DEF_CURSOR);
	}
    }
    return TRUE;
} /* key_release_event */


static gint
motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
    int x, y;
    GdkModifierType state;
    GdkRectangle update_rect;
    
    if (event->is_hint)
	gdk_window_get_pointer (event->window, &x, &y, &state);
    else {
	x = event->x;
	y = event->y;
	state = event->state;
    }
    
    if (screen.pixmap != NULL) {
	char str[60];
	double X, Y;
	int px, py;

	if (screen.statusbar.absid)
	    gtk_statusbar_pop((GtkStatusbar*)screen.statusbar.abs,
			      screen.statusbar.absid);
	screen.statusbar.absid = 
	    gtk_statusbar_get_context_id((GtkStatusbar*)screen.statusbar.abs,
					 "MotionNotify");
	X = screen.gerber_bbox.x1 + (x+screen.trans_x)/(double)screen.scale;
	gdk_window_get_size(screen.pixmap, &px, &py); /* Need pixmap size, not screen */
	Y = screen.gerber_bbox.y1 + (py-y-screen.trans_y)/(double)screen.scale;
	sprintf(str,
		"(X, Y) (%7.1f, %7.1f)mils (%7.2f, %7.2f)mm",
		X*1000.0, Y*1000.0, X*25.4, Y*25.4);
	gtk_statusbar_push((GtkStatusbar*)screen.statusbar.abs,
			   screen.statusbar.absid, str);
	switch (screen.state) {
	case MOVE: {

	    x = widget->allocation.height - x;
	    y = widget->allocation.width - y;

	    if (screen.last_x != 0 || screen.last_y != 0) {
		screen.trans_x = screen.trans_x + x - screen.last_x;
		screen.trans_y = screen.trans_y + y - screen.last_y;
	    }
	    screen.last_x = x;
	    screen.last_y = y;

	    update_rect.x = 0, update_rect.y = 0;
	    update_rect.width  = widget->allocation.width;
	    update_rect.height = widget->allocation.height;
	
	    /*
	     * Calls expose_event
	     */
	    gtk_widget_draw(widget, &update_rect);
	    break;
	}
	case ZOOM_OUTLINE: {
	    if (screen.last_x || screen.last_y)
		draw_zoom_outline(screen.centered_outline_zoom);

	    screen.last_x = x;
	    screen.last_y = y;

	    draw_zoom_outline(screen.centered_outline_zoom);
	    break;
	}
	case MEASURE: {
	    if (screen.last_x || screen.last_y)
		draw_measure_distance();

	    screen.last_x = x;
	    screen.last_y = y;

	    draw_measure_distance();
	    break;
	}
	default:
	    break;
	}
    }
    
    return TRUE;
} /* motion_notify_event */


/* Redraw the screen from the backing pixmap */
static gint
expose_event (GtkWidget *widget, GdkEventExpose *event)
{

    GdkPixmap *new_pixmap;
    GdkGC *gc = gdk_gc_new(widget->window);

    /*
     * Create a pixmap with default background
     */
    new_pixmap = gdk_pixmap_new(widget->window,
				widget->allocation.width,
				widget->allocation.height,
				-1);

    gdk_gc_set_foreground(gc, screen.background);

    gdk_draw_rectangle(new_pixmap, gc, TRUE, 
		       event->area.x, event->area.y,
		       event->area.width, event->area.height);
    
    /*
     * Copy gerber pixmap onto background if we have one to copy.
     * Do translation at the same time.
     */
    if (screen.pixmap != NULL)
	gdk_draw_pixmap(new_pixmap,
			widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			screen.pixmap, 
			screen.trans_x + event->area.x, 
			screen.trans_y + event->area.y, 
			event->area.x, event->area.y,
			event->area.width, event->area.height);

    /*
     * Draw the whole thing onto screen
     */
    gdk_draw_pixmap(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    new_pixmap,
		    event->area.x, event->area.y,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);

    gdk_pixmap_unref(new_pixmap);
    gdk_gc_unref(gc);

    /*
     * Draw Zooming outline if we are in that mode
     */
    if (screen.state == ZOOM_OUTLINE) {
	draw_zoom_outline(screen.centered_outline_zoom);
    } else if (screen.state == MEASURE) {
	draw_measure_distance();
    }
    /*
     * Raise popup windows if they happen to disappear
     */
    if (screen.load_file_popup && screen.load_file_popup->window)
	gdk_window_raise(screen.load_file_popup->window);
    if (screen.color_selection_popup && screen.color_selection_popup->window)
	gdk_window_raise(screen.color_selection_popup->window);
    if (screen.export_png_popup && screen.export_png_popup->window)
	gdk_window_raise(screen.export_png_popup->window);

    return FALSE;
} /* expose_event */

static void
draw_zoom_outline(gboolean centered)
{
    GdkGC *gc;
    GdkGCValues values;
    GdkGCValuesMask values_mask;
    gint x1, y1, x2, y2, dx, dy;

    memset(&values, 0, sizeof(values));
    values.function = GDK_XOR;
    values.foreground = *screen.zoom_outline_color;
    values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
    gc = gdk_gc_new_with_values(screen.drawing_area->window, &values, values_mask);

    x1 = MIN(screen.start_x, screen.last_x);
    y1 = MIN(screen.start_y, screen.last_y);
    x2 = MAX(screen.start_x, screen.last_x);
    y2 = MAX(screen.start_y, screen.last_y);
    dx = x2-x1;
    dy = y2-y1;

    if (centered) {
	/* Centered outline mode */
	x1 = screen.start_x - dx;
	y1 = screen.start_y - dy;
	dx *= 2;
	dy *= 2;
	x2 = x1+dx;
	y2 = y1+dy;
    }

    gdk_draw_rectangle(screen.drawing_area->window, gc, FALSE, x1, y1, dx, dy);
    gdk_gc_unref(gc);

    /* Draw actual zoom area in dashed lines */
    memset(&values, 0, sizeof(values));
    values.function = GDK_XOR;
    values.foreground = *screen.dist_measure_color;
    values.line_style = GDK_LINE_ON_OFF_DASH;
    values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND | GDK_GC_LINE_STYLE;
    gc = gdk_gc_new_with_values(screen.drawing_area->window, &values,
				values_mask);

    if ((dy == 0) || ((double)dx/dy > (double)screen.drawing_area->allocation.width/screen.drawing_area->allocation.height)) {
	    dy = dx * (double)screen.drawing_area->allocation.height/screen.drawing_area->allocation.width;
    } else {
	    dx = dy * (double)screen.drawing_area->allocation.width/screen.drawing_area->allocation.height;
    }

    gdk_draw_rectangle(screen.drawing_area->window, gc, FALSE, (x1+x2-dx)/2, (y1+y2-dy)/2, dx, dy);

    gdk_gc_unref(gc);
}

static void
draw_measure_distance()
{
    GdkGC *gc;
    GdkGCValues values;
    GdkGCValuesMask values_mask;
    gint x1, y1, x2, y2;
    GdkFont *font;
    const char *fontname = "-*-helvetica-bold-r-normal--*-120-*-*-*-*-iso8859-1";

    if (screen.state != MEASURE)
	return;

    memset(&values, 0, sizeof(values));
    values.function = GDK_XOR;
    values.foreground = *screen.dist_measure_color;
    values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
    gc = gdk_gc_new_with_values(screen.drawing_area->window, &values,
				values_mask);
    font = gdk_font_load(fontname);

    x1 = MIN(screen.start_x, screen.last_x);
    y1 = MIN(screen.start_y, screen.last_y);
    x2 = MAX(screen.start_x, screen.last_x);
    y2 = MAX(screen.start_y, screen.last_y);

    gdk_draw_line(screen.drawing_area->window, gc, screen.start_x,
		  screen.start_y, screen.last_x, screen.last_y);
    if (font == NULL) {
	fprintf(stderr, "Failed to load font '%s'\n", fontname);
    } else {
	gchar string[65];
	double delta, dx, dy;
	gint lbearing, rbearing, width, ascent, descent;
	gint linefeed;	/* Pseudonym for inter line gap */

	dx = (x2 - x1)/(double) screen.scale;
	dy = (y2 - y1)/(double) screen.scale;
	delta = sqrt(dx*dx + dy*dy); /* Pythagoras */

	sprintf(string, "[dist %7.1f, dX %7.1f, dY %7.1f] mils",
		delta*1000.0, dx*1000.0, dy*1000.0);

	gdk_string_extents(font, string, &lbearing, &rbearing, &width,
			   &ascent, &descent);
	gdk_draw_string(screen.drawing_area->window, font, gc,
			(x1+x2)/2-width/2, (y1+y2)/2, string);

	linefeed = ascent+descent;
	linefeed *= (double)1.2;

	sprintf(string, "[dist %7.2f, dX %7.2f, dY %7.2f] mm",
		delta*25.4, dx*25.4, dy*25.4);

	gdk_string_extents(font, string, &lbearing, &rbearing, &width,
			   &ascent, &descent);
	gdk_draw_string(screen.drawing_area->window, font, gc,
			(x1+x2)/2 - width/2, (y1+y2)/2 + linefeed, string);

	gdk_font_unref(font);

	/*
	 * Update statusbar
	 */
	if (screen.statusbar.relid)
	    gtk_statusbar_pop((GtkStatusbar*)screen.statusbar.rel,
			      screen.statusbar.relid);
	screen.statusbar.relid = 
	    gtk_statusbar_get_context_id((GtkStatusbar*)screen.statusbar.rel,
					 "MotionNotify");
	sprintf(string,
		"(dist,dX,dY) (%7.1f, %7.1f, %7.1f)mils (%7.2f, %7.3f, %7.3f)mm",
		delta*1000.0, dx*1000.0, dy*1000.0, delta*25.4, dx*25.4, dy*25.4);
	gtk_statusbar_push((GtkStatusbar*)screen.statusbar.rel,
			   screen.statusbar.relid, string);

    }
    gdk_gc_unref(gc);
}

#ifdef GUILE_IN_USE
static void
batch(char *backend, char *filename)
{
    char         *path[3];
    char 	 *complete_path;
    char         *home;
    int		  i;
    gerb_file_t  *fd;
    gerb_image_t *image;
    gerb_verify_error_t error = GERB_IMAGE_OK;
    SCM	          scm_image;

    if ((home = getenv("HOME")) == NULL)
	err(1, "HOME not set\n");

    /*
     * Define the paths to look for backend in.
     */
    if ((path[0] = (char *)malloc(strlen(".") + 1)) == NULL)
	err(1, "Malloc failed\n");
    strcpy(path[0], ".");

    if ((path[1] = (char *)malloc(strlen(home) + strlen("/.gerbv/scheme") + 1)) == NULL)
	err(1, "Malloc failed\n");
    strcpy(path[1], home);
    strcat(path[1], "/.gerbv/scheme");

    if ((path[2] = (char *)malloc(strlen(BACKEND_DIR) + 1)) == NULL)
	err(1, "Malloc failed\n");
    strcpy(path[2], BACKEND_DIR);

    /*
     * Search for backend in along the paths. Break when you find one.
     */
    for (i = 0; i < sizeof(path)/sizeof(path[0]); i++) {
	complete_path = (char *)malloc(strlen(path[i]) + strlen(backend) + 2);
	strcpy(complete_path, path[i]);
	strcat(complete_path, "/");
	strcat(complete_path, backend);
	if (access(complete_path, R_OK) != -1)
	    break;
	free(complete_path);
	complete_path = NULL;
    }

    /*
     * First minor cleanup
     */
    for (i = 0; i < sizeof(path)/sizeof(path[0]); i++) {
	free(path[i]);
	path[i] = NULL;
    }

    /*
     * Did we find a backend?
     */
    if (complete_path == NULL) {
	printf("Backend not found\n");
	return;
    } else {
	printf("Backend is [%s]\n", complete_path);
    }

    /*
     * Read and parse Gerberfile
     */
    fd = gerb_fopen(filename);
    if (fd == NULL) {
	fprintf(stderr, "Trying to open %s: ", filename);
	perror("");
	return;
    }

    if (drill_file_p(fd))
	image = parse_drillfile(fd);
    else
	image = parse_gerb(fd);
    
    gerb_fclose(fd);

    /*
     * Do error check before continuing
     */
    error = gerb_image_verify(image);
    if (error) {
	fprintf(stderr, "%s: Parse error: ", filename);
	if (error & GERB_IMAGE_MISSING_NETLIST)
	    fprintf(stderr, "Missing netlist ");
	if (error & GERB_IMAGE_MISSING_FORMAT)
	    fprintf(stderr, "Missing format ");
	if (error & GERB_IMAGE_MISSING_APERTURES) 
	    fprintf(stderr, "Missing apertures/drill sizes ");
	if (error & GERB_IMAGE_MISSING_INFO)
	    fprintf(stderr, "Missing info ");
	fprintf(stderr, "\n");
	exit(1);
    }
   
    /*
     * Convert it to Scheme
     */
    scm_image = scm_image2scm(image, filename);
    
    /*
     * Call external Scheme function in found backend
     */
    scm_primitive_load(scm_makfrom0str(complete_path));
    scm_apply(scm_eval(gh_symbol2scm("main")), scm_image, SCM_EOL);
    
    /*
     * Cleanup
     */
    free(complete_path);
    complete_path = NULL;
    
    free_gerb_image(image);
    
    return;
}
#endif /* GUILE_IN_USE */


#ifdef HAVE_GETOPT_LONG
static struct option longopts[] = {
    /* name     has_arg            flag  val */
    {"version", no_argument,       NULL, 'V'},
    {"batch",   required_argument, NULL, 'b'},
    {0, 0, 0, 0},
};
#endif /* HAVE_GETOPT_LONG*/


void
internal_main(int argc, char *argv[])
{
    GtkWidget *main_win;
    GtkWidget *vbox;
    GtkWidget *hbox, *hbox2;
    GtkWidget *menubar;
    gint      screen_width, width, height;
    char      read_opt;
    int       i;
#ifdef GUILE_IN_USE
    char      *backend = NULL;
    int	      run_batch = 0;
#endif /* GUILE_IN_USE */

#ifdef HAVE_GETOPT_LONG
    while ((read_opt = getopt_long(argc, argv, "Vb:", 
				   longopts, NULL)) != -1) {
#else
    while ((read_opt = getopt(argc, argv, "Vb:")) != -1) {
#endif /* HAVE_GETOPT_LONG */
	switch (read_opt) {
	case 'V' :
	    printf("gerbv version %s\n", VERSION);
	    printf("(C) Stefan Petersen (spe@stacken.kth.se)\n");
	    exit(0);
	case 'b' :
#ifdef GUILE_IN_USE
	    run_batch = 1;
	    if (optarg == NULL)
		err(1, "You must give a backend in batch mode\n");
	    
	    backend = (char *)malloc(strlen(optarg) + strlen("gerb-.scm") + 1);
	    if (backend == NULL) 
		err(1, "Failed mallocing backend string\n");
	    strcpy(backend, "gerb-");
	    strcat(backend, optarg);
	    strcat(backend, ".scm");
#else
	    fprintf(stderr, "This version doesn't have batch support\n");
	    exit(0);
#endif /* GUILE_IN_USE */
	    break;
	case '?':
#ifdef GUILE_IN_USE
	    fprintf(stderr, "Usage : %s [--version|-V][--batch=<backend>|-b <backend>] <gerber file(s)>\n", argv[0]);
#else
	    fprintf(stderr, "Usage : %s [--version|-V] <gerber file(s)>\n", argv[0]);
#endif /* GUILE_IN_USE */
	    exit(1);
	    break;
	default :
	    printf("Not handled option [%d=%c]\n", read_opt, read_opt);
	}
    }
    
#ifdef GUILE_IN_USE
    if (run_batch) {
	if (optind == argc)
	    err(1, "No file to work on\n");
	
	/*
	 * Loop through gerber files
	 */
	for(i = optind ; i < argc; i++) {
	    printf("%s\n", argv[i]);
	    batch(backend, argv[i]);
	}

	free(backend);
	exit(0);
    }
#endif /* GUILE_IN_USE */
    /*
     * Setup the screen info
     */
    memset((void *)&screen, 0, sizeof(gerbv_screen_t));
    screen.state = NORMAL;
	
    /*
     * Init GTK+
     */
    gtk_init(&argc, &argv);

    /* 
     * Good defaults according to Ales. Gives aspect ratio of 1.3333...
     */
    screen_width = gdk_screen_width();
    width = screen_width * 3/4;
    height = width * 3/4;

    /*
     * Setup some GTK+ defaults
     */
    screen.tooltips = gtk_tooltips_new();        
    screen.background = alloc_color(0, 0, 0, "black");
    screen.err_color  = alloc_color(0, 0, 0, "red1");
    screen.zoom_outline_color  = alloc_color(0, 0, 0, "gray");
    screen.dist_measure_color  = alloc_color(0, 0, 0, "lightblue");

    /*
     * Main window 
     */
    main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_win), "Gerber Viewer");
    gtk_signal_connect(GTK_OBJECT(main_win), "delete_event", destroy, NULL);
    gtk_signal_connect(GTK_OBJECT(main_win), "destroy", destroy, NULL);
    
    /* 
     * vbox contains menubar and hbox
     */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(main_win), vbox);
    
    create_menubar(main_win, &menubar);
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    
    /* 
     * hbox contains drawing area and image select area
     */
    hbox = gtk_hbox_new(FALSE, 0);
    
    /*
     * Create drawing area
     */
    screen.drawing_area = create_drawing_area(width, height);
    gtk_box_pack_start(GTK_BOX(hbox), screen.drawing_area, TRUE, TRUE, 0);
    
    /*
     * Build layer buttons with popup menus
     */
    create_popupmenu(&screen.popup_menu);
    gtk_box_pack_start(GTK_BOX(hbox), create_layer_buttons(MAX_FILES), 
		       FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    /*
     * Add status bar (three sections: mesages, abs and rel coords)
     */
    hbox = gtk_hbox_new(FALSE, 0);
    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_widget_set_usize(hbox, screen.drawing_area->allocation.width/2,0);
    screen.statusbar.msgs = gtk_statusbar_new();
    screen.statusbar.msgid =
	gtk_statusbar_get_context_id((GtkStatusbar*)screen.statusbar.msgs,
				     "Dummy");
	gtk_statusbar_push((GtkStatusbar*)screen.statusbar.msgs,
			   screen.statusbar.msgid, "                                                                               ");

    screen.statusbar.abs = gtk_statusbar_new();
    screen.statusbar.rel = gtk_statusbar_new();
    screen.statusbar.relid = 0;
    gtk_box_pack_start(GTK_BOX(hbox), screen.statusbar.msgs, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), hbox2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), screen.statusbar.abs, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), screen.statusbar.rel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    /*
     * Fill with files (eventually) given on command line
     */
    for(i = optind ; i < argc; i++)
	open_image(argv[i], i - optind, FALSE);

    /*
     * Connect all events on drawing area 
     */    
    gtk_signal_connect(GTK_OBJECT(screen.drawing_area), "expose_event",
		       GTK_SIGNAL_FUNC(expose_event), NULL);
    gtk_signal_connect(GTK_OBJECT(screen.drawing_area),"configure_event",
		       GTK_SIGNAL_FUNC(configure_event), NULL);
    gtk_signal_connect(GTK_OBJECT(screen.drawing_area), "motion_notify_event",
		       GTK_SIGNAL_FUNC(motion_notify_event), NULL);
    gtk_signal_connect(GTK_OBJECT(screen.drawing_area), "button_press_event",
		       GTK_SIGNAL_FUNC(button_press_event), NULL);
    gtk_signal_connect(GTK_OBJECT(screen.drawing_area), "button_release_event",
		       GTK_SIGNAL_FUNC(button_release_event), NULL);
    gtk_signal_connect_after(GTK_OBJECT(main_win), "key_press_event",
		       GTK_SIGNAL_FUNC(key_press_event), NULL);
    gtk_signal_connect_after(GTK_OBJECT(main_win), "key_release_event",
		       GTK_SIGNAL_FUNC(key_release_event), NULL);

    gtk_widget_set_events(screen.drawing_area, GDK_EXPOSURE_MASK
			  | GDK_LEAVE_NOTIFY_MASK
			  | GDK_BUTTON_PRESS_MASK
			  | GDK_BUTTON_RELEASE_MASK
			  | GDK_KEY_PRESS_MASK
			  | GDK_KEY_RELEASE_MASK
			  | GDK_POINTER_MOTION_MASK
			  | GDK_POINTER_MOTION_HINT_MASK);
    
    gtk_widget_show_all(main_win);

    /* It seems this has to be done after the button is shown for
       the first time, or we get a segmentation fault */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(screen.layer_button[0]),
				 TRUE);

    gtk_main();
    
    return;
}
    

int
main(int argc, char *argv[])
{
#ifdef GUILE_IN_USE
    gh_enter(argc, argv, internal_main);
#else
    internal_main(argc, argv);
#endif /* GUILE_IN_USE */
    return 0;
}
