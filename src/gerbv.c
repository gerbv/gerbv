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
  #include "draw-gdk.h"
  #include "draw.h"
#endif

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
#include "export-rs274x.h"
#include "export-drill.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

#define NUMBER_OF_DEFAULT_COLORS 18
#define NUMBER_OF_DEFAULT_TRANSFORMATIONS 20
/* Notice that the pixel field is used for alpha in this case */
typedef struct{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
}LayerColor;

static LayerColor defaultColors[NUMBER_OF_DEFAULT_COLORS] = {
	{115,115,222,177},
	{255,127,115,177},
	{193,0,224,177},
	{117,242,103,177},
	{0,195,195,177},
	{213,253,51,177},
	{209,27,104,177},
	{255,197,51,177},
	{186,186,186,177},
	{211,211,255,177},
	{253,210,206,177},
	{236,194,242,177},
	{208,249,204,177},
	{183,255,255,177},
	{241,255,183,177},
	{255,202,225,177},
	{253,238,197,177},
	{226,226,226,177}
};

static gerb_user_transformations_t defaultTransformations[NUMBER_OF_DEFAULT_TRANSFORMATIONS] = {
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},	
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},		
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},	
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
	{0,0,0,0,FALSE},
};

#ifdef HAVE_GETOPT_LONG
int longopt_val = 0;
int longopt_idx = 0;
const struct option longopts[] = {
    /* name              has_arg            flag  val */
    {"border",		required_argument,  NULL,    'B'},
    {"dpi",		required_argument,  NULL,    'D'},
    {"version",         no_argument,	    NULL,    'V'},
    {"origin",          required_argument,  NULL,    'O'},
    {"window_inch",	required_argument,  NULL,    'W'},
    {"antialias",	no_argument,	    NULL,    'a'},
    {"background",      required_argument,  NULL,    'b'},
    {"dump",            no_argument,	    NULL,    'd'},
    {"foreground",      required_argument,  NULL,    'f'},
    {"help",            no_argument,	    NULL,    'h'},
    {"log",             required_argument,  NULL,    'l'},
    {"output",          required_argument,  NULL,    'o'},
    {"project",         required_argument,  NULL,    'p'},
    {"tools",           required_argument,  NULL,    't'},
    {"translate",       required_argument,  NULL,    'T'},
    {"window",		required_argument,  NULL,    'w'},
    {"export",          required_argument,  NULL,    'x'},
    {"geometry",        required_argument,  &longopt_val, 1},
    /* GDK/GDK debug flags to be "let through" */
    {"gtk-module",      required_argument,  &longopt_val, 2},
    {"g-fatal-warnings",no_argument,	    &longopt_val, 2},
    {"gtk-debug",       required_argument,  &longopt_val, 2},
    {"gtk-no-debug",    required_argument,  &longopt_val, 2},
    {"gdk-debug",       required_argument,  &longopt_val, 2},
    {"gdk-no-debug",    required_argument,  &longopt_val, 2},
    {"display",         required_argument,  &longopt_val, 2},
    {"sync",            no_argument,	    &longopt_val, 2},
    {"no-xshm",         no_argument,	    &longopt_val, 2},
    {"name",            required_argument,  &longopt_val, 2},
    {"class",           required_argument,  &longopt_val, 2},
    {0, 0, 0, 0},
};
#endif /* HAVE_GETOPT_LONG*/
const char *opt_options = "Vadh:B:D:O:W:b:f:l:o:p:t:T:w:x:";

/**Global state variable to keep track of what's happening on the screen.
   Declared extern in gerbv_screen.h
 */
gerbv_screen_t screen;
#if defined (__MINGW32__)
const char path_separator = '\\';
#else
const char path_separator = '/';
#endif

