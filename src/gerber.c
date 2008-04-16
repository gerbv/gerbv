/*
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
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

#include <stdlib.h>
#include <string.h>
#include <math.h>  /* pow() */
#include <glib.h>
#include <locale.h>
#include <errno.h>
#include <ctype.h>


#include "config.h"
#include "gerber.h"
#include "gerb_error.h"
#include "gerb_stats.h"

//#define AMACRO_DEBUG

#ifndef RENDER_USING_GDK
#include <cairo.h>
#endif

/* include this for macro enums */
#include "draw-gdk.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

//#define AMACRO_DEBUG

#define A2I(a,b) (((a & 0xff) << 8) + (b & 0xff))

#define MAXL 200

typedef struct gerb_state {
    int curr_x;
    int curr_y;
    int prev_x;
    int prev_y;
    int delta_cp_x;
    int delta_cp_y;
    int curr_aperture;
    int changed;
    enum aperture_state_t aperture_state;
    enum interpolation_t interpolation;
    enum interpolation_t prev_interpolation;
    gerb_net_t *parea_start_node;
    gerb_layer_t *layer;
    gerb_netstate_t *state;
    int in_parea_fill;
    int mq_on;
} gerb_state_t;


/* Local function prototypes */
static void parse_G_code(gerb_file_t *fd, gerb_state_t *state, 
			 gerb_image_t *image);
static void parse_D_code(gerb_file_t *fd, gerb_state_t *state, 
			 gerb_image_t *image);
static int parse_M_code(gerb_file_t *fd, gerb_image_t *image);
static void parse_rs274x(gint levelOfRecursion, gerb_file_t *fd, 
			 gerb_image_t *image, gerb_state_t *state, 
			 gerb_net_t *curr_net, gerb_stats_t *stats, 
			 gchar *directoryPath);
static int parse_aperture_definition(gerb_file_t *fd, 
				     gerb_aperture_t *aperture,
				     gerb_image_t *image, gdouble scale);
static void calc_cirseg_sq(struct gerb_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);
static void calc_cirseg_mq(struct gerb_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);

static void
gerber_update_min_and_max(gerb_image_info_t *info, gdouble repeatX, gdouble repeatY,
			  gdouble x, gdouble y, gdouble apertureSizeX1,
			  gdouble apertureSizeX2,gdouble apertureSizeY1,
			  gdouble apertureSizeY2);


static void gerber_update_any_running_knockout_measurements(gerb_image_t *image);

static void gerber_calculate_final_justify_effects (gerb_image_t *image);

gboolean knockoutMeasure = FALSE;
gdouble knockoutLimitXmin, knockoutLimitYmin, knockoutLimitXmax, 
    knockoutLimitYmax;
gerb_layer_t *knockoutLayer = NULL;

#ifndef RENDER_USING_GDK
cairo_matrix_t currentMatrix;
#endif  

gerb_net_t *
gerber_create_new_net (gerb_net_t *currentNet, gerb_layer_t *layer, gerb_netstate_t *state){
	gerb_net_t *newNet = g_new0 (gerb_net_t, 1);
	
	currentNet->next = newNet;
	newNet->layer = layer;
	newNet->state = state;
	return newNet;
}

