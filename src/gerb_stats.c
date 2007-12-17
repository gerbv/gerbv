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

#include "gerb_error.h"
#include "gerb_stats.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

/* ------------------------------------------------------- */
/** Allocates a new gerb_stats structure
   @return gerb_stats pointer on success, NULL on ERROR */
gerb_stats_t *
gerb_stats_new(void) {

    gerb_stats_t *stats;
    error_list_t *error_list;

    /* Malloc space for new stats struct.  Return NULL if error. */
    if ((stats = (gerb_stats_t *)malloc(sizeof(gerb_stats_t))) == NULL) {
        return NULL;
    }

    /* Set new stats struct to zero */
    memset((void *)stats, 0, sizeof(gerb_stats_t));

    /* Initialize error list */
    error_list = gerb_stats_new_error_list();
    if (error_list == NULL)
        GERB_FATAL_ERROR("malloc error_list failed\n");
    stats->error_list = (error_list_t *) error_list;

    return stats;
}


/* ------------------------------------------------------- */
void
gerb_stats_add_layer(gerb_stats_t *accum_stats, 
		     gerb_stats_t *input_stats,
		     int this_layer) {

    error_list_t *error;

    accum_stats->layer_count++;
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

    /* ==== Now deal with the error list ==== */
    for (error = input_stats->error_list;
         error != NULL;
         error = error->next) {
        if (error->error_text != NULL) {
            gerb_stats_add_error(accum_stats->error_list,
                                  this_layer,
                                  error->error_text,
                                  error->type);
        }
    }


    return;
}

/* ------------------------------------------------------- */
error_list_t *
gerb_stats_new_error_list() {
    error_list_t *error_list;

    /* Malloc space for new error_list struct.  Return NULL if error. */
    if ((error_list = (error_list_t *)malloc(sizeof(error_list_t))) == NULL) {
        return NULL;
    }

    error_list->layer = -1;
    error_list->error_text = NULL;
    error_list->next = NULL;
    return error_list;
}


/* ------------------------------------------------------- */
void
gerb_stats_add_error(error_list_t *error_list_in,
                      int layer, const char *error_text,
                      enum error_type_t type) {

    error_list_t *error_list_new;
    error_list_t *error_last = NULL;
    error_list_t *error;

    /* Replace embedded error messages */
    switch (type) {
        case FATAL:
            GERB_FATAL_ERROR(error_text);
            break;
        case ERROR:
            GERB_COMPILE_ERROR(error_text);
            break;
        case WARNING:
            GERB_COMPILE_WARNING(error_text);
            break;
        case NOTE:
            break;
    }

    /* First handle case where this is the first list element */
    if (error_list_in->error_text == NULL) {
        error_list_in->layer = layer;
        error_list_in->error_text = g_strdup_printf("%s", error_text);
        error_list_in->type = type;
        error_list_in->next = NULL;
        return;
    }

    /* Next check to see if this error is already in the list */
    for(error = error_list_in; error != NULL; error = error->next) {
        if ((strcmp(error->error_text, error_text) == 0) &&
            (error->layer == layer) ) {
            return;  /* This error text is already in the error list */
        }
        error_last = error;  /* point to last element in error list */
    }
    /* This error text is unique.  Therefore, add it to the list */

    /* Now malloc space for new error list element */
    error_list_new = (error_list_t *) malloc(sizeof(error_list_t));
    if (error_list_new == NULL) {
        GERB_FATAL_ERROR("malloc error_list failed\n");
    }

    /* Set member elements */
    error_list_new->layer = layer;
    error_list_new->error_text = g_strdup_printf("%s", error_text);
    error_list_new->type = type;
    error_list_new->next = NULL;
    error_last->next = error_list_new;

    return;
}

