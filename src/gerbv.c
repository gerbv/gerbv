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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <libgen.h> /* dirname */
#include <errno.h>

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

#ifdef USE_GTK2
#include <pango/pango.h>
#endif

#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"
#include "draw.h"
#include "color.h"
#include "gerbv_screen.h"
#include "gerbv_icon.h"
#include "log.h"
#include "setup.h"
#include "project.h"
#ifdef EXPORT_PNG
#include "exportimage.h"
#endif /* EXPORT_PNG */
#include "tooltable.h"

#define WIN_TITLE "Gerber Viewer : "


typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, ZOOM_SET } gerbv_zoom_dir_t;
typedef struct {
    gerbv_zoom_dir_t z_dir;
    GdkEventButton *z_event;
    int scale;
} gerbv_zoom_data_t;


/*
 * Declared extern in gerbv_screen.h. Global state variable to keep track
 * of what's happening on the screen.
 */
gerbv_screen_t screen;

#define SAVE_PROJECT 0
#define SAVE_AS_PROJECT 1
#define OPEN_PROJECT 2

static gint expose_event (GtkWidget *widget, GdkEventExpose *event);
static void draw_zoom_outline(gboolean centered);
static void draw_measure_distance();
static void swap_layers(GtkWidget *widget, gpointer data);
static void color_selection_popup(GtkWidget *widget, gpointer data);
static void invert_color(GtkWidget *widget, gpointer data);
static void load_file_popup(GtkWidget *widget, gpointer data);
#ifdef EXPORT_PNG
static void export_png_popup(GtkWidget *widget, gpointer data);
#endif /* EXPORT_PNG */
static void cb_ok_project(GtkWidget *widget, gpointer data);
static void project_popup(GtkWidget *widget, gpointer data);
static void project_save_cb(GtkWidget *widget, gpointer data);
static void unload_file(GtkWidget *widget, gpointer data);
static void reload_files(GtkWidget *widget, gpointer data);
static void menu_zoom(GtkWidget *widget, gpointer data);
static void si_func(GtkWidget *widget, gpointer data);
static void unit_func(GtkWidget *widget, gpointer data);
static void all_layers_on(GtkWidget *widget, gpointer data);
static void all_layers_off(GtkWidget *widget, gpointer data);
static void load_project(project_list_t *project_list);
static void zoom(GtkWidget *widget, gpointer data);
static void zoom_outline(GtkWidget *widget, GdkEventButton *event);
static gint redraw_pixmap(GtkWidget *widget, int restart);
static int open_image(char *filename, int idx, int reload);
static void update_statusbar(gerbv_screen_t *scr);

static void menu_ask_zoom (GtkWidget * widget, gpointer data);
static GtkWidget *create_ZoomFactorWindow (void);
static void zoom_spinbutton1_realize (GtkWidget * widget, gpointer user_data);
GtkWidget *lookup_widget (GtkWidget * widget, const gchar * widget_name);
static void zoom_ok_button_clicked (GtkButton * button, gpointer user_data);
static void zoom_cancel_button_clicked (GtkButton * button, gpointer user_data);

void rename_main_window(char *filename, GtkWidget *main_win);

void
destroy(GtkWidget *widget, gpointer data)
{
    int i;

    for (i = 0; i < MAX_FILES; i++) {
	if (screen.file[i] && screen.file[i]->image)
	    free_gerb_image(screen.file[i]->image);
	if (screen.file[i] && screen.file[i]->color)
	    free(screen.file[i]->color);
	if (screen.file[i] && screen.file[i]->name)
	    free(screen.file[i]->name);
	if (screen.file[i])
	    free(screen.file[i]);
    }

    /* Free all colors allocated */
    if (screen.background)
	free(screen.background);
    if (screen.zoom_outline_color)
	free(screen.zoom_outline_color);
    if (screen.dist_measure_color)
	free(screen.dist_measure_color);

    setup_destroy();

    gtk_main_quit();
} /* destroy */


static GtkItemFactoryEntry menu_items[] = {
    {"/_File",               NULL,     NULL,             0, "<Branch>"},
    {"/File/_Open Project...",NULL,    project_popup,    OPEN_PROJECT, NULL},
    {"/File/_Save Project As...",NULL, project_popup,    SAVE_AS_PROJECT,NULL},
    {"/File/_Save Project",  NULL,     project_save_cb,  0, NULL},
    {"/File/sep1",           NULL,     NULL,             0, "<Separator>"},
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
    {"/Zoom/_Set scale",     NULL,     menu_ask_zoom,    0, NULL},
    {"/_Setup",              NULL,     NULL,             0, "<Branch>"},

    {"/Setup/_Superimpose",  NULL,     NULL,             0, "<Branch>"},
    {"/Setup/_Superimpose/Copy",NULL, si_func, 0, "<RadioItem>"},
    {"/Setup/_Superimpose/And", NULL, si_func, GDK_AND,  "/Setup/Superimpose/Copy"},
    {"/Setup/_Superimpose/Or",  NULL, si_func, GDK_OR,   "/Setup/Superimpose/Copy"},
    {"/Setup/_Superimpose/Xor", NULL, si_func, GDK_XOR,  "/Setup/Superimpose/Copy"},
    {"/Setup/_Superimpose/Invert", NULL, si_func, GDK_INVERT,  "/Setup/Superimpose/Copy"},

    {"/Setup/_Background Color",NULL, color_selection_popup, 1, NULL},
    {"/Setup/_Units",  NULL,     NULL,             0, "<Branch>"},
    {"/Setup/_Units/m_ils",NULL, unit_func, GERBV_MILS, "<RadioItem>"},
    {"/Setup/_Units/_mm",NULL, unit_func, GERBV_MMS, "/Setup/Units/mils"},
};