gboolean
gerber_parse_file_segment (gint levelOfRecursion, gerb_image_t *image, 
			   gerb_state_t *state,	gerb_net_t *curr_net, 
			   gerb_stats_t *stats, gerb_file_t *fd, 
			   gchar *directoryPath) {
    int read, coord, len, polygonPoints=0;
    double x_scale = 0.0, y_scale = 0.0;
    double delta_cp_x = 0.0, delta_cp_y = 0.0;
    double aperture_size;
    double scale;
    gboolean foundEOF = FALSE;
    
    while ((read = gerb_fgetc(fd)) != EOF) {
        /* figure out the scale, since we need to normalize 
	   all dimensions to inches */
        if (state->state->unit == MM)
            scale = 25.4;
        else
            scale = 1.0;
	switch ((char)(read & 0xff)) {
	case 'G':
	    dprintf("... Found G code\n");
	    parse_G_code(fd, state, image);
	    break;
	case 'D':
	    dprintf("... Found D code\n");
	    parse_D_code(fd, state, image);
	    break;
	case 'M':
	    dprintf("... Found M code\n");
	    switch(parse_M_code(fd, image)) {
	    case 1 :
	    case 2 :
	    case 3 :
		foundEOF = TRUE;
		break;
	    default:
		gerb_stats_add_error(stats->error_list,
				     -1,
				     "Unknown M code found.\n",
				     GRB_ERROR);
	    } /* switch(parse_M_code) */
	    break;
	case 'X':
	    dprintf("... Found X code\n");
	    stats->X++;
	    coord = gerb_fgetint(fd, &len);
	    if (image->format && image->format->omit_zeros == TRAILING) {
		
		switch ((image->format->x_int + image->format->x_dec) - len) {
		case 5:
		    coord *= 10;
		case 4:
		    coord *= 10;
		case 3:
		    coord *= 10;
		case 2:
		    coord *= 10;
		case 1:
		    coord *= 10;
		    break;
		default:
		    ;
		}
	    }
	    if (image->format && (image->format->coordinate==INCREMENTAL))
	        state->curr_x += coord;
	    else
	        state->curr_x = coord;
	    state->changed = 1;
	    break;
	case 'Y':
	    dprintf("... Found Y code\n");
	    stats->Y++;
	    coord = gerb_fgetint(fd, &len);
	    if (image->format && image->format->omit_zeros == TRAILING) {

		switch ((image->format->y_int + image->format->y_dec) - len) {
		case 5:
		    coord *= 10;
		case 4:
		    coord *= 10;
		case 3:
		    coord *= 10;
		case 2:
		    coord *= 10;
		case 1:
		    coord *= 10;
		    break;
		default:
		    ;
		}
	    }
	    if (image->format && (image->format->coordinate==INCREMENTAL))
	        state->curr_y += coord;
	    else
	        state->curr_y = coord;
	    state->changed = 1;
	    break;
	case 'I':
	    dprintf("... Found I code\n");
	    stats->I++;
	    state->delta_cp_x = gerb_fgetint(fd, NULL);
	    state->changed = 1;
	    break;
	case 'J':
	    dprintf("... Found J code\n");
	    stats->J++;
	    state->delta_cp_y = gerb_fgetint(fd, NULL);
	    state->changed = 1;
	    break;
	case '%':
	    dprintf("... Found %% code\n");
	    parse_rs274x(levelOfRecursion, fd, image, state, curr_net, stats, directoryPath);
	    while (1) {
		int c = gerb_fgetc(fd);
		if(c == EOF || c == '%')
		    break;
	    }
	    break;
	case '*':  
	    dprintf("... Found * code\n");
	    stats->star++;
	    if (state->changed == 0) break;
	    state->changed = 0;
	    
	    /* don't even bother saving the net if the aperture state is OFF and we
	       aren't starting a polygon fill (where we need it to get to the start point) */
	    if ((state->aperture_state == OFF)&&(!state->in_parea_fill)&&
	    		(state->interpolation != PAREA_START)) {
		/* save the coordinate so the next net can use it for a start point */
		state->prev_x = state->curr_x;
		state->prev_y = state->curr_y;
		break;
	    }
	    curr_net = gerber_create_new_net (curr_net, state->layer, state->state);
	    /*
	     * Scale to given coordinate format
	     * XXX only "omit leading zeros".
	     */
	    if (image && image->format ){
		x_scale = pow(10.0, (double)image->format->x_dec);
		y_scale = pow(10.0, (double)image->format->y_dec);
	    }
	    x_scale *= scale;
	    y_scale *= scale;
	    curr_net->start_x = (double)state->prev_x / x_scale;
	    curr_net->start_y = (double)state->prev_y / y_scale;
	    curr_net->stop_x = (double)state->curr_x / x_scale;
	    curr_net->stop_y = (double)state->curr_y / y_scale;
	    delta_cp_x = (double)state->delta_cp_x / x_scale;
	    delta_cp_y = (double)state->delta_cp_y / y_scale;
	    
	    
	    switch (state->interpolation) {
	    case CW_CIRCULAR :
		curr_net->cirseg = g_new0 (gerb_cirseg_t,1);
		if (state->mq_on)
		    calc_cirseg_mq(curr_net, 1, delta_cp_x, delta_cp_y);
		else
		    calc_cirseg_sq(curr_net, 1, delta_cp_x, delta_cp_y);
		break;
	    case CCW_CIRCULAR :
		curr_net->cirseg = g_new0 (gerb_cirseg_t,1);
		if (state->mq_on)
		    calc_cirseg_mq(curr_net, 0, delta_cp_x, delta_cp_y);
		else
		    calc_cirseg_sq(curr_net, 0, delta_cp_x, delta_cp_y);
		break;
	    case PAREA_START :
		/* 
		 * To be able to get back and fill in number of polygon corners
		 */
		state->parea_start_node = curr_net;
		state->in_parea_fill = 1;
		polygonPoints = 0;
		break;
	    case PAREA_END :
		state->parea_start_node = NULL;
		state->in_parea_fill = 0;
		polygonPoints = 0;
		break;
	    default :
		break;
	    }  /* switch(state->interpolation) */

	    /* 
	     * Count number of points in Polygon Area 
	     */
	    if (state->in_parea_fill && state->parea_start_node) {
		/* 
		 * "...all lines drawn with D01 are considered edges of the
		 * polygon. D02 closes and fills the polygon."
		 * p.49 rs274xrevd_e.pdf
		 * D02 -> state->aperture_state == OFF
		 */
		 
		 /* UPDATE: only end the polygon during a D02 call if we've already
		    drawn a polygon edge (with D01) */
		   
		if ((state->aperture_state == OFF &&
		    	state->interpolation != PAREA_START) && (polygonPoints > 0)) {
		    curr_net->interpolation = PAREA_END;
		    
		    curr_net = gerber_create_new_net (curr_net, state->layer, state->state);
		    curr_net->interpolation = PAREA_START;
		    state->parea_start_node = curr_net;
		    
		    curr_net = gerber_create_new_net (curr_net, state->layer, state->state);		    
		    curr_net->start_x = (double)state->prev_x / x_scale;
		    curr_net->start_y = (double)state->prev_y / y_scale;
		    curr_net->stop_x = (double)state->curr_x / x_scale;
		    curr_net->stop_y = (double)state->curr_y / y_scale;
		}
		if (state->interpolation != PAREA_START)
		    polygonPoints++;
		
	    }  /* if (state->in_parea_fill && state->parea_start_node) */
	    
	    curr_net->interpolation = state->interpolation;

	    /* 
	     * Override circular interpolation if no center was given.
	     * This should be a safe hack, since a good file should always 
	     * include I or J.  And even if the radius is zero, the endpoint 
	     * should be the same as the start point, creating no line 
	     */
	    if (((state->interpolation == CW_CIRCULAR) || 
		 (state->interpolation == CCW_CIRCULAR)) && 
		((state->delta_cp_x == 0.0) && (state->delta_cp_y == 0.0)))
		curr_net->interpolation = LINEARx1;
	    
	    /*
	     * If we detected the end of Polygon Area Fill we go back to
	     * the interpolation we had before that.
	     * Also if we detected any of the quadrant flags, since some
	     * gerbers don't reset the interpolation (EagleCad again).
	     */
	    if ((state->interpolation == PAREA_START) ||
		(state->interpolation == PAREA_END))
		state->interpolation = state->prev_interpolation;

	    /*
	     * Save layer polarity and unit
	     */
	    curr_net->layer = state->layer;  
	    
	    state->delta_cp_x = 0.0;
	    state->delta_cp_y = 0.0;
	    curr_net->aperture = state->curr_aperture;
	    curr_net->aperture_state = state->aperture_state;

	    /*
	     * For next round we save the current position as
	     * the previous position
	     */
	    state->prev_x = state->curr_x;
	    state->prev_y = state->curr_y;

	    /*
	     * If we have an aperture defined at the moment we find 
	     * min and max of image with compensation for mm.
	     */
	    if ((curr_net->aperture == 0) && !state->in_parea_fill) 
		break;
	    
	    /* only update the min/max values if we are drawing */
	    if ((curr_net->aperture_state != OFF)){
		double repeat_off_X = 0.0, repeat_off_Y = 0.0;
		
		/*
		 * If step_and_repeat (%SR%) is used, check min_x,max_y etc for
		 * the ends of the step_and_repeat lattice. This goes wrong in 
		 * the case of negative dist_X or dist_Y, in which case we 
		 * should compare against the startpoints of the lines, not 
		 * the stoppoints, but that seems an uncommon case (and the 
		 * error isn't very big any way).
		 */
		repeat_off_X = (state->layer->stepAndRepeat.X - 1) *
		    state->layer->stepAndRepeat.dist_X;
		repeat_off_Y = (state->layer->stepAndRepeat.Y - 1) *
		    state->layer->stepAndRepeat.dist_Y;
		
		
#ifndef RENDER_USING_GDK
		cairo_matrix_init (&currentMatrix, 1, 0, 0, 1, 0, 0);
		/* offset image */
		cairo_matrix_translate (&currentMatrix, image->info->offsetA, 
					image->info->offsetB);
		/* do image rotation */
		cairo_matrix_rotate (&currentMatrix, image->info->imageRotation);
		/* it's a new layer, so recalculate the new transformation 
		 * matrix for it */
		/* do any rotations */
		cairo_matrix_rotate (&currentMatrix, state->layer->rotation);
			
		/* calculate current layer and state transformation matrices */
		/* apply scale factor */
		cairo_matrix_scale (&currentMatrix, state->state->scaleA, 
				    state->state->scaleB);
		/* apply offset */
		cairo_matrix_translate (&currentMatrix, state->state->offsetA,
					state->state->offsetB);
		/* apply mirror */
		switch (state->state->mirrorState) {
		case FLIPA:
		    cairo_matrix_scale (&currentMatrix, -1, 1);
		    break;
		case FLIPB:
		    cairo_matrix_scale (&currentMatrix, 1, -1);
		    break;
		case FLIPAB:
		    cairo_matrix_scale (&currentMatrix, -1, -1);
		    break;
		default:
		    break;
		}
		/* finally, apply axis select */
		if (state->state->axisSelect == SWAPAB) {
		    /* we do this by rotating 270 (counterclockwise, then 
		     *  mirroring the Y axis 
		     */
		    cairo_matrix_rotate (&currentMatrix, 3 * M_PI / 2);
		    cairo_matrix_scale (&currentMatrix, 1, -1);
		}
#endif
		/* if it's a macro, step through all the primitive components
		   and calculate the true bounding box */
		if ((image->aperture[curr_net->aperture] != NULL) &&
		    (image->aperture[curr_net->aperture]->type == MACRO)) {
		    gerb_simplified_amacro_t *ls = image->aperture[curr_net->aperture]->simplified;
	      
		    while (ls != NULL) {
			gdouble offsetx = 0, offsety = 0, widthx = 0, widthy = 0;
			gboolean calculatedAlready = FALSE;
			
			if (ls->type == MACRO_CIRCLE) {
			    offsetx=ls->parameter[CIRCLE_CENTER_X];
			    offsety=ls->parameter[CIRCLE_CENTER_Y];
			    widthx=widthy=ls->parameter[CIRCLE_DIAMETER];
			} else if (ls->type == MACRO_OUTLINE) {
			    int pointCounter,numberOfPoints;
			    numberOfPoints = (int) ls->parameter[OUTLINE_NUMBER_OF_POINTS];
		
			    for (pointCounter = 0; pointCounter < numberOfPoints; pointCounter++) {
				gerber_update_min_and_max (image->info, repeat_off_X, repeat_off_Y,
							   ls->parameter[pointCounter * 2 + OUTLINE_FIRST_X],
							   ls->parameter[pointCounter * 2 + OUTLINE_FIRST_Y], 
							   0,0,0,0);
			    }
			    calculatedAlready = TRUE;
			} else if (ls->type == MACRO_POLYGON) {
			    offsetx = ls->parameter[POLYGON_CENTER_X];
			    offsety = ls->parameter[POLYGON_CENTER_Y];
			    widthx = widthy = ls->parameter[POLYGON_DIAMETER];
			} else if (ls->type == MACRO_MOIRE) {
			    offsetx = ls->parameter[MOIRE_CENTER_X];
			    offsety = ls->parameter[MOIRE_CENTER_Y];
			    widthx = widthy = ls->parameter[MOIRE_OUTSIDE_DIAMETER];
			} else if (ls->type == MACRO_THERMAL) {
			    offsetx = ls->parameter[THERMAL_CENTER_X];
			    offsety = ls->parameter[THERMAL_CENTER_Y];
			    widthx = widthy = ls->parameter[THERMAL_OUTSIDE_DIAMETER];
			} else if (ls->type == MACRO_LINE20) {
			    widthx = widthy = ls->parameter[LINE20_LINE_WIDTH];
			    gerber_update_min_and_max (image->info, repeat_off_X, repeat_off_Y,
						       ls->parameter[LINE20_START_X] + offsetx,
						       ls->parameter[LINE20_START_Y] + offsety, 
						       widthx/2,widthx/2,widthy/2,widthy/2);
			    gerber_update_min_and_max (image->info, repeat_off_X, repeat_off_Y,
						       ls->parameter[LINE20_END_X] + offsetx,
						       ls->parameter[LINE20_END_Y] + offsety, 
						       widthx/2,widthx/2,widthy/2,widthy/2);
			    calculatedAlready = TRUE;
			} else if (ls->type == MACRO_LINE21) {
			    gdouble largestDimension = sqrt (ls->parameter[LINE21_WIDTH]/2 *
							     ls->parameter[LINE21_WIDTH]/2 + ls->parameter[LINE21_HEIGHT/2] *
							     ls->parameter[LINE21_HEIGHT]/2);

			    offsetx = ls->parameter[LINE21_CENTER_X];
			    offsety = ls->parameter[LINE21_CENTER_Y];
			    widthx = widthy=largestDimension;
			} else if (ls->type == MACRO_LINE22) {
			    gdouble largestDimension = sqrt (ls->parameter[LINE22_WIDTH]/2 *
							     ls->parameter[LINE22_WIDTH]/2 + ls->parameter[LINE22_HEIGHT/2] *
							     ls->parameter[LINE22_HEIGHT]/2);

			    offsetx = ls->parameter[LINE22_LOWER_LEFT_X] +
	      			ls->parameter[LINE22_WIDTH]/2;
			    offsety = ls->parameter[LINE22_LOWER_LEFT_Y] +
	      			ls->parameter[LINE22_HEIGHT]/2;
			    widthx = widthy=largestDimension;
			}
	      	
			if (!calculatedAlready) {
			    gerber_update_min_and_max (image->info, repeat_off_X, repeat_off_Y,
						       curr_net->stop_x + offsetx,
						       curr_net->stop_y + offsety, 
						       widthx/2,widthx/2,widthy/2,widthy/2);
			}
	    		ls = ls->next;
		    }
		} else {
		    if (image->aperture[curr_net->aperture] != NULL) {
			aperture_size = image->aperture[curr_net->aperture]->parameter[0];
		    } else {
			/* this is usually for polygon fills, where the aperture width
			   if "zero" */
			aperture_size = 0;
		    }
		    
		    /* check both the start and stop of the aperture points against
		       a running min/max counter */
		    /* Note: only check start coordinate if this isn't a flash, 
		       since the start point may be bogus if it is a flash */
		    if (curr_net->aperture_state != FLASH) {
			gerber_update_min_and_max (image->info, repeat_off_X, repeat_off_Y,
						   curr_net->start_x, curr_net->start_y, 
						   aperture_size/2,aperture_size/2,
						   aperture_size/2,aperture_size/2);
		    }
		    gerber_update_min_and_max (image->info, repeat_off_X, repeat_off_Y,
					       curr_net->stop_x, curr_net->stop_y, 
					       aperture_size/2,aperture_size/2,
					       aperture_size/2,aperture_size/2);
		}
	    }
	    break;
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	case 0 :
	    break;
	default:
	    stats->unknown++;
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Found unknown character (whitespace?) [%d]%c\n", 
						 read, read),
				 GRB_ERROR);
	}  /* switch((char) (read & 0xff)) */
    }
    return foundEOF;
}


