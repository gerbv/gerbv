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

#include "gerbv.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#ifdef WIN32
# include <windows.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "common.h"
#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "render.h"
#include "project.h"

#if (DEBUG)
# define dprintf printf("%s():  ", __FUNCTION__); printf
#else
# define dprintf if(0) printf
#endif

#define NUMBER_OF_DEFAULT_COLORS 18
#define NUMBER_OF_DEFAULT_TRANSFORMATIONS 20

static void
gerbv_print_help(void);

static int
getopt_configured(int argc, char * const argv[], const char *optstring,
		const struct option *longopts, int *longindex);
static int
getopt_lengh_unit(const char *optarg, double *input_div,
		gerbv_screen_t *screen);

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
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},	
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},		
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},	
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
	{0,0,1,1,0,FALSE,FALSE,FALSE},
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
    {"rotate",          required_argument,  NULL,    'r'},
    {"mirror",          required_argument,  NULL,    'm'},
    {"help",            no_argument,	    NULL,    'h'},
    {"log",             required_argument,  NULL,    'l'},
    {"output",          required_argument,  NULL,    'o'},
    {"project",         required_argument,  NULL,    'p'},
    {"tools",           required_argument,  NULL,    't'},
    {"translate",       required_argument,  NULL,    'T'},
    {"units",           required_argument,  NULL,    'u'},
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
const char *opt_options = "VadhB:D:O:W:b:f:r:m:l:o:p:t:T:u:w:x:";

/**Global state variable to keep track of what's happening on the screen.
   Declared extern in main.h
 */
gerbv_project_t *mainProject;
gerbv_screen_t screen;

gboolean logToFileOption;
gchar *logToFileFilename;

/* Coords like "0x2" parsed by "%fx%f" will result in first number = 2 and
   second number 0. Replace 'x' in coordinate string with ';'*/
static void
care_for_x_in_cords(char *string)
{
	char *found;
	found = strchr(string, 'x');
	if (!found)
		found = strchr(string, 'X');
	if (found)
		*found = ';';
}

/* ------------------------------------------------------------------ */
void 
main_open_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
	project_list_t *list, *plist;
	gint i, max_layer_num = -1;
	gerbv_fileinfo_t *file_info;

	dprintf("Opening project = %s\n", (gchar *) filename);
	list = read_project_file(filename);

	if (!list) {
		GERB_COMPILE_WARNING(_("Could not read \"%s\" (loaded %d)"),
				(gchar *) filename, gerbvProject->last_loaded);

		return;
	}

	/* Get the max layer number in the project list */
	plist = list;
	while (plist) {
		if (plist->layerno > max_layer_num)
			max_layer_num = plist->layerno;

		plist = plist->next;
	}

	/* Increase the layer count each time and find (if any) the
	 * corresponding entry */
	for (i = -1; i <= max_layer_num; i++) {
		plist = list;
		while (plist) {
			if (plist->layerno != i) {
				plist = plist->next;
				continue;
			}

			GdkColor colorTemplate = {0,
				plist->rgb[0], plist->rgb[1], plist->rgb[2]};
			if (i == -1) {
				screen.background_is_from_project= TRUE;
				gerbvProject->background = colorTemplate;
				plist = plist->next;
				continue;
			}

			gchar *fullName = NULL;
			gchar *dirName = NULL;
			gint fileIndex = gerbvProject->last_loaded + 1;

			if (!g_path_is_absolute (plist->filename)) {
				/* Build the full pathname to the layer */
				dirName = g_path_get_dirname (filename);
				fullName = g_build_filename (dirName,
					plist->filename, NULL);
			} else {
				fullName = g_strdup (plist->filename);
			}

			if (gerbv_open_image(gerbvProject, fullName,
					fileIndex, FALSE,
					plist->attr_list,
					plist->n_attr, TRUE) == -1) {
				GERB_MESSAGE(_("could not read file: %s"),
						fullName);
				plist = plist->next;
				continue;
			}

			g_free (dirName);
			g_free (fullName);

			/* Change color from default to from the project list */
			file_info = gerbvProject->file[fileIndex];
			file_info->color = colorTemplate;
			file_info->alpha = plist->alpha;
			file_info->transform.inverted =	plist->inverted;
			file_info->transform.translateX = plist->translate_x;
			file_info->transform.translateY = plist->translate_y;
			file_info->transform.rotation = plist->rotation;
			file_info->transform.scaleX = plist->scale_x;
			file_info->transform.scaleY = plist->scale_y;
			file_info->transform.mirrorAroundX = plist->mirror_x;
			file_info->transform.mirrorAroundY = plist->mirror_y;
			file_info->isVisible = plist->visible;

			plist = plist->next;
		}
	}

	project_destroy_project_list(list);

	/* Save project filename for later use */
	if (gerbvProject->project) {
		g_free(gerbvProject->project);
		gerbvProject->project = NULL;
	}
	gerbvProject->project = g_strdup(filename);
	if (gerbvProject->project == NULL)
		GERB_FATAL_ERROR("malloc gerbvProject->project failed in %s()",
				__FUNCTION__);
} /* gerbv_open_project_from_filename */

