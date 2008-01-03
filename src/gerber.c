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


#include "config.h"
#include "gerber.h"
#include "gerb_error.h"
#include "gerb_stats.h"

#ifndef RENDER_USING_GDK
  #include <cairo.h>
#endif

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
static void parse_rs274x(gerb_file_t *fd, gerb_image_t *image, 
			 gerb_state_t *state);
static int parse_aperture_definition(gerb_file_t *fd, 
				     gerb_aperture_t *aperture,
				     gerb_image_t *image);
static void calc_cirseg_sq(struct gerb_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);
static void calc_cirseg_mq(struct gerb_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);
static gerb_net_t *gen_circle_segments(gerb_net_t *curr_net,
				       int cw, int *nuf_pcorners);

static void gerber_update_min_and_max (gdouble *minX, gdouble *minY,
	gdouble *maxX, gdouble *maxY, gdouble x, gdouble y, gdouble apertureSize);


static void gerber_update_any_running_knockout_measurements (gerb_image_t *image);

static void
gerber_calculate_final_justify_effects (gerb_image_t *image);

gboolean knockoutMeasure = FALSE;
gdouble knockoutLimitXmin,knockoutLimitYmin,knockoutLimitXmax,knockoutLimitYmax;
gerb_layer_t *knockoutLayer = NULL;
#ifndef RENDER_USING_GDK
	cairo_matrix_t currentMatrix;
#endif  

