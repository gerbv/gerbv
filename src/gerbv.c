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
#ifndef NO_GUILE
#include "scm_gerber.h"
#endif
#include "draw.h"

#define BUTTON
/*#define ARC_DEBUG*/

#define INITIAL_SCALE 200
#define MAX_FILES 10

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif

enum gerbv_state {NORMAL, MOVE};

struct gerbv_color {
    char *name;
    GdkColor *color;
};

static struct gerbv_color colors [] = {
    {"grey50", NULL},
    {"magenta2", NULL},
    {"purple2", NULL},
    {"white", NULL},
    {"green", NULL},
    {"blue", NULL},
    {"yellow", NULL},
    {"red", NULL},
};

static struct gerbv_color background = {"black", NULL};

struct gerbv_fileinfo {
    struct gerb_image *image;
    int color_index;
};


struct gerbv_screen {
    GtkWidget *drawing_area;
    GdkPixmap *pixmap;

    struct gerbv_fileinfo *file[MAX_FILES];
    int curr_index;

    enum gerbv_state state;

    int scale;

    gint last_x;
    gint last_y;

    gint trans_x; /* Translate offset */
    gint trans_y;
};

struct gerbv_screen screen;


static gint expose_event (GtkWidget *widget, GdkEventExpose *event);
static void open_file(GtkWidget *widget, gpointer data);
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
    {"/File/_Open", "<alt>O", open_file,    0, NULL},
    {"/File/sep1",  NULL,          NULL,    0, "<Separator>"},
    {"/File/_Quit", "<alt>Q", destroy  ,    0, NULL},
    {"/_Zoom",      NULL,          NULL,    0, "<Branch>"},
    {"/Zoom/_Out",  "<alt>P", zoom     ,    0, NULL},
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
       Param 2: The path og the menu.
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
} /* create_menu_bar */


static void
alloc_colors(void)
{
    int nuf_colors = sizeof(colors)/sizeof(colors[0]);
    int i;
    GdkColormap *colormap = gdk_colormap_get_system();
    
    for (i = 0; i < nuf_colors; i++) {
	colors[i].color = (struct _GdkColor *)malloc(sizeof(struct _GdkColor));
	gdk_color_parse(colors[i].name, colors[i].color);
	gdk_colormap_alloc_color(colormap, colors[i].color, FALSE, TRUE);
    }
    
    background.color = (struct _GdkColor *)malloc(sizeof(struct _GdkColor));
    gdk_color_parse(background.name, background.color);
    gdk_colormap_alloc_color(colormap, background.color, FALSE, TRUE);
    
    return;
} /* alloc_colors */


static void
cb_radio_button(GtkWidget *widget, gpointer data)
{
    long int button = (long int)data;
    
    if (GTK_TOGGLE_BUTTON(widget)->active &&
	button != screen.curr_index) {
	screen.curr_index = button;
	/* Make loaded image appear on screen */
	redraw_pixmap(screen.drawing_area);
    }
} /* cb_radio_button */


static GtkWidget *
create_radio_buttons(int nuf_buttons)
{
    GtkWidget *button = NULL;
    GtkWidget *box = NULL;
    GSList    *button_group = NULL;
    char      info[5];
    long int bi;
    

    box = gtk_vbox_new(FALSE, 0);
    
    for (bi = 0; bi < nuf_buttons; bi++) {
	sprintf(info, "%d", bi);
	button = gtk_radio_button_new_with_label(button_group, info);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", 
			   GTK_SIGNAL_FUNC(cb_radio_button), (gpointer)bi);
	button_group = gtk_radio_button_group(GTK_RADIO_BUTTON(button));
	gtk_box_pack_start(GTK_BOX(box), button, TRUE, FALSE, 0);
	if (bi == 0)
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    }
    
    return box;
} /* create_radio_buttons */


static void
cb_ok_open_file(GtkWidget *widget, GtkFileSelection *fs)
{
    open_image(gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs)), screen.curr_index);
    
    /* Make loaded image appear on screen */
    redraw_pixmap(screen.drawing_area);
    
    return;
} /* cb_ok_open_file */


static void
open_file(GtkWidget *widget, gpointer data)
{
    /* File Selection Window */
    GtkWidget *fsw;

    fsw = gtk_file_selection_new("Select Gerberfile To View");
    
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->ok_button),
		       "clicked", GTK_SIGNAL_FUNC(cb_ok_open_file), (gpointer)fsw);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->ok_button),
			      "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)fsw);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->cancel_button),
			      "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)fsw);
    
    gtk_widget_show(fsw);
    
    return;
} /* open_file */

