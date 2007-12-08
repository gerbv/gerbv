/*
 * gEDA - GNU Electronic Design Automation
 *   gerb_stats.c -- a part of gerbv.
 *
 *   Copyright (C) Stuart Brorson (sdb@cloud9.net)
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

#include "gerb_stats.h"

/* ------------------------------------------------------- */
/** Allocates a new gerb_stats structure
   @param stats will be freed up if not NULL
   @return gerb_stats pointer on success, NULL on ERROR */
gerb_stats_t *
gerb_stats_new(void) {

    gerb_stats_t *stats;

    /* Malloc space for new stats struct.  Return NULL if error. */
    if ((stats = (gerb_stats_t *)malloc(sizeof(gerb_stats_t))) == NULL) {
        return NULL;
    }

    /* Set new stats struct to zero */
    memset((void *)stats, 0, sizeof(gerb_stats_t));

    return stats;
}


/* ------------------------------------------------------- */
void
gerb_stats_add_layer(gerb_stats_t *accum_stats, 
		     gerb_stats_t *input_stats) {
    accum_stats->G0 += input_stats->G0;
    accum_stats->G1 += input_stats->G1;
    accum_stats->G2 += input_stats->G2;
    accum_stats->G3 += input_stats->G3;
    accum_stats->G4 += input_stats->G4;
    accum_stats->G10 += input_stats->G10;
    accum_stats->G11 += input_stats->G11;
    accum_stats->G12 += input_stats->G12;
    accum_stats->G36 += input_stats->G36;
    accum_stats->G37 += input_stats->G37;
    accum_stats->G54 += input_stats->G54;
    accum_stats->G55 += input_stats->G55;
    accum_stats->G70 += input_stats->G70;
    accum_stats->G71 += input_stats->G71;
    accum_stats->G74 += input_stats->G74;
    accum_stats->G75 += input_stats->G75;
    accum_stats->G90 += input_stats->G90;
    accum_stats->G91 += input_stats->G91;
    accum_stats->G_unknown += input_stats->G_unknown;

    accum_stats->D1 += input_stats->D1;
    accum_stats->D2 += input_stats->D2;
    accum_stats->D3 += input_stats->D3;
    /* Must also accomodate user defined */
    accum_stats->D_unknown += input_stats->D_unknown;
    accum_stats->D_error += input_stats->D_error;

    accum_stats->M0 += input_stats->M0;
    accum_stats->M1 += input_stats->M1;
    accum_stats->M2 += input_stats->M2;
    accum_stats->M_unknown += input_stats->M_unknown;

    accum_stats->X += input_stats->X;
    accum_stats->Y += input_stats->Y;
    accum_stats->I += input_stats->I;
    accum_stats->J += input_stats->J;

    accum_stats->star += input_stats->star;
    accum_stats->unknown += input_stats->unknown;

    return;
}
