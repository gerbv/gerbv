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

/** \file gerb_image.c
    \brief This file contains general files for handling the gerbv_image_t structure
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
#include "gerb_image.h"
#include "gerber.h"
#include "amacro.h"

typedef struct {
    int oldAperture;
    int newAperture;
} gerb_translation_entry_t;

gerbv_image_t *
gerbv_create_image(gerbv_image_t *image, const gchar *type)
{
    gerbv_destroy_image(image);
    
    /* Malloc space for image */
    if ((image = (gerbv_image_t *)g_malloc(sizeof(gerbv_image_t))) == NULL) {
	return NULL;
    }
    memset((void *)image, 0, sizeof(gerbv_image_t));
    
    /* Malloc space for image->netlist */
    if ((image->netlist = (gerbv_net_t *)g_malloc(sizeof(gerbv_net_t))) == NULL) {
	g_free(image);
	return NULL;
    }
    memset((void *)image->netlist, 0, sizeof(gerbv_net_t));
    
    /* Malloc space for image->info */
    if ((image->info = (gerbv_image_info_t *)g_malloc(sizeof(gerbv_image_info_t))) == NULL) {
	g_free(image->netlist);
	g_free(image);
	return NULL;
    }
    memset((void *)image->info, 0, sizeof(gerbv_image_info_t));
    
    /* Set aside position for stats struct */
    image->gerbv_stats = NULL;
    image->drill_stats = NULL;

    image->info->min_x = HUGE_VAL;
    image->info->min_y = HUGE_VAL;
    image->info->max_x = -HUGE_VAL;
    image->info->max_y = -HUGE_VAL;

    /* create our first layer and fill with non-zero default values */
    image->layers = g_new0 (gerbv_layer_t, 1);
    image->layers->stepAndRepeat.X = 1;
    image->layers->stepAndRepeat.Y = 1;
    image->layers->polarity = GERBV_POLARITY_DARK;
    
    /* create our first netstate and fill with non-zero default values */
    image->states = g_new0 (gerbv_netstate_t, 1);
    image->states->scaleA = 1;
    image->states->scaleB = 1;

    /* fill in some values for our first net */
    image->netlist->layer = image->layers;
    image->netlist->state = image->states;
    
    if (type == NULL)
	image->info->type = g_strdup ("unknown");
    else
	image->info->type = g_strdup (type);

    /* the individual file parsers will have to set this. */
    image->info->attr_list = NULL;
    image->info->n_attr = 0;

    return image;
}


