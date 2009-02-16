/*
 * gEDA - GNU Electronic Design Automation
 *   drill_stats.c -- a part of gerbv.
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

/** \file drill_stats.c
    \brief Statistics generating functions for Excellon drill files
    \ingroup libgerbv
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>

#include "common.h"
#include "gerbv.h"
#include "drill_stats.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf


/* ------------------------------------------------------- */
/** Allocates a new drill_stats structure
   @return drill_stats pointer on success, NULL on ERROR */
gerbv_drill_stats_t *
gerbv_drill_stats_new(void) {

    gerbv_drill_stats_t *stats;
    gerbv_drill_list_t *drill_list;
    gerbv_error_list_t *error_list;

    /* Malloc space for new stats struct.  Return NULL if error. */
    if ((stats = (gerbv_drill_stats_t *)g_malloc(sizeof(gerbv_drill_stats_t))) == NULL) {
        return NULL;
    }

    /* Set new stats struct to zero */
    memset((void *)stats, 0, sizeof(gerbv_drill_stats_t));

    /* Initialize drill list */
    drill_list = gerbv_drill_stats_new_drill_list();
    if (drill_list == NULL)
        GERB_FATAL_ERROR("malloc drill_list failed\n");
    stats->drill_list = (gerbv_drill_list_t *) drill_list;

    /* Initialize error list */
    error_list = gerbv_drill_stats_new_error_list();
    if (error_list == NULL)
        GERB_FATAL_ERROR("malloc error_list failed\n");
    stats->error_list = (gerbv_error_list_t *) error_list;

    stats->detect = NULL;

    return stats;
}

void
gerbv_drill_destroy_error_list (gerbv_error_list_t *errorList) {
	gerbv_error_list_t *nextError=errorList,*tempError;
	
	while (nextError) {
		tempError = nextError->next;
		g_free (nextError->error_text);
		g_free (nextError);
		nextError = tempError;
	}
}

void
gerbv_drill_destroy_drill_list (gerbv_drill_list_t *apertureList) {
	gerbv_drill_list_t *nextAperture=apertureList,*tempAperture;
	
	while (nextAperture) {
		tempAperture = nextAperture->next;
		g_free(nextAperture->drill_unit);
		g_free (nextAperture);
		nextAperture = tempAperture;
	}
}

void
gerbv_drill_stats_destroy(gerbv_drill_stats_t *stats) {
	if (stats == NULL)
		return;
	gerbv_drill_destroy_error_list (stats->error_list);
	gerbv_drill_destroy_drill_list (stats->drill_list);
	g_free (stats);
}	
	
