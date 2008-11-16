/*
 * gEDA - GNU Electronic Design Automation
 *   gerbv_stats.c -- a part of gerbv.
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

/** \file gerb_stats.c
    \brief Statistics generating functions for RS274X files
    \ingroup libgerbv
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>

#include "gerbv.h"
#include "gerb_stats.h"


/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

/* ------------------------------------------------------- */
/** Allocates a new gerbv_stats structure
   @return gerbv_stats pointer on success, NULL on ERROR */
gerbv_stats_t *
gerbv_stats_new(void) {

    gerbv_stats_t *stats;
    gerbv_error_list_t *error_list;
    gerbv_aperture_list_t *aperture_list;
    gerbv_aperture_list_t *D_code_list;

    /* Malloc space for new stats struct.  Return NULL if error. */
    if ((stats = (gerbv_stats_t *)g_malloc(sizeof(gerbv_stats_t))) == NULL) {
        return NULL;
    }

    /* Set new stats struct to zero */
    memset((void *)stats, 0, sizeof(gerbv_stats_t));

    /* Initialize error list */
    error_list = gerbv_stats_new_error_list();
    if (error_list == NULL)
        GERB_FATAL_ERROR("malloc error_list failed\n");
    stats->error_list = (gerbv_error_list_t *) error_list;

    /* Initialize aperture list */
    aperture_list = gerbv_stats_new_aperture_list();
    if (aperture_list == NULL)
        GERB_FATAL_ERROR("malloc aperture_list failed\n");
    stats->aperture_list = (gerbv_aperture_list_t *) aperture_list;

    /* Initialize D codes list */
    D_code_list = gerbv_stats_new_aperture_list();
    if (D_code_list == NULL)
        GERB_FATAL_ERROR("malloc D_code_list failed\n");
    stats->D_code_list = (gerbv_aperture_list_t *) D_code_list;

    return stats;
}

void
gerbv_destroy_error_list (gerbv_error_list_t *errorList) {
	gerbv_error_list_t *nextError=errorList,*tempError;
	
	while (nextError) {
		tempError = nextError->next;
		g_free (nextError->error_text);
		g_free (nextError);
		nextError = tempError;
	}
}

void
gerbv_destroy_aperture_list (gerbv_aperture_list_t *apertureList) {
	gerbv_aperture_list_t *nextAperture=apertureList,*tempAperture;
	
	while (nextAperture) {
		tempAperture = nextAperture->next;
		g_free (nextAperture);
		nextAperture = tempAperture;
	}
}

/* ------------------------------------------------------- */
void
gerbv_stats_destroy(gerbv_stats_t *stats) {
	if (stats == NULL)
		return;
	gerbv_destroy_error_list (stats->error_list);
	gerbv_destroy_aperture_list (stats->aperture_list);
	gerbv_destroy_aperture_list (stats->D_code_list);
	g_free (stats);
}	


/* ------------------------------------------------------- */
/*! This fcn is called with a two gerbv_stats_t structs:
 * accum_stats and input_stats.  Accum_stats holds 
 * a list of stats accumulated for
 * all layers.  This will be reported in the report window.
 * Input_stats holds a list of the stats for one particular layer
 * to be added to the accumulated list.  */