/* ------------------------------------------------------------------ */
gerb_image_t *
parse_gerb(gerb_file_t *fd, gchar *directoryPath)
{
    gerb_state_t *state = NULL;
    gerb_image_t *image = NULL;
    gerb_net_t *curr_net = NULL;
    gerb_stats_t *stats;
    gboolean foundEOF = FALSE;
    
    /* added by t.motylewski@bfad.de
     * many locales redefine "." as "," and so on, 
     * so sscanf and strtod has problems when
     * reading files using %f format */
    setlocale(LC_NUMERIC, "C" );

    /* 
     * Create new state.  This is used locally to keep track
     * of the photoplotter's state as the Gerber is read in.
     */
    state = g_new0 (gerb_state_t, 1);

    /* 
     * Create new image.  This will be returned.
     */
    image = new_gerb_image(image, "RS274-X (Gerber) File");
    if (image == NULL)
	GERB_FATAL_ERROR("malloc image failed\n");
    curr_net = image->netlist;
    image->layertype = GERBER;
    image->gerb_stats = gerb_stats_new();
    if (image->gerb_stats == NULL)
	GERB_FATAL_ERROR("malloc gerb_stats failed\n");
    stats = (gerb_stats_t *) image->gerb_stats;

    /* set active layer and netstate to point to first default one created */
    state->layer = image->layers;
    state->state = image->states;
    curr_net->layer = state->layer;
    curr_net->state = state->state;

    /*
     * Start parsing
     */
    dprintf("In parse_gerb, starting to parse file...\n");
    foundEOF = gerber_parse_file_segment (1, image, state, curr_net, stats,
					  fd, directoryPath);

    if (!foundEOF) {
    	gerb_stats_add_error(stats->error_list,
			     -1,
			     "File is missing Gerber EOF code.\n",
			     GRB_ERROR);
    }
    g_free(state);
    
    dprintf("               ... done parsing Gerber file\n");
    gerber_update_any_running_knockout_measurements (image);
    gerber_calculate_final_justify_effects(image);

    return image;
} /* parse_gerb */


/* ------------------------------------------------------------------- */
/*
 * Checks for signs that this is a RS-274X file
 * Returns TRUE if it is, FALSE if not.
 */
gboolean
gerber_is_rs274x_p(gerb_file_t *fd, gboolean *returnFoundBinary) 
{
    char *buf;
    int len = 0;
    char *letter;
    int i;
    gboolean found_binary = FALSE;
    gboolean found_ADD = FALSE;
    gboolean found_D0 = FALSE;
    gboolean found_D2 = FALSE;
    gboolean found_M0 = FALSE;
    gboolean found_M2 = FALSE;
    gboolean found_star = FALSE;
    gboolean found_X = FALSE;
    gboolean found_Y = FALSE;
   
    dprintf ("gerber_is_rs274x_p(%p, %p), fd->fd = %p\n", fd, returnFoundBinary, fd->fd); 
    buf = (char *) g_malloc(MAXL);
    if (buf == NULL) 
	GERB_FATAL_ERROR("malloc buf failed while checking for rs274x.\n");
    
    while (fgets(buf, MAXL, fd->fd) != NULL) {
        dprintf ("buf = \"%s\"\n", buf);
	len = strlen(buf);
    
	/* First look through the file for indications of its type by
	 * checking that file is not binary (non-printing chars and white 
	 * spaces)
	 */
	for (i = 0; i < len; i++) {
	    if (!isprint((int) buf[i]) && (buf[i] != '\r') && 
		(buf[i] != '\n') && (buf[i] != '\t')) {
		found_binary = TRUE;
                dprintf ("found_binary (%d)\n", buf[i]);
	    }
	}
	if (g_strstr_len(buf, len, "%ADD")) {
	    found_ADD = TRUE;
            dprintf ("found_ADD\n");
	}
	if (g_strstr_len(buf, len, "D00")) {
	    found_D0 = TRUE;
            dprintf ("found_D0\n");
	}
	if (g_strstr_len(buf, len, "D02")) {
	    found_D2 = TRUE;
            dprintf ("found_D2\n");
	}
	if (g_strstr_len(buf, len, "M0")) {
	    found_M0 = TRUE;
            dprintf ("found_M0\n");
	}
	if (g_strstr_len(buf, len, "M00")) {
	    found_M0 = TRUE;
            dprintf ("found_M0\n");
	}
	if (g_strstr_len(buf, len, "M2")) {
	    found_M2 = TRUE;
            dprintf ("found_M2\n");
	}
	if (g_strstr_len(buf, len, "M02")) {
	    found_M2 = TRUE;
            dprintf ("found_M2\n");
	}
	if (g_strstr_len(buf, len, "*")) {
	    found_star = TRUE;
            dprintf ("found_star\n");
	}
	/* look for X<number> or Y<number> */
	if ((letter = g_strstr_len(buf, len, "X")) != NULL) {
	    if (isdigit((int) letter[1])) { /* grab char after X */
		found_X = TRUE;
                dprintf ("found_X\n");
	    }
	}
	if ((letter = g_strstr_len(buf, len, "Y")) != NULL) {
	    if (isdigit((int) letter[1])) { /* grab char after Y */
		found_Y = TRUE;
                dprintf ("found_Y\n");
	    }
	}
    }
    rewind(fd->fd);
    free(buf);
   
    *returnFoundBinary = found_binary;

    /* Now form logical expression determining if the file is RS-274X */
    if ((found_D0 || found_D2 || found_M0 || found_M2) && 
	found_ADD && found_star && (found_X || found_Y)) 
	return TRUE;

    
    return FALSE;

} /* gerber_is_rs274x */


/* ------------------------------------------------------------------- */
/*
 * Checks for signs that this is a RS-274D file
 * Returns TRUE if it is, FALSE if not.
 */
gboolean
gerber_is_rs274d_p(gerb_file_t *fd) 
{
    char *buf;
    int len = 0;
    char *letter;
    int i;
    gboolean found_binary = FALSE;
    gboolean found_ADD = FALSE;
    gboolean found_D0 = FALSE;
    gboolean found_D2 = FALSE;
    gboolean found_M0 = FALSE;
    gboolean found_M2 = FALSE;
    gboolean found_star = FALSE;
    gboolean found_X = FALSE;
    gboolean found_Y = FALSE;
    
    buf = malloc(MAXL);
    if (buf == NULL) 
	GERB_FATAL_ERROR("malloc buf failed while checking for rs274d.\n");

    while (fgets(buf, MAXL, fd->fd) != NULL) {
	len = strlen(buf);
    
	/* First look through the file for indications of its type */
    
	/* check that file is not binary (non-printing chars */
	for (i = 0; i < len; i++) {
	    if (!isprint( (int) buf[i]) && (buf[i] != '\r') && 
		(buf[i] != '\n') && (buf[i] != '\t')) {
		found_binary = TRUE;
	    }
	}
	
	if (g_strstr_len(buf, len, "%ADD")) {
	    found_ADD = TRUE;
	}
	if (g_strstr_len(buf, len, "D00")) {
	    found_D0 = TRUE;
	}
	if (g_strstr_len(buf, len, "D02")) {
	    found_D2 = TRUE;
	}
	if (g_strstr_len(buf, len, "M0")) {
	    found_M0 = TRUE;
	}
	if (g_strstr_len(buf, len, "M00")) {
	    found_M0 = TRUE;
	}
	if (g_strstr_len(buf, len, "M02")) {
	    found_M2 = TRUE;
	}
	if (g_strstr_len(buf, len, "*")) {
	    found_star = TRUE;
	}
	/* look for X<number> or Y<number> */
	if ((letter = g_strstr_len(buf, len, "X")) != NULL) {
	    /* grab char after X */
	    if (isdigit( (int) letter[1])) {
		found_X = TRUE;
	    }
	}
	if ((letter = g_strstr_len(buf, len, "Y")) != NULL) {
	    /* grab char after Y */
	    if (isdigit( (int) letter[1])) {
		found_Y = TRUE;
	    }
	}
    }
    rewind(fd->fd);
    free(buf);

    /* Now form logical expression determining if the file is RS-274D */
    if ((found_D0 || found_D2 || found_M0 || found_M2) && 
	!found_ADD && found_star && (found_X || found_Y) && 
	!found_binary) 
	return TRUE;

    return FALSE;

} /* gerber_is_rs274d */


