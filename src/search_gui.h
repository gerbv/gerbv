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

#include "search.h"
#include "search_file.h"

#define MINIMUM_WINDOW_WIDTH   370
#define MINIMUM_WINDOW_HEIGHT  310

/** Stores options for autocompleting search entries.
    handles the autocompletion functionality when entering search strings (search and select parts dialog).\n
    list contains only the designators and comments in contrast to parsed_PNP_data, which holds all 11 columns of a pick and place file.\n
    
    @see parsed_PNP_data */
extern GtkListStore *completion_model;



/** lowercase entries are hidden from search window display.*/
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

/** Handles the search dialog/interface.
struct _InterfaceStruct {\n
	GtkWidget		  *file_is_named_entry;\n
	GtkWidget		  *find_button;\n
	GtkWidget 		  *main_window;\n	
	GtkWidget		  *table;\n
    GtkTreeViewColumn *column;\n	
	GtkWidget 		  *file_selector;\n
	GtkWidget		  *check_designator;\n
    GtkWidget		  *invert_selection;\n
    GtkWidget		  *top_parts_selection;\n
    GtkWidget		  *bottom_parts_selection;\n
    GtkWidget		  *check_comment;\n
    GtkWidget		  *results_label;\n
	GtkWidget         *results;\n
	GtkWidget         *tree;\n
    GdkGeometry 	   geometry;\n
    GtkWidget         *layer_active;\n
    int                layer_select_active;\n
    GtkListStore      *model;\n
    GList             *PNP_entries_list;\n
	GtkTreeSelection  *selection;\n
	GtkTreeIter        iter;\n
    char              *pnp_filename;\n
} interface;
 */
struct _InterfaceStruct {
	GtkWidget		  *file_is_named_entry;
	GtkWidget		  *find_button;
	GtkWidget 		  *main_window;	
	GtkWidget		  *table;
    GtkTreeViewColumn *column;	
	GtkWidget 		  *file_selector;
	GtkWidget		  *check_designator;
    GtkWidget		  *invert_selection;
    GtkWidget		  *top_parts_selection;
    GtkWidget		  *bottom_parts_selection;
    GtkWidget		  *check_comment;
    GtkWidget		  *results_label;
	GtkWidget         *results;
	GtkWidget         *tree;
    GdkGeometry 	   geometry;
    GtkWidget         *layer_active;
    int                layer_select_active;
    GtkListStore      *model;
    GList             *PNP_entries_list;
	GtkTreeSelection  *selection;
	GtkTreeIter        iter;
    char              *pnp_filename;
} interface;

void
create_search_window(GtkWidget *widget, gpointer data);

void
update_combo_box_model(void);


gboolean  	
compare_regex	 		(const gchar *regex, 
				 const gchar *string);
				 			      

/** search dialog callbacks.
void   \n
click_find_cb	 		(GtkWidget 	*widget, \n
				 gpointer 	data);\n
void\n
click_invert_selection_cb       (GtkWidget 	*widget,\n
                                 gpointer      *data);\n  
void\n
click_top_parts_selection_cb       (GtkWidget 	*widget,\n
                                 gpointer      *data); \n
void\n
click_bottom_parts_selection_cb       (GtkWidget 	*widget,\n
                                 gpointer      *data);  \n
                                 
void\n
click_layer_active_cb           (GtkWidget 	*widget,\n
                                 gpointer      *data);\n
void\n
click_stop_cb 			(GtkWidget 	*widget, \n
	       			 gpointer 	data);\n
void\n
click_check_button_cb		(GtkWidget	*widget, \n
				 gpointer 	data);\n
                 
void\n
file_is_named_activate_cb 	(GtkWidget 	*widget, \n
				 gpointer 	data);\n

gboolean  \n
key_press_cb 			(GtkWidget 	*widget, \n
				 GdkEventKey 	*event,\n
				 gpointer 	data);	\n
gboolean   \n
file_is_named_entry_key_press_cb 	(GtkWidget 	*widget, \n
				 GdkEventKey    *event,\n
				 gpointer 	data);\n
				 
gboolean \n
file_button_release_event_cb	(GtkWidget 	*widget,\n
				 GdkEventButton *event,\n
				 gpointer 	data);\n

gboolean\n
file_event_after_cb	        (GtkWidget 	*widget,\n
				 GdkEventButton *event,\n
				 gpointer 	data);  \n
gboolean	\n
file_button_press_event_cb	(GtkWidget 	*widget, \n
				 GdkEventButton *event, \n
				 gpointer 	data);\n
gboolean	\n
file_key_press_event_cb		(GtkWidget 	*widget,\n
				 GdkEventKey    *event,\n
				 gpointer 	data); */


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

#endif /* _SEARCH_GUI_H */