void
gerbv_stats_add_layer(gerbv_stats_t *accum_stats, 
		     gerbv_stats_t *input_stats,
		     int this_layer) {
    
    dprintf("---> Entering gerbv_stats_add_layer ... \n");

    gerbv_error_list_t *error;
    gerbv_aperture_list_t *aperture;
    gerbv_aperture_list_t *D_code;

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
    /* Create list of user-defined D codes from aperture list */
    for (D_code = input_stats->D_code_list;
         D_code != NULL;
         D_code = D_code->next) {
        if (D_code->number != -1) {
	  dprintf("     .... In gerbv_stats_add_layer, D code section, adding number = %d to accum_stats D list ...\n",
		  D_code->number);
	  gerbv_stats_add_to_D_list(accum_stats->D_code_list,
				   D_code->number);
	  dprintf("     .... In gerbv_stats_add_layer, D code section, calling increment_D_count with count %d ...\n", 
		  D_code->count);
	  gerbv_stats_increment_D_list_count(accum_stats->D_code_list,
					    D_code->number,
					    D_code->count,
					    accum_stats->error_list);
      }
    }
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
            gerbv_stats_add_error(accum_stats->error_list,
                                  this_layer,
                                  error->error_text,
                                  error->type);
        }
    }

    /* ==== Now deal with the aperture list ==== */
    for (aperture = input_stats->aperture_list;
         aperture != NULL;
         aperture = aperture->next) {
        if (aperture->number != -1) {
            gerbv_stats_add_aperture(accum_stats->aperture_list,
				    this_layer,
				    aperture->number,
				    aperture->type,
				    aperture->parameter);
        }
    }

    dprintf("<---- .... Leaving gerbv_stats_add_layer. \n");

    return;
}