static GtkItemFactoryEntry popup_menu_items[] = {
    {"/Swap with Above", NULL, swap_layers, 0, NULL},
    {"/Layer Color...", NULL, color_selection_popup, 0, NULL},
    {"/Load File...", NULL, load_file_popup, 0, NULL},
    {"/Unload File...", NULL, unload_file, 0, NULL},
    {"/Invert Color", NULL, invert_color, 0, NULL},
    {"/Swap with Below", NULL, swap_layers, 1, NULL},
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
#ifdef USE_GTK2
    gtk_window_add_accel_group (GTK_WINDOW(window), accel_group);
#else
    gtk_accel_group_attach(accel_group, GTK_OBJECT(window));
#endif
    
    if(menubar) {
	GtkWidget *menuEntry;

	/* Finally, return the actual menu bar created by the item factory. */
	*menubar = gtk_item_factory_get_widget(item_factory, "<main>");
	/* Set menu selection to reflect the current active unit */
	if (GERBV_DEFAULT_UNIT == GERBV_MMS) {
	    menuEntry = gtk_item_factory_get_widget(item_factory, "/Setup/Units/mm");
	    gtk_menu_item_activate(GTK_MENU_ITEM(menuEntry));
	}
    }
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


void
set_window_icon (GtkWidget * this_window)
{
    GdkPixmap *pixmap;
    GdkBitmap *mask;

    pixmap = gdk_pixmap_create_from_xpm_d (this_window->window, &mask,
&this_window->style->bg[GTK_STATE_NORMAL], gerbv_icon_xpm);
    gdk_window_set_icon (this_window->window, NULL, pixmap, mask);

    return;
} /* set_window_icon */


static void
cb_layer_button(GtkWidget *widget, gpointer data)
{
    long idx = (long)data;

    if (screen.file[idx] == NULL)
	return;

    screen.curr_index = idx;

    /* Redraw the image(s) */
    redraw_pixmap(screen.drawing_area, TRUE);

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
    GdkColor  *color;
    GtkStyle  *defstyle, *newstyle;
    GtkTooltips *tooltips = gtk_tooltips_new();
    char      info[5];
    long int  bi;

    box = gtk_vbox_new(TRUE, 0);

    /* 
     * Create style to be used by "all layer" buttons
     */
    color = alloc_color(0, 0, 0, "white");
    defstyle = gtk_widget_get_default_style();
    newstyle = gtk_style_copy(defstyle);
    newstyle->bg[GTK_STATE_NORMAL] = *color;
    newstyle->bg[GTK_STATE_ACTIVE] = *color;
    newstyle->bg[GTK_STATE_PRELIGHT] = *color;

    /*
     * Create On button with callback, color and tooltips
     */
    button = gtk_button_new_with_label("On");
    gtk_signal_connect(GTK_OBJECT(button), "button_press_event",
		       GTK_SIGNAL_FUNC(all_layers_on), 
		       (gpointer)NULL);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    gtk_widget_set_style(button, newstyle);
    gtk_tooltips_set_tip(tooltips, button, "Turn On All Layers", NULL); 

    /*
     * Create Off button with callback, color and tooltips
     */
    button = gtk_button_new_with_label("Off");
    gtk_signal_connect(GTK_OBJECT(button), "button_press_event",
		       GTK_SIGNAL_FUNC(all_layers_off), 
		       (gpointer)NULL);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    gtk_widget_set_style(button, newstyle);
    gtk_tooltips_set_tip(tooltips, button, "Turn Off All Layers", NULL); 

    /*
     * Create the rest of the buttons
     */
    for (bi = 0; bi < nuf_buttons; bi++) {
	snprintf(info, sizeof(info), "%ld", bi);
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
    /* Remove modal grab and destroy color selection dialog */
    gtk_grab_remove(screen.win.color_selection);
    gtk_widget_destroy(screen.win.color_selection);
    screen.win.color_selection = NULL;
} /* color_selection_destroy */


static void
color_selection_ok(GtkWidget *widget, gpointer data)
{
    int background = (long int)data;
    GtkColorSelection *colorsel;
    gdouble color[4];
    GtkStyle *oldstyle, *newstyle;

    /* Get selected color */
    colorsel = GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG
				   (screen.win.color_selection)->colorsel);
    gtk_color_selection_get_color(colorsel, color);

    /* Allocate new color  */
    if (background) {
	free(screen.background);
	screen.background = 
	    alloc_color((int)(color[0] * MAX_COLOR_RESOLUTION),
			(int)(color[1] * MAX_COLOR_RESOLUTION),
			(int)(color[2] * MAX_COLOR_RESOLUTION),
			NULL);
    } else {
	free(screen.file[screen.curr_index]->color);
	screen.file[screen.curr_index]->color = 
	    alloc_color((int)(color[0] * MAX_COLOR_RESOLUTION), 
			(int)(color[1] * MAX_COLOR_RESOLUTION), 
			(int)(color[2] * MAX_COLOR_RESOLUTION), 
			NULL);
    }

    /* Redraw image on screen */
    redraw_pixmap(screen.drawing_area, TRUE);

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
    gtk_grab_remove(screen.win.color_selection);
    gtk_widget_destroy(screen.win.color_selection);
    screen.win.color_selection = NULL;

    return;
} /* cb_ok_color_selection */


static void 
swap_layers(GtkWidget *widget, gpointer data)
{
    gerbv_fileinfo_t *temp_file;
    GtkStyle *s1, *s2;
    GtkTooltipsData *td1, *td2;
    int idx;

    if (screen.file[screen.curr_index] == NULL) return;

    if (data == 0) {
	/* Moving Up */
	if (screen.curr_index == 0) return;
	if (screen.file[screen.curr_index - 1] == NULL) return;
	idx = -1;
    } else { 
	/* Moving Down */
	if (screen.curr_index == MAX_FILES - 1) return;
	if (screen.file[screen.curr_index + 1] == NULL) return;
	idx = 1;
    }

    /* Swap file */
    temp_file = screen.file[screen.curr_index];
    screen.file[screen.curr_index] = screen.file[screen.curr_index + idx];
    screen.file[screen.curr_index + idx] = temp_file;

    /* Swap color on button */
    s1 = gtk_widget_get_style(screen.layer_button[screen.curr_index]);
    s2 = gtk_widget_get_style(screen.layer_button[screen.curr_index + idx]);
    gtk_widget_set_style(screen.layer_button[screen.curr_index + idx], s1);
    gtk_widget_set_style(screen.layer_button[screen.curr_index], s2);

    /* Swap tooltips on button */
    td1 = gtk_tooltips_data_get(screen.layer_button[screen.curr_index]);
    td2 = gtk_tooltips_data_get(screen.layer_button[screen.curr_index + idx]);
    gtk_tooltips_set_tip(td1->tooltips, td1->widget,
			 screen.file[screen.curr_index]->name, NULL);
    gtk_tooltips_set_tip(td2->tooltips, td2->widget,
			 screen.file[screen.curr_index + idx]->name, NULL);


    redraw_pixmap(screen.drawing_area, TRUE);
    
} /* swap_layers */


static void
color_selection_popup(GtkWidget *widget, gpointer data)
{
    gdouble curr_color[4];
    int background = (long int)data;

    if (!background && (screen.file[screen.curr_index] == NULL))
	return;

    screen.win.color_selection = gtk_color_selection_dialog_new("Color Selection");

    gtk_color_selection_set_update_policy
	(GTK_COLOR_SELECTION
	 (GTK_COLOR_SELECTION_DIALOG(screen.win.color_selection)->colorsel),
	 GTK_UPDATE_CONTINUOUS);

    gtk_signal_connect
	(GTK_OBJECT
	 (GTK_COLOR_SELECTION_DIALOG(screen.win.color_selection)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(color_selection_destroy), NULL);

    gtk_signal_connect
	(GTK_OBJECT
	 (GTK_COLOR_SELECTION_DIALOG(screen.win.color_selection)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(color_selection_ok), data);

    gtk_signal_connect(GTK_OBJECT(screen.win.color_selection), "destroy",
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
			     (screen.win.color_selection)->colorsel),
	 curr_color);

    /* Display widget */
    gtk_widget_show(screen.win.color_selection);

    /* Make widget modal */
    gtk_grab_add(screen.win.color_selection);

    return;
} /* color_selection_popup */


static void 
invert_color(GtkWidget *widget, gpointer data)
{
    if (!screen.file[screen.curr_index])
	return;

    if (screen.file[screen.curr_index]->inverted)
	screen.file[screen.curr_index]->inverted = 0;
    else
	screen.file[screen.curr_index]->inverted = 1;

    redraw_pixmap(screen.drawing_area, TRUE);
	
    return;
} /* invert_color */


static void
cb_ok_load_file(GtkWidget *widget, GtkFileSelection *fs)
{
    char *filename;

    filename = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
    if (open_image(filename, screen.curr_index, FALSE) != -1) {

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
	redraw_pixmap(screen.drawing_area, TRUE);
    }

    gtk_grab_remove(screen.win.load_file);
    gtk_widget_destroy(screen.win.load_file);
    screen.win.load_file = NULL;

    return;
} /* cb_ok_load_file */


static void
cb_cancel_load_file(GtkWidget *widget, gpointer data)
{
    gtk_grab_remove(screen.win.load_file);
    gtk_widget_destroy(screen.win.load_file);
    screen.win.load_file = NULL;

    return;
} /* cb_cancel_load_file */


static void
load_file_popup(GtkWidget *widget, gpointer data)
{
    screen.win.load_file = gtk_file_selection_new("Select File To View");

    if (screen.path)
	gtk_file_selection_set_filename
	    (GTK_FILE_SELECTION(screen.win.load_file), screen.path);

    gtk_signal_connect(GTK_OBJECT(screen.win.load_file), "destroy",
		       GTK_SIGNAL_FUNC(cb_cancel_load_file),
		       NULL);
    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.load_file)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_load_file), 
	 (gpointer)screen.win.load_file);
    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.load_file)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_cancel_load_file), 
	 (gpointer)screen.win.load_file);
    
    gtk_widget_show(screen.win.load_file);

    gtk_grab_add(screen.win.load_file);
    
    return;
} /* load_file_popup */


#ifdef EXPORT_PNG
static void
cb_ok_export_png(GtkWidget *widget, GtkFileSelection *fs)
{
    char *filename;
    gboolean result;
    GdkWindow *window;

    filename = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

    /* This might be lengthy, show that we're busy by changing the pointer */
    window = gtk_widget_get_parent_window(widget);
    if (window) {
	GdkCursor *cursor;

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
    }

    /* Export PNG */
#ifdef EXPORT_DISPLAYED_IMAGE
    result = png_export(screen.pixmap, filename);
#else
    result = png_export(NULL, filename);
#endif /* EXPORT_DISPLAYED_IMAGE */
    if (!result) {
	GERB_MESSAGE("Failed to save PNG at %s\n", filename);
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
   
    gtk_grab_remove(screen.win.export_png);
    
    screen.win.export_png = NULL;

    return;
} /* cb_ok_export_png */


static void
export_png_popup(GtkWidget *widget, gpointer data)
{
    screen.win.export_png = gtk_file_selection_new("Save PNG filename");

    if (screen.path)
	gtk_file_selection_set_filename
	    (GTK_FILE_SELECTION(screen.win.export_png), screen.path);

    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.export_png)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_export_png), 
	 (gpointer)screen.win.export_png);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.export_png)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.export_png);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.export_png)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.export_png);
    
    gtk_widget_show(screen.win.export_png);

    gtk_grab_add(screen.win.export_png);
    
    return;
} /* export_png_popup */

#endif /* EXPORT_PNG */