static int gerbv_open_image(char *filename, int idx, int reload, 
			    HID_Attribute *attr_list, int n_attr);

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
		if (gerbv_open_image(fullName, idx, FALSE, 
				     project_list->attr_list, 
				     project_list->n_attr) == -1) {
		    GERB_MESSAGE("could not read %s[%d]", fullName, idx);
		    goto next_layer;
		}
		g_free (dirName);
		g_free (fullName);
		/* 
		 * Change color from default to from the project list
		 */
		screen.file[idx]->color = colorTemplate;
		screen.file[idx]->transform.inverted = project_list->inverted;
		screen.file[idx]->isVisible = project_list->visible;
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
    
    
    if (gerbv_open_image(filename, ++screen.last_loaded, FALSE, NULL, 0) == -1) {
	GERB_MESSAGE("could not read %s[%d]", (gchar *) filename,
		     screen.last_loaded);
	screen.last_loaded--;
    } else {
	dprintf("     Successfully opened file!\n");	
    }
} /* gerbv_open_layer_from_filename */

gboolean 
gerbv_save_layer_from_index(gint index, gchar *filename) 
{
    if (strcmp (screen.file[index]->image->info->type,"RS274-X (Gerber) File")==0) {
	export_rs274x_file_from_image (filename, screen.file[index]->image);
    }
    else if (strcmp (screen.file[index]->image->info->type,"Excellon Drill File")==0) {
	export_drill_file_from_image (filename, screen.file[index]->image);
    }
    else {
	return FALSE;
    }
    return TRUE;
} /* gerbv_save_project_from_filename */

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
	    tmp->inverted = screen.file[idx]->transform.inverted;
	    tmp->visible = screen.file[idx]->isVisible;
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

/* ------------------------------------------------------------------ */
int
gerbv_revert_file(int idx){
	int rv;
	rv = gerbv_open_image(screen.file[idx]->fullPathname, idx, TRUE, NULL, 0);
	return rv;
}

void 
gerbv_revert_all_files(void) 
{
    int idx;
    
    for (idx = 0; idx < screen.max_files; idx++) {
	if (screen.file[idx] && screen.file[idx]->fullPathname) {
	    gerbv_revert_file (idx);
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
    /* make sure the final spot is clear */
    screen.file[screen.max_files-1] = NULL;
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
	r = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].red*257;
	g = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].green*257;
	b = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].blue*257;
    }

    GdkColor colorTemplate = {0, r, g, b};
    screen.file[idx]->color = colorTemplate;
    screen.file[idx]->alpha = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].alpha*257;
    screen.file[idx]->isVisible = TRUE;
    screen.file[idx]->transform = defaultTransformations[idx % NUMBER_OF_DEFAULT_TRANSFORMATIONS];
    return 1;
}