void
gerbv_destroy_image(gerbv_image_t *image)
{
    int i;
    gerbv_net_t *net, *tmp;
    gerbv_layer_t *layer;
    gerbv_netstate_t *state;
    gerbv_simplified_amacro_t *sam,*sam2;

    if(image==NULL)
        return;
        
    /*
     * Free apertures
     */
    for (i = 0; i < APERTURE_MAX; i++) 
	if (image->aperture[i] != NULL) {
	    for (sam = image->aperture[i]->simplified; sam != NULL; ){
	      sam2 = sam->next;
	    	g_free (sam);
	    	sam = sam2;
	    }

	    g_free(image->aperture[i]);
	    image->aperture[i] = NULL;
	}

    /*
     * Free aperture macro
     */
     
    if (image->amacro) {
	free_amacro(image->amacro);
    }

    /*
     * Free format
     */
    if (image->format)
	g_free(image->format);
    
    /*
     * Free info
     */
    if (image->info) {
	g_free(image->info->name);
	g_free(image->info->type);
	gerbv_attribute_destroy_HID_attribute (image->info->attr_list, image->info->n_attr);
	g_free(image->info);
    }
    
    /*
     * Free netlist
     */
    for (net = image->netlist; net != NULL; ) {
	tmp = net; 
	net = net->next; 
	if (tmp->cirseg != NULL) {
	    g_free(tmp->cirseg);
	    tmp->cirseg = NULL;
	}
	if (tmp->label) {
		g_string_free (tmp->label, TRUE);
	}
	g_free(tmp);
	tmp = NULL;
    }
    for (layer = image->layers; layer != NULL; ) {
    	gerbv_layer_t *tempLayer = layer;
    	
    	layer = layer->next;
    	g_free (tempLayer);
    }
    for (state = image->states; state != NULL; ) {
    	gerbv_netstate_t *tempState = state;
    	
    	state = state->next;
    	g_free (tempState);
    }
    gerbv_stats_destroy(image->gerbv_stats);
    gerbv_drill_stats_destroy(image->drill_stats);

    /*
     * Free and reset the final image
     */
    g_free(image);
    image = NULL;
    
    return;
}


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
gerbv_image_verify(gerbv_image_t const* image)
{
    gerb_verify_error_t error = GERB_IMAGE_OK;
    int i, n_nets;;
    gerbv_net_t *net;

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
gerbv_image_interpolation(gerbv_interpolation_t interpolation)
{
    switch (interpolation) {
    case GERBV_INTERPOLATION_LINEARx1:
	printf("linearX1");
	break;
    case GERBV_INTERPOLATION_x10:
	printf("linearX10");
	break;
    case GERBV_INTERPOLATION_LINEARx01:
	printf("linearX01");
	break;
    case GERBV_INTERPOLATION_LINEARx001:
	printf("linearX001");
	break;
    case GERBV_INTERPOLATION_CW_CIRCULAR:
	printf("CW circular");
	break;
    case GERBV_INTERPOLATION_CCW_CIRCULAR:
	printf("CCW circular");
	break;
    case  GERBV_INTERPOLATION_PAREA_START:
	printf("polygon area start");
	break;
    case  GERBV_INTERPOLATION_PAREA_END:
	printf("polygon area end");
	break;
    default:
	printf("unknown");
    }
} /* gerb_image_interpolation */

static void
gerbv_image_aperture_state(gerbv_aperture_state_t state)
{
    switch (state) {
    case GERBV_APERTURE_STATE_OFF:
	printf("..state off");
	break;
    case GERBV_APERTURE_STATE_ON:
	printf("..state on");
	break;
    case GERBV_APERTURE_STATE_FLASH:
	printf("..state flash");
	break;
    default:
	printf("..state unknown");
    }
}

/* Dumps a written version of image to stdout */
void 
gerbv_image_dump(gerbv_image_t const* image)
{
    int i, j;
    gerbv_aperture_t * const* aperture;
    gerbv_net_t const * net;

    /* Apertures */
    printf("Apertures:\n");
    aperture = image->aperture;
    for (i = 0; i < APERTURE_MAX; i++) {
	if (aperture[i]) {
	    printf(" Aperture no:%d is an ", i);
	    switch(aperture[i]->type) {
	    case GERBV_APTYPE_CIRCLE:
		printf("circle");
		break;
	    case GERBV_APTYPE_RECTANGLE:
		printf("rectangle");
		break;
	    case GERBV_APTYPE_OVAL:
		printf("oval");
		break;
	    case GERBV_APTYPE_POLYGON:
		printf("polygon");
		break;
	    case GERBV_APTYPE_MACRO:
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
	gerbv_image_interpolation(net->interpolation);
	gerbv_image_aperture_state(net->aperture_state);
	printf(")\n");
	net = net->next;
    }
} /* gerbv_image_dump */


gerbv_layer_t *
gerbv_image_return_new_layer (gerbv_layer_t *previousLayer)
{
    gerbv_layer_t *newLayer = g_new0 (gerbv_layer_t, 1);
    
    *newLayer = *previousLayer;
    previousLayer->next = newLayer;
    /* clear this boolean so we only draw the knockout once */
    newLayer->knockout.firstInstance = FALSE;
    newLayer->next = NULL;
    
    return newLayer;
} /* gerbv_image_return_new_layer */


gerbv_netstate_t *
gerbv_image_return_new_netstate (gerbv_netstate_t *previousState)
{
    gerbv_netstate_t *newState = g_new0 (gerbv_netstate_t, 1);
    
    *newState = *previousState;
    previousState->next = newState;
    newState->scaleA = 1.0;
    newState->scaleB = 1.0;
    newState->next = NULL;
    
    return newState;
} /* gerbv_image_return_new_netstate */

gerbv_layer_t *
gerbv_image_duplicate_layer (gerbv_layer_t *oldLayer) {
    gerbv_layer_t *newLayer = g_new (gerbv_layer_t,1);
    
    *newLayer = *oldLayer;
    newLayer->name = g_strdup (oldLayer->name);
    return newLayer;
}

gerbv_netstate_t *
gerbv_image_duplicate_state (gerbv_netstate_t *oldState) {
    gerbv_netstate_t *newState = g_new (gerbv_netstate_t,1);
    
    *newState = *oldState;
    return newState;
}

gerbv_aperture_t *
gerbv_image_duplicate_aperture (gerbv_aperture_t *oldAperture){
    gerbv_aperture_t *newAperture = g_new0 (gerbv_aperture_t,1);
    gerbv_simplified_amacro_t *simplifiedMacro, *tempSimplified;
       
    *newAperture = *oldAperture;
	  
    /* delete the amacro section, since we really don't need it anymore
    now that we have the simplified section */
    newAperture->amacro = NULL;
    newAperture->simplified = NULL;

    /* copy any simplified macros over */
    tempSimplified = NULL;
    for (simplifiedMacro = oldAperture->simplified; simplifiedMacro != NULL; simplifiedMacro = simplifiedMacro->next) {
	gerbv_simplified_amacro_t *newSimplified = g_new0 (gerbv_simplified_amacro_t,1);
	*newSimplified = *simplifiedMacro;
	if (tempSimplified)
	  tempSimplified->next = newSimplified;
	else
	  newAperture->simplified = newSimplified;
	tempSimplified = newSimplified;
    }
    return newAperture;
}

void
gerbv_image_copy_all_nets (gerbv_image_t *sourceImage, gerbv_image_t *newImage, gerbv_layer_t *lastLayer,
		gerbv_netstate_t *lastState, gerbv_net_t *lastNet, gerbv_user_transformation_t *transform,
		GArray *translationTable){
    gerbv_netstate_t *oldState,*newSavedState;
    gerbv_layer_t *oldLayer,*newSavedLayer;
    gerbv_net_t *currentNet,*newNet,*newSavedNet;
    int i;
    
    oldLayer = sourceImage->layers;
    oldState = sourceImage->states;
    
    newSavedLayer = lastLayer;
    newSavedState = lastState;
    newSavedNet = lastNet;
    
    for (currentNet = sourceImage->netlist; currentNet; currentNet = currentNet->next){
      /* check for any new layers and duplicate them if needed */
	if (currentNet->layer != oldLayer) {
	  newSavedLayer->next = gerbv_image_duplicate_layer (currentNet->layer);
	  newSavedLayer = newSavedLayer->next;
	}
	/* check for any new states and duplicate them if needed */
	if (currentNet->state != oldState) {
	  newSavedState->next = gerbv_image_duplicate_state (currentNet->state);
	  newSavedState = newSavedState->next;
      }
      /* create and copy the actual net over */
      newNet = g_new (gerbv_net_t,1);
      *newNet = *currentNet;
      
      if (currentNet->cirseg) {
      	newNet->cirseg = g_new (gerbv_cirseg_t,1);
      	*(newNet->cirseg) = *(currentNet->cirseg);
      }
      
      if (currentNet->label)
      	newNet->label = g_string_new(currentNet->label->str);
      
      newNet->state = newSavedState;
      newNet->layer = newSavedLayer;
      /* check if we need to translate the aperture number */
      if (translationTable) {
        for (i=0; i<translationTable->len; i++){
          gerb_translation_entry_t translationEntry=g_array_index (translationTable, gerb_translation_entry_t, i);
          
          if (translationEntry.oldAperture == newNet->aperture) {
            newNet->aperture = translationEntry.newAperture;
            break;
          }
        }
      }
      /* check if we are transforming the net (translating, scaling, etc) */
      if (transform) {
        newNet->start_x += transform->translateX;
        newNet->start_y += transform->translateY;
        newNet->stop_x += transform->translateX;
        newNet->stop_y += transform->translateY;
        if (newNet->cirseg) {
          newNet->cirseg->cp_x += transform->translateX;
          newNet->cirseg->cp_y += transform->translateY;
        }
      }
      if (newSavedNet)
      	newSavedNet->next = newNet;
      else
      	newImage->netlist = newNet;
      newSavedNet = newNet;
    }		
		
}

gint
gerbv_image_find_existing_aperture_match (gerbv_aperture_t *checkAperture, gerbv_image_t *imageToSearch) {
    int i,j;
    gboolean isMatch;
    
    for (i = 0; i < APERTURE_MAX; i++) {
	if (imageToSearch->aperture[i] != NULL) {
	  if ((imageToSearch->aperture[i]->type == checkAperture->type) &&
	      (imageToSearch->aperture[i]->simplified == NULL) &&
	      (imageToSearch->aperture[i]->unit == checkAperture->unit)) {
	    /* check all parameters match too */
	    isMatch=TRUE;
	    for (j=0; j<APERTURE_PARAMETERS_MAX; j++){
	      if (imageToSearch->aperture[i]->parameter[j] != checkAperture->parameter[j])
	        isMatch = FALSE;
	    }
	    if (isMatch)
	      return i;
	  }	      
	}
    }
    return 0;
}

int
gerbv_image_find_unused_aperture_number (int startIndex, gerbv_image_t *image){
    int i;
    
    for (i = startIndex; i < APERTURE_MAX; i++) {
	if (image->aperture[i] == NULL) {
	  return i;
	}
    }
    return -1;
}

gerbv_image_t *
gerbv_image_duplicate_image (gerbv_image_t *sourceImage, gerbv_user_transformation_t *transform) {
    gerbv_image_t *newImage = gerbv_create_image(NULL, sourceImage->info->type);
    int i;
    int lastUsedApertureNumber = APERTURE_MIN - 1;
    GArray *apertureNumberTable = g_array_new(FALSE,FALSE,sizeof(gerb_translation_entry_t));
    
    newImage->layertype = sourceImage->layertype;
    /* copy information layer over */
    *(newImage->info) = *(sourceImage->info);
    newImage->info->name = g_strdup (sourceImage->info->name);
    newImage->info->type = g_strdup (sourceImage->info->type);
    newImage->info->plotterFilm = g_strdup (sourceImage->info->plotterFilm);
    newImage->info->attr_list = gerbv_attribute_dup (sourceImage->info->attr_list,
    		 sourceImage->info->n_attr);
    
    /* copy apertures over, compressing all the numbers down for a cleaner output, and
       moving and apertures less than 10 up to the correct range */
    for (i = 0; i < APERTURE_MAX; i++) {
	if (sourceImage->aperture[i] != NULL) {
	  gerbv_aperture_t *newAperture = gerbv_image_duplicate_aperture (sourceImage->aperture[i]);

	  lastUsedApertureNumber = gerbv_image_find_unused_aperture_number (lastUsedApertureNumber + 1, newImage);
	  /* store the aperture numbers (new and old) in the translation table */
	  gerb_translation_entry_t translationEntry={i,lastUsedApertureNumber};
	  g_array_append_val (apertureNumberTable,translationEntry);

	  newImage->aperture[lastUsedApertureNumber] = newAperture;
	}
    }
    
    /* step through all nets and create new layers and states on the fly, since
       we really don't have any other way to figure out where states and layers are used */
    gerbv_image_copy_all_nets (sourceImage, newImage, newImage->layers, newImage->states, NULL, transform, apertureNumberTable);
    g_array_free (apertureNumberTable, TRUE);
    return newImage;
}

void
gerbv_image_copy_image (gerbv_image_t *sourceImage, gerbv_user_transformation_t *transform, gerbv_image_t *destinationImage) {
    int lastUsedApertureNumber = APERTURE_MIN - 1;
    int i;
    GArray *apertureNumberTable = g_array_new(FALSE,FALSE,sizeof(gerb_translation_entry_t));
    
    /* copy apertures over */
    for (i = 0; i < APERTURE_MAX; i++) {
	if (sourceImage->aperture[i] != NULL) {
	  gint existingAperture = gerbv_image_find_existing_aperture_match (sourceImage->aperture[i], destinationImage);
	  
	  /* if we already have an existing aperture in the destination image that matches what
	     we want, just use it instead */
	  if (existingAperture > 0) {
	  	gerb_translation_entry_t translationEntry={i,existingAperture};
	 	g_array_append_val (apertureNumberTable,translationEntry);
	  }
	  /* else, create a new aperture and put it in the destination image */
	  else {
	  	gerbv_aperture_t *newAperture = gerbv_image_duplicate_aperture (sourceImage->aperture[i]);
	  
	  	lastUsedApertureNumber = gerbv_image_find_unused_aperture_number (lastUsedApertureNumber + 1, destinationImage);
	  	/* store the aperture numbers (new and old) in the translation table */
	  	gerb_translation_entry_t translationEntry={i,lastUsedApertureNumber};
	  	g_array_append_val (apertureNumberTable,translationEntry);

	  	destinationImage->aperture[lastUsedApertureNumber] = newAperture;
	  }
	}
    }
    /* find the last layer, state, and net in the linked chains */
    gerbv_netstate_t *lastState;
    gerbv_layer_t *lastLayer;
    gerbv_net_t *lastNet;
    
    for (lastState = destinationImage->states; lastState->next; lastState=lastState->next){}
    for (lastLayer = destinationImage->layers; lastLayer->next; lastLayer=lastLayer->next){}
    for (lastNet = destinationImage->netlist; lastNet->next; lastNet=lastNet->next){}
    
    /* and then copy them all to the destination image, using the aperture translation table we just built */
    gerbv_image_copy_all_nets (sourceImage, destinationImage, lastLayer, lastState, lastNet, transform, apertureNumberTable);
    g_array_free (apertureNumberTable, TRUE);
}

void
gerbv_image_delete_net (gerbv_net_t *currentNet) {
	gerbv_net_t *tempNet;
	
	g_assert (currentNet);
	/* we have a match, so just zero out all the important data fields */
	currentNet->aperture = 0;
	currentNet->aperture_state = GERBV_APERTURE_STATE_OFF;
	
	/* if this is a polygon start, we need to erase all the rest of the
		 nets in this polygon too */
	if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_START){
		for (tempNet = currentNet->next; tempNet; tempNet = tempNet->next){	
			tempNet->aperture = 0;
			tempNet->aperture_state = GERBV_APERTURE_STATE_OFF;
			
			if (tempNet->interpolation == GERBV_INTERPOLATION_PAREA_END) {
				tempNet->interpolation = GERBV_INTERPOLATION_DELETED;
				break;
			}
			/* make sure we don't leave a polygon interpolation in, since
		  	 it will still draw if it is */
			tempNet->interpolation = GERBV_INTERPOLATION_DELETED;
		}
	}
	/* make sure we don't leave a polygon interpolation in, since
	   it will still draw if it is */
	currentNet->interpolation = GERBV_INTERPOLATION_DELETED;
}

void
gerbv_image_delete_selected_nets (gerbv_image_t *sourceImage, GArray *selectedNodeArray) {
	int i;
	gerbv_net_t *currentNet;
    
	for (currentNet = sourceImage->netlist; currentNet; currentNet = currentNet->next){	
		for (i=0; i<selectedNodeArray->len; i++){
			gerbv_selection_item_t sItem = g_array_index (selectedNodeArray,
				gerbv_selection_item_t, i);
			if (sItem.net == currentNet) {
				gerbv_image_delete_net (currentNet);
				
			}
		}
	}
}

void
gerbv_image_create_rectangle_object (gerbv_image_t *image, gdouble coordinateX,
		gdouble coordinateY, gdouble width, gdouble height) {
	gerbv_net_t *currentNet;
	
	/* run through and find last net pointer */
	for (currentNet = image->netlist; currentNet->next; currentNet = currentNet->next){}
	
	/* create the polygon start node */
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_PAREA_START;
	
	/* go to start point (we need this to create correct RS274X export code) */
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_LINEARx1;
	currentNet->aperture_state = GERBV_APERTURE_STATE_OFF;
	currentNet->start_x = coordinateX;
	currentNet->start_y = coordinateY;
	currentNet->stop_x = coordinateX;
	currentNet->stop_y = coordinateY;
	
	/* draw the 4 corners */
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_LINEARx1;
	currentNet->aperture_state = GERBV_APERTURE_STATE_ON;
	currentNet->start_x = coordinateX;
	currentNet->start_y = coordinateY;
	currentNet->stop_x = coordinateX + width;
	currentNet->stop_y = coordinateY;
	gerber_update_min_and_max (&currentNet->boundingBox,currentNet->stop_x,currentNet->stop_y, 
		0,0,0,0);
	gerber_update_image_min_max (&currentNet->boundingBox, 0, 0, image);
		
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_LINEARx1;
	currentNet->aperture_state = GERBV_APERTURE_STATE_ON;
	currentNet->stop_x = coordinateX + width;
	currentNet->stop_y = coordinateY + height;
	gerber_update_min_and_max (&currentNet->boundingBox,currentNet->stop_x,currentNet->stop_y, 
		0,0,0,0);
	gerber_update_image_min_max (&currentNet->boundingBox, 0, 0, image);
	
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_LINEARx1;
	currentNet->aperture_state = GERBV_APERTURE_STATE_ON;
	currentNet->stop_x = coordinateX;
	currentNet->stop_y = coordinateY + height;
	gerber_update_min_and_max (&currentNet->boundingBox,currentNet->stop_x,currentNet->stop_y, 
		0,0,0,0);
	gerber_update_image_min_max (&currentNet->boundingBox, 0, 0, image);
	
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_LINEARx1;
	currentNet->aperture_state = GERBV_APERTURE_STATE_ON;
	currentNet->stop_x = coordinateX;
	currentNet->stop_y = coordinateY;
	gerber_update_min_and_max (&currentNet->boundingBox,currentNet->stop_x,currentNet->stop_y, 
		0,0,0,0);
	gerber_update_image_min_max (&currentNet->boundingBox, 0, 0, image);
	
	/* create the polygon end node */
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_PAREA_END;
	
	return;
}

gerbv_net_t *
gerb_image_return_aperture_index (gerbv_image_t *image, gdouble lineWidth, int *apertureIndex){
	gerbv_net_t *currentNet;
	gerbv_aperture_t *aperture=NULL;
	int i;
		
	/* run through and find last net pointer */
	for (currentNet = image->netlist; currentNet->next; currentNet = currentNet->next){}
	
	/* try to find an existing aperture that matches the requested width and type */
	for (i = 0; i < APERTURE_MAX; i++) {
		if (image->aperture[i] != NULL) {
			if ((image->aperture[i]->type == GERBV_APTYPE_CIRCLE) && 
				(fabs (image->aperture[i]->parameter[0] - lineWidth) < 0.001)){
				aperture = image->aperture[i];
				*apertureIndex = i;
				break;
			}
		}
	}

	if (!aperture) {
		/* we didn't find a useable old aperture, so create a new one */
		if (!gerber_create_new_aperture (image, apertureIndex,
				GERBV_APTYPE_CIRCLE, lineWidth, 0)) {
			/* if we didn't succeed, then return */
			return FALSE;
		}
	}
	return currentNet;
}

void
gerbv_image_create_arc_object (gerbv_image_t *image, gdouble centerX, gdouble centerY,
		gdouble radius, gdouble startAngle, gdouble endAngle, gdouble lineWidth,
		gerbv_aperture_type_t apertureType) {
	int apertureIndex;
	gerbv_net_t *currentNet;
	gerbv_cirseg_t cirSeg = {centerX, centerY, radius, radius, startAngle, endAngle};
	
	currentNet = gerb_image_return_aperture_index(image, lineWidth, &apertureIndex);
	
	if (!currentNet)
		return;
	
	/* draw the arc */
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_CCW_CIRCULAR;
	currentNet->aperture_state = GERBV_APERTURE_STATE_ON;
	currentNet->aperture = apertureIndex;
	currentNet->start_x = centerX + (cos(startAngle*M_PI/180) * radius);
	currentNet->start_y = centerY + (sin(startAngle*M_PI/180) * radius);
	currentNet->stop_x = centerX + (cos(endAngle*M_PI/180) * radius);
	currentNet->stop_y = centerY + (sin(endAngle*M_PI/180) * radius);;
	currentNet->cirseg = g_new0 (gerbv_cirseg_t,1);
	*(currentNet->cirseg) = cirSeg;
	
	gdouble angleDiff = currentNet->cirseg->angle2 - currentNet->cirseg->angle1;
	gint i, steps = abs(angleDiff);
	for (i=0; i<=steps; i++){
		gdouble tempX = currentNet->cirseg->cp_x + currentNet->cirseg->width / 2.0 *
				 cos ((currentNet->cirseg->angle1 +
				 (angleDiff * i) / steps)*M_PI/180);
		gdouble tempY = currentNet->cirseg->cp_y + currentNet->cirseg->width / 2.0 *
				 sin ((currentNet->cirseg->angle1 +
				 (angleDiff * i) / steps)*M_PI/180);
		gerber_update_min_and_max (&currentNet->boundingBox,
			       tempX, tempY, 
			       lineWidth/2,lineWidth/2,
			       lineWidth/2,lineWidth/2);
	}
	gerber_update_image_min_max (&currentNet->boundingBox, 0, 0, image);	
	return;
}

void
gerbv_image_create_line_object (gerbv_image_t *image, gdouble startX, gdouble startY,
		gdouble endX, gdouble endY, gdouble lineWidth, gerbv_aperture_type_t apertureType) {
	int apertureIndex;
	gerbv_net_t *currentNet;
	
	currentNet = gerb_image_return_aperture_index(image, lineWidth, &apertureIndex);
	
	if (!currentNet)
		return;
	
	/* draw the line */
	currentNet = gerber_create_new_net (currentNet, NULL, NULL);
	currentNet->interpolation = GERBV_INTERPOLATION_LINEARx1;
	
	/* if the start and end coordinates are the same, use a "flash" aperture state */
	if ((fabs(startX - endX) < 0.001) && (fabs(startY - endY) < 0.001))
		currentNet->aperture_state = GERBV_APERTURE_STATE_FLASH;
	else
		currentNet->aperture_state = GERBV_APERTURE_STATE_ON;
	currentNet->aperture = apertureIndex;
	currentNet->start_x = startX;
	currentNet->start_y = startY;
	currentNet->stop_x = endX;
	currentNet->stop_y = endY;

	gerber_update_min_and_max (&currentNet->boundingBox,currentNet->stop_x,currentNet->stop_y, 
		lineWidth/2,lineWidth/2,lineWidth/2,lineWidth/2);
	gerber_update_min_and_max (&currentNet->boundingBox,currentNet->start_x,currentNet->start_y, 
		lineWidth/2,lineWidth/2,lineWidth/2,lineWidth/2);
	gerber_update_image_min_max (&currentNet->boundingBox, 0, 0, image);
	return;
}

void
gerbv_image_create_window_pane_objects (gerbv_image_t *image, gdouble lowerLeftX,
		gdouble lowerLeftY, gdouble width, gdouble height, gdouble areaReduction,
		gint paneRows, gint paneColumns, gdouble paneSeparation){
	int i,j;
	gdouble startX,startY,boxWidth,boxHeight;
	
	startX = lowerLeftX + (areaReduction * width) / 2.0;
	startY = lowerLeftY + (areaReduction * height) / 2.0;
	boxWidth = (width * (1.0 - areaReduction) - (paneSeparation * (paneColumns - 1))) / paneColumns;
	boxHeight = (height * (1.0 - areaReduction) - (paneSeparation * (paneRows - 1))) / paneRows;
	
	for (i=0; i<paneColumns; i++){
		for (j=0; j<paneRows; j++) {
			gerbv_image_create_rectangle_object (image, startX + (i * (boxWidth + paneSeparation)),
				startY + (j * (boxHeight + paneSeparation)),boxWidth, boxHeight);
		}
	}

	return;
}
		
gboolean
gerbv_image_reduce_area_of_selected_objects (GArray *selectionArray,
		gdouble areaReduction, gint paneRows, gint paneColumns, gdouble paneSeparation){
	int i;
	gdouble minX,minY,maxX,maxY;
	
	for (i=0; i<selectionArray->len; i++) {
		gerbv_selection_item_t sItem = g_array_index (selectionArray,gerbv_selection_item_t, i);
		gerbv_image_t *image = sItem.image;
		gerbv_net_t *currentNet = sItem.net;
		
		/* determine the object type first */
		minX = HUGE_VAL;
		maxX = -HUGE_VAL;
		minY = HUGE_VAL;
		maxY = -HUGE_VAL;
		
		if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_START) {
			/* if it's a polygon, just determine the overall area of it and delete it */
			currentNet->interpolation = GERBV_INTERPOLATION_DELETED;
			
			for (currentNet = currentNet->next; currentNet; currentNet = currentNet->next){
				if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_END)
					break;
				currentNet->interpolation = GERBV_INTERPOLATION_DELETED;
				if (currentNet->stop_x < minX)
					minX = currentNet->stop_x;
				if (currentNet->stop_y < minY)
					minY = currentNet->stop_y;
				if (currentNet->stop_x > maxX)
					maxX = currentNet->stop_x;
				if (currentNet->stop_y > maxY)
					maxY = currentNet->stop_y;
			}
			currentNet->interpolation = GERBV_INTERPOLATION_DELETED;
		}
		else if ((currentNet->interpolation == GERBV_INTERPOLATION_x10) ||
				(currentNet->interpolation == GERBV_INTERPOLATION_LINEARx01) ||
				(currentNet->interpolation == GERBV_INTERPOLATION_LINEARx001) ||
				(currentNet->interpolation == GERBV_INTERPOLATION_LINEARx1)) {
			gdouble dx=0,dy=0;
			/* figure out the overall size of this element */
			switch (image->aperture[currentNet->aperture]->type) {
				case GERBV_APTYPE_CIRCLE :
				case GERBV_APTYPE_OVAL :
				case GERBV_APTYPE_POLYGON :
					dx = dy = image->aperture[currentNet->aperture]->parameter[0];
					break;
				case GERBV_APTYPE_RECTANGLE :
					dx = (image->aperture[currentNet->aperture]->parameter[0]/ 2);
					dy = (image->aperture[currentNet->aperture]->parameter[1]/ 2);
					break;
				default :
					break;
			}
			if (currentNet->start_x-dx < minX)
				minX = currentNet->start_x-dx;
			if (currentNet->start_y-dy < minY)
				minY = currentNet->start_y-dy;
			if (currentNet->start_x+dx > maxX)
				maxX = currentNet->start_x+dx;
			if (currentNet->start_y+dy > maxY)
				maxY = currentNet->start_y+dy;
				
			if (currentNet->stop_x-dx < minX)
				minX = currentNet->stop_x-dx;
			if (currentNet->stop_y-dy < minY)
				minY = currentNet->stop_y-dy;
			if (currentNet->stop_x+dx > maxX)
				maxX = currentNet->stop_x+dx;
			if (currentNet->stop_y+dy > maxY)
				maxY = currentNet->stop_y+dy;
			
			/* finally, delete node */
			currentNet->interpolation = GERBV_INTERPOLATION_DELETED;
		}
		/* we don't current support arcs */
		else
			return FALSE;
		
		/* create new structures */
		gerbv_image_create_window_pane_objects (image, minX, minY, maxX - minX, maxY - minY,
			areaReduction, paneRows, paneColumns, paneSeparation);
	}
	return TRUE;
}