/* ------------------------------------------------------------------- */
static void 
parse_G_code(gerb_file_t *fd, gerb_state_t *state, gerb_image_t *image)
{
    int  op_int;
    gerb_format_t *format = image->format;
    gerb_stats_t *stats = image->gerb_stats;
    int c;

    op_int=gerb_fgetint(fd, NULL);
    
    switch(op_int) {
    case 0:  /* Move */
	/* Is this doing anything really? */
	stats->G0++;
	break;
    case 1:  /* Linear Interpolation (1X scale) */
	state->interpolation = LINEARx1;
	stats->G1++;
	break;
    case 2:  /* Clockwise Linear Interpolation */
	state->interpolation = CW_CIRCULAR;
	stats->G2++;
	break;
    case 3:  /* Counter Clockwise Linear Interpolation */
	state->interpolation = CCW_CIRCULAR;
	stats->G3++;
	break;
    case 4:  /* Ignore Data Block */
	/* Don't do anything, just read 'til * */
	c = gerb_fgetc(fd);
	while ((c != EOF) && (c != '*')) {
	    c = gerb_fgetc(fd);
	}
	stats->G4++;
	break;
    case 10: /* Linear Interpolation (10X scale) */
	state->interpolation = LINEARx10;
	stats->G10++;
	break;
    case 11: /* Linear Interpolation (0.1X scale) */
	state->interpolation = LINEARx01;
	stats->G11++;
	break;
    case 12: /* Linear Interpolation (0.01X scale) */
	state->interpolation = LINEARx001;
	stats->G12++;
	break;
    case 36: /* Turn on Polygon Area Fill */
	state->prev_interpolation = state->interpolation;
	state->interpolation = PAREA_START;
	state->changed = 1;
	stats->G36++;
	break;
    case 37: /* Turn off Polygon Area Fill */
	state->interpolation = PAREA_END;
	state->changed = 1;
	stats->G37++;
	break;
    case 54: /* Tool prepare */
	/* XXX Maybe uneccesary??? */
	if (gerb_fgetc(fd) == 'D') {
	    int a = gerb_fgetint(fd, NULL);
	    if ((a >= APERTURE_MIN) && (a <= APERTURE_MAX)) {
		state->curr_aperture = a;
		dprintf("     In parse_G_code, case 54, found D and adding 1 to no %d in D_list ...\n", a);
		gerb_stats_increment_D_list_count(stats->D_code_list, 
						  a, 
						  1,
						  stats->error_list); 
	    } else { 
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Found aperture out of bounds while parsing G code: %d\n", a),
				     GRB_ERROR);
	    }
	} else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Found unexpected code after G54\n",
				 GRB_ERROR);
	    /* Must insert error count here */
	}
	stats->G54++;
	break;
    case 55: /* Prepare for flash */
	stats->G55++;
	break;
    case 70: /* Specify inches */
	state->state = gerb_image_return_new_netstate (state->state);
	state->state->unit = INCH;
	stats->G70++;
	break;
    case 71: /* Specify millimeters */
	state->state = gerb_image_return_new_netstate (state->state);
	state->state->unit = MM;
	stats->G71++;
	break;
    case 74: /* Disable 360 circular interpolation */
	state->mq_on = 0;
	stats->G74++;
	break;
    case 75: /* Enable 360 circular interpolation */
	state->mq_on = 1;
	stats->G75++;
	break;
    case 90: /* Specify absolut format */
	if (format) format->coordinate = ABSOLUTE;
	stats->G90++;
	break;
    case 91: /* Specify incremental format */
	if (format) format->coordinate = INCREMENTAL;
	stats->G91++;
	break;
    default:
	gerb_stats_add_error(stats->error_list,
			     -1,
			     g_strdup_printf("Encountered unknown G code : G%d\n", op_int),
			     GRB_ERROR);
	gerb_stats_add_error(stats->error_list,
			     -1,
			     g_strdup_printf("Ignorning unknown G code\n"),
			     WARNING);
	
	stats->G_unknown++;
	/* Enter error count here */
	break;
    }
    
    return;
} /* parse_G_code */


/* ------------------------------------------------------------------ */
static void 
parse_D_code(gerb_file_t *fd, gerb_state_t *state, gerb_image_t *image)
{
    int a;
    gerb_stats_t *stats = image->gerb_stats;
    
    a = gerb_fgetint(fd, NULL);
    dprintf("     In parse_D_code, found D number = %d ... \n", a);
    switch(a) {
    case 1 : /* Exposure on */
	state->aperture_state = ON;
	state->changed = 1;
	stats->D1++;
	break;
    case 2 : /* Exposure off */
	state->aperture_state = OFF;
	state->changed = 1;
	stats->D2++;
	break;
    case 3 : /* Flash aperture */
	state->aperture_state = FLASH;
	state->changed = 1;
	stats->D3++;
	break;
    default: /* Aperture in use */
	if ((a >= APERTURE_MIN) && (a <= APERTURE_MAX)) {
	    state->curr_aperture = a;
	    
	    dprintf("     In parse_D_code, adding 1 to D_list ...\n");
	    int retcode = gerb_stats_increment_D_list_count(stats->D_code_list, 
							    a, 
							    1,
							    stats->error_list);
	    if (retcode == -1) {
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Found undefined D code: D%d\n", a),
				     GRB_ERROR);
		stats->D_unknown++;
	    }
	} else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Found aperture number out of bounds while parsing D code: %d\n", a),
				 GRB_ERROR);
	    stats->D_error++;
	}
	state->changed = 0;
	break;
    }
    
    return;
} /* parse_D_code */


/* ------------------------------------------------------------------ */
static int
parse_M_code(gerb_file_t *fd, gerb_image_t *image)
{
    int op_int;
    gerb_stats_t *stats = image->gerb_stats;
    
    op_int=gerb_fgetint(fd, NULL);
    
    switch (op_int) {
    case 0:  /* Program stop */
	stats->M0++;
	return 1;
    case 1:  /* Optional stop */
	stats->M1++;
	return 2;
    case 2:  /* End of program */
	stats->M2++;
	return 3;
    default:
	gerb_stats_add_error(stats->error_list,
			     -1,
			     g_strdup_printf("Encountered unknown M code : M%d\n", op_int),
			     GRB_ERROR);
	gerb_stats_add_error(stats->error_list,
			     -1,
			     g_strdup_printf("Ignorning unknown M code\n"),
			     WARNING);
	stats->M_unknown++;
    }
    return 0;
} /* parse_M_code */


