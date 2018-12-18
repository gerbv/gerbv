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

/** \file gerber.c
    \brief RS274X parsing functions
    \ingroup libgerbv
*/

#include "gerbv.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>  /* pow() */
#include <errno.h>
#include <ctype.h>

#include "common.h"
#include "gerb_image.h"
#include "gerber.h"
#include "gerb_stats.h"
#include "amacro.h"

#undef AMACRO_DEBUG
#define dprintf if(DEBUG) printf

#define A2I(a,b) (((a & 0xff) << 8) + (b & 0xff))

#define MAXL 200

/* Local function prototypes */
static void parse_G_code(gerb_file_t *fd, gerb_state_t *state, 
			 gerbv_image_t *image, long int *line_num_p);
static void parse_D_code(gerb_file_t *fd, gerb_state_t *state, 
			 gerbv_image_t *image, long int *line_num_p);
static int parse_M_code(gerb_file_t *fd, gerbv_image_t *image,
			long int *line_num_p);
static void parse_rs274x(gint levelOfRecursion, gerb_file_t *fd, 
			 gerbv_image_t *image, gerb_state_t *state, 
			 gerbv_net_t *curr_net, gerbv_stats_t *stats, 
			 gchar *directoryPath, long int *line_num_p);
static int parse_aperture_definition(gerb_file_t *fd, 
				     gerbv_aperture_t *aperture,
				     gerbv_image_t *image, gdouble scale,
				     long int *line_num_p);
static void calc_cirseg_sq(struct gerbv_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);
static void calc_cirseg_mq(struct gerbv_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);
static void calc_cirseg_bbox(const gerbv_cirseg_t *cirseg,
			double apert_size_x, double apert_size_y,
			gerbv_render_size_t *bbox);

static void gerber_update_any_running_knockout_measurements(
							gerbv_image_t *image);

static void gerber_calculate_final_justify_effects (gerbv_image_t *image);

static gboolean add_trailing_zeros_if_omitted(int *coord, int omitted_num,
						gerbv_format_t *format);

gboolean knockoutMeasure = FALSE;
gdouble knockoutLimitXmin, knockoutLimitYmin,
	knockoutLimitXmax, knockoutLimitYmax;
gerbv_layer_t *knockoutLayer = NULL;
cairo_matrix_t currentMatrix;

/* --------------------------------------------------------- */
gerbv_net_t *
gerber_create_new_net (gerbv_net_t *currentNet, gerbv_layer_t *layer, gerbv_netstate_t *state){
	gerbv_net_t *newNet = g_new0 (gerbv_net_t, 1);
	
	currentNet->next = newNet;
	if (layer)
		newNet->layer = layer;
	else
		newNet->layer = currentNet->layer;
	if (state)
		newNet->state = state;
	else
		newNet->state = currentNet->state;
	return newNet;
}

/* --------------------------------------------------------- */
gboolean
gerber_create_new_aperture (gerbv_image_t *image, int *indexNumber,
		gerbv_aperture_type_t apertureType, gdouble parameter1, gdouble parameter2){
	int i;
	
	/* search for an available aperture spot */
	for (i = 0; i <= APERTURE_MAX; i++) {
		if (image->aperture[i] == NULL) {
			image->aperture[i] = g_new0 (gerbv_aperture_t, 1);
			image->aperture[i]->type = apertureType;
			image->aperture[i]->parameter[0] = parameter1;
			image->aperture[i]->parameter[1] = parameter2;
			*indexNumber = i;
			return TRUE;
		}
	}
	return FALSE;
}

/* --------------------------------------------------------- */
/*! This function reads the Gerber file char by char, looking
 *  for various Gerber codes (e.g. G, D, etc).  Once it reads 
 *  a code, it then dispatches control to one or another 
 *  bits of code which parse the individual code.
 *  It also updates the state struct, which holds info about
 *  the current state of the hypothetical photoplotter
 *  (i.e. updates whether the aperture is on or off, updates
 *  any other parameters, like units, offsets, apertures, etc.)
 */
