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

/** \file gerb_image.h
    \brief Header info for the image editing and support functions
    \ingroup libgerbv
*/

#ifndef GERB_IMAGE_H
#define GERB_IMAGE_H

#include "gerb_stats.h"
#include "drill_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Function prototypes
 */

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
typedef enum { 
    GERB_IMAGE_OK = 0,
    GERB_IMAGE_MISSING_NETLIST = 1,
    GERB_IMAGE_MISSING_FORMAT = 2, 
    GERB_IMAGE_MISSING_APERTURES = 4,
    GERB_IMAGE_MISSING_INFO = 8,
} gerb_verify_error_t;

gerb_verify_error_t gerbv_image_verify(gerbv_image_t const* image);

/* Dumps a written version of image to stdout */
void gerbv_image_dump(gerbv_image_t const* image);

gerbv_layer_t *
gerbv_image_return_new_layer (gerbv_layer_t *previousLayer);

gerbv_netstate_t *
gerbv_image_return_new_netstate (gerbv_netstate_t *previousState);


#ifdef __cplusplus
}
#endif

#endif /* GERB_IMAGE_H */