/* ------------------------------------------------------------------ */
void 
main_save_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
    project_list_t *list, *plist;
    gchar *dirName = g_path_get_dirname (filename);
    gerbv_fileinfo_t *file_info;
    int idx;
    
    list = g_new0 (project_list_t, 1);
    list->next = NULL;
    list->layerno = -1;
    list->filename = g_strdup(gerbvProject->path);
    list->rgb[0] = gerbvProject->background.red;
    list->rgb[1] = gerbvProject->background.green;
    list->rgb[2] = gerbvProject->background.blue;
    
    /* loop over all layer files */
    for (idx = 0; idx <= gerbvProject->last_loaded; idx++) {
	if (gerbvProject->file[idx]) {
	    plist = g_new0 (project_list_t, 1);
	    plist->next = list;
	    plist->layerno = idx;
	    
	    /* figure out the relative path to the layer from the project
	       directory */
	    if (strncmp (dirName, gerbvProject->file[idx]->fullPathname, strlen(dirName)) == 0) {
		/* skip over the common dirname and the separator */
		plist->filename = g_strdup(gerbvProject->file[idx]->fullPathname + strlen(dirName) + 1);
	    } else {
		/* if we can't figure out a relative path, just save the 
		 * absolute one */
		plist->filename = g_strdup(gerbvProject->file[idx]->fullPathname);
	    }
	    file_info = gerbvProject->file[idx];
	    plist->rgb[0] =		file_info->color.red;
	    plist->rgb[1] =		file_info->color.green;
	    plist->rgb[2] =		file_info->color.blue;
	    plist->alpha =		file_info->alpha;
	    plist->inverted =		file_info->transform.inverted;
	    plist->visible =		file_info->isVisible;
	    plist->translate_x =	file_info->transform.translateX;
	    plist->translate_y =	file_info->transform.translateY;
	    plist->rotation =		file_info->transform.rotation;
	    plist->scale_x =		file_info->transform.scaleX;
	    plist->scale_y =		file_info->transform.scaleY;
	    plist->mirror_x =		file_info->transform.mirrorAroundX;
	    plist->mirror_y =		file_info->transform.mirrorAroundY;
	    list= plist;
	}
    }
    
    if (write_project_file(gerbvProject, gerbvProject->project, list)) {
	GERB_MESSAGE(_("Failed to write project"));
    }
    project_destroy_project_list(list);
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
	GERB_FATAL_ERROR("malloc gerbvProject->project failed in %s()",
			__FUNCTION__);
    main_save_project_from_filename (gerbvProject, filename);
} /* gerbv_save_as_project_from_filename */

GArray *log_array_tmp = NULL;

/* Temporary log messages handler. It will store log messages before GUI
 * initialization. */
void
callbacks_temporary_handle_log_messages(const gchar *log_domain,
		GLogLevelFlags log_level,
		const gchar *message, gpointer user_data) {
    struct log_struct item;

    item.domain = g_strdup (log_domain);
    item.level = log_level;
    item.message = g_strdup (message);
    g_array_append_val (log_array_tmp, item);

    g_log_default_handler (log_domain, log_level, message, user_data);
}

#ifdef WIN32
static void
wait_console_for_win(void)
{
    FILE *console = fopen("CONOUT$", "w");

    fprintf(console, _("\n*** Press Enter to continue ***"));
    fflush(console);
}

/* Attach console in application which is build with -mwindows flag */
static void
attach_console_for_win(void)
{
    if (((HANDLE)_get_osfhandle(fileno(stdout)) == INVALID_HANDLE_VALUE
      || (HANDLE)_get_osfhandle(fileno(stderr)) == INVALID_HANDLE_VALUE)
    && AttachConsole(ATTACH_PARENT_PROCESS)) {

	if ((HANDLE)_get_osfhandle(fileno (stdout)) == INVALID_HANDLE_VALUE)
	    freopen("CONOUT$", "w", stdout);

	if ((HANDLE)_get_osfhandle(fileno (stderr)) == INVALID_HANDLE_VALUE)
	    freopen("CONOUT$", "w", stderr);

	atexit(wait_console_for_win);
    }
}
#else
static void
attach_console_for_win(void) {}
#endif