gboolean
gerber_parse_file_segment (gint levelOfRecursion, gerbv_image_t *image, 
			   gerb_state_t *state,	gerbv_net_t *curr_net, 
			   gerbv_stats_t *stats, gerb_file_t *fd, 
			   gchar *directoryPath)
{
    int read, coord, len, polygonPoints=0;
    double x_scale = 0.0, y_scale = 0.0;
    double delta_cp_x = 0.0, delta_cp_y = 0.0;
    double aperture_sizeX, aperture_sizeY;
    double scale;
    gboolean foundEOF = FALSE;
    gerbv_render_size_t boundingBoxNew = {HUGE_VAL,-HUGE_VAL,HUGE_VAL,-HUGE_VAL},
			boundingBox = boundingBoxNew;
    gerbv_error_list_t *error_list = stats->error_list;
    long int line_num = 1;

    while ((read = gerb_fgetc(fd)) != EOF) {
        /* figure out the scale, since we need to normalize 
	   all dimensions to inches */
        if (state->state->unit == GERBV_UNIT_MM)
            scale = 25.4;
        else
            scale = 1.0;
	switch ((char)(read & 0xff)) {
	case 'G':
	    dprintf("... Found G code at line %ld\n", line_num);
	    parse_G_code(fd, state, image, &line_num);
	    break;
	case 'D':
	    dprintf("... Found D code at line %ld\n", line_num);
	    parse_D_code(fd, state, image, &line_num);
	    break;
	case 'M':
	    dprintf("... Found M code at line %ld\n", line_num);

	    switch(parse_M_code(fd, image, &line_num)) {
	    case 1 :
	    case 2 :
	    case 3 :
		foundEOF = TRUE;
		break;
	    default:
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unknown M code found at line %ld in file \"%s\""),
			line_num, fd->filename);
	    } /* switch(parse_M_code) */
	    break;
	case 'X':
	    stats->X++;
	    coord = gerb_fgetint(fd, &len);
	    if (image->format)
		    add_trailing_zeros_if_omitted(&coord,
			    image->format->x_int + image->format->x_dec - len,
			    image->format);
	    dprintf("... Found X code %d at line %ld\n", coord, line_num);
	    if (image->format
	    &&  image->format->coordinate==GERBV_COORDINATE_INCREMENTAL)
	        state->curr_x += coord;
	    else
	        state->curr_x = coord;

	    state->changed = 1;
	    break;

	case 'Y':
	    stats->Y++;
	    coord = gerb_fgetint(fd, &len);
	    if (image->format)
		    add_trailing_zeros_if_omitted(&coord,
			    image->format->y_int + image->format->y_dec - len,
			    image->format);
	    dprintf("... Found Y code %d at line %ld\n", coord, line_num);
	    if (image->format
	    &&  image->format->coordinate==GERBV_COORDINATE_INCREMENTAL)
	        state->curr_y += coord;
	    else
	        state->curr_y = coord;

	    state->changed = 1;
	    break;

	case 'I':
	    stats->I++;
	    coord = gerb_fgetint(fd, &len);
	    if (image->format)
		    add_trailing_zeros_if_omitted(&coord,
			    image->format->x_int + image->format->x_dec - len,
			    image->format);
	    dprintf("... Found I code %d at line %ld\n", coord, line_num);
	    state->delta_cp_x = coord;
	    state->changed = 1;
	    break;

	case 'J':
	    stats->J++;
	    coord = gerb_fgetint(fd, &len);
	    if (image->format)
		    add_trailing_zeros_if_omitted(&coord,
			    image->format->y_int + image->format->y_dec - len,
			    image->format);
	    dprintf("... Found J code %d at line %ld\n", coord, line_num);
	    state->delta_cp_y = coord;
	    state->changed = 1;
	    break;

	case '%':
	    dprintf("... Found %% code at line %ld\n", line_num);
	    while (1) {
	    	parse_rs274x(levelOfRecursion, fd, image, state, curr_net,
				stats, directoryPath, &line_num);

	    	/* advance past any whitespace here */
		int c;
		while (1) {
		    c = gerb_fgetc(fd);

		    switch (c) {
		    case '\0': case '\t': case ' ':

			continue;

		    case '\n':
			line_num++;

			/* Get <CR> char, if any, from <LF><CR> pair */
			read = gerb_fgetc(fd);
			if (read != '\r' && read != EOF)
			    gerb_ungetc(fd);

			continue;

		    case '\r':
			line_num++;

			/* Get <LF> char, if any, from <CR><LF> pair */
			read = gerb_fgetc(fd);
			if (read != '\n' && read != EOF)
			    gerb_ungetc(fd);

			continue;
		    }

		    break; /* break while(1) */
		};

		if(c == EOF || c == '%')
		    break;

		/* Loop again to catch multiple blocks on the same line
		 * (separated by * char) */
		gerb_ungetc(fd);
	    }
	    break;
	case '*':  
	    dprintf("... Found * code at line %ld\n", line_num);
	    stats->star++;
	    if (state->changed == 0) break;
	    state->changed = 0;
	    
	    /* don't even bother saving the net if the aperture state is GERBV_APERTURE_STATE_OFF and we
	       aren't starting a polygon fill (where we need it to get to the start point) */
	    if ((state->aperture_state == GERBV_APERTURE_STATE_OFF)&&(!state->in_parea_fill)&&
	    		(state->interpolation != GERBV_INTERPOLATION_PAREA_START)) {
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
	    case GERBV_INTERPOLATION_CW_CIRCULAR :
	    case GERBV_INTERPOLATION_CCW_CIRCULAR : {
		int cw = (state->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR);

		curr_net->cirseg = g_new0 (gerbv_cirseg_t, 1);
		if (state->mq_on) {
		    calc_cirseg_mq(curr_net, cw, delta_cp_x, delta_cp_y);
		} else {
		    calc_cirseg_sq(curr_net, cw, delta_cp_x, delta_cp_y);

		    /*
		     * In single quadrant circular interpolation Ix and Jy
		     * incremental distance must be unsigned.
		     */
		    if (delta_cp_x < 0 || delta_cp_y < 0) {
			gerbv_stats_printf(error_list,
				GERBV_MESSAGE_ERROR, -1, 
				_("Signed incremental distance IxJy "
				    "in single quadrant %s circular "
				    "interpolation %s at line %ld "
				    "in file \"%s\""),
				cw? _("CW"): _("CCW"), cw? "G02": "G03",
				line_num, fd->filename);
		    }

		}
		break;
	    }
	    case GERBV_INTERPOLATION_PAREA_START :
		/* 
		 * To be able to get back and fill in number of polygon corners
		 */
		state->parea_start_node = curr_net;
		state->in_parea_fill = 1;
		polygonPoints = 0;
		boundingBox = boundingBoxNew;
		break;
	    case GERBV_INTERPOLATION_PAREA_END :
		/* save the calculated bounding box to the master node */
		if (state->parea_start_node != NULL) {
		    state->parea_start_node->boundingBox = boundingBox;
		} else {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("End of polygon without start "
				"at line %ld in file \"%s\""),
			    line_num, fd->filename);
		}

		/* close out the polygon */
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
		 * D02 -> state->aperture_state == GERBV_APERTURE_STATE_OFF
		 */
		 
		 /* UPDATE: only end the polygon during a D02 call if we've already
		    drawn a polygon edge (with D01) */
		   
		if (state->aperture_state == GERBV_APERTURE_STATE_OFF
		&&  state->interpolation  != GERBV_INTERPOLATION_PAREA_START
		&&  polygonPoints > 0) {
		    curr_net->interpolation = GERBV_INTERPOLATION_PAREA_END;
		    curr_net = gerber_create_new_net (curr_net, state->layer, state->state);
		    curr_net->interpolation = GERBV_INTERPOLATION_PAREA_START;
		    state->parea_start_node->boundingBox = boundingBox;
		    state->parea_start_node = curr_net;
		    polygonPoints = 0;
		    curr_net = gerber_create_new_net (curr_net, state->layer, state->state);		    
		    curr_net->start_x = (double)state->prev_x / x_scale;
		    curr_net->start_y = (double)state->prev_y / y_scale;
		    curr_net->stop_x = (double)state->curr_x / x_scale;
		    curr_net->stop_y = (double)state->curr_y / y_scale;
		    boundingBox = boundingBoxNew;
		}
		else if (state->interpolation != GERBV_INTERPOLATION_PAREA_START)
		    polygonPoints++;
		
	    }  /* if (state->in_parea_fill && state->parea_start_node) */
	    
	    curr_net->interpolation = state->interpolation;

	    /* 
	     * Override circular interpolation if no center was given.
	     * This should be a safe hack, since a good file should always 
	     * include I or J.  And even if the radius is zero, the endpoint 
	     * should be the same as the start point, creating no line 
	     */
	    if (((state->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR) || 
		 (state->interpolation == GERBV_INTERPOLATION_CCW_CIRCULAR)) && 
		((state->delta_cp_x == 0.0) && (state->delta_cp_y == 0.0)))
		curr_net->interpolation = GERBV_INTERPOLATION_LINEARx1;
	    
	    /*
	     * If we detected the end of Polygon Area Fill we go back to
	     * the interpolation we had before that.
	     * Also if we detected any of the quadrant flags, since some
	     * gerbers don't reset the interpolation (EagleCad again).
	     */
	    if ((state->interpolation == GERBV_INTERPOLATION_PAREA_START
	    ||   state->interpolation == GERBV_INTERPOLATION_PAREA_END)
	    && state->prev_interpolation != GERBV_INTERPOLATION_PAREA_END) {
		state->interpolation = state->prev_interpolation;
	    }

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
	    
	    /* only update the min/max values and aperture stats if we are drawing */
	    if ((curr_net->aperture_state != GERBV_APERTURE_STATE_OFF)&&
	    		(curr_net->interpolation != GERBV_INTERPOLATION_PAREA_START)){
		double repeat_off_X = 0.0, repeat_off_Y = 0.0;

		/* Update stats with current aperture number if not in polygon */
		if (!state->in_parea_fill) {
			dprintf("     In %s(), adding 1 to D_list ...\n",
					__func__);
			int retcode = gerbv_stats_increment_D_list_count(
				stats->D_code_list, curr_net->aperture,
				1, error_list);
			if (retcode == -1) {
			    gerbv_stats_printf(error_list,
				    GERBV_MESSAGE_ERROR, -1,
				    _("Found undefined D code D%02d "
					"at line %ld in file \"%s\""),
				    curr_net->aperture, line_num, fd->filename);
			    stats->D_unknown++;
			}
		}

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
		case GERBV_MIRROR_STATE_FLIPA:
		    cairo_matrix_scale (&currentMatrix, -1, 1);
		    break;
		case GERBV_MIRROR_STATE_FLIPB:
		    cairo_matrix_scale (&currentMatrix, 1, -1);
		    break;
		case GERBV_MIRROR_STATE_FLIPAB:
		    cairo_matrix_scale (&currentMatrix, -1, -1);
		    break;
		default:
		    break;
		}
		/* finally, apply axis select */
		if (state->state->axisSelect == GERBV_AXIS_SELECT_SWAPAB) {
		    /* we do this by rotating 270 (counterclockwise, then 
		     *  mirroring the Y axis 
		     */
		    cairo_matrix_rotate (&currentMatrix, M_PI + M_PI_2);
		    cairo_matrix_scale (&currentMatrix, 1, -1);
		}
		/* if it's a macro, step through all the primitive components
		   and calculate the true bounding box */
		if ((image->aperture[curr_net->aperture] != NULL) &&
		    (image->aperture[curr_net->aperture]->type == GERBV_APTYPE_MACRO)) {
		    gerbv_simplified_amacro_t *ls = image->aperture[curr_net->aperture]->simplified;
	      
		    while (ls != NULL) {
			gdouble offsetx = 0, offsety = 0, widthx = 0, widthy = 0;
			gboolean calculatedAlready = FALSE;
			
			if (ls->type == GERBV_APTYPE_MACRO_CIRCLE) {
			    offsetx=ls->parameter[CIRCLE_CENTER_X];
			    offsety=ls->parameter[CIRCLE_CENTER_Y];
			    widthx=widthy=ls->parameter[CIRCLE_DIAMETER];
			} else if (ls->type == GERBV_APTYPE_MACRO_OUTLINE) {
			    int pointCounter,numberOfPoints;
			    numberOfPoints = ls->parameter[OUTLINE_NUMBER_OF_POINTS] + 1;
		
			    for (pointCounter = 0; pointCounter < numberOfPoints; pointCounter++) {
				gerber_update_min_and_max (&boundingBox,
							   curr_net->stop_x +
							   ls->parameter[OUTLINE_X_IDX_OF_POINT(pointCounter)],
							   curr_net->stop_y +
							   ls->parameter[OUTLINE_Y_IDX_OF_POINT(pointCounter)], 
							   0,0,0,0);
			    }
			    calculatedAlready = TRUE;
			} else if (ls->type == GERBV_APTYPE_MACRO_POLYGON) {
			    offsetx = ls->parameter[POLYGON_CENTER_X];
			    offsety = ls->parameter[POLYGON_CENTER_Y];
			    widthx = widthy = ls->parameter[POLYGON_DIAMETER];
			} else if (ls->type == GERBV_APTYPE_MACRO_MOIRE) {
			    offsetx = ls->parameter[MOIRE_CENTER_X];
			    offsety = ls->parameter[MOIRE_CENTER_Y];
			    widthx = widthy = ls->parameter[MOIRE_OUTSIDE_DIAMETER];
			} else if (ls->type == GERBV_APTYPE_MACRO_THERMAL) {
			    offsetx = ls->parameter[THERMAL_CENTER_X];
			    offsety = ls->parameter[THERMAL_CENTER_Y];
			    widthx = widthy = ls->parameter[THERMAL_OUTSIDE_DIAMETER];
			} else if (ls->type == GERBV_APTYPE_MACRO_LINE20) {
			    widthx = widthy = ls->parameter[LINE20_LINE_WIDTH];
			    gerber_update_min_and_max (&boundingBox,
						       curr_net->stop_x +
						       ls->parameter[LINE20_START_X],
						       curr_net->stop_y +
						       ls->parameter[LINE20_START_Y], 
						       widthx/2,widthx/2,widthy/2,widthy/2);
			    gerber_update_min_and_max (&boundingBox,
						       curr_net->stop_x +
						       ls->parameter[LINE20_END_X],
						       curr_net->stop_y +
						       ls->parameter[LINE20_END_Y], 
						       widthx/2,widthx/2,widthy/2,widthy/2);
			    calculatedAlready = TRUE;
			} else if (ls->type == GERBV_APTYPE_MACRO_LINE21) {
			    gdouble largestDimension = hypot(ls->parameter[LINE21_WIDTH],
							     ls->parameter[LINE21_HEIGHT]);
			    offsetx = ls->parameter[LINE21_CENTER_X];
			    offsety = ls->parameter[LINE21_CENTER_Y];
			    widthx = widthy = largestDimension;
			} else if (ls->type == GERBV_APTYPE_MACRO_LINE22) {
			    gdouble largestDimension = hypot(ls->parameter[LINE22_WIDTH],
							     ls->parameter[LINE22_HEIGHT]);

			    offsetx = ls->parameter[LINE22_LOWER_LEFT_X] +
	      			ls->parameter[LINE22_WIDTH]/2;
			    offsety = ls->parameter[LINE22_LOWER_LEFT_Y] +
	      			ls->parameter[LINE22_HEIGHT]/2;
			    widthx = widthy=largestDimension;
			}
	      	
			if (!calculatedAlready) {
			    gerber_update_min_and_max (&boundingBox,
						       curr_net->stop_x + offsetx,
						       curr_net->stop_y + offsety, 
						       widthx/2,widthx/2,widthy/2,widthy/2);
			}
	    		ls = ls->next;
		    }
		} else {
		    if (image->aperture[curr_net->aperture] != NULL) {
			aperture_sizeX = image->aperture[curr_net->aperture]->parameter[0];
			if ((image->aperture[curr_net->aperture]->type == GERBV_APTYPE_RECTANGLE) || (image->aperture[curr_net->aperture]->type == GERBV_APTYPE_OVAL)) {
				aperture_sizeY = image->aperture[curr_net->aperture]->parameter[1];
			}
			else
				aperture_sizeY = aperture_sizeX;
		    } else {
			/* this is usually for polygon fills, where the aperture width
			   is "zero" */
			aperture_sizeX = aperture_sizeY = 0;
		    }

		    /* if it's an arc path, use a special calc */

		    if ((curr_net->interpolation ==
					GERBV_INTERPOLATION_CW_CIRCULAR) ||
			(curr_net->interpolation ==
					GERBV_INTERPOLATION_CCW_CIRCULAR)) {
				calc_cirseg_bbox(curr_net->cirseg,
						aperture_sizeX, aperture_sizeY,
						&boundingBox);
		    } else {
			    /* check both the start and stop of the aperture points against
			       a running min/max counter */
			    /* Note: only check start coordinate if this isn't a flash, 
			       since the start point may be bogus if it is a flash */
			    if (curr_net->aperture_state != GERBV_APERTURE_STATE_FLASH) {
				gerber_update_min_and_max (&boundingBox,
							   curr_net->start_x, curr_net->start_y, 
							   aperture_sizeX/2,aperture_sizeX/2,
							   aperture_sizeY/2,aperture_sizeY/2);
			    }
			    gerber_update_min_and_max (&boundingBox,
						       curr_net->stop_x, curr_net->stop_y, 
						       aperture_sizeX/2,aperture_sizeX/2,
						       aperture_sizeY/2,aperture_sizeY/2);
		    }
					     
		}
		/* update the info bounding box with this latest bounding box */
		/* don't change the bounding box if the polarity is clear */
		if (state->layer->polarity != GERBV_POLARITY_CLEAR){
			gerber_update_image_min_max(&boundingBox, repeat_off_X, repeat_off_Y, image);
		}
		/* optionally update the knockout measurement box */
		if (knockoutMeasure) {
			if (boundingBox.left < knockoutLimitXmin)
				knockoutLimitXmin = boundingBox.left;
			if (boundingBox.right+repeat_off_X > knockoutLimitXmax)
				knockoutLimitXmax = boundingBox.right+repeat_off_X;
			if (boundingBox.bottom < knockoutLimitYmin)
				knockoutLimitYmin = boundingBox.bottom;
			if (boundingBox.top+repeat_off_Y > knockoutLimitYmax)
				knockoutLimitYmax = boundingBox.top+repeat_off_Y;
		}
		/* if we're not in a polygon fill, then update the object bounding box */
		if (!state->in_parea_fill) {
			curr_net->boundingBox = boundingBox;
			boundingBox = boundingBoxNew;
		}
	    }
	    break;

	case '\0': case '\t': case ' ':
	    break;

	case '\n':
	    line_num++;

	    /* Get <CR> char, if any, from <LF><CR> pair */
	    read = gerb_fgetc(fd);
	    if (read != '\r' && read != EOF)
		    gerb_ungetc(fd);
	    break;

	case '\r':
	    line_num++;

	    /* Get <LF> char, if any, from <CR><LF> pair */
	    read = gerb_fgetc(fd);
	    if (read != '\n' && read != EOF)
		    gerb_ungetc(fd);
	    break;

	default:
	    stats->unknown++;
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Found unknown character '%s' (0x%x) "
			"at line %ld in file \"%s\""),
		    gerbv_escape_char(read), read,
		    line_num, fd->filename);
	}  /* switch((char) (read & 0xff)) */
    }
    return foundEOF;
}


