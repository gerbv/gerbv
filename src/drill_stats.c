/*
 * gEDA - GNU Electronic Design Automation
 *   drill_stats.c -- a part of gerbv.
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
#include "drill_stats.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf


/* ------------------------------------------------------- */
/** Allocates a new drill_stats structure
   @return drill_stats pointer on success, NULL on ERROR */
drill_stats_t *
drill_stats_new(void) {

    drill_stats_t *stats;
    drill_list_t *list;

    /* Malloc space for new stats struct.  Return NULL if error. */
    if ((stats = (drill_stats_t *)malloc(sizeof(drill_stats_t))) == NULL) {
        return NULL;
    }

    /* Set new stats struct to zero */
    memset((void *)stats, 0, sizeof(drill_stats_t));

    /* Initialize drill list */
    list = drill_stats_new_drill_list();
    if (list == NULL)
        GERB_FATAL_ERROR("malloc drill_list failed\n");
    stats->drill_list = (drill_list_t *) list;

    return stats;
}


/* ------------------------------------------------------- */
void
drill_stats_add_layer(drill_stats_t *accum_stats, 
		      drill_stats_t *input_stats) {

    drill_list_t *drill;

    dprintf("--->  Entering drill_stats_add_layer ..... \n");

    accum_stats->layer_count++;

    accum_stats->comment += input_stats->comment;
    /* F codes go here */

    accum_stats->G00 += input_stats->G00;
    accum_stats->G01 += input_stats->G01;
    accum_stats->G02 += input_stats->G02;
    accum_stats->G03 += input_stats->G03;
    accum_stats->G05 += input_stats->G05;
    accum_stats->G90 += input_stats->G90;
    accum_stats->G91 += input_stats->G91;
    accum_stats->G93 += input_stats->G93;
    accum_stats->G_unknown += input_stats->G_unknown;

    accum_stats->M00 += input_stats->M00;
    accum_stats->M01 += input_stats->M01;
    accum_stats->M18 += input_stats->M18;
    accum_stats->M25 += input_stats->M25;
    accum_stats->M30 += input_stats->M30;
    accum_stats->M31 += input_stats->M31;
    accum_stats->M45 += input_stats->M45;
    accum_stats->M47 += input_stats->M47;
    accum_stats->M48 += input_stats->M48;
    accum_stats->M71 += input_stats->M71;
    accum_stats->M72 += input_stats->M72;
    accum_stats->M95 += input_stats->M95;
    accum_stats->M97 += input_stats->M97;
    accum_stats->M98 += input_stats->M98;
    accum_stats->M_unknown += input_stats->M_unknown;

    /* ==== Now deal with the drill list ==== */
    for (drill = input_stats->drill_list;
         drill != NULL;
	 drill = drill->next) {
	dprintf("   In drill_stats_add_layer, adding drill_num = %d to list\n",
		drill->drill_num);
	/* First add this input drill to the accumulated list.
	 * Drills already in accum list will not be added. */
	drill_stats_add_to_drill_list(accum_stats->drill_list,
		 		      drill->drill_num, 
				      drill->drill_size,
				      drill->drill_unit);

	/* Now add count of input drill to accum list */
	dprintf("   adding count %d of drills for drill %d\n", 
		drill->drill_count, drill->drill_num);
	drill_stats_add_to_drill_counter(accum_stats->drill_list,
					 drill->drill_num,
					 drill->drill_count);
    }

    dprintf("<---  .... Leaving drill_stats_add_layer.\n");
	    
    return;
}


/* ------------------------------------------------------- */
gboolean
drill_stats_in_drill_list(drill_list_t *drill_list_in,
			  int drill_num_in) {
    drill_list_t *drill;
    for(drill = drill_list_in; drill != NULL; drill = drill->next) {
	if (drill_num_in == drill->drill_num) {
	    return TRUE;
	}
    }
    return FALSE;

}

