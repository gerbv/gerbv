/*
 * gEDA - GNU Electronic Design Automation
 * gerb_stats.h -- a part of gerbv.
 *
 *   Copyright (C) 2007 Stuart Brorson (sdb@cloud9.net)
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

#ifndef GERB_STATS_H
#define GERB_STATS_H

#include "gerb_error.h"
#include "gerb_aperture.h"

/* the gerb_aperture_list is used to keep track of 
 * apertures used in stats reporting */
typedef struct gerb_aperture_list_t {
    int number;
    int layer;
    int count;
    enum aperture_t type;
    double parameter[5];
    struct gerb_aperture_list_t *next;
} gerb_aperture_list_t;


typedef struct {
    struct error_list_t *error_list;
    struct gerb_aperture_list_t *aperture_list;
    struct gerb_aperture_list_t *D_code_list;

    int layer_count;
    int G0;
    int G1;
    int G2;
    int G3;
    int G4;
    int G10;
    int G11;
    int G12;
    int G36;
    int G37;
    int G54;
    int G55;
    int G70;
    int G71;
    int G74;
    int G75;
    int G90;
    int G91;
    int G_unknown;

    int D1;
    int D2;
    int D3;
/*    GHashTable *D_user_defined; */
    int D_unknown;
    int D_error;

    int M0;
    int M1;
    int M2;
    int M_unknown;

    int X;
    int Y;
    int I;
    int J;

    /* Must include % RS-274 codes */
    int star;
    int unknown;

} gerb_stats_t;

/* ===================  Prototypes ================ */
gerb_stats_t * gerb_stats_new(void);
void gerb_stats_add_layer(gerb_stats_t *accum_stats, 
			  gerb_stats_t *input_stats,
			  int this_layer);

error_list_t *gerb_stats_new_error_list(void);
void gerb_stats_add_error(error_list_t *error_list_in,
                           int layer, const char *error_text,
                           enum error_type_t type);

gerb_aperture_list_t *gerb_stats_new_aperture_list(void);
void gerb_stats_add_aperture(gerb_aperture_list_t *aperture_list_in,
			     int layer, int number, enum aperture_t type,
			     double parameter[5]);
void gerb_stats_add_to_D_list(gerb_aperture_list_t *D_list_in,
			      int number);
void gerb_stats_increment_D_list_count(gerb_aperture_list_t *D_list_in,
				       int number, 
				       int count,
				       error_list_t *error); 

#endif /* GERB_STATS_H */