/* ------------------------------------------------------------------ */
static void 
parse_rs274x(gint levelOfRecursion, gerb_file_t *fd, gerb_image_t *image, 
	     gerb_state_t *state, gerb_net_t *curr_net, gerb_stats_t *stats, 
	     gchar *directoryPath)
{
    int op[2];
    char str[3];
    int tmp;
    gerb_aperture_t *a = NULL;
    amacro_t *tmp_amacro;
    int ano;
    gdouble scale = 1.0;
    
    if (state->state->unit == MM)
    	scale = 25.4;
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	gerb_stats_add_error(stats->error_list,
			     -1,
			     "Unexpected EOF found.\n",
			     GRB_ERROR);

    switch (A2I(op[0], op[1])){
	
	/* 
	 * Directive parameters 
	 */
    case A2I('A','S'): /* Axis Select */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	state->state = gerb_image_return_new_netstate (state->state);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Unexpected EOF found.\n",
				 GRB_ERROR);
	
	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
	    state->state->axisSelect = SWAPAB;
	} else {
	    state->state->axisSelect = NOSELECT;
	}

	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Unexpected EOF found.\n",
				 GRB_ERROR);
	
	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
	    state->state->axisSelect = SWAPAB;
	} else {
	    state->state->axisSelect = NOSELECT;
	}
	break;

    case A2I('F','S'): /* Format Statement */
	image->format = g_new0 (gerb_format_t,1);
	
	switch (gerb_fgetc(fd)) {
	case 'L':
	    image->format->omit_zeros = LEADING;
	    break;
	case 'T':
	    image->format->omit_zeros = TRAILING;
	    break;
	case 'D':
	    image->format->omit_zeros = EXPLICIT;
	    break;
	default:
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "EagleCad bug detected: Undefined handling of zeros in format code\n", 
				 GRB_ERROR);
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Defaulting to omitting leading zeros.\n", 
				 WARNING);
	    gerb_ungetc(fd);
	    image->format->omit_zeros = LEADING;
	}
	
	switch (gerb_fgetc(fd)) {
	case 'A':
	    image->format->coordinate = ABSOLUTE;
	    break;
	case 'I':
	    image->format->coordinate = INCREMENTAL;
	    break;
	default:
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Invalid coordinate type defined in format code.\n", 
				 GRB_ERROR);
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Defaulting to absolute.\n", 
				 WARNING);
	    image->format->coordinate = ABSOLUTE;
	}
	op[0] = gerb_fgetc(fd);
	while((op[0] != '*')&&(op[0] != EOF)) {
	    switch (op[0]) {
	    case 'N':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_seqno = op[0] - '0';
		break;
	    case 'G':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_gf = op[0] - '0';
		break;
	    case 'D':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_pf = op[0] - '0';
		break;
	    case 'M':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_mf = op[0] - '0';
		break;
	    case 'X' :
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    gerb_stats_add_error(stats->error_list,
					 -1,
					 g_strdup_printf("Illegal format size : %c\n", (char)op[0]),
					 GRB_ERROR);
		image->format->x_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    gerb_stats_add_error(stats->error_list,
					 -1,
					 g_strdup_printf("Illegal format size : %c\n", (char)op[0]),
					 GRB_ERROR);
		image->format->x_dec = op[0] - '0';
		break;
	    case 'Y':
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    gerb_stats_add_error(stats->error_list,
					 -1,
					 g_strdup_printf("Illegal format size : %c\n", (char)op[0]),
					 GRB_ERROR);
		image->format->y_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    gerb_stats_add_error(stats->error_list,
					 -1,
					 g_strdup_printf("Illegal format size : %c\n", (char)op[0]),
					 GRB_ERROR);
		image->format->y_dec = op[0] - '0';
		break;
	    default :
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Invalid format statement [%c]\n", op[0]),
				     GRB_ERROR);
		gerb_stats_add_error(stats->error_list,
				     -1,
				     "Ignoring invalid format statement.\n",
				     WARNING);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('M','I'): /* Mirror Image */
	op[0] = gerb_fgetc(fd);
	state->state = gerb_image_return_new_netstate (state->state);
	
	while ((op[0] != '*')&&(op[0] != EOF)) {
            gint readValue=0;
	    switch (op[0]) {
	    case 'A' :
		readValue = gerb_fgetint(fd, NULL);
		if (readValue == 1) {
		    if (state->state->mirrorState == FLIPB)
			state->state->mirrorState=FLIPAB;
		    else
			state->state->mirrorState=FLIPA;
		}
		break;
	    case 'B' :
		readValue = gerb_fgetint(fd, NULL);
		if (readValue == 1) {
		    if (state->state->mirrorState == FLIPA)
			state->state->mirrorState=FLIPAB;
		    else
			state->state->mirrorState=FLIPB;
		}
		break;
	    default :
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Wrong character in mirror:%c\n", op[0]),
				     GRB_ERROR);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;  
    case A2I('M','O'): /* Mode of Units */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Unexpected EOF found.\n",
				 GRB_ERROR);
	switch (A2I(op[0],op[1])) {
	case A2I('I','N'):
	    state->state = gerb_image_return_new_netstate (state->state);
	    state->state->unit = INCH;
	    break;
	case A2I('M','M'):
	    state->state = gerb_image_return_new_netstate (state->state);
	    state->state->unit = MM;
	    break;
	default:
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Illegal unit:%c%c\n", op[0], op[1]),
				 GRB_ERROR);
	}
	break;
    case A2I('O','F'): /* Offset */
	op[0] = gerb_fgetc(fd);
	
	while ((op[0] != '*')&&(op[0] != EOF)) {
	    switch (op[0]) {
	    case 'A' :
		state->state->offsetA = gerb_fgetdouble(fd) / scale;
		break;
	    case 'B' :
		state->state->offsetB = gerb_fgetdouble(fd) / scale;
		break;
	    default :
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Wrong character in offset:%c\n", op[0]),
				     GRB_ERROR);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('I','F'): /* Include file */
	{
	    gchar *includeFilename = gerb_fgetstring(fd, '*');
	    
	    if (includeFilename) {
		gchar *fullPath;
		if (!g_path_is_absolute(includeFilename)) {
		    fullPath = g_build_filename (directoryPath, includeFilename, NULL);
		} else {
		    fullPath = g_strdup (includeFilename);
		}
		if (levelOfRecursion < 10) {
		    gerb_file_t *includefd = NULL;
		    
		    includefd = gerb_fopen(fullPath);
		    if (includefd) {
			gerber_parse_file_segment (levelOfRecursion + 1, image, state, curr_net, stats, includefd, directoryPath);
			gerb_fclose(includefd);
		    } else {
			gerb_stats_add_error(stats->error_list, -1,
					     g_strdup_printf("Included file cannot be found:%s\n",fullPath), GRB_ERROR);
		    }
		    g_free (fullPath);
		} else {
		    gerb_stats_add_error(stats->error_list, -1,
					 g_strdup_printf("Parser encountered more than 10 levels of include file recursion, which is not allowed by the RS-274X spec\n"), GRB_ERROR);
		}
		
	    }
	}
	break;
    case A2I('I','O'): /* Image offset */
	op[0] = gerb_fgetc(fd);
	
	while ((op[0] != '*')&&(op[0] != EOF)) {
	    switch (op[0]) {
	    case 'A' :
		image->info->offsetA = gerb_fgetdouble(fd) / scale;
		break;
	    case 'B' :
		image->info->offsetB = gerb_fgetdouble(fd) / scale;
		break;
	    default :
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Wrong character in offset:%c\n", op[0]),
				     GRB_ERROR);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('S','F'): /* Scale Factor */
	if (gerb_fgetc(fd) == 'A')
	    state->state->scaleA = gerb_fgetdouble(fd);
	else 
	    gerb_ungetc(fd);
	if (gerb_fgetc(fd) == 'B')
	    state->state->scaleB = gerb_fgetdouble(fd);
	else 
	    gerb_ungetc(fd);
	break;
    case A2I('I','C'): /* Input Code */
	/* Thanks to Stephen Adam for providing this information. As he writes:
	 *      btw, here's a logic puzzle for you.  If you need to
	 * read the gerber file to see how it's encoded, then
	 * how can you read it?
	 */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Unexpected EOF found.\n",
				 GRB_ERROR);
	switch (A2I(op[0],op[1])) {
	case A2I('A','S'):
	    image->info->encoding = ASCII;
	    break;
	case A2I('E','B'):
	    image->info->encoding = EBCDIC;
	    break;
	case A2I('B','C'):
	    image->info->encoding = BCD;
	    break;
	case A2I('I','S'):
	    image->info->encoding = ISO_ASCII;
	    break;
	case A2I('E','I'):
	    image->info->encoding = EIA;
	    break;
	default:
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Unknown input code (IC): %c%c\n", op[0], op[1]),
				 GRB_ERROR);
	}
	break;
	
	/* Image parameters */
    case A2I('I','J'): /* Image Justify */
	op[0] = gerb_fgetc(fd);
	image->info->imageJustifyTypeA = LOWERLEFT;
	image->info->imageJustifyTypeB = LOWERLEFT;
	image->info->imageJustifyOffsetA = 0.0;
	image->info->imageJustifyOffsetB = 0.0;
	while ((op[0] != '*')&&(op[0] != EOF)) {
	    switch (op[0]) {
	    case 'A' :
	    	op[0] = gerb_fgetc(fd);
	    	if (op[0] == 'C') {
		    image->info->imageJustifyTypeA = CENTERJUSTIFY;
	    	} else if (op[0] == 'L') {
		    image->info->imageJustifyTypeA = LOWERLEFT;
	    	} else {
		    gerb_ungetc (fd);
		    image->info->imageJustifyOffsetA = gerb_fgetdouble(fd) / scale;
	    	}
		break;
	    case 'B' :
		op[0] = gerb_fgetc(fd);
	    	if (op[0] == 'C') {
		    image->info->imageJustifyTypeB = CENTERJUSTIFY;
	    	} else if (op[0] == 'L') {
		    image->info->imageJustifyTypeB = LOWERLEFT;
	    	} else {
		    gerb_ungetc (fd);
		    image->info->imageJustifyOffsetB = gerb_fgetdouble(fd) / scale;
	    	}
		break;
	    default :
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Wrong character in image justify:%c\n", op[0]),
				     GRB_ERROR);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('I','N'): /* Image Name */
	image->info->name = gerb_fgetstring(fd, '*');
	break;
    case A2I('I','P'): /* Image Polarity */
	
	for (ano = 0; ano < 3; ano++) {
	    op[0] = gerb_fgetc(fd);
	    if (op[0] == EOF)
		gerb_stats_add_error(stats->error_list,
				     -1,
				     "Unexpected EOF while reading image polarity (IP)\n",
				     GRB_ERROR);
	    str[ano] = (char)op[0];
	}
	
	if (strncmp(str, "POS", 3) == 0) 
	    image->info->polarity = POSITIVE;
	else if (strncmp(str, "NEG", 3) == 0)
	    image->info->polarity = NEGATIVE;
	else 
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Unknown polarity : %c%c%c\n", str[0], str[1], str[2]),
				 GRB_ERROR);
	break;
    case A2I('I','R'): /* Image Rotation */
	tmp = gerb_fgetint(fd, NULL);
	if (tmp == 90)
	    image->info->imageRotation = M_PI / 2.0;
	else if (tmp == 180)
	    image->info->imageRotation = M_PI;
	else if (tmp == 270)
	    image->info->imageRotation = 3.0 * M_PI / 2.0;
	else 
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Image rotation must be 0, 90, 180 or 270 (is actually %d)\n", tmp),
				 GRB_ERROR);
	break;
    case A2I('P','F'): /* Plotter Film */
	image->info->plotterFilm = gerb_fgetstring(fd, '*');
	break;
	
	/* Aperture parameters */
    case A2I('A','D'): /* Aperture Description */
	a = (gerb_aperture_t *) g_new0 (gerb_aperture_t,1);

	ano = parse_aperture_definition(fd, a, image, scale);
	if ((ano >= APERTURE_MIN) && (ano <= APERTURE_MAX)) {
	    a->unit = state->state->unit;
	    image->aperture[ano] = a;
	    dprintf("     In parse_rs274x, adding new aperture to aperture list ...\n");
	    gerb_stats_add_aperture(stats->aperture_list,
				    -1, ano, 
				    a->type,
				    a->parameter);
	    gerb_stats_add_to_D_list(stats->D_code_list,
				     ano);
	} else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Aperture number out of bounds : %d\n", ano),
				 GRB_ERROR);
	}
	/* Add aperture info to stats->aperture_list here */
	
	break;
    case A2I('A','M'): /* Aperture Macro */
	tmp_amacro = image->amacro;
	image->amacro = parse_aperture_macro(fd);
	if (image->amacro) {
	    image->amacro->next = tmp_amacro;
#ifdef AMACRO_DEBUG
	    print_program(image->amacro);
#endif
	} else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Failed to parse aperture macro\n",
				 GRB_ERROR);
	}
	break;
	/* Layer */
    case A2I('L','N'): /* Layer Name */
	state->layer = gerb_image_return_new_layer (state->layer);
	state->layer->name = gerb_fgetstring(fd, '*');
	break;
    case A2I('L','P'): /* Layer Polarity */
	state->layer = gerb_image_return_new_layer (state->layer);
	switch (gerb_fgetc(fd)) {
	case 'D': /* Dark Polarity (default) */
	    state->layer->polarity = DARK;
	    break;
	case 'C': /* Clear Polarity */
	    state->layer->polarity = CLEAR;
	    break;
	default:
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Unknown Layer Polarity: %c\n", op[0]),
				 GRB_ERROR);
	}
	break;
    case A2I('K','O'): /* Knock Out */
        state->layer = gerb_image_return_new_layer (state->layer);
        gerber_update_any_running_knockout_measurements (image);
        /* reset any previous knockout measurements */
        knockoutMeasure = FALSE;
        op[0] = gerb_fgetc(fd);
	if (op[0] == '*') { /* Disable previous SR parameters */
	    state->layer->knockout.type = NOKNOCKOUT;
	    break;
	} else if (op[0] == 'C') {
	    state->layer->knockout.polarity = CLEAR;
	} else if (op[0] == 'D') {
	    state->layer->knockout.polarity = DARK;
	} else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Knockout must supply a polarity (C, D, or *)\n",
				 GRB_ERROR);
	}
	state->layer->knockout.lowerLeftX = 0.0;
	state->layer->knockout.lowerLeftY = 0.0;
	state->layer->knockout.width = 0.0;
	state->layer->knockout.height = 0.0;
	state->layer->knockout.border = 0.0;
	state->layer->knockout.firstInstance = TRUE;
	op[0] = gerb_fgetc(fd);
	while ((op[0] != '*')&&(op[0] != EOF)) { 
	    switch (op[0]) {
	    case 'X':
	        state->layer->knockout.type = FIXEDKNOCK;
		state->layer->knockout.lowerLeftX = gerb_fgetdouble(fd) / scale;
		break;
	    case 'Y':
	        state->layer->knockout.type = FIXEDKNOCK;
		state->layer->knockout.lowerLeftY = gerb_fgetdouble(fd) / scale;
		break;
	    case 'I':
	        state->layer->knockout.type = FIXEDKNOCK;
		state->layer->knockout.width = gerb_fgetdouble(fd) / scale;
		break;
	    case 'J':
	        state->layer->knockout.type = FIXEDKNOCK;
		state->layer->knockout.height = gerb_fgetdouble(fd) / scale;
		break;
	    case 'K':
	        state->layer->knockout.type = BORDER;
	        state->layer->knockout.border = gerb_fgetdouble(fd) / scale;
	        /* this is a bordered knockout, so we need to start measuring the
	           size of a square bordering all future components */
	        knockoutMeasure = TRUE;
	        knockoutLimitXmin = HUGE_VAL;
	        knockoutLimitYmin = HUGE_VAL;
	        knockoutLimitXmax = -HUGE_VAL;
	        knockoutLimitYmax = -HUGE_VAL;
	        knockoutLayer = state->layer;
	        break;
	    default:
		gerb_stats_add_error(stats->error_list,
				     -1,
				     "Unknown variable in knockout",
				     GRB_ERROR);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('S','R'): /* Step and Repeat */
        /* start by generating a new layer (duplicating previous layer settings */
        state->layer = gerb_image_return_new_layer (state->layer);
	op[0] = gerb_fgetc(fd);
	if (op[0] == '*') { /* Disable previous SR parameters */
	    state->layer->stepAndRepeat.X = 1;
	    state->layer->stepAndRepeat.Y = 1;
	    state->layer->stepAndRepeat.dist_X = 0.0;
	    state->layer->stepAndRepeat.dist_Y = 0.0;
	    break;
	}
	while ((op[0] != '*')&&(op[0] != EOF)) { 
	    switch (op[0]) {
	    case 'X':
		state->layer->stepAndRepeat.X = gerb_fgetint(fd, NULL);
		break;
	    case 'Y':
		state->layer->stepAndRepeat.Y = gerb_fgetint(fd, NULL);
		break;
	    case 'I':
		state->layer->stepAndRepeat.dist_X = gerb_fgetdouble(fd) / scale;
		break;
	    case 'J':
		state->layer->stepAndRepeat.dist_Y = gerb_fgetdouble(fd) / scale;
		break;
	    default:
		gerb_stats_add_error(stats->error_list,
				     -1,
				     "Step-and-repeat parameter error\n",
				     GRB_ERROR);
	    }
	    
	    /*
	     * Repeating 0 times in any direction would disable the whole plot, and
	     * is probably not intended. At least one other tool (viewmate) seems
	     * to interpret 0-time repeating as repeating just once too.
	     */
	    if(state->layer->stepAndRepeat.X == 0)
		state->layer->stepAndRepeat.X = 1;
	    if(state->layer->stepAndRepeat.Y == 0)
		state->layer->stepAndRepeat.Y = 1;
	    
	    op[0] = gerb_fgetc(fd);
	}
	break;
	/* is this an actual RS274X command??  It isn't explainined in the spec... */
    case A2I('R','O'):
	state->layer = gerb_image_return_new_layer (state->layer);
	
	state->layer->rotation = gerb_fgetdouble(fd) * M_PI / 180;
	op[0] = gerb_fgetc(fd);
	if (op[0] != '*') {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Error in layer rotation comman\n",
				 GRB_ERROR);
	}
	break;
    default:
	gerb_stats_add_error(stats->error_list,
			     -1,
			     g_strdup_printf("Unknown RS-274X extension found %%%c%c%%\n", op[0], op[1]),
			     GRB_ERROR);
    }
    
    return;
} /* parse_rs274x */