static void
cb_ok_project(GtkWidget *widget, gpointer data)
{
    char *filename = NULL;
    project_list_t *project_list = NULL, *tmp;
    int idx;
    
    if (screen.win.project) {
	filename = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(screen.win.project));

	/*
	 * Remember where we loaded file from last time
	 */
	if (screen.path)
	    free(screen.path);
	screen.path = (char *)malloc(strlen(filename) + 1);
	strcpy(screen.path, filename);
	dirname(screen.path);
	screen.path = strncat(screen.path, "/", 1);
    }

    switch ((long)data) {
    case OPEN_PROJECT:
	
	project_list = read_project_file(filename);

	if (project_list) {
	    load_project(project_list);
	    /*
	     * Save project filename for later use
	     */
	    if (screen.project) {
		free(screen.project);
		screen.project = NULL;
	    }
	    screen.project = (char *)malloc(strlen(filename) + 1);
	    memset((void *)screen.project, 0, strlen(filename) + 1);
	    strncpy(screen.project, filename, strlen(filename));
            rename_main_window(filename, NULL);
	    redraw_pixmap(screen.drawing_area, TRUE);
	} else {
	    GERB_MESSAGE("Failed to load project\n");
	    goto cb_ok_project_end;
	}
	all_layers_on(NULL, NULL);
	break;
    case SAVE_AS_PROJECT:
	/*
	 * Save project filename for later use
	 */
	if (screen.project) {
	    free(screen.project);
	    screen.project = NULL;
	}
	screen.project = (char *)malloc(strlen(filename) + 1);
	memset((void *)screen.project, 0, strlen(filename) + 1);
	strncpy(screen.project, filename, strlen(filename));
	rename_main_window(filename, NULL);
	
    case SAVE_PROJECT:
	if (!screen.project) {
	    GERB_MESSAGE("Missing project filename\n");
	    goto cb_ok_project_end;
	}
	
	if (screen.path) {
	    project_list = (project_list_t *)malloc(sizeof(project_list_t));
	    memset(project_list, 0, sizeof(project_list_t));
	    project_list->next = project_list;
	    project_list->layerno = -1;
	    project_list->filename = screen.path;
	    project_list->rgb[0] = screen.background->red;
	    project_list->rgb[1] = screen.background->green;
	    project_list->rgb[2] = screen.background->blue;
	    project_list->next = NULL;
	}
	
	for (idx = 0; idx < MAX_FILES; idx++) {
	    if (screen.file[idx] && screen.file[idx]->name) {
		tmp = (project_list_t *)malloc(sizeof(project_list_t));
		memset(tmp, 0, sizeof(project_list_t));
		tmp->next = project_list;
		tmp->layerno = idx;
		tmp->filename = screen.file[idx]->name;
		tmp->rgb[0] = screen.file[idx]->color->red;
		tmp->rgb[1] = screen.file[idx]->color->green;
		tmp->rgb[2] = screen.file[idx]->color->blue;
		tmp->inverted = screen.file[idx]->inverted;
		project_list = tmp;
	    }
	}
	if (write_project_file(screen.project, project_list)) {
	    GERB_MESSAGE("Failed to write project\n");
	    goto cb_ok_project_end;
	}
	break;
    default:
	GERB_FATAL_ERROR("Unknown operation in cb_ok_project\n");
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

 cb_ok_project_end:
    if (screen.win.project) {
	gtk_grab_remove(screen.win.project);
    	screen.win.project = NULL;
    }

    return;
} /* cb_ok_project */


static void 
project_popup(GtkWidget *widget, gpointer data)
{

    switch ((long)data) {
    case SAVE_AS_PROJECT:
    case SAVE_PROJECT:
	screen.win.project = gtk_file_selection_new("Save project filename");
	break;
    case OPEN_PROJECT:
    	screen.win.project = gtk_file_selection_new("Open project filename");
	break;
    default:
	GERB_FATAL_ERROR("Unknown operation in project_popup\n");
    }

    if (screen.project)
	gtk_file_selection_set_filename
	    (GTK_FILE_SELECTION(screen.win.project), screen.project);
    else if (screen.path)
	gtk_file_selection_set_filename
	    (GTK_FILE_SELECTION(screen.win.project), screen.path);
    
    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.project)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_project), data);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.project)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.project);
    gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.project)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.project);
    

    gtk_widget_show(screen.win.project);

    gtk_grab_add(screen.win.project);
    
    return;
} /* project_popup */

static void
project_save_cb(GtkWidget *widget, gpointer data)
{

    if (!screen.project) 
        project_popup(widget, (gpointer) SAVE_AS_PROJECT);
    else 
        cb_ok_project(widget, (gpointer) SAVE_PROJECT);

}

static void
all_layers_on(GtkWidget *widget, gpointer data)
{
    int idx;
    for (idx = 0; idx < MAX_FILES; idx++) {
	if (screen.file[idx]) {
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
					 (screen.layer_button[idx]),TRUE);
	}
    }
} /* all_layers_on */


static void
all_layers_off(GtkWidget *widget, gpointer data)
{
    int idx;
    for (idx = 0; idx < MAX_FILES; idx++) {
	if (screen.file[idx]) {
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
					 (screen.layer_button[idx]),FALSE);
	}
    }
} /* all_layers_off */


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
	    if (open_image(screen.file[idx]->name, idx, TRUE) == -1)
		return;
	}
    }
    
    redraw_pixmap(screen.drawing_area, TRUE);

    return;
} /* reload_files */


static void
load_project(project_list_t *project_list)
{
    project_list_t *pl_tmp;
    GtkStyle *defstyle, *newstyle;
    
    while (project_list) {
	if (project_list->layerno == -1) {
	    screen.background = alloc_color(project_list->rgb[0], 
					    project_list->rgb[1],
					    project_list->rgb[2], NULL);
	} else {
	    int idx =  project_list->layerno;

	    if (open_image(project_list->filename, idx, FALSE) == -1)
		exit(-1);

	    /* 
	     * Change color from default to from the project list
	     */
	    free(screen.file[idx]->color);
	    screen.file[idx]->color = alloc_color(project_list->rgb[0], 
						  project_list->rgb[1],
						  project_list->rgb[2], NULL);

	    /* 
	     * Also change color on layer button
	     */
	    defstyle = gtk_widget_get_default_style();
	    newstyle = gtk_style_copy(defstyle);
	    newstyle->bg[GTK_STATE_NORMAL] = *(screen.file[idx]->color);
	    newstyle->bg[GTK_STATE_ACTIVE] = *(screen.file[idx]->color);
	    newstyle->bg[GTK_STATE_PRELIGHT] = *(screen.file[idx]->color);
	    gtk_widget_set_style(screen.layer_button[idx], newstyle);

	    screen.file[idx]->inverted = project_list->inverted;
	}
	pl_tmp = project_list;
	project_list = project_list->next;
	free(pl_tmp);
    }

    return;
} /* load_project */


