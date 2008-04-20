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

#include "gerbv.h"
#include "gerber.h"
#include "drill.h"

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #include "draw-gdk.h"
  #include "draw.h"
#endif

#include "project.h"
#include "tooltable.h"
#include "pick-and-place.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

#define NUMBER_OF_DEFAULT_COLORS 18
#define NUMBER_OF_DEFAULT_TRANSFORMATIONS 20

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

void 
gerbv_open_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
    project_list_t *project_list = NULL;
    
    dprintf("Opening project = %s\n", (gchar *) filename);
    project_list = read_project_file(gerbvProject, filename);
    
    if (project_list) {
	project_list_t *pl_tmp;
	
	while (project_list) {
	    GdkColor colorTemplate = {0,project_list->rgb[0],
				      project_list->rgb[1],project_list->rgb[2]};
	    if (project_list->layerno == -1) {
		gerbvProject->background = colorTemplate;
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
		if (gerbv_open_image(gerbvProject, fullName, idx, FALSE, 
				     project_list->attr_list, 
				     project_list->n_attr, TRUE) == -1) {
		    GERB_MESSAGE("could not read %s[%d]", fullName, idx);
		    goto next_layer;
		}
		g_free (dirName);
		g_free (fullName);
		/* 
		 * Change color from default to from the project list
		 */
		gerbvProject->file[idx]->color = colorTemplate;
		gerbvProject->file[idx]->transform.inverted = project_list->inverted;
		gerbvProject->file[idx]->isVisible = project_list->visible;
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
	if (gerbvProject->project) {
	    g_free(gerbvProject->project);
	    gerbvProject->project = NULL;
	}
	gerbvProject->project = g_strdup (filename);
	if (gerbvProject->project == NULL)
	    GERB_FATAL_ERROR("malloc gerbvProject->project failed\n");
    } else {
	GERB_MESSAGE("could not read %s[%d]\n", (gchar *) filename,
		     gerbvProject->last_loaded);
    }
} /* gerbv_open_project_from_filename */


void 
gerbv_open_layer_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
    dprintf("Opening filename = %s\n", (gchar *) filename);
    
    
    if (gerbv_open_image(gerbvProject, filename, ++gerbvProject->last_loaded, FALSE, NULL, 0, TRUE) == -1) {
	GERB_MESSAGE("could not read %s[%d]", (gchar *) filename,
		     gerbvProject->last_loaded);
	gerbvProject->last_loaded--;
    } else {
	dprintf("     Successfully opened file!\n");	
    }
} /* gerbv_open_layer_from_filename */

gboolean 
gerbv_save_layer_from_index(gerbv_project_t *gerbvProject, gint index, gchar *filename) 
{
    if (strcmp (gerbvProject->file[index]->image->info->type,"RS274-X (Gerber) File")==0) {
	export_rs274x_file_from_image (filename, gerbvProject->file[index]->image);
    }
    else if (strcmp (gerbvProject->file[index]->image->info->type,"Excellon Drill File")==0) {
	export_drill_file_from_image (filename, gerbvProject->file[index]->image);
    }
    else {
	return FALSE;
    }
    return TRUE;
} /* gerbv_save_project_from_filename */

