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

#include "gerbv.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#ifdef HAVE_LIBGEN_H
# include <libgen.h> /* dirname */
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

#include <pango/pango.h>

#include "common.h"
#include "gerber.h"
#include "drill.h"
#include "selection.h"

#include "draw-gdk.h"
#include "draw.h"

#include "pick-and-place.h"
#include "ipcd356a.h"
#include "x2attr.h"
#include "search.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

/** Return string name of gerbv_aperture_type_t aperture type. */
const char *gerbv_aperture_type_name(gerbv_aperture_type_t type)
{
	/* These are the names of the valid apertures.  Please keep this in
	 * sync with the gerbv_aperture_type_t enum defined in gerbv.h */
	const char *names[] = {
		N_("none"),
		N_("circle"),
		N_("rectangle"),
		N_("oval"),		/* ovular (obround) aperture */
		N_("polygon"),		/* polygon aperture */
		N_("macro"),		/* RS274X macro */
		N_("circle macro"),	/* RS274X circle macro */
		N_("outline macro"),	/* RS274X outline macro */
		N_("polygon macro"),	/* RS274X polygon macro */
		N_("moire macro"),	/* RS274X moire macro */
		N_("thermal macro"),	/* RS274X thermal macro */
		N_("line20 macro"),	/* RS274X line (code 20) macro */
		N_("line21 macro"),	/* RS274X line (code 21) macro */
		N_("line22 macro"),	/* RS274X line (code 22) macro */
	};

	if (type >=0 && type < sizeof(names)/sizeof(names[0]))
		return names[type];

	return N_("<undefined>");
}