static void
autoscale(void)
{
    double max_width = LONG_MIN, max_height = LONG_MIN;
    double x_scale, y_scale;
    int i;
    
    if (screen.drawing_area == NULL)
	return;

    screen.gerber_bbox.x1 = HUGE_VAL;
    screen.gerber_bbox.y1 = HUGE_VAL;
    screen.gerber_bbox.x2 = -HUGE_VAL;
    screen.gerber_bbox.y2 = -HUGE_VAL;

    for(i = 0; i < MAX_FILES; i++) {
        if (screen.file[i]) {
            
            /* 
             * Find the biggest image and use as a size reference
             */
	    screen.gerber_bbox.x1 = MIN(screen.gerber_bbox.x1,
					screen.file[i]->image->info->min_x+
					screen.file[i]->image->info->offset_a);
	    screen.gerber_bbox.y1 = MIN(screen.gerber_bbox.y1,
					screen.file[i]->image->info->min_y+
					screen.file[i]->image->info->offset_b);
	    screen.gerber_bbox.x2 = MAX(screen.gerber_bbox.x2,
					screen.file[i]->image->info->max_x+
					screen.file[i]->image->info->offset_a);
	    screen.gerber_bbox.y2 = MAX(screen.gerber_bbox.y2,
					screen.file[i]->image->info->max_y+
					screen.file[i]->image->info->offset_b);
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
     * Calculate translation
     */
    if (x_scale < y_scale) {
	screen.trans_x = 0;
	screen.trans_y = -(int)((double)((screen.drawing_area->allocation.height-screen.scale*(max_height))/2.0));
    } else {
	screen.trans_x = -(int)((double)((screen.drawing_area->allocation.width-screen.scale*(max_width))/2.0));
	screen.trans_y = 0;
    }

    /* Initialize clipping bbox to contain entire image */
    screen.clip_bbox.x1 = -screen.trans_x/(double)screen.scale;
    screen.clip_bbox.y1 = -screen.trans_y/(double)screen.scale;
    screen.clip_bbox.x2 = screen.gerber_bbox.x2-screen.gerber_bbox.x1;
    screen.clip_bbox.y2 = screen.gerber_bbox.y2-screen.gerber_bbox.y1;
    screen.off_x = 0;
    screen.off_y = 0;

    return;
} /* autoscale */

/*
 * idle_redraw_pixmap_active is true when idle_redraw_pixmap
 * is called, during this time we are not allowed to call the
 * gtk_idle_xxx functions.
 */
static gboolean idle_redraw_pixmap_active = FALSE;
gboolean idle_redraw_pixmap(gpointer data);

void
start_idle_redraw_pixmap(GtkWidget *data)
{
    if (!idle_redraw_pixmap_active) {
	gtk_idle_add(idle_redraw_pixmap, (gpointer) data);
	idle_redraw_pixmap_active = TRUE;
    }
} /* start_idle_redraw_pixmap */


void
stop_idle_redraw_pixmap(GtkWidget *data)
{
    if (!idle_redraw_pixmap_active) {
	gtk_idle_remove_by_data ((gpointer)data);
	idle_redraw_pixmap_active = FALSE;
    }
} /* stop_idle_redraw_pixmap */


/*
 * On idle callback to ensure a zoomed image is properly redrawn
 */
gboolean
idle_redraw_pixmap(gpointer data)
{
    gboolean retval;

    idle_redraw_pixmap_active = TRUE;
    if (redraw_pixmap((GtkWidget *) data, FALSE)) {
	retval = TRUE;
    } else {
	retval = FALSE;
    }
    idle_redraw_pixmap_active = FALSE;
    return retval;
} /* idle_redraw_pixmap */


static void
menu_zoom(GtkWidget *widget, gpointer data)
{
    gerbv_zoom_data_t z_data = { (gerbv_zoom_dir_t)data, NULL };

    zoom(widget, &z_data);
} /* menu_zoom */


/* Superimpose function */
static void 
si_func(GtkWidget *widget, gpointer data)
{

    if ((GdkFunction)data  == screen.si_func)
	return;
    
    screen.si_func = (GdkFunction)data;

    /* Redraw the image(s) */
    redraw_pixmap(screen.drawing_area, TRUE);

    return;
}


/* Unit function, sets unit for statusbar */
static void 
unit_func(GtkWidget *widget, gpointer data)
{

    if ((gerbv_unit_t)data  == screen.unit)
	return;
    
    screen.unit = (gerbv_unit_t)data;

    /* Redraw the status bar */
    update_statusbar(&screen);

    return;
} /* unit_func() */


/* Zoom function */
static void
zoom(GtkWidget *widget, gpointer data)
{
    double us_midx, us_midy;	/* unscaled translation for screen center */
    int half_w, half_h;		/* cache for half window dimensions */
    int mouse_delta_x = 0;	/* Delta for mouse to window center */
    int mouse_delta_y = 0;
    gerbv_zoom_data_t *z_data = (gerbv_zoom_data_t *) data;

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
    case ZOOM_SET : /*explicit scale set by user */
	screen.scale = z_data->scale;
	screen.trans_x = screen.scale * us_midx - half_w;
	screen.trans_y = screen.scale * us_midy - half_h;
        break;
    default :
	GERB_MESSAGE("Illegal zoom direction %ld\n", (long int)data);
    }

    if (z_data->z_dir == ZOOM_IN_CMOUSE ||
	z_data->z_dir == ZOOM_OUT_CMOUSE) {
	screen.trans_x += mouse_delta_x;
	screen.trans_y += mouse_delta_y;
    }

    /* Update clipping bbox */
    screen.clip_bbox.x1 = -screen.trans_x/(double)screen.scale;
    screen.clip_bbox.y1 = -screen.trans_y/(double)screen.scale;    

    /* Redraw screen */
    redraw_pixmap(screen.drawing_area, TRUE);
    
    return;
} /* zoom */


static void
zoom_outline(GtkWidget *widget, GdkEventButton *event)
{
    int x1, y1, x2, y2, dx, dy;	/* Zoom outline (UR and LL corners) */
    double us_x1, us_y1, us_x2, us_y2;
    int half_w, half_h;		/* cache for half window dimensions */

    half_w = screen.drawing_area->allocation.width / 2;
    half_h = screen.drawing_area->allocation.height / 2;

    x1 = MIN(screen.start_x, event->x);
    y1 = MIN(screen.start_y, event->y);
    x2 = MAX(screen.start_x, event->x);
    y2 = MAX(screen.start_y, event->y);
    dx = x2-x1;
    dy = y2-y1;

    if (dx < 4 && dy < 4) {
	GERB_MESSAGE("Warning: Zoom area too small, bailing out!\n");
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
    screen.clip_bbox.x1 = -screen.trans_x/(double)screen.scale;
    screen.clip_bbox.y1 = -screen.trans_y/(double)screen.scale;

zoom_outline_end:
    /* Redraw screen */
    redraw_pixmap(screen.drawing_area, TRUE);
} /* zoom_outline */


GtkWidget *
create_drawing_area(gint win_width, gint win_height)
{
    GtkWidget *drawing_area;
    
    drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area), win_width, win_height);
    
    return drawing_area;
} /* create_drawing_area */

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

/* Invalidate state, free up pixmaps if necessary */
static void
invalidate_redraw_state(struct gerbv_redraw_state *state)
{
    if (state->valid) {
	state->valid = 0;
	/* Free up pixmaps */
	if (state->curr_pixmap) {
	    gdk_pixmap_unref(state->curr_pixmap);
	}
	if (state->clipmask) {
	    gdk_pixmap_unref(state->clipmask);
	}
    }
} /* invalidate_redraw_state() */

