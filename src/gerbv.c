/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif


#include "gerber.h"
#include "drill.h"
#ifndef NO_GUILE
#include "scm_gerber.h"
#endif
#include "draw.h"
#include "color.h"

#define INITIAL_SCALE 200
#define MAX_FILES 20

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif

enum gerbv_state_t {NORMAL, MOVE};


typedef struct gerbv_fileinfo {
    gerb_image_t *image;
    GdkColor *color;
} gerbv_fileinfo_t;


typedef struct gerbv_screen {
    GtkWidget *drawing_area;
    GdkPixmap *pixmap;
    GdkColor  *background;
    GdkColor  *err_color;

    gerbv_fileinfo_t *file[MAX_FILES];
    int curr_index;

    GtkTooltips *tooltips;
    GtkWidget *layer_button[MAX_FILES];

    enum gerbv_state_t state;

    int scale;

    gint last_x;
    gint last_y;

    int trans_x; /* Translate offset */
    int trans_y;
} gerbv_screen_t;

gerbv_screen_t screen;


static gint expose_event (GtkWidget *widget, GdkEventExpose *event);
static void open_file_popup(GtkWidget *widget, gpointer data);
static void zoom(GtkWidget *widget, gpointer data);
static gint redraw_pixmap(GtkWidget *widget);
static void open_image(char *filename, int index);


void
destroy(GtkWidget *widget, gpointer data)
{
    int i;

    for (i = 0; i < MAX_FILES; i++) {
	if (screen.file[i] != NULL && 
	    screen.file[i]->image != NULL) {
	    free_gerb_image(screen.file[i]->image);
	    free(screen.file[i]);
	}
    }
    gtk_main_quit();
} /* destroy */


static GtkItemFactoryEntry menu_items[] = {
    {"/_File",      NULL,          NULL,    0, "<Branch>"},
    {"/File/Open _File...", "<alt>F", open_file_popup,    0, NULL},
    {"/File/sep1",  NULL,          NULL,    0, "<Separator>"},
    {"/File/_Quit", "<alt>Q", destroy  ,    0, NULL},
    {"/_Zoom",      NULL,          NULL,    0, "<Branch>"},
    {"/Zoom/_Out",  "<alt>O", zoom     ,    0, NULL},
    {"/Zoom/_In",   "<alt>I", zoom     ,    1, NULL},
    {"/Zoom/sep1",  NULL,          NULL,    0, "<Separator>"},
    {"/Zoom/_Clear All",NULL, zoom     ,    2, NULL},
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


static void
cb_layer_button(GtkWidget *widget, gpointer data)
{
    
    screen.curr_index = (long int)data;

    /* Redraw the image(s) */
    redraw_pixmap(screen.drawing_area);

} /* cb_layer_button */


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
	gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
	screen.layer_button[bi] = button;
    }

    return box;
} /* create_layer_buttons */


static void
cb_ok_open_file(GtkWidget *widget, GtkFileSelection *fs)
{
    open_image(gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs)), 
	       screen.curr_index);
    
    /* Make loaded image appear on screen */
    redraw_pixmap(screen.drawing_area);
    
    return;
} /* cb_ok_open_file */


static void
open_file_popup(GtkWidget *widget, gpointer data)
{
    /* File Selection Window */
    GtkWidget *fsw;

    fsw = gtk_file_selection_new("Select File To View");
    
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->ok_button),
		       "clicked", GTK_SIGNAL_FUNC(cb_ok_open_file), 
		       (gpointer)fsw);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->ok_button),
			      "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
			      (gpointer)fsw);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->cancel_button),
			      "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
			      (gpointer)fsw);
    
    gtk_widget_show(fsw);
    
    return;
} /* open_file_popup */


