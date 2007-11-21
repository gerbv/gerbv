/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include "gerb_error.h"
#include "gerb_image.h"

/** Allocates a new gerb_image structure
   @param image will be freed up if not NULL
   @return gerb_image pointer on success, NULL on ERROR */
gerb_image_t *
new_gerb_image(gerb_image_t *image)
{
    free_gerb_image(image);
    
    if ((image = (gerb_image_t *)malloc(sizeof(gerb_image_t))) == NULL) {
	return NULL;
    }
    memset((void *)image, 0, sizeof(gerb_image_t));
    
    if ((image->netlist = (gerb_net_t *)malloc(sizeof(gerb_net_t))) == NULL) {
	free(image);
	return NULL;
    }
    memset((void *)image->netlist, 0, sizeof(gerb_net_t));
    
    if ((image->info = (gerb_image_info_t *)malloc(sizeof(gerb_image_info_t))) == NULL) {
	free(image->netlist);
	free(image);
	return NULL;
    }
    memset((void *)image->info, 0, sizeof(gerb_image_info_t));
    
    if ((image->transf = gerb_transf_new()) == NULL) {
        free(image->info);
        free(image->netlist);
	free(image);
	return NULL;
    }

    
    image->info->min_x = HUGE_VAL;
    image->info->min_y = HUGE_VAL;
    image->info->max_x = -HUGE_VAL;
    image->info->max_y = -HUGE_VAL;

    image->info->step_and_repeat.X = 1;
    image->info->step_and_repeat.Y = 1;

    return image;
}


void
free_gerb_image(gerb_image_t *image)
{
    int i;
    gerb_net_t *net, *tmp;
    
    if(image==NULL)
        return;
        
    /*
     * Free apertures
     */
    for (i = 0; i < APERTURE_MAX; i++) 
	if (image->aperture[i] != NULL) {
	    free(image->aperture[i]);
	    image->aperture[i] = NULL;
	}

    /*
     * Free aperture macro
     */
    if (image->amacro)
	free_amacro(image->amacro);

    /*
     * Free format
     */
    if (image->format)
	free(image->format);
    
    /*
     * Free info
     */
    if (image->info) {
	if (image->info->name)
	    free(image->info->name);
	free(image->info);
    }
    
    /*
     * Free netlist
     */
    for (net = image->netlist; net != NULL; ) {
	tmp = net; 
	net = net->next; 
	if (tmp->cirseg != NULL) {
	    free(tmp->cirseg);
	    tmp->cirseg = NULL;
	}
	if (tmp->label) {
		g_string_free (tmp->label, TRUE);
	}
	free(tmp);
	tmp = NULL;
    }
    
 //   gerb_transf_free(image->transf);

    /*
     * Free and reset the final image
     */
    free(image);
    image = NULL;
    
    return;
} /* free_gerb_image */


/*
 * Check that the parsed gerber image is complete.
 * Returned errorcodes are:
 * 0: No problems
 * 1: Missing netlist
 * 2: Missing format
 * 4: Missing apertures
 * 8: Missing info
 * It could be any of above or'ed together
 */
gerb_verify_error_t
gerb_image_verify(gerb_image_t *image)
{
    gerb_verify_error_t error = GERB_IMAGE_OK;
    int i, n_nets;;
    gerb_net_t *net;

    if (image->netlist == NULL) error |= GERB_IMAGE_MISSING_NETLIST;
    if (image->format == NULL)  error |= GERB_IMAGE_MISSING_FORMAT;
    if (image->info == NULL)    error |= GERB_IMAGE_MISSING_INFO;

    /* Count how many nets we have */
    n_nets = 0;
    if (image->netlist != NULL) {
      for (net = image->netlist->next ; net != NULL; net = net->next) {
	n_nets++;
      }
    }

    /* If we have nets but no apertures are defined, then complain */
    if( n_nets > 0) {
      for (i = 0; i < APERTURE_MAX && image->aperture[i] == NULL; i++);
      if (i == APERTURE_MAX) error |= GERB_IMAGE_MISSING_APERTURES;
    }

    return error;
} /* gerb_image_verify */


static void
gerb_image_interpolation(enum interpolation_t interpolation)
{
    switch (interpolation) {
    case LINEARx1:
	printf("linearX1");
	break;
    case LINEARx10:
	printf("linearX10");
	break;
    case LINEARx01:
	printf("linearX01");
	break;
    case LINEARx001:
	printf("linearX001");
	break;
    case CW_CIRCULAR:
	printf("CW circular");
	break;
    case CCW_CIRCULAR:
	printf("CCW circular");
	break;
    case  PAREA_START:
	printf("polygon area start");
	break;
    case  PAREA_END:
	printf("polygon area end");
	break;
    default:
	printf("unknown");
    }
} /* gerb_image_interpolation */


/* Dumps a written version of image to stdout */
void 
gerb_image_dump(gerb_image_t *image)
{
    int i, j;
    gerb_aperture_t **aperture;
    gerb_net_t *net;

    /* Apertures */
    printf("Apertures:\n");
    aperture = image->aperture;
    for (i = 0; i < APERTURE_MAX; i++) {
	if (aperture[i]) {
	    printf(" Aperture no:%d is an ", i);
	    switch(aperture[i]->type) {
	    case CIRCLE:
		printf("circle");
		break;
	    case RECTANGLE:
		printf("rectangle");
		break;
	    case OVAL:
		printf("oval");
		break;
	    case POLYGON:
		printf("polygon");
		break;
	    case MACRO:
		printf("macro");
		break;
	    default:
		printf("unknown");
	    }
	    for (j = 0; j < aperture[i]->nuf_parameters; j++) {
		printf(" %f", aperture[i]->parameter[j]);
	    }
	    printf("\n");
	}
    }

    /* Netlist */
    net = image->netlist;
    while (net){
	printf("(%f,%f)->(%f,%f) with %d (", net->start_x, net->start_y, 
	       net->stop_x, net->stop_y, net->aperture);
	gerb_image_interpolation(net->interpolation);
	printf(")\n");
	net = net->next;
    }
} /* gerb_image_dump */