/* Create a new backing pixmap of the appropriate size */
static gint
redraw_pixmap(GtkWidget *widget, int restart)
{
    int i;
    double dmax_x = LONG_MIN, dmax_y = LONG_MIN;
    double dmin_x = LONG_MAX, dmin_y = LONG_MAX;
    GdkGC *gc = gdk_gc_new(widget->window);
    GdkRectangle update_rect;
    int file_loaded = 0;
    GdkWindow *window;
    int retval = TRUE;
    static struct gerbv_redraw_state state;

    window = gtk_widget_get_parent_window(widget);
    /* This might be lengthy, show that we're busy by changing the pointer */
    if (window) {
	GdkCursor *cursor;

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
    }

    /* Stop the idle-function if we are not within an idle-call */
    if (state.valid) {
	stop_idle_redraw_pixmap(widget);
    }
    retval = FALSE;

    /* Called first when opening window and then when resizing window */
    for(i = 0; i < MAX_FILES; i++) {
	if (screen.file[i]) {
	    file_loaded++;
	}
    }

    /*
     * Setup scale etc first time we load a file
     */
    if (file_loaded && screen.scale == 0) {
	autoscale();
	invalidate_redraw_state(&state);
    }

    /*
     * Find the biggest image and use as a size reference
     */
    dmax_x = screen.gerber_bbox.x2;
    dmax_y = screen.gerber_bbox.y2;

    /*
     * Also find the smallest coordinates to see if we have negative
     * ones that must be compensated for.
     */
    dmin_x = screen.gerber_bbox.x1;
    dmin_y = screen.gerber_bbox.y1;

    /* Should we restart drawing, or try to load a saved state? */
    if (!restart && state.valid) {
	if (file_loaded != state.files_loaded) {
	    invalidate_redraw_state(&state);
	}

	if ((state.max_width != screen.drawing_area->allocation.width ||
	    state.max_height != screen.drawing_area->allocation.height)) {
	    invalidate_redraw_state(&state);
	}
    } else {
	invalidate_redraw_state(&state);
    }

    /* Check for useful data in saved state or initialise state */
    if (!state.valid) {
	int width = 0, height = 0;

	/*
	 * Paranoia check; size in width or height is zero
	 */
	if (!file_loaded) {
	    retval = FALSE;
	    goto redraw_pixmap_end;
	}

	/* Clear state */
	memset(&state, 0, sizeof(state));

	state.files_loaded = file_loaded;

	/*
	 * Pixmap size is always size of window, no
	 * matter how the scale.
	 */
	gdk_window_get_size(widget->window, &width, &height);	
	state.max_width = width;
	state.max_height = height;

	/* 
	 * Remove old pixmap, allocate a new one, draw the background.
	 */
	if (screen.pixmap) 
	    gdk_pixmap_unref(screen.pixmap);
	screen.pixmap = gdk_pixmap_new(widget->window, state.max_width,
				       state.max_height,  -1);
	gdk_gc_set_foreground(gc, screen.background);
	gdk_draw_rectangle(screen.pixmap, gc, TRUE, 0, 0, -1, -1);

	/*
	 * Allocate the pixmap and the clipmask (a one pixel pixmap)
	 */
	state.curr_pixmap = gdk_pixmap_new(widget->window,
					   state.max_width,
					   state.max_height,  -1);
	state.clipmask = gdk_pixmap_new(widget->window,
					state.max_width,
					state.max_height,  1);

	state.valid = 1;
    }

    /*
     * Set superimposing function.
     */
    gdk_gc_set_function(gc, screen.si_func);


    /* 
     * This now allows drawing several layers on top of each other.
     * Higher layer numbers have higher priority in the Z-order. 
     */
    for(i = state.file_index; i < MAX_FILES; i++) {
	if (g_main_pending()) {
	    /* return TRUE to keep this idle function active */
	    retval = TRUE;
	    start_idle_redraw_pixmap(widget);
	    state.file_index = i;
	    goto redraw_pixmap_end;
	}
	if (GTK_TOGGLE_BUTTON(screen.layer_button[i])->active &&
	    screen.file[i]) {
	    enum polarity_t polarity;

	    if (screen.file[i]->inverted) {
		if (screen.file[i]->image->info->polarity == POSITIVE)
		    polarity = NEGATIVE;
		else
		    polarity = POSITIVE;
	    } else {
		polarity = screen.file[i]->image->info->polarity;
	    }

	    /*
	     * Show progress in status bar
	     */
	    snprintf(screen.statusbar.msgstr, MAX_STATUSMSGLEN,
		     "%d %s...",
		     i, screen.file[i]->basename);
	    update_statusbar(&screen);

	    /*
	     * Fill up image with all the foreground color. Excess pixels
	     * will be removed by clipmask.
	     */
	    gdk_gc_set_foreground(gc, screen.file[i]->color);
	    gdk_draw_rectangle(state.curr_pixmap, gc, TRUE, 0, 0, -1, -1);

	    /*
	     * Translation is to get it inside the allocated pixmap,
	     * which is not always centered perfectly for GTK/X.
	     */
	    image2pixmap(&(state.clipmask),
			 screen.file[i]->image, screen.scale, 
			 (screen.clip_bbox.x1-dmin_x)*screen.scale,
			 (screen.clip_bbox.y1+dmax_y)*screen.scale,
			 polarity);

	    /* 
	     * Set clipmask and draw the clipped out image onto the
	     * screen pixmap. Afterwards we remove the clipmask, else
	     * it will screw things up when run this loop again.
	     */
	    gdk_gc_set_clip_mask(gc, state.clipmask);
	    gdk_gc_set_clip_origin(gc, 0, 0);
	    gdk_draw_pixmap(screen.pixmap, gc, state.curr_pixmap, -screen.off_x, -screen.off_y, 0, 0, -1, -1);
	    gdk_gc_set_clip_mask(gc, NULL);
	}
    }

    screen.statusbar.msgstr[0] = '\0';
    update_statusbar(&screen);
    /* Clean up */
    state.valid = 0;
    /* Free up pixmaps */
    if (state.curr_pixmap) {
	gdk_pixmap_unref(state.curr_pixmap);
    }
    if (state.clipmask) {
	gdk_pixmap_unref(state.clipmask);
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


static int
open_image(char *filename, int idx, int reload)
{
    gerb_file_t *fd;
    int r, g, b;
    GtkStyle *defstyle, *newstyle;
    gerb_image_t *parsed_image;
    gerb_verify_error_t error = GERB_IMAGE_OK;
    char *cptr;

    if (idx >= MAX_FILES) {
	GERB_MESSAGE("Couldn't open %s. Maximum number of files opened.\n",
		     filename);
	return -1;
    }

    fd = gerb_fopen(filename);
    if (fd == NULL) {
	GERB_MESSAGE("Trying to open %s:%s\n", filename, strerror(errno));
	return -1;
    }
    
    if(drill_file_p(fd))
	parsed_image = parse_drillfile(fd);
    else 
	parsed_image = parse_gerb(fd);

    gerb_fclose(fd);

    /*
     * Do error check before continuing
     */
    error = gerb_image_verify(parsed_image);
    if (error) {
	GERB_COMPILE_ERROR("%s: Parse error:\n", filename);
	if (error & GERB_IMAGE_MISSING_NETLIST)
	    GERB_COMPILE_ERROR("* Missing netlist\n");
	if (error & GERB_IMAGE_MISSING_FORMAT)
	    GERB_COMPILE_ERROR("* Missing format\n");
	if (error & GERB_IMAGE_MISSING_APERTURES) 
	    GERB_COMPILE_ERROR("* Missing apertures/drill sizes\n");
	if (error & GERB_IMAGE_MISSING_INFO)
	    GERB_COMPILE_ERROR("* Missing info\n");
	GERB_COMPILE_ERROR("\n");
	GERB_COMPILE_ERROR("You probably tried to read an RS-274D file, which gerbv doesn't support\n");
	free_gerb_image(parsed_image);
	return -1;
    }

    /* Used to debug parser */
    if (screen.dump_parsed_image)
	gerb_image_dump(parsed_image);

    /*
     * If reload, just exchange the image. Else we have to allocate
     * a new memory before we define anything more.
     */
    if (reload) {
	free_gerb_image(screen.file[idx]->image);
	screen.file[idx]->image = parsed_image;
	return 0;
    } else {
	screen.file[idx] = (gerbv_fileinfo_t *)malloc(sizeof(gerbv_fileinfo_t));
	memset((void *)screen.file[idx], 0, sizeof(gerbv_fileinfo_t));
	screen.file[idx]->image = parsed_image;
    }

    /*
     * Store filename for eventual reload
     * XXX Really should check retval from malloc!!! And use strncpy
     */
    screen.file[idx]->name = (char *)malloc(strlen(filename) + 1);
    strcpy(screen.file[idx]->name, filename);

    /*
     * Try to get a basename for the file
     */
    cptr = strrchr(filename, '/');
    if (cptr) {
	int len;

	len = strlen(cptr);
	screen.file[idx]->basename = (char *)malloc(len + 1);
	if (screen.file[idx]->basename) {
	    strncpy(screen.file[idx]->basename, cptr+1, len);
	    screen.file[idx]->basename[len] = '\0';
	} else {
	    screen.file[idx]->basename = screen.file[idx]->name;
	}
    } else {
	screen.file[idx]->basename = screen.file[idx]->name;
    }

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

    return 0;
} /* open_image */


static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
    return redraw_pixmap(widget, TRUE);
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
    case 4 : /* Scroll wheel */
	data.z_dir = ZOOM_IN_CMOUSE;
	do_zoom = TRUE;
	break;
    case 5 :  /* Scroll wheel */
	data.z_dir = ZOOM_OUT_CMOUSE;
	do_zoom = TRUE;
	break;
    default:
	break;
    }

    if (do_zoom) {
	data.z_event = event;
	zoom(widget, &data);
    }

    return TRUE;
} /* button_press_event */


#ifdef USE_GTK2
/* Scroll wheel */
static gint
scroll_event(GtkWidget *widget, GdkEventScroll *event)
{
    gerbv_zoom_data_t data;

    switch (event->direction) {
    case GDK_SCROLL_UP:
	data.z_dir = ZOOM_IN_CMOUSE;
	break;
    case GDK_SCROLL_DOWN:
	data.z_dir = ZOOM_OUT_CMOUSE;
	break;
    case GDK_SCROLL_LEFT: 
	/* Ignore */
	return TRUE;
    case GDK_SCROLL_RIGHT:
	/* Ignore */
	return TRUE;
    default:
	return TRUE;
    }

    /* XXX Ugly hack */
    data.z_event = (GdkEventButton *)event;
    zoom(widget, &data);

    return TRUE;
} /* scroll_event */
#endif