gboolean
gerbv_image_move_selected_objects (GArray *selectionArray, gdouble translationX,
		gdouble translationY) {
	int i;
	
	for (i=0; i<selectionArray->len; i++) {
		gerbv_selection_item_t sItem = g_array_index (selectionArray,gerbv_selection_item_t, i);
		gerbv_net_t *currentNet = sItem.net;

		if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_START) {
			/* if it's a polygon, step through every vertex and translate the point */
			for (currentNet = currentNet->next; currentNet; currentNet = currentNet->next){
				if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_END)
					break;
				currentNet->start_x += translationX;
				currentNet->start_y += translationY;
				currentNet->stop_x += translationX;
				currentNet->stop_y += translationY;
			}
		}
		else {
			/* otherwise, just move the single element */
			currentNet->start_x += translationX;
			currentNet->start_y += translationY;
			currentNet->stop_x += translationX;
			currentNet->stop_y += translationY;
		}
	}
	return TRUE;
}

gerbv_net_t *
gerbv_image_return_next_renderable_object (gerbv_net_t *oldNet) {
	gerbv_net_t *currentNet=oldNet;
	
	if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_START) {
		/* if it's a polygon, step to the next non-polygon net */
		for (currentNet = currentNet->next; currentNet; currentNet = currentNet->next){
			if (currentNet->interpolation == GERBV_INTERPOLATION_PAREA_END) {
				return currentNet->next;
			}
		}
		return NULL;
	}
	else {
		return currentNet->next;
	}
}

void
gerbv_image_create_dummy_apertures (gerbv_image_t *parsed_image) {
	gerbv_net_t *currentNet;
		
	/* run through and find last net pointer */
	for (currentNet = parsed_image->netlist; currentNet->next; currentNet = currentNet->next){
		if (parsed_image->aperture[currentNet->aperture] == NULL) {
			parsed_image->aperture[currentNet->aperture] = g_new0 (gerbv_aperture_t, 1);
			parsed_image->aperture[currentNet->aperture]->type = GERBV_APTYPE_CIRCLE;
			parsed_image->aperture[currentNet->aperture]->parameter[0] = 0;
			parsed_image->aperture[currentNet->aperture]->parameter[1] = 0;
		}
	}
}
