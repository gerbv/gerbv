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

#include <glib.h>
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
#include "tooltable.h"
#include "pick-and-place.h"
#include "interface.h"
#include "callbacks.h"
#include "render.h"
#include "exportimage.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

#define NUMBER_OF_DEFAULT_COLORS 18
static GdkColor defaultColors[NUMBER_OF_DEFAULT_COLORS] = {
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
    {"export",          required_argument, NULL,    'x'},
    {"output",          required_argument, NULL,    'o'},
    {"foreground",          required_argument, NULL,    'f'},
    {"background",          required_argument, NULL,    'b'},
    {"resolution",          required_argument, NULL,    'r'},
    {"scale",          required_argument, NULL,    's'},
    {"origin",          required_argument, NULL,    'g'},
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

static int gerbv_open_image(char *filename, int idx, int reload);

void 
gerbv_open_project_from_filename(gchar *filename) 
{
    project_list_t *project_list = NULL;
    
    dprintf("Opening project = %s\n", (gchar *) filename);
    project_list = read_project_file(filename);
    
    if (project_list) {
	project_list_t *pl_tmp;
	
	while (project_list) {
	    GdkColor colorTemplate = {0,project_list->rgb[0],
				      project_list->rgb[1],project_list->rgb[2]};
	    if (project_list->layerno == -1) {
		screen.background = colorTemplate;
	    } else {
		int  idx =  project_list->layerno;
		gchar *fullName = NULL;
		gchar *dirName = NULL;
		    
		if (!g_path_is_absolute (project_list->filename)) {
		    /* build the full pathname to the layer */
		    dirName = g_path_get_dirname (filename);
		    fullName = g_build_filename (dirName, project_list->filename, NULL);
		} else {
		    fullName = g_strdup (project_list->filename);
		}
		if (gerbv_open_image(fullName, idx, FALSE) == -1) {
		    GERB_MESSAGE("could not read %s[%d]", fullName, idx);
		    goto next_layer;
		}
		g_free (dirName);
		g_free (fullName);
		/* 
		 * Change color from default to from the project list
		 */
		screen.file[idx]->color = colorTemplate;
#ifdef RENDER_USING_GDK	    
		gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.file[idx]->color, FALSE, TRUE);
#endif
		screen.file[idx]->inverted = project_list->inverted;
	    }
	next_layer:
	    pl_tmp = project_list;
	    project_list = project_list->next;
	    g_free(pl_tmp->filename);
	    g_free(pl_tmp);
	}
	
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
    } else {
	GERB_MESSAGE("could not read %s[%d]\n", (gchar *) filename,
		     screen.last_loaded);
    }
} /* gerbv_open_project_from_filename */


void 
gerbv_open_layer_from_filename(gchar *filename) 
{
    dprintf("Opening filename = %s\n", (gchar *) filename);
    
    
    if (gerbv_open_image(filename, ++screen.last_loaded, FALSE) == -1) {
	GERB_MESSAGE("could not read %s[%d]", (gchar *) filename,
		     screen.last_loaded);
	screen.last_loaded--;
    } else {
	dprintf("     Successfully opened file!\n");	
    }
} /* gerbv_open_layer_from_filename */


void 
gerbv_save_project_from_filename(gchar *filename) 
{
    project_list_t *project_list = NULL, *tmp;
    int idx;
    gchar *dirName = g_path_get_dirname (filename);
    
    project_list = g_new0 (project_list_t, 1);
    project_list->next = project_list;
    project_list->layerno = -1;
    project_list->filename = screen.path;
    project_list->rgb[0] = screen.background.red;
    project_list->rgb[1] = screen.background.green;
    project_list->rgb[2] = screen.background.blue;
    project_list->next = NULL;
    
    for (idx = 0; idx < screen.max_files; idx++) {
	if (screen.file[idx]) {
	    tmp = g_new0 (project_list_t, 1);
	    tmp->next = project_list;
	    tmp->layerno = idx;
	    
	    /* figure out the relative path to the layer from the project
	       directory */
	    if (strncmp (dirName, screen.file[idx]->name, strlen(dirName)) == 0) {
		/* skip over the common dirname and the separator */
		tmp->filename = (screen.file[idx]->name + strlen(dirName) + 1);
	    } else {
		/* if we can't figure out a relative path, just save the 
		 * absolute one */
		tmp->filename = screen.file[idx]->name;
	    }
	    tmp->rgb[0] = screen.file[idx]->color.red;
	    tmp->rgb[1] = screen.file[idx]->color.green;
	    tmp->rgb[2] = screen.file[idx]->color.blue;
	    tmp->inverted = screen.file[idx]->inverted;
	    project_list = tmp;
	}
    }
    
    if (write_project_file(screen.project, project_list)) {
	GERB_MESSAGE("Failed to write project\n");
    }
    g_free (dirName);
} /* gerbv_save_project_from_filename */


