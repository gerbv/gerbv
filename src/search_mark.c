/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
 *
 * $Id$
 *  
 *                                      Juergen H. (juergenhaas@gmx.net)and
 *                                      Tomasz M. (T.Motylewski@bfad.de)
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


/** @file search_mark.c
    @brief draws selected parts on screen
 */ 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */

#include <pango/pango.h>

#include <assert.h>

#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"
#include "draw.h"
#include "color.h"
#include "gerbv_screen.h"
#include "search.h"
#include "search_file.h"
#include "search_mark.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "search_gui.h"
/* CHECKME - here gi18n is disabled */
#define _(String) (String)


/**will actually create the layer set in the dialog containing all selected parts.
  *this is called whenever enter is pressed, Mark button is clicked or right-click/double click was recognised on a single part.
  *parts are differentiated between by using the PART_SHAPE_ identifier set in\n 
  *parse_pnp
  *@see parse_pnp()
  */
void create_marked_layer(int idx) {
    int            r, g, b;
    GtkStyle      *defstyle, *newstyle;
    gerb_net_t    *curr_net = NULL;
    gerb_image_t  *image = NULL;
    GList         *list;
    char          *tmp_name = NULL;
    gerb_transf_t *tr_rot = gerb_transf_new();
  //  GtkTreeIter   iter;
    
        
/*   if ((interface.selection != NULL) 
      && (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION(interface.selection)) == 0) 
      && (!screen.file[idx]->color)) {
  	    
	  return;
    }*/
    
    if(!screen.file[idx]) {
        screen.file[idx] = (gerbv_fileinfo_t *)malloc(sizeof(gerbv_fileinfo_t));
        memset((void *)screen.file[idx], 0, sizeof(gerbv_fileinfo_t));
        screen.file[idx]->name = tmp_name;
    }
    screen.file[idx]->image = new_gerb_image(screen.file[idx]->image);
    image = screen.file[idx]->image;
    if (image == NULL) {
	GERB_FATAL_ERROR("malloc image failed\n");
        return;
    }
    curr_net = image->netlist;

    if (!screen.file[idx]->color) {
      //  int   idx0;
      //  char  tmp_iter_str[MAXL];
        r = (12341 + 657371 * idx) % (int)(MAX_COLOR_RESOLUTION);
        g = (23473 + 434382 * idx) % (int)(MAX_COLOR_RESOLUTION);
        b = (90341 + 123393 * idx) % (int)(MAX_COLOR_RESOLUTION);

        screen.file[idx]->color = alloc_color(r, g, b, NULL);
        screen.file[idx]->inverted = 0;

        /* This code will remove the layer which the selection is drawn onto from
         * the available layers to be drawn on in future, dactivate, because the
         * number of layers decreases quite fast*/
         
        /* combo_box_model = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING); 
        for (idx0 =  0; idx0 < MAX_FILES; idx0++) {

            if (screen.file[idx0] == NULL) {
                gtk_list_store_append(combo_box_model, &iter);
                gtk_list_store_set (combo_box_model, &iter, 0, idx0, -1);
            } 
        }
        gtk_combo_box_set_model(GTK_COMBO_BOX(interface.layer_active), GTK_TREE_MODEL(combo_box_model));
        //sprintf(tmp_iter_str, "%i", idx);
        //gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(combo_box_model), &iter, tmp_iter_str);
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(combo_box_model), &iter);
        gtk_combo_box_set_active_iter   (GTK_COMBO_BOX(interface.layer_active), &iter);
        */
    }    

    /* 
     * Set color on layer button
     */
    defstyle = gtk_widget_get_default_style();
    newstyle = gtk_style_copy(defstyle);
    newstyle->bg[GTK_STATE_NORMAL] = *(screen.file[idx]->color);
    newstyle->bg[GTK_STATE_ACTIVE] = *(screen.file[idx]->color);
    newstyle->bg[GTK_STATE_PRELIGHT] = *(screen.file[idx]->color);
    gtk_widget_set_style(screen.layer_button[idx], newstyle);

    /* 
     * Tool tips on button is the file name 
     */
    gtk_tooltips_set_tip(screen.tooltips, screen.layer_button[idx],
			 "selected parts", NULL); 
              
    image->info->min_x = -1;
    image->info->min_y = -1;
    image->info->max_x = 5;
    image->info->max_y = 5;
    image->info->scale_factor_A = 1.0;
    image->info->scale_factor_B = 1.0;
    image->info->offset_a = 0.0;
    image->info->offset_b = 0.0;
    image->info->step_and_repeat.X = 1.0;
    image->info->step_and_repeat.Y = 1.0;
    image->info->step_and_repeat.dist_X = 0.0;
    image->info->step_and_repeat.dist_Y = 0.0;

    image->aperture[0] = (gerb_aperture_t *)malloc(sizeof(gerb_aperture_t));
    memset((void *) image->aperture[0], 0, sizeof(gerb_aperture_t));
    image->aperture[0]->type = CIRCLE;
    image->aperture[0]->amacro = NULL;
    image->aperture[0]->parameter[0] = 0.4;
    image->aperture[0]->parameter[1] = 0.0;
    image->aperture[0]->parameter[2] = 0.0;
    image->aperture[0]->parameter[3] = 0.0;
    image->aperture[0]->parameter[4] = 0.0;
    image->aperture[0]->nuf_parameters = 1;
    image->aperture[0]->unit = MM;

	    
    list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION(interface.selection),
							 (GtkTreeModel **)&interface.model);
    if (list == NULL) 
        return;                             
    list = g_list_first (list);
   
    do {
        gchar *designator, *footprint, *layer, *comment;
        double mid_x, mid_y, ref_x, ref_y, pad_x, pad_y, rotation, radius;
	    double length, width;
	    int shape;
        GtkTreeIter iter;
        gboolean no_files_found;
          
        gtk_tree_model_get_iter (GTK_TREE_MODEL(interface.model), &iter, 
			     list->data);
	 
        gtk_tree_model_get (GTK_TREE_MODEL(interface.model), &iter,
    			    COLUMN_DESIGNATOR, &designator,
                    COLUMN_footprint, &footprint,
		            COLUMN_mid_x, &mid_x,
                    COLUMN_mid_y, &mid_y,
                    COLUMN_ref_x, &ref_x,
                    COLUMN_ref_y, &ref_y,
                    COLUMN_pad_x, &pad_x,
                    COLUMN_pad_y, &pad_y,
                    COLUMN_LAYER, &layer,
                    COLUMN_rotation, &rotation,
			        COLUMN_length, &length,
			        COLUMN_width, &width,
			        COLUMN_shape, &shape,
                    COLUMN_COMMENT, &comment,
			        COLUMN_NO_FILES_FOUND, &no_files_found,
                                               -1);
        curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
        curr_net = curr_net->next;
        assert(curr_net);
        memset((void *)curr_net, 0, sizeof(gerb_net_t));
        rotation *= M_PI/180; /* convert deg to rad */

	    switch(shape) {
	    case PART_SHAPE_RECTANGLE:
        case PART_SHAPE_STD:
	    // TODO: draw rectangle length x width taking into account rotation or pad x,y
            gerb_transf_reset(tr_rot);
            gerb_transf_rotate(tr_rot, rotation);
            gerb_transf_shift(tr_rot, mid_x, mid_y);

            gerb_transf_apply(length/2, width/2, tr_rot, 
                &curr_net->start_x, &curr_net->start_y);
            gerb_transf_apply(-length/2, width/2, tr_rot, 
                &curr_net->stop_x, &curr_net->stop_y);
    
            curr_net->aperture = 0;
            curr_net->layer_polarity = POSITIVE;
            curr_net->unit = MM;
            curr_net->aperture_state = ON;
            curr_net->interpolation = LINEARx1;
            
            curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
            curr_net = curr_net->next;
            assert(curr_net);
            memset((void *)curr_net, 0, sizeof(gerb_net_t));
            
            gerb_transf_apply(-length/2, width/2, tr_rot, 
                &curr_net->start_x, &curr_net->start_y);
            gerb_transf_apply(-length/2, -width/2, tr_rot, 
                &curr_net->stop_x, &curr_net->stop_y);
                    
            curr_net->aperture = 0;
            curr_net->layer_polarity = POSITIVE;
            curr_net->unit = MM;
            curr_net->aperture_state = ON;
            curr_net->interpolation = LINEARx1;
            
            curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
            curr_net = curr_net->next;
            assert(curr_net);
            memset((void *)curr_net, 0, sizeof(gerb_net_t));
            
            gerb_transf_apply(-length/2, -width/2, tr_rot, 
                &curr_net->start_x, &curr_net->start_y);
            gerb_transf_apply(length/2, -width/2, tr_rot, 
                &curr_net->stop_x, &curr_net->stop_y);
                    
            curr_net->aperture = 0;
            curr_net->layer_polarity = POSITIVE;
            curr_net->unit = MM;
            curr_net->aperture_state = ON;
            curr_net->interpolation = LINEARx1;

            curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
            curr_net = curr_net->next;
            assert(curr_net);
            memset((void *)curr_net, 0, sizeof(gerb_net_t));
            
            gerb_transf_apply(length/2, -width/2, tr_rot, 
                &curr_net->start_x, &curr_net->start_y);
            gerb_transf_apply(length/2, width/2, tr_rot, 
                &curr_net->stop_x, &curr_net->stop_y);
                
            curr_net->aperture = 0;
            curr_net->layer_polarity = POSITIVE;
            curr_net->unit = MM;
            curr_net->aperture_state = ON;
            curr_net->interpolation = LINEARx1;

            curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
            curr_net = curr_net->next;
            assert(curr_net);
            memset((void *)curr_net, 0, sizeof(gerb_net_t));
            
            if (shape == PART_SHAPE_RECTANGLE) {
                gerb_transf_apply(length/4, -width/2, tr_rot, 
                    &curr_net->start_x, &curr_net->start_y);
                gerb_transf_apply(length/4, width/2, tr_rot, 
                    &curr_net->stop_x, &curr_net->stop_y);
            } else {
                gerb_transf_apply(length/4, width/2, tr_rot, 
                    &curr_net->start_x, &curr_net->start_y);
                gerb_transf_apply(length/4, width/4, tr_rot, 
                    &curr_net->stop_x, &curr_net->stop_y);
                    
                curr_net->aperture = 0;
                curr_net->layer_polarity = POSITIVE;
                curr_net->unit = MM;
                curr_net->aperture_state = ON;
                curr_net->interpolation = LINEARx1;
                
                curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
                curr_net = curr_net->next;
                assert(curr_net);
                memset((void *)curr_net, 0, sizeof(gerb_net_t));
                gerb_transf_apply(length/2, width/4, tr_rot, 
                    &curr_net->start_x, &curr_net->start_y);
                gerb_transf_apply(length/4, width/4, tr_rot, 
                    &curr_net->stop_x, &curr_net->stop_y);
                    
            }
                                
                
            curr_net->aperture = 0;
            curr_net->layer_polarity = POSITIVE;
            curr_net->unit = MM;
            curr_net->aperture_state = ON;
            curr_net->interpolation = LINEARx1;
            
            break;
            
  	case PART_SHAPE_UNKNOWN:
	default:

            curr_net->start_x = mid_x;
            curr_net->start_y = mid_y;
            curr_net->stop_x = pad_x;
            curr_net->stop_y = pad_y;
    
            curr_net->aperture = 0;
            curr_net->layer_polarity = POSITIVE;
            curr_net->unit = MM;
            curr_net->aperture_state = ON;
            curr_net->interpolation = LINEARx1;
        
            curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
            curr_net = curr_net->next;
            assert(curr_net);
            memset((void *)curr_net, 0, sizeof(gerb_net_t));
  

            curr_net->start_x = mid_x;
            curr_net->start_y = mid_y;
            curr_net->stop_x = pad_x;
            curr_net->stop_y = pad_y;
    
            curr_net->aperture = 0;
            curr_net->layer_polarity = POSITIVE;
            curr_net->unit = MM;
            curr_net->aperture_state = ON;
            curr_net->interpolation = CW_CIRCULAR;
            curr_net->cirseg = (gerb_cirseg_t *)malloc(sizeof(gerb_cirseg_t));
            memset((void *)curr_net->cirseg, 0, sizeof(gerb_cirseg_t));
            curr_net->cirseg->angle1 = 0.0;
            curr_net->cirseg->angle2 = 360.0;
            curr_net->cirseg->cp_x = mid_x;
            curr_net->cirseg->cp_y = mid_y;
            radius = sqrt((pad_x-mid_x)*(pad_x-mid_x) + (pad_y-mid_y)*(pad_y-mid_y));
            curr_net->cirseg->width = 2*radius; /* fabs(pad_x-mid_x) */
            curr_net->cirseg->height = 2*radius;
            break;
	}
                           
        
       // if (!no_files_found)   GERB_MESSAGE("%s %s: mid_x %f\n", designator, comment, mid_x);
        g_free(designator);
        g_free(footprint);
        g_free(layer);
        g_free(comment);      
        
    } while ((list = g_list_next(list)));
         
    curr_net->next = NULL;		    
  //  g_list_free (list);
    gerb_transf_free(tr_rot);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				 (screen.layer_button[idx]),TRUE); 
                              
    
} /* create_marked_layer */