/* ------------------------------------------------------------------ */
gerb_image_t *
parse_gerb(gerb_file_t *fd)
{
    gerb_state_t *state = NULL;
    gerb_image_t *image = NULL;
    gerb_net_t *curr_net = NULL;
    int read, coord, len;
    double x_scale = 0.0, y_scale = 0.0;
    double delta_cp_x = 0.0, delta_cp_y = 0.0;
    double aperture_size;
    double scale;
    gerb_stats_t *stats;

    /* added by t.motylewski@bfad.de
     * many locales redefine "." as "," and so on, 
     * so sscanf and strtod has problems when
     * reading files using %f format */
    setlocale(LC_NUMERIC, "C" );

    /* 
     * Create new state.  This is used locally to keep track
     * of the photoplotter's state as the Gerber is read in.
     */
    state = (gerb_state_t *)g_malloc(sizeof(gerb_state_t));
    if (state == NULL)
	GERB_FATAL_ERROR("malloc state failed\n");

    /*
     * Set some defaults
     */
    memset((void *)state, 0, sizeof(gerb_state_t));

    /* 
     * Create new image.  This will be returned.
     */
    image = new_gerb_image(image);
    if (image == NULL)
	GERB_FATAL_ERROR("malloc image failed\n");
    curr_net = image->netlist;
    image->layertype = GERBER;
    image->gerb_stats = gerb_stats_new();
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

    while ((read = gerb_fgetc(fd)) != EOF) {
        /* figure out the scale, since we need to normailize 
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
	      gerber_update_any_running_knockout_measurements (image);
	      gerber_calculate_final_justify_effects(image);
		g_free(state);
		return image;
		break;
	    default:
		gerb_stats_add_error(stats->error_list,
				     -1,
				     "Unknown M code found.\n",
				     ERROR);
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
	    parse_rs274x(fd, image, state);
	    while (1){
	      char c=gerb_fgetc(fd);
	      if(c==EOF || c=='%')
		break;
	    }
	    break;
	case '*':  
	    dprintf("... Found * code\n");
	    stats->star++;
	    if (state->changed == 0) break;
	    state->changed = 0;

	    curr_net->next = (gerb_net_t *)g_malloc(sizeof(gerb_net_t));
	    if (curr_net->next == NULL)
		GERB_FATAL_ERROR("malloc curr_net->next failed\n");
	    curr_net = curr_net->next;
	    memset((void *)curr_net, 0, sizeof(gerb_net_t));
	    curr_net->layer = state->layer;
	    curr_net->state = state->state;
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
		curr_net->cirseg = (gerb_cirseg_t *)g_malloc(sizeof(gerb_cirseg_t));
		if (curr_net->cirseg == NULL)
		    GERB_FATAL_ERROR("malloc curr_net->cirseg failed\n");
		memset((void *)curr_net->cirseg, 0, sizeof(gerb_cirseg_t));
		if (state->mq_on)
		    calc_cirseg_mq(curr_net, 1, delta_cp_x, delta_cp_y);
		else
		    calc_cirseg_sq(curr_net, 1, delta_cp_x, delta_cp_y);
		break;
	    case CCW_CIRCULAR :
		curr_net->cirseg = (gerb_cirseg_t *)g_malloc(sizeof(gerb_cirseg_t));
		if (curr_net->cirseg == NULL)
		    GERB_FATAL_ERROR("malloc curr_net->cirseg failed\n");
		memset((void *)curr_net->cirseg, 0, sizeof(gerb_cirseg_t));
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
		break;
	    case PAREA_END :
		state->parea_start_node = NULL;
		state->in_parea_fill = 0;
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
		if (state->aperture_state == OFF &&
		    state->interpolation != PAREA_START) {
		    curr_net->interpolation = PAREA_END;
			
		    curr_net->next = (gerb_net_t *)g_malloc(sizeof(gerb_net_t));
		    if (curr_net->next == NULL)
			GERB_FATAL_ERROR("malloc curr_net->next failed\n");
		    curr_net = curr_net->next;
		    memset((void *)curr_net, 0, sizeof(gerb_net_t));
		    curr_net->layer = state->layer;
		    curr_net->state = state->state;

		    curr_net->interpolation = PAREA_START;
		    state->parea_start_node = curr_net;
		    curr_net->next = (gerb_net_t *)g_malloc(sizeof(gerb_net_t));
		    if (curr_net->next == NULL)
			GERB_FATAL_ERROR("malloc curr_net->next failed\n");
		    curr_net = curr_net->next;
		    memset((void *)curr_net, 0, sizeof(gerb_net_t));
		    curr_net->layer = state->layer;
		    curr_net->state = state->state;
		    
		    curr_net->start_x = (double)state->prev_x / x_scale;
		    curr_net->start_y = (double)state->prev_y / y_scale;
		    curr_net->stop_x = (double)state->curr_x / x_scale;
		    curr_net->stop_y = (double)state->curr_y / y_scale;
		}

		/*
		 * if this outline is circular, approximate it with short lines
		 */
		if (curr_net->cirseg) {
		    curr_net->interpolation = state->interpolation;	    
		    curr_net = gen_circle_segments(curr_net, 
						   state->interpolation == CW_CIRCULAR, 
						   &(state->parea_start_node->nuf_pcorners));
		}

		state->parea_start_node->nuf_pcorners++;
	    }  /* if (state->in_parea_fill && state->parea_start_node) */

	    curr_net->interpolation = state->interpolation;

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
	     * Make sure we don't hit any undefined aperture
	     * In macros the first parameter could be basically anything
	     */
	    if ((curr_net->aperture != 0) && 
		(image->aperture[curr_net->aperture] != NULL) &&
		(image->aperture[curr_net->aperture]->type != MACRO)) {
		aperture_size = image->aperture[curr_net->aperture]->parameter[0];
		if (image->aperture[curr_net->aperture]->unit == MM)
			scale = 25.4;
		else 
			scale = 1.0;
	    }
	    else {
		aperture_size = 0.0;
		scale = 1.0;
	    }

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

	    /* only update the min/max values if we are drawing and have an
	       aperture size */
	    if ((curr_net->aperture_state != OFF)){
	      double repeat_off_X=0, repeat_off_Y=0;
	      
	      /*
	       * If step_and_repeat (%SR%) is used, check min_x,max_y etc for the
	       * ends of the step_and_repeat lattice. This goes wrong in the case of
	       * negative dist_X or dist_Y, in which case we should compare against
	       * the startpoints of the lines, not the stoppoints, but that seems an
	       * uncommon case (and the error isn't very big any way).
	       *
	       */
		repeat_off_X = (state->layer->stepAndRepeat.X - 1)*
		  state->layer->stepAndRepeat.dist_X;
		repeat_off_Y = (state->layer->stepAndRepeat.Y - 1)*
		  state->layer->stepAndRepeat.dist_Y;


#ifndef RENDER_USING_GDK
		cairo_matrix_init (&currentMatrix, 1, 0, 0, 1, 0, 0);
		/* offset image */
		cairo_matrix_translate (&currentMatrix, image->info->offsetA, image->info->offsetB);
		/* do image rotation */
		cairo_matrix_rotate (&currentMatrix, image->info->imageRotation);
		/* it's a new layer, so recalculate the new transformation matrix
		   for it */
		/* do any rotations */
		cairo_matrix_rotate (&currentMatrix, state->layer->rotation);
			
		/* calculate current layer and state transformation matrices */
		/* apply scale factor */
		cairo_matrix_scale (&currentMatrix, state->state->scaleA, state->state->scaleB);
		/* apply offset */
		cairo_matrix_translate (&currentMatrix, state->state->offsetA, state->state->offsetB);
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
			/* we do this by rotating 270 (counterclockwise, then mirroring
			   the Y axis */
			cairo_matrix_rotate (&currentMatrix, 3 * M_PI / 2);
			cairo_matrix_scale (&currentMatrix, 1, -1);
		}
#endif

	      /* check both the start and stop of the aperture points against
	         a running min/max counter */
	      /* Note: only check start coordinate if this isn't a flash, since
	         the start point may be bogus if it is a flash */
	      if (curr_net->aperture_state != FLASH) {
		      gerber_update_min_and_max (&image->info->min_x, &image->info->min_y,
		      	&image->info->max_x, &image->info->max_y, curr_net->start_x,
		      	curr_net->start_y, aperture_size / 2.0 / scale);
		      gerber_update_min_and_max (&image->info->min_x, &image->info->min_y,
		      	&image->info->max_x, &image->info->max_y, curr_net->start_x + repeat_off_X,
		      	curr_net->start_y + repeat_off_Y, aperture_size / 2.0 / scale);
	      }
	      gerber_update_min_and_max (&image->info->min_x, &image->info->min_y,
	      	&image->info->max_x, &image->info->max_y, curr_net->stop_x,
	      	curr_net->stop_y, aperture_size / 2.0 / scale);
	      gerber_update_min_and_max (&image->info->min_x, &image->info->min_y,
	      	&image->info->max_x, &image->info->max_y, curr_net->stop_x + repeat_off_X,
	      	curr_net->stop_y + repeat_off_Y, aperture_size / 2.0 / scale);

		if (knockoutMeasure) {
			if (curr_net->aperture_state != FLASH) {
			      gerber_update_min_and_max (&knockoutLimitXmin, &knockoutLimitYmin, 
			      	&knockoutLimitXmax, &knockoutLimitYmax, curr_net->start_x,
			      	curr_net->start_y, aperture_size / 2.0 / scale);
		      }
		      gerber_update_min_and_max (&knockoutLimitXmin, &knockoutLimitYmin, 
		      	&knockoutLimitXmax, &knockoutLimitYmax, curr_net->stop_x,
		      	curr_net->stop_y, aperture_size / 2.0 / scale);		
		}
	    }
	    
	    break;
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	    break;
	default:
	    stats->unknown++;
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 g_strdup_printf("Found unknown character (whitespace?) %c[%d]\n", 
						 read, read),
				 ERROR);
	}  /* switch((char) (read & 0xff)) */
    }
    
    gerb_stats_add_error(stats->error_list,
			  -1,
			  "File is missing Gerber EOF code.\n",
			  ERROR);

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
gerber_is_rs274x_p(gerb_file_t *fd) {
  char *buf;
  int len = 0;
  char *letter;
  int ascii;
  int zero = 48;  /* ascii 0 */
  int nine = 57;  /* ascii 9 */
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

  buf = (char *) g_malloc(MAXL);
  if (buf == NULL) 
    GERB_FATAL_ERROR("malloc buf failed while checking for rs274x.\n");

  while (fgets(buf, MAXL, fd->fd) != NULL) {
    len = strlen(buf);
    
    /* First look through the file for indications of its type */

    /* check that file is not binary (non-printing chars */
    for (i = 0; i < len; i++) {
      ascii = (int) buf[i];
      if ((ascii > 128) || (ascii < 0)) {
        found_binary = TRUE;
      }
    }
    if (g_strstr_len(buf, len, "%ADD")) {
      found_ADD = TRUE;
    }
    if (g_strstr_len(buf, len, "D00")) {
      found_D0 = TRUE;
    }
    if (g_strstr_len(buf, len, "D2")) {
      found_D2 = TRUE;
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
    if (g_strstr_len(buf, len, "M2")) {
      found_M2 = TRUE;
    }
    if (g_strstr_len(buf, len, "M02")) {
      found_M2 = TRUE;
    }
    if (g_strstr_len(buf, len, "*")) {
      found_star = TRUE;
    }
    /* look for X<number> or Y<number> */
    if ((letter = g_strstr_len(buf, len, "X")) != NULL) {
      ascii = (int) letter[1]; /* grab char after X */
      if ((ascii >= zero) && (ascii <= nine)) {
	found_X = TRUE;
      }
    }
    if ((letter = g_strstr_len(buf, len, "Y")) != NULL) {
      ascii = (int) letter[1]; /* grab char after Y */
      if ((ascii >= zero) && (ascii <= nine)) {
	found_Y = TRUE;
      }
    }
  }
  rewind(fd->fd);
  free(buf);

  /* Now form logical expression determining if the file is RS-274X */
  if ((found_D0 || found_D2 || found_M0 || found_M2) && 
      found_ADD && found_star && (found_X || found_Y) &&
      !found_binary) 
    return TRUE;
  else return FALSE;

} /* gerber_is_rs274x */


/* ------------------------------------------------------------------- */
/*
 * Checks for signs that this is a RS-274D file
 * Returns TRUE if it is, FALSE if not.
 */
gboolean
gerber_is_rs274d_p(gerb_file_t *fd) {
  char *buf;
  int len = 0;
  char *letter;
  int ascii;
  int zero = 48; /* ascii 0 */
  int nine = 57; /* ascii 9 */
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

  buf = g_malloc(MAXL);
  if (buf == NULL) 
    GERB_FATAL_ERROR("malloc buf failed while checking for rs274d.\n");

  while (fgets(buf, MAXL, fd->fd) != NULL) {
    len = strlen(buf);
    
    /* First look through the file for indications of its type */

    /* check that file is not binary (non-printing chars */
    for (i = 0; i < len; i++) {
      ascii = (int) buf[i];
      if ((ascii > 128) || (ascii < 0)) {
        found_binary = TRUE;
      }
    }

    if (g_strstr_len(buf, len, "%ADD")) {
      found_ADD = TRUE;
    }
    if (g_strstr_len(buf, len, "D00")) {
      found_D0 = TRUE;
    }
    if (g_strstr_len(buf, len, "D2")) {
      found_D2 = TRUE;
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
    if (g_strstr_len(buf, len, "M2")) {
      found_M2 = TRUE;
    }
    if (g_strstr_len(buf, len, "M02")) {
      found_M2 = TRUE;
    }
    if (g_strstr_len(buf, len, "*")) {
      found_star = TRUE;
    }
    /* look for X<number> or Y<number> */
    if ((letter = g_strstr_len(buf, len, "X")) != NULL) {
      ascii = (int) letter[1]; /* grab char after X */
      if ((ascii >= zero) && (ascii <= nine)) {
	found_X = TRUE;
      }
    }
    if ((letter = g_strstr_len(buf, len, "Y")) != NULL) {
      ascii = (int) letter[1]; /* grab char after Y */
      if ((ascii >= zero) && (ascii <= nine)) {
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
  else return FALSE;

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
					 ERROR);
		}
	} else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Found unexpected code after G54\n",
				 ERROR);
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
			     ERROR);
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
				   ERROR);
	      stats->D_unknown++;
	  }
	} else {
	      gerb_stats_add_error(stats->error_list,
				   -1,
				   g_strdup_printf("Found aperture number out of bounds while parsing D code: %d\n", a),
				   ERROR);
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
			     ERROR);
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
parse_rs274x(gerb_file_t *fd, gerb_image_t *image, gerb_state_t *state)
{
    int op[2];
    char str[3];
    int tmp;
    gerb_aperture_t *a = NULL;
    amacro_t *tmp_amacro;
    int ano;
    gerb_stats_t *stats=image->gerb_stats;
    gdouble scale = 1.0;
    
    if (state->state->unit == MM)
    	scale = 25.4;

    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	gerb_stats_add_error(stats->error_list,
			     -1,
			     "Unexpected EOF found.\n",
			     ERROR);

    switch (A2I(op[0], op[1])){

      /* Directive parameters */
    case A2I('A','S'): /* Axis Select */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	state->state = gerb_image_return_new_netstate (state->state);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Unexpected EOF found.\n",
				 ERROR);
	
	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
		state->state->axisSelect = SWAPAB;
	}
	else {
		state->state->axisSelect = NOSELECT;
	}

	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);

	if ((op[0] == EOF) || (op[1] == EOF))
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Unexpected EOF found.\n",
				 ERROR);

	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
		state->state->axisSelect = SWAPAB;
	}
	else {
		state->state->axisSelect = NOSELECT;
	}
	break;

    case A2I('F','S'): /* Format Statement */
	image->format = (gerb_format_t *)g_malloc(sizeof(gerb_format_t));
	if (image->format == NULL) 
	    GERB_FATAL_ERROR("Failed malloc for format\n");
	memset((void *)image->format, 0, sizeof(gerb_format_t));
	
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
				 ERROR);
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
				 ERROR);
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
					 ERROR);
		image->format->x_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    gerb_stats_add_error(stats->error_list,
					 -1,
					 g_strdup_printf("Illegal format size : %c\n", (char)op[0]),
					 ERROR);
		image->format->x_dec = op[0] - '0';
		break;
	    case 'Y':
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    gerb_stats_add_error(stats->error_list,
					 -1,
					 g_strdup_printf("Illegal format size : %c\n", (char)op[0]),
					 ERROR);
		image->format->y_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    gerb_stats_add_error(stats->error_list,
					 -1,
					 g_strdup_printf("Illegal format size : %c\n", (char)op[0]),
					 ERROR);
		image->format->y_dec = op[0] - '0';
		break;
	    default :
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Invalid format statement [%c]\n", op[0]),
				     ERROR);
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
				     ERROR);
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
				 ERROR);
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
				 ERROR);
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
				     ERROR);
	    }
	    op[0] = gerb_fgetc(fd);
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
				     ERROR);
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
				 ERROR);
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
				 ERROR);
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
	    	if (op[0] == 'C')
	    		image->info->imageJustifyTypeA = CENTERJUSTIFY;
	    	else if (op[0] == 'L')
	    		image->info->imageJustifyTypeA = LOWERLEFT;
	    	else {
	    		gerb_ungetc (fd);
	    		image->info->imageJustifyOffsetA = gerb_fgetdouble(fd) / scale;
	    	}
		break;
	    case 'B' :
		op[0] = gerb_fgetc(fd);
	    	if (op[0] == 'C')
	    		image->info->imageJustifyTypeB = CENTERJUSTIFY;
	    	else if (op[0] == 'L')
	    		image->info->imageJustifyTypeB = LOWERLEFT;
	    	else {
	    		gerb_ungetc (fd);
	    		image->info->imageJustifyOffsetB = gerb_fgetdouble(fd) / scale;
	    	}
		break;
	    default :
		gerb_stats_add_error(stats->error_list,
				     -1,
				     g_strdup_printf("Wrong character in image justify:%c\n", op[0]),
				     ERROR);
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
				     ERROR);
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
				 ERROR);
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
				 ERROR);
	break;
    case A2I('P','F'): /* Plotter Film */
	image->info->plotterFilm = gerb_fgetstring(fd, '*');
	break;
	
	/* Aperture parameters */
    case A2I('A','D'): /* Aperture Description */
	a = (gerb_aperture_t *)g_malloc(sizeof(gerb_aperture_t));
	if (a == NULL)
	    GERB_FATAL_ERROR("malloc aperture failed\n");
	memset((void *)a, 0, sizeof(gerb_aperture_t));
	ano = parse_aperture_definition(fd, a, image);
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
				 ERROR);
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
	}
	else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Failed to parse aperture macro\n",
				 ERROR);
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
				 ERROR);
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
	}
	else if (op[0] == 'C') {
	    state->layer->knockout.polarity = CLEAR;
	}
	else if (op[0] == 'D') {
	    state->layer->knockout.polarity = DARK;
	}
	else {
	    gerb_stats_add_error(stats->error_list,
				 -1,
				 "Knockout must supply a polarity (C, D, or *)\n",				 ERROR);
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
	        state->layer->knockout.type = FIXED;
		state->layer->knockout.lowerLeftX = gerb_fgetdouble(fd) / scale;
		break;
	    case 'Y':
	        state->layer->knockout.type = FIXED;
		state->layer->knockout.lowerLeftY = gerb_fgetdouble(fd) / scale;
		break;
	    case 'I':
	        state->layer->knockout.type = FIXED;
		state->layer->knockout.width = gerb_fgetdouble(fd) / scale;
		break;
	    case 'J':
	        state->layer->knockout.type = FIXED;
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
				 "Unknown variable in knockout",				 ERROR);
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
				     ERROR);
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
				     ERROR);
      }
      break;
    default:
    gerb_stats_add_error(stats->error_list,
			 -1,
			 g_strdup_printf("Unknown RS-274X extension found %%%c%c%%\n", op[0], op[1]),
			 ERROR);
    }
    
    return;
} /* parse_rs274x */