static gint
button_release_event (GtkWidget *widget, GdkEventButton *event)
{
    if (event->type == GDK_BUTTON_RELEASE) {
	if (screen.state == MOVE) {
	    screen.state = NORMAL;
	    /* Redraw the image(s) */
	    screen.off_x = 0;
	    screen.off_y = 0;
	    redraw_pixmap(screen.drawing_area, TRUE);
	} else if (screen.state == ZOOM_OUTLINE) {
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
    GdkCursor *cursor;
    gerbv_zoom_data_t z_data;

    switch (screen.state) {
    case NORMAL:
	switch(event->keyval) {
	case GDK_Shift_L:
	case GDK_Shift_R:
	    cursor = gdk_cursor_new(GDK_CROSSHAIR);
	    gdk_window_set_cursor(gtk_widget_get_parent_window(screen.drawing_area),
				  cursor);
	    gdk_cursor_destroy(cursor);
	    break;
	case GDK_Alt_L:
	case GDK_Alt_R: 
	    screen.state = ALT_PRESSED;
	    screen.selected_layer = -1;
	    break;
	case GDK_f:
	case GDK_F:
	    autoscale();
	    redraw_pixmap(screen.drawing_area, TRUE);
	    break;
	case GDK_z:
	    z_data.z_dir = ZOOM_IN;
	    zoom(widget, &z_data);
	    break;
	case GDK_Z:
	    z_data.z_dir = ZOOM_OUT;
	    zoom(widget, &z_data);
	    break;
	default:
	    break;
	}
	break;
    case ALT_PRESSED:
	if ((event->keyval >= GDK_KP_0) &&
	    (event->keyval <= GDK_KP_9)) {
	    if (screen.selected_layer == -1) 
		screen.selected_layer = event->keyval - GDK_KP_0;
	    else
		screen.selected_layer = 10 * screen.selected_layer + 
		    (event->keyval - GDK_KP_0);
	}
	break;
    default:
	break;
    }
	    
    /* Escape may be used to abort outline zoom and just plain repaint */
    if (event->keyval == GDK_Escape) {
	GdkRectangle update_rect;

	screen.state = NORMAL;

	screen.statusbar.diststr[0] = '\0';
	update_statusbar(&screen);

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


#define TOGGLE_BUTTON(button) gtk_toggle_button_set_active( \
                                      GTK_TOGGLE_BUTTON(button),\
				 !gtk_toggle_button_get_active( \
                                      GTK_TOGGLE_BUTTON(button)));

static gint
key_release_event (GtkWidget *widget, GdkEventKey *event)
{
    switch (screen.state) {
    case NORMAL:
	if((event->keyval == GDK_Shift_L) ||
	   (event->keyval == GDK_Shift_R)) {
	    gdk_window_set_cursor(gtk_widget_get_parent_window(screen.drawing_area),
				  GERBV_DEF_CURSOR);
	}
	break;
    case ALT_PRESSED:
	if ((event->keyval == GDK_Alt_L) ||
	     (event->keyval == GDK_Alt_R)) {
	    if ((screen.selected_layer != -1) &&
		(screen.selected_layer < MAX_FILES)){
		TOGGLE_BUTTON(screen.layer_button[screen.selected_layer]);
	    }
	    screen.state = NORMAL;
	}
    default:
	break;
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
	double X, Y;

	X = screen.gerber_bbox.x1 + (x+screen.trans_x)/(double)screen.scale;
	Y = (screen.gerber_bbox.y2 - (y+screen.trans_y)/(double)screen.scale);
	if (screen.unit == GERBV_MILS) {
	    snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
		     "X,Y (%7.1f, %7.1f)mils",
		     COORD2MILS(X), COORD2MILS(Y));
	} else /* unit is GERBV_MMS */ {
	    snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
		     "X,Y (%7.2f, %7.2f)mm",
		     COORD2MMS(X), COORD2MMS(Y));
	}
	update_statusbar(&screen);

	switch (screen.state) {
	case MOVE: {

	    x = widget->allocation.height - x;
	    y = widget->allocation.width - y;

	    if (screen.last_x != 0 || screen.last_y != 0) {
		screen.trans_x = screen.trans_x + x - screen.last_x;
		screen.trans_y = screen.trans_y + y - screen.last_y;

		screen.clip_bbox.x1 = -screen.trans_x/(double)screen.scale;
		screen.clip_bbox.y1 = -screen.trans_y/(double)screen.scale;

		/* Move pixmap to get a snappier feel of movement */
		screen.off_x += x - screen.last_x;
		screen.off_y += y - screen.last_y;
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
    if (screen.pixmap != NULL) {
	gdk_draw_pixmap(new_pixmap,
			widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			screen.pixmap, 
			event->area.x + screen.off_x, 
			event->area.y + screen.off_y, 
			event->area.x, event->area.y,
			event->area.width, event->area.height);
    }

    /*
     * Draw the whole thing onto screen
     */
    gdk_draw_pixmap(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    new_pixmap,
		    event->area.x, event->area.y,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);
#ifdef GERBV_DEBUG_OUTLINE
    {
	    double dx, dy;

	    dx = screen.gerber_bbox.x2-screen.gerber_bbox.x1;
	    dy = screen.gerber_bbox.y2-screen.gerber_bbox.y1;
	    gdk_gc_set_foreground(gc, screen.dist_measure_color);
	    gdk_draw_rectangle(widget->window, gc, FALSE, 
			       (screen.gerber_bbox.x1-1.1)*screen.scale - screen.trans_x,
			       ((screen.gerber_bbox.y1-0.6)*screen.scale - screen.trans_y),
			       dx*screen.scale,
			       dy*screen.scale);
    }
#endif /* DEBUG_GERBV_OUTLINE */

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
    if (screen.win.load_file && screen.win.load_file->window)
	gdk_window_raise(screen.win.load_file->window);
    if (screen.win.color_selection && screen.win.color_selection->window)
	gdk_window_raise(screen.win.color_selection->window);
    if (screen.win.export_png && screen.win.export_png->window)
	gdk_window_raise(screen.win.export_png->window);
    if (screen.win.scale && screen.win.scale->window)
	gdk_window_raise(screen.win.scale->window);
    if (screen.win.log && screen.win.log->window)
	gdk_window_raise(screen.win.log->window);

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
} /* draw_zoom_outline */


static void
draw_measure_distance(void)
{
    GdkGC *gc;
    GdkGCValues values;
    GdkGCValuesMask values_mask;
    gint x1, y1, x2, y2;
    GdkFont *font;

    if (screen.state != MEASURE)
	return;

    memset(&values, 0, sizeof(values));
    values.function = GDK_XOR;
    values.foreground = *screen.dist_measure_color;
    values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
    gc = gdk_gc_new_with_values(screen.drawing_area->window, &values,
				values_mask);
    font = gdk_font_load(setup.dist_fontname);

    x1 = MIN(screen.start_x, screen.last_x);
    y1 = MIN(screen.start_y, screen.last_y);
    x2 = MAX(screen.start_x, screen.last_x);
    y2 = MAX(screen.start_y, screen.last_y);

    gdk_draw_line(screen.drawing_area->window, gc, screen.start_x,
		  screen.start_y, screen.last_x, screen.last_y);
    if (font == NULL) {
	GERB_MESSAGE("Failed to load font '%s'\n", setup.dist_fontname);
    } else {
	gchar string[65];
	double delta, dx, dy;
	gint lbearing, rbearing, width, ascent, descent;
	gint linefeed;	/* Pseudonym for inter line gap */

	dx = (x2 - x1)/(double) screen.scale;
	dy = (y2 - y1)/(double) screen.scale;
	delta = sqrt(dx*dx + dy*dy); /* Pythagoras */

	snprintf(string, sizeof(string),
		 "[dist %7.1f, dX %7.1f, dY %7.1f] mils",
		 COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));

	gdk_string_extents(font, string, &lbearing, &rbearing, &width,
			   &ascent, &descent);
	gdk_draw_string(screen.drawing_area->window, font, gc,
			(x1+x2)/2-width/2, (y1+y2)/2, string);

	linefeed = ascent+descent;
	linefeed *= (double)1.2;

	snprintf(string, sizeof(string),
		 "[dist %7.2f, dX %7.2f, dY %7.2f] mm",
		 COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));

	gdk_string_extents(font, string, &lbearing, &rbearing, &width,
			   &ascent, &descent);
	gdk_draw_string(screen.drawing_area->window, font, gc,
			(x1+x2)/2 - width/2, (y1+y2)/2 + linefeed, string);

	gdk_font_unref(font);

	/*
	 * Update statusbar
	 */
	if (screen.unit == GERBV_MILS) {
	    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     " dist,dX,dY (%7.1f, %7.1f, %7.1f)mils",
		     COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));
	} else /* unit is GERBV_MMS */ {
	    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     " dist,dX,dY (%7.2f, %7.2f, %7.2f)mm",
		     COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));
	}
	update_statusbar(&screen);

    }
    gdk_gc_unref(gc);
} /* draw_measure_distance */


static void 
update_statusbar(gerbv_screen_t *scr)
{
    char str[MAX_STATUSMSGLEN+1];

    snprintf(str, MAX_STATUSMSGLEN, " %-*s|%-*s|%.*s",
	     MAX_COORDLEN-1, scr->statusbar.coordstr,
	     MAX_DISTLEN-1, scr->statusbar.diststr,
	     MAX_ERRMSGLEN-1, scr->statusbar.msgstr);
    if (scr->statusbar.msg != NULL) {
	    gtk_label_set_text(GTK_LABEL(scr->statusbar.msg), str);
    }
} /* update_statusbar */