/* ------------------------------------------------------------------ */
static int
gerbv_open_image(char *filename, int idx, int reload, HID_Attribute *fattr, int n_fattr)
{
    gerb_file_t *fd;
    gerb_image_t *parsed_image = NULL, *parsed_image2 = NULL;
    gint retv = -1;
    gboolean isPnpFile = FALSE, foundBinary, forceLoadFile = FALSE;
    HID_Attribute *attr_list = NULL;
    int n_attr = 0;
    /* If we're reloading, we'll pass in our file format attribute list
     * since this is our hook for letting the user override the fileformat.
     */
    if (reload)
	{
	    /* We're reloading so use the attribute list in memory */
	    attr_list =  screen.file[idx]->image->info->attr_list;
	    n_attr =  screen.file[idx]->image->info->n_attr;
	}
    else
	{
	    /* We're not reloading so use the attribute list read from the 
	     * project file if given or NULL otherwise.
	     */
	    attr_list = fattr;
	    n_attr = n_fattr;
	}
    /* if we don't have enough spots, then grow the file list by 2 to account for the possible 
       loading of two images for PNP files */
    if ((idx+1) >= screen.max_files) {
	screen.file = (gerbv_fileinfo_t **) realloc (screen.file, (screen.max_files + 2) * sizeof (gerbv_fileinfo_t *));

	if (screen.file == NULL)
	    {
		fprintf (stderr, "realloc failed\n");
		exit (1);
	    }
	screen.file[screen.max_files] = NULL;
	screen.file[screen.max_files+1] = NULL;
	screen.max_files += 2;
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
	    parsed_image = parse_drillfile(fd, attr_list, n_attr, reload);
	
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
    int       i,r,g,b,a;
    int       req_width = -1, req_height = -1;
#ifdef HAVE_GETOPT_LONG
    int       req_x = 0, req_y = 0;
    char      *rest;
#endif
    char      *project_filename = NULL;
    gboolean exportFromCommandline = FALSE,  userSuppliedOrigin=FALSE, userSuppliedWindow=FALSE, 
	     userSuppliedAntiAlias=FALSE, userSuppliedWindowInPixels=FALSE, userSuppliedDpi=FALSE;
    gint  layerctr =0, transformCount = 0, exportType = 0;
    gchar *exportFilename = NULL;
    gfloat userSuppliedOriginX=0.0,userSuppliedOriginY=0.0,userSuppliedDpiX=72.0, userSuppliedDpiY=72.0, 
	   userSuppliedWidth=0, userSuppliedHeight=0, userSuppliedBorder=0.05;


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
    	case 'B' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must specify the border in the format <alpha>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 10) {
		fprintf(stderr, "Specified border is not recognized.\n");
		exit(1);
	    }
	    sscanf (optarg,"%f",&userSuppliedBorder);
	    if (userSuppliedBorder <  0) {
		fprintf(stderr, "Specified border is smaller than zero!\n");
		exit(1);
	    }
	    userSuppliedBorder/=100.0;
	    break;
	case 'D' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give an resolution in the format <DPI X,DPI Y> or <DPI_X_and_Y>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, "Specified resolution is not recognized.\n");
		exit(1);
	    }
	    if(strchr(optarg, 'x')!=NULL){
		sscanf (optarg,"%fx%f",&userSuppliedDpiX,&userSuppliedDpiY);
	    }else{
		sscanf (optarg,"%f",&userSuppliedDpiX);
		userSuppliedDpiY = userSuppliedDpiX;
	    }
	    if ((userSuppliedDpiX <= 0) || (userSuppliedDpiY <= 0)) {
		fprintf(stderr, "Specified resolution should be greater than 0.\n");
		exit(1);
	    }
	    userSuppliedDpi=TRUE;
	    break;
    	case 'O' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give an origin in the format <lower_left_X x lower_left_Y>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, "Specified origin is not recognized.\n");
		exit(1);
	    }
	    sscanf (optarg,"%fx%f",&userSuppliedOriginX,&userSuppliedOriginY);
	    userSuppliedOrigin=TRUE;
	    break;
    	case 'V' :
	    printf("gerbv version %s\n", VERSION);
	    printf("(C) Stefan Petersen (spe@stacken.kth.se)\n");
	    exit(0);	
	case 'a' :
	    userSuppliedAntiAlias = TRUE;
	    break;
    	case 'b' :	// Set background to this color
	    if (optarg == NULL) {
		fprintf(stderr, "You must give an background color in the hex-format <#RRGGBB>.\n");
		exit(1);
	    }
	    if ((strlen (optarg) != 7)||(optarg[0]!='#')) {
		fprintf(stderr, "Specified color format is not recognized.\n");
		exit(1);
	    }
    	    r=g=b=-1;
	    sscanf (optarg,"#%2x%2x%2x",&r,&g,&b);
	    if ( (r<0)||(r>255)||(g<0)||(g>255)||(b<0)||(b>255)) {

		fprintf(stderr, "Specified color values should be between 00 and FF.\n");
		exit(1);
	    }
	    screen.background.red = r*257;
    	    screen.background.green = g*257;
    	    screen.background.blue = b*257;
	    break;
	case 'f' :	// Set layer colors to this color (foreground color)
	    if (optarg == NULL) {
#ifdef RENDER_USING_GDK
		fprintf(stderr, "You must give an foreground color in the hex-format <#RRGGBB>.\n");
#else
		fprintf(stderr, "You must give an foreground color in the hex-format <#RRGGBB> or <#RRGGBBAA>.\n");
#endif
		exit(1);
	    }
	    if (((strlen (optarg) != 7)&&(strlen (optarg) != 9))||(optarg[0]!='#')) {
		fprintf(stderr, "Specified color format is not recognized.\n");
		exit(1);
	    }
	    r=g=b=a=-1;
	    if(strlen(optarg)==7){
		sscanf (optarg,"#%2x%2x%2x",&r,&g,&b);
		a=177;
	    }
	    else{
		sscanf (optarg,"#%2x%2x%2x%2x",&r,&g,&b,&a);
	    }

	    if ( (r<0)||(r>255)||(g<0)||(g>255)||(b<0)||(b>255)||(a<0)||(a>255) ) {

		fprintf(stderr, "Specified color values should be between 0x00 (0) and 0xFF (255).\n");
		exit(1);
	    }
	    defaultColors[layerctr].red   = r;
	    defaultColors[layerctr].green = g;
	    defaultColors[layerctr].blue  = b;
	    defaultColors[layerctr].alpha = a;
	    layerctr++;
	    /* just reset the counter back to 0 if we read too many */
	    if (layerctr == NUMBER_OF_DEFAULT_COLORS)
	    	layerctr = 0;
	    break;
	case 'l' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a filename to send log to\n");
		exit(1);
	    }
	    setup.log.to_file = 1;
	    setup.log.filename = optarg;
	    break;
    	case 'o' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a filename to export to.\n");
		exit(1);
	    }
	    exportFilename = optarg;
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
	case 'T' :	// Translate the layer
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a translation in the format <X,Y>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 30) {
		fprintf(stderr, "The translation format is not recognized.\n");
		exit(1);
	    }
	    float transX=0, transY=0;
	    
	    sscanf (optarg,"%f,%f",&transX,&transY);
	    defaultTransformations[transformCount].translateX = transX;
	    defaultTransformations[transformCount].translateY = transY;
	    transformCount++;
	    /* just reset the counter back to 0 if we read too many */
	    if (transformCount == NUMBER_OF_DEFAULT_TRANSFORMATIONS)
	    	transformCount = 0;
	    break;
	case 'w':
	    userSuppliedWindowInPixels = TRUE;
    	case 'W' :
	    if (optarg == NULL) {
		fprintf(stderr, "You must give a window size in the format <width x height>.\n");
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, "Specified window size is not recognized.\n");
		exit(1);
	    }
	    sscanf (optarg, "%fx%f", &userSuppliedWidth, &userSuppliedHeight);
	    if (((userSuppliedWidth < 0.001) || (userSuppliedHeight < 0.001)) ||
		((userSuppliedWidth > 2000) || (userSuppliedHeight > 2000))) {
		fprintf(stderr, "Specified window size is out of bounds.\n");
		exit(1);
	    }
	    userSuppliedWindow = TRUE;
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
	    else if (strcmp (optarg,"rs274x") == 0) {
		exportType = 5;
		exportFromCommandline = TRUE;
	    }
	    else if (strcmp (optarg,"drill") == 0) {
		exportType = 6;
		exportFromCommandline = TRUE;
	    }
	    else {
		fprintf(stderr, "Unrecognized export type.\n");
		exit(1);				
	    }		
	    break;
	case 'd':
	    screen.dump_parsed_image = 1;
	    break;
	case '?':
	case 'h':