/* ------------------------------------------------------------------ */
/*! This is a wrapper which gets called from top level.  It 
 *  does some initialization and pre-processing, and 
 *  then calls gerber_parse_file_segment
 *  which processes the actual file.  Then it does final 
 *  modifications to the image created.
 */
gerbv_image_t *
parse_gerb(gerb_file_t *fd, gchar *directoryPath)
{
    gerb_state_t *state = NULL;
    gerbv_image_t *image = NULL;
    gerbv_net_t *curr_net = NULL;
    gerbv_stats_t *stats;
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
    image = gerbv_create_image(image, "RS274-X (Gerber) File");
    if (image == NULL)
	GERB_FATAL_ERROR("malloc image failed in %s()", __FUNCTION__);
    curr_net = image->netlist;
    image->layertype = GERBV_LAYERTYPE_RS274X;
    image->gerbv_stats = gerbv_stats_new();
    if (image->gerbv_stats == NULL)
	GERB_FATAL_ERROR("malloc gerbv_stats failed in %s()", __FUNCTION__);

    stats = image->gerbv_stats;

    /* set active layer and netstate to point to first default one created */
    state->layer = image->layers;
    state->state = image->states;
    curr_net->layer = state->layer;
    curr_net->state = state->state;

    /*
     * Start parsing
     */
    dprintf("In %s(), starting to parse file...\n", __func__);
    foundEOF = gerber_parse_file_segment (1, image, state, curr_net, stats,
					  fd, directoryPath);

    if (!foundEOF) {
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Missing Gerber EOF code in file \"%s\""), fd->filename);
    }
    g_free(state);
    
    dprintf("               ... done parsing Gerber file\n");
    gerber_update_any_running_knockout_measurements (image);
    gerber_calculate_final_justify_effects(image);

    return image;
} /* parse_gerb */


