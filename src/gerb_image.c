/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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
#include <math.h>
#include "gerb_image.h"

/* Allocates a new gerb_image structure
   Returns gerb_image pointer on success, NULL on ERROR */
gerb_image_t *
new_gerb_image(gerb_image_t *image)
{

    image = (gerb_image_t *)malloc(sizeof(gerb_image_t));
    if (image != NULL) {
	memset((void *)image, 0, sizeof(gerb_image_t));

	image->netlist = (gerb_net_t *)malloc(sizeof(gerb_net_t));
	if (image->netlist != NULL) {
	    memset((void *)image->netlist, 0, sizeof(gerb_net_t));
	    
	    image->info = (gerb_image_info_t *)malloc(sizeof(gerb_image_info_t));
	    if (image->info != NULL) {
		memset((void *)image->info, 0, sizeof(gerb_image_info_t));

		image->info->min_x = HUGE_VAL;
		image->info->min_y = HUGE_VAL;
		image->info->max_x = -HUGE_VAL;
		image->info->max_y = -HUGE_VAL;
		
		return image;
	    }
	    
	    free(image->netlist);
	    image->netlist = NULL;
	}
	free(image);
	image = NULL;
    }
    
    return NULL;
}


void
free_gerb_image(gerb_image_t *image)
{
    int i;
    gerb_net_t *net, *tmp;
    
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
	free(tmp);
	tmp = NULL;
    }

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
    int i;

    if (image->netlist == NULL) error |= GERB_IMAGE_MISSING_NETLIST;
    if (image->format == NULL)  error |= GERB_IMAGE_MISSING_FORMAT;
    if (image->info == NULL)    error |= GERB_IMAGE_MISSING_INFO;

    for (i = 0; i < APERTURE_MAX && image->aperture[i] == NULL; i++);
    if (i == APERTURE_MAX) error |= GERB_IMAGE_MISSING_APERTURES;
    
    return error;
}