void 
gerbv_save_as_project_from_filename(gchar *filename) 
{
	
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
    gerbv_save_project_from_filename (filename);
} /* gerbv_save_as_project_from_filename */


void 
gerbv_revert_all_files(void) 
{
    int idx;
    
    for (idx = 0; idx < screen.max_files; idx++) {
	if (screen.file[idx] && screen.file[idx]->name) {
	    if (gerbv_open_image(screen.file[idx]->name, idx, TRUE) == -1)
		return;
	}
    }
} /* gerbv_revert_all_files */

void 
gerbv_unload_layer(int index) 
{
    gint i;
    
    free_gerb_image(screen.file[index]->image);  screen.file[index]->image = NULL;
    g_free(screen.file[index]->name);
    screen.file[index]->name = NULL;
    g_free(screen.file[index]);
    screen.file[index] = NULL;
    
    /* slide all later layers down to fill the empty slot */
    for (i=index; i<(screen.max_files-1); i++) {
	screen.file[i]=screen.file[i+1];
    }
    screen.last_loaded--;
} /* gerbv_unload_layer */

void 
gerbv_unload_all_layers (void) 
{
    int index;

    /* Must count down since gerbv_unload_layer collapses
     * layers down.  Otherwise, layers slide past the index */
    for (index = screen.max_files-1 ; index >= 0; index--) {
	if (screen.file[index] && screen.file[index]->name) {
	    gerbv_unload_layer (index);
	}
    }
} /* gerbv_unload_all_layers */


void 
gerbv_change_layer_order(gint oldPosition, gint newPosition) 
{
    gerbv_fileinfo_t *temp_file;
    int index;
    
    temp_file = screen.file[oldPosition];
	
    if (oldPosition < newPosition){
	for (index = oldPosition; index < newPosition; index++) {
	    screen.file[index] = screen.file[index + 1];
	}
    } else {
	for (index = oldPosition; index > newPosition; index--) {
	    screen.file[index] = screen.file[index - 1];
	}
    }
    screen.file[newPosition] = temp_file;
} /* gerbv_change_layer_order */

gint
gerbv_add_parsed_image_to_project (gerb_image_t *parsed_image,
			gchar *filename, gchar *baseName, int idx, int reload){
    gerb_verify_error_t error = GERB_IMAGE_OK;
    int r, g, b; 
    
    dprintf("In open_image, now error check file....\n");
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
	screen.file[idx] = (gerbv_fileinfo_t *)g_malloc(sizeof(gerbv_fileinfo_t));
	if (screen.file[idx] == NULL)
	    GERB_FATAL_ERROR("malloc screen.file[idx] failed\n");
	memset((void *)screen.file[idx], 0, sizeof(gerbv_fileinfo_t));
	screen.file[idx]->image = parsed_image;
    }
    
    /*
     * Store filename for eventual reload
     */
    screen.file[idx]->fullPathname = g_strdup (filename);
    screen.file[idx]->name = g_strdup (baseName);
    
    {
	r = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].red*256;
	g = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].green*256;
	b = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].blue*256;
    }

    GdkColor colorTemplate = {0, r, g, b};
    screen.file[idx]->color = colorTemplate;
#ifdef RENDER_USING_GDK
    gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.file[idx]->color, FALSE, TRUE);
#endif
    screen.file[idx]->alpha = 45535;
    screen.file[idx]->isVisible = TRUE;
    return 1;
}