#ifdef HAVE_GETOPT_LONG
	    printf("Usage: gerbv [OPTIONS...] [FILE...]\n\n");
	    printf("Available options:\n");
	    printf("  -B, --border=<b>                Border around the image in percent of the\n");
	    printf("                                  width/height. Defaults to 5%%.\n");
#ifdef RENDER_USING_GDK
	    printf("  -D, --dpi=<R>                   Resolution (Dots per inch) for the output\n");
	    printf("                                  bitmap.\n");
#else
	    printf("  -D, --dpi=<XxY>or<R>            Resolution (Dots per inch) for the output\n");
	    printf("                                  bitmap. With the format <XxY>, different\n");
	    printf("                                  resolutions for X- and Y-direction are used.\n");
	    printf("                                  With the format <R>, both are the same.\n");
#endif
	    printf("  -O, --origin=<XxY>              Use the specified coordinates (in inches)\n");
	    printf("                                  for the lower left corner.\n");
	    printf("  -V, --version                   Print version of gerbv.\n");
	    printf("  -a, --antialias                 Use antialiasing for generated bitmap output.\n");
	    printf("  -b, --background=<hex>          Use background color <hex> (like #RRGGBB).\n");
#ifdef RENDER_USING_GDK
	    printf("  -f, --foreground=<hex>          Use foreground color <hex> (like #RRGGBB)\n");
