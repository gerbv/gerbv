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
#include <math.h>  /* ceil(), atan2() */

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
}

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
}


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
}


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
}


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
}


static void
cb_ok_open_file(GtkWidget *widget, GtkFileSelection *fs)
{
    open_image(gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs)), screen.curr_index);
    
    /* Make loaded image appear on screen */
    redraw_pixmap(screen.drawing_area);
    
    return;
}

GtkWidget *fsw;

static void
open_file(GtkWidget *widget, gpointer data)
{
    fsw = gtk_file_selection_new("Select Gerberfile To View");
    
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->ok_button),
		       "clicked", GTK_SIGNAL_FUNC(cb_ok_open_file), (gpointer)fsw);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->ok_button),
			      "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)fsw);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fsw)->cancel_button),
			      "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer)fsw);
    
    gtk_widget_show(fsw);
    
    return;
}

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
}

GtkWidget *
create_drawing_area(gint win_width, gint win_height)
{
    GtkWidget *drawing_area;
    
    drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area), win_width, win_height);
    
    return drawing_area;
}


/*
 * Draws a circle _centered_ at x,y with diameter dia
 */
static void 
gerbv_draw_circle(GdkPixmap *pixmap, GdkGC *gc, gint filled, gint x, gint y, gint dia)
{
    static const gint full_circle = 23360;
    gint real_x = x - dia / 2;
    gint real_y = y - dia / 2;
    
    gdk_draw_arc(pixmap, gc, filled, real_x, real_y, dia, dia, 0, full_circle);
    
    return;
}


/*
 * Draws a rectangle _centered_ at x,y with sides x_side, y_side
 */
static void 
gerbv_draw_rectangle(GdkPixmap *pixmap, GdkGC *gc, gint filled, gint x, gint y, gint x_side, gint y_side)
{
    
    gint real_x = x - x_side / 2;
    gint real_y = y - y_side / 2;
    
    gdk_draw_rectangle(pixmap, gc, filled, real_x, real_y, x_side, y_side);
    
    return;
}


/*
 * Draws an oval _centered_ at x,y with x axel x_axel and y axel y_axel
 */ 
static void
gerbv_draw_oval(GdkPixmap *pixmap, GdkGC *gc, gint filled, gint x, gint y, gint x_axel, gint y_axel)
{
    static const gint full_circle = 23360;
    gint real_x = x - x_axel / 2;
    gint real_y = y - y_axel / 2;
    
    gdk_draw_arc(pixmap, gc, filled, real_x, real_y, x_axel, y_axel, 0, full_circle);
    
    return;
}

/*
 * Draws an arc 
 * direction: 0 clockwise, 1 counterclockwise
 */