/* ------------------------------------------------------------------ */
static int
gerbv_open_image(char *filename, int idx, int reload)
{
    gerb_file_t *fd;
    gerb_image_t *parsed_image = NULL, *parsed_image2 = NULL;
    gint retv = -1;
    gboolean isPnpFile = FALSE, foundBinary, forceLoadFile = FALSE;
    
    
    /* if too many, then grow the file list */
    if (idx >= screen.max_files) {
	screen.file = (gerbv_fileinfo_t **) realloc (screen.file, (screen.max_files + 1) * sizeof (gerbv_fileinfo_t *));


	if (screen.file == NULL)
	    {
		fprintf (stderr, "realloc failed\n");
		exit (1);
	    }
	screen.file[screen.max_files] = NULL;
	screen.max_files++;
    }
    
    dprintf("In open_image, about to try opening filename = %s\n", filename);
    
    fd = gerb_fopen(filename);
    if (fd == NULL) {
	GERB_MESSAGE("Trying to open %s:%s\n", filename, strerror(errno));
	return -1;
    }
    
    dprintf("In open_image, successfully opened file.  Now check its type....\n");
    /* Here's where we decide what file type we have */
    /* Note: if the file has some invalid characters in it but still appears to
       be a valid file, we check with the user if he wants to continue (only
       if user opens the layer from the menu...if from the command line, we go
       ahead and try to load it anyways) */

    if (gerber_is_rs274x_p(fd, &foundBinary)) {
	dprintf("Found RS-274X file\n");
	if ((foundBinary)&&(screen.win.topLevelWindow)) {
		gchar *primaryText = g_strdup_printf ("File %s appears to be a RS-274X file, but contains characters which are not valid ASCII",g_path_get_basename(filename));
		if (interface_get_alert_dialog_response (primaryText,
			"Invalid characters may cause problems with the parser. Do you still want to continue?"))
			forceLoadFile = TRUE;
		g_free (primaryText);
	}
	if ((!(screen.win.topLevelWindow))||(!foundBinary || forceLoadFile)) {
		/* figure out the directory path in case parse_gerb needs to
		 * load any include files */
		gchar *currentLoadDirectory = g_path_get_dirname (filename);
		parsed_image = parse_gerb(fd, currentLoadDirectory);
		g_free (currentLoadDirectory);
	}
    } else if(drill_file_p(fd, &foundBinary)) {
	dprintf("Found drill file\n");
	if ((foundBinary)&&(screen.win.topLevelWindow)) {
		gchar *primaryText = g_strdup_printf ("File %s appears to be a drill file, but contains characters which are not valid ASCII",g_path_get_basename(filename));
		if (interface_get_alert_dialog_response (primaryText,
			"Invalid characters may cause problems with the parser. Do you still want to continue?"))
			forceLoadFile = TRUE;
		g_free (primaryText);
	}
	if ((!(screen.win.topLevelWindow))||(!foundBinary || forceLoadFile))
		parsed_image = parse_drillfile(fd);
	
    } else if (pick_and_place_check_file_type(fd, &foundBinary)) {
	dprintf("Found pick-n-place file\n");
	if ((foundBinary)&&(screen.win.topLevelWindow)) {
		gchar *primaryText = g_strdup_printf ("File %s appears to be a pick and place file, but contains characters which are not valid ASCII",g_path_get_basename(filename));
		if (interface_get_alert_dialog_response (primaryText,
			"Invalid characters may cause problems with the parser. Do you still want to continue?"))
			forceLoadFile = TRUE;
		g_free (primaryText);
	}
	if ((!(screen.win.topLevelWindow))||(!foundBinary || forceLoadFile)) {
		pick_and_place_parse_file_to_images(fd, &parsed_image, &parsed_image2);
		isPnpFile = TRUE;
	}
    } else if (gerber_is_rs274d_p(fd)) {
	dprintf("Found RS-274D file");
	GERB_COMPILE_ERROR("%s: Found RS-274D file -- not supported by gerbv.\n", filename);
	parsed_image = NULL;

    } else {
	/* This is not a known file */
	dprintf("Unknown filetype");
	GERB_COMPILE_ERROR("%s: Unknown file type.\n", filename);
	parsed_image = NULL;
    }
    
    gerb_fclose(fd);
    if (parsed_image == NULL) {
	return -1;
    }
    
    if (parsed_image) {
	/* strip the filename to the base */
	gchar *baseName = g_path_get_basename (filename);
	gchar *displayedName;
	if (isPnpFile)
		displayedName = g_strconcat (baseName, " (top)",NULL);
	else
		displayedName = g_strdup (baseName);
    	retv = gerbv_add_parsed_image_to_project (parsed_image, filename, displayedName, idx, reload);
    	g_free (baseName);
    	g_free (displayedName);
    }
    /* for PNP place files, we may need to add a second image for the other
       board side */
    if (parsed_image2) {
      /* strip the filename to the base */
	gchar *baseName = g_path_get_basename (filename);
	gchar *displayedName;
	displayedName = g_strconcat (baseName, " (bottom)",NULL);
    	retv = gerbv_add_parsed_image_to_project (parsed_image2, filename, displayedName, idx + 1, reload);
    	g_free (baseName);
    	g_free (displayedName);
    }

    return retv;
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
    gboolean exportFromCommandline = FALSE, userSuppliedOrigin=FALSE;
    gint exportType = 0;
    gchar *exportFilename = NULL;
    gfloat userSuppliedOriginX=0.0,userSuppliedOriginY=0.0,
	userSuppliedScale=0.0;
    gint userSuppliedWidth=0, userSuppliedHeight=0;
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
    /* default to using the current directory path for our starting guesses
       on future file loads */
    screen.path = g_get_current_dir ();
    screen.last_loaded = -1;  /* Will be updated to 0 
			       * when first Gerber is loaded 
			       */
    screen.max_files = 1;
    screen.file = (gerbv_fileinfo_t **) calloc (screen.max_files, sizeof (gerbv_fileinfo_t *));
    if (screen.file == NULL)
	{
	    fprintf (stderr, "malloc failed\n");
	    exit (1);
	}

    setup_init();

    /*
     * Now process command line flags
     */
    while (
#ifdef HAVE_GETOPT_LONG
	   (read_opt = getopt_long(argc, argv, opt_options, 
				   longopts, &longopt_idx))
#else
	   (read_opt = getopt(argc, argv, opt_options))
#endif /* HAVE_GETOPT_LONG */
	   != -1) {

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
	case 'g' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give an origin in the format <lower left X,lower left Y>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, "Specified origin is not recognized.\n");
		exit(1);
	    }
	    sscanf (optarg,"%f,%f",&userSuppliedOriginX,&userSuppliedOriginY);
	    if ((abs(userSuppliedOriginX) > 20) || (abs(userSuppliedOriginY) > 20)) {
		fprintf(stderr, "Specified origin is out of bounds.\n");
		exit(1);
	    }
	    userSuppliedOrigin = TRUE;
	    break;
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
	case 'r' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a resolution in the format <width,height>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, "Specified resolution is not recognized.\n");
		exit(1);
	    }
	    sscanf (optarg, "%d,%d", &userSuppliedWidth, &userSuppliedHeight);
	    if (((userSuppliedWidth < 1) || (userSuppliedHeight < 1)) ||
		((userSuppliedWidth > 2000) || (userSuppliedHeight > 2000))) {
		fprintf(stderr, "Specified resolution is out of bounds.\n");
		exit(1);
	    }
	    break;
	case 's' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a scale in the format <scale>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 10) {
		fprintf(stderr, "Specified scale is not recognized.\n");
		exit(1);
	    }
	    sscanf (optarg,"%f",&userSuppliedScale);
	    if ((userSuppliedScale < 0.001) || (userSuppliedHeight > 2000)) {
		fprintf(stderr, "Specified scale is out of bounds.\n");
		exit(1);
	    }
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
	case 'x' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must supply an export type.\n");
		exit(1);
	    }
	    if (strcmp (optarg,"png") == 0) {
		exportType = 1;
		exportFromCommandline = TRUE;
	    }