#else
            printf("  -f, --foreground=<hex>          Use foreground color <hex> (like #RRGGBB or\n");
            printf("                                  #RRGGBBAA for setting the alpha).\n");
#endif
            printf("                                  Use multiple -f flags to set the color for\n");
	    printf("                                  multiple layers.\n");
	    printf("  -h, --help                      Print this help message.\n");
	    printf("  -l, --log=<logfile>             Send error messages to <logfile>.\n");
	    printf("  -o, --output=<filename>         Export to <filename>\n");
	    printf("  -p, --project=<prjfile>         Load project file <prjfile>\n");
	    printf("  -W, --window_inch=<WxH>         Window size in inches <WxH> for the\n");
	    printf("                                  exported image.\n");
   	    printf("  -w, --window=<WxH>              Window size in pixels <WxH> for the\n");
	    printf("                                  exported image. Autoscales to fit\n");
	    printf("                                  if no resolution is specified. If a\n");
	    printf("                                  resolution is specified, it will clip.\n");
	    printf("  -t, --tools=<toolfile>          Read Excellon tools from file <toolfile>.\n");
	    printf("  -T, --translate=<X,Y>           Translate the image by <X,Y> (useful for\n");
	    printf("                                  arranging panels). Use multiple -T flags\n");
	    printf("                                  for multiple layers.\n");
#ifdef RENDER_USING_GDK
	    printf("  -x, --export=<png>              Export a rendered picture to a PNG file.\n");
#else
	    printf("  -x, --export=<png/pdf/ps/svg/   Export a rendered picture to a file with\n");
	    printf("                rs274x/drill>     the specified format.\n");
#endif


#else
	    printf("Usage: gerbv [OPTIONS...] [FILE...]\n\n");
	    printf("Available options:\n");
	    printf("  -B<b>                   Border around the image in percent of the\n");
	    printf("                          width/height. Defaults to 5%%.\n");
#ifdef RENDER_USING_GDK
	    printf("  -D<R>                   Resolution (Dots per inch) for the output\n");
	    printf("                          bitmap\n");
#else
	    printf("  -D<XxY>or<R>            Resolution (Dots per inch) for the output\n");
	    printf("                          bitmap. With the format <XxY>, different\n");
	    printf("                          resolutions for X- and Y-direction are used.\n");
	    printf("                          With the format <R>, both are the same.\n");
#endif
	    printf("  -O<XxY>                 Use the specified coordinates (in inches)\n");
	    printf("                          for the lower left corner.\n");
    	    printf("  -V                      Print version of gerbv.\n");
    	    printf("  -a                      Use antialiasing for generated bitmap output.\n");
	    printf("  -b<hexcolor>	      Use background color <hexcolor> (like #RRGGBB)\n");
#ifdef RENDER_USING_GDK
	    printf("  -f<hexcolor>            Use foreground color <hexcolor> (like #RRGGBB)\n");
#else
	    printf("  -f<hexcolor>            Use foreground color <hexcolor> (like #RRGGBB or\n");
	    printf("                          #RRGGBBAA for setting the alpha).\n");