/* ------------------------------------------------------------------ */
int
main(int argc, char *argv[])
{
    int       read_opt;
    int       i,r,g,b,a;
    int       req_width = -1, req_height = -1;
#ifdef HAVE_GETOPT_LONG
    char      *rest;
#endif
    char      *project_filename = NULL;
    gboolean userSuppliedOrigin=FALSE, userSuppliedWindow=FALSE, 
	     userSuppliedAntiAlias=FALSE, userSuppliedWindowInPixels=FALSE, userSuppliedDpi=FALSE;
    gint  layerctr =0, transformCount = 0;
    gdouble initial_rotation = 0.0;
    gdouble input_divisor = 1.0; /* 1.0 for inch */
    int unit_flag_counter;
    gboolean initial_mirror_x = FALSE;
    gboolean initial_mirror_y = FALSE;
    const gchar *exportFilename = NULL;
    gfloat userSuppliedOriginX=0.0,userSuppliedOriginY=0.0,userSuppliedDpiX=72.0, userSuppliedDpiY=72.0, 
	   userSuppliedWidth=0, userSuppliedHeight=0,
	   userSuppliedBorder = GERBV_DEFAULT_BORDER_COEFF;

    gerbv_image_t *exportImage;

    enum exp_type {
	EXP_TYPE_NONE = -1,
	EXP_TYPE_PNG,
	EXP_TYPE_PDF,
	EXP_TYPE_SVG,
	EXP_TYPE_PS,
	EXP_TYPE_RS274X,
	EXP_TYPE_DRILL,
	EXP_TYPE_IDRILL,
    };
    enum exp_type exportType = EXP_TYPE_NONE;
    const char *export_type_names[] = {
	"png",
	"pdf",
	"svg",
	"ps",
	"rs274x",
	"drill",
	"idrill",
	NULL
    };
    const gchar *export_def_file_names[] = {
	"output.png",
	"output.pdf",
	"output.svg",
	"output.ps",
	"output.gbx",
	"output.cnc",
	"output.ncp",
	NULL
    };

    const gchar *settings_schema_env = "GSETTINGS_SCHEMA_DIR";
#ifdef WIN32
    /* On Windows executable can be not in bin/ dir */
    const gchar *settings_schema_fallback_dir =
	    "share/glib-2.0/schemas" G_SEARCHPATH_SEPARATOR_S
	    "../share/glib-2.0/schemas";
#else
    const gchar *settings_schema_fallback_dir = "../share/glib-2.0/schemas";
#endif
    gchar *env_val;

#if ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
# ifdef WIN32
    bind_textdomain_codeset(PACKAGE, "UTF-8");
# endif
    textdomain(PACKAGE);
#endif

    attach_console_for_win();

    /*
     * Setup the screen info. Must do this before getopt, since getopt
     * eventually will set some variables in screen.
     */
    memset((void *)&screen, 0, sizeof(gerbv_screen_t));
    screen.state = NORMAL;
    screen.unit = GERBV_DEFAULT_UNIT;
    
    mainProject = gerbv_create_project();
    mainProject->execname = g_strdup(argv[0]);
    mainProject->execpath = g_path_get_dirname(argv[0]);

    /* Add "fallback" directory with settings schema file from this
     * executable path */
    if (NULL == g_getenv(settings_schema_env))
	    /* Empty env var */
	    env_val = g_strconcat(
		    mainProject->execpath, G_DIR_SEPARATOR_S,
				settings_schema_fallback_dir,
		    NULL);
    else
	    /* Not empty env var */
	    env_val = g_strconcat(g_getenv(settings_schema_env),
		    G_SEARCHPATH_SEPARATOR_S,
		    mainProject->execpath, G_DIR_SEPARATOR_S,
				settings_schema_fallback_dir,
		    NULL);
    g_setenv(settings_schema_env, env_val, TRUE);
    g_free(env_val);
    
    /* set default rendering mode */
#ifdef WIN32
    /* Cairo seems to render faster on Windows, so use it for default */
    screenRenderInfo.renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL;
#else
    screenRenderInfo.renderType = GERBV_RENDER_TYPE_GDK;
#endif

    logToFileOption = FALSE;
    logToFileFilename = NULL;

    log_array_tmp = g_array_new (TRUE, FALSE, sizeof (struct log_struct));
    g_log_set_handler (NULL,
		    G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION | G_LOG_LEVEL_MASK,
		    callbacks_temporary_handle_log_messages, NULL);


    /* 1. Process "length unit" command line flag */
    unit_flag_counter = 0;
    opterr = 0; /* Disable getopt() error messages */
    while (-1 != (read_opt = getopt_configured(argc, argv, opt_options,
				    longopts, &longopt_idx))) {
	switch (read_opt) {
	case 'u':
	    unit_flag_counter++;

	    if (!getopt_lengh_unit(optarg, &input_divisor, &screen))
		GERB_COMPILE_WARNING(
			_("Unrecognized length unit \"%s\" in command line"),
			optarg);

	    break;
	}
    }

    /* 2. Process all other command line flags */
    optind = 0; /* Reset getopt() index */
    opterr = 1; /* Enable getopt() error messages */
    while (-1 != (read_opt = getopt_configured(argc, argv, opt_options,
				    longopts, &longopt_idx))) {
	switch (read_opt) {
#ifdef HAVE_GETOPT_LONG
	case 0:
	    /* Only long options like GDK/GTK debug */
	    switch (longopt_val) {
	    case 0: /* default value if nothing is set */
		GERB_COMPILE_WARNING(
			_("Not handled option \"%s\" in command line"),
			longopts[longopt_idx].name);
		break;
	    case 1: /* geometry */
		errno = 0;
		req_width = (int)strtol(optarg, &rest, 10);
		if (errno) {
		    perror(_("Width"));
		    break;
		}
		if (rest[0] != 'x'){
		    fprintf(stderr, _("Split X and Y parameters with an x\n"));
		    break;
		}
		rest++;
		errno = 0;
		req_height = (int)strtol(rest, &rest, 10);
		if (errno) {
		    perror(_("Height"));
		    break;
		}
		/*
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
		*/
		break;
	    default:
		break;
	    }
	    break;
#endif /* HAVE_GETOPT_LONG */
    	case 'B' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must specify the border in the format <alpha>.\n"));
		exit(1);
	    }
	    if (strlen (optarg) > 10) {
		fprintf(stderr, _("Specified border is not recognized.\n"));
		exit(1);
	    }
	    sscanf (optarg,"%f",&userSuppliedBorder);
	    if (userSuppliedBorder <  0) {
		fprintf(stderr, _("Specified border is smaller than zero!\n"));
		exit(1);
	    }
	    userSuppliedBorder/=100.0;
	    break;
	case 'D' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give an resolution in the format <DPI_XxDPI_Y> or <DPI_X_and_Y>.\n"));
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, _("Specified resolution is not recognized.\n"));
		exit(1);
	    }
	    if(strchr(optarg, 'x')!=NULL){
		sscanf (optarg,"%fx%f",&userSuppliedDpiX,&userSuppliedDpiY);
	    }else{
		sscanf (optarg,"%f",&userSuppliedDpiX);
		userSuppliedDpiY = userSuppliedDpiX;
	    }
	    if ((userSuppliedDpiX <= 0) || (userSuppliedDpiY <= 0)) {
		fprintf(stderr, _("Specified resolution should be greater than 0.\n"));
		exit(1);
	    }
	    userSuppliedDpi=TRUE;
	    break;
    	case 'O' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give an origin in the format "
					"<XxY> or <X;Y>.\n"));
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, _("Specified origin is not recognized.\n"));
		exit(1);
	    }

	    care_for_x_in_cords(optarg);
	    sscanf(optarg,"%f;%f", &userSuppliedOriginX, &userSuppliedOriginY);
	    userSuppliedOriginX /= input_divisor;
	    userSuppliedOriginY /= input_divisor;
	    userSuppliedOrigin=TRUE;
	    break;
    	case 'V' :
	    printf(_("gerbv version %s\n"), VERSION);
	    printf(_("Copyright (C) 2001-2008 by Stefan Petersen\n"
		"and the respective original authors listed in the source files.\n"));
	    exit(0);	
	case 'a' :
	    userSuppliedAntiAlias = TRUE;
	    break;
    	case 'b' :	// Set background to this color
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give an background color "
					"in the hex-format <#RRGGBB>.\n"));
		exit(1);
	    }
	    if ((strlen (optarg) != 7)||(optarg[0]!='#')) {
		fprintf(stderr, _("Specified color format "
					"is not recognized.\n"));
		exit(1);
	    }
    	    r=g=b=-1;
	    sscanf (optarg,"#%2x%2x%2x",&r,&g,&b);
	    if ( (r<0)||(r>255)||(g<0)||(g>255)||(b<0)||(b>255)) {

		fprintf(stderr, _("Specified color values should be "
					"between 00 and FF.\n"));
		exit(1);
	    }

	    screen.background_is_from_cmdline = TRUE;
	    mainProject->background.red = r*257;
    	    mainProject->background.green = g*257;
    	    mainProject->background.blue = b*257;

	    break;
	case 'f' :	// Set layer colors to this color (foreground color)
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give an foreground color in the hex-format <#RRGGBB> or <#RRGGBBAA>.\n"));
		exit(1);
	    }
	    if (((strlen (optarg) != 7)&&(strlen (optarg) != 9))||(optarg[0]!='#')) {
		fprintf(stderr, _("Specified color format is not recognized.\n"));
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

		fprintf(stderr, _("Specified color values should be between 0x00 (0) and 0xFF (255).\n"));
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
	case 'r' :	// Set initial orientation for all layers (rotation)
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give the initial rotation angle\n"));
		exit(1);
	    }
	    errno = 0;
	    initial_rotation = (gdouble)strtod(optarg, &rest);
	    if (errno) {
		perror(_("Rotate"));
		exit(1);
	    }
	    if (*rest) {
		fprintf(stderr, _("Failed parsing rotate value\n"));
		exit(1);
	    }
	    break;
	case 'm' :	// Set initial mirroring state for all layers
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give the axis to mirror about\n"));
		exit(1);
	    }
	    if (strchr(optarg, 'x') != NULL || strchr(optarg, 'X') != NULL) {
		initial_mirror_x = TRUE;
	    }
	    if (strchr(optarg, 'y') != NULL || strchr(optarg, 'Y') != NULL) {
		initial_mirror_y = TRUE;
	    }
	    if (!(initial_mirror_x || initial_mirror_y)) {
		fprintf(stderr, _("Failed parsing mirror axis\n"));
		exit(1);
	    }
	    break;
	case 'l' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give a filename to send log to\n"));
		exit(1);
	    }
	    logToFileOption = TRUE;
	    logToFileFilename = optarg;
	    break;
    	case 'o' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give a filename to export to.\n"));
		exit(1);
	    }
	    exportFilename = optarg;
	    break;
	case 'p' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give a project filename\n"));
		exit(1);
	    }
	    project_filename = optarg;
	    break;
	case 't' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give a filename to read the tools from.\n"));
		exit(1);
	    }
	    if (!gerbv_process_tools_file(optarg)) {
		fprintf(stderr, _("*** ERROR processing tools file \"%s\".\n"), optarg);
		fprintf(stderr, _("Make sure all lines of the file are formatted like this:\n"
			"T01 0.024\nT02 0.032\nT03 0.040\n...\n"
			"*** EXITING to prevent erroneous display.\n"));
		exit(1);
	    }
	    break;
	case 'T' :	// Translate the layer
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give a translation in the format "
					"<XxY> or <X;Y>.\n"));
		exit(1);
	    }
	    if (strlen (optarg) > 30) {
		fprintf(stderr, _("The translation format is not recognized.\n"));
		exit(1);
	    }

	    float transX = 0, transY = 0, rotate = 0;

	    care_for_x_in_cords(optarg);
	    sscanf(optarg, "%f;%fr%f", &transX, &transY, &rotate);
	    transX /= input_divisor;
	    transY /= input_divisor;
	    mainDefaultTransformations[transformCount].translateX = transX;
	    mainDefaultTransformations[transformCount].translateY = transY;
	    mainDefaultTransformations[transformCount].rotation   = DEG2RAD(rotate);
	    transformCount++;
	    /* just reset the counter back to 0 if we read too many */
	    if (transformCount == NUMBER_OF_DEFAULT_TRANSFORMATIONS)
	    	transformCount = 0;
	    break;
	case 'u':
	    if (unit_flag_counter == 1)
		    /* Length unit flag occurred only once and processed */
		    break;

	    /* Length unit flag occurred more than once, process each one */
	    if (!getopt_lengh_unit(optarg, &input_divisor, &screen))
		GERB_COMPILE_WARNING(
			_("Unrecognized length unit \"%s\" in command line"),
			optarg);

	    break;

	case 'w':
	    userSuppliedWindowInPixels = TRUE;
    	case 'W' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must give a window size in the format <width x height>.\n"));
		exit(1);
	    }
	    if (strlen (optarg) > 20) {
		fprintf(stderr, _("Specified window size is not recognized.\n"));
		exit(1);
	    }
	    sscanf (optarg, "%fx%f", &userSuppliedWidth, &userSuppliedHeight);
	    if (((userSuppliedWidth < 0.001) || (userSuppliedHeight < 0.001)) ||
		((userSuppliedWidth > 2000) || (userSuppliedHeight > 2000))) {
		fprintf(stderr, _("Specified window size is out of bounds.\n"));
		exit(1);
	    }
	    userSuppliedWindow = TRUE;
	    break;
	case 'x' :
	    if (optarg == NULL) {
		fprintf(stderr, _("You must supply an export type.\n"));
		exit(1);
	    }

	    for (i = 0; export_type_names[i] != NULL; i++) {
		if (strcmp (optarg, export_type_names[i]) == 0) {
		    exportType = i;
		    break;
		}
	    }

	    if (exportType == EXP_TYPE_NONE) {
		fprintf(stderr, _("Unrecognized \"%s\" export type.\n"),
				optarg);
		exit(1);				
	    }
	    break;
	case 'd':
	    screen.dump_parsed_image = 1;
	    break;
	case '?':
	case 'h':
	    gerbv_print_help();

	    exit(1);
	    break;
	default :
	    /* This should not be reached */
	    GERB_COMPILE_WARNING(_("Not handled option '%c' in command line"),
			    read_opt);
	}
    }
    
    /*
     * If project is given, load that one and use it for files and colors.
     * Else load files (eventually) given on the command line.
     * This limits you to either give files on the commandline or just load
     * a project.
     */

    if (project_filename) {
	dprintf(_("Loading project %s...\n"), project_filename);
	/* calculate the absolute pathname to the project if the user
	   used a relative path */
	g_free (mainProject->path);
	if (!g_path_is_absolute(project_filename)) {
	    gchar *currentDir = g_get_current_dir ();
	    gchar *fullName = g_build_filename (currentDir,
						project_filename, NULL);
	    main_open_project_from_filename (mainProject, fullName);
	    mainProject->path = g_path_get_dirname (fullName);
	    g_free (fullName);
	    g_free (currentDir);
	} else {
	    main_open_project_from_filename (mainProject, project_filename);
	    mainProject->path = g_path_get_dirname (project_filename);
	}
    } else {
    	gint loadedIndex = 0;
	for(i = optind ; i < argc; i++) {
	    g_free (mainProject->path);
	    if (!g_path_is_absolute(argv[i])) {
		gchar *currentDir = g_get_current_dir ();
		gchar *fullName = g_build_filename (currentDir,
						    argv[i], NULL);
		gerbv_open_layer_from_filename_with_color (mainProject, fullName,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].red*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].green*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].blue*257,
			mainDefaultColors[loadedIndex % NUMBER_OF_DEFAULT_COLORS].alpha*257);
		mainProject->path = g_path_get_dirname (fullName);
		g_free (fullName);
		g_free (currentDir);
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

    if (initial_rotation != 0.0) {
	/* Set initial layer orientation */

	gdouble initial_radians = DEG2RAD(initial_rotation);

	dprintf("Rotating all layers by %.0f degrees\n", (float) initial_rotation);
	for(i = 0; i < mainProject->max_files; i++) {
	    if (mainProject->file[i])
		mainProject->file[i]->transform.rotation = initial_radians;
	}
    }

    if (initial_mirror_x || initial_mirror_y) {
	/* Set initial mirroring of all layers */

	if (initial_mirror_x) {
	    dprintf("Mirroring all layers about x axis\n");
	}
	if (initial_mirror_y) {
	    dprintf("Mirroring all layers about y axis\n");
	}

	for (i = 0; i < mainProject->max_files; i++) {
	    if (mainProject->file[i]) {
		mainProject->file[i]->transform.mirrorAroundX = initial_mirror_x;
		mainProject->file[i]->transform.mirrorAroundY = initial_mirror_y;
	    }
	}
    }

    if (exportType != EXP_TYPE_NONE) {
	/* load the info struct with the default values */

	if (!exportFilename)
		exportFilename = export_def_file_names[exportType];

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
		userSuppliedDpiX = MIN((userSuppliedWidth-0.5)/width,
			(userSuppliedHeight-0.5)/height);
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


	gerbv_render_info_t renderInfo = {userSuppliedDpiX, userSuppliedDpiY, 
	    userSuppliedOriginX, userSuppliedOriginY,
	    userSuppliedAntiAlias? GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY: GERBV_RENDER_TYPE_CAIRO_NORMAL,
	    userSuppliedWidth,userSuppliedHeight };
	
	switch (exportType) {
	case EXP_TYPE_PNG:
	    gerbv_export_png_file_from_project(mainProject,
			    &renderInfo, exportFilename);
	    break;
	case EXP_TYPE_PDF:
	    gerbv_export_pdf_file_from_project(mainProject,
			    &renderInfo, exportFilename);
	    break;
	case EXP_TYPE_SVG:
	    gerbv_export_svg_file_from_project(mainProject,
			    &renderInfo, exportFilename);
	    break;
	case EXP_TYPE_PS:
	    gerbv_export_postscript_file_from_project(mainProject,
			    &renderInfo, exportFilename);
	    break;
	case EXP_TYPE_RS274X:
	case EXP_TYPE_DRILL:
	case EXP_TYPE_IDRILL:
	    if (!mainProject->file[0]->image) {
		fprintf(stderr, _("A valid file was not loaded.\n"));
		exit(1);
	    }

	    exportImage = gerbv_image_duplicate_image(
			    mainProject->file[0]->image,
			    &mainDefaultTransformations[0]);

	    /* If more than one file, merge them before exporting */
	    for (i = mainProject->last_loaded; i > 0; i--) {
		if (mainProject->file[i])
			gerbv_image_copy_image(mainProject->file[i]->image,
				&mainDefaultTransformations[i], exportImage);
	    }

	    switch (exportType) {
	    case EXP_TYPE_RS274X:
		gerbv_export_rs274x_file_from_image(exportFilename,
			exportImage, &mainProject->file[0]->transform);
		break;
	    case EXP_TYPE_DRILL:
		gerbv_export_drill_file_from_image(exportFilename,
			exportImage, &mainProject->file[0]->transform);
		break;
	    case EXP_TYPE_IDRILL:
		gerbv_export_isel_drill_file_from_image (exportFilename,
			exportImage, &mainProject->file[0]->transform);
		break;
	    default:
		break;
	    }

	    gerbv_destroy_image (exportImage);
	    break;
	default:
	    fprintf(stderr, _("A valid file was not loaded.\n"));
	    exit(1);
	}

	/* exit now and don't start up gtk if this is a command line export */
	exit(0);
    }
    gtk_init (&argc, &argv);
    interface_create_gui (req_width, req_height);
    
    /* we've exited the GTK loop, so free all resources */
    render_free_screen_resources();
    gerbv_destroy_project (mainProject);
    return 0;
} /* main */