/*
 * Stack declarations and operations to be used by the simple engine that
 * executes the parsed aperture macros.
 */
typedef struct {
    double *stack;
    int sp;
} macro_stack_t;


static macro_stack_t *
new_stack(unsigned int stack_size)
{
    macro_stack_t *s;

    s = (macro_stack_t *) g_new0 (macro_stack_t,1);
    s->stack = (double *) g_new0 (double, stack_size);
    s->sp = 0;
    return s;
} /* new_stack */


static void
free_stack(macro_stack_t *s)
{
    if (s && s->stack)
	free(s->stack);

    if (s)
	free(s);

    return;
} /* free_stack */


static void
push(macro_stack_t *s, double val)
{
    s->stack[s->sp++] = val;
    return;
} /* push */


static int
pop(macro_stack_t *s, double *value)
{
    /* Check if we try to pop an empty stack */
    if (s->sp == 0) {
	return -1;
    }
    
    *value = s->stack[--s->sp];
    return 0;
} /* pop */


/* ------------------------------------------------------------------ */
static int
simplify_aperture_macro(gerb_aperture_t *aperture, gdouble scale)
{
    const int extra_stack_size = 10;
    macro_stack_t *s;
    instruction_t *ip;
    int handled = 1, nuf_parameters = 0, i, j, clearOperatorUsed = FALSE;
    double *lp; /* Local copy of parameters */
    double tmp[2] = {0.0, 0.0};
    enum aperture_t type = APERTURE_NONE;
    gerb_simplified_amacro_t *sam;

    if (aperture == NULL)
	GERB_FATAL_ERROR("aperture NULL in simplify aperture macro\n");

    if (aperture->amacro == NULL)
	GERB_FATAL_ERROR("aperture->amacro NULL in simplify aperture macro\n");

    /* Allocate stack for VM */
    s = new_stack(aperture->amacro->nuf_push + extra_stack_size);
    if (s == NULL) 
	GERB_FATAL_ERROR("malloc stack failed\n");

    /* Make a copy of the parameter list that we can rewrite if necessary */
    lp = (double *)malloc(sizeof(double) * APERTURE_PARAMETERS_MAX);
    if (lp == NULL)
	GERB_FATAL_ERROR("malloc local parameter storage failed\n");

    memcpy(lp, aperture->parameter, sizeof(double) * APERTURE_PARAMETERS_MAX);
    
    for(ip = aperture->amacro->program; ip != NULL; ip = ip->next) {
	switch(ip->opcode) {
	case NOP:
	    break;
	case PUSH :
	    push(s, ip->data.fval);
	    break;
        case PPUSH :
	    push(s, lp[ip->data.ival - 1]);
	    break;
	case PPOP:
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    lp[ip->data.ival - 1] = tmp[0];
	    break;
	case ADD :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    push(s, tmp[1] + tmp[0]);
	    break;
	case SUB :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    push(s, tmp[1] - tmp[0]);
	    break;
	case MUL :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    push(s, tmp[1] * tmp[0]);
	    break;
	case DIV :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR("Tried to pop an empty stack");
	    push(s, tmp[1] / tmp[0]);
	    break;
	case PRIM :
	    /* 
	     * This handles the exposure thing in the aperture macro
	     * The exposure is always the first element on stack independent
	     * of aperture macro.
	     */
	    switch(ip->data.ival) {
	    case 1:
		dprintf("  Aperture macro circle [1] (");
		type = MACRO_CIRCLE;
		nuf_parameters = 4;
		break;
	    case 3:
		break;
	    case 4 :
		dprintf("  Aperture macro outline [4] (");
		type = MACRO_OUTLINE;
		/*
		 * Number of parameters are:
		 * - number of points defined in entry 1 of the stack + 
		 *   start point. Times two since it is both X and Y.
		 * - Then three more; exposure,  nuf points and rotation.
		 */
		nuf_parameters = ((int)s->stack[1] + 1) * 2 + 3;
		break;
	    case 5 :
		dprintf("  Aperture macro polygon [5] (");
		type = MACRO_POLYGON;
		nuf_parameters = 6;
		break;
	    case 6 :
		dprintf("  Aperture macro moiré [6] (");
		type = MACRO_MOIRE;
		nuf_parameters = 9;
		break;
	    case 7 :
		dprintf("  Aperture macro thermal [7] (");
		type = MACRO_THERMAL;
		nuf_parameters = 6;
		break;
	    case 2  :
	    case 20 :
		dprintf("  Aperture macro line 20/2 (");
		type = MACRO_LINE20;
		nuf_parameters = 7;
		break;
	    case 21 :
		dprintf("  Aperture macro line 21 (");
		type = MACRO_LINE21;
		nuf_parameters = 6;
		break;
	    case 22 :
		dprintf("  Aperture macro line 22 (");
		type = MACRO_LINE22;
		nuf_parameters = 6;
		break;
	    default :
		handled = 0;
	    }

	    if (type != APERTURE_NONE) { 
		if (nuf_parameters > APERTURE_PARAMETERS_MAX) {
		    GERB_COMPILE_ERROR("Number of parameters to aperture macro are more than gerbv is able to store\n");
		}

		/*
		 * Create struct for simplified aperture macro and
		 * start filling in the blanks.
		 */
		sam = (gerb_simplified_amacro_t *)malloc(sizeof(gerb_simplified_amacro_t));
		if (sam == NULL)
		    GERB_FATAL_ERROR("Failed to malloc simplified aperture macro\n");
		sam->type = type;
		sam->next = NULL;
		memset(sam->parameter, 0, 
		       sizeof(double) * APERTURE_PARAMETERS_MAX);
		memcpy(sam->parameter, s->stack, 
		       sizeof(double) *  nuf_parameters);
		
		/* convert any mm values to inches */
		switch (type) {
		    case MACRO_CIRCLE:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			break;
		    case MACRO_OUTLINE:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			for (j=2; j<nuf_parameters-1; j++){
			    sam->parameter[j]/=scale;
			}
			break;
		    case MACRO_POLYGON:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			break;
		    case MACRO_MOIRE:
			sam->parameter[0]/=scale;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			sam->parameter[6]/=scale;
			sam->parameter[7]/=scale;
			break;
		    case MACRO_THERMAL:
			sam->parameter[0]/=scale;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			break;
		    case MACRO_LINE20:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			sam->parameter[5]/=scale;
			break;
		    case MACRO_LINE21:
		    case MACRO_LINE22:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			break;
		    default: 
			break;			
		}
		/* 
		 * Add this simplified aperture macro to the end of the list
		 * of simplified aperture macros. If first entry, put it
		 * in the top.
		 */
		if (aperture->simplified == NULL) {
		    aperture->simplified = sam;
		} else {
		    gerb_simplified_amacro_t *tmp_sam;
		    tmp_sam = aperture->simplified;
		    while (tmp_sam->next != NULL) {
			tmp_sam = tmp_sam->next;
		    }
		    tmp_sam->next = sam;
		}

#ifdef DEBUG
		for (i = 0; i < nuf_parameters; i++) {
		    dprintf("%f, ", s->stack[i]);
		}
#endif /* DEBUG */
		dprintf(")\n");
	    }

	    /* 
	     * Here we reset the stack pointer. It's not general correct
	     * correct to do this, but since I know how the compiler works
	     * I can do this. The correct way to do this should be to 
	     * subtract number of used elements in each primitive operation.
	     */
	    s->sp = 0;
	    break;
	default :
	    break;
	}
    }
    free_stack(s);

    /* store a flag to let the renderer know if it should expect any "clear"
       primatives */
    aperture->parameter[0]= (gdouble) clearOperatorUsed;
    return handled;
} /* simplify_aperture_macro */