/* ------------------------------------------------------- */
gerbv_error_list_t *
gerbv_stats_new_error_list() {
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
gerbv_stats_add_error(gerbv_error_list_t *error_list_in,
                      int layer, const char *error_text,
                      gerbv_message_type_t type) {

    gerbv_error_list_t *error_list_new;
    gerbv_error_list_t *error_last = NULL;
    gerbv_error_list_t *error;

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

    return;
}

/* ------------------------------------------------------- */
gerbv_aperture_list_t *
gerbv_stats_new_aperture_list() {
    gerbv_aperture_list_t *aperture_list;
    int i;

    dprintf("Mallocing new gerb aperture list\n");
    /* Malloc space for new aperture_list struct.  Return NULL if error. */
    if ((aperture_list = (gerbv_aperture_list_t *)g_malloc(sizeof(gerbv_aperture_list_t))) 
	 == NULL) {
        return NULL;
    }

    dprintf("   Placing values in certain structs.\n");
    aperture_list->number = -1;
    aperture_list->count = 0;
    aperture_list->type = 0;
    for (i = 0; i<5; i++) {
	aperture_list->parameter[i] = 0.0;
    }
    aperture_list->next = NULL;
    return aperture_list;
}


/* ------------------------------------------------------- */
void
gerbv_stats_add_aperture(gerbv_aperture_list_t *aperture_list_in,
			int layer, int number, gerbv_aperture_type_t type,
			double parameter[5]) {

    gerbv_aperture_list_t *aperture_list_new;
    gerbv_aperture_list_t *aperture_last = NULL;
    gerbv_aperture_list_t *aperture;
    int i;

    dprintf("   --->  Entering gerbv_stats_add_aperture ....\n"); 

    /* First handle case where this is the first list element */
    if (aperture_list_in->number == -1) {
	dprintf("     .... Adding first aperture to aperture list ... \n"); 
	dprintf("     .... Aperture type = %d ... \n", type); 
        aperture_list_in->number = number;
        aperture_list_in->type = type;
	aperture_list_in->layer = layer;
	for(i=0; i<5; i++) { 
	    aperture_list_in->parameter[i] = parameter[i];
	}
        aperture_list_in->next = NULL;
	dprintf("   <---  .... Leaving gerbv_stats_add_aperture.\n"); 
        return;
    }

    /* Next check to see if this aperture is already in the list */
    for(aperture = aperture_list_in; 
	aperture != NULL; 
	aperture = aperture->next) {
        if ((aperture->number == number) &&
            (aperture->layer == layer) ) {
	  dprintf("     .... This aperture is already in the list ... \n"); 
	    dprintf("   <---  .... Leaving gerbv_stats_add_aperture.\n"); 
            return;  
        }
        aperture_last = aperture;  /* point to last element in list */
    }
    /* This aperture number is unique.  Therefore, add it to the list */
    dprintf("     .... Adding another aperture to list ... \n"); 
    dprintf("     .... Aperture type = %d ... \n", type); 
	
    /* Now malloc space for new aperture list element */
    aperture_list_new = (gerbv_aperture_list_t *) g_malloc(sizeof(gerbv_aperture_list_t));
    if (aperture_list_new == NULL) {
        GERB_FATAL_ERROR("malloc aperture_list failed\n");
    }

    /* Set member elements */
    aperture_list_new->layer = layer;
    aperture_list_new->number = number;
    aperture_list_new->type = type;
    aperture_list_new->next = NULL;
    for(i=0; i<5; i++) { 
	aperture_list_new->parameter[i] = parameter[i];
    }
    aperture_last->next = aperture_list_new;

    dprintf("   <---  .... Leaving gerbv_stats_add_aperture.\n"); 

    return;
}

/* ------------------------------------------------------- */
void
gerbv_stats_add_to_D_list(gerbv_aperture_list_t *D_list_in,
			 int number) {
  
  gerbv_aperture_list_t *D_list;
  gerbv_aperture_list_t *D_list_last=NULL;
  gerbv_aperture_list_t *D_list_new;

    dprintf("   ----> Entering add_to_D_list, numbr = %d\n", number);

    /* First handle case where this is the first list element */
    if (D_list_in->number == -1) {
	dprintf("     .... Adding first D code to D code list ... \n"); 
	dprintf("     .... Aperture number = %d ... \n", number); 
        D_list_in->number = number;
	D_list_in->count = 0;
        D_list_in->next = NULL;
	dprintf("   <---  .... Leaving add_to_D_list.\n"); 
        return;
    }

    /* Look to see if this is already in list */
    for(D_list = D_list_in; 
	D_list != NULL; 
	D_list = D_list->next) {
        if (D_list->number == number) {
  	    dprintf("    .... Found in D list .... \n");
	    dprintf("   <---  .... Leaving add_to_D_list.\n"); 
            return;  
        }
        D_list_last = D_list;  /* point to last element in list */
    }

    /* This aperture number is unique.  Therefore, add it to the list */
    dprintf("     .... Adding another D code to D code list ... \n"); 
	
    /* Malloc space for new aperture list element */
    D_list_new = (gerbv_aperture_list_t *) g_malloc(sizeof(gerbv_aperture_list_t));
    if (D_list_new == NULL) {
        GERB_FATAL_ERROR("malloc D_list failed\n");
    }

    /* Set member elements */
    D_list_new->number = number;
    D_list_new->count = 0;
    D_list_new->next = NULL;
    D_list_last->next = D_list_new;

    dprintf("   <---  .... Leaving add_to_D_list.\n"); 

    return;
}

/* ------------------------------------------------------- */
int 
gerbv_stats_increment_D_list_count(gerbv_aperture_list_t *D_list_in,
				    int number, 
				    int count,
				    gerbv_error_list_t *error) {
  
    gerbv_aperture_list_t *D_list;

    dprintf("   Entering inc_D_list_count, code = D%d, input count to add = %d\n", number, count);

    /* Find D code in list and increment it */
    for(D_list = D_list_in; 
	D_list != NULL; 
	D_list = D_list->next) {
        if (D_list->number == number) {
	    dprintf("    old count = %d\n", D_list->count);
	    D_list->count += count;  /* Add to this aperture count, then return */
	    dprintf("    updated count = %d\n", D_list->count);
            return 0;  /* Return 0 for success */  
        }
    }

    /* This D number is not defined.  Therefore, flag error */
    dprintf("    .... Didn't find this D code in defined list .... \n");
    dprintf("   <---  .... Leaving inc_D_list_count.\n"); 
    gerbv_stats_add_error(error,
			 -1,
			 "Undefined aperture number called out in D code.\n",
			 GERBV_MESSAGE_ERROR);
    return -1;  /* Return -1 for failure */
}