void 
gerbv_save_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
    project_list_t *project_list = NULL, *tmp;
    int idx;
    gchar *dirName = g_path_get_dirname (filename);
    
    project_list = g_new0 (project_list_t, 1);
    project_list->next = project_list;
    project_list->layerno = -1;
    project_list->filename = gerbvProject->path;
    project_list->rgb[0] = gerbvProject->background.red;
    project_list->rgb[1] = gerbvProject->background.green;
    project_list->rgb[2] = gerbvProject->background.blue;
    project_list->next = NULL;
    
    for (idx = 0; idx < gerbvProject->max_files; idx++) {
	if (gerbvProject->file[idx]) {
	    tmp = g_new0 (project_list_t, 1);
	    tmp->next = project_list;
	    tmp->layerno = idx;
	    
	    /* figure out the relative path to the layer from the project
	       directory */
	    if (strncmp (dirName, gerbvProject->file[idx]->name, strlen(dirName)) == 0) {
		/* skip over the common dirname and the separator */
		tmp->filename = (gerbvProject->file[idx]->name + strlen(dirName) + 1);
	    } else {
		/* if we can't figure out a relative path, just save the 
		 * absolute one */
		tmp->filename = gerbvProject->file[idx]->name;
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
    g_free (dirName);
} /* gerbv_save_project_from_filename */


void 
gerbv_save_as_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
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
    gerbv_save_project_from_filename (gerbvProject, filename);
} /* gerbv_save_as_project_from_filename */

/* ------------------------------------------------------------------ */
int
gerbv_revert_file(gerbv_project_t *gerbvProject, int idx){
	int rv;
	rv = gerbv_open_image(gerbvProject, gerbvProject->file[idx]->fullPathname, idx, TRUE, NULL, 0, TRUE);
	return rv;
}

void 
gerbv_revert_all_files(gerbv_project_t *gerbvProject) 
{
    int idx;
    
    for (idx = 0; idx < gerbvProject->max_files; idx++) {
	if (gerbvProject->file[idx] && gerbvProject->file[idx]->fullPathname) {
	    gerbv_revert_file (gerbvProject, idx);
	    return;
	}
    }
} /* gerbv_revert_all_files */

void 
gerbv_unload_layer(gerbv_project_t *gerbvProject, int index) 
{
    gint i;
    
    free_gerb_image(gerbvProject->file[index]->image);  gerbvProject->file[index]->image = NULL;
    g_free(gerbvProject->file[index]->name);
    gerbvProject->file[index]->name = NULL;
    g_free(gerbvProject->file[index]);
    gerbvProject->file[index] = NULL;
    
    /* slide all later layers down to fill the empty slot */
    for (i=index; i<(gerbvProject->max_files-1); i++) {
	gerbvProject->file[i]=gerbvProject->file[i+1];
    }
    /* make sure the final spot is clear */
    gerbvProject->file[gerbvProject->max_files-1] = NULL;
    gerbvProject->last_loaded--;
} /* gerbv_unload_layer */

void 
gerbv_unload_all_layers (gerbv_project_t *gerbvProject) 
{
    int index;

    /* Must count down since gerbv_unload_layer collapses
     * layers down.  Otherwise, layers slide past the index */
    for (index = gerbvProject->max_files-1 ; index >= 0; index--) {
	if (gerbvProject->file[index] && gerbvProject->file[index]->name) {
	    gerbv_unload_layer (gerbvProject, index);
	}
    }
} /* gerbv_unload_all_layers */


void 
gerbv_change_layer_order(gerbv_project_t *gerbvProject, gint oldPosition, gint newPosition) 
{
    gerbv_fileinfo_t *temp_file;
    int index;
    
    temp_file = gerbvProject->file[oldPosition];
	
    if (oldPosition < newPosition){
	for (index = oldPosition; index < newPosition; index++) {
	    gerbvProject->file[index] = gerbvProject->file[index + 1];
	}
    } else {
	for (index = oldPosition; index > newPosition; index--) {
	    gerbvProject->file[index] = gerbvProject->file[index - 1];
	}
    }
    gerbvProject->file[newPosition] = temp_file;
} /* gerbv_change_layer_order */

gint
gerbv_add_parsed_image_to_project (gerbv_project_t *gerbvProject, gerb_image_t *parsed_image,
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
    //if (gerbvProject->dump_parsed_image)
	//gerb_image_dump(parsed_image);
    
    /*
     * If reload, just exchange the image. Else we have to allocate
     * a new memory before we define anything more.
     */
    if (reload) {
	free_gerb_image(gerbvProject->file[idx]->image);
	gerbvProject->file[idx]->image = parsed_image;
	return 0;
    } else {
	/* Load new file. */
	gerbvProject->file[idx] = (gerbv_fileinfo_t *)g_malloc(sizeof(gerbv_fileinfo_t));
	if (gerbvProject->file[idx] == NULL)
	    GERB_FATAL_ERROR("malloc gerbvProject->file[idx] failed\n");
	memset((void *)gerbvProject->file[idx], 0, sizeof(gerbv_fileinfo_t));
	gerbvProject->file[idx]->image = parsed_image;
    }
    
    /*
     * Store filename for eventual reload
     */
    gerbvProject->file[idx]->fullPathname = g_strdup (filename);
    gerbvProject->file[idx]->name = g_strdup (baseName);
    
    {
	r = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].red*257;
	g = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].green*257;
	b = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].blue*257;
    }

    GdkColor colorTemplate = {0, r, g, b};
    gerbvProject->file[idx]->color = colorTemplate;
    gerbvProject->file[idx]->alpha = defaultColors[idx % NUMBER_OF_DEFAULT_COLORS].alpha*257;
    gerbvProject->file[idx]->isVisible = TRUE;
    gerbvProject->file[idx]->transform = defaultTransformations[idx % NUMBER_OF_DEFAULT_TRANSFORMATIONS];
    return 1;
}

