/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
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
       	GdkGeometry 		geometry;
        GtkWidget               *layer_active;
        int                     layer_select_active;
        GtkListStore     	*model;
	GtkTreeSelection 	*selection;
	GtkTreeIter       	iter;
	gboolean  	  	is_gail_loaded;
} interface;

void
create_search_window(GtkWidget *widget, gpointer data);



/*
gchar *
build_search_command 		(void);

void
spawn_search_command 		(gchar *command);

void  		
add_constraint 			(gint constraint_id,
				 gchar *value,
				 gboolean show_constraint);

void  		
remove_constraint 		(gint constraint_id);

void  		
update_constraint_info 		(SearchConstraint *constraint, 
				 gchar *info);
void
set_constraint_selected_state	(gint		constraint_id, 
				 gboolean	state);
void
set_constraint_gconf_boolean 	(gint 		constraint_id, 
				 gboolean 	flag);
gboolean
update_progress_bar 		(gpointer data);

gboolean
update_animation_timeout_cb 	(gpointer data);

void
set_clone_command		(gint *argcp,
				 gchar ***argvp,
				 gpointer client_data,
				 gboolean escape_values);
gchar *
get_desktop_item_name 		(void);
*/

gboolean
file_extension_is 		(const char *filename, 
		   		 const char *ext);
gboolean  	
compare_regex	 		(const gchar *regex, 
				 const gchar *string);
gint 	 	
count_of_char_in_string		(const gchar *string, 
				 const gchar q);
gchar *   	
get_readable_date 		(const time_t file_time_raw);

gchar *    	
strdup_strftime 		(const gchar *format, 
				 struct tm *time_pieces); 
			      
GtkWidget *
gsearchtool_hig_dialog_new      (GtkWindow      *parent,
                                 GtkDialogFlags flags,
                                 GtkButtonsType buttons,
                                 const gchar    *header,
                                 const gchar    *message);
				 			      

/* gsearchtool-callbacks */

			 						 
void	
quit_cb 			(GtkWidget 	*widget, 
				 gpointer 	data);
void    
help_cb 			(GtkWidget 	*widget, 
				 gpointer 	data);
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
add_constraint_cb		(GtkWidget 	*widget, 
				 gpointer 	data);
void    
remove_constraint_cb		(GtkWidget 	*widget, 
				 gpointer 	data);
void    
constraint_activate_cb 		(GtkWidget 	*widget, 
				 gpointer 	data);
void    
constraint_update_info_cb 	(GtkWidget 	*widget, 
				 gpointer 	data);	
void    
constraint_menu_item_activate_cb(GtkWidget 	*widget,
                                 gpointer 	data);
void    
constraint_entry_changed_cb 	(GtkWidget 	*widget, 
				 gpointer 	data);
void    
file_is_named_activate_cb 	(GtkWidget 	*widget, 
				 gpointer 	data);
void 	
open_file_cb 			(GtkWidget 	*widget, 
				 gpointer 	data);
void	
open_folder_cb 			(GtkWidget 	*widget, 

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
void    
show_file_selector_cb 		(GtkWidget 	*widget,
				 gpointer 	data);
void   	
save_results_cb 		(GtkFileSelection   *selector, 
				 gpointer 	    user_data, 
				 gpointer 	    data);
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
gboolean
not_running_timeout_cb 		(gpointer data);
#endif