static void
zoom(GtkWidget *widget, gpointer data)
{
    if (screen.file[screen.curr_index] == NULL)
	return;
    
    switch((long int)data) {
    case 0 :  /* Zoom In */
	if (screen.scale > 0) 
	    screen.scale -= 10;
	break;
    case 1 : /* Zoom Out */
	screen.scale += 10;
	break;
    case 2 : /* Clear All */
	screen.scale = INITIAL_SCALE;
	screen.trans_x = 0;
	screen.trans_y = 0;
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
    GdkGC *gc = gdk_gc_new(widget->window);
    
    /* Called first when opening window and then when resizing window */
    if (screen.pixmap)
	gdk_pixmap_unref(screen.pixmap);
    
    screen.pixmap = gdk_pixmap_new(widget->window,
				   widget->allocation.width,
				   widget->allocation.height,
				   -1);
    
    if (screen.file[screen.curr_index] &&
	screen.file[screen.curr_index]->image->info->polarity == NEGATIVE) 
	/* Current line color */
	gdk_gc_set_foreground(gc, colors[screen.file[screen.curr_index]->color_index].color);
    else
	/* Black */
	gdk_gc_set_foreground(gc, background.color);
    
    gdk_draw_rectangle(screen.pixmap,
		       gc,
		       TRUE,
		       0, 0,
		       widget->allocation.width,
		       widget->allocation.height);

    if (screen.file[screen.curr_index])
	image2pixmap(&screen.pixmap, screen.file[screen.curr_index]->image, 
		     screen.scale, screen.trans_x, screen.trans_y,
		     screen.file[screen.curr_index]->image->info->polarity,
		     colors[screen.file[screen.curr_index]->color_index].color,
		     background.color);
    
    gdk_draw_pixmap(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    screen.pixmap,
		    0, 0,
		    0, 0,
		    widget->allocation.width, widget->allocation.height);
    
    return TRUE;
} /* redraw_pixmap */


static void
open_image(char *filename, int index)
{
    FILE *fd;
    
    fd = fopen(filename, "r");
    if (fd == NULL) {
	perror("fopen");
	exit(1);
    }
    screen.file[index] = (struct gerbv_fileinfo *)malloc(sizeof(struct gerbv_fileinfo));
    screen.file[index]->image = parse_gerb(fd);
    screen.file[index]->color_index = index;
    fclose(fd);
    
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
	screen.last_x = event->x, screen.last_y = event->y;
	break;
    case 2 :
	break;
    case 3 :
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
    
    if (event->is_hint)
	gdk_window_get_pointer (event->window, &x, &y, &state);
    else {
	x = event->x;
	y = event->y;
	state = event->state;
    }
    
    if (screen.state == MOVE && screen.pixmap != NULL) {
	if (screen.last_x != 0 || screen.last_y != 0) {
	    screen.trans_x = screen.trans_x + x- screen.last_x;
	    screen.trans_y = screen.trans_y + y - screen.last_y;
	}
	screen.last_x = x, screen.last_y = y;
	redraw_pixmap(widget);
    }
    
    return TRUE;
} /* motion_notify_event */

/* Redraw the screen from the backing pixmap */
static gint
expose_event (GtkWidget *widget, GdkEventExpose *event)
{
    gdk_draw_pixmap(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    screen.pixmap,
		    event->area.x, event->area.y,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);
    
    return FALSE;
} /* expose_event */


#ifndef NO_GUILE
static void
batch(char *backend, char *file)
{
    char              *path[3];
    char 	      *complete_path;
    char              *home;
    int		       i;
    FILE              *fd;
    struct gerb_image *image;
    SCM	               scm_image;

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
    fd = fopen(file, "r");
    if (fd == NULL) {
	perror("fopen");
	exit(1);
    }
    image = parse_gerb(fd);
    fclose(fd);
    
    /*
     * Convert it to Scheme
     */
    scm_image = scm_image2scm(image, file);
    
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
    int 	   option_index;
    
#ifdef HAVE_GETOPT_LONG
    while ((read_opt = getopt_long(argc, argv, "Vb:", 
				   longopts, &option_index)) != -1) {
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
	    strcpy(backend, "gerb-");
	    strcat(backend, optarg);
	    strcat(backend, ".scm");
#endif
	    break;
	case '?':
	    fprintf(stderr, "Usage : %s [--batch=<backend>|-b <backend>]\n", argv[0]);
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
	
	for(i = optind ; i < argc; i++) {
	    printf("%s\n", argv[i]);
	    batch(backend, argv[i]);
	    free(backend);
	}
	exit(0);
    }
#endif
    /*
     * Setup the screen info
     */
    bzero((void *)&screen, sizeof(struct gerbv_screen));
    screen.state = NORMAL;
    screen.scale = INITIAL_SCALE;
    
    gtk_init(&argc, &argv);
    
    /* 
     * Good defaults according to Ales. Gives aspect ratio of 1.3333...
     */
    screen_width = gdk_screen_width();
    width = screen_width * 3/4;
    height = width * 3/4;
    
    alloc_colors();
    
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
     * Create drawing area and fill with eventually given on command line
     */
    screen.drawing_area = create_drawing_area(width, height);
    gtk_box_pack_start(GTK_BOX(hbox), screen.drawing_area, TRUE, TRUE, 0);
    for(i = optind ; i < argc; i++)
	open_image(argv[i], i - optind);
    
    gtk_box_pack_start(GTK_BOX(hbox), create_radio_buttons(MAX_FILES), FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    
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
