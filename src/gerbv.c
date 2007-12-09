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
#include "render.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, 
	      ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, 
	      ZOOM_SET } gerbv_zoom_dir_t;

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
/* gint redraw_pixmap(GtkWidget *widget, int restart); */
int open_image(char *filename, int idx, int reload);


void gerbv_open_project_from_filename (gchar *filename) {
	project_list_t *project_list = NULL;
	
	dprintf("Opening project = %s\n", (gchar *) filename);
	project_list = read_project_file(filename);
	if (project_list) {
	    load_project(project_list);
	    /*
	     * Save project filename for later use
	     */
	    if (screen.project) {
		g_free(screen.project);
		screen.project = NULL;
	    }
	    screen.project = g_strdup (filename);
	    if (screen.project == NULL)
		GERB_FATAL_ERROR("malloc screen.project failed\n");
	    rename_main_window(filename, NULL);
	} else {
	    GERB_MESSAGE("could not read %s[%d]", (gchar *) filename,
				 screen.last_loaded);
	}
}

void gerbv_open_layer_from_filename (gchar *filename) {	
	dprintf("Opening filename = %s\n", (gchar *) filename);

	if (open_image(filename, ++screen.last_loaded, FALSE) == -1) {
		GERB_MESSAGE("could not read %s[%d]", (gchar *) filename,
			 screen.last_loaded);
	} else {
		dprintf("     Successfully opened file!\n");	
	}
}

void gerbv_save_project_from_filename (gchar *filename) {
	project_list_t *project_list = NULL, *tmp;
	int idx;
	gchar *pathName=NULL;
    
	if (screen.path) {
	    project_list = (project_list_t *)malloc(sizeof(project_list_t));
	    if (project_list == NULL)
		GERB_FATAL_ERROR("malloc project_list failed\n");
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
	    if (screen.file[idx] &&  screen.file[idx]->color) {
		tmp = (project_list_t *)malloc(sizeof(project_list_t));
		if (tmp == NULL) 
		    GERB_FATAL_ERROR("malloc tmp failed\n");
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
	}
#ifdef HAVE_LIBGEN_H
	/*
	* Remember where we loaded file from last time
	*/
	pathName = dirname(filename);
#endif
	if (screen.path)
		g_free(screen.path);
	screen.path = g_strconcat (pathName,"/",NULL);
	if (screen.path == NULL)
		GERB_FATAL_ERROR("malloc screen.path failed\n");
}

void gerbv_save_as_project_from_filename (gchar *filename) {
	
	/*
	 * Save project filename for later use
	 */
	if (screen.project) {
	    g_free(screen.project);
	    screen.project = NULL;
	}
	screen.project = g_strdup(filename);
	if (screen.project == NULL)
	    GERB_FATAL_ERROR("malloc screen.project failed\n");
	rename_main_window(filename, NULL);
	gerbv_save_project_from_filename (filename);
}

void gerbv_revert_all_files (void) {
	int idx;

	for (idx = 0; idx < MAX_FILES; idx++) {
	    if (screen.file[idx] && screen.file[idx]->name) {
	        if (open_image(screen.file[idx]->name, idx, TRUE) == -1)
		    return;
	    }
	}
	/* Redraw screen */
	redraw_pixmap(screen.drawing_area, TRUE);
}

void gerbv_unload_layer (int index) {
	free_gerb_image(screen.file[index]->image);  screen.file[index]->image = NULL;
	g_free(screen.file[index]->color);
	screen.file[index]->color = NULL;
	g_free(screen.file[index]->name);
	screen.file[index]->name = NULL;
	g_free(screen.file[index]);
	screen.file[index] = NULL;
}

void gerbv_unload_all_layers (void) {
	int idx;

	for (idx = 0; idx < MAX_FILES; idx++) {
	    if (screen.file[idx] && screen.file[idx]->name) {
	        gerbv_unload_layer (idx);
	    }
	}
}

void gerbv_export_to_png_file (int width, int height, gchar *filename) {
#ifdef RENDER_USING_GDK
		(void) png_export(screen.pixmap, filename);
#else
		cairo_surface_t *cSurface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
                                                         width, height);
            cairo_t *cairoTarget = cairo_create (cSurface);
            render_project_to_cairo_target (cairoTarget);
		cairo_surface_write_to_png (cSurface, filename);
		cairo_destroy (cairoTarget);
		cairo_surface_destroy (cSurface);
#endif
}

void gerbv_export_to_pdf_file (gchar *filename) {
#ifndef RENDER_USING_GDK
		cairo_surface_t *cSurface = cairo_pdf_surface_create (filename, 828, 576);
            cairo_t *cairoTarget = cairo_create (cSurface);
            render_project_to_cairo_target (cairoTarget);
		cairo_destroy (cairoTarget);
		cairo_surface_destroy (cSurface);
#endif
}

void gerbv_export_to_postscript_file (gchar *filename) {
#ifndef RENDER_USING_GDK
		cairo_surface_t *cSurface = cairo_ps_surface_create (filename, 828, 576);
            cairo_t *cairoTarget = cairo_create (cSurface);
            render_project_to_cairo_target (cairoTarget);
		cairo_surface_write_to_png (cSurface, filename);
		cairo_destroy (cairoTarget);
		cairo_surface_destroy (cSurface);
#endif
}

void gerbv_export_to_svg_file (gchar *filename) {
#ifndef RENDER_USING_GDK
		cairo_surface_t *cSurface = cairo_svg_surface_create (filename, 828, 576);
            cairo_t *cairoTarget = cairo_create (cSurface);
            render_project_to_cairo_target (cairoTarget);
		cairo_surface_write_to_png (cSurface, filename);
		cairo_destroy (cairoTarget);
		cairo_surface_destroy (cSurface);
#endif
}


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



/* ------------------------------------------------------------------ */
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


/* ------------------------------------------------------------------ */
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



/* ------------------------------------------------------------------ */
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


/* ------------------------------------------------------------------ */
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

    /*
     * Now process command line flags
     */
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
    	gerbv_open_project_from_filename (project_filename);
    } else {
	for(i = optind ; i < argc; i++) {
		gerbv_open_layer_from_filename (argv[i]);
	}
    }
    
    /* Set default unit to the configured default */
    screen.unit = GERBV_DEFAULT_UNIT;

    interface_create_gui (req_width, req_height);

    return 0;
} /* main */