/* ------------------------------------------------------------------ */
static int 
parse_aperture_definition(gerb_file_t *fd, gerb_aperture_t *aperture,
			  gerb_image_t *image)
{
    int ano, i;
    char *ad;
    char *token;
    amacro_t *curr_amacro;
    amacro_t *amacro = image->amacro;
    gerb_stats_t *stats = image->gerb_stats;

    if (gerb_fgetc(fd) != 'D') {
	gerb_stats_add_error(stats->error_list,
			     -1,
			     g_strdup_printf("Found AD code with no following 'D'.\n"),
			     ERROR);	
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
				 ERROR);
	    break;
	}
	errno = 0;
	/* we can't normalize these numbers for in/mm, since some may be 
	   integers used for macros */
	aperture->parameter[i] = strtod(token, NULL);
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


/* ------------------------------------------------------------------ */
static gerb_net_t *
gen_circle_segments(gerb_net_t *curr_net, int cw, int *nuf_pcorners)
{
    double end_x, end_y;
    double angle, angle_diff;
    double cp_x, cp_y;
    double radius;
    int steps, i;
    gerb_net_t * new_net;

    radius = curr_net->cirseg->width / 2.0;
    cp_x = curr_net->cirseg->cp_x;
    cp_y = curr_net->cirseg->cp_y;

    end_x = curr_net->stop_x;
    end_y = curr_net->stop_y;

    angle = curr_net->cirseg->angle1;
    angle_diff = curr_net->cirseg->angle2 - curr_net->cirseg->angle1;

    /*
     * compute number of segments, each is approx 1 degree
     */
    if (cw)
        steps = (int)(1.0 - angle_diff);
    else
        steps = (int)(1.0 + angle_diff);

    for (i = 1; i < steps; i++) {
#define DEG2RAD(a) (((a) * M_PI) / 180.0) 

	/*
	 * calculate end point for this segment
	 */
	curr_net->stop_x = cp_x + radius * cos (DEG2RAD(angle + (angle_diff * i) / steps));
	curr_net->stop_y = cp_y + radius * sin (DEG2RAD(angle + (angle_diff * i) / steps));

	/*
	 * create a new net, and copy current into it, (but not cirseg)
	 */
	new_net = (gerb_net_t *)g_malloc(sizeof(gerb_net_t));
	if (new_net == NULL)
	    GERB_FATAL_ERROR("malloc new_net failed\n");
	*new_net = *curr_net;
	new_net->cirseg = NULL;

	/*
	 * set start point to be old stop
	 */
	new_net->start_x = curr_net->stop_x;
	new_net->start_y = curr_net->stop_y;

	curr_net->next = new_net;
	curr_net = new_net;

	/*
	 * increment the polygon corner count
	 */
	(*nuf_pcorners)++;
    }

    /*
     * ensure the last point is at the end passed in
     */
    curr_net->stop_x = end_x;
    curr_net->stop_y = end_y;

    (*nuf_pcorners)++;

    return curr_net;
} /* gen_circle_segments */