static void
zoom(GtkWidget *widget, gpointer data)
{
    if (screen.file[screen.curr_index] == NULL)
	return;

    switch((long int)data) {
    case 0 :  /* Zoom In */
	if (screen.scale > 10) {
	    /* The translation vaules are crap.
	       This _must_ be done in a better way. */
	    screen.trans_x += 25;
	    screen.trans_y -= 19; /* 25.4 * 3/4 */
	    screen.scale -= 10;
	}

	break;
    case 1 : /* Zoom Out */
	screen.trans_x -= 25;
	screen.trans_y += 19; /* 25.4 * 3/4 */
	screen.scale += 10;
	break;
    case 2 : /* Clear All */
	screen.scale = INITIAL_SCALE;
	screen.trans_x = 0;
	screen.trans_y = screen.drawing_area->allocation.height;
	break;
    default :
	fprintf(stderr, "Illegal zoom direction %ld\n", (long int)data);
    }
    
    redraw_pixmap(screen.drawing_area);
    
    return;
} /* zoom */


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
    int max_width = 0, max_height = 0;
    GdkGC *gc = gdk_gc_new(widget->window);
    GdkRectangle update_rect;

    /* Called first when opening window and then when resizing window */


    for(i = 0; i < MAX_FILES; i++) {
	if (screen.file[i]) {

	    /* 
	     * Find the biggest image and use as a size reference
	     */
	    int width, height;
	    width = (int)ceil(screen.file[i]->image->info->max_x * 
			       (double)screen.scale);
	    height = (int)ceil(screen.file[i]->image->info->max_y * 
				(double)screen.scale);
	    if (width > max_width) max_width = width;
	    if (height > max_height) max_height = height;

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

    if ((max_width == 0) && (max_height == 0)) 
	return FALSE;

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
    
    /* This now allows drawing several layers on top of each other.
       Higher layer numbers have higher priority in the Z-order. */
    for(i = 0; i < MAX_FILES; i++) {
	if (GTK_TOGGLE_BUTTON(screen.layer_button[i])->active &&
	    screen.file[i]) {
	    image2pixmap(&screen.pixmap, screen.file[i]->image, 
			 screen.scale, 
			 0, max_height,
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

    return TRUE;
} /* redraw_pixmap */


static void
open_image(char *filename, int index)
{
    gerb_file_t *fd;
    int r, g, b;
    GtkStyle *defstyle, *newstyle;

    if (index >= MAX_FILES) {
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
    screen.file[index] = (gerbv_fileinfo_t *)malloc(sizeof(gerbv_fileinfo_t));
    if(drill_file_p(fd))
	screen.file[index]->image = parse_drillfile(fd);
    else 
	screen.file[index]->image = parse_gerb(fd);

    /*
     * Calculate a "clever" random color based on index.
     */
    r = (12341 + 657371 * index) % (int)(MAX_COLOR_RESOLUTION);
    g = (23473 + 434382 * index) % (int)(MAX_COLOR_RESOLUTION);
    b = (90341 + 123393 * index) % (int)(MAX_COLOR_RESOLUTION);

    screen.file[index]->color = alloc_color(r, g, b, NULL);

    defstyle = gtk_widget_get_default_style();
    newstyle = gtk_style_copy(defstyle);
    newstyle->bg[GTK_STATE_NORMAL] = *(screen.file[index]->color);
    newstyle->bg[GTK_STATE_ACTIVE] = *(screen.file[index]->color);
    newstyle->bg[GTK_STATE_PRELIGHT] = *(screen.file[index]->color);
    gtk_widget_set_style(screen.layer_button[index], newstyle);

    gtk_tooltips_set_tip(screen.tooltips, screen.layer_button[index],
			 filename, NULL); 
    gerb_fclose(fd);

    return;
} /* open_image */


static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
    return redraw_pixmap(widget);
} /*configure_event */


static gint
button_press_event (GtkWidget *widget, GdkEventButton *event)
{
    switch (event->button) {
    case 1 :
	screen.state = MOVE;
	screen.last_x = widget->allocation.height - event->x;
	screen.last_y = widget->allocation.width  - event->y;
	break;
    case 2 :
	/* And now, some Veribest-like mouse commands for
	   all us who dislike scroll wheels ;) */
	if((event->state & GDK_SHIFT_MASK) != 0) {
	    /* Middle button + shift == zoom in */
	    zoom(widget, (gpointer)0);
	} else {
	    /* Only middle button == zoom out */
	    zoom(widget, (gpointer)1);
	}
	break;
    case 3 :
	/* Add color selection code here? */
	break;
    case 4 : 
	zoom(widget, (gpointer)1);
	break;
    case 5 : 
	zoom(widget, (gpointer)0);
	break;
    default:
    }
    
    return TRUE;
} /* button_press_event */


static gint
button_release_event (GtkWidget *widget, GdkEventButton *event)
{
    if (event->type == GDK_BUTTON_RELEASE)
	screen.state = NORMAL;

    return TRUE;
} /* button_release_event */


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
    
    x = widget->allocation.height - x;
    y = widget->allocation.width - y;

    if (screen.state == MOVE && screen.pixmap != NULL) {
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

    return FALSE;
} /* expose_event */


#ifndef NO_GUILE
static void
batch(char *backend, char *filename)
{
    char         *path[3];
    char 	 *complete_path;
    char         *home;
    int		  i;
    gerb_file_t  *fd;
    gerb_image_t *image;
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
#endif


#ifdef HAVE_GETOPT_LONG
static struct option longopts[] = {
    /* name     has_arg            flag  val */
    {"version", no_argument,       NULL, 'V'},
    {"batch",   required_argument, NULL, 'b'},
    {0, 0, 0, 0},
};
#endif


void
internal_main(int argc, char *argv[])
{
    GtkWidget *main_win;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *menubar;
    gint	   screen_width, width, height;
    int	   run_batch = 0;
    int 	   i;
    char      *backend = NULL;
    char 	   read_opt;
    
#ifdef HAVE_GETOPT_LONG
    while ((read_opt = getopt_long(argc, argv, "Vb:", 
				   longopts, NULL)) != -1) {
#else
    while ((read_opt = getopt(argc, argv, "Vb:")) != -1) {
#endif
	switch (read_opt) {
	case 'V' :
	    printf("gerbv version %s\n", VERSION);
	    printf("(C) Stefan Petersen (spe@stacken.kth.se)\n");
	    exit(0);
	case 'b' :
#ifdef NO_GUILE
	    fprintf(stderr, "This version doesn't have batch support\n");
	    exit(0);
#else
	    run_batch = 1;
	    if (optarg == NULL)
		err(1, "You must give a backend in batch mode\n");
	    
	    backend = (char *)malloc(strlen(optarg) + strlen("gerb-.scm") + 1);
	    if (backend == NULL) 
		err(1, "Failed mallocing backend string\n");
	    strcpy(backend, "gerb-");
	    strcat(backend, optarg);
	    strcat(backend, ".scm");
#endif
	    break;
	case '?':
	    fprintf(stderr, "Usage : %s [--version|-V][--batch=<backend>|-b <backend>] <gerber file(s)>\n", argv[0]);
	    exit(1);
	    break;
	default :
	    printf("Not handled option [%d=%c]\n", read_opt, read_opt);
	}
    }
    
#ifndef NO_GUILE
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
#endif
    /*
     * Setup the screen info
     */
    memset((void *)&screen, 0, sizeof(gerbv_screen_t));
    screen.state = NORMAL;
    screen.scale = INITIAL_SCALE;
	
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
     * Translation values set so that 0,0 is in the bottom left corner
     */
    screen.trans_x = 0;
    screen.trans_y = 0;

    /*
     * Setup some GTK+ defaults
     */
    screen.tooltips = gtk_tooltips_new();        
    screen.background = alloc_color(0, 0, 0, "black");
    screen.err_color  = alloc_color(0, 0, 0, "red1");

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
     * Build layer buttons
     */
    gtk_box_pack_start(GTK_BOX(hbox), create_layer_buttons(MAX_FILES), 
		       FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    /*
     * Fill with files (eventually) given on command line
     */
    for(i = optind ; i < argc; i++)
	open_image(argv[i], i - optind);

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

    gtk_widget_set_events(screen.drawing_area, GDK_EXPOSURE_MASK
			  | GDK_LEAVE_NOTIFY_MASK
			  | GDK_BUTTON_PRESS_MASK
			  | GDK_BUTTON_RELEASE_MASK
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
#ifdef NO_GUILE
    internal_main(argc, argv);
#else
    gh_enter(argc, argv, internal_main);
#endif
    return 0;
}