/* ------------------------------------------------------- */
drill_list_t *
drill_stats_new_drill_list() {
    drill_list_t *drill_list;

    /* Malloc space for new drill_list struct.  Return NULL if error. */
    if ((drill_list = (drill_list_t *)malloc(sizeof(drill_list_t))) == NULL) {
        return NULL;
    }

    drill_list->drill_count = 0;
    drill_list->drill_num = -1; /* default val */
    drill_list->drill_size = 0.0;
    drill_list->drill_unit = NULL;
    drill_list->next = NULL;
    return drill_list;
} 


/* ------------------------------------------------------- */
void
drill_stats_add_to_drill_list(drill_list_t *drill_list_in, 
			      int drill_num_in, double drill_size_in,
			      char *drill_unit_in) {

    drill_list_t *drill_list_new;
    drill_list_t *drill;
    drill_list_t *drill_last;

    dprintf("   ---> Entering drill_stats_add_to_drill_list, first drill_num in list = %d ...\n", 
	    drill_list_in->drill_num);

    /* First check for empty list.  If empty, then just add this drill */
    if (drill_list_in->drill_num == -1) {
	dprintf("    .... In drill_stats_add_to_drill_list, adding first drill, no %d\n", 
		drill_num_in);
	drill_list_in->drill_num = drill_num_in;
	drill_list_in->drill_size = drill_size_in;
	drill_list_in->drill_count = 0;
	drill_list_in->drill_unit = g_strdup_printf("%s", drill_unit_in);
	drill_list_in->next = NULL;
	return;
    }
    /* Else check to see if this drill is already in the list */
    for(drill = drill_list_in; 
	drill != NULL; 
	drill = (drill_list_t *) drill->next) {
	dprintf("checking this drill_num %d against that in list %d.\n", 
		drill_num_in, drill->drill_num);
	if (drill_num_in == drill->drill_num) {
	    dprintf("   .... In drill_stats_add_to_drill_list, drill no %d already in list\n", 
		    drill_num_in);
	    return;  /* Found it in list, so return */
	}
	drill_last = drill;
    }

    /* Now malloc space for new drill list element */
    drill_list_new = (drill_list_t *) malloc(sizeof(drill_list_t));
    if (drill_list_new == NULL) {
	GERB_FATAL_ERROR("malloc format failed\n");
    }

    /* Now set various parameters based upon calling args */
    dprintf("    .... In drill_stats_add_to_drill_list, adding new drill, no %d\n", 
	    drill_num_in);
    drill_list_new->drill_num = drill_num_in;
    drill_list_new->drill_size = drill_size_in;
    drill_list_new->drill_count = 0;
    drill_list_new->drill_unit = g_strdup_printf("%s", drill_unit_in);
    drill_list_new->next = NULL;
    drill_last->next = drill_list_new;

    dprintf("   <---- ... leaving drill_stats_add_to_drill_list.\n");
    return;
}

/* ------------------------------------------------------- */
void
drill_stats_increment_drill_counter(drill_list_t *drill_list_in, 
				    int drill_num_in) {

    dprintf("   ----> Entering drill_stats_increment_drill_counter......\n");
    /* First check to see if this drill is already in the list */
    drill_list_t *drill;
    for(drill = drill_list_in; drill != NULL; drill = drill->next) {
	if (drill_num_in == drill->drill_num) {
	    drill->drill_count++;
	    dprintf("         .... incrementing drill count.  drill_num = %d, drill_count = %d.\n",
		    drill_list_in->drill_num, drill_list_in->drill_count);
	    dprintf("   <---- .... Leaving drill_stats_increment_drill_counter after incrementing counter.\n");
	    return;
	}
    }
    dprintf("   <---- .... Leaving drill_stats_increment_drill_counter without incrementing any counter.\n");

}

/* ------------------------------------------------------- */
void
drill_stats_add_to_drill_counter(drill_list_t *drill_list_in, 
				 int drill_num_in, 
				 int increment) {

    drill_list_t *drill;
    for(drill = drill_list_in; drill != NULL; drill = drill->next) {
	if (drill_num_in == drill->drill_num) {
	    dprintf("    In drill_stats_add_to_drill_counter, adding increment = %d drills to drill list\n", increment);
	    drill->drill_count += increment;
	    return;
	}
    }
}