/* ------------------------------------------------------------------ */
int
gerbv_open_image(gerbv_project_t *gerbvProject, char *filename, int idx, int reload,
		HID_Attribute *fattr, int n_fattr, gboolean forceLoadFile)
{
    gerb_file_t *fd;
    gerb_image_t *parsed_image = NULL, *parsed_image2 = NULL;
    gint retv = -1;
    gboolean isPnpFile = FALSE, foundBinary;
    HID_Attribute *attr_list = NULL;
    int n_attr = 0;
    /* If we're reloading, we'll pass in our file format attribute list
     * since this is our hook for letting the user override the fileformat.
     */
    if (reload)
	{
	    /* We're reloading so use the attribute list in memory */
	    attr_list =  gerbvProject->file[idx]->image->info->attr_list;
	    n_attr =  gerbvProject->file[idx]->image->info->n_attr;
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
    if ((idx+1) >= gerbvProject->max_files) {
	gerbvProject->file = (gerbv_fileinfo_t **) realloc (gerbvProject->file, (gerbvProject->max_files + 2) * sizeof (gerbv_fileinfo_t *));

	if (gerbvProject->file == NULL)
	    {
		fprintf (stderr, "realloc failed\n");
		exit (1);
	    }
	gerbvProject->file[gerbvProject->max_files] = NULL;
	gerbvProject->file[gerbvProject->max_files+1] = NULL;
	gerbvProject->max_files += 2;
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
	if ((!foundBinary || forceLoadFile)) {
		/* figure out the directory path in case parse_gerb needs to
		 * load any include files */
		gchar *currentLoadDirectory = g_path_get_dirname (filename);
		parsed_image = parse_gerb(fd, currentLoadDirectory);
		g_free (currentLoadDirectory);
	}
    } else if(drill_file_p(fd, &foundBinary)) {
	dprintf("Found drill file\n");
	if ((!foundBinary || forceLoadFile))
	    parsed_image = parse_drillfile(fd, attr_list, n_attr, reload);
	
    } else if (pick_and_place_check_file_type(fd, &foundBinary)) {
	dprintf("Found pick-n-place file\n");
	if ((!foundBinary || forceLoadFile)) {
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
    	retv = gerbv_add_parsed_image_to_project (gerbvProject, parsed_image, filename, displayedName, idx, reload);
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
    	retv = gerbv_add_parsed_image_to_project (gerbvProject, parsed_image2, filename, displayedName, idx + 1, reload);
    	g_free (baseName);
    	g_free (displayedName);
    }

    return retv;
} /* open_image */

/* ------------------------------------------------------------------ */
void
gerbv_render_get_boundingbox(gerbv_project_t *gerbvProject, gerbv_render_size_t *boundingbox)
{
	double x1=HUGE_VAL,y1=HUGE_VAL, x2=-HUGE_VAL,y2=-HUGE_VAL;
	int i;
	gerb_image_info_t *info;

	for(i = 0; i < gerbvProject->max_files; i++) {
		if ((gerbvProject->file[i]) && (gerbvProject->file[i]->isVisible)){
			info = gerbvProject->file[i]->image->info;
			/* 
			* Find the biggest image and use as a size reference
			*/
#ifdef RENDER_USING_GDK
			x1 = MIN(x1, info->min_x + info->offsetA);
			y1 = MIN(y1, info->min_y + info->offsetB);
			x2 = MAX(x2, info->max_x + info->offsetA);
			y2 = MAX(y2, info->max_y + info->offsetB);
#else
			/* cairo info already has offset calculated into min/max */
			x1 = MIN(x1, info->min_x);
			y1 = MIN(y1, info->min_y);
			x2 = MAX(x2, info->max_x);
			y2 = MAX(y2, info->max_y);
#endif
		}
	}
	boundingbox->left    = x1;
	boundingbox->right   = x2;
	boundingbox->top    = y1;
	boundingbox->bottom = y2;
}

/* ------------------------------------------------------------------ */
void
gerbv_render_zoom_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo) {
	gerbv_render_size_t bb;
	double width, height;
	double x_scale, y_scale;

	/* Grab maximal width and height of all layers */
	gerbv_render_get_boundingbox(gerbvProject, &bb);
	width = bb.right - bb.left;
	height = bb.bottom - bb.top;
	/* add in a 5% buffer around the drawing */
	width *= 1.05;
	height *=1.05;

	/* if the values aren't sane (probably we have no models loaded), then
	   put in some defaults */
	if ((width < 0.01) && (height < 0.01)) {
		renderInfo->lowerLeftX = 0.0;
		renderInfo->lowerLeftY = 0.0;
		renderInfo->scaleFactorX = 200;
		renderInfo->scaleFactorY = 200;
		return;
	}
	/*
	* Calculate scale for both x axis and y axis
	*/
	x_scale = renderInfo->displayWidth / width;
	y_scale = renderInfo->displayHeight / height;
	/*
	* Take the scale that fits both directions with some extra checks
	*/
	renderInfo->scaleFactorX = MIN(x_scale, y_scale);
	renderInfo->scaleFactorY = renderInfo->scaleFactorX;
	if (renderInfo->scaleFactorX < 1){
	    renderInfo->scaleFactorX = 1;
	    renderInfo->scaleFactorY = 1;
	}
	renderInfo->lowerLeftX = ((bb.left + bb.right) / 2.0) -
		((double) renderInfo->displayWidth / 2.0 / renderInfo->scaleFactorX);
	renderInfo->lowerLeftY = ((bb.top + bb.bottom) / 2.0) -
		((double) renderInfo->displayHeight / 2.0 / renderInfo->scaleFactorY);
	return;
}

void
gerbv_render_translate_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo) {
	double x1=HUGE_VAL,y1=HUGE_VAL;
	int i;
	gerb_image_info_t *info;
	
	for(i = 0; i < gerbvProject->max_files; i++) {
		if ((gerbvProject->file[i]) && (gerbvProject->file[i]->isVisible)){
			info = gerbvProject->file[i]->image->info;

			/* cairo info already has offset calculated into min/max */
			x1 = MIN(x1, info->min_x);
			y1 = MIN(y1, info->min_y);
		}
	}
	renderInfo->lowerLeftX = x1;
	renderInfo->lowerLeftY = y1;
}

