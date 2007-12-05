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
 
 
/** @file gerbv.c
    @brief gerbv GUI control/main.
    This is the core of gerbv it connects all modules in a single GUI
*/

/**
\mainpage gerbv Index Page

\section intro_sec Introduction

gerbv is part of gEDA tools.\n

It displays gerberfiles in an overlayable multilayer fashion and it also allows to measure distances.\n
Searching (powerful wildcard search in a browsable list) for electronic parts is available once a so-called
pick and place file has been read in (by definition there is only one for each project).\n
Apart from saving projects files, it also can export PNG graphics.


The doxygen config file is called Doxyfile.nopreprocessing and is located in the /doc directory of gerbv.

Project Manager is Stefan Petersen < speatstacken.kth.se >
*/



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_LIBGEN_H
#include <libgen.h> /* dirname */
#endif
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

#include <pango/pango.h>

#include <locale.h>

#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #include "draw.h"
#endif

#include "color.h"
#include "gerbv_screen.h"
#include "log.h"
#include "setup.h"
#include "project.h"
#ifdef EXPORT_PNG
#include "exportimage.h"
#endif /* EXPORT_PNG */
#include "tooltable.h"
#include "pick-and-place.h"
#include "interface.h"
#include "callbacks.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, ZOOM_SET } gerbv_zoom_dir_t;
typedef struct {
    gerbv_zoom_dir_t z_dir;
    GdkEventButton *z_event;
    int scale;
} gerbv_zoom_data_t;


GdkColor defaultColors[MAX_FILES] = {
	{0,115,115,222},
	{0,255,127,115},
	{0,193,0,224},
	{0,117,242,103},
	{0,0,195,195},
	{0,213,253,51},
	{0,209,27,104},
	{0,255,197,51},
	{0,186,186,186},
	{0,211,211,255},
	{0,253,210,206},
	{0,236,194,242},
	{0,208,249,204},
	{0,183,255,255},
	{0,241,255,183},
	{0,255,202,225},
	{0,253,238,197},
	{0,226,226,226}
};

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

/**Global state variable to keep track of what's happening on the screen.
   Declared extern in gerbv_screen.h
 */
gerbv_screen_t screen;
#if defined (__MINGW32__)
const char path_separator = '\\';
#else
const char path_separator = '/';
#endif

void load_project(project_list_t *project_list);
gint redraw_pixmap(GtkWidget *widget, int restart);
int open_image(char *filename, int idx, int reload);


void
load_project(project_list_t *project_list)
{
    project_list_t *pl_tmp;
     
    while (project_list) {
	if (project_list->layerno == -1) {
	    screen.background = alloc_color(project_list->rgb[0], 
					    project_list->rgb[1],
					    project_list->rgb[2], NULL);
	} else {
	    int  idx =  project_list->layerno;
        
		if (open_image(project_list->filename, idx, FALSE) == -1) {
			GERB_MESSAGE("could not read %s[%d]", project_list->filename,
				 idx);
		    goto next_layer;
		}
	    
	    /* 
	     * Change color from default to from the project list
	     */
	    free(screen.file[idx]->color);
	    screen.file[idx]->color = alloc_color(project_list->rgb[0], 
						  project_list->rgb[1],
						  project_list->rgb[2], NULL);
	    
	    
	    screen.file[idx]->inverted = project_list->inverted;
	}
    next_layer:
	pl_tmp = project_list;
	project_list = project_list->next;
        free(pl_tmp->filename);
	free(pl_tmp);
    }
    
    return;
} /* load_project */