static void
gerbv_draw_arc(GdkPixmap *pixmap, GdkGC *gc, int cw, 
	       gint start_x, gint start_y, 
	       gint stop_x, gint stop_y, 
	       gint dist_x, gint dist_y)
{
    int quadrant = 0;
    gint angle1 = 0;
    gint angle2 = 0;
    gint real_x = 0;
    gint real_y = 0;
    gint width  = 0;
    gint height = 0;
    gint arc_cp_x = 0;
    gint arc_cp_y = 0;
    gint d1x, d1y, d2x, d2y;
    double alfa, beta;
    
    /*
     * Convert cw to ccw by swapping start and stop points
     */
    if (cw) {
	gint tmp;
	
	tmp = start_x;
	start_x = stop_x;
	stop_x = tmp;
	
	tmp = start_y;
	start_y = stop_y;
	stop_y = tmp;
    }
    
    /*
     * Quadrant detection 
     *    ---->X
     *    !
     *   \!/
     *  Y V
     */
    if (start_x > stop_x)
	/* 1st and 2nd quadrant */
	if (start_y > stop_y)
	    quadrant = 1;
	else
	    quadrant = 2;
    else
	/* 3rd and 4th quadrant */
	if (start_y < stop_y)
	    quadrant = 3;
	else
	    quadrant = 4;
    
    /*
     * Calculate arc center point
     * Depending on direction, we have swapped start and stop points
     */
    arc_cp_x = cw ? stop_x : start_x;
    arc_cp_y = cw ? stop_y : start_y;
    switch (quadrant) {
    case 1 :
	arc_cp_x -= dist_x;
	arc_cp_y += dist_y;
	break;
    case 2 :
	arc_cp_x += dist_x;
	arc_cp_y += dist_y;
	break;
    case 3 : 
	arc_cp_x += dist_x;
	arc_cp_y -= dist_y;
	break;
    case 4 :
	arc_cp_x -= dist_x;
	arc_cp_y -= dist_y;
	break;
    default :
	err(1, "Strange quadrant : %d\n", quadrant);
    }
    
    /*
     * Some good values 
     */
#define DIFF(a, b) ((a > b) ? a - b : b - a)
    d1x = DIFF(start_x, arc_cp_x);
    d1y = DIFF(start_y, arc_cp_y);
    d2x = DIFF(stop_x, arc_cp_x);
    d2y = DIFF(stop_y, arc_cp_y);
    
#define RAD2DEG(a) (int)ceil(a * 180 / M_PI) 
    alfa = atan2((double)d1y, (double)d1x);
    beta = atan2((double)d2y, (double)d2x);
    
    /*
     * Avoid divide by zero when sin(0) = 0 and cos(90) = 0
     */
    width  = alfa < beta ? 2 * (d1x / cos(alfa)) : 2 * (d2x / cos(beta));
    height = alfa > beta ? 2 * (d1y / sin(alfa)) : 2 * (d2y / sin(beta));
    if (alfa < 0.000001 && beta < 0.000001) height = 0;
    
    real_x = arc_cp_x - width / 2;
    real_y = arc_cp_y - height / 2;
    
    switch (quadrant) {
    case 1 :
	angle1 = RAD2DEG(alfa);
	angle2 = RAD2DEG(beta);
	break;
    case 2 :
	angle1 = 180 - RAD2DEG(alfa);
	angle2 = 180 - RAD2DEG(beta);
	break;
    case 3 : 
	angle1 = 180 + RAD2DEG(alfa);
	angle2 = 180 + RAD2DEG(beta);
	break;
    case 4 :
	angle1 = 360 - RAD2DEG(alfa);
	angle2 = 360 - RAD2DEG(beta);
	break;
    default :
	err(1, "Strange quadrant : %d\n", quadrant);
    }
    
    if (width < 0)
	fprintf(stderr, "Negative width [%d] in quadrant %d [%f][%f]\n", 
		width, quadrant, alfa, beta);
    
    if (height < 0)
	fprintf(stderr, "Negative height [%d] in quadrant %d [%d][%d]\n", 
		height, quadrant, RAD2DEG(alfa), RAD2DEG(beta));
    
    gdk_draw_arc(pixmap, gc, FALSE, real_x, real_y, 
		 width, height, 
		 angle1 * 64, (angle2 - angle1) * 64);
    
    return;
} /* gerbv_draw_arc */


