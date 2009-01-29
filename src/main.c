/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2008 Julian Lamb
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

/** \file main.c
    \brief The main code for the Gerber Viewer GUI application and command line processing
    \ingroup gerbv
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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

#include <locale.h>

#include "gerbv.h"
#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "render.h"
#include "project.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

#define NUMBER_OF_DEFAULT_COLORS 18
#define NUMBER_OF_DEFAULT_TRANSFORMATIONS 20

static gerbv_layer_color mainDefaultColors[NUMBER_OF_DEFAULT_COLORS] = {
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

static gerbv_user_transformation_t mainDefaultTransformations[NUMBER_OF_DEFAULT_TRANSFORMATIONS] = {
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
gerbv_project_t *mainProject;
gerbv_screen_t screen;

gboolean logToFileOption;
gchar *logToFileFilename;

/* ------------------------------------------------------------------ */
void 
main_open_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
	project_list_t *project_list, *originalList;
	gint i,maxLayerNumber = -1;

	dprintf("Opening project = %s\n", (gchar *) filename);
	originalList = project_list = read_project_file(filename);

	if (project_list) {
		/* first, get the max layer number in the project list */
		while (project_list) {
			if (project_list->layerno > maxLayerNumber)
				maxLayerNumber = project_list->layerno;
			project_list = project_list->next;
		}
		project_list = originalList;
		/* increase the layer count each time and find (if any) the corresponding entry */
		for (i = -1; i<=maxLayerNumber; i++){
			project_list = originalList;
			while (project_list) {
				if (project_list->layerno == i) {
					GdkColor colorTemplate = {0,project_list->rgb[0],
					project_list->rgb[1],project_list->rgb[2]};
					if (i == -1) {
						gerbvProject->background = colorTemplate;
					}
					else {
						gchar *fullName = NULL;
						gchar *dirName = NULL;
						gint fileIndex = gerbvProject->last_loaded + 1;

						if (!g_path_is_absolute (project_list->filename)) {
							/* build the full pathname to the layer */
							dirName = g_path_get_dirname (filename);
							fullName = g_build_filename (dirName,
								project_list->filename, NULL);
						}
						else {
							fullName = g_strdup (project_list->filename);
						}
						if (gerbv_open_image(gerbvProject, fullName,
								fileIndex, FALSE, 
								project_list->attr_list, 
								project_list->n_attr, TRUE) == -1) {
							GERB_MESSAGE("could not read file: %s", fullName);
						}
						else {
							g_free (dirName);
							g_free (fullName);
							/* 
							* Change color from default to from the project list
							*/
							gerbvProject->file[fileIndex]->color = colorTemplate;
							gerbvProject->file[fileIndex]->transform.inverted = project_list->inverted;
							gerbvProject->file[fileIndex]->isVisible = project_list->visible;
						}
					}
				}
				project_list = project_list->next;
			}
		}
		project_destroy_project_list (originalList);
		/*
		* Save project filename for later use
		*/
		if (gerbvProject->project) {
		g_free(gerbvProject->project);
		gerbvProject->project = NULL;
		}
		gerbvProject->project = g_strdup (filename);
		if (gerbvProject->project == NULL)
		GERB_FATAL_ERROR("malloc gerbvProject->project failed\n");
	}
	else {
		GERB_MESSAGE("could not read %s[%d]\n", (gchar *) filename,
		gerbvProject->last_loaded);
	}
} /* gerbv_open_project_from_filename */