void
autoscale(void)
{
    double max_width = LONG_MIN, max_height = LONG_MIN;
    double x_scale, y_scale;
    int i;
    gerb_image_info_t *info;

    if (screen.drawing_area == NULL)
	return;

    screen.gerber_bbox.x1 = HUGE_VAL;
    screen.gerber_bbox.y1 = HUGE_VAL;
    screen.gerber_bbox.x2 = -HUGE_VAL;
    screen.gerber_bbox.y2 = -HUGE_VAL;

    for(i = 0; i < MAX_FILES; i++) {
    /*check if not only screen.file[] exists, but also if it is not a search and select layer*/
        if ((screen.file[i]) && (screen.file[i]->image != NULL)){
	  
	  info = screen.file[i]->image->info;
	  /* 
	   * Find the biggest image and use as a size reference
	   */
	  screen.gerber_bbox.x1 = MIN(screen.gerber_bbox.x1,
				      info->min_x +
				      info->offset_a_in);
	  screen.gerber_bbox.y1 = MIN(screen.gerber_bbox.y1,
				      info->min_y +
				      info->offset_b_in);
	  screen.gerber_bbox.x2 = MAX(screen.gerber_bbox.x2,
				      info->max_x +
				      info->offset_a_in);
	  screen.gerber_bbox.y2 = MAX(screen.gerber_bbox.y2,
				      info->max_y +
				      info->offset_b_in);
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
    screen.transf->scale = (int)ceil(MIN(x_scale, y_scale));
    if (screen.transf->scale < 1)
	screen.transf->scale = 1;
    if (screen.transf->scale > 10)
	screen.transf->scale = floor(screen.transf->scale / 10) * 10;

    /*
     * Calculate translation
     */
#ifdef RENDER_USING_GDK
	screen.trans_y = -(int)((double)((screen.drawing_area->allocation.height-screen.transf->scale*(max_height))/2.0));
#else
	screen.trans_y = -(int)((double)((screen.drawing_area->allocation.height-screen.transf->scale*(max_height))/2.0)+screen.drawing_area->allocation.height);
#endif
    screen.trans_x = -(int)((double)((screen.drawing_area->allocation.width-screen.transf->scale*(max_width))/2.0));
    

    /* Initialize clipping bbox to contain entire image */
    screen.clip_bbox.x1 = -screen.trans_x/(double)screen.transf->scale;
    screen.clip_bbox.y1 = -screen.trans_y/(double)screen.transf->scale;
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
gboolean idle_redraw_pixmap_active = FALSE;
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


/**Properly Redraw zoomed image.
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



/* Superimpose function */
void 
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
void 
unit_func(GtkWidget *widget, gpointer data)
{

    if ((gerbv_unit_t)data  == screen.unit)
	return;
    
    screen.unit = (gerbv_unit_t)data;

    /* Redraw the status bar */
    update_statusbar(&screen);

    return;
} /* unit_func() */

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
void
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
gint
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
   /* if (state.valid) {
	stop_idle_redraw_pixmap(widget);
   } */
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
    if (file_loaded && screen.transf->scale < 0.001) {
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

	if (g_main_pending()) {
	    /* return TRUE to keep this idle function active */
	    retval = TRUE;
	    start_idle_redraw_pixmap(widget);
	    state.file_index = i;
	    goto redraw_pixmap_end;
	}
	  
#ifdef RENDER_USING_GDK

    /*
     * Set superimposing function.
     */
    gdk_gc_set_function(gc, screen.si_func);


    /* 
     * This now allows drawing several layers on top of each other.
     * Higher layer numbers have higher priority in the Z-order. 
     */
    for(i = state.file_index; i < MAX_FILES; i++) {
	if (screen.file[i] && screen.file[i]->isVisible) {
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
			 screen.file[i]->image, screen.transf->scale, 
			 (screen.clip_bbox.x1-dmin_x)*screen.transf->scale,
			 (screen.clip_bbox.y1+dmax_y)*screen.transf->scale,
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
    
#else
	cairo_t *cr;

	window = gtk_widget_get_parent_window(widget);
	/* This might be lengthy, show that we're busy by changing the pointer */
	if (window) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new(GDK_WATCH);
		gdk_window_set_cursor(window, cursor);
		gdk_cursor_destroy(cursor);
	}

	/* get a cairo_t */
	cairo_surface_t* cs=cairo_image_surface_create (CAIRO_FORMAT_RGB24, state.max_width, state.max_height);
      cr= cairo_create(cs);

	/* speed up rendering by reducing quality by a small amount (default
	   is 1.0 */
	cairo_set_tolerance (cr,1.5);
	
	/* translate the draw area before drawing */
	cairo_translate (cr,-screen.trans_x,-screen.trans_y);

	/* scale the drawing by the specified scale factor (inverting y since
	 * cairo y axis points down)
	 */ 
	cairo_scale (cr,(float) screen.transf->scale,-(float) screen.transf->scale);
	
	/* 
	* This now allows drawing several layers on top of each other.
	* Higher layer numbers have higher priority in the Z-order. 
	*/
	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i]) {
			cairo_pattern_t *testp = NULL;
						
			cairo_push_group   (cr);
			
			cairo_set_source_rgba (cr, (double) screen.file[i]->color->red/G_MAXUINT16,
				(double) screen.file[i]->color->green/G_MAXUINT16,
				(double) screen.file[i]->color->blue/G_MAXUINT16, 1);
			
			/* for now, scale the cairo context if the units are mms */
			/* TODO: normalize all gerb_image data to mm during parsing */
			//cairo_save (cr);
			if ((screen.file[i]->image->netlist->next)&&
				(screen.file[i]->image->netlist->next->unit==MM)) {
				cairo_scale (cr, 1.0/25.4, 1.0/25.4);
			}
			render_image_to_cairo_target (cr, screen.file[i]->image);
			//cairo_restore (cr);
			testp = cairo_pop_group (cr);
			if (screen.file[i]->privateRenderData) {
				cairo_pattern_destroy ((cairo_pattern_t *) screen.file[i]->privateRenderData);
			}
			screen.file[i]->privateRenderData = (gpointer) testp;
		}
	}
	cairo_surface_destroy (cs);
	cairo_destroy (cr);	

#endif

    update_rect.x = 0, update_rect.y = 0;
    update_rect.width =	widget->allocation.width;
    update_rect.height = widget->allocation.height;
    
    /*
     * Calls expose_event
     */
    gtk_widget_draw (widget, &update_rect);
    //gdk_window_invalidate_rect(widget->window, &widget->allocation,FALSE);
    
 redraw_pixmap_end:
    /* Return default pointer shape */
    if (window) {
	gdk_window_set_cursor(window, GERBV_DEF_CURSOR);
    }
    
    gdk_gc_unref(gc);
    
    return retval;
} /* redraw_pixmap */

int
open_image(char *filename, int idx, int reload)
{
    gerb_file_t *fd;
    int r, g, b;
    gerb_image_t *parsed_image;
    gerb_verify_error_t error = GERB_IMAGE_OK;
    char *cptr;

    if (idx >= MAX_FILES) {
	GERB_MESSAGE("Couldn't open %s. Maximum number of files opened.\n",
		     filename);
	return -1;
    }
    
    dprintf("In open_image, about to try opening filename = %s\n", filename);

    fd = gerb_fopen(filename);
    if (fd == NULL) {
	GERB_MESSAGE("Trying to open %s:%s\n", filename, strerror(errno));
	return -1;
    }
    
    dprintf("In open_image, successfully opened file.  Now parse it....\n");

    if(drill_file_p(fd))
	parsed_image = parse_drillfile(fd);
    else if (pick_and_place_check_file_type(fd))
	parsed_image = pick_and_place_parse_file_to_image (fd);
    else 
	parsed_image = parse_gerb(fd);
    
    if (parsed_image == NULL) {
	return -1;
    }

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
	/* Load new file. */
	screen.file[idx] = (gerbv_fileinfo_t *)malloc(sizeof(gerbv_fileinfo_t));
	if (screen.file[idx] == NULL)
	    GERB_FATAL_ERROR("malloc screen.file[idx] failed\n");
	memset((void *)screen.file[idx], 0, sizeof(gerbv_fileinfo_t));
	screen.file[idx]->image = parsed_image;
    }
    
    /*
     * Store filename for eventual reload
     */
    screen.file[idx]->name = (char *)malloc(strlen(filename) + 1);
    if (screen.file[idx]->name == NULL)
	GERB_FATAL_ERROR("malloc screen.file[idx]->name failed\n");
    strcpy(screen.file[idx]->name, filename);
    
    /*
     * Try to get a basename for the file
     */
    
    cptr = strrchr(filename, path_separator);
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
    
    r = defaultColors[idx].red*256;
    g = defaultColors[idx].green*256;
    b = defaultColors[idx].blue*256;
    
    screen.file[idx]->color = alloc_color(r, g, b, NULL);
    screen.file[idx]->alpha = 45535;
    screen.file[idx]->isVisible = TRUE;                     
    
    return 0;
} /* open_image */