int
image2pixmap(GdkPixmap **pixmap, struct gerb_image *image, int scale, gint trans_x, gint trans_y)
{
    GdkGC *line_gc = gdk_gc_new(*pixmap);
    GdkColor err_color;
    GdkColor other_color;
    GdkGC *err_gc = gdk_gc_new(*pixmap);
    GdkGC *other_gc = gdk_gc_new(*pixmap);
    GdkColormap *colormap = gdk_colormap_get_system();
    struct gerb_net *net;
    gint x1, y1, x2, y2, i, j;
    int p1, p2;
    double x_scale, y_scale;
    
    if (image == NULL || image->netlist == NULL) 
	return 0;
    
    gdk_color_parse("red1", &err_color);
    gdk_colormap_alloc_color(colormap, &err_color, FALSE, TRUE);
    gdk_gc_set_foreground(err_gc, &err_color);
    
    gdk_color_parse("green", &other_color);
    gdk_colormap_alloc_color(colormap, &other_color, FALSE, TRUE);
    gdk_gc_set_foreground(other_gc, &other_color);
    
    if (screen.file[screen.curr_index]->image->info->polarity == NEGATIVE) 
	/* Black */
	gdk_gc_set_foreground(line_gc, background.color);
    else
	/* Current line color */
	gdk_gc_set_foreground(line_gc, colors[screen.file[screen.curr_index]->color_index].color);
    
    for (net = image->netlist->next ; net != NULL; net = net->next) {
	if (image->aperture[net->aperture - APERTURE_MIN] == NULL) {
	    fprintf(stderr, "Aperture [%d] is not defined\n", net->aperture);
	    continue;
	}
	
	x1 = (int)ceil((image->info->offset_a + net->start_x) * scale);
	y1 = (int)ceil((image->info->offset_b + 2.5 - net->start_y) * scale);
	x2 = (int)ceil((image->info->offset_a + net->stop_x) * scale);
	y2 = (int)ceil((image->info->offset_b + 2.5 - net->stop_y)  * scale);
	i  = (int)ceil(net->arc_start_x * scale);
	j  = (int)ceil(net->arc_start_y * scale);
	
	x1 = x1 + trans_x;
	y1 = y1 + trans_y;
	x2 = x2 + trans_x;
	y2 = y2 + trans_y;
	
	switch (net->aperture_state) {
	case ON :
	    p1 = (int)(image->aperture[net->aperture - APERTURE_MIN]->parameter[0] * scale);
	    gdk_gc_set_line_attributes(line_gc, p1, 
				       GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
	    gdk_gc_set_line_attributes(err_gc, p1, 
				       GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
	    gdk_gc_set_line_attributes(other_gc, p1, 
				       GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
	    switch (net->interpolation) {
	    case LINEARx10 :
	    case LINEARx01 :
	    case LINEARx001 :
		fprintf(stderr, "Linear != x1\n");
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		break;
	    case LINEARx1 :
		gdk_draw_line(*pixmap, line_gc, x1, y1, x2, y2);
		break;
		
	    case MQ_CW_CIRCULAR :
		fprintf(stderr, "Multi quadrant cw arc\n");
		/*gerbv_draw_arc(*pixmap, err_gc, 1, x1, y1, x2, y2, i, j);*/
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		break;
	    case CW_CIRCULAR :
#ifdef ARC_DEBUG
		gerbv_draw_arc(*pixmap, other_gc, 1, x1, y1, x2, y2, i, j);
#else
		gerbv_draw_arc(*pixmap, line_gc, 1, x1, y1, x2, y2, i, j);
#endif
		break;
		
	    case MQ_CCW_CIRCULAR :
		fprintf(stderr, "Multi quadrant ccw arc\n");
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		/*gerbv_draw_arc(*pixmap, err_gc, 0, x1, y1, x2, y2, i, j);*/
		break;
	    case CCW_CIRCULAR :
#ifdef ARC_DEBUG
		gerbv_draw_arc(*pixmap, err_gc, 0, x1, y1, x2, y2, i, j);
#else
		gerbv_draw_arc(*pixmap, line_gc, 0, x1, y1, x2, y2, i, j);
#endif
		break;
		
	    default :
		
	    }
	    break;
	case OFF :
	    break;
	case FLASH :
	    p1 = (int)(image->aperture[net->aperture - APERTURE_MIN]->parameter[0] * scale);
	    p2 = (int)(image->aperture[net->aperture - APERTURE_MIN]->parameter[1] * scale);
	    
	    switch (image->aperture[net->aperture-APERTURE_MIN]->type) {
	    case CIRCLE :
		gerbv_draw_circle(*pixmap, line_gc, TRUE, x2, y2, p1);
		break;
	    case RECTANGLE:
		gerbv_draw_rectangle(*pixmap, line_gc, TRUE, x2, y2, p1, p2);
		break;
	    case OVAL :
		gerbv_draw_oval(*pixmap, line_gc, TRUE, x2, y2, p1, p2);
		break;
	    case POLYGON :
		fprintf(stderr, "Warning! Very bad at drawing polygons.\n");
		gerbv_draw_circle(*pixmap, line_gc, TRUE, x2, y2, p1);
		break;
	    case MACRO :
		gerbv_draw_circle(*pixmap, err_gc, TRUE, x2, y2, 10);
		break;
	    default :
		err(1, "Unknown aperture type\n");
	    }
	    break;
	default :
	    err(1, "Unknown aperture state\n");
	}
    }
    
    return 1;
}

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
		     screen.scale, screen.trans_x, screen.trans_y);
    
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

#ifdef NO_GUILE
int
main(int argc, char *argv[])
#else
void
internal_main(int argc, char *argv[])
#endif
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
    
#ifdef NO_GUILE
    return 0;
#else
    return;
#endif
    }
    
#ifndef NO_GUILE
int
main(int argc, char *argv[])
{
    gh_enter(argc, argv, internal_main);

    return 0;
}
#endif