/** Return string name of gerbv_interpolation_t interpolation. */
const char *gerbv_interpolation_name(gerbv_interpolation_t interp)
{
	/* These are the names of the interpolation method.  Please keep this
	 * in sync with the gerbv_interpolation_t enum defined in gerbv.h */
	const char *names[] = {
		N_("1X linear"),
		N_("10X linear"),
		N_("0.1X linear"),
		N_("0.01X linear"),
		N_("CW circular"),
		N_("CCW circular"),
		N_("poly area start"),
		N_("poly area stop"),
		N_("deleted"),
	};

	if (interp >= 0 && interp < sizeof(names)/sizeof(names[0]))
		return names[interp];

	return N_("<undefined>");
}

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
gerbv_destroy_project (gerbv_project_t *gerbvProject)
{
	int i;
	
	/* destroy all the files attached to the project */
	for(i = gerbvProject->last_loaded; i >= 0; i--) {
		if (gerbvProject->file[i]) {
			gerbv_destroy_fileinfo (gerbvProject->file[i]);
			g_free(gerbvProject->file[i]);
		}
	}
	x2attr_destroy(gerbvProject->attrs);
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
gerbv_open_layer_from_filename(gerbv_project_t *gerbvProject, gchar const* filename)
{
  gint idx_loaded;
  dprintf("Opening filename = %s\n", filename);
  
  if (gerbv_open_image(gerbvProject, filename, ++gerbvProject->last_loaded, FALSE, NULL, 0, TRUE) == -1) {
    GERB_COMPILE_WARNING(_("Could not read \"%s\" (loaded %d)"),
		    filename, gerbvProject->last_loaded);
    gerbvProject->last_loaded--;
  } else {
    idx_loaded = gerbvProject->last_loaded;
    gerbvProject->file[idx_loaded]->layer_dirty = FALSE;
    dprintf("     Successfully opened file!\n");	
  }
} /* gerbv_open_layer_from_filename */

/* ------------------------------------------------------------------ */
void 
gerbv_open_layer_from_filename_with_color(gerbv_project_t *gerbvProject, gchar const* filename,
		guint16 red, guint16 green, guint16 blue, guint16 alpha)
{
  gint idx_loaded;
  dprintf("Opening filename = %s\n", filename);
  
  if (gerbv_open_image(gerbvProject, filename, ++gerbvProject->last_loaded, FALSE, NULL, 0, TRUE) == -1) {
    GERB_COMPILE_WARNING(_("Could not read \"%s\" (loaded %d)"),
		    filename, gerbvProject->last_loaded);
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
	gerbv_fileinfo_t *file = gerbvProject->file[index];
	gerbv_user_transformation_t *trans = &file->transform;

	switch (file->image->layertype) {
	// IPC is parsed and converted to an RS274-X2 equivalent, thus we can export as normal.
	// The user should probably select a GRB extension etc.  It will be read back in as
	// an RS274-X2 with the standard file attribute .FileFunction set to "Other,IPC-D-356A".
	case GERBV_LAYERTYPE_IPCD356A:
	case GERBV_LAYERTYPE_RS274X:
		if (trans) {
			/* NOTE: mirrored file is not yet supported */
			if (trans->mirrorAroundX || trans->mirrorAroundY) {
				GERB_COMPILE_ERROR(_("Exporting mirrored file "
							"is not supported!"));
				return FALSE;
			}

			/* NOTE: inverted file is not yet supported */
			if (trans->inverted) {
				GERB_COMPILE_ERROR(_("Exporting inverted file "
							"is not supported!"));
				return FALSE;
			}
		}

		gerbv_export_rs274x_file_from_image (filename,
				file->image, trans);
		break;

	case GERBV_LAYERTYPE_DRILL:
		if (trans) {
			/* NOTE: inverted file is not yet supported */
			if (trans->inverted) {
				GERB_COMPILE_ERROR(_("Exporting inverted file "
							"is not supported!"));
				return FALSE;
			}
		}

		gerbv_export_drill_file_from_image (filename,
				file->image, trans);
		break;
	default:
		return FALSE;
	}

	file->layer_dirty = FALSE;

	return TRUE;
}


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
			gchar const* filename, gchar const* baseName, int idx, int reload){
    gerb_verify_error_t error = GERB_IMAGE_OK;
    int r, g, b; 
    
    dprintf("In open_image, now error check file....\n");
    error = gerbv_image_verify(parsed_image);

    if (error) {
	if (error & GERB_IMAGE_MISSING_NETLIST) {
	    GERB_COMPILE_ERROR(_("Missing netlist - aborting file read"));
	    gerbv_destroy_image(parsed_image);
	    return -1;
	}
	/* if the error was one of the following, try to open up the file anyways in case
	   the file is a poorly formatted RS-274X file */
	if (error & GERB_IMAGE_MISSING_FORMAT)
	    g_warning(_("Missing format in file...trying to load anyways\n"));
	if (error & GERB_IMAGE_MISSING_APERTURES) {
	    g_warning(_("Missing apertures/drill sizes...trying to load anyways\n"));
	    /* step through the file and check for aperture references. For each one found, create
	       a dummy aperture holder to visually draw something on the screen */
	    gerbv_image_create_dummy_apertures (parsed_image);
	}
	if (error & GERB_IMAGE_MISSING_INFO)
	    g_warning(_("Missing info...trying to load anyways\n"));
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
gerbv_open_image(gerbv_project_t *gerbvProject, gchar const* filename, int idx, int reload,
		gerbv_HID_Attribute *fattr, int n_fattr, gboolean forceLoadFile)
{
    gerb_file_t *fd;
    gerbv_image_t *parsed_image = NULL, *parsed_image2 = NULL;
    gint retv = -1;
    gboolean isPnpFile = FALSE, foundBinary;
    gerbv_HID_Attribute *attr_list = NULL;
    int n_attr = 0;
    int instance = 0, i, maxlayer = 0, thislayer;
    gboolean is_signal = FALSE;
    unsigned long layer_bitmap = 0;
    const char * file_functions;
    char * ff = NULL, * ffp, * polarity = NULL, * plane = NULL, * other = NULL;
    char attrbuf[100];
    gerbv_layertype_t layertype;
    file_type_t ftype, ftype_best;
    file_type_t ftypes[] = { FILE_TYPE_IPCD356A };
    file_sniffer_likely_line_t sniffers[] = { ipcd356a_likely_line };
    
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

    // For now, use the generalized file sniffer just for IPC-D-356A.
    //TODO: implement sniffer funcs for other types, to unify handling.
    ftype = file_sniffer_guess_file_type(filename, sizeof(ftypes)/sizeof(ftypes[0]),
                        ftypes, sniffers, &ftype_best);    
    if (ftype == FILE_TYPE_CANNOT_OPEN)
        return -1;      // Error already posted.
    
    dprintf("In open_image, about to try opening filename = %s\n", filename);
    
    fd = gerb_fopen(filename);
    if (fd == NULL) {
	GERB_COMPILE_ERROR(_("Trying to open \"%s\": %s"),
			filename, strerror(errno));
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

    // Get the filetype and (target) layertype.
    if (ftype == FILE_TYPE_IPCD356A) {
        layertype = GERBV_LAYERTYPE_IPCD356A;
    }
    else if (gerber_is_rs274x_p(fd, &foundBinary)) {
        ftype = FILE_TYPE_RS274X;
        layertype = GERBV_LAYERTYPE_RS274X;
    }
    else if (drill_file_p(fd, &foundBinary)) {
        ftype = FILE_TYPE_EXCELLON;
        layertype = GERBV_LAYERTYPE_DRILL;
    }
    else if (pick_and_place_check_file_type(fd, &foundBinary)) {
        ftype = FILE_TYPE_CSV;
        //TODO: inappropriate use of layertype to distinguish PnP sides.
        layertype = GERBV_LAYERTYPE_PICKANDPLACE_TOP;
    }
    else if (gerber_is_rs274d_p(fd)) {
        ftype = FILE_TYPE_RS274D;
        layertype = GERBV_LAYERTYPE_RS274X;
    }
        
    
    // Count how many of this type already loaded.  Used to index CDL args that apply to 
    // this target layertype.  'instance' will be 0-based instance for the new file.
    for (i = 0; i <= gerbvProject->last_loaded; ++i) {
        gerbv_fileinfo_t * f = gerbvProject->file[i];
        
        if (f 
            && f->image
            && (f->image->layertype == layertype
                || (ftype == FILE_TYPE_CSV && f->image->layertype == GERBV_LAYERTYPE_PICKANDPLACE_BOT)
               )
            )
            ++instance;
    }
    
    // If target layertype is RS274-X, then parse any relevant project options that assign X2 attributes
    // etc.  The "layers" arg attribute is particularly useful for assigning a function to each
    // layer.  It also indicates how many layers to expect.  E.g. if --layers=1,2,3,4 then we know it's a 4-layer
    // board and the bottom layer is #4.
    
    if (layertype == GERBV_LAYERTYPE_RS274X) {
        /* Get *all* layers defined, so can map 'b' to max (bottom) layer.  't' is always 1.
           Copper layer number is always first char: digit(2) 1..9 or t or b.  (Others are not copper).
           
           Syntax is (no spaces, case insensitive):
             <ff>[,<ff>]...   file functions, assigned 1:1 to each RS274-X/D file listed on command line.
           where <ff> is
             <layer><process><qualifier>
           where <layer> is
             t          top
             1          top
             2..99      inner or possibly bottom layer
             b          bottom
               (following types cannot have process or qualifier except for polarity)
             o          board outline/profile
             f          fabrication drawing
             a          array/panel drawing
             d          drillmap
             x<type>    "other" of type <type>
           <process> is
             <empty>    copper layer or not applicable
               (following process must be on top or bottom else no file function assigned)
             m          soldermask
             l          legend/silk
             p          paste
             g          glue
             a          assembly drawing
           <qualifier> is
             <empty>    no qualifier
             =          plane layer
             .          signal layer
             =.         mixed plane+signal
             -          negative polarity
             +          positive polarity
           
           Note regarding bottom layer number:
           - minimum layer number for b is 2.
           - if 'b' (without process) does not appear, then it is assumed to be the greatest layer number
             that was seen.
           - if 'b' (without process) appears, then it is assigned the greatest layer number seen
             elsewhere, plus 1.
           For example:
             t,b        - b=2.
             t,bm       - b=2.  No bottom copper, but the soldermask is for layer 2.
             1,2        - b=2.
             1,2,b      - b=3.
             t          - b=2.  But it's not used.
             1,3,b,2    - b=4.  Ordering doesn't matter.
             t,2,3,bm   - b=3.  bm (soldermask) is a process (m) hence b=3 not 4.
             
           Typical commandline examples:
           . 2-layer board:
           gerbv --layers=tl,tm,t,b,bm,bl  tsilk.grb tmask.grb tcopper.grb bcopper.grb bmask.grb bsilk.grb
           . 4-layer board:
           gerbv --layers=1.,2-=,3-=,4.  tcopper.grb power.grb ground.grb bcopper.grb
              Top and bottom layers are "signal" (.), inner layers are "plane" (=) and in negative polarity (-).
           
        */ 
        gboolean haveb = FALSE;
        gboolean havebprocess = FALSE;
        file_functions = x2attr_get_project_attr_or_default(gerbvProject, "layers", "");
        maxlayer = 1;
        for (i = 0; ; ++i) {
                g_free(ff);
                ff = x2attr_get_field_or_default(i, file_functions, "!");
                if (!strcmp(ff, "!"))
                        break;
                ffp = ff;
                switch (tolower(*ffp)) {
                case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
                        thislayer = *ffp-'0';
                        while (isdigit(*++ffp))
                                thislayer = thislayer*10 + (*ffp - '0');
                        maxlayer = MAX(thislayer, maxlayer);
                        break;
                case 'b':
                        if (strlen(ffp) == 1 || !isalpha(*ffp))
                                haveb = TRUE;
                        else
                                havebprocess = TRUE;
                        break;
                default:
                        break;
                }
        }  
        if (haveb)
                // Got a 'b' by itself, so e.g. if only t,b then maxlayer will be 2.
                // If 2,3,t,b then maxlayer=4.  But 'bm' etc. do not count.
                ++maxlayer;
        else if (havebprocess && maxlayer==1)
                // Got e.g. bm but everything with explicit layer was top layer.
                // Let's assume then that it's actually 2 layers, so that t and
                // b are distinct!
                maxlayer = 2;
        g_free(ff);
        // Now that we know what 'b' layer means, go ahead and parse this instance.
        ff = x2attr_get_field_or_default(instance, file_functions, "");
        attrbuf[0] = 0;
        ffp = ff;
        switch (tolower(*ffp++)) {
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
                thislayer = ffp[-1]-'0';
                while (isdigit(*ffp))
                        thislayer = thislayer*10 + (*ffp++ - '0');
                break;
        case 'b':
                thislayer = maxlayer;
                break;
        case 't':
                thislayer = 1;
                break;
        default:
                thislayer = 0;
                other = ffp-1;
                break;
        }
        while (thislayer && *ffp) {
                switch (tolower(*ffp++)) {
                case 'm':
                        if (thislayer == 1)
                                strcpy(attrbuf,"Soldermask,Top");
                        else if (thislayer == maxlayer)
                                strcpy(attrbuf,"Soldermask,Bot");
                        break;
                case 'l':
                        if (thislayer == 1)
                                strcpy(attrbuf,"Legend,Top");
                        else if (thislayer == maxlayer)
                                strcpy(attrbuf,"Legend,Bot");
                        break;
                case 'p':
                        if (thislayer == 1)
                                strcpy(attrbuf,"Paste,Top");
                        else if (thislayer == maxlayer)
                                strcpy(attrbuf,"Paste,Bot");
                        break;
                case 'g':
                        if (thislayer == 1)
                                strcpy(attrbuf,"Glue,Top");
                        else if (thislayer == maxlayer)
                                strcpy(attrbuf,"Glue,Bot");
                        break;
                case 'a':
                        if (thislayer == 1)
                                strcpy(attrbuf,"AssemblyDrawing,Top");
                        else if (thislayer == maxlayer)
                                strcpy(attrbuf,"AssemblyDrawing,Bot");
                        break;
                case '-': case '+':
                        polarity = ffp-1;
                        break;
                case '=': case '.':     // plane or signal (=. or .= means mixed)
                        if (plane && *plane != ffp[-1])
                                plane = "m";
                        else
                                plane = ffp-1;
                        break;
                default:
                        break;
                }
        }
        if (other) {
                // Not a layer-specific file function.  Set something appropriate:
                switch (tolower(*other)) {
                case 'o':
                        strcpy(attrbuf, "Profile,NP");   // board outline, non-plated presumably.
                        break;
                case 'f':
                        strcpy(attrbuf, "FabricationDrawing");
                        break;
                case 'a':
                        strcpy(attrbuf, "ArrayDrawing");
                        break;
                case 'd':
                        strcpy(attrbuf, "Drillmap");
                        break;
                case 'x':
                        snprintf(attrbuf, sizeof(attrbuf), "Other,%s", other+1);
                        break;
                default:
                        break;
                }
        }
        if (thislayer && !attrbuf[0]) {
                // Must be a copper layer.
                is_signal = !plane || *plane!='=';
                snprintf(attrbuf, sizeof(attrbuf), "Copper,L%d,%s%s",
                                thislayer,
                                thislayer == 1 ? "Top" :
                                thislayer == maxlayer ? "Bot" :
                                "Inr",
                                plane && *plane=='=' ? ",Plane" : 
                                plane && *plane=='.' ? ",Signal" : 
                                plane && *plane=='m' ? ",Mixed" : 
                                ""
                                );
        }
        // Now attrbuf may have a file function,
        // polarity may point to '+' or '-',
        // These are added to image (file) attributes after reading in.
    }
    
    if (ftype == FILE_TYPE_IPCD356A) {
        // Default to getting the non-copper (0) and top (1) layers.  Default to not getting tracks.
        char * layers, * lp;
        char * tracks;
        char * label;
        layers = x2attr_get_field_or_last(instance, x2attr_get_project_attr_or_default(gerbvProject, "ipcd356a-layers", "01"));
        tracks = x2attr_get_field_or_last(instance, x2attr_get_project_attr_or_default(gerbvProject, "ipcd356a-tracks", "no"));
        label = x2attr_get_field_or_last(instance, x2attr_get_project_attr_or_default(gerbvProject, "ipcd356a-labels", "n"));
        lp = layers;
        while (*layers) {
                layer_bitmap |= 1uL<<(*layers - '0');
                ++layers;
        }
	printf("Found IPC-D-356A file, instance %d.  Layers=0x%lX, tracks=%s, label=%s\n", instance, layer_bitmap, tracks, label);
	
        parsed_image = ipcd356a_parse(fd, layer_bitmap, tracks[0]=='y', label);
        g_free(label);
        g_free(tracks);
        g_free(lp);
        
    }
    else if (ftype == FILE_TYPE_RS274X) {
	dprintf("Found RS-274X file\n");
	if (!foundBinary || forceLoadFile) {
		/* figure out the directory path in case parse_gerb needs to
		 * load any include files */
		gchar *currentLoadDirectory = g_path_get_dirname (filename);
		parsed_image = parse_gerb(fd, currentLoadDirectory);
		g_free (currentLoadDirectory);
	}
    } else if (ftype == FILE_TYPE_EXCELLON) {
	dprintf("Found drill file\n");
	if (!foundBinary || forceLoadFile)
	    parsed_image = parse_drillfile(fd, attr_list, n_attr, reload);
	
    } else if (ftype == FILE_TYPE_CSV) {
	dprintf("Found pick-n-place file\n");
	if (!foundBinary || forceLoadFile) {
		if (!reload) {
			pick_and_place_parse_file_to_images(fd, &parsed_image, &parsed_image2);
		} else {
			switch (gerbvProject->file[idx]->image->layertype) {
			case GERBV_LAYERTYPE_PICKANDPLACE_TOP:
				/* Non NULL pointer is used as "not to reload" mark */
				parsed_image2 = (void *)!NULL;
				pick_and_place_parse_file_to_images(fd, &parsed_image, &parsed_image2);
				parsed_image2 = NULL;
				break;
			case GERBV_LAYERTYPE_PICKANDPLACE_BOT:
				/* Non NULL pointer is used as "not to reload" mark */
				parsed_image2 = (void *)!NULL;
				pick_and_place_parse_file_to_images(fd, &parsed_image2, &parsed_image);
				parsed_image2 = NULL;
				break;
			default:
				GERB_COMPILE_ERROR(_("%s: unknown pick-and-place board side to reload"), filename);
			}
		}
			
		isPnpFile = TRUE;
	}
    } else if (ftype == FILE_TYPE_RS274D) {
	gchar *str = g_strdup_printf(_("Most likely found a RS-274D file "
			"\"%s\" ... trying to open anyways\n"), filename);
	dprintf("%s", str);
	g_warning("%s", str);
	g_free (str);

	if (!foundBinary || forceLoadFile) {
		/* figure out the directory path in case parse_gerb needs to
		 * load any include files */
		gchar *currentLoadDirectory = g_path_get_dirname (filename);
		parsed_image = parse_gerb(fd, currentLoadDirectory);
		g_free (currentLoadDirectory);
	}
    } else {
	/* This is not a known file */
	dprintf("Unknown filetype");
	GERB_COMPILE_ERROR(_("%s: Unknown file type."), filename);
	parsed_image = NULL;
    }
    
    
    if (parsed_image) {
        // Assign image (file) attributes if applicable
        // Standard attributes...
        if (attrbuf[0])
                x2attr_set_image_attr(parsed_image, ".FileFunction", attrbuf);
        if (polarity)
                x2attr_set_image_attr(parsed_image, ".FilePolarity", *polarity == '-' ? "Negative" : "Positive");
        // Non-standard, for our convenience...
        if (ftype == FILE_TYPE_IPCD356A) {
                sprintf(attrbuf, "0x%lX", layer_bitmap);
                x2attr_set_image_attr(parsed_image, "LayerSet", attrbuf);
        }
        else if (ftype == FILE_TYPE_RS274X || ftype == FILE_TYPE_RS274D) {
                sprintf(attrbuf, "%d", thislayer);
                x2attr_set_image_attr(parsed_image, "LayerNum", attrbuf);
                if (maxlayer > 0) {
                        sprintf(attrbuf, "%d", maxlayer);
                        x2attr_set_image_attr(parsed_image, "LayerMax", attrbuf);
                }
                if (is_signal)
                        x2attr_set_image_attr(parsed_image, "LayerIsSignal", "");
        }
        
    }
    
    g_free(ff);
    
    g_free(fd->filename);
    gerb_fclose(fd);
    if (parsed_image == NULL) {
	return -1;
    }
    
    if (parsed_image) {
	/* strip the filename to the base */
	gchar *baseName = g_path_get_basename (filename);
	gchar *displayedName;
	if (isPnpFile)
		displayedName = g_strconcat (baseName, _(" (top)"), NULL);
	else
		displayedName = g_strdup (baseName);
    	retv = gerbv_add_parsed_image_to_project (gerbvProject, parsed_image, filename, displayedName, idx, reload);
    	g_free (baseName);
    	g_free (displayedName);

        // Automatically make the IPC file invisible if annotation being requested.
        if (ftype == FILE_TYPE_IPCD356A
            && tolower(x2attr_get_project_attr_or_default(gerbvProject, "annotate", "y")[0]) == 'y')
                gerbvProject->file[idx]->isVisible = FALSE;
    }

    /* Set layer_dirty flag to FALSE */
    gerbvProject->file[idx]->layer_dirty = FALSE;

    /* for PNP place files, we may need to add a second image for the other
       board side */
    if (parsed_image2) {
      /* strip the filename to the base */
	gchar *baseName = g_path_get_basename (filename);
	gchar *displayedName;
	displayedName = g_strconcat (baseName, _(" (bottom)"), NULL);
    	retv = gerbv_add_parsed_image_to_project (gerbvProject, parsed_image2, filename, displayedName, idx + 1, reload);
    	g_free (baseName);
    	g_free (displayedName);
    }

    return retv;
} /* open_image */

gerbv_image_t *
gerbv_create_rs274x_image_from_filename (gchar const* filename){
	gerbv_image_t *returnImage;
	gerb_file_t *fd;
	
	fd = gerb_fopen(filename);
	if (fd == NULL) {
		GERB_COMPILE_ERROR(_("Trying to open \"%s\": %s"),
				filename, strerror(errno));
		return NULL;
	}
	gchar *currentLoadDirectory = g_path_get_dirname (filename);
	returnImage = parse_gerb(fd, currentLoadDirectory);
	g_free (currentLoadDirectory);
	gerb_fclose(fd);
	return returnImage;
}

static inline int isnormal_or_zero(double x)
{
	int cl = fpclassify(x);
	return cl == FP_NORMAL || cl == FP_ZERO;
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
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible){
			
			
			info = gerbvProject->file[i]->image->info;
			/* 
			* Find the biggest image and use as a size reference
			*/
			/* cairo info already has offset calculated into min/max */
			
			minX = info->min_x;
			minY = info->min_y;
			maxX = info->max_x;
			maxY = info->max_y;

			if (!isnormal_or_zero(minX) || !isnormal_or_zero(minY)
			 || !isnormal_or_zero(maxX) || !isnormal_or_zero(maxY)) {
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
gerbv_render_zoom_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo)
{
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
		renderInfo->scaleFactorY = renderInfo->scaleFactorX;
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
	renderInfo->scaleFactorX = MIN((gdouble)GERBV_SCALE_MAX,
			renderInfo->scaleFactorX);
	renderInfo->scaleFactorX = MAX((gdouble)GERBV_SCALE_MIN,
			renderInfo->scaleFactorX);
	renderInfo->scaleFactorY = renderInfo->scaleFactorX;
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

	/* Render the selection group to the top of the output */
	if (selectionInfo && selectionInfo->selectedNodeArray
	&& (selection_length (selectionInfo) != 0)) {
		if (!selectionColor->pixel)
	 		gdk_colormap_alloc_color(gdk_colormap_get_system(), selectionColor, FALSE, TRUE);

		gdk_gc_set_foreground(gc, selectionColor);
		gdk_gc_set_function(gc, GDK_COPY);
		gdk_draw_rectangle(colorStamp, gc, TRUE, 0, 0, -1, -1);

		gerbv_selection_item_t sItem;
		gerbv_fileinfo_t *file;
		int j;
		guint k;

		for (j = gerbvProject->last_loaded; j >= 0; j--) {
			file = gerbvProject->file[j]; 
			if (!file || (!gerbvProject->show_invisible_selection && !file->isVisible))
				continue;

			for (k = 0; k < selection_length (selectionInfo); k++) {
				sItem = selection_get_item_by_index (selectionInfo, k);

				if (file->image != sItem.image)
					continue;

				/* Have selected image(s) on this layer, draw it */
				draw_gdk_image_to_pixmap(&clipmask, file->image,
					renderInfo->scaleFactorX,
					-(renderInfo->lowerLeftX * renderInfo->scaleFactorX),
					(renderInfo->lowerLeftY * renderInfo->scaleFactorY) + renderInfo->displayHeight,
					DRAW_SELECTIONS, selectionInfo,
					renderInfo, file->transform);

				gdk_gc_set_clip_mask(gc, clipmask);
				gdk_gc_set_clip_origin(gc, 0, 0);
				gdk_draw_drawable(pixmap, gc, colorStamp, 0, 0, 0, 0, -1, -1);
				gdk_gc_set_clip_mask(gc, NULL);

				break;
			}
		}
	}

	gdk_pixmap_unref(colorStamp);
	gdk_pixmap_unref(clipmask);
	gdk_gc_unref(gc);
}

/* ------------------------------------------------------------------ */
void
gerbv_render_all_layers_to_cairo_target_for_vector_output (
		gerbv_project_t *gerbvProject, cairo_t *cr,
		gerbv_render_info_t *renderInfo)
{
	GdkColor *bg = &gerbvProject->background;
	int i;
	double r, g, b;
        printf("RALCTVO\n");
        
	gerbv_render_cairo_set_scale_and_translation (cr, renderInfo);

	/* Fill the background with the appropriate not white and not black
	 * color for backward culpability. */ 
	if ((bg->red != 0xffff || bg->green != 0xffff || bg->blue != 0xffff)
	 && (bg->red != 0x0000 || bg->green != 0x0000 || bg->blue != 0x0000)) {
		r = (double) bg->red/G_MAXUINT16;
		g = (double) bg->green/G_MAXUINT16;
		b = (double) bg->blue/G_MAXUINT16;
		cairo_set_source_rgba (cr, r, g, b, 1);
		cairo_paint (cr);

		/* Set cairo user data with background color information, to be
		 * used for clear color. */
		cairo_set_user_data (cr, (cairo_user_data_key_t *)0, &r, NULL);
		cairo_set_user_data (cr, (cairo_user_data_key_t *)1, &g, NULL);
		cairo_set_user_data (cr, (cairo_user_data_key_t *)2, &b, NULL);
	}

	for (i = gerbvProject->last_loaded; i >= 0; i--) {
		if (gerbvProject->file[i] && gerbvProject->file[i]->isVisible) {
		        gerbv_set_render_options_for_file (gerbvProject, gerbvProject->file[i], renderInfo);
			gerbv_render_layer_to_cairo_target_without_transforming(
					cr, gerbvProject->file[i],
					renderInfo, FALSE);
		}
	}
}

/* ------------------------------------------------------------------ */

void
gerbv_set_render_options_for_file (gerbv_project_t *gerbvProject, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo)
{
        const char * text_min;
        const char * text_max;
        const char * text_mils;
        const char * text_color;
        gfloat tmin, tmax, tmils, pts;

        text_min = x2attr_get_field_or_last(0, 
                                x2attr_get_project_attr_or_default(gerbvProject, "text-min", "6"));
        text_max = x2attr_get_field_or_last(0, 
                                x2attr_get_project_attr_or_default(gerbvProject, "text-max", "12"));
        text_mils = x2attr_get_field_or_last(0, 
                                x2attr_get_project_attr_or_default(gerbvProject, "text-mils", "40"));
        
        tmin = 6.;
        tmax = 12.;
        tmils = 40.;
        sscanf(text_min, "%g", &tmin);
        sscanf(text_max, "%g", &tmax);
        sscanf(text_mils, "%g", &tmils);
        
	// Set the target text size (em square) in inches.
	renderInfo->textSizeInch = 0.001 * tmils;
	
	// scale factor is (display) pixels per (board) inch.  We'll assume that the monitor is 96dpi physical -
	// if it isn't, user can adjust point size args.  So the magnification from 1:1 board:display is
	// scaleFactorX/96.  1 point = 1/72".
	// Compute pts = visible size of text at nominal board inch size.
	pts = renderInfo->scaleFactorX/96. * renderInfo->textSizeInch*72.;
	// Now clamp the text size appropriately.  Setting to 0 disables text.
	if (pts > tmax)
	        renderInfo->textSizeInch *= tmax/pts;
	else if (pts < tmin)
	        renderInfo->textSizeInch = 0.;

        //printf("SROF: text=%g,%g,%g;  pts=%g, scaleFactorX=%g, textSizeInch=%g\n", 
        //        tmin, tmax, tmils, pts, renderInfo->scaleFactorX, renderInfo->textSizeInch);

        // default to file color
        renderInfo->textColor = fileInfo->color;
        text_color = x2attr_get_field_or_last(0, 
                x2attr_get_project_attr_or_default(gerbvProject, "text-color", ""));
        if (*text_color)
            // If format error, will not change color from default.
    	    gerbv_parse_gdk_color(&renderInfo->textColor,
    	                          text_color,
    	                          FALSE,
    	                          "text");
	
}


void
gerbv_render_all_layers_to_cairo_target (gerbv_project_t *gerbvProject,
		cairo_t *cr, gerbv_render_info_t *renderInfo)
{
	int i;
        
        gerbv_fileinfo_t *fileInfo;
        
	/* Fill the background with the appropriate color. */
	cairo_set_source_rgba (cr,
			(double) gerbvProject->background.red/G_MAXUINT16,
			(double) gerbvProject->background.green/G_MAXUINT16,
			(double) gerbvProject->background.blue/G_MAXUINT16, 1);
	cairo_paint (cr);

	for (i = gerbvProject->last_loaded; i >= 0; i--) {
	        fileInfo = gerbvProject->file[i];
		if (gerbvProject->file[i] && fileInfo->isVisible) {
			cairo_push_group (cr);

		        gerbv_set_render_options_for_file (gerbvProject, fileInfo, renderInfo);
			gerbv_render_layer_to_cairo_target (cr,
					fileInfo, renderInfo);
			cairo_pop_group_to_source (cr);
			cairo_paint_with_alpha (cr, (double)
					fileInfo->alpha/G_MAXUINT16);
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
    fprintf (stderr, "malloc failed in %s()\n", __FUNCTION__);
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

gerbv_fileinfo_t *
gerbv_get_fileinfo_for_image(const gerbv_image_t *image,
		const gerbv_project_t *project)
{
	int i;

	for (i = 0; i <= project->last_loaded; i++) {
		if (project->file[i]->image == image)
			return project->file[i];
	}

	return NULL;	
}

inline void
gerbv_rotate_coord(double *x, double *y, double rad)
{
	double x0 = *x;

	*x = x0*cos(rad) - *y*sin(rad);
	*y = x0*sin(rad) + *y*cos(rad);
}

void
gerbv_transform_coord(double *x, double *y,
			const gerbv_user_transformation_t *trans)
{

	*x = trans->scaleX * *x;
	*y = trans->scaleY * *y;

	gerbv_rotate_coord(x, y, trans->rotation);

	if (trans->mirrorAroundY)
		*x = -*x;

	if (trans->mirrorAroundX)
		*y = -*y;

	*x += trans->translateX;
	*y += trans->translateY;
}

int
gerbv_transform_coord_for_image(double *x, double *y,
		const gerbv_image_t *image, const gerbv_project_t *project)
{
	gerbv_fileinfo_t *fileinfo =
		gerbv_get_fileinfo_for_image(image, project);

	if (fileinfo == NULL) {
		dprintf("%s(): NULL fileinfo\n", __func__);
		return -1;
	}

	gerbv_transform_coord(x, y, &fileinfo->transform);

	return 0;
}

int 
gerbv_parse_color(gerbv_layer_color * color,
                   const char * color_str,
                   gboolean allow_alpha,
                   const char * context)
{
        int r, g, b, a;
        
        // Code moved from 'main' so can use it throughout library, and maybe one day
        // support words like "copper".
        if (!color_str) {
                fprintf(stderr, 
                        allow_alpha ?
                                _("You must give a %s color in the hex-format <#RRGGBB> or <#RRGGBBAA>.\n")
                             :
                                _("You must give a %s color in the hex-format <#RRGGBB>.\n"),
                        context
                        );
                return -1;
        }
        if (((strlen (color_str) != 7) && (!allow_alpha || strlen (color_str) != 9))
            || (color_str[0] != '#')) {
                fprintf(stderr, 
                        _("Specified %s color format is not recognized.\n"),
                        context
                        );
                return -1;
        }
        r=g=b=a=-1;
        if(strlen(color_str)==7){
                sscanf (color_str,"#%2x%2x%2x",&r,&g,&b);
                a=177;
        }
        else{
                sscanf (color_str,"#%2x%2x%2x%2x",&r,&g,&b,&a);
        }

        if ( (r<0)||(r>255)||(g<0)||(g>255)||(b<0)||(b>255)||(a<0)||(a>255) ) {

                fprintf(stderr, 
                        _("Specified %s color values should be between 0x00 (0) and 0xFF (255).\n"),
                        context
                        );
                return -1;
        }
        color->red   = r;
        color->green = g;
        color->blue  = b;
        color->alpha = a;
        return 0;
}

int 
gerbv_parse_gdk_color(GdkColor * color,
                   const char * color_str,
                   gboolean allow_alpha,
                   const char * context)
{
        //TODO: GdKColor is deprecated.  Should be using RGBA.
        // "allow_alpha" must currently be false since GdkColor does not support transparency.
        gerbv_layer_color k;
        if (gerbv_parse_color(&k, color_str, allow_alpha, context))
                return -1;
        color->red   = k.red * 257;
        color->green = k.green * 257;
        color->blue  = k.blue * 257;
        return 0;
}

typedef struct key_point
{
        vertex_t        v;      // The key coordinate of the IPC feature
        gerbv_net_t *   ipc;    // IPC object that has netname or component attributes of intereest.
} key_point_t;

typedef struct anno_data
{
        GArray *        ipcs;   // Array of the above key_point_t, for this layer.
        int             layernum;
        int             maxlayer;
        gboolean        overwrite;
        
} anno_data_t;

static void
_get_key_points_cb(search_state_t * ss, search_context_t ctx)
{
        /*
        This is the callback for when scanning the IPC (source) image.
        
        Although true IPC data will have only generated round and rectangular
        flashes, and straight tracks, we should support general Gerber images,
        so the only context not supported is for polygon regions.
        */
        anno_data_t * d = (anno_data_t *)ss->user_data;
        const char * ipclayer_str;
        int ipclayer;

        // Populate ipcs array with key point data.  No pours, but want macro polygons
        // since these are probably pads.
        if (ctx == SEARCH_POLYGON && !ss->amacro)
                return;
                
        // IPC layer must be 00 (access both sides) or equal to the target layer.
        ipclayer_str = x2attr_get_net_attr_or_default(ss->net, "IPCLayer", "0");
        sscanf(ipclayer_str, "%d", &ipclayer);
        if (ipclayer && ipclayer != d->layernum)
                return;
        if (!ipclayer && d->layernum != 1 && d->layernum != d->maxlayer)
                // IPC access (~layer) 00 refers to top and bottom only.
                return;
        if (x2attr_get_net_attr(ss->net, ".N")
            || x2attr_get_net_attr(ss->net, ".P")
            || x2attr_get_net_attr(ss->net, ".C")) {
                key_point_t kp;
                kp.v.x = ss->net->stop_x;
                kp.v.y = ss->net->stop_y;
                kp.ipc = ss->net;
                g_array_append_vals(d->ipcs, &kp, 1);
                if (ss->net->aperture_state != GERBV_APERTURE_STATE_FLASH) {
                        // Assume it's a track, so add both ends.
                        kp.v.x = ss->net->start_x;
                        kp.v.y = ss->net->start_y;
                        g_array_append_vals(d->ipcs, &kp, 1);
                }
        }
}

static void
_anno_cb(search_state_t * ss, search_context_t ctx)
{
        anno_data_t * d = (anno_data_t *)ss->user_data;
        gdouble dist;
        guint i;
        cairo_matrix_t m;
        
        // See if the current object (from the image we are annotating) strictly encloses
        // any of the objects in d->ipcs.  The first one which is found causes its attributes
        // to be assigned to this.
        // Currently, we're not processing pours, unless from an aperture macro.
        if (ctx == SEARCH_POLYGON && !ss->amacro)
                return;
                
        m = ss->transform[ss->stack];
        if (cairo_matrix_invert(&m) == CAIRO_STATUS_INVALID_MATRIX)
                return;
        for (i = 0; i < d->ipcs->len; ++i) {
                key_point_t * kp = (key_point_t *)d->ipcs->data + i;
                gdouble x = kp->v.x;
                gdouble y = kp->v.y;
                
                cairo_matrix_transform_point(&m, &x, &y);       
                dist = search_distance_to_border_no_transform(ss, ctx, x, y);
                if (dist < 0.) {
                        const char * netname = x2attr_get_net_attr(kp->ipc, ".N");
                        const char * pin = x2attr_get_net_attr(kp->ipc, ".P");
                        const char * cmp = x2attr_get_net_attr(kp->ipc, ".C");
                        // If source is a track, but this is a flash, or vice versa,
                        // (i.e. aperture state mismatch) then keep looking.
                        if (ss->net->aperture_state != kp->ipc->aperture_state)
                                continue;
                        // Don't insert if already exists, unless overwrite.
                        if (!d->overwrite) {
                                if (x2attr_get_net_attr(ss->net, ".N"))
                                        netname = NULL;
                                if (x2attr_get_net_attr(ss->net, ".P"))
                                        pin = NULL;
                                if (x2attr_get_net_attr(ss->net, ".C"))
                                        cmp = NULL;
                        }
                        if (netname)
                                x2attr_set_net_attr(ss->net, ".N", netname);
                        if (pin)
                                x2attr_set_net_attr(ss->net, ".P", pin);
                        if (cmp)
                                x2attr_set_net_attr(ss->net, ".C", cmp);
                        break;  
                }
        }
}


void
gerbv_annotate_rs274x_from_ipcd356a(int layernum, int maxlayer, 
                                gerbv_fileinfo_t * rs274x, 
                                gerbv_fileinfo_t * ipcd356a,
                                gboolean overwrite)
{
        /* Use data for given layer in ipc data to annotate rs274x (on same layer).
        
           First, collate an array of known coordinates that have netname and/or component pins,
           gleaned from the ipc file.
           
           Then, search the rs274x layer iterating through each object and, if the object definitely
           contains one of the ipc points, assign appropriate object attributes.
        */
        anno_data_t d;
        search_state_t * ss;

        printf("Annotating layer %d%s\n", layernum, overwrite ? ", overwrite" : "");
        
        d.ipcs = g_array_sized_new(FALSE, FALSE, sizeof(key_point_t), 1024);
        d.overwrite = overwrite;
        d.layernum = layernum;
        d.maxlayer = maxlayer;
        
        // First scan the IPC data to fill in key points for this layer
        ss = search_image(NULL, ipcd356a->image, _get_key_points_cb, &d);
        search_destroy_search_state(ss);

        // Now to the actual annotation.
        ss = search_image(NULL, rs274x->image, _anno_cb, &d);
        search_destroy_search_state(ss);
        g_array_free(d.ipcs, TRUE);
}