/* ------------------------------------------------------------------- */
/*! Checks for signs that this is a RS-274X file
 *  Returns TRUE if it is, FALSE if not.
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
   
    dprintf ("%s(%p, %p), fd->fd = %p\n",
		    __func__, fd, returnFoundBinary, fd->fd); 
    buf = (char *) g_malloc(MAXL);
    if (buf == NULL) 
	GERB_FATAL_ERROR("malloc buf failed while checking for rs274x in %s()",
			__FUNCTION__);
    
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
	if (g_strstr_len(buf, len, "D00") || g_strstr_len(buf, len, "D0")) {
	    found_D0 = TRUE;
            dprintf ("found_D0\n");
	}
	if (g_strstr_len(buf, len, "D02") || g_strstr_len(buf, len, "D2")) {
	    found_D2 = TRUE;
            dprintf ("found_D2\n");
	}
	if (g_strstr_len(buf, len, "M00") || g_strstr_len(buf, len, "M0")) {
	    found_M0 = TRUE;
            dprintf ("found_M0\n");
	}
	if (g_strstr_len(buf, len, "M02") || g_strstr_len(buf, len, "M2")) {
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
/*! Checks for signs that this is a RS-274D file
 *  Returns TRUE if it is, FALSE if not.
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
	GERB_FATAL_ERROR("malloc buf failed while checking for rs274d in %s()",
			__FUNCTION__);

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
	if (g_strstr_len(buf, len, "D00") || g_strstr_len(buf, len, "D0")) {
	    found_D0 = TRUE;
	}
	if (g_strstr_len(buf, len, "D02") || g_strstr_len(buf, len, "D2")) {
	    found_D2 = TRUE;
	}
	if (g_strstr_len(buf, len, "M00") || g_strstr_len(buf, len, "M0")) {
	    found_M0 = TRUE;
	}
	if (g_strstr_len(buf, len, "M02") || g_strstr_len(buf, len, "M2")) {
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
/*! This function reads a G number and updates the current
 *  state.  It also updates the G stats counters
 */
static void 
parse_G_code(gerb_file_t *fd, gerb_state_t *state,
		gerbv_image_t *image, long int *line_num_p)
{
    int  op_int;
    gerbv_format_t *format = image->format;
    gerbv_stats_t *stats = image->gerbv_stats;
    gerbv_error_list_t *error_list = stats->error_list;
    int c;

    op_int=gerb_fgetint(fd, NULL);

    /* Emphasize text with new line '\n' in the beginning */
    dprintf("\n     Found G%02d at line %ld (%s)\n",
		    op_int, *line_num_p, gerber_g_code_name(op_int));
    
    switch(op_int) {
    case 0:  /* Move */
	/* Is this doing anything really? */
	stats->G0++;
	break;
    case 1:  /* Linear Interpolation (1X scale) */
	state->interpolation = GERBV_INTERPOLATION_LINEARx1;
	stats->G1++;
	break;
    case 2:  /* Clockwise Linear Interpolation */
	state->interpolation = GERBV_INTERPOLATION_CW_CIRCULAR;
	stats->G2++;
	break;
    case 3:  /* Counter Clockwise Linear Interpolation */
	state->interpolation = GERBV_INTERPOLATION_CCW_CIRCULAR;
	stats->G3++;
	break;
    case 4:  /* Ignore Data Block */
	/* Don't do anything, just read 'til * */
        /* SDB asks:  Should we look for other codes while reading G04 in case
	 * user forgot to put * at end of comment block? */
	do {
	    c = gerb_fgetc(fd);
	}
	while (c != EOF && c != '*');

	stats->G4++;
	break;
    case 10: /* Linear Interpolation (10X scale) */
	state->interpolation = GERBV_INTERPOLATION_LINEARx10;
	stats->G10++;
	break;
    case 11: /* Linear Interpolation (0.1X scale) */
	state->interpolation = GERBV_INTERPOLATION_LINEARx01;
	stats->G11++;
	break;
    case 12: /* Linear Interpolation (0.01X scale) */
	state->interpolation = GERBV_INTERPOLATION_LINEARx001;
	stats->G12++;
	break;
    case 36: /* Turn on Polygon Area Fill */
	state->prev_interpolation = state->interpolation;
	state->interpolation = GERBV_INTERPOLATION_PAREA_START;
	state->changed = 1;
	stats->G36++;
	break;
    case 37: /* Turn off Polygon Area Fill */
	state->interpolation = GERBV_INTERPOLATION_PAREA_END;
	state->changed = 1;
	stats->G37++;
	break;
    case 54: /* Tool prepare */
	/* XXX Maybe uneccesary??? */
	if (gerb_fgetc(fd) == 'D') {
	    int a = gerb_fgetint(fd, NULL);
	    if ((a >= 0) && (a <= APERTURE_MAX)) {
		state->curr_aperture = a;
	    } else { 
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1, 
			_("Found aperture D%02d out of bounds while parsing "
			    "G code at line %ld in file \"%s\""),
			a, *line_num_p, fd->filename);
	    }
	} else {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Found unexpected code after G54 "
			"at line %ld in file \"%s\""),
		    *line_num_p, fd->filename);
/* TODO: insert error count here */
	}
	stats->G54++;
	break;
    case 55: /* Prepare for flash */
	stats->G55++;
	break;
    case 70: /* Specify inches */
	state->state = gerbv_image_return_new_netstate (state->state);
	state->state->unit = GERBV_UNIT_INCH;
	stats->G70++;
	break;
    case 71: /* Specify millimeters */
	state->state = gerbv_image_return_new_netstate (state->state);
	state->state->unit = GERBV_UNIT_MM;
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
	if (format) format->coordinate = GERBV_COORDINATE_ABSOLUTE;
	stats->G90++;
	break;
    case 91: /* Specify incremental format */
	if (format) format->coordinate = GERBV_COORDINATE_INCREMENTAL;
	stats->G91++;
	break;
    default:
	gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		_("Encountered unknown G code G%02d "
		    "at line %ld in file \"%s\""),
		op_int, *line_num_p, fd->filename);
	gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1,
		_("Ignorning unknown G code G%02d"), op_int);
	stats->G_unknown++;
/* TODO: insert error count here */

	break;
    }
    
    return;
} /* parse_G_code */


/* ------------------------------------------------------------------ */
/*! This function reads the numeric value of a D code and updates the 
 *  state.  It also updates the D stats counters
 */
static void 
parse_D_code(gerb_file_t *fd, gerb_state_t *state,
		gerbv_image_t *image, long int *line_num_p)
{
    int a;
    gerbv_stats_t *stats = image->gerbv_stats;
    gerbv_error_list_t *error_list = stats->error_list;

    a = gerb_fgetint(fd, NULL);
    dprintf("     Found D%02d code at line %ld\n", a, *line_num_p);

    switch(a) {
    case 0 : /* Invalid code */
        gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		_("Found invalid D00 code at line %ld in file \"%s\""),
		*line_num_p, fd->filename);
        stats->D_error++;
	break;
    case 1 : /* Exposure on */
	state->aperture_state = GERBV_APERTURE_STATE_ON;
	state->changed = 1;
	stats->D1++;
	break;
    case 2 : /* Exposure off */
	state->aperture_state = GERBV_APERTURE_STATE_OFF;
	state->changed = 1;
	stats->D2++;
	break;
    case 3 : /* Flash aperture */
	state->aperture_state = GERBV_APERTURE_STATE_FLASH;
	state->changed = 1;
	stats->D3++;
	break;
    default: /* Aperture in use */
	if ((a >= 0) && (a <= APERTURE_MAX)) {
	    state->curr_aperture = a;
	    
	} else {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Found out of bounds aperture D%02d "
			"at line %ld in file \"%s\""),
		    a, *line_num_p, fd->filename);
	    stats->D_error++;
	}
	state->changed = 0;
	break;
    }
    
    return;
} /* parse_D_code */


/* ------------------------------------------------------------------ */
static int
parse_M_code(gerb_file_t *fd, gerbv_image_t *image, long int *line_num_p)
{
    int op_int;
    gerbv_stats_t *stats = image->gerbv_stats;
    
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
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Encountered unknown M%02d code at line %ld in file \"%s\""),
		op_int, *line_num_p, fd->filename);
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
		_("Ignorning unknown M%02d code"), op_int);
	stats->M_unknown++;
    }
    return 0;
} /* parse_M_code */

