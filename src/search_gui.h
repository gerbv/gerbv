/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2004 Juergen Haas (juergenhaas@gmx.net)
 *
 * $Id$
 *  
 *                                      Juergen H. (juergenhaas@gmx.net) 
 *                                      and Tomasz M. (T.Motylewski@bfad.de)
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

#ifndef _SEARCH_GUI_H
#define _SEARCH_GUI_H
#ifdef USE_GTK2
#include "search.h"
#include "search_file.h"

#define MINIMUM_WINDOW_WIDTH   370
#define MINIMUM_WINDOW_HEIGHT  310

extern GtkListStore *completion_model;



/** lowercase entries are hidden from search window display*/
typedef enum {
	COLUMN_DESIGNATOR,
	COLUMN_footprint,
	COLUMN_mid_x,  
	COLUMN_mid_y,
	COLUMN_ref_x,
	COLUMN_ref_y,
	COLUMN_pad_x,
	COLUMN_pad_y,
        COLUMN_LAYER,
        COLUMN_rotation,
	COLUMN_length,
	COLUMN_width,
	COLUMN_shape,
        COLUMN_COMMENT,
        COLUMN_NO_FILES_FOUND,
	NUM_COLUMNS
} ResultColumn;

struct _InterfaceStruct {
	GtkWidget		*file_is_named_entry;
	GtkWidget		*find_button;
	GtkWidget 		*main_window;	
	GtkWidget		*table;
        GtkTreeViewColumn       *column;	
	GtkWidget 		*file_selector;
	GtkWidget		*check_designator;
        GtkWidget		*invert_selection;
        GtkWidget		*top_parts_selection;
        GtkWidget		*bottom_parts_selection;
        GtkWidget		*check_comment;
       	GtkWidget		*results_label;
	GtkWidget       	*results;
	GtkWidget        	*tree;
       	GdkGeometry 		 geometry;
        GtkWidget               *layer_active;
        int                      layer_select_active;
        GtkListStore     	*model;
        GList                   *PNP_entries_list;
	GtkTreeSelection 	*selection;
	GtkTreeIter       	 iter;
	gboolean  	  	 is_gail_loaded;
    char            *pnp_filename;
} interface;

void
create_search_window(GtkWidget *widget, gpointer data);

void
update_combo_box_model(void);


gboolean  	
compare_regex	 		(const gchar *regex, 
				 const gchar *string);
				 			      

/* gsearchtool-callbacks */


void   
click_find_cb	 		(GtkWidget 	*widget, 
				 gpointer 	data);
void
click_invert_selection_cb       (GtkWidget 	*widget,
                                 gpointer      *data);  
void
click_top_parts_selection_cb       (GtkWidget 	*widget,
                                 gpointer      *data); 
void
click_bottom_parts_selection_cb       (GtkWidget 	*widget,
                                 gpointer      *data);  
                                 
void
click_layer_active_cb           (GtkWidget 	*widget,
                                 gpointer      *data);                               
void
click_stop_cb 			(GtkWidget 	*widget, 
	       			 gpointer 	data);
void
click_check_button_cb		(GtkWidget	*widget, 
				 gpointer 	data);
                 
void    
file_is_named_activate_cb 	(GtkWidget 	*widget, 
				 gpointer 	data);

/*                                 
void  
drag_begin_file_cb  		(GtkWidget          *widget,
	      			 GdkDragContext     *context,
	       			 GtkSelectionData   *selection_data,
	       			 guint               info,
	       			 guint               time,
	       			 gpointer            data);
void  
drag_file_cb  			(GtkWidget          *widget,
	      			 GdkDragContext     *context,
	       			 GtkSelectionData   *selection_data,
	       			 guint               info,
	       			 guint               time,
	       			 gpointer            data);
void  
drag_data_animation_cb		(GtkWidget          *widget,
	      			 GdkDragContext     *context,
	       			 GtkSelectionData   *selection_data,
	       			 guint               info,
	       			 guint               time,
	       			 gpointer            data);
*/                                 

gboolean  
key_press_cb 			(GtkWidget 	*widget, 
				 GdkEventKey 	*event,
				 gpointer 	data);	
gboolean   
file_is_named_entry_key_press_cb 	(GtkWidget 	*widget, 
				 GdkEventKey    *event,
				 gpointer 	data);
				 
gboolean
file_button_release_event_cb	(GtkWidget 	*widget,
				 GdkEventButton *event,
				 gpointer 	data);

gboolean
file_event_after_cb	        (GtkWidget 	*widget,
				 GdkEventButton *event,
				 gpointer 	data);  
gboolean	
file_button_press_event_cb	(GtkWidget 	*widget, 
				 GdkEventButton *event, 
				 gpointer 	data);
gboolean	
file_key_press_event_cb		(GtkWidget 	*widget,
				 GdkEventKey    *event,
				 gpointer 	data);

#endif

#endif