/* ------------------------------------------------------- */
void
gerbv_drill_stats_add_layer(gerbv_drill_stats_t *accum_stats, 
		      gerbv_drill_stats_t *input_stats,
		      int this_layer) {

    gerbv_drill_list_t *drill;
    gerbv_error_list_t *error;
    char *tmps, *tmps2;

    dprintf("--->  Entering gerbv_drill_stats_add_layer ..... \n");

    accum_stats->layer_count++;

    accum_stats->comment += input_stats->comment;
    /* F codes go here */

    accum_stats->G00 += input_stats->G00;
    accum_stats->G01 += input_stats->G01;
    accum_stats->G02 += input_stats->G02;
    accum_stats->G03 += input_stats->G03;
    accum_stats->G04 += input_stats->G04;
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

    accum_stats->R += input_stats->R;

    /* ==== Now deal with the drill list ==== */
    for (drill = input_stats->drill_list;
         drill != NULL;
	 drill = drill->next) {
	dprintf("   In gerbv_drill_stats_add_layer, adding drill_num = %d to list\n",
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
	accum_stats->total_count += drill->drill_count;
    }

    /* ==== Now deal with the error list ==== */
    for (error = input_stats->error_list;
         error != NULL;
	 error = error->next) {
	if (error->error_text != NULL) {
	    drill_stats_add_error(accum_stats->error_list,
				  this_layer,
				  error->error_text,
				  error->type);
	}
    }

    /* ==== Now deal with the misc header stuff ==== */
    tmps = NULL;
    tmps2 = NULL;
    if (input_stats->detect) {
	tmps2 = g_strdup_printf ("Broken tool detect %s (layer %d)", input_stats->detect, this_layer);
    }    
    if (accum_stats->detect) {
	if (tmps2) {
	    tmps = g_strdup_printf ("%s\n%s", accum_stats->detect, tmps2);
	    g_free (accum_stats->detect);
	    accum_stats->detect = NULL;
	}
    } else {
	if (tmps2) {
	    tmps = g_strdup_printf ("%s", tmps2);
	}
    }
    if (tmps2) {
	g_free (tmps2);
    }
    if (tmps != NULL) {
	accum_stats->detect = tmps;
    }

    for (error = input_stats->error_list;
         error != NULL;
	 error = error->next) {
	if (error->error_text != NULL) {
	    drill_stats_add_error(accum_stats->error_list,
				  this_layer,
				  error->error_text,
				  error->type);
	}
    }


    dprintf("<---  .... Leaving gerbv_drill_stats_add_layer.\n");
	    
    return;
}


/* ------------------------------------------------------- */
gboolean
drill_stats_in_drill_list(gerbv_drill_list_t *drill_list_in,
			  int drill_num_in) {
    gerbv_drill_list_t *drill;
    for(drill = drill_list_in; drill != NULL; drill = drill->next) {
	if (drill_num_in == drill->drill_num) {
	    return TRUE;
	}
    }
    return FALSE;

}

/* ------------------------------------------------------- */
gerbv_drill_list_t *
gerbv_drill_stats_new_drill_list() {
    gerbv_drill_list_t *drill_list;

    /* Malloc space for new drill_list struct.  Return NULL if error. */
    if ((drill_list = (gerbv_drill_list_t *)g_malloc(sizeof(gerbv_drill_list_t))) == NULL) {
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
drill_stats_add_to_drill_list(gerbv_drill_list_t *drill_list_in, 
			      int drill_num_in, double drill_size_in,
			      char *drill_unit_in) {

    gerbv_drill_list_t *drill_list_new;
    gerbv_drill_list_t *drill;
    gerbv_drill_list_t *drill_last = NULL;

    dprintf ("%s(%p, %d, %g, \"%s\")\n", __FUNCTION__, drill_list_in, drill_num_in,
	     drill_size_in, drill_unit_in);

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
	drill = (gerbv_drill_list_t *) drill->next) {
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
    drill_list_new = (gerbv_drill_list_t *) g_malloc(sizeof(gerbv_drill_list_t));
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
drill_stats_modify_drill_list(gerbv_drill_list_t *drill_list_in, 
			      int drill_num_in, double drill_size_in,
			      char *drill_unit_in) {

    gerbv_drill_list_t *drill;

    dprintf("   ---> Entering drill_stats_modify_drill_list, first drill_num in list = %d ...\n", 
	    drill_list_in->drill_num);

    /* Look for this drill num in list */
    for(drill = drill_list_in; 
	drill != NULL; 
	drill = (gerbv_drill_list_t *) drill->next) {
	dprintf("checking this drill_num %d against that in list %d.\n", 
		drill_num_in, drill->drill_num);
	if (drill_num_in == drill->drill_num) {
	    dprintf("   .... Found it, now update it ....\n");
	    drill->drill_size = drill_size_in;
	    if (drill->drill_unit) 
		g_free(drill->drill_unit);
	    drill->drill_unit = g_strdup_printf("%s", drill_unit_in);
	    dprintf("   <---- ... Modified drill.  leaving drill_stats_modify_drill_list.\n");
	    return;
	}
    }
    dprintf("   <---- ... Did not find drill.  leaving drill_stats_modify_drill_list.\n");
    return;
}

/* ------------------------------------------------------- */
void
drill_stats_increment_drill_counter(gerbv_drill_list_t *drill_list_in, 
				    int drill_num_in) {

    dprintf("   ----> Entering drill_stats_increment_drill_counter......\n");
    /* First check to see if this drill is already in the list */
    gerbv_drill_list_t *drill;
    for(drill = drill_list_in; drill != NULL; drill = drill->next) {
	if (drill_num_in == drill->drill_num) {
	    drill->drill_count++;
	    dprintf("         .... incrementing drill count.  drill_num = %d, drill_count = %d.\n",
		    drill_list_in->drill_num, drill->drill_count);
	    dprintf("   <---- .... Leaving drill_stats_increment_drill_counter after incrementing counter.\n");
	    return;
	}
    }
    dprintf("   <---- .... Leaving drill_stats_increment_drill_counter without incrementing any counter.\n");

}

/* ------------------------------------------------------- */
void
drill_stats_add_to_drill_counter(gerbv_drill_list_t *drill_list_in, 
				 int drill_num_in, 
				 int increment) {

    gerbv_drill_list_t *drill;
    for(drill = drill_list_in; drill != NULL; drill = drill->next) {
	if (drill_num_in == drill->drill_num) {
	    dprintf("    In drill_stats_add_to_drill_counter, adding increment = %d drills to drill list\n", increment);
	    drill->drill_count += increment;
	    return;
	}
    }
}


/* ------------------------------------------------------- */
gerbv_error_list_t *
gerbv_drill_stats_new_error_list() {
    gerbv_error_list_t *error_list;

    /* Malloc space for new error_list struct.  Return NULL if error. */
    if ((error_list = (gerbv_error_list_t *)g_malloc(sizeof(gerbv_error_list_t))) == NULL) {
        return NULL;
    }

    error_list->layer = -1;
    error_list->error_text = NULL;
    error_list->next = NULL;
    return error_list;
} 



/* ------------------------------------------------------- */
void
drill_stats_add_error(gerbv_error_list_t *error_list_in, 
		      int layer, const char *error_text,
		      gerbv_message_type_t type) {

    gerbv_error_list_t *error_list_new;
    gerbv_error_list_t *error_last = NULL;
    gerbv_error_list_t *error;

    dprintf("   ----> Entering drill_stats_add_error......\n");

    /* Replace embedded error messages */
    switch (type) {
	case GERBV_MESSAGE_FATAL:
	    GERB_FATAL_ERROR("%s",error_text);
	    break;
	case GERBV_MESSAGE_ERROR:
	    GERB_COMPILE_ERROR("%s",error_text);
	    break;
	case GERBV_MESSAGE_WARNING:
	    GERB_COMPILE_WARNING("%s",error_text);
	    break;
	case GERBV_MESSAGE_NOTE:
	    break;
    }


    /* First handle case where this is the first list element */
    if (error_list_in->error_text == NULL) {
	error_list_in->layer = layer;
	error_list_in->error_text = g_strdup_printf("%s", error_text);
	error_list_in->type = type;
	error_list_in->next = NULL;
	dprintf("   <---- .... Leaving drill_stats_add_error after adding first error message.\n");
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
    error_list_new = (gerbv_error_list_t *) g_malloc(sizeof(gerbv_error_list_t));
    if (error_list_new == NULL) {
	GERB_FATAL_ERROR("malloc error_list failed\n");
    }
    
    /* Set member elements */
    error_list_new->layer = layer;
    error_list_new->error_text = g_strdup_printf("%s", error_text);
    error_list_new->type = type;
    error_list_new->next = NULL;
    error_last->next = error_list_new;

    dprintf("   <---- .... Leaving drill_stats_add_error after adding new error message.\n");
    return;

}