#endif
            printf("                          Use multiple -f flags to set the color for\n");
	    printf("                          multiple layers.\n");
	    printf("  -h                      Print this help message.\n");
	    printf("  -l<logfile>             Send error messages to <logfile>\n");
	    printf("  -o<filename>            Export to <filename>\n");
	    printf("  -p<prjfile>             Load project file <prjfile>\n");
	    printf("  -W<WxH>                 Window size in inches <WxH> for the\n");
	    printf("                          exported image\n");
       	    printf("  -w<WxH>                 Window size in pixels <WxH> for the\n");
	    printf("                          exported image. Autoscales to fit\n");
	    printf("                          if no resolution is specified. If a\n");
	    printf("                          resolution is specified, it will clip.\n");
	    printf("                          exported image\n");
	    printf("  -t<toolfile>            Read Excellon tools from file <toolfile>\n");
	    printf("  -T<X,Y>                 Translate the image by <X,Y> (useful for\n");
	    printf("                          arranging panels). Use multiple -T flags\n");
	    printf("                          for multiple layers.\n");
#ifdef RENDER_USING_GDK
	    printf("  -x<png>                 Export a rendered picture to a PNG file\n");
#else
	    printf("  -x <png/pdf/ps/svg/     Export a rendered picture to a file with\n");
	    printf("      rs274x/drill>       the specified format\n");
#endif

#endif /* HAVE_GETOPT_LONG */
	    exit(1);
	    break;
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
#ifdef RENDER_USING_GDK
    /* GDK renderer needs gtk started up even for png export */
    gtk_init (&argc, &argv);