void
gerbv_render_to_pixmap_using_gdk (gerbv_project_t *gerbvProject, GdkPixmap *pixmap,
		gerbv_render_info_t *renderInfo, gerb_selection_info_t *selectionInfo,
		GdkColor *selectionColor){
	GdkGC *gc = gdk_gc_new(pixmap);
	GdkPixmap *colorStamp, *clipmask;
	int i;
	
	/* 
	 * Remove old pixmap, allocate a new one, draw the background.
	 */
	if (!gerbvProject->background.pixel)
	 	gdk_colormap_alloc_color(gdk_colormap_get_system(), &gerbvProject->background, FALSE, TRUE);
	gdk_gc_set_foreground(gc, &gerbvProject->background);
	gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, -1, -1);

	/*
	 * Allocate the pixmap and the clipmask (a one pixel pixmap)
	 */
	colorStamp = gdk_pixmap_new(pixmap, renderInfo->displayWidth,
						renderInfo->displayHeight, -1);
	clipmask = gdk_pixmap_new(NULL, renderInfo->displayWidth,
						renderInfo->displayHeight, 1);
							
	/* 
	* This now allows drawing several layers on top of each other.
	* Higher layer numbers have higher priority in the Z-order. 
	*/
	for(i = gerbvProject->max_files-1; i >= 0; i--) {
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible) {
			enum polarity_t polarity;

			if (gerbvProject->file[i]->transform.inverted) {
				if (gerbvProject->file[i]->image->info->polarity == POSITIVE)
					polarity = NEGATIVE;
				else
					polarity = POSITIVE;
			} else {
				polarity = gerbvProject->file[i]->image->info->polarity;
			}

			/*
			* Fill up image with all the foreground color. Excess pixels
			* will be removed by clipmask.
			*/
			if (!gerbvProject->file[i]->color.pixel)
	 			gdk_colormap_alloc_color(gdk_colormap_get_system(), &gerbvProject->file[i]->color, FALSE, TRUE);
			gdk_gc_set_foreground(gc, &gerbvProject->file[i]->color);
			
			/* switch back to regular draw function for the initial
			   bitmap clear */
			gdk_gc_set_function(gc, GDK_COPY);
			gdk_draw_rectangle(colorStamp, gc, TRUE, 0, 0, -1, -1);
			
			if (renderInfo->renderType == 0) {
				gdk_gc_set_function(gc, GDK_COPY);
			}
			else if (renderInfo->renderType == 1) {
				gdk_gc_set_function(gc, GDK_XOR);
			}
			/*
			* Translation is to get it inside the allocated pixmap,
			* which is not always centered perfectly for GTK/X.
			*/
			dprintf("  .... calling image2pixmap on image %d...\n", i);
			// Dirty scaling solution when using GDK; simply use scaling factor for x-axis, ignore y-axis
			draw_gdk_image_to_pixmap(&clipmask, gerbvProject->file[i]->image,
				renderInfo->scaleFactorX, -(renderInfo->lowerLeftX * renderInfo->scaleFactorX),
				(renderInfo->lowerLeftY * renderInfo->scaleFactorY) + renderInfo->displayHeight,
				polarity, DRAW_IMAGE, NULL);

			/* 
			* Set clipmask and draw the clipped out image onto the
			* screen pixmap. Afterwards we remove the clipmask, else
			* it will screw things up when run this loop again.
			*/
			gdk_gc_set_clip_mask(gc, clipmask);
			gdk_gc_set_clip_origin(gc, 0, 0);
			gdk_draw_drawable(pixmap, gc, colorStamp, 0, 0, 0, 0, -1, -1);
			gdk_gc_set_clip_mask(gc, NULL);
		}
	}
	/* render the selection group to the top of the output */
	if (selectionInfo->type != EMPTY) {
		if (!selectionColor->pixel)
	 		gdk_colormap_alloc_color(gdk_colormap_get_system(), selectionColor, FALSE, TRUE);
	 		
		gdk_gc_set_foreground(gc, selectionColor);
		gdk_gc_set_function(gc, GDK_COPY);
		gdk_draw_rectangle(colorStamp, gc, TRUE, 0, 0, -1, -1);
		
		/* for now, assume everything in the selection buffer is from one image */
		gerb_image_t *matchImage;
		int j;
		if (selectionInfo->selectedNodeArray->len > 0) {
			gerb_selection_item_t sItem = g_array_index (selectionInfo->selectedNodeArray,
					gerb_selection_item_t, 0);
			matchImage = (gerb_image_t *) sItem.image;	

			for(j = gerbvProject->max_files-1; j >= 0; j--) {
				if ((gerbvProject->file[j]) && (gerbvProject->file[j]->image == matchImage)) {
					draw_gdk_image_to_pixmap(&clipmask, gerbvProject->file[j]->image,
						renderInfo->scaleFactorX, -(renderInfo->lowerLeftX * renderInfo->scaleFactorX),
						(renderInfo->lowerLeftY * renderInfo->scaleFactorY) + renderInfo->displayHeight,
						POSITIVE, DRAW_SELECTIONS, selectionInfo);
				}
			}
			gdk_gc_set_clip_mask(gc, clipmask);
			gdk_gc_set_clip_origin(gc, 0, 0);
			gdk_draw_drawable(pixmap, gc, colorStamp, 0, 0, 0, 0, -1, -1);
			gdk_gc_set_clip_mask(gc, NULL);
		}
	}

	gdk_pixmap_unref(colorStamp);
	gdk_pixmap_unref(clipmask);
	gdk_gc_unref(gc);
}