/* ------------------------------------------------------------------ */
static int 
parse_aperture_definition(gerb_file_t *fd, gerb_aperture_t *aperture,
			  gerb_image_t *image, gdouble scale)
{
    int ano, i;
    char *ad;
    char *token;
    amacro_t *curr_amacro;
    amacro_t *amacro = image->amacro;
    gerb_stats_t *stats = image->gerb_stats;
    gdouble tempHolder;
    
    if (gerb_fgetc(fd) != 'D') {
	gerb_stats_add_error(stats->error_list,
			     -1,
			     g_strdup_printf("Found AD code with no following 'D'.\n"),
			     GRB_ERROR);	
	return -1;
    }
    
    /*
     * Get aperture no
     */
    ano = gerb_fgetint(fd, NULL);
    
    /*
     * Read in the whole aperture defintion and tokenize it
     */
    ad = gerb_fgetstring(fd, '*');
    token = strtok(ad, ",");
    
    if (strlen(token) == 1) {
	switch (token[0]) {
	case 'C':
	    aperture->type = CIRCLE;
	    break;
	case 'R' :
	    aperture->type = RECTANGLE;
	    break;
	case 'O' :
	    aperture->type = OVAL;
	    break;
	case 'P' :
	    aperture->type = POLYGON;
	    break;
	}
	/* Here a should a T be defined, but I don't know what it represents */
    } else {
	aperture->type = MACRO;
	/*
	 * In aperture definition, point to the aperture macro 
	 * used in the defintion
	 */
	curr_amacro = amacro;
	while (curr_amacro) {
	    if ((strlen(curr_amacro->name) == strlen(token)) &&
		(strcmp(curr_amacro->name, token) == 0)) {
		aperture->amacro = curr_amacro;
		break;
	    }
	    curr_amacro = curr_amacro->next;
	}
    }
    
    /*
     * Parse all parameters
     */
    for (token = strtok(NULL, "X"), i = 0; token != NULL; 
	 token = strtok(NULL, "X"), i++) {
	if (i == APERTURE_PARAMETERS_MAX) {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Maximum number of allowed parameters exceeded in aperture %d\n", ano),
				 GRB_ERROR);
	    break;
	}
	errno = 0;

	tempHolder = strtod(token, NULL);
	/* convert any MM values to inches */
	/* don't scale polygon angles or side numbers, or macro parmaeters */
	if (!(((aperture->type == POLYGON) && ((i==1) || (i==2)))||
		(aperture->type == MACRO))) {
	    tempHolder /= scale;
	}
	
	aperture->parameter[i] = tempHolder;
	if (errno) {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Failed to read all parameters exceeded in aperture %d\n", ano),
				 WARNING);
            aperture->parameter[i] = 0.0;
        }
    }
    
    aperture->nuf_parameters = i;
    
    gerb_ungetc(fd);

    if (aperture->type == MACRO) {
	dprintf("Simplifying aperture %d using aperture macro \"%s\"\n", ano,
		aperture->amacro->name);
	simplify_aperture_macro(aperture, scale);
	dprintf("Done simplifying\n");
    }
    
    g_free(ad);
    
    return ano;
} /* parse_aperture_definition */