/* ------------------------------------------------------------------ */
void 
main_save_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
    project_list_t *project_list = NULL, *tmp;
    int idx;
    gchar *dirName = g_path_get_dirname (filename);
    
    project_list = g_new0 (project_list_t, 1);
    project_list->next = project_list;
    project_list->layerno = -1;
    project_list->filename = g_strdup(gerbvProject->path);
    project_list->rgb[0] = gerbvProject->background.red;
    project_list->rgb[1] = gerbvProject->background.green;
    project_list->rgb[2] = gerbvProject->background.blue;
    project_list->next = NULL;
    
    /* loop over all layer files */
    for (idx = 0; idx <= gerbvProject->last_loaded; idx++) {
	if (gerbvProject->file[idx]) {
	    tmp = g_new0 (project_list_t, 1);
	    tmp->next = project_list;
	    tmp->layerno = idx;
	    
	    /* figure out the relative path to the layer from the project
	       directory */
	    if (strncmp (dirName, gerbvProject->file[idx]->fullPathname, strlen(dirName)) == 0) {
		/* skip over the common dirname and the separator */
		tmp->filename = g_strdup(gerbvProject->file[idx]->fullPathname + strlen(dirName) + 1);
	    } else {
		/* if we can't figure out a relative path, just save the 
		 * absolute one */
		tmp->filename = g_strdup(gerbvProject->file[idx]->fullPathname);
	    }
	    tmp->rgb[0] = gerbvProject->file[idx]->color.red;
	    tmp->rgb[1] = gerbvProject->file[idx]->color.green;
	    tmp->rgb[2] = gerbvProject->file[idx]->color.blue;
	    tmp->inverted = gerbvProject->file[idx]->transform.inverted;
	    tmp->visible = gerbvProject->file[idx]->isVisible;

	    project_list = tmp;
	}
    }
    
    if (write_project_file(gerbvProject, gerbvProject->project, project_list)) {
	GERB_MESSAGE("Failed to write project\n");
    }
    project_destroy_project_list (project_list);
    g_free (dirName);
} /* gerbv_save_project_from_filename */

/* ------------------------------------------------------------------ */
void 
main_save_as_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
	
    /*
     * Save project filename for later use
     */
    if (gerbvProject->project) {
	g_free(gerbvProject->project);
	gerbvProject->project = NULL;
    }
    gerbvProject->project = g_strdup(filename);
    if (gerbvProject->project == NULL)
	GERB_FATAL_ERROR("malloc gerbvProject->project failed\n");
    main_save_project_from_filename (gerbvProject, filename);
} /* gerbv_save_as_project_from_filename */


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
    
    mainProject = gerbv_create_project();
    mainProject->execname = g_strdup(argv[0]);
    mainProject->execpath = g_path_get_dirname(argv[0]);
    
    /* set default rendering mode */
#ifdef WIN32
    /* Cairo seems to render faster on Windows, so use it for default */
    screenRenderInfo.renderType = 2;
#else
    screenRenderInfo.renderType = 0;