/* ------------------------------------------------------------------ */
static void 
parse_rs274x(gint levelOfRecursion, gerb_file_t *fd, gerbv_image_t *image, 
	     gerb_state_t *state, gerbv_net_t *curr_net, gerbv_stats_t *stats, 
	     gchar *directoryPath, long int *line_num_p)
{
    int op[2];
    char str[3];
    int tmp;
    gerbv_aperture_t *a = NULL;
    gerbv_amacro_t *tmp_amacro;
    int ano;
    gdouble scale = 1.0;
    gerbv_error_list_t *error_list = stats->error_list;
    
    if (state->state->unit == GERBV_UNIT_MM)
    	scale = 25.4;
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if (op[0] == EOF || op[1] == EOF)
	gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unexpected EOF found in file \"%s\""), fd->filename);

    switch (A2I(op[0], op[1])){
	
	/* 
	 * Directive parameters 
	 */
    case A2I('A','S'): /* Axis Select */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	state->state = gerbv_image_return_new_netstate (state->state);
	
	if (op[0] == EOF || op[1] == EOF)
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unexpected EOF found in file \"%s\""), fd->filename);
	
	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
	    state->state->axisSelect = GERBV_AXIS_SELECT_SWAPAB;
	} else {
	    state->state->axisSelect = GERBV_AXIS_SELECT_NOSELECT;
	}

	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if (op[0] == EOF || op[1] == EOF)
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unexpected EOF found in file \"%s\""), fd->filename);

	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
	    state->state->axisSelect = GERBV_AXIS_SELECT_SWAPAB;
	} else {
	    state->state->axisSelect = GERBV_AXIS_SELECT_NOSELECT;
	}
	break;

    case A2I('F','S'): /* Format Statement */
	image->format = g_new0 (gerbv_format_t,1);
	
	switch (gerb_fgetc(fd)) {
	case 'L':
	    image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
	    break;
	case 'T':
	    image->format->omit_zeros = GERBV_OMIT_ZEROS_TRAILING;
	    break;
	case 'D':
	    image->format->omit_zeros = GERBV_OMIT_ZEROS_EXPLICIT;
	    break;
	default:
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("EagleCad bug detected: Undefined handling of zeros "
			"in format code at line %ld in file \"%s\""),
		    *line_num_p, fd->filename);
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1,
		    _("Defaulting to omitting leading zeros"));
	    gerb_ungetc(fd);
	    image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
	}
	
	switch (gerb_fgetc(fd)) {
	case 'A':
	    image->format->coordinate = GERBV_COORDINATE_ABSOLUTE;
	    break;
	case 'I':
	    image->format->coordinate = GERBV_COORDINATE_INCREMENTAL;
	    break;
	default:
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Invalid coordinate type defined in format code "
			"at line %ld in file \"%s\""),
		    *line_num_p, fd->filename);
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1,
		    _("Defaulting to absolute coordinates"));
	    image->format->coordinate = GERBV_COORDINATE_ABSOLUTE;
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
		if ((op[0] < '0') || (op[0] > '6')) {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Illegal format size '%s' "
				"at line %ld in file \"%s\""),
			    gerbv_escape_char(op[0]),
			    *line_num_p, fd->filename);
		}
		image->format->x_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6')) {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Illegal format size '%s' "
				"at line %ld in file \"%s\""),
			    gerbv_escape_char(op[0]),
			    *line_num_p, fd->filename);
		}
		image->format->x_dec = op[0] - '0';
		break;
	    case 'Y':
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6')) {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Illegal format size '%s' "
				"at line %ld in file \"%s\""),
			    gerbv_escape_char(op[0]),
			    *line_num_p, fd->filename);
		}
		image->format->y_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6')) {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Illegal format size '%s' "
			       "at line %ld in file \"%s\""),
			    gerbv_escape_char(op[0]),
			    *line_num_p, fd->filename);
		}
		image->format->y_dec = op[0] - '0';
		break;
	    default :
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Illegal format statement '%s' "
			   "at line %ld in file \"%s\""),
			gerbv_escape_char(op[0]),
			*line_num_p, fd->filename);
		gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1,
			_("Ignoring invalid format statement"));
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('M','I'): /* Mirror Image */
	op[0] = gerb_fgetc(fd);
	state->state = gerbv_image_return_new_netstate (state->state);
	
	while ((op[0] != '*')&&(op[0] != EOF)) {
            gint readValue=0;
	    switch (op[0]) {
	    case 'A' :
		readValue = gerb_fgetint(fd, NULL);
		if (readValue == 1) {
		    if (state->state->mirrorState == GERBV_MIRROR_STATE_FLIPB)
			state->state->mirrorState=GERBV_MIRROR_STATE_FLIPAB;
		    else
			state->state->mirrorState=GERBV_MIRROR_STATE_FLIPA;
		}
		break;
	    case 'B' :
		readValue = gerb_fgetint(fd, NULL);
		if (readValue == 1) {
		    if (state->state->mirrorState == GERBV_MIRROR_STATE_FLIPA)
			state->state->mirrorState=GERBV_MIRROR_STATE_FLIPAB;
		    else
			state->state->mirrorState=GERBV_MIRROR_STATE_FLIPB;
		}
		break;
	    default :
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Wrong character '%s' in mirror "
			    "at line %ld in file \"%s\""),
			gerbv_escape_char(op[0]), *line_num_p, fd->filename);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;  
    case A2I('M','O'): /* Mode of Units */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if (op[0] == EOF || op[1] == EOF)
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unexpected EOF found in file \"%s\""), fd->filename);

	switch (A2I(op[0],op[1])) {
	case A2I('I','N'):
	    state->state = gerbv_image_return_new_netstate (state->state);
	    state->state->unit = GERBV_UNIT_INCH;
	    break;
	case A2I('M','M'):
	    state->state = gerbv_image_return_new_netstate (state->state);
	    state->state->unit = GERBV_UNIT_MM;
	    break;
	default:
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Illegal unit '%s%s' at line %ld in file \"%s\""),
		    gerbv_escape_char(op[0]), gerbv_escape_char(op[1]),
		    *line_num_p, fd->filename);
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
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Wrong character '%s' in offset "
			    "at line %ld in file \"%s\""),
			gerbv_escape_char(op[0]), *line_num_p, fd->filename);
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
			gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
				_("Included file \"%s\" cannot be found "
				    "at line %ld in file \"%s\""),
				fullPath, *line_num_p, fd->filename);
		    }
		    g_free (fullPath);
		} else {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Parser encountered more than 10 levels of "
				"include file recursion which is not allowed "
				"by the RS-274X spec"));
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
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Wrong character '%s' in image offset "
			    "at line %ld in file \"%s\""),
			gerbv_escape_char(op[0]), *line_num_p, fd->filename);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('S','F'): /* Scale Factor */
     state->state = gerbv_image_return_new_netstate (state->state);
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
	
	if (op[0] == EOF || op[1] == EOF)
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unexpected EOF found in file \"%s\""), fd->filename);

	switch (A2I(op[0],op[1])) {
	case A2I('A','S'):
	    image->info->encoding = GERBV_ENCODING_ASCII;
	    break;
	case A2I('E','B'):
	    image->info->encoding = GERBV_ENCODING_EBCDIC;
	    break;
	case A2I('B','C'):
	    image->info->encoding = GERBV_ENCODING_BCD;
	    break;
	case A2I('I','S'):
	    image->info->encoding = GERBV_ENCODING_ISO_ASCII;
	    break;
	case A2I('E','I'):
	    image->info->encoding = GERBV_ENCODING_EIA;
	    break;
	default:
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unknown input code (IC) '%s%s' "
			"at line %ld in file \"%s\""),
		    gerbv_escape_char(op[0]), gerbv_escape_char(op[1]),
		    *line_num_p, fd->filename);
	}
	break;
	
	/* Image parameters */
    case A2I('I','J'): /* Image Justify */
	op[0] = gerb_fgetc(fd);
	image->info->imageJustifyTypeA = GERBV_JUSTIFY_LOWERLEFT;
	image->info->imageJustifyTypeB = GERBV_JUSTIFY_LOWERLEFT;
	image->info->imageJustifyOffsetA = 0.0;
	image->info->imageJustifyOffsetB = 0.0;
	while ((op[0] != '*')&&(op[0] != EOF)) {
	    switch (op[0]) {
	    case 'A' :
	    	op[0] = gerb_fgetc(fd);
	    	if (op[0] == 'C') {
		    image->info->imageJustifyTypeA = GERBV_JUSTIFY_CENTERJUSTIFY;
	    	} else if (op[0] == 'L') {
		    image->info->imageJustifyTypeA = GERBV_JUSTIFY_LOWERLEFT;
	    	} else {
		    gerb_ungetc (fd);
		    image->info->imageJustifyOffsetA = gerb_fgetdouble(fd) / scale;
	    	}
		break;
	    case 'B' :
		op[0] = gerb_fgetc(fd);
	    	if (op[0] == 'C') {
		    image->info->imageJustifyTypeB = GERBV_JUSTIFY_CENTERJUSTIFY;
	    	} else if (op[0] == 'L') {
		    image->info->imageJustifyTypeB = GERBV_JUSTIFY_LOWERLEFT;
	    	} else {
		    gerb_ungetc (fd);
		    image->info->imageJustifyOffsetB = gerb_fgetdouble(fd) / scale;
	    	}
		break;
	    default :
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Wrong character '%s' in image justify "
			    "at line %ld in file \"%s\""),
			gerbv_escape_char(op[0]), *line_num_p, fd->filename);
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
	    if (op[0] == EOF) {
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unexpected EOF while reading image polarity (IP) "
			    "in file \"%s\""), fd->filename);
	    }
	    str[ano] = (char)op[0];
	}
	
	if (strncmp(str, "POS", 3) == 0) 
	    image->info->polarity = GERBV_POLARITY_POSITIVE;
	else if (strncmp(str, "NEG", 3) == 0)
	    image->info->polarity = GERBV_POLARITY_NEGATIVE;
	else {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unknown polarity '%s%s%s' "
			"at line %ld in file \"%s\""),
		    gerbv_escape_char(str[0]), gerbv_escape_char(str[1]),
		    gerbv_escape_char(str[2]), *line_num_p, fd->filename);
	}
	break;
    case A2I('I','R'): /* Image Rotation */
	tmp = gerb_fgetint(fd, NULL) % 360;
	if (tmp == 0)
	    image->info->imageRotation = 0.0;
	else if (tmp == 90)
	    image->info->imageRotation = M_PI_2;
	else if (tmp == 180)
	    image->info->imageRotation = M_PI;
	else if (tmp == 270)
	    image->info->imageRotation = M_PI + M_PI_2;
	else {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Image rotation must be 0, 90, 180 or 270 "
			"(is actually %d) at line %ld in file \"%s\""),
		    tmp, *line_num_p, fd->filename);
	}
	break;
    case A2I('P','F'): /* Plotter Film */
	image->info->plotterFilm = gerb_fgetstring(fd, '*');
	break;
	
	/* Aperture parameters */
    case A2I('A','D'): /* Aperture Description */
	a = (gerbv_aperture_t *) g_new0 (gerbv_aperture_t,1);

	ano = parse_aperture_definition(fd, a, image, scale, line_num_p);
	if (ano == -1) {
		/* error with line parse, so just quietly ignore */
	}
	else if ((ano >= 0) && (ano <= APERTURE_MAX)) {
	    a->unit = state->state->unit;
	    image->aperture[ano] = a;
	    dprintf("     In %s(), adding new aperture to aperture list ...\n",
			    __func__);
	    gerbv_stats_add_aperture(stats->aperture_list,
				    -1, ano, 
				    a->type,
				    a->parameter);
	    gerbv_stats_add_to_D_list(stats->D_code_list,
				     ano);
	    if (ano < APERTURE_MIN) {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Aperture number out of bounds %d "
				"at line %ld in file \"%s\""),
			    ano, *line_num_p, fd->filename);
	    }
	} else {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Aperture number out of bounds %d "
		       "at line %ld in file \"%s\""),
		    ano, *line_num_p, fd->filename);
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
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Failed to parse aperture macro "
		       "at line %ld in file \"%s\""),
		    *line_num_p, fd->filename);
	}
	// return, since we want to skip the later back-up loop
	return;
	/* Layer */
    case A2I('L','N'): /* Layer Name */
	state->layer = gerbv_image_return_new_layer (state->layer);
	state->layer->name = gerb_fgetstring(fd, '*');
	break;
    case A2I('L','P'): /* Layer Polarity */
	state->layer = gerbv_image_return_new_layer (state->layer);
	switch (gerb_fgetc(fd)) {
	case 'D': /* Dark Polarity (default) */
	    state->layer->polarity = GERBV_POLARITY_DARK;
	    break;
	case 'C': /* Clear Polarity */
	    state->layer->polarity = GERBV_POLARITY_CLEAR;
	    break;
	default:
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unknown layer polarity '%s' "
		       "at line %ld in file \"%s\""),
		    gerbv_escape_char(op[0]), *line_num_p, fd->filename);
	}
	break;
    case A2I('K','O'): /* Knock Out */
        state->layer = gerbv_image_return_new_layer (state->layer);
        gerber_update_any_running_knockout_measurements (image);
        /* reset any previous knockout measurements */
        knockoutMeasure = FALSE;
        op[0] = gerb_fgetc(fd);
	if (op[0] == '*') { /* Disable previous SR parameters */
	    state->layer->knockout.type = GERBV_KNOCKOUT_TYPE_NOKNOCKOUT;
	    break;
	} else if (op[0] == 'C') {
	    state->layer->knockout.polarity = GERBV_POLARITY_CLEAR;
	} else if (op[0] == 'D') {
	    state->layer->knockout.polarity = GERBV_POLARITY_DARK;
	} else {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Knockout must supply a polarity (C, D, or *) "
			"at line %ld in file \"%s\""),
		    *line_num_p, fd->filename);
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
	        state->layer->knockout.type = GERBV_KNOCKOUT_TYPE_FIXEDKNOCK;
		state->layer->knockout.lowerLeftX = gerb_fgetdouble(fd) / scale;
		break;
	    case 'Y':
	        state->layer->knockout.type = GERBV_KNOCKOUT_TYPE_FIXEDKNOCK;
		state->layer->knockout.lowerLeftY = gerb_fgetdouble(fd) / scale;
		break;
	    case 'I':
	        state->layer->knockout.type = GERBV_KNOCKOUT_TYPE_FIXEDKNOCK;
		state->layer->knockout.width = gerb_fgetdouble(fd) / scale;
		break;
	    case 'J':
	        state->layer->knockout.type = GERBV_KNOCKOUT_TYPE_FIXEDKNOCK;
		state->layer->knockout.height = gerb_fgetdouble(fd) / scale;
		break;
	    case 'K':
	        state->layer->knockout.type = GERBV_KNOCKOUT_TYPE_BORDER;
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
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unknown variable in knockout "
			    "at line %ld in file \"%s\""),
			*line_num_p, fd->filename);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	break;
    case A2I('S','R'): /* Step and Repeat */
        /* start by generating a new layer (duplicating previous layer settings */
        state->layer = gerbv_image_return_new_layer (state->layer);
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
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Step-and-repeat parameter error "
			   "at line %ld in file \"%s\""),
			*line_num_p, fd->filename);
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
	state->layer = gerbv_image_return_new_layer (state->layer);
	
	state->layer->rotation = DEG2RAD(gerb_fgetdouble(fd));
	op[0] = gerb_fgetc(fd);
	if (op[0] != '*') {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Error in layer rotation command "
		       "at line %ld in file \"%s\""),
		    *line_num_p, fd->filename);
	}
	break;
    default:
	gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unknown RS-274X extension found %%%s%s%% "
		    "at line %ld in file \"%s\""),
		gerbv_escape_char(op[0]), gerbv_escape_char(op[1]),
		*line_num_p, fd->filename);
    }

    // make sure we read until the trailing * character
    // first, backspace once in case we already read the trailing *
    gerb_ungetc(fd);
    do {
    	tmp = gerb_fgetc(fd);
    } while (tmp != EOF && tmp != '*');

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
simplify_aperture_macro(gerbv_aperture_t *aperture, gdouble scale)
{
    const int extra_stack_size = 10;
    macro_stack_t *s;
    gerbv_instruction_t *ip;
    int handled = 1, nuf_parameters = 0, i, j, clearOperatorUsed = FALSE;
    double *lp; /* Local copy of parameters */
    double tmp[2] = {0.0, 0.0};
    gerbv_aperture_type_t type = GERBV_APTYPE_NONE;
    gerbv_simplified_amacro_t *sam;

    if (aperture == NULL)
	GERB_FATAL_ERROR(_("aperture NULL in simplify aperture macro"));

    if (aperture->amacro == NULL)
	GERB_FATAL_ERROR(_("aperture->amacro NULL in simplify aperture macro"));

    /* Allocate stack for VM */
    s = new_stack(aperture->amacro->nuf_push + extra_stack_size);
    if (s == NULL) 
	GERB_FATAL_ERROR("malloc stack failed in %s()", __FUNCTION__);

    /* Make a copy of the parameter list that we can rewrite if necessary */
    lp = g_new (double,APERTURE_PARAMETERS_MAX);

    memcpy(lp, aperture->parameter, sizeof(double) * APERTURE_PARAMETERS_MAX);
    
    for(ip = aperture->amacro->program; ip != NULL; ip = ip->next) {
	switch(ip->opcode) {
	case GERBV_OPCODE_NOP:
	    break;
	case GERBV_OPCODE_PUSH :
	    push(s, ip->data.fval);
	    break;
        case GERBV_OPCODE_PPUSH :
	    push(s, lp[ip->data.ival - 1]);
	    break;
	case GERBV_OPCODE_PPOP:
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    lp[ip->data.ival - 1] = tmp[0];
	    break;
	case GERBV_OPCODE_ADD :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    push(s, tmp[1] + tmp[0]);
	    break;
	case GERBV_OPCODE_SUB :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    push(s, tmp[1] - tmp[0]);
	    break;
	case GERBV_OPCODE_MUL :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    push(s, tmp[1] * tmp[0]);
	    break;
	case GERBV_OPCODE_DIV :
	    if (pop(s, &tmp[0]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    if (pop(s, &tmp[1]) < 0)
		GERB_FATAL_ERROR(_("Tried to pop an empty stack"));
	    push(s, tmp[1] / tmp[0]);
	    break;
	case GERBV_OPCODE_PRIM :
	    /* 
	     * This handles the exposure thing in the aperture macro
	     * The exposure is always the first element on stack independent
	     * of aperture macro.
	     */
	    switch(ip->data.ival) {
	    case 1:
		dprintf("  Aperture macro circle [1] (");
		type = GERBV_APTYPE_MACRO_CIRCLE;
		nuf_parameters = 4;
		break;
	    case 3:
		break;
	    case 4 :
		dprintf("  Aperture macro outline [4] (");
		type = GERBV_APTYPE_MACRO_OUTLINE;
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
		type = GERBV_APTYPE_MACRO_POLYGON;
		nuf_parameters = 6;
		break;
	    case 6 :
		dprintf("  Aperture macro moire [6] (");
		type = GERBV_APTYPE_MACRO_MOIRE;
		nuf_parameters = 9;
		break;
	    case 7 :
		dprintf("  Aperture macro thermal [7] (");
		type = GERBV_APTYPE_MACRO_THERMAL;
		nuf_parameters = 6;
		break;
	    case 2  :
	    case 20 :
		dprintf("  Aperture macro line 20/2 (");
		type = GERBV_APTYPE_MACRO_LINE20;
		nuf_parameters = 7;
		break;
	    case 21 :
		dprintf("  Aperture macro line 21 (");
		type = GERBV_APTYPE_MACRO_LINE21;
		nuf_parameters = 6;
		break;
	    case 22 :
		dprintf("  Aperture macro line 22 (");
		type = GERBV_APTYPE_MACRO_LINE22;
		nuf_parameters = 6;
		break;
	    default :
		handled = 0;
	    }

	    if (type != GERBV_APTYPE_NONE) { 
		if (nuf_parameters > APERTURE_PARAMETERS_MAX) {
			GERB_COMPILE_ERROR(_("Number of parameters to aperture macro (%d) "
							"are more than gerbv is able to store (%d)"),
							nuf_parameters, APERTURE_PARAMETERS_MAX);
			nuf_parameters = APERTURE_PARAMETERS_MAX;
		}

		/*
		 * Create struct for simplified aperture macro and
		 * start filling in the blanks.
		 */
		sam = g_new (gerbv_simplified_amacro_t, 1);
		sam->type = type;
		sam->next = NULL;
		memset(sam->parameter, 0, 
		       sizeof(double) * APERTURE_PARAMETERS_MAX);
		memcpy(sam->parameter, s->stack, 
		       sizeof(double) *  nuf_parameters);
		
		/* convert any mm values to inches */
		switch (type) {
		    case GERBV_APTYPE_MACRO_CIRCLE:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			break;
		    case GERBV_APTYPE_MACRO_OUTLINE:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			for (j=2; j<nuf_parameters-1; j++){
			    sam->parameter[j]/=scale;
			}
			break;
		    case GERBV_APTYPE_MACRO_POLYGON:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			break;
		    case GERBV_APTYPE_MACRO_MOIRE:
			sam->parameter[0]/=scale;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			sam->parameter[6]/=scale;
			sam->parameter[7]/=scale;
			break;
		    case GERBV_APTYPE_MACRO_THERMAL:
			sam->parameter[0]/=scale;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			break;
		    case GERBV_APTYPE_MACRO_LINE20:
			if (fabs(sam->parameter[0]) < 0.001)
			    clearOperatorUsed = TRUE;
			sam->parameter[1]/=scale;
			sam->parameter[2]/=scale;
			sam->parameter[3]/=scale;
			sam->parameter[4]/=scale;
			sam->parameter[5]/=scale;
			break;
		    case GERBV_APTYPE_MACRO_LINE21:
		    case GERBV_APTYPE_MACRO_LINE22:
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
		    gerbv_simplified_amacro_t *tmp_sam;
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
    g_free (lp);

    /* store a flag to let the renderer know if it should expect any "clear"
       primatives */
    aperture->parameter[0]= (gdouble) clearOperatorUsed;
    return handled;
} /* simplify_aperture_macro */


/* ------------------------------------------------------------------ */
static int 
parse_aperture_definition(gerb_file_t *fd, gerbv_aperture_t *aperture,
			  gerbv_image_t *image, gdouble scale,
			  long int *line_num_p)
{
    int ano, i;
    char *ad;
    char *token;
    gerbv_amacro_t *curr_amacro;
    gerbv_amacro_t *amacro = image->amacro;
    gerbv_error_list_t *error_list = image->gerbv_stats->error_list;
    gdouble tempHolder;
    
    if (gerb_fgetc(fd) != 'D') {
	gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		_("Found AD code with no following 'D' "
		    "at line %ld in file \"%s\""),
		*line_num_p, fd->filename);
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
    
    if (token == NULL) {
	gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		_("Invalid aperture definition "
		    "at line %ld in file \"%s\""),
		*line_num_p, fd->filename);
	return -1;
    }
    if (strlen(token) == 1) {
	switch (token[0]) {
	case 'C':
	    aperture->type = GERBV_APTYPE_CIRCLE;
	    break;
	case 'R' :
	    aperture->type = GERBV_APTYPE_RECTANGLE;
	    break;
	case 'O' :
	    aperture->type = GERBV_APTYPE_OVAL;
	    break;
	case 'P' :
	    aperture->type = GERBV_APTYPE_POLYGON;
	    break;
	}
	/* Here a should a T be defined, but I don't know what it represents */
    } else {
	aperture->type = GERBV_APTYPE_MACRO;
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
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Maximum number of allowed parameters exceeded "
			"in aperture %d at line %ld in file \"%s\""),
		    ano, *line_num_p, fd->filename);
	    break;
	}
	errno = 0;

	tempHolder = strtod(token, NULL);
	/* convert any MM values to inches */
	/* don't scale polygon angles or side numbers, or macro parmaeters */
	if (!(((aperture->type == GERBV_APTYPE_POLYGON) && ((i==1) || (i==2)))||
		(aperture->type == GERBV_APTYPE_MACRO))) {
	    tempHolder /= scale;
	}
	
	aperture->parameter[i] = tempHolder;
	if (errno) {
	    gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1,
		    _("Failed to read all parameters exceeded in "
			"aperture %d at line %ld in file \"%s\""),
		    ano, *line_num_p, fd->filename);
            aperture->parameter[i] = 0.0;
        }
    }
    
    aperture->nuf_parameters = i;
    
    gerb_ungetc(fd);

    if (aperture->type == GERBV_APTYPE_MACRO) {
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
calc_cirseg_sq(struct gerbv_net *net, int cw, 
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
	    GERB_COMPILE_ERROR(_("Unknow quadrant value while converting to cw"));
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
	GERB_COMPILE_ERROR(_("Strange quadrant: %d"), quadrant);
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

    if (alfa < GERBV_PRECISION_ANGLE_RAD
    &&  beta < GERBV_PRECISION_ANGLE_RAD) {
	net->cirseg->height = 0;
    }

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
	GERB_COMPILE_ERROR(_("Strange quadrant: %d"), quadrant);
    }

    if (net->cirseg->width < 0.0)
	GERB_COMPILE_WARNING(_("Negative width [%f] in quadrant %d [%f][%f]"),
			     net->cirseg->width, quadrant, RAD2DEG(alfa), RAD2DEG(beta));
    
    if (net->cirseg->height < 0.0)
	GERB_COMPILE_WARNING(_("Negative height [%f] in quadrant %d [%f][%f]"),
			     net->cirseg->height, quadrant, RAD2DEG(alfa), RAD2DEG(beta));

    return;

} /* calc_cirseg_sq */


/* Multiquadrant circular interpolation */
static void 
calc_cirseg_mq(struct gerbv_net *net, int cw, 
	       double delta_cp_x, double delta_cp_y)
{
    double d1x, d1y, d2x, d2y;
    double alfa, beta;

    net->cirseg->cp_x = net->start_x + delta_cp_x;
    net->cirseg->cp_y = net->start_y + delta_cp_y;

    /*
     * Some good values 
     */
    d1x = -delta_cp_x;
    d1y = -delta_cp_y;
    d2x = net->stop_x - net->cirseg->cp_x;
    d2y = net->stop_y - net->cirseg->cp_y;

    /*
     * It's possible for values to be calculated that smaller than epsilon,
     * but containing opposite signs. These values cause essentially a
     * "signed zero" effect, which makes atan2 results unpredictable.
     */
    if (fabs(d1x) < DBL_EPSILON) d1x = 0;
    if (fabs(d1y) < DBL_EPSILON) d1y = 0;
    if (fabs(d2x) < DBL_EPSILON) d2x = 0;
    if (fabs(d2y) < DBL_EPSILON) d2y = 0;

    net->cirseg->width = hypot(delta_cp_x, delta_cp_y);
    net->cirseg->width *= 2.0;
    net->cirseg->height = net->cirseg->width;

    /*
     *  Keep values in radians, convert to degrees only results
     */
    alfa = atan2(d1y, d1x);
    beta = atan2(d2y, d2x);

    /*
     * Make sure it's always positive angles
     */
    if (alfa < 0.0) {
	alfa += M_PI + M_PI;
	beta += M_PI + M_PI;
    }

    if (beta < 0.0)
	beta += M_PI + M_PI;

    /*
     * This is a sanity check for angles after the nature of atan2.
     * If cw we must make sure angle1-angle2 are always positive,
     * If ccw we must make sure angle2-angle1 are always negative.
     * We should really return one angle and the difference as GTK
     * uses them. But what the heck, it works for me.
     */
    if (cw) {
	if (alfa - beta < DBL_EPSILON)
	    beta -= M_PI + M_PI;
    } else {
	if (beta - alfa < DBL_EPSILON)
	    beta += M_PI + M_PI;
    }

    net->cirseg->angle1 = RAD2DEG(alfa);
    net->cirseg->angle2 = RAD2DEG(beta);
}

/* Calculate circular interpolation bounding box */
static void
calc_cirseg_bbox(const gerbv_cirseg_t *cirseg,
		double apert_size_x, double apert_size_y,
		gerbv_render_size_t *bbox)
{
	gdouble x, y, ang1, ang2, step_pi_2;

	/* For bounding box calculation only half of aperture size is used */
	apert_size_x /= 2;
	apert_size_y /= 2;

	ang1 = DEG2RAD(MIN(cirseg->angle1, cirseg->angle2));
	ang2 = DEG2RAD(MAX(cirseg->angle1, cirseg->angle2));

	/* Start arc point */
	x = cirseg->cp_x + cirseg->width*cos(ang1)/2;
	y = cirseg->cp_y + cirseg->width*sin(ang1)/2;
	gerber_update_min_and_max(bbox, x, y,
				apert_size_x, apert_size_x,
				apert_size_y, apert_size_y);

	/* Middle arc points */
	for (step_pi_2 = (ang1/M_PI_2 + 1)*M_PI_2;
				step_pi_2 < MIN(ang2, ang1 + 2*M_PI);
				step_pi_2 += M_PI_2) {
		x = cirseg->cp_x + cirseg->width*cos(step_pi_2)/2;
		y = cirseg->cp_y + cirseg->width*sin(step_pi_2)/2;
		gerber_update_min_and_max(bbox, x, y,
					apert_size_x, apert_size_x,
					apert_size_y, apert_size_y);
	}

	/* Stop arc point */
	x = cirseg->cp_x + cirseg->width*cos(ang2)/2;
	y = cirseg->cp_y + cirseg->width*sin(ang2)/2;
	gerber_update_min_and_max(bbox, x, y,
				apert_size_x, apert_size_x,
				apert_size_y, apert_size_y);
}

static void
gerber_update_any_running_knockout_measurements (gerbv_image_t *image)
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
gerber_calculate_final_justify_effects(gerbv_image_t *image)
{
    gdouble translateA = 0.0, translateB = 0.0;
    
    if (image->info->imageJustifyTypeA != GERBV_JUSTIFY_NOJUSTIFY) {
	if (image->info->imageJustifyTypeA == GERBV_JUSTIFY_CENTERJUSTIFY)
	    translateA = (image->info->max_x - image->info->min_x) / 2.0;
	else
	    translateA = -image->info->min_x;
    }
    if (image->info->imageJustifyTypeB != GERBV_JUSTIFY_NOJUSTIFY) {
	if (image->info->imageJustifyTypeB == GERBV_JUSTIFY_CENTERJUSTIFY)
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


void
gerber_update_image_min_max (gerbv_render_size_t *boundingBox,
		double repeat_off_X, double repeat_off_Y,
		gerbv_image_t* image)
{
	image->info->min_x = MIN(image->info->min_x, boundingBox->left);
	image->info->min_y = MIN(image->info->min_y, boundingBox->bottom);
	image->info->max_x = MAX(image->info->max_x,
			boundingBox->right + repeat_off_X);
	image->info->max_y = MAX(image->info->max_y,
			boundingBox->top + repeat_off_Y);
}

void
gerber_update_min_and_max(gerbv_render_size_t *boundingBox,
			  gdouble x, gdouble y, gdouble apertureSizeX1,
			  gdouble apertureSizeX2,gdouble apertureSizeY1,
			  gdouble apertureSizeY2)
{
    gdouble ourX1 = x - apertureSizeX1, ourY1 = y - apertureSizeY1;
    gdouble ourX2 = x + apertureSizeX2, ourY2 = y + apertureSizeY2;
    
    /* transform the point to the final rendered position, accounting
       for any scaling, offsets, mirroring, etc */
    /* NOTE: we need to already add/subtract in the aperture size since
       the final rendering may be scaled */
    cairo_matrix_transform_point (&currentMatrix, &ourX1, &ourY1);
    cairo_matrix_transform_point (&currentMatrix, &ourX2, &ourY2);

    /* check both points against the min/max, since depending on the rotation,
       mirroring, etc, either point could possibly be a min or max */

    boundingBox->left =   MIN(boundingBox->left,   ourX1);
    boundingBox->left =   MIN(boundingBox->left,   ourX2);
    boundingBox->right =  MAX(boundingBox->right,  ourX1);
    boundingBox->right =  MAX(boundingBox->right,  ourX2);
    boundingBox->bottom = MIN(boundingBox->bottom, ourY1);
    boundingBox->bottom = MIN(boundingBox->bottom, ourY2);
    boundingBox->top =    MAX(boundingBox->top,    ourY1);
    boundingBox->top =    MAX(boundingBox->top,    ourY2);
} /* gerber_update_min_and_max */

static gboolean
add_trailing_zeros_if_omitted(int *coord, int omitted_num,
		gerbv_format_t *format)
{
	if (format
	&&  format->omit_zeros == GERBV_OMIT_ZEROS_TRAILING
	&&  omitted_num > 0) {
		for (int i = 0; i < omitted_num; i++)
			*coord *= 10;

		return TRUE;
	}

	return FALSE;
} /* add_trailing_zeros_if_omitted() */

/** Return Gerber D-code name by code number. */
const char *gerber_d_code_name(int d_code)
{
	switch (d_code) {
	case 1:  return N_("exposure on");
	case 2:  return N_("exposure off");
	case 3:  return N_("flash aperture");
	default: return N_("unknown D-code");
	}
} /* gerber_d_code_name() */

/** Return Gerber G-code name by code number. */
const char *gerber_g_code_name(int g_code)
{
	switch (g_code) {
	case  0: return  N_("move");
	case  1: return  N_("1X linear interpolation");
	case  2: return  N_("CW interpolation");
	case  3: return  N_("CCW interpolation");
	case  4: return  N_("comment/ignore block");
	case 10: return  N_("10X linear interpolation");
	case 11: return  N_("0.1X linear interpolation");
	case 12: return  N_("0.01X linear interpolation");
	case 36: return  N_("poly fill on");
	case 37: return  N_("poly fill off");
	case 54: return  N_("tool prepare");
	case 55: return  N_("flash prepare");
	case 70: return  N_("units = inches");
	case 71: return  N_("units = mm");
	case 74: return  N_("disable 360 circ. interpolation");
	case 75: return  N_("enable 360 circ. interpolation");
	case 90: return  N_("absolute units");
	case 91: return  N_("incremental units");
	default: return  N_("unknown G-code");
	}
} /* gerber_g_code_name() */

/** Return Gerber M-code name by code number. */
const char *gerber_m_code_name(int m_code)
{
	switch (m_code) {
	case 0:  return N_("program stop (obsolete)");
	case 1:  return N_("optional stop (obsolete)");
	case 2:  return N_("end of file");
	default: return N_("unknown M-code");
	}
} /* gerber_m_code_name() */