/* ------------------------------------------------------------------ */
static void 
calc_cirseg_sq(struct gerb_net *net, int cw, 
	       double delta_cp_x, double delta_cp_y)
{
    double d1x, d1y, d2x, d2y;
    double alfa, beta;
    int quadrant = 0;

    
    /*
     * Quadrant detection (based on ccw, converted below if cw)
     *  Y ^
     *   /!\
     *    !
     *    ---->X
     */
    if (net->start_x > net->stop_x)
	/* 1st and 2nd quadrant */
	if (net->start_y < net->stop_y)
	    quadrant = 1;
	else
	    quadrant = 2;
    else
	/* 3rd and 4th quadrant */
	if (net->start_y > net->stop_y)
	    quadrant = 3;
	else
	    quadrant = 4;

    /* 
     * If clockwise, rotate quadrant
     */
    if (cw) {
	switch (quadrant) {
	case 1 : 
	    quadrant = 3;
	    break;
	case 2 : 
	    quadrant = 4;
	    break;
	case 3 : 
	    quadrant = 1;
	    break;
	case 4 : 
	    quadrant = 2;
	    break;
	default : 
	    GERB_COMPILE_ERROR("Unknow quadrant value while converting to cw\n");
	}
    }

    /*
     * Calculate arc center point
     */
    switch (quadrant) {
    case 1 :
	net->cirseg->cp_x = net->start_x - delta_cp_x;
	net->cirseg->cp_y = net->start_y - delta_cp_y;
	break;
    case 2 :
	net->cirseg->cp_x = net->start_x + delta_cp_x;
	net->cirseg->cp_y = net->start_y - delta_cp_y;
	break;
    case 3 : 
	net->cirseg->cp_x = net->start_x + delta_cp_x;
	net->cirseg->cp_y = net->start_y + delta_cp_y;
	break;
    case 4 :
	net->cirseg->cp_x = net->start_x - delta_cp_x;
	net->cirseg->cp_y = net->start_y + delta_cp_y;
	break;
    default :
	GERB_COMPILE_ERROR("Strange quadrant : %d\n", quadrant);
    }

    /*
     * Some good values 
     */
    d1x = fabs(net->start_x - net->cirseg->cp_x);
    d1y = fabs(net->start_y - net->cirseg->cp_y);
    d2x = fabs(net->stop_x - net->cirseg->cp_x);
    d2y = fabs(net->stop_y - net->cirseg->cp_y);

    alfa = atan2(d1y, d1x);
    beta = atan2(d2y, d2x);

    /*
     * Avoid divide by zero when sin(0) = 0 and cos(90) = 0
     */
    net->cirseg->width = alfa < beta ? 
	2 * (d1x / cos(alfa)) : 2 * (d2x / cos(beta));
    net->cirseg->height = alfa > beta ? 
	2 * (d1y / sin(alfa)) : 2 * (d2y / sin(beta));

    if (alfa < 0.000001 && beta < 0.000001) {
	net->cirseg->height = 0;
    }

#define RAD2DEG(a) (a * 180 / M_PI) 
    
    switch (quadrant) {
    case 1 :
	net->cirseg->angle1 = RAD2DEG(alfa);
	net->cirseg->angle2 = RAD2DEG(beta);
	break;
    case 2 :
	net->cirseg->angle1 = 180.0 - RAD2DEG(alfa);
	net->cirseg->angle2 = 180.0 - RAD2DEG(beta);
	break;
    case 3 : 
	net->cirseg->angle1 = 180.0 + RAD2DEG(alfa);
	net->cirseg->angle2 = 180.0 + RAD2DEG(beta);
	break;
    case 4 :
	net->cirseg->angle1 = 360.0 - RAD2DEG(alfa);
	net->cirseg->angle2 = 360.0 - RAD2DEG(beta);
	break;
    default :
	GERB_COMPILE_ERROR("Strange quadrant : %d\n", quadrant);
    }

    if (net->cirseg->width < 0.0)
	GERB_COMPILE_WARNING("Negative width [%f] in quadrant %d [%f][%f]\n", 
			     net->cirseg->width, quadrant, alfa, beta);
    
    if (net->cirseg->height < 0.0)
	GERB_COMPILE_WARNING("Negative height [%f] in quadrant %d [%f][%f]\n", 
			     net->cirseg->height, quadrant, RAD2DEG(alfa), RAD2DEG(beta));

    return;

} /* calc_cirseg_sq */


/* ------------------------------------------------------------------ */
static void 
calc_cirseg_mq(struct gerb_net *net, int cw, 
	       double delta_cp_x, double delta_cp_y)
{
    double d1x, d1y, d2x, d2y;
    double alfa, beta;

    net->cirseg->cp_x = net->start_x + delta_cp_x;
    net->cirseg->cp_y = net->start_y + delta_cp_y;

    /*
     * Some good values 
     */
    d1x = net->start_x - net->cirseg->cp_x;
    d1y = net->start_y - net->cirseg->cp_y;
    d2x = net->stop_x - net->cirseg->cp_x;
    d2y = net->stop_y - net->cirseg->cp_y;
    
    alfa = atan2(d1y, d1x);
    beta = atan2(d2y, d2x);

    net->cirseg->width = sqrt(delta_cp_x*delta_cp_x + delta_cp_y*delta_cp_y);
    net->cirseg->width *= 2.0;
    net->cirseg->height = net->cirseg->width;

    net->cirseg->angle1 = RAD2DEG(alfa);
    net->cirseg->angle2 = RAD2DEG(beta);

    /*
     * Make sure it's always positive angles
     */
    if (net->cirseg->angle1 < 0.0) {
	net->cirseg->angle1 += 360.0;
	net->cirseg->angle2 += 360.0;
    }

    if (net->cirseg->angle2 < 0.0) 
	net->cirseg->angle2 += 360.0;

    if(net->cirseg->angle2 == 0.0) 
	net->cirseg->angle2 = 360.0;

    /*
     * This is a sanity check for angles after the nature of atan2.
     * If cw we must make sure angle1-angle2 are always positive,
     * If ccw we must make sure angle2-angle1 are always negative.
     * We should really return one angle and the difference as GTK
     * uses them. But what the heck, it works for me.
     */
    if (cw) {
	if (net->cirseg->angle1 <= net->cirseg->angle2)
	    net->cirseg->angle2 -= 360.0;
    } else {
	if (net->cirseg->angle1 >= net->cirseg->angle2)
	    net->cirseg->angle2 += 360.0;
    }

    return;
} /* calc_cirseg_mq */


static void
gerber_update_any_running_knockout_measurements (gerb_image_t *image)
{
    if (knockoutMeasure) {
	knockoutLayer->knockout.lowerLeftX = knockoutLimitXmin;
	knockoutLayer->knockout.lowerLeftY = knockoutLimitYmin;
	knockoutLayer->knockout.width = knockoutLimitXmax - knockoutLimitXmin;
	knockoutLayer->knockout.height = knockoutLimitYmax - knockoutLimitYmin;
	knockoutMeasure = FALSE;
    }
}


static void
gerber_calculate_final_justify_effects(gerb_image_t *image)
{
    gdouble translateA = 0.0, translateB = 0.0;
    
    if (image->info->imageJustifyTypeA != NOJUSTIFY) {
	if (image->info->imageJustifyTypeA == CENTERJUSTIFY)
	    translateA = (image->info->max_x - image->info->min_x) / 2.0;
	else
	    translateA = -image->info->min_x;
    }
    if (image->info->imageJustifyTypeB != NOJUSTIFY) {
	if (image->info->imageJustifyTypeB == CENTERJUSTIFY)
	    translateB = (image->info->max_y - image->info->min_y) / 2.0;
	else
	    translateB = -image->info->min_y;
    }

    /* update the min/max values so the autoscale function can correctly
       centered a justified image */
    image->info->min_x += translateA+ image->info->imageJustifyOffsetA;
    image->info->max_x += translateA+ image->info->imageJustifyOffsetA;
    image->info->min_y += translateB+ image->info->imageJustifyOffsetB;
    image->info->max_y += translateB+ image->info->imageJustifyOffsetB;
 
    /* store the absolute offset for the justify so we can quickly offset
       the rendered picture during drawing */
    image->info->imageJustifyOffsetActualA = translateA + 
	image->info->imageJustifyOffsetA;
    image->info->imageJustifyOffsetActualB = translateB + 
	image->info->imageJustifyOffsetB;
} /* gerber_calculate_final_justify_effects */


static void
gerber_update_min_and_max(gerb_image_info_t *info, gdouble repeatX, gdouble repeatY,
			  gdouble x, gdouble y, gdouble apertureSizeX1,
			  gdouble apertureSizeX2,gdouble apertureSizeY1,
			  gdouble apertureSizeY2)
{
    gdouble ourX1 = x - apertureSizeX1, ourY1 = y - apertureSizeY1;
    gdouble ourX2 = x + apertureSizeX2, ourY2 = y + apertureSizeY2;
    
    if (repeatX > 0)
    	ourX2 += repeatX;
    if (repeatY > 0)
    	ourY2 += repeatY;
    
#ifndef RENDER_USING_GDK
    /* transform the point to the final rendered position, accounting
       for any scaling, offsets, mirroring, etc */
    /* NOTE: we need to already add/subtract in the aperture size since
       the final rendering may be scaled */
    cairo_matrix_transform_point (&currentMatrix, &ourX1, &ourY1);
    cairo_matrix_transform_point (&currentMatrix, &ourX2, &ourY2);
#endif

    /* check both points against the min/max, since depending on the rotation,
       mirroring, etc, either point could possibly be a min or max */
    if(info->min_x > ourX1)
	info->min_x = ourX1;
    if(info->min_x > ourX2)
	info->min_x = ourX2;
    if(info->max_x < ourX1)
	info->max_x = ourX1;
    if(info->max_x < ourX2)
	info->max_x = ourX2;
    if(info->min_y > ourY1)
	info->min_y = ourY1;
    if(info->min_y > ourY2)
	info->min_y = ourY2;
    if(info->max_y < ourY1)
	info->max_y = ourY1;
    if(info->max_y < ourY2)
	info->max_y = ourY2;

    if (knockoutMeasure) {
	if(knockoutLimitXmin > ourX1)
	    knockoutLimitXmin = ourX1;
	if(knockoutLimitXmin > ourX2)
	    knockoutLimitXmin = ourX2;
	if(knockoutLimitXmax < ourX1)
	    knockoutLimitXmax = ourX1;
	if(knockoutLimitXmax < ourX2)
	    knockoutLimitXmax = ourX2;
	if(knockoutLimitYmin > ourY1)
	    knockoutLimitYmin = ourY1;
	if(knockoutLimitYmin > ourY2)
	    knockoutLimitYmin = ourY2;
	if(knockoutLimitYmax < ourY1)
	    knockoutLimitYmax = ourY1;
	if(knockoutLimitYmax < ourY2)
	    knockoutLimitYmax = ourY2; 
    }
} /* gerber_update_min_and_max */