#ifndef RENDER_USING_GDK
	    else if (strcmp (optarg,"pdf") == 0) {
		exportType = 2;
		exportFromCommandline = TRUE;
	    } else if (strcmp (optarg,"svg") == 0) {
		exportType = 3;
		exportFromCommandline = TRUE;
	    } else if (strcmp (optarg,"ps") == 0) {
		exportType = 4;
		exportFromCommandline = TRUE;
	    }
#endif
	    else {
		fprintf(stderr, "Unrecognized export type.\n");
		exit(1);				
	    }		
	    break;
	case 'o' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a filename to export to.\n");
		exit(1);
	    }
	    exportFilename = optarg;
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
#ifdef RENDER_USING_GDK
	    fprintf(stderr, "  --export=<png>|-x <png> : Export rendered picture to a PNG file\n");
#else
	    fprintf(stderr, "  --export=<png/pdf/ps/svg>|-x <png/pdf/ps/svg> : Export rendered picture to the specified file format\n");
#endif
	    fprintf(stderr, "  --output=<filename>|-o <filename> : Export to the file <filename>\n");
	    fprintf(stderr, "  --foreground=<hexcolor>|-f <hexcolor> : Use foreground color <hexcolor>\n");
	    /*
	      fprintf(stderr, "  --background=<hexcolor>|-b <hexcolor> : Use background color <hexcolor>\n");
	      fprintf(stderr, "  --resolution=<width,height>|-r <width,height> : Use resolution <width> and <height>\n");
	    */
	    fprintf(stderr, "  --scale=<scale>|-s <scale> : Use the specified scale value <scale>\n");
	    fprintf(stderr, "  --origin=<x,y>|-g <x,y> : Use the specified origin to the lower left corner\n");
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
#ifdef RENDER_USING_GDK
	    fprintf(stderr, "  -x <png> : Export rendered picture to a PNG file\n");