#endif

    if (exportFromCommandline) {
	/* load the info struct with the default values */

	gboolean freeFilename = FALSE;
	
	if (!exportFilename) {
		if (exportType == 1) {
		    exportFilename = g_strdup ("output.png");
		} else if (exportType == 2) {
		    exportFilename = g_strdup ("output.pdf");
		} else if (exportType == 3) {
		    exportFilename = g_strdup ("output.svg");
		} else if (exportType == 4){
		    exportFilename = g_strdup ("output.ps");
		} else if (exportType == 5){
		    exportFilename = g_strdup ("output.gbx");
		} else {
		    exportFilename = g_strdup ("output.cnc");
		}
		freeFilename = TRUE;
	}

	gerbv_render_size_t bb;
	render_get_boundingbox(&bb);
	// Set origin to the left-bottom corner if it is not specified
	if(!userSuppliedOrigin){
	    userSuppliedOriginX = bb.left;
	    userSuppliedOriginY = bb.top;
	}

	float width  = bb.right  - userSuppliedOriginX + 0.001;	// Plus a little extra to prevent from 
	float height = bb.bottom - userSuppliedOriginY + 0.001; // missing items due to round-off errors
	// If the user did not specify a height and width, autoscale w&h till full size from origin.
	if(!userSuppliedWindow){
	    userSuppliedWidth  = width;
	    userSuppliedHeight = height;
	}else{
	    // If size was specified in pixels, and no resolution was specified, autoscale resolution till fit
	    if( (!userSuppliedDpi)&& userSuppliedWindowInPixels){
		userSuppliedDpiX = MIN(((userSuppliedWidth-0.5)  / width),((userSuppliedHeight-0.5) / height));
		userSuppliedDpiY = userSuppliedDpiX;
		userSuppliedOriginX -= 0.5/userSuppliedDpiX;
		userSuppliedOriginY -= 0.5/userSuppliedDpiY;
	    }
	}


	// Add the border size (if there is one)
	if(userSuppliedBorder!=0){
	    // If supplied in inches, add a border around the image
	    if(!userSuppliedWindowInPixels){
		userSuppliedWidth  += userSuppliedWidth*userSuppliedBorder;
		userSuppliedHeight  += userSuppliedHeight*userSuppliedBorder;
		userSuppliedOriginX -= (userSuppliedWidth*userSuppliedBorder)/2.0;
		userSuppliedOriginY -= (userSuppliedHeight*userSuppliedBorder)/2.0;
	    }
	    // If supplied in pixels, shrink image content for border_size
	    else{
		userSuppliedDpiX -= (userSuppliedDpiX*userSuppliedBorder);
		userSuppliedDpiY -= (userSuppliedDpiY*userSuppliedBorder);
		userSuppliedOriginX -= ((userSuppliedWidth/userSuppliedDpiX)*userSuppliedBorder)/2.0;
		userSuppliedOriginY -= ((userSuppliedHeight/userSuppliedDpiX)*userSuppliedBorder)/2.0;
	    }
	}
	
	if(!userSuppliedWindowInPixels){
	    userSuppliedWidth  *= userSuppliedDpiX;
	    userSuppliedHeight *= userSuppliedDpiY;
	}
	
	// Make sure there is something valid in it. It could become negative if 
	// the userSuppliedOrigin is further than the bb.right or bb.top.
	if(userSuppliedWidth <=0)
	    userSuppliedWidth  = 1;
	if(userSuppliedHeight <=0)
	    userSuppliedHeight = 1;


#ifdef RENDER_USING_GDK
	gerbv_render_info_t renderInfo = {MIN(userSuppliedDpiX, userSuppliedDpiY), MIN(userSuppliedDpiX, userSuppliedDpiY),
	    userSuppliedOriginX, userSuppliedOriginY,1, 
	    userSuppliedWidth,userSuppliedHeight };
#else	
	gerbv_render_info_t renderInfo = {userSuppliedDpiX, userSuppliedDpiY, 
	    userSuppliedOriginX, userSuppliedOriginY, userSuppliedAntiAlias?3:2, 
	    userSuppliedWidth,userSuppliedHeight };
#endif
	
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
	} else if (exportType == 5) {
	    if (screen.file[0]->image) {
		/* if we have more than one file, we need to merge them before exporting */
		if (screen.file[1]) {
		  gerb_image_t *exportImage;
		  exportImage = gerb_image_duplicate_image (screen.file[0]->image, &screen.file[0]->transform);
		  for(i = screen.max_files-1; i > 0; i--) {
		    if (screen.file[i]) {
		      gerb_image_copy_image (screen.file[i]->image, &screen.file[i]->transform, exportImage);
		    }
		  }
		  export_rs274x_file_from_image (exportFilename, exportImage);
		  free_gerb_image (exportImage);
		}
		/* otherwise, just export the single image file as it is */
		else {
		  export_rs274x_file_from_image (exportFilename, screen.file[0]->image);
		}
	    }
	    else {
		fprintf(stderr, "A valid file was not loaded.\n");
		exit(1);
	    }
	} else if (exportType == 6) {
	    if (screen.file[0]->image) {
		/* if we have more than one file, we need to merge them before exporting */
		if (screen.file[1]) {
		  gerb_image_t *exportImage;
		  exportImage = gerb_image_duplicate_image (screen.file[0]->image, &screen.file[0]->transform);
		  for(i = screen.max_files-1; i > 0; i--) {
		    if (screen.file[i]) {
		      gerb_image_copy_image (screen.file[i]->image, &screen.file[i]->transform, exportImage);
		    }
		  }
		  export_drill_file_from_image (exportFilename, exportImage);
		  free_gerb_image (exportImage);
		}
		/* otherwise, just export the single image file as it is */
		else {
		  export_drill_file_from_image (exportFilename, screen.file[0]->image);
		}
	    }
	    else {
		fprintf(stderr, "A valid file was not loaded.\n");
		exit(1);
	    }
	}

	if (freeFilename)
	    free (exportFilename);
	/* exit now and don't start up gtk if this is a command line export */
	exit(1);
    }
#ifndef RENDER_USING_GDK
    gtk_init (&argc, &argv);
#endif
    interface_create_gui (req_width, req_height);
    
    return 0;
} /* main */