#endif

    logToFileOption = FALSE;
    logToFileFilename = NULL;
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
	    printf("Copyright (C) 2001 -- 2008 by Stefan Petersen\n");
	    printf("and the respective original authors listed in the source files.\n");
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
	    mainProject->background.red = r*257;
    	    mainProject->background.green = g*257;
    	    mainProject->background.blue = b*257;
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
	    mainDefaultColors[layerctr].red   = r;
	    mainDefaultColors[layerctr].green = g;
	    mainDefaultColors[layerctr].blue  = b;
	    mainDefaultColors[layerctr].alpha = a;
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
	    logToFileOption = TRUE;
	    logToFileFilename = optarg;
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
	    if (!gerbv_process_tools_file(optarg)) {
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
	    mainDefaultTransformations[transformCount].translateX = transX;
	    mainDefaultTransformations[transformCount].translateY = transY;
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
	printf("Loading project %s...\n", project_filename);
	/* calculate the absolute pathname to the project if the user
	   used a relative path */
	g_free (mainProject->path);
	if (!g_path_is_absolute(project_filename)) {
	    gchar *fullName = g_build_filename (g_get_current_dir (),
						project_filename, NULL);
	    main_open_project_from_filename (mainProject, fullName);
	    mainProject->path = g_path_get_dirname (fullName);
	    g_free (fullName);
	} else {
	    main_open_project_from_filename (mainProject, project_filename);
	    mainProject->path = g_path_get_dirname (project_filename);
	}
    } else {
    	gint loadedIndex = 0;
	for(i = optind ; i < argc; i++) {
	    g_free (mainProject->path);
	    if (!g_path_is_absolute(argv[i])) {
		gchar *fullName = g_build_filename (g_get_current_dir (),
						    argv[i], NULL);
		gerbv_open_layer_from_filename_with_color (mainProject, fullName,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].red*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].green*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].blue*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].alpha*257);
		mainProject->path = g_path_get_dirname (fullName);
		g_free (fullName);
	    } else {
		gerbv_open_layer_from_filename_with_color (mainProject, argv[i],
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].red*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].green*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].blue*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].alpha*257);
		mainProject->path = g_path_get_dirname (argv[i]);
	    }
	    loadedIndex++;
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
	gerbv_render_get_boundingbox(mainProject, &bb);
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
	      userSuppliedOriginX -= (userSuppliedWidth*userSuppliedBorder)/2.0;
		userSuppliedOriginY -= (userSuppliedHeight*userSuppliedBorder)/2.0;
		userSuppliedWidth  += userSuppliedWidth*userSuppliedBorder;
		userSuppliedHeight  += userSuppliedHeight*userSuppliedBorder;
	    }
	    // If supplied in pixels, shrink image content for border_size
	    else{
	      userSuppliedOriginX -= ((userSuppliedWidth/userSuppliedDpiX)*userSuppliedBorder)/2.0;
		userSuppliedOriginY -= ((userSuppliedHeight/userSuppliedDpiX)*userSuppliedBorder)/2.0;
		userSuppliedDpiX -= (userSuppliedDpiX*userSuppliedBorder);
		userSuppliedDpiY -= (userSuppliedDpiY*userSuppliedBorder);
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
	    gerbv_export_png_file_from_project (mainProject, &renderInfo, exportFilename);
#endif
	} else if (exportType == 2) {
	    gerbv_export_pdf_file_from_project (mainProject, &renderInfo, exportFilename);
	} else if (exportType == 3) {
	    gerbv_export_svg_file_from_project (mainProject, &renderInfo, exportFilename);
	} else if (exportType == 4) {
	    gerbv_export_postscript_file_from_project (mainProject, &renderInfo, exportFilename);
	} else if (exportType == 5) {
	    if (mainProject->file[0]->image) {
		gerbv_image_t *exportImage;
		exportImage = gerbv_image_duplicate_image (mainProject->file[0]->image, &mainDefaultTransformations[0]);
		/* if we have more than one file, we need to merge them before exporting */
		for(i = mainProject->last_loaded; i > 0; i--) {
		  if (mainProject->file[i]) {
		    gerbv_image_copy_image (mainProject->file[i]->image, &mainDefaultTransformations[i], exportImage);
		  }
		}
		gerbv_export_rs274x_file_from_image (exportFilename, exportImage);
		gerbv_destroy_image (exportImage);
	    }
	    else {
		fprintf(stderr, "A valid file was not loaded.\n");
		exit(1);
	    }
	} else if (exportType == 6) {
	    if (mainProject->file[0]->image) {
	      gerbv_image_t *exportImage;
		exportImage = gerbv_image_duplicate_image (mainProject->file[0]->image, &mainDefaultTransformations[0]);
		/* if we have more than one file, we need to merge them before exporting */
		for(i = mainProject->last_loaded; i > 0; i--) {
		  if (mainProject->file[i]) {
		    gerbv_image_copy_image (mainProject->file[i]->image, &mainDefaultTransformations[i], exportImage);
		  }
		}
		gerbv_export_drill_file_from_image (exportFilename, exportImage);
		gerbv_destroy_image (exportImage);
	    }
	    else {
		fprintf(stderr, "A valid file was not loaded.\n");
		exit(1);
	    }
	}

	if (freeFilename)
	    free (exportFilename);
	/* exit now and don't start up gtk if this is a command line export */
	exit(0);
    }
#ifndef RENDER_USING_GDK
    gtk_init (&argc, &argv);
#endif
    interface_create_gui (req_width, req_height);
    
    /* we've exited the GTK loop, so free all resources */
    render_free_screen_resources();
    gerbv_destroy_project (mainProject);
    return 0;
} /* main */