static int
getopt_configured(int argc, char * const argv[], const char *optstring,
		const struct option *longopts, int *longindex)
{
#ifdef HAVE_GETOPT_LONG
	return getopt_long(argc, argv, optstring, longopts, longindex);
#else
	return getopt(argc, argv, optstring);
#endif
}

static int
getopt_lengh_unit(const char *optarg, double *input_div, gerbv_screen_t *screen)
{
	if (strncasecmp(optarg, "mm", 2) == 0) {
		*input_div = 25.4;
		screen->unit = GERBV_MMS;
		screen->unit_is_from_cmdline = TRUE;
	} else if (strncasecmp(optarg, "mil", 3) == 0) {
		*input_div = 1000.0;
		screen->unit = GERBV_MILS;
		screen->unit_is_from_cmdline = TRUE;
	} else if (strncasecmp(optarg, "inch", 4) == 0) {
		*input_div = 1.0;
		screen->unit = GERBV_INS;
		screen->unit_is_from_cmdline = TRUE;
	} else {
		return 0;
	}

	return 1;
}

static void
gerbv_print_help(void)
{
	printf(_(
"Usage: gerbv [OPTIONS...] [FILE...]\n"
"\n"
"Available options:\n"));

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -B, --border=<b>        Border around the image in percent of the\n"
"                          width/height. Defaults to %d%%.\n"),
			(int)(100*GERBV_DEFAULT_BORDER_COEFF));