void
gerbv_render_all_layers_to_cairo_target_for_vector_output (gerbv_project_t *gerbvProject,
		cairo_t *cr, gerbv_render_info_t *renderInfo) {
	int i;
	gerbv_render_cairo_set_scale_and_translation(cr, renderInfo);
	/* don't paint background for vector output, since it isn't needed */
	for(i = gerbvProject->max_files-1; i >= 0; i--) {
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible) {

		    gerbv_render_layer_to_cairo_target_without_transforming(cr, gerbvProject->file[i], renderInfo);
		}
	}
}

void
gerbv_render_all_layers_to_cairo_target (gerbv_project_t *gerbvProject, cairo_t *cr,
			gerbv_render_info_t *renderInfo) {
	int i;
	/* fill the background with the appropriate color */
	cairo_set_source_rgba (cr, (double) gerbvProject->background.red/G_MAXUINT16,
		(double) gerbvProject->background.green/G_MAXUINT16,
		(double) gerbvProject->background.blue/G_MAXUINT16, 1);
	cairo_paint (cr);
	for(i = gerbvProject->max_files-1; i >= 0; i--) {
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible) {
			cairo_push_group (cr);
			gerbv_render_layer_to_cairo_target (cr, gerbvProject->file[i], renderInfo);
			cairo_pop_group_to_source (cr);
			cairo_paint_with_alpha (cr, (double) gerbvProject->file[i]->alpha/G_MAXUINT16);
		}
	}
}

