/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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

/** \file gerber.h
    \brief Header info for the RS274X parsing functions
    \ingroup libgerbv
*/

#ifndef GERBER_H
#define GERBER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>

#include "gerb_file.h"

typedef struct gerb_state {
    int curr_x;
    int curr_y;
    int prev_x;
    int prev_y;
    int delta_cp_x;
    int delta_cp_y;
    int curr_aperture;
    int changed;
    gerbv_aperture_state_t aperture_state;
    gerbv_interpolation_t interpolation;
    gerbv_interpolation_t prev_interpolation;
    gerbv_net_t *parea_start_node;
    gerbv_layer_t *layer;
    gerbv_netstate_t *state;
    int in_parea_fill;
    int mq_on;
} gerb_state_t;

/*
 * parse gerber file pointed to by fd
 */
gerbv_image_t *parse_gerb(gerb_file_t *fd, gchar *directoryPath);
gboolean gerber_is_rs274x_p(gerb_file_t *fd, gboolean *returnFoundBinary);
gboolean gerber_is_rs274d_p(gerb_file_t *fd);
gerbv_net_t *
gerber_create_new_net (gerbv_net_t *currentNet, gerbv_layer_t *layer, gerbv_netstate_t *state);

gboolean
gerber_create_new_aperture (gerbv_image_t *image, int *indexNumber,
		gerbv_aperture_type_t apertureType, gdouble parameter1, gdouble parameter2);
		
void gerber_update_image_min_max (gerbv_render_size_t *boundingBox, double repeat_off_X,
		double repeat_off_Y, gerbv_image_t* image);
void gerber_update_min_and_max(gerbv_render_size_t *boundingBox,
			  gdouble x, gdouble y, gdouble apertureSizeX1,
			  gdouble apertureSizeX2,gdouble apertureSizeY1,
			  gdouble apertureSizeY2);
#ifdef __cplusplus
}
#endif

#endif /* GERBER_H */