#else
	printf(_(
"  -B<b>                   Border around the image in percent of the\n"
"                          width/height. Defaults to %d%%.\n"),
			(int)(100*GERBV_DEFAULT_BORDER_COEFF));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -D, --dpi=<XxY|R>       Resolution (Dots per inch) for the output\n"
"                          bitmap. With the format <XxY>, different\n"
"                          resolutions for X- and Y-direction are used.\n"
"                          With the format <R>, both are the same.\n"));
#else
	printf(_(
"  -D<XxY|R>               Resolution (Dots per inch) for the output\n"
"                          bitmap. With the format <XxY>, different\n"
"                          resolutions for X- and Y-direction are used.\n"
"                          With the format <R>, both are the same.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -O, --origin=<XxY|X;Y>  Use the specified coordinates (in inches)\n"
"                          for the lower left corner.\n"));
#else
	printf(_(
"  -O<XxY|X;Y>             Use the specified coordinates (in inches)\n"
"                          for the lower left corner.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -V, --version           Print version of Gerbv.\n"));
#else
	printf(_(
"  -V                      Print version of Gerbv.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -a, --antialias         Use antialiasing for generated bitmap output.\n"));
#else
	printf(_(
"  -a                      Use antialiasing for generated bitmap output.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -b, --background=<hex>  Use background color <hex> (like #RRGGBB).\n"));
#else
	printf(_(
"  -b<hexcolor>            Use background color <hexcolor> (like #RRGGBB).\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -f, --foreground=<hex>  Use foreground color <hex> (like #RRGGBB or\n"
"                          #RRGGBBAA for setting the alpha).\n"
"                          Use multiple -f flags to set the color for\n"
"                          multiple layers.\n"));
#else
	printf(_(
"  -f<hexcolor>            Use foreground color <hexcolor> (like #RRGGBB or\n"
"                          #RRGGBBAA for setting the alpha).\n"
"                          Use multiple -f flags to set the color for\n"
"                          multiple layers.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -r, --rotate=<degree>   Set initial orientation for all layers.\n"));
#else
	printf(_(
"  -r<degree>              Set initial orientation for all layers.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -m, --mirror=<axis>     Set initial mirroring axis (X or Y).\n"));
#else
	printf(_(
"  -m<axis>                Set initial mirroring axis (X or Y).\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -h, --help              Print this help message.\n"));
#else
	printf(_(
"  -h                      Print this help message.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -l, --log=<logfile>     Send error messages to <logfile>.\n"));
#else
	printf(_(
"  -l<logfile>             Send error messages to <logfile>.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -o, --output=<filename> Export to <filename>.\n"));
#else
	printf(_(
"  -o<filename>            Export to <filename>.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -p, --project=<prjfile> Load project file <prjfile>.\n"));
#else
	printf(_(
"  -p<prjfile>             Load project file <prjfile>.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -u, --units=<inch|mm|mil>\n"
"                          Use given unit for coordinates.\n"
"                          Default to inch.\n"));
#else
	printf(_(
"  -u<inch|mm|mil>         Use given unit for coordinates.\n"
"                          Default to inch.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -W, --window_inch=<WxH> Window size in inches <WxH> for the exported image.\n"));
#else
	printf(_(
"  -W<WxH>                 Window size in inches <WxH> for the exported image.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -w, --window=<WxH>      Window size in pixels <WxH> for the exported image.\n"
"                          Autoscales to fit if no resolution is specified.\n"
"                          If a resolution is specified, it will clip\n"
"                          exported image.\n"));
#else
	printf(_(
"  -w<WxH>                 Window size in pixels <WxH> for the exported image.\n"
"                          Autoscales to fit if no resolution is specified.\n"
"                          If a resolution is specified, it will clip\n"
"                          exported image.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -t, --tools=<toolfile>  Read Excellon tools from file <toolfile>.\n"));
#else
	printf(_(
"  -t<toolfile>            Read Excellon tools from file <toolfile>\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -T, --translate=<XxYrR| Translate image by X and Y and rotate by R degree.\n"
"                   X;YrR> Useful for arranging panels.\n"
"                          Use multiple -T flags for multiple layers.\n"
"                          Only evaluated when exporting as RS274X or drill.\n"));
#else
	printf(_(
"  -T<XxYrR|X;YrR>         Translate image by X and Y and rotate by R degree.\n"
"                          Useful for arranging panels.\n"
"                          Use multiple -T flags for multiple files.\n"
"                          Only evaluated when exporting as RS274X or drill.\n"));
#endif

#ifdef HAVE_GETOPT_LONG
	printf(_(
"  -x, --export=<png|pdf|ps|svg|rs274x|drill|idrill>\n"
"                          Export a rendered picture to a file with\n"
"                          the specified format.\n"));
#else
	printf(_(
"  -x<png|pdf|ps|svg|      Export a rendered picture to a file with\n"
"     rs274x|drill|        the specified format.\n"
"     idrill>\n"));
#endif

}