#else
	    fprintf(stderr, "  -x <png/pdf/ps/svg> : Export rendered picture to the specified file format\n");
#endif
	    fprintf(stderr, "  -o <filename> : Export to the file <filename>\n");
	    /*
	      fprintf(stderr, "  -f <hexcolor> : Use foreground color <hexcolor>\n");
	      fprintf(stderr, "  -b <hexcolor> : Use background color <hexcolor>\n");
	    */
	    fprintf(stderr, "  -r <width,height> : Use resolution <width> and <height>\n");
	    fprintf(stderr, "  -s <scale> : Use the specified scale value <scale>\n");
	    fprintf(stderr, "  -g <x,y> : Use the specified origin to the lower left corner\n");				
	    exit(1);
	    break;
#endif /* HAVE_GETOPT_LONG */
	default :
	    printf("Not handled option [%d=%c]\n", read_opt, read_opt);
	}
    }
    
    /* even for command line exporting, GDK renderer needs gtk started up */
#ifdef RENDER_USING_GDK
    gtk_init (&argc, &argv);
#endif
    
    /*
     * If project is given, load that one and use it for files and colors.
     * Else load files (eventually) given on the command line.
     * This limits you to either give files on the commandline or just load
     * a project.
     */
    if (project_filename) {
	/* calculate the absolute pathname to the project if the user
	   used a relative path */
	g_free (screen.path);
	if (!g_path_is_absolute(project_filename)) {
	    gchar *fullName = g_build_filename (g_get_current_dir (),
						project_filename, NULL);
	    gerbv_open_project_from_filename (fullName);
	    screen.path = g_path_get_dirname (fullName);
	    g_free (fullName);
	} else {
	    gerbv_open_project_from_filename (project_filename);
	    screen.path = g_path_get_dirname (project_filename);
	}
	
    } else {
	for(i = optind ; i < argc; i++) {
	    g_free (screen.path);
	    if (!g_path_is_absolute(argv[i])) {
		gchar *fullName = g_build_filename (g_get_current_dir (),
						    argv[i], NULL);
		gerbv_open_layer_from_filename (fullName);
		screen.path = g_path_get_dirname (fullName);
		g_free (fullName);
	    } else {
		gerbv_open_layer_from_filename (argv[i]);
		screen.path = g_path_get_dirname (argv[i]);
	    }
	}
    }

    screen.unit = GERBV_DEFAULT_UNIT;
    
    if (exportFromCommandline) {
	/* load the info struct with the default values */
#ifdef RENDER_USING_GDK
	gerbv_render_info_t renderInfo = {1.0, 0, 0, 0, 640, 480};
#else
	gerbv_render_info_t renderInfo = {1.0, 0, 0, 2, 640, 480};
#endif	
	gboolean freeFilename = FALSE;
	
	if (!exportFilename) {
	    exportFilename = g_strdup ("output.png");
	    freeFilename = TRUE;
	}

	if (userSuppliedWidth)
	    renderInfo.displayWidth = userSuppliedWidth;
	if (userSuppliedHeight)
	    renderInfo.displayHeight = userSuppliedHeight;
	
	if (userSuppliedScale > 0.001)
	    renderInfo.scaleFactor = userSuppliedScale;
	else
	    render_zoom_to_fit_display (&renderInfo);
	
	if (userSuppliedOrigin) {
	    if (abs(userSuppliedOriginX) > 0.001 )
		renderInfo.lowerLeftX = userSuppliedOriginX;
	    if (abs(userSuppliedOriginY) > 0.001 )
		renderInfo.lowerLeftY = userSuppliedOriginY;
	}
	
	if (exportType == 1) {
#ifdef EXPORT_PNG
	    exportimage_export_to_png_file (&renderInfo, exportFilename);
#endif
	} else if (exportType == 2) {
	    exportimage_export_to_pdf_file (&renderInfo, exportFilename);
	} else if (exportType == 3) {
	    exportimage_export_to_svg_file (&renderInfo, exportFilename);
	} else if (exportType == 4) {
	    exportimage_export_to_postscript_file (&renderInfo, exportFilename);
	}

	if (freeFilename)
	    free (exportFilename);
	/* exit now and don't start up gtk if this is a command line export */
	exit(1);
    }
    
    /* cairo rendering can wait to start up gtk only if we're using the gui */
#ifndef RENDER_USING_GDK
    gtk_init (&argc, &argv);
#endif
    interface_create_gui (req_width, req_height);
    
    return 0;
} /* main */