int
main(int argc, char *argv[])
{
    int       read_opt;
    int       i;
    int       req_width = -1, req_height = -1;
#ifdef HAVE_GETOPT_LONG
    int       req_x = 0, req_y = 0;
    char      *rest;
#endif
    char      *project_filename = NULL;

    /*
     * Setup the screen info. Must do this before getopt, since getopt
     * eventually will set some variables in screen.
     */
    memset((void *)&screen, 0, sizeof(gerbv_screen_t));
    screen.state = NORMAL;
 #ifdef HAVE_LIBGEN_H    
    screen.execpath = dirname(argv[0]);
#else 
    screen.execpath = "";
#endif    
    screen.transf = gerb_transf_new();
    screen.transf->scale = 0.0; /* will force reinitialization of the screen later */

    screen.last_loaded = -1;  /* Will be updated to 0 
			       * when first Gerber is loaded 
			       */

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
		fprintf(stderr, "Not handled option %s\n", longopts[longopt_idx].name);
		break;
	    case 1: /* geometry */
		errno = 0;
		req_width = (int)strtol(optarg, &rest, 10);
		if (errno) {
		    perror("Width");
		    break;
		}
		if (rest[0] != 'x'){
		    fprintf(stderr, "Split X and Y parameters with an x\n");
		    break;
		}
		rest++;
		errno = 0;
		req_height = (int)strtol(rest, &rest, 10);
		if (errno) {
		    perror("Height");
		    break;
		}
		if ((rest[0] == 0) || ((rest[0] != '-') && (rest[0] != '+')))
		    break;
		errno = 0;
		req_x = (int)strtol(rest, &rest, 10);
		if (errno) {
		    perror("X");
		    break;
		}
		if ((rest[0] == 0) || ((rest[0] != '-') && (rest[0] != '+')))
		    break;
		errno = 0;
		req_y = (int)strtol(rest, &rest, 10);
		if (errno) {
		    perror("Y");
		    break;
		}
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
     * If project is given, load that one and use it for files and colors.
     * Else load files (eventually) given on the command line.
     * This limits you to either give files on the commandline or just load
     * a project.
     */
     
    gtk_init (&argc, &argv);
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
	    if (screen.project == NULL)
		GERB_FATAL_ERROR("malloc screen.project failed\n");
	    memset((void *)screen.project, 0, strlen(project_filename) + 1);
	    strncpy(screen.project, project_filename, strlen(project_filename));
	    /*
	     * Remember where we loaded file from last time
	     */
	    if (screen.path)
		free(screen.path);
	    screen.path = (char *)malloc(strlen(project_filename) + 2);
	    if (screen.path == NULL)
		GERB_FATAL_ERROR("malloc screen.path failed\n");
	    strcpy(screen.path, project_filename);
#ifdef HAVE_LIBGEN_H             
	    dirname(screen.path);
#endif
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
     * Set console error log handler. The default gives us error levels in
     * in the beginning which I don't want.
     */
    g_log_set_handler (NULL, 
		       G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION | G_LOG_LEVEL_MASK, 
		       gerbv_console_log_handler, NULL); 

    /* Set default unit to the configured default */
    screen.unit = GERBV_DEFAULT_UNIT;

 
    interface_create_gui (req_width, req_height);

    return 0;
} /* main */