static void
menu_ask_zoom (GtkWidget * widget, gpointer data)
{
    screen.win.scale = create_ZoomFactorWindow();
    gtk_widget_show(screen.win.scale);
    gtk_grab_add(screen.win.scale);
} /* menu_ask_zoom */
    

static GtkWidget *
create_ZoomFactorWindow(void)
{
    GtkWidget *ZoomFactorWindow;
    GtkWidget *table2;
    GtkObject *zoom_spinbutton1_adj;
    GtkWidget *zoom_spinbutton1;
    GtkWidget *zoom_cancel_button;
    GtkWidget *zoom_ok_button;
    GtkWidget *zoomwindowlabel;
    
    ZoomFactorWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_object_set_data (GTK_OBJECT (ZoomFactorWindow), "ZoomFactorWindow",
			 ZoomFactorWindow);
    gtk_window_set_title (GTK_WINDOW (ZoomFactorWindow), "Set Scale");
    /* XXX Hardcoded window size values */
    gtk_window_set_default_size(GTK_WINDOW(ZoomFactorWindow), 180, 50);
    
    table2 = gtk_table_new (2, 2, FALSE);
    gtk_widget_ref (table2);
    gtk_object_set_data_full (GTK_OBJECT (ZoomFactorWindow), "table2", table2,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (table2);
    gtk_container_add (GTK_CONTAINER (ZoomFactorWindow), table2);
    
    zoom_spinbutton1_adj = gtk_adjustment_new (1, 1, 65535, 1, 10, 10);
    zoom_spinbutton1 =
	gtk_spin_button_new (GTK_ADJUSTMENT (zoom_spinbutton1_adj), 1, 0);
    gtk_widget_ref (zoom_spinbutton1);
    gtk_object_set_data_full (GTK_OBJECT (ZoomFactorWindow),
			      "zoom_spinbutton1", zoom_spinbutton1,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (zoom_spinbutton1);
    gtk_table_attach (GTK_TABLE (table2), zoom_spinbutton1, 1, 2, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    zoom_cancel_button = gtk_button_new_with_label ("Cancel");
    gtk_widget_ref (zoom_cancel_button);
    gtk_object_set_data_full (GTK_OBJECT (ZoomFactorWindow),
			      "zoom_cancel_button", zoom_cancel_button,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (zoom_cancel_button);
    gtk_table_attach (GTK_TABLE (table2), zoom_cancel_button, 1, 2, 1, 2,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    zoom_ok_button = gtk_button_new_with_label ("OK");
    gtk_widget_ref (zoom_ok_button);
    gtk_object_set_data_full (GTK_OBJECT (ZoomFactorWindow), "zoom_ok_button",
			      zoom_ok_button,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (zoom_ok_button);
    gtk_table_attach (GTK_TABLE (table2), zoom_ok_button, 0, 1, 1, 2,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    zoomwindowlabel = gtk_label_new ("Zoom factor:");
    gtk_widget_ref (zoomwindowlabel);
    gtk_object_set_data_full (GTK_OBJECT (ZoomFactorWindow),
			      "zoomwindowlabel", zoomwindowlabel,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (zoomwindowlabel);
    gtk_table_attach (GTK_TABLE (table2), zoomwindowlabel, 0, 1, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (zoomwindowlabel), 0, 0.5);

    gtk_signal_connect(GTK_OBJECT(ZoomFactorWindow), "destroy",
		       GTK_SIGNAL_FUNC(zoom_cancel_button_clicked),
		       NULL);
    gtk_signal_connect (GTK_OBJECT (zoom_spinbutton1), "realize",
			GTK_SIGNAL_FUNC (zoom_spinbutton1_realize), NULL);
    gtk_signal_connect (GTK_OBJECT (zoom_cancel_button), "clicked",
			GTK_SIGNAL_FUNC (zoom_cancel_button_clicked),
			(gpointer) ZoomFactorWindow);
    gtk_signal_connect (GTK_OBJECT (zoom_ok_button), "clicked",
			GTK_SIGNAL_FUNC (zoom_ok_button_clicked),
			(gpointer) ZoomFactorWindow);

    return ZoomFactorWindow;
} /* create_ZoomFactorWindow */


void
zoom_spinbutton1_realize(GtkWidget * widget, gpointer user_data)
{
    gtk_spin_button_set_value ((GtkSpinButton *) widget,
			       (gfloat) screen.scale);
} /* zoom_spinbutton1_realize */


static void
zoom_ok_button_clicked(GtkButton * button, gpointer user_data)
{
    GtkSpinButton *ZoomSpin;
    int newscale;
    gerbv_zoom_data_t z_data;

    ZoomSpin = (GtkSpinButton *) lookup_widget ((GtkWidget *) button,
						"zoom_spinbutton1");
    newscale = gtk_spin_button_get_value_as_int (ZoomSpin);
    z_data.z_dir = ZOOM_SET;
    z_data.z_event = NULL;
    z_data.scale = newscale;
    zoom (screen.drawing_area, &z_data);

    gtk_grab_remove(screen.win.scale);
    gtk_widget_destroy (screen.win.scale);
    screen.win.scale = NULL;
} /* zoom_ok_button_clicked */


static void
zoom_cancel_button_clicked(GtkButton * button, gpointer user_data)
{
    gtk_grab_remove(screen.win.scale);
    gtk_widget_destroy(screen.win.scale);
    screen.win.scale = NULL;
} /* zoom_cancel_button_clicked */


GtkWidget *
lookup_widget (GtkWidget * widget, const gchar * widget_name)
{
    GtkWidget *parent, *found_widget;

    for (;;) {
	if (GTK_IS_MENU (widget))
	    parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
	else
	    parent = widget->parent;
	if (parent == NULL)
	    break;
	widget = parent;
    }

    found_widget = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (widget),
						      widget_name);
    if (!found_widget)
	g_warning ("Widget not found: %s", widget_name);
    return found_widget;
} /* lookup_widget */


void
rename_main_window(char *filename, GtkWidget *main_win)
{
    char *win_title;
    static GtkWidget *win=NULL;
    size_t len;

    if( main_win != NULL )
	win = main_win;

    assert(win != NULL);

    len = strlen(WIN_TITLE) + strlen(VERSION) + 2 + strlen(filename) + 1;
    win_title = (char *)malloc(len);
    snprintf(win_title, len, "%s%s: %s", WIN_TITLE, VERSION, filename);
    gtk_window_set_title(GTK_WINDOW(win), win_title);
    free(win_title);
				 
}

#ifdef HAVE_GETOPT_LONG
int longopt_val = 0;
int longopt_idx = 0;
const struct option longopts[] = {
    /* name              has_arg            flag  val */
    {"version",          no_argument,       NULL,    'V'},
    {"help",             no_argument,       NULL,    'h'},
    {"batch",            required_argument, NULL,    'b'},
    {"log",              required_argument, NULL,    'l'},
    {"tools",            required_argument, NULL,    't'},
    {"project",          required_argument, NULL,    'p'},
    {"dump",             no_argument,       NULL,    'd'},
    {"geometry",         required_argument, &longopt_val, 1},
    /* GDK/GDK debug flags to be "let through" */
    {"gtk-module",       required_argument, &longopt_val, 2},
    {"g-fatal-warnings", no_argument,       &longopt_val, 2},
    {"gtk-debug",        required_argument, &longopt_val, 2},
    {"gtk-no-debug",     required_argument, &longopt_val, 2},
    {"gdk-debug",        required_argument, &longopt_val, 2},
    {"gdk-no-debug",     required_argument, &longopt_val, 2},
    {"display",          required_argument, &longopt_val, 2},
    {"sync",             no_argument,       &longopt_val, 2},
    {"no-xshm",          no_argument,       &longopt_val, 2},
    {"name",             required_argument, &longopt_val, 2},
    {"class",            required_argument, &longopt_val, 2},
    {0, 0, 0, 0},
};
#endif /* HAVE_GETOPT_LONG*/
const char *opt_options = "Vhl:t:p:d";

int
main(int argc, char *argv[])
{
    GtkWidget *main_win;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *menubar;
    GtkStyle  *textStyle;
    gint      screen_width, width, height;
    int       read_opt;
    int       i;
    int       req_width = -1, req_height = -1, req_x = 0, req_y = 0;
    char      *rest, *project_filename = NULL;

    /*
     * Setup the screen info. Must do this before getopt, since getopt
     * eventually will set some variables in screen.
     */
    memset((void *)&screen, 0, sizeof(gerbv_screen_t));
    screen.state = NORMAL;
    screen.execpath = dirname(argv[0]);

    setup_init();
	
#ifdef HAVE_GETOPT_LONG
    while ((read_opt = getopt_long(argc, argv, opt_options, 
				   longopts, &longopt_idx)) != -1) {
#else
    while ((read_opt = getopt(argc, argv, opt_options)) != -1) {
#endif /* HAVE_GETOPT_LONG */
	switch (read_opt) {
#ifdef HAVE_GETOPT_LONG
	case 0:
	    /* Only long options like GDK/GTK debug */
	    switch (longopt_val) {
	    case 0: /* default value if nothing is set */
		printf("Not handled option %s\n", longopts[longopt_idx].name);
		break;
	    case 1: /* geometry */
		req_width = (int)strtol(optarg, &rest, 10);
		if (rest[0] != 'x'){
		    printf("Split X and Y parameters with an x\n");
		    break;
		}
		rest++;
		req_height = (int)strtol(rest, &rest, 10);
		if ((rest[0] == 0) || ((rest[0] != '-') && (rest[0] != '+')))
		    break;
		req_x = (int)strtol(rest, &rest, 10);
		if ((rest[0] == 0) || ((rest[0] != '-') && (rest[0] != '+')))
		    break;
		req_y = (int)strtol(rest, &rest, 10);
		break;
	    default:
		break;
	    }
	    break;
#endif /* HAVE_GETOPT_LONG */
	case 'V' :
	    printf("gerbv version %s\n", VERSION);
	    printf("(C) Stefan Petersen (spe@stacken.kth.se)\n");
	    exit(0);
	case 'l' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a filename to send log to\n");
		exit(1);
	    }
	    setup.log.to_file = 1;
	    setup.log.filename = optarg;
	    break;
	case 'p' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a project filename\n");
		exit(1);
	    }
	    project_filename = optarg;
	    break;
	case 't' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a filename to read the tools from.\n");
		exit(1);
	    }
	    if (!ProcessToolsFile(optarg)) {
		fprintf(stderr, "*** ERROR processing tools file \"%s\".\n", optarg);
		fprintf(stderr, "Make sure all lines of the file are formatted like this:\n");
		fprintf(stderr, "T01 0.024\nT02 0.032\nT03 0.040\n...\n");
		fprintf(stderr, "*** EXITING to prevent erroneous display.\n");
		exit(1);
            }
	    break;
	case 'd':
	    screen.dump_parsed_image = 1;
	    break;
	case '?':
	case 'h':
#ifdef HAVE_GETOPT_LONG
	    fprintf(stderr, "Usage : %s [FLAGS] <gerber file(s)>\n", argv[0]);
	    fprintf(stderr, "where FLAGS could be any of\n");
	    fprintf(stderr, "  --version|-V : Prints version of gerbv\n");
	    fprintf(stderr, "  --help|-h : Prints this help message\n");
	    fprintf(stderr, "  --log=<logfile>|-l <logfile> : Send error messages to <logfile>\n");
	    fprintf(stderr, "  --project=<prjfile>|-p <prjfile> : Load project file <prjfile>\n");
	    fprintf(stderr, "  --tools=<toolfile>|-t <toolfile> : Read Excellon tools from file <toolfile>\n");
	    exit(1);
	    break;
#else
	    fprintf(stderr, "Usage : %s [FLAGS] <gerber file(s)>\n", argv[0]);
	    fprintf(stderr, "where FLAGS could be any of\n");
	    fprintf(stderr, "  -V : Prints version of gerbv\n");
	    fprintf(stderr, "  -h : Prints this help message\n");
	    fprintf(stderr, "  -l <logfile> : Send error messages to <logfile>\n");
	    fprintf(stderr, "  -p <prjfile> : Load project file <prjfile>\n");
	    fprintf(stderr, "  -t <toolfile> : Read Excellon tools from file <toolfile>\n");
	    exit(1);
	    break;
#endif /* HAVE_GETOPT_LONG */
	default :
	    printf("Not handled option [%d=%c]\n", read_opt, read_opt);
	}
    }
    
    /*
     * Init GTK+
     */
    gtk_init(&argc, &argv);

    /* 
     * Good defaults according to Ales. Gives aspect ratio of 1.3333...
     */
    if ((req_width != -1) && (req_height != -1)) {
	width = req_width;
	height = req_height;
    } else {
	screen_width = gdk_screen_width();
	width = screen_width * 3/4;
	height = width * 3/4;
    }

    /*
     * Setup some GTK+ defaults
     */
    screen.tooltips = gtk_tooltips_new();        
    screen.background = alloc_color(0, 0, 0, "black");
    screen.zoom_outline_color  = alloc_color(0, 0, 0, "gray");
    screen.dist_measure_color  = alloc_color(0, 0, 0, "lightblue");

    /*
     * Set console error log handler. The default gives us error levels in
     * in the beginning which I don't want.
     */
    g_log_set_handler (NULL, 
		       G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION | G_LOG_LEVEL_MASK, 
		       gerbv_console_log_handler, NULL); 

    /* Set default unit to the configured default */
    screen.unit = GERBV_DEFAULT_UNIT;

    /*
     * Main window 
     */
    main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    rename_main_window("", main_win);
#ifdef USE_GTK2
    g_signal_connect(GTK_OBJECT(main_win), "delete_event", G_CALLBACK(destroy), NULL);
#else
    gtk_signal_connect(GTK_OBJECT(main_win), "delete_event", destroy, NULL);
    gtk_signal_connect(GTK_OBJECT(main_win), "destroy", destroy, NULL);
#endif

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
     * Add status bar (three sections: messages, abs and rel coords)
     */
    hbox = gtk_hbox_new(FALSE, 0);
    screen.statusbar.msg = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(screen.statusbar.msg), GTK_JUSTIFY_LEFT);
    textStyle = gtk_style_new();
#ifndef USE_GTK2
    textStyle->font = gdk_font_load(setup.status_fontname);
#else
    textStyle->font_desc = pango_font_description_from_string(setup.status_fontname);
#endif
    gtk_widget_set_style(GTK_WIDGET(screen.statusbar.msg), textStyle);
    screen.statusbar.msgstr[0] = '\0';
    screen.statusbar.coordstr[0] = '\0';
    screen.statusbar.diststr[0] = '\0';
    gtk_box_pack_start(GTK_BOX(hbox), screen.statusbar.msg, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    /*
     * If project is given, load that one and use it for files and colors.
     * Else load files (eventually) given on the command line.
     * This limits you to either give files on the commandline or just load
     * a project.
     */
    if (project_filename) {
	project_list_t *project_list;

	project_list = read_project_file(project_filename);
	
	if (project_list) {
	    load_project(project_list);
	    if (screen.project) {
		free(screen.project);
		screen.project = NULL;
	    }
	    screen.project = (char *)malloc(strlen(project_filename) + 1);
	    memset((void *)screen.project, 0, strlen(project_filename) + 1);
	    strncpy(screen.project, project_filename, strlen(project_filename));
	    /*
	     * Remember where we loaded file from last time
	     */
	    if (screen.path)
		free(screen.path);
	    screen.path = (char *)malloc(strlen(project_filename) + 1);
	    strcpy(screen.path, project_filename);
	    dirname(screen.path);
	    screen.path = strncat(screen.path, "/", 1);
	    
            rename_main_window(screen.project, NULL);
	} else {
	    GERB_MESSAGE("Failed to load project\n");
	}

    } else {
	for(i = optind ; i < argc; i++)
	    if (open_image(argv[i], i - optind, FALSE) == -1)
		exit(-1);
    }

    /*
     * Set gtk error log handler
     */
    g_log_set_handler (NULL, 
		       G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION | G_LOG_LEVEL_MASK, 
		       gerbv_gtk_log_handler, NULL); 

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
#ifdef USE_GTK2
    gtk_signal_connect_after(GTK_OBJECT(main_win), "scroll_event",
		       GTK_SIGNAL_FUNC(scroll_event), NULL);
#endif

    gtk_widget_set_events(screen.drawing_area, GDK_EXPOSURE_MASK
			  | GDK_LEAVE_NOTIFY_MASK
			  | GDK_BUTTON_PRESS_MASK
			  | GDK_BUTTON_RELEASE_MASK
			  | GDK_KEY_PRESS_MASK
			  | GDK_KEY_RELEASE_MASK
			  | GDK_POINTER_MOTION_MASK
			  | GDK_POINTER_MOTION_HINT_MASK
#ifdef USE_GTK2
			  | GDK_SCROLL_MASK
#endif
			  );

    gtk_widget_show_all(main_win);
    set_window_icon(main_win);

    /* It seems this has to be done after the button is shown for
       the first time, or we get a segmentation fault */
    if (project_filename)
	all_layers_on(NULL, NULL);
    else
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(screen.layer_button[0]),
				     TRUE);

    gtk_main();
    
    return 0;
} /* main */