void
gerbv_render_layer_to_cairo_target (cairo_t *cr, gerbv_fileinfo_t *fileInfo,
						gerbv_render_info_t *renderInfo) {
	gerbv_render_cairo_set_scale_and_translation(cr, renderInfo);
	gerbv_render_layer_to_cairo_target_without_transforming(cr, fileInfo, renderInfo);
}

void
gerbv_render_cairo_set_scale_and_translation(cairo_t *cr, gerbv_render_info_t *renderInfo){
	gdouble translateX, translateY;
	
	translateX = (renderInfo->lowerLeftX * renderInfo->scaleFactorX);
	translateY = (renderInfo->lowerLeftY * renderInfo->scaleFactorY);
	
	/* renderTypes 0 and 1 use GDK rendering, so we shouldn't have made it
	   this far */
	if (renderInfo->renderType == 2) {
		cairo_set_tolerance (cr, 1.5);
		cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
	}
	else if (renderInfo->renderType == 3) {
		cairo_set_tolerance (cr, 1);
		/* disable ALL anti-aliasing for now due to the way cairo is rendering
		   ground planes from PCB output */
		cairo_set_antialias (cr, CAIRO_ANTIALIAS_DEFAULT);
	}

	/* translate the draw area before drawing.  We must translate the whole
	   drawing down an additional displayHeight to account for the negative
	   y flip done later */
	cairo_translate (cr, -translateX, translateY + renderInfo->displayHeight);
	/* scale the drawing by the specified scale factor (inverting y since
		cairo y axis points down) */
	cairo_scale (cr, renderInfo->scaleFactorX, -renderInfo->scaleFactorY);
}

void
gerbv_render_layer_to_cairo_target_without_transforming(cairo_t *cr, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo ) {
	cairo_set_source_rgba (cr, (double) fileInfo->color.red/G_MAXUINT16,
		(double) fileInfo->color.green/G_MAXUINT16,
		(double) fileInfo->color.blue/G_MAXUINT16, 1);
	
	draw_image_to_cairo_target (cr, fileInfo->image, fileInfo->transform.inverted,
		1.0/MAX(renderInfo->scaleFactorX, renderInfo->scaleFactorY), DRAW_IMAGE, NULL);
}


