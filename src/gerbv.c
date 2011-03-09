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
 
 
/** \file gerbv.c
    \brief This file contains high-level functions for the libgerbv library
    \ingroup libgerbv
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

#include "common.h"
#include "gerbv.h"
#include "gerber.h"
#include "drill.h"

#include "draw-gdk.h"
#include "draw.h"

#include "pick-and-place.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

#define NUMBER_OF_DEFAULT_COLORS 18
#define NUMBER_OF_DEFAULT_TRANSFORMATIONS 20

static int defaultColorIndex = 0;

/* ------------------------------------------------------------------ */
static gerbv_layer_color defaultColors[NUMBER_OF_DEFAULT_COLORS] = {
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

/* ------------------------------------------------------------------ */
static gerbv_user_transformation_t defaultTransformations[NUMBER_OF_DEFAULT_TRANSFORMATIONS] = {
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

/* ------------------------------------------------------------------ */
gerbv_project_t *
gerbv_create_project (void) {
	gerbv_project_t *returnProject= (gerbv_project_t *) g_new0(gerbv_project_t,1);
	
	/* default to using the current directory path for our starting guesses
		on future file loads */
	returnProject->path = g_get_current_dir ();
	/* Will be updated to 0 when first Gerber is loaded */
	returnProject->last_loaded = -1;
	returnProject->max_files = 1;
	returnProject->check_before_delete = TRUE;
	returnProject->file = g_new0 (gerbv_fileinfo_t *, returnProject->max_files);

	return returnProject;
}

/* ------------------------------------------------------------------ */
void
gerbv_destroy_project (gerbv_project_t *gerbvProject){
	int i;
	
	/* destroy all the files attached to the project */
	for(i = gerbvProject->last_loaded; i >= 0; i--) {
		if (gerbvProject->file[i])
			gerbv_destroy_fileinfo (gerbvProject->file[i]);
	}
	/* destroy strings */
	g_free (gerbvProject->path);
	g_free (gerbvProject->execname);
	g_free (gerbvProject->execpath);
	g_free (gerbvProject->project);
	/* destroy the fileinfo array */
	g_free (gerbvProject->file);
	g_free (gerbvProject);
}

/* ------------------------------------------------------------------ */
void
gerbv_destroy_fileinfo (gerbv_fileinfo_t *fileInfo){
	gerbv_destroy_image (fileInfo->image);
	g_free (fileInfo->fullPathname);
	g_free (fileInfo->name);
	if (fileInfo->privateRenderData) {
		cairo_surface_destroy ((cairo_surface_t *)
			fileInfo->privateRenderData);
	}			
}

/* ------------------------------------------------------------------ */
void 
gerbv_open_layer_from_filename(gerbv_project_t *gerbvProject, gchar *filename) 
{
  gint idx_loaded;
  dprintf("Opening filename = %s\n", (gchar *) filename);
  
  if (gerbv_open_image(gerbvProject, filename, ++gerbvProject->last_loaded, FALSE, NULL, 0, TRUE) == -1) {
    GERB_MESSAGE("could not read %s[%d]", (gchar *) filename,
		 gerbvProject->last_loaded);
    gerbvProject->last_loaded--;
  } else {
    idx_loaded = gerbvProject->last_loaded;
    gerbvProject->file[idx_loaded]->layer_dirty = FALSE;
    dprintf("     Successfully opened file!\n");	
  }
} /* gerbv_open_layer_from_filename */

/* ------------------------------------------------------------------ */
void 
gerbv_open_layer_from_filename_with_color(gerbv_project_t *gerbvProject, gchar *filename,
		guint16 red, guint16 green, guint16 blue, guint16 alpha)
{
  gint idx_loaded;
  dprintf("Opening filename = %s\n", (gchar *) filename);
  
  if (gerbv_open_image(gerbvProject, filename, ++gerbvProject->last_loaded, FALSE, NULL, 0, TRUE) == -1) {
    GERB_MESSAGE("could not read %s[%d]", (gchar *) filename,
		 gerbvProject->last_loaded);
    gerbvProject->last_loaded--;
  } else {
    idx_loaded = gerbvProject->last_loaded;
    gerbvProject->file[idx_loaded]->layer_dirty = FALSE;
    GdkColor colorTemplate = {0, red, green, blue};
    gerbvProject->file[idx_loaded]->color = colorTemplate;
    gerbvProject->file[idx_loaded]->alpha = alpha;
    dprintf("     Successfully opened file!\n");	
  }
} /* gerbv_open_layer_from_filename_with_color */  
    
/* ------------------------------------------------------------------ */
gboolean 
gerbv_save_layer_from_index(gerbv_project_t *gerbvProject, gint index, gchar *filename) 
{
    if (strcmp (gerbvProject->file[index]->image->info->type,"RS274-X (Gerber) File")==0) {
	gerbv_export_rs274x_file_from_image (filename, gerbvProject->file[index]->image,
		&gerbvProject->file[index]->transform);
    }
    else if (strcmp (gerbvProject->file[index]->image->info->type,"Excellon Drill File")==0) {
	gerbv_export_drill_file_from_image (filename, gerbvProject->file[index]->image,
		&gerbvProject->file[index]->transform);
    }
    else {
	return FALSE;
    }
    gerbvProject->file[index]->layer_dirty = FALSE;
    return TRUE;
} /* gerbv_save_project_from_filename */


/* ------------------------------------------------------------------ */
int
gerbv_revert_file(gerbv_project_t *gerbvProject, int idx){
  int rv;
  
  rv = gerbv_open_image(gerbvProject, gerbvProject->file[idx]->fullPathname, idx, TRUE, NULL, 0, TRUE);
  gerbvProject->file[idx]->layer_dirty = FALSE;
  return rv;
}

/* ------------------------------------------------------------------ */
void 
gerbv_revert_all_files(gerbv_project_t *gerbvProject) 
{
  int idx;
  
  for (idx = 0; idx <= gerbvProject->last_loaded; idx++) {
    if (gerbvProject->file[idx] && gerbvProject->file[idx]->fullPathname) {
      (void) gerbv_revert_file (gerbvProject, idx);
      gerbvProject->file[idx]->layer_dirty = FALSE;
    }
  }
} /* gerbv_revert_all_files */

/* ------------------------------------------------------------------ */
void 
gerbv_unload_layer(gerbv_project_t *gerbvProject, int index) 
{
    gint i;

    gerbv_destroy_fileinfo (gerbvProject->file[index]);
    
    /* slide all later layers down to fill the empty slot */
    for (i=index; i<(gerbvProject->last_loaded); i++) {
	gerbvProject->file[i]=gerbvProject->file[i+1];
    }
    /* make sure the final spot is clear */
    gerbvProject->file[gerbvProject->last_loaded] = NULL;
    gerbvProject->last_loaded--;
} /* gerbv_unload_layer */

/* ------------------------------------------------------------------ */
void 
gerbv_unload_all_layers (gerbv_project_t *gerbvProject) 
{
    int index;

    /* Must count down since gerbv_unload_layer collapses
     * layers down.  Otherwise, layers slide past the index */
    for (index = gerbvProject->last_loaded ; index >= 0; index--) {
	if (gerbvProject->file[index] && gerbvProject->file[index]->name) {
	    gerbv_unload_layer (gerbvProject, index);
	}
    }
} /* gerbv_unload_all_layers */


/* ------------------------------------------------------------------ */
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


/* ------------------------------------------------------------------ */
gint
gerbv_add_parsed_image_to_project (gerbv_project_t *gerbvProject, gerbv_image_t *parsed_image,
			gchar *filename, gchar *baseName, int idx, int reload){
    gerb_verify_error_t error = GERB_IMAGE_OK;
    int r, g, b; 
    
    dprintf("In open_image, now error check file....\n");
    error = gerbv_image_verify(parsed_image);

    if (error) {
	if (error & GERB_IMAGE_MISSING_NETLIST) {
	    GERB_COMPILE_ERROR("Missing netlist - aborting file read\n");
	    GERB_COMPILE_ERROR("\n");
	    gerbv_destroy_image(parsed_image);
	    return -1;
	}
	/* if the error was one of the following, try to open up the file anyways in case
	   the file is a poorly formatted RS-274X file */
	if (error & GERB_IMAGE_MISSING_FORMAT)
	    g_warning("Missing format in file...trying to load anyways\n");
	if (error & GERB_IMAGE_MISSING_APERTURES) {
	    g_warning("Missing apertures/drill sizes...trying to load anyways\n");
	    /* step through the file and check for aperture references. For each one found, create
	       a dummy aperture holder to visually draw something on the screen */
	    gerbv_image_create_dummy_apertures (parsed_image);
	}
	if (error & GERB_IMAGE_MISSING_INFO)
	    g_warning("Missing info...trying to load anyways\n");
    }
    
    /*
     * If reload, just exchange the image. Else we have to allocate
     * a new memory before we define anything more.
     */
    if (reload) {
	gerbv_destroy_image(gerbvProject->file[idx]->image);
	gerbvProject->file[idx]->image = parsed_image;
	return 0;
    } else {
	/* Load new file. */
	gerbvProject->file[idx] = (gerbv_fileinfo_t *) g_new0 (gerbv_fileinfo_t, 1);
	gerbvProject->file[idx]->image = parsed_image;
    }
    
    /*
     * Store filename for eventual reload
     */
    gerbvProject->file[idx]->fullPathname = g_strdup (filename);
    gerbvProject->file[idx]->name = g_strdup (baseName);
    
    
    r = defaultColors[defaultColorIndex % NUMBER_OF_DEFAULT_COLORS].red*257;
    g = defaultColors[defaultColorIndex % NUMBER_OF_DEFAULT_COLORS].green*257;
    b = defaultColors[defaultColorIndex % NUMBER_OF_DEFAULT_COLORS].blue*257;

    GdkColor colorTemplate = {0, r, g, b};
    gerbvProject->file[idx]->color = colorTemplate;
    gerbvProject->file[idx]->alpha = defaultColors[defaultColorIndex % NUMBER_OF_DEFAULT_COLORS].alpha*257;
    gerbvProject->file[idx]->isVisible = TRUE;
    gerbvProject->file[idx]->transform = defaultTransformations[defaultColorIndex % NUMBER_OF_DEFAULT_TRANSFORMATIONS];
    /* update the number of files if we need to */
    if (gerbvProject->last_loaded <= idx) {
	gerbvProject->last_loaded = idx;
    }
    defaultColorIndex++;
    return 1;
}

/* ------------------------------------------------------------------ */
int
gerbv_open_image(gerbv_project_t *gerbvProject, char *filename, int idx, int reload,
		gerbv_HID_Attribute *fattr, int n_fattr, gboolean forceLoadFile)
{
    gerb_file_t *fd;
    gerbv_image_t *parsed_image = NULL, *parsed_image2 = NULL;
    gint retv = -1;
    gboolean isPnpFile = FALSE, foundBinary;
    gerbv_HID_Attribute *attr_list = NULL;
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
	gerbvProject->file = g_renew (gerbv_fileinfo_t *,
			gerbvProject->file, gerbvProject->max_files + 2);

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

    /* Store filename info fd for further use */
    fd->filename = g_strdup(filename);
    
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
	dprintf("Most likely found a RS-274D file...trying to open anyways");
	g_warning("Most likely found a RS-274D file...trying to open anyways");
	if ((!foundBinary || forceLoadFile)) {
		/* figure out the directory path in case parse_gerb needs to
		 * load any include files */
		gchar *currentLoadDirectory = g_path_get_dirname (filename);
		parsed_image = parse_gerb(fd, currentLoadDirectory);
		g_free (currentLoadDirectory);
	}
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

    /* Set layer_dirty flag to FALSE */
    gerbvProject->file[idx]->layer_dirty = FALSE;

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

gerbv_image_t *
gerbv_create_rs274x_image_from_filename (gchar *filename){
	gerbv_image_t *returnImage;
	gerb_file_t *fd;
	
	fd = gerb_fopen(filename);
	if (fd == NULL) {
		GERB_MESSAGE("Trying to open %s:%s\n", filename, strerror(errno));
		return NULL;
	}
	gchar *currentLoadDirectory = g_path_get_dirname (filename);
	returnImage = parse_gerb(fd, currentLoadDirectory);
	g_free (currentLoadDirectory);
	gerb_fclose(fd);
	return returnImage;
}

/* ------------------------------------------------------------------ */
void
gerbv_render_get_boundingbox(gerbv_project_t *gerbvProject, gerbv_render_size_t *boundingbox)
{
	double x1=HUGE_VAL,y1=HUGE_VAL, x2=-HUGE_VAL,y2=-HUGE_VAL;
	int i;
	gerbv_image_info_t *info;
	gdouble minX, minY, maxX, maxY;
	
	for(i = 0; i <= gerbvProject->last_loaded; i++) {
		if ((gerbvProject->file[i]) && (gerbvProject->file[i]->isVisible)){
			
			
			info = gerbvProject->file[i]->image->info;
			/* 
			* Find the biggest image and use as a size reference
			*/
			/* cairo info already has offset calculated into min/max */
			
			minX = info->min_x;
			minY = info->min_y;
			maxX = info->max_x;
			maxY = info->max_y;
			
			if (!isnormal(minX)||!isnormal(minY)||!isnormal(maxX)||!isnormal(maxY)){
				continue;
			}
			/* transform the bounding box based on the user transform */
	
			cairo_matrix_t fullMatrix;
			cairo_matrix_init (&fullMatrix, 1, 0, 0, 1, 0, 0);

			cairo_matrix_translate (&fullMatrix, gerbvProject->file[i]->transform.translateX,
				gerbvProject->file[i]->transform.translateY);
			// don't use mirroring for the scale matrix
			gdouble scaleX = gerbvProject->file[i]->transform.scaleX;
			gdouble scaleY = gerbvProject->file[i]->transform.scaleY;
			if (gerbvProject->file[i]->transform.mirrorAroundX)
				scaleY *= -1;
			if (gerbvProject->file[i]->transform.mirrorAroundY)
				scaleX *= -1;
			cairo_matrix_scale (&fullMatrix, scaleX, scaleY);
			cairo_matrix_rotate (&fullMatrix, gerbvProject->file[i]->transform.rotation);	
			
			cairo_matrix_transform_point (&fullMatrix, &minX, &minY);
			cairo_matrix_transform_point (&fullMatrix, &maxX, &maxY);
			/* compare to both min and max, since a mirror transform may have made the "max"
			    number smaller than the "min" */
			x1 = MIN(x1, minX);
			x1 = MIN(x1, maxX);
			y1 = MIN(y1, minY);
			y1 = MIN(y1, maxY);
			x2 = MAX(x2, minX);
			x2 = MAX(x2, maxX);
			y2 = MAX(y2, minY);
			y2 = MAX(y2, maxY);
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
	if (!isnormal(width)||!isnormal(height)||((width < 0.01) && (height < 0.01))) {
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
}

/* ------------------------------------------------------------------ */
void
gerbv_render_translate_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo) {
	gerbv_render_size_t bb;

	/* Grab maximal width and height of all layers */
	gerbv_render_get_boundingbox(gerbvProject, &bb);
	renderInfo->lowerLeftX = ((bb.left + bb.right) / 2.0) -
		((double) renderInfo->displayWidth / 2.0 / renderInfo->scaleFactorX);
	renderInfo->lowerLeftY = ((bb.top + bb.bottom) / 2.0) -
		((double) renderInfo->displayHeight / 2.0 / renderInfo->scaleFactorY);
}

/* ------------------------------------------------------------------ */
void
gerbv_render_to_pixmap_using_gdk (gerbv_project_t *gerbvProject, GdkPixmap *pixmap,
		gerbv_render_info_t *renderInfo, gerbv_selection_info_t *selectionInfo,
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
	for(i = gerbvProject->last_loaded; i >= 0; i--) {
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible) {
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
			
			if (renderInfo->renderType == GERBV_RENDER_TYPE_GDK) {
				gdk_gc_set_function(gc, GDK_COPY);
			}
			else if (renderInfo->renderType == GERBV_RENDER_TYPE_GDK_XOR) {
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
				DRAW_IMAGE, NULL, renderInfo, gerbvProject->file[i]->transform);

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
	if ((selectionInfo) && (selectionInfo->type != GERBV_SELECTION_EMPTY)) {
		if (!selectionColor->pixel)
	 		gdk_colormap_alloc_color(gdk_colormap_get_system(), selectionColor, FALSE, TRUE);

		gdk_gc_set_foreground(gc, selectionColor);
		gdk_gc_set_function(gc, GDK_COPY);
		gdk_draw_rectangle(colorStamp, gc, TRUE, 0, 0, -1, -1);
		
		/* for now, assume everything in the selection buffer is from one image */
		gerbv_image_t *matchImage;
		int j;
		if (selectionInfo->selectedNodeArray->len > 0) {
			gerbv_selection_item_t sItem = g_array_index (selectionInfo->selectedNodeArray,
					gerbv_selection_item_t, 0);
			matchImage = (gerbv_image_t *) sItem.image;	

			for(j = gerbvProject->last_loaded; j >= 0; j--) {
				if ((gerbvProject->file[j]) && (gerbvProject->file[j]->image == matchImage)) {
					draw_gdk_image_to_pixmap(&clipmask, gerbvProject->file[j]->image,
						renderInfo->scaleFactorX, -(renderInfo->lowerLeftX * renderInfo->scaleFactorX),
						(renderInfo->lowerLeftY * renderInfo->scaleFactorY) + renderInfo->displayHeight,
						DRAW_SELECTIONS, selectionInfo,
						renderInfo, gerbvProject->file[j]->transform);
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

/* ------------------------------------------------------------------ */
void
gerbv_render_all_layers_to_cairo_target_for_vector_output (gerbv_project_t *gerbvProject,
		cairo_t *cr, gerbv_render_info_t *renderInfo) {
	int i;
	gerbv_render_cairo_set_scale_and_translation(cr, renderInfo);
	/* don't paint background for vector output, since it isn't needed */
	for(i = gerbvProject->last_loaded; i >= 0; i--) {
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible) {

		    gerbv_render_layer_to_cairo_target_without_transforming(cr, gerbvProject->file[i], renderInfo, FALSE);
		}
	}
}

/* ------------------------------------------------------------------ */
void
gerbv_render_all_layers_to_cairo_target (gerbv_project_t *gerbvProject, cairo_t *cr,
			gerbv_render_info_t *renderInfo) {
	int i;
	/* fill the background with the appropriate color */
	cairo_set_source_rgba (cr, (double) gerbvProject->background.red/G_MAXUINT16,
		(double) gerbvProject->background.green/G_MAXUINT16,
		(double) gerbvProject->background.blue/G_MAXUINT16, 1);
	cairo_paint (cr);
	for(i = gerbvProject->last_loaded; i >= 0; i--) {
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible) {
			cairo_push_group (cr);
			gerbv_render_layer_to_cairo_target (cr, gerbvProject->file[i], renderInfo);
			cairo_pop_group_to_source (cr);
			cairo_paint_with_alpha (cr, (double) gerbvProject->file[i]->alpha/G_MAXUINT16);
		}
	}
}

/* ------------------------------------------------------------------ */
void
gerbv_render_layer_to_cairo_target (cairo_t *cr, gerbv_fileinfo_t *fileInfo,
						gerbv_render_info_t *renderInfo) {
	gerbv_render_cairo_set_scale_and_translation(cr, renderInfo);
	gerbv_render_layer_to_cairo_target_without_transforming(cr, fileInfo, renderInfo, TRUE);
}

/* ------------------------------------------------------------------ */
void
gerbv_render_cairo_set_scale_and_translation(cairo_t *cr, gerbv_render_info_t *renderInfo){
	gdouble translateX, translateY;
	
	translateX = (renderInfo->lowerLeftX * renderInfo->scaleFactorX);
	translateY = (renderInfo->lowerLeftY * renderInfo->scaleFactorY);
	
	/* renderTypes 0 and 1 use GDK rendering, so we shouldn't have made it
	   this far */
	if (renderInfo->renderType == GERBV_RENDER_TYPE_CAIRO_NORMAL) {
		cairo_set_tolerance (cr, 1.0);
		cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
	}
	else if (renderInfo->renderType == GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY) {
		cairo_set_tolerance (cr, 0.1);
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

/* ------------------------------------------------------------------ */
void
gerbv_render_layer_to_cairo_target_without_transforming(cairo_t *cr, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo, gboolean pixelOutput) {
	cairo_set_source_rgba (cr, (double) fileInfo->color.red/G_MAXUINT16,
		(double) fileInfo->color.green/G_MAXUINT16,
		(double) fileInfo->color.blue/G_MAXUINT16, 1);
	
	/* translate, rotate, and modify the image based on the layer-specific transformation struct */
	cairo_save (cr);
	
	draw_image_to_cairo_target (cr, fileInfo->image,
		1.0/MAX(renderInfo->scaleFactorX, renderInfo->scaleFactorY), DRAW_IMAGE, NULL,
		renderInfo, TRUE, fileInfo->transform, pixelOutput);
	cairo_restore (cr);
}

void
gerbv_attribute_destroy_HID_attribute (gerbv_HID_Attribute *attributeList, int n_attr)
{
  int i;

  /* free the string attributes */
  for (i = 0 ; i < n_attr ; i++) {
    if ( (attributeList[i].type == HID_String ||
	  attributeList[i].type == HID_Label) &&
	attributeList[i].default_val.str_value != NULL) {
      free (attributeList[i].default_val.str_value);
    }
  }

  /* and free the attribute list */
  if (attributeList != NULL) {
    free (attributeList);
  }
}


/* allocate memory and make a copy of an attribute list */
gerbv_HID_Attribute *
gerbv_attribute_dup (gerbv_HID_Attribute *attributeList, int n_attr)
{
  gerbv_HID_Attribute *nl;
  int i;

  nl = (gerbv_HID_Attribute *) malloc (n_attr * sizeof (gerbv_HID_Attribute));
  if (nl == NULL) {
    fprintf (stderr, "%s():  malloc failed\n", __FUNCTION__);
    exit (1);
  }

  /* copy the attribute list being sure to strdup the strings */
  for (i = 0 ; i < n_attr ; i++) {

    if (attributeList[i].type == HID_String ||
	attributeList[i].type == HID_Label) {

      if (attributeList[i].default_val.str_value != NULL) {
	nl[i].default_val.str_value = strdup (attributeList[i].default_val.str_value);
      } else {
	nl[i].default_val.str_value = NULL;
      }

    } else {
      nl[i] = attributeList[i];
    }
  }

  return nl;
}