static void
gerber_update_any_running_knockout_measurements (gerb_image_t *image){
	if (knockoutMeasure) {
		knockoutLayer->knockout.lowerLeftX = knockoutLimitXmin;
		knockoutLayer->knockout.lowerLeftY = knockoutLimitYmin;
		knockoutLayer->knockout.width = knockoutLimitXmax - knockoutLimitXmin;
		knockoutLayer->knockout.height = knockoutLimitYmax - knockoutLimitYmin;
		knockoutMeasure = FALSE;
	}
}

static void
gerber_calculate_final_justify_effects (gerb_image_t *image){
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
	image->info->imageJustifyOffsetActualA = translateA + image->info->imageJustifyOffsetA;
	image->info->imageJustifyOffsetActualB = translateB + image->info->imageJustifyOffsetB;
}


static void
gerber_update_min_and_max (gdouble *minX, gdouble *minY, gdouble *maxX, gdouble *maxY,
					gdouble x, gdouble y, gdouble apertureSize){
	gdouble ourX1 = x - apertureSize, ourY1 = y - apertureSize;
	gdouble ourX2 = x + apertureSize, ourY2 = y + apertureSize;

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
	if(*minX > ourX1)
		*minX = ourX1;
	if(*minX > ourX2)
		*minX = ourX2;
	if(*maxX < ourX1)
		*maxX = ourX1;
	if(*maxX < ourX2)
		*maxX = ourX2;
	if(*minY > ourY1)
		*minY = ourY1;
	if(*minY > ourY2)
		*minY = ourY2;
	if(*maxY < ourY1)
		*maxY = ourY1;
	if(*maxY < ourY2)
		*maxY = ourY2;
}

