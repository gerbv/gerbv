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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef USE_GTK2

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <errno.h>
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif /* HAVE_REGEX_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gunicode.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */

#ifdef USE_GTK2
#include <pango/pango.h>
#endif /* USE_GTK2 */

#include <glib/gi18n.h>
#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"
#include "draw.h"
#include "color.h"
#include "gerbv_screen.h"
#include "gerbv_icon.h"
#include "search.h"
#include "search_cb.h"
#include "search_file.h"
#include "search_gui.h"
#include "search_mark.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "search_gui.h"
/* CHECKME - here gi18n is disabled */
//#define _(String) (String)

pnp_state_t  *parsed_PNP_data;

GtkListStore        *combo_box_model;
//GtkListStore        *model_completion; 
GtkEntryCompletion  *entry_completion;
char composed_entry[MAXL] = "";

void select_by_regex(GtkWidget    	*widget,  gpointer 		data) 
{
    GtkTreeIter       iter;
    gboolean          no_files_found = FALSE;
    gchar            *designator;
    gchar            *comment;
#ifdef HAVE_REGEX_H
    regex_t           regexec_pattern; 
#endif
    const gchar      *entry; 
    gchar             regex[MAXL]="";
    gboolean          match, nomatch;
    gboolean          designator_active, comment_active;
  //  int             not_in_list = 0;
    GList            *selection_list, *first_in_list;
    GtkTreeSelection  *tree_sel;
    

        
    if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
        GERB_MESSAGE("empty interface.model\n");
        return;
    }                        
    designator_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_designator));
    comment_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_comment));
    
    if (((!designator_active) && (!comment_active)) || ((designator_active) && (!comment_active))) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(interface.check_designator), TRUE);  
        designator_active = TRUE;
        entry_completion = gtk_entry_completion_new ();  
        gtk_entry_set_completion( GTK_ENTRY(interface.file_is_named_entry), entry_completion);
        gtk_entry_completion_set_model (entry_completion, GTK_TREE_MODEL(completion_model));
        gtk_entry_completion_set_text_column (entry_completion, 0); 
    } else if ((!designator_active) && (comment_active)) {
        entry_completion = gtk_entry_completion_new ();  
        gtk_entry_set_completion( GTK_ENTRY(interface.file_is_named_entry), entry_completion);
        gtk_entry_completion_set_model (entry_completion, GTK_TREE_MODEL(completion_model));
        gtk_entry_completion_set_text_column (entry_completion, 1); 
    }
    

    entry = gtk_entry_get_text(GTK_ENTRY(gtk_entry_completion_get_entry(entry_completion)));
    composed_entry[1] = '\0';
   /* if (entry == "*") {
        entry[0] = '.';
        entry[1] = '*';
        entry[2] = '\0';
    }*/
    if   (entry[0] != '\0') {
 
        /* This following code is working, it will add a new entry to the list, unfortunately it 
         * is not useful when selecting all R or similar bcause the new entries will not have the
         * necessary data
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(completion_model), &iter);
        do {
        gtk_tree_model_get (GTK_TREE_MODEL(completion_model), &iter, 0, &tmp_entry , -1);
        if (strncasecmp(tmp_entry, entry, strlen(tmp_entry)) != 0 ){
            not_in_list = 1;
           // GERB_MESSAGE("new in the gang %s\n", entry)
        }
       // GERB_MESSAGE("entry>%s\n",tmp_entry);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(completion_model), &iter));
        if (not_in_list == 1) {
            gtk_list_store_append (completion_model, &iter); 
            gtk_list_store_set (completion_model, &iter, 0, entry, -1);
            gtk_entry_completion_set_model (entry_completion, GTK_TREE_MODEL(completion_model));
        }Enter the users entries->problems with non existing data on a cumulative selection*/

        
        if(sscanf(entry, "%100s", regex)==1 && regex[0]) {
            // TODO transform * into .*, ? into . i.e. find char make sure it is 
            //single (and not already .*) and trafo it
#ifdef HAVE_REGEX_H
              regcomp (&regexec_pattern, regex , REG_ICASE);  //compiles regexp string
//              regcomp (&regexec_pattern, regex , REG_NOSUB);  //compiles regexp string
#endif
            nomatch = FALSE;
        } else {
            nomatch = TRUE;
        }
        do {
            gtk_tree_model_get (GTK_TREE_MODEL(interface.model), &iter,
                COLUMN_DESIGNATOR, &designator,
                COLUMN_COMMENT, &comment,
                COLUMN_NO_FILES_FOUND, &no_files_found,
	   	        -1);
	    match = FALSE;	  
              
            if(!nomatch) {
#ifdef HAVE_REGEX_H
                if ((designator_active) && (designator != NULL))
                    match = (regexec (&regexec_pattern, designator, 0, 0, 0) != REG_NOMATCH);
                if ((comment_active) && (comment != NULL))
                    match = match || (regexec (&regexec_pattern, g_locale_from_utf8(comment, -1, NULL, NULL, NULL), 0, 0, 0) != REG_NOMATCH);
#else 
                if ((designator_active) && (designator != NULL))
                    match = (strstr (designator, regex) != NULL);
                if ((comment_active) && (comment != NULL))
                    match = match || (strstr (g_locale_from_utf8(comment, -1, NULL, NULL, NULL), regex) != NULL);    
/*?use      g_utf8_strdown (const gchar *str, gssize len); before strstr and maybe g_strstr_len?*/
#endif     
            }
            if(match) {
                gtk_tree_selection_select_iter (GTK_TREE_SELECTION(interface.selection), &iter);
            } else {
                gtk_tree_selection_unselect_iter (GTK_TREE_SELECTION(interface.selection), &iter);
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter));	
   
        selection_list = gtk_tree_selection_get_selected_rows
                                                (GTK_TREE_SELECTION(interface.selection),
                                                 (GtkTreeModel **)&interface.model);
        if (selection_list != NULL) {                                         
            first_in_list = g_list_first(selection_list);                                             
        
            gtk_tree_view_scroll_to_cell    (GTK_TREE_VIEW(interface.tree),
                                                  first_in_list->data,
                                                  COLUMN_DESIGNATOR,
                                                  TRUE,
                                                  0.3,
                                                  0.5);
        }                                                  
     //   g_list_foreach (selection_list, gtk_tree_path_free, NULL);                                          
      //  g_list_free (selection_list);                                          
      //  g_list_foreach (first_in_list, gtk_tree_path_free, NULL);
      //  g_list_free (first_in_list);                                          
        g_free (designator);
        g_free (comment);
        } else {
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
            gtk_tree_selection_unselect_all (tree_sel);
            /*hitting enter on nothing entered will deselect all*/
        }
        return;
} /* select_by_regex */


GtkWidget *
create_search_results_section (void)
{
    GtkWidget 	      *label;
    GtkWidget 	      *vbox;
    GtkWidget	      *hbox;
    GtkWidget	      *window;	
    GtkTreeViewColumn *column;	
    GtkCellRenderer   *renderer;
    
           
   
    vbox = gtk_vbox_new (FALSE, 6);	
    
    hbox = gtk_hbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new_with_mnemonic (_("S_earch results:"));
    g_object_set (G_OBJECT(label), "xalign", 0.0, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    
    interface.results_label = gtk_label_new (NULL);
    g_object_set (G_OBJECT (interface.results_label), "xalign", 1.0, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), interface.results_label, FALSE, FALSE, 0);
    
    window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(window), GTK_SHADOW_NONE);	
    gtk_container_set_border_width (GTK_CONTAINER(window), 0);
    gtk_widget_set_size_request (window, 330, 160); 
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(window),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
/*interface.model = */   

     interface.model = gtk_list_store_new (NUM_COLUMNS, 
				      G_TYPE_STRING, // COLUMN_DESIGNATOR
				      G_TYPE_STRING, // COLUMN_footprint
                                      G_TYPE_DOUBLE, // COLUMN_mid_x
				      G_TYPE_DOUBLE, // COLUMN_mid_y
                                      G_TYPE_DOUBLE, // COLUMN_ref_x
                                      G_TYPE_DOUBLE, // COLUMN_ref_y
                                      G_TYPE_DOUBLE, // COLUMN_pad_x
                                      G_TYPE_DOUBLE, // COLUMN_pad_y
                                      G_TYPE_STRING, // COLUMN_LAYER
                                      G_TYPE_DOUBLE, // COLUMN_rotation
				      G_TYPE_DOUBLE, // COLUMN_length
				      G_TYPE_DOUBLE, // COLUMN_width
				      G_TYPE_INT, // COLUMN_shape
                                      G_TYPE_STRING, // COLUMN_COMMENT
                                      G_TYPE_BOOLEAN); // COLUMN_NO_FILES_FOUND
                                                                       
    interface.tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(interface.model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(interface.tree), TRUE);						
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW(interface.tree), TRUE);
// 	g_object_unref (G_OBJECT(interface.model));
    interface.selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree));
    gtk_tree_selection_set_mode (GTK_TREE_SELECTION(interface.selection),
				     GTK_SELECTION_MULTIPLE);
				 
/*	gtk_drag_source_set (interface.tree, 
			 GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			 dnd_table, n_dnds, 
			 GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect (G_OBJECT (interface.tree), 
		      "drag_data_get",
		      G_CALLBACK (drag_file_cb), 
		      NULL);
		      
    g_signal_connect (G_OBJECT (interface.tree),
		      "drag_begin",
		      G_CALLBACK (drag_begin_file_cb),
		      NULL);	  
		  
	g_signal_connect (G_OBJECT(interface.tree), 
		      "event_after",
		      G_CALLBACK(file_event_after_cb), 
		      NULL);*/
		      
    g_signal_connect (G_OBJECT(interface.tree), 
		      "button_release_event",
		      G_CALLBACK(file_button_release_event_cb), 
		      NULL);
    g_signal_connect (G_OBJECT(interface.tree),
		      "key_press_event",
		      G_CALLBACK(file_key_press_event_cb),
		      NULL);
		  	      
    g_signal_connect (G_OBJECT(interface.tree), 
		      "button_press_event",
		      G_CALLBACK(file_button_press_event_cb), 
		      NULL);		   
		      
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), interface.tree);
    
    gtk_container_add (GTK_CONTAINER(window), GTK_WIDGET(interface.tree));
    gtk_box_pack_end (GTK_BOX (vbox), window, TRUE, TRUE, 0);
     
    /*gtk_tree_view_column_set_visible () */    

    /* create the Designator column */
    renderer = gtk_cell_renderer_text_new ();          
    interface.column = gtk_tree_view_column_new_with_attributes (_("Designator"), renderer,
						       "text",COLUMN_DESIGNATOR,
						       NULL);
    gtk_tree_view_column_set_sizing (interface.column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_resizable (interface.column, TRUE);
    gtk_tree_view_column_set_sort_column_id (interface.column, COLUMN_DESIGNATOR); 
    gtk_tree_view_append_column (GTK_TREE_VIEW(interface.tree), interface.column);
    
    /* create the Layer column */
    renderer = gtk_cell_renderer_text_new ();
  //  g_object_set( renderer, "xalign", 1.0, NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Layer"), renderer,
						       "text", COLUMN_LAYER,
						       NULL);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, COLUMN_LAYER);
    gtk_tree_view_append_column (GTK_TREE_VIEW(interface.tree), column);

    /* create the type column */ 
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Comment"), renderer,
						       "text", COLUMN_COMMENT,
						       NULL);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, COLUMN_COMMENT);
    gtk_tree_view_append_column (GTK_TREE_VIEW(interface.tree), column);

    return vbox;

} /* create_search_results_section */


GtkWidget *
create_main_search_window (void)
{
    //GtkTargetEntry  drag_types[] = { { "text/uri-list", 0, 0 } };
    GtkWidget 	        *hbox;	
    GtkWidget	        *vbox;
    GtkWidget	        *label;
    GtkWidget 	        *window;
    GtkTooltips         *tooltips = gtk_tooltips_new();
    int                 idx;
    char                s_MAX_FILES[MAXL] = "";
    GtkTreeIter         iter;
    GtkCellRenderer     *renderer;
     
    window = gtk_vbox_new (FALSE, 6);	
    gtk_container_set_border_width (GTK_CONTAINER(window), 12);

    hbox = gtk_hbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX(window), hbox, FALSE, FALSE, 0);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
/*
    interface.drawing_area = gtk_drawing_area_new ();
    gtk_box_pack_start (GTK_BOX (vbox), interface.drawing_area, TRUE, FALSE, 0);

    gtk_drawing_area_size (GTK_DRAWING_AREA (interface.drawing_area),
	                   DEFAULT_ANIMATION_WIDTH,
			   DEFAULT_ANIMATION_HEIGHT);
		      
    gtk_drag_source_set (interface.drawing_area, 
			 GDK_BUTTON1_MASK,
			 drag_types, 
			 G_N_ELEMENTS (drag_types), 
			 GDK_ACTION_COPY);
			 
    gtk_drag_source_set_icon_stock (interface.drawing_area,
	                            GTK_STOCK_FIND);
		      
    g_signal_connect (G_OBJECT (interface.drawing_area), 
		      "drag_data_get",
		      G_CALLBACK (drag_data_animation_cb), 
		      interface.main_window);

    g_signal_connect (interface.drawing_area, "expose-event",
		      G_CALLBACK (animation_expose_event_cb), NULL);
*/	

    interface.table = gtk_table_new (3, 1, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(interface.table), 6);
    gtk_table_set_col_spacings (GTK_TABLE(interface.table), 12);
    gtk_container_add (GTK_CONTAINER(hbox), interface.table);

  /*completion_model*/

   completion_model = gtk_list_store_new (2, 
				      G_TYPE_STRING,
				      G_TYPE_STRING);

    interface.file_is_named_entry = gtk_entry_new (); 
    entry_completion = gtk_entry_completion_new ();  
    gtk_entry_set_completion( GTK_ENTRY(interface.file_is_named_entry), entry_completion);
    gtk_entry_completion_set_model (entry_completion, GTK_TREE_MODEL(completion_model));
   
    /* gtk_entry_completion_set_text_column (entry_completion, 0);   
     * Default minimum length of entry before completion func gets activated is 1
     * printf("key length %i\n", gtk_entry_completion_get_minimum_key_length(entry_completion));
     */
     
    label = gtk_label_new_with_mnemonic (_("_Search for Parts:")); 
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    g_object_set (G_OBJECT(label), "xalign", 0.0, NULL);
    gtk_label_set_mnemonic_widget (GTK_LABEL(label), interface.file_is_named_entry);
    gtk_table_attach (GTK_TABLE(interface.table), interface.file_is_named_entry, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    
    interface.find_button = gtk_button_new_from_stock (GTK_STOCK_FIND);
    gtk_button_set_label(GTK_BUTTON(interface.find_button), _("Mark"));
    gtk_table_attach (GTK_TABLE(interface.table), interface.find_button, 1, 2, 0, 1, GTK_FILL | GTK_SHRINK, 0, 0, 0);
    g_signal_connect (G_OBJECT(interface.find_button),"clicked",
		      G_CALLBACK(click_find_cb), NULL);
        
    gtk_widget_set_sensitive (interface.find_button, TRUE);
    gtk_button_set_focus_on_click(GTK_BUTTON(interface.find_button), FALSE);

    hbox = gtk_hbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX(window), hbox, FALSE, FALSE, 0);

    interface.check_designator = gtk_check_button_new_with_mnemonic (_("Designator"));
    gtk_box_pack_start (GTK_BOX(hbox), interface.check_designator, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (interface.check_designator),"clicked",
		      G_CALLBACK (click_check_button_cb), (void*)COLUMN_DESIGNATOR);
                          
    interface.invert_selection = gtk_button_new_from_stock (GTK_STOCK_FIND);
    gtk_button_set_label(GTK_BUTTON(interface.invert_selection), _("Invert Selection"));
    gtk_table_attach (GTK_TABLE(interface.table),interface.invert_selection, 0, 1, 1, 2, GTK_SHRINK, 0, 0, 0);
    g_signal_connect (G_OBJECT(interface.invert_selection),"clicked",
		      G_CALLBACK(click_invert_selection_cb), NULL);
                      
    interface.top_parts_selection = gtk_button_new_from_stock (GTK_STOCK_FIND);
    gtk_button_set_label(GTK_BUTTON(interface.top_parts_selection), _("Top Parts"));
    gtk_table_attach (GTK_TABLE(interface.table),interface.top_parts_selection, 1, 2, 1, 2,  GTK_SHRINK, 0, 0, 0);
    g_signal_connect (G_OBJECT(interface.top_parts_selection),"clicked",
		      G_CALLBACK(click_top_parts_selection_cb), NULL); 
                      
    interface.bottom_parts_selection = gtk_button_new_from_stock (GTK_STOCK_FIND);
    gtk_button_set_label(GTK_BUTTON(interface.bottom_parts_selection), _("Bottom Parts"));
    gtk_table_attach (GTK_TABLE(interface.table),interface.bottom_parts_selection, 2, 3, 1, 2,  GTK_SHRINK, 0, 0, 0);
    g_signal_connect (G_OBJECT(interface.bottom_parts_selection),"clicked",
		      G_CALLBACK(click_bottom_parts_selection_cb), NULL);                                                                                                                                           
     
    combo_box_model = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING); 
    interface.layer_active = gtk_combo_box_new_with_model(GTK_TREE_MODEL(combo_box_model));
    
    for (idx =  0; idx < MAX_FILES; idx++) {

        if (screen.file[idx] == NULL) {
            gtk_list_store_append(combo_box_model, &iter);
            gtk_list_store_set (combo_box_model, &iter, 0, idx, -1);
        } 
    }

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(GTK_COMBO_BOX(interface.layer_active)), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT(GTK_COMBO_BOX(interface.layer_active)), renderer, "text", 0);  
      

    
    gtk_table_attach (GTK_TABLE(interface.table),interface.layer_active, 2, 3, 0, 1, GTK_SHRINK, 0, 0, 0);
    g_signal_connect (G_OBJECT((GTK_COMBO_BOX(interface.layer_active))),"changed",
		      G_CALLBACK(click_layer_active_cb), NULL);  

    sprintf(s_MAX_FILES,"%i",MAX_FILES-2);
    gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(combo_box_model), &iter, s_MAX_FILES);
    gtk_combo_box_set_active_iter   (GTK_COMBO_BOX(interface.layer_active), &iter);
    /*click_layer_active_cb(GTK_WIDGET(interface.layer_active), NULL);*/
     
                                                                                            
            
    interface.check_comment = gtk_check_button_new_with_mnemonic (_("Comment"));
    gtk_box_pack_end (GTK_BOX(hbox), interface.check_comment, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (interface.check_comment),"clicked",
		      G_CALLBACK (click_check_button_cb), (void*) COLUMN_COMMENT);
    g_signal_connect (G_OBJECT (interface.file_is_named_entry), "key_press_event",
		      G_CALLBACK (file_is_named_entry_key_press_cb),
		      NULL);
    g_signal_connect (G_OBJECT (interface.file_is_named_entry), "activate",
		      G_CALLBACK (file_is_named_activate_cb),
		      NULL);
                
    gtk_tooltips_set_tip(tooltips, interface.invert_selection, "Invert current selection", NULL);
    gtk_tooltips_set_tip(tooltips, interface.check_designator, "Search for Designator <Default>", NULL);
    gtk_tooltips_set_tip(tooltips, interface.top_parts_selection, "Select parts on the top side", NULL);
    gtk_tooltips_set_tip(tooltips, interface.bottom_parts_selection, "Select parts on the bottom side", NULL);
    gtk_tooltips_set_tip(tooltips, interface.check_comment, "Search in Comments", NULL);
#ifdef HAVE_REGEX_H
    gtk_tooltips_set_tip(tooltips, interface.file_is_named_entry, "Enter a part name to search for;\nUse .* (regex) as a wildcard", NULL);
#else
    gtk_tooltips_set_tip(tooltips, interface.file_is_named_entry, "Enter a part name to search for, press Enter to mark", NULL);
#endif
    gtk_tooltips_set_tip(tooltips, interface.find_button, "Mark search results on screen", NULL); 
    gtk_tooltips_set_tip(tooltips, interface.layer_active, "Choose layer for selection", NULL);    
    
    vbox = gtk_vbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX (window), vbox, TRUE, TRUE, 0);
    
    interface.results = create_search_results_section ();
           
    gtk_box_pack_start (GTK_BOX (vbox), interface.results, TRUE, TRUE, 0);
    
    gtk_widget_set_sensitive (interface.results, TRUE);
    gtk_widget_set_sensitive (interface.find_button, TRUE);
    return window;
} /* create_main_search_window */

void
create_search_window(GtkWidget *widget, gpointer data)
{
    GtkWidget    *window;
   
    if (interface.main_window != NULL) {
        gtk_widget_destroy(interface.main_window);
        free_pnp_state(parsed_PNP_data);
    }  
    memset (&interface, 0, sizeof (interface));
    interface.geometry.min_height = MINIMUM_WINDOW_HEIGHT;
    interface.geometry.min_width  = MINIMUM_WINDOW_WIDTH;
   
    interface.main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    window = create_main_search_window ();
    gtk_container_add(GTK_CONTAINER(interface.main_window), window);
 
    gtk_window_set_focus (GTK_WINDOW(interface.main_window), 
	    interface.file_is_named_entry);
    gtk_window_set_title(GTK_WINDOW(interface.main_window),
            _("Select parts"));

    GTK_WIDGET_SET_FLAGS (interface.find_button, GTK_CAN_DEFAULT);
    gtk_window_set_default (GTK_WINDOW(interface.main_window), interface.find_button); 

    gtk_widget_show_all (window);

    gtk_widget_show (interface.main_window);
    
    g_signal_connect (G_OBJECT (interface.main_window),"delete-event",
		      G_CALLBACK (gtk_widget_hide_on_delete), interface.main_window);

    return;
} /* create_search_window */



void
click_check_button_cb (GtkWidget	*widget, 
	       	       gpointer 	data)
{   gboolean     designator_active, comment_active; 
    
    designator_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_designator));
    comment_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_comment));
    
    if (designator_active) {
        entry_completion = gtk_entry_completion_new ();  
        gtk_entry_set_completion( GTK_ENTRY(interface.file_is_named_entry), entry_completion);
        gtk_entry_completion_set_model (entry_completion, GTK_TREE_MODEL(completion_model));
        gtk_entry_completion_set_text_column (entry_completion, 0); 
    } else  {
        entry_completion = gtk_entry_completion_new ();  
        gtk_entry_set_completion( GTK_ENTRY(interface.file_is_named_entry), entry_completion);
        gtk_entry_completion_set_model (entry_completion, GTK_TREE_MODEL(completion_model));
        gtk_entry_completion_set_text_column (entry_completion, 1); 
    }
} /* click_check_button_cb */

void
click_invert_selection_cb(GtkWidget	*widget,
                              gpointer *data)
{

    GtkTreeIter iter;
    GtkTreeSelection *tree_sel;

    gtk_widget_set_sensitive (interface.invert_selection, FALSE);
    if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
      GERB_MESSAGE("empty interface.model\n");
    }
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
    do {
    /* We loop through the current selection inverting 
     * all selections 
     */
       if (gtk_tree_selection_iter_is_selected(tree_sel,&iter)) {
          gtk_tree_selection_unselect_iter(tree_sel, &iter);
       } else 
       if (!gtk_tree_selection_iter_is_selected(tree_sel,&iter)) {
               gtk_tree_selection_select_iter(tree_sel, &iter);
       }
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter));
   
    //g_free(tree_sel);    
    gtk_widget_set_sensitive (interface.invert_selection, TRUE);

} /* click_invert_selection_cb */


void
click_bottom_parts_selection_cb(GtkWidget	*widget,
                              gpointer *data)
{

    GtkTreeIter      iter;
    GtkTreeSelection *tree_sel;
    char             *layer;
    int              none_selected = 0;

    gtk_widget_set_sensitive (interface.bottom_parts_selection, FALSE);
    if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
      GERB_MESSAGE("empty interface.model\n");
    }
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
    if (gtk_tree_selection_count_selected_rows(tree_sel) > 0) 
        none_selected = 0;
    else {
        if (gtk_tree_selection_count_selected_rows(tree_sel) == 0)
        none_selected = 1;
        }
    do {
    /* We loop through the current selection checking if layer is T or B 
     * 
     */

    gtk_tree_model_get(GTK_TREE_MODEL(interface.model), &iter,  COLUMN_LAYER, &layer, -1);
    if (none_selected) {
        if (strchr(layer,'T'))  
            gtk_tree_selection_unselect_iter(tree_sel, &iter);
        else if  (strchr(layer,'B'))         
            gtk_tree_selection_select_iter(tree_sel, &iter);
        else
            GERB_MESSAGE("No layer information in this line");
    } else {
             if (gtk_tree_selection_iter_is_selected(tree_sel,&iter) && (strchr(layer,'T')))
                 gtk_tree_selection_unselect_iter(tree_sel, &iter); 
                 /*takes out all of current selection which are on top side*/
    }           
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter));
   
    //g_free(tree_sel);    
    gtk_widget_set_sensitive (interface.bottom_parts_selection, TRUE);

} /* click_bottom_parts_selection_cb */



void
click_top_parts_selection_cb(GtkWidget	*widget,
                              gpointer *data)
{

    GtkTreeIter      iter;
    GtkTreeSelection *tree_sel;
    char             *layer;
    int              none_selected = 0;
    
    gtk_widget_set_sensitive (interface.top_parts_selection, FALSE);
 
    if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
      GERB_MESSAGE("empty interface.model\n");
    }
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
     if (gtk_tree_selection_count_selected_rows(tree_sel) > 0) 
        none_selected = 0;
    else {
        if (gtk_tree_selection_count_selected_rows(tree_sel) == 0)
        none_selected = 1;
        }
    do {
    /* We loop through the current selection checking if layer is T or B 
     * 
     */

    gtk_tree_model_get(GTK_TREE_MODEL(interface.model), &iter,  COLUMN_LAYER, &layer, -1);

    if (none_selected) {
        if (strchr(layer,'B'))  
            gtk_tree_selection_unselect_iter(tree_sel, &iter);
        else if  (strchr(layer,'T'))         
            gtk_tree_selection_select_iter(tree_sel, &iter);
        else
            GERB_MESSAGE("No layer information in this line");
    } else {
             if (gtk_tree_selection_iter_is_selected(tree_sel,&iter) && (strchr(layer,'B')))
                 gtk_tree_selection_unselect_iter(tree_sel, &iter); 
                 /*takes out all of current selection which are on bottom side*/
    }           
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter));   
    //g_free(tree_sel);    
    gtk_widget_set_sensitive (interface.top_parts_selection, TRUE);

} /* click_top_parts_selection_cb */

void
click_layer_active_cb(GtkWidget	*widget,
                              gpointer *data)
{

    int           idx;
    GtkTreeIter   iter;
   
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(combo_box_model), &iter)) {
        gtk_widget_set_sensitive (interface.layer_active, FALSE);
        gtk_combo_box_get_active_iter(GTK_COMBO_BOX(interface.layer_active), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL(combo_box_model), &iter, 0, &idx, -1);
        interface.layer_select_active = idx;
        gtk_widget_set_sensitive (interface.layer_active, TRUE);
    }
    

} /* click_layer_active_cb */
   

void
click_find_cb (GtkWidget	*widget, 
	       gpointer 	data)
{

    gtk_widget_set_sensitive (interface.find_button, FALSE);

    create_marked_layer(interface.layer_select_active);
    
        /* Redraw the image(s) */
  
    redraw_pixmap(screen.drawing_area, TRUE);
	    
    gtk_widget_set_sensitive (interface.find_button, TRUE);
} /* click_find_cb */



void
file_is_named_activate_cb (GtkWidget 	*widget, 
			   gpointer 	data)
{
      if ((GTK_WIDGET_VISIBLE (interface.find_button)) && 
	(GTK_WIDGET_SENSITIVE (interface.find_button))) {
	    click_find_cb (interface.find_button, NULL);
      }
} /* file_is_named_activate_cb */


gboolean
file_is_named_entry_key_press_cb (GtkWidget    	*widget, 
		     	     GdkEventKey	*event, 
		     	     gpointer 		data)   
{ 

    g_return_val_if_fail (GTK_IS_WIDGET(widget), FALSE);
    if (event->keyval == GDK_Escape)    {
        GERB_MESSAGE("ESC unselected all\n");
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
    }  else if  (event->keyval == GDK_Return) {  
        if ((GTK_WIDGET_VISIBLE (interface.find_button)) && 
	    (GTK_WIDGET_SENSITIVE (interface.find_button))) {
            select_by_regex(interface.find_button, NULL);
           
	}
    } else if ((event->keyval) && (event->keyval != GDK_Shift_L) 
              && (event->keyval != GDK_Shift_R) && (event->keyval != GDK_Return)) {
        GtkTreeIter          iter;
        GtkTreePath         *path;
        int                  scroll_line_number = 1, not_found = 1; 
        char                *tmp_entry;
        char                 entered_key[MAXL], path_string[MAXL];
        gboolean             designator_active = FALSE, comment_active = FALSE;
         
        if  (event->keyval == GDK_BackSpace) {
           /*CHECK ME also scroll on hitting backspace?*/
            if (strlen(composed_entry)>1) {
                composed_entry[strlen(composed_entry)-1] = '\0';/*we shorten our composed string by one*/
            } else if (strlen(composed_entry) == 1) {
                strncpy(composed_entry, "", 1);
            }
        } else if (event->keyval != GDK_Return) {
            designator_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_designator));
            comment_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_comment));
            sprintf(entered_key, "%c", event->keyval);
            strncat(composed_entry, entered_key, 1);

            if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
              GERB_MESSAGE("empty interface.model\n");
              return TRUE;
            }
            do {
               /* Scroll Engine:
                * We loop through the treemodel, as soon we found the first which is matching the key 
                * pressed we exit and scroll there
                */
                scroll_line_number++;
                if (((!designator_active) && (!comment_active)) || ((designator_active) && (!comment_active))) {
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(interface.check_designator), TRUE);  
                    gtk_tree_model_get (GTK_TREE_MODEL(interface.model), &iter, COLUMN_DESIGNATOR, &tmp_entry , -1); 
                    if (strncasecmp(composed_entry, tmp_entry, strlen(composed_entry)) == 0) {
                        sprintf(path_string, "%i", scroll_line_number);
                        path = gtk_tree_path_new_from_string (path_string);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(interface.tree), path, COLUMN_DESIGNATOR, TRUE, 0.3, 0.5); 
                        not_found = 0;
                    }
                } 
#if 0
          /* using strncasecomp with the utf8 set does not seem to work so scrolling to 
           * comment entries is deactivated for the moment */
                else if ((!designator_active) && (comment_active)) {
                //  printf("comment active");
                    gtk_tree_model_get (GTK_TREE_MODEL(interface.model), &iter, COLUMN_COMMENT, &tmp_entry , -1); 
                    sprintf(entered_key, "%c", event->keyval);

                    if (strncasecmp(composed_entry, tmp_entry, strlen(composed_entry)) == 0) {
      //                printf("This:%c compared to:%s and this %s\n", event->keyval, tmp_entry, entered_key);
  
                        sprintf(path_string, "%i", scroll_line_number);
                     // printf("%d linnumber:%d new path:%s\n", scroll_line_number, tmp_line_number, path_string);
                        path = gtk_tree_path_new_from_string (path_string);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(interface.tree), path, COLUMN_COMMENT, TRUE, 0.3, 0.5); 
                        not_found = 0;
                    }
                }
#endif              
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter) && not_found);	          
        }  
  }
   return FALSE;
      
} /* file_is_named_entry_key_press_cb */


gboolean row_selected_by_button_press_event = FALSE;


gboolean
file_button_press_event_cb (GtkWidget 		*widget, 
			    GdkEventButton 	*event, 
			    gpointer 		data)
{
    GtkTreePath *path;
    row_selected_by_button_press_event = TRUE;

    if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW(interface.tree))) {
	    return FALSE;
    }
     

    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW(interface.tree), event->x, event->y,
    &path, NULL, NULL, NULL)) {

        if ((event->button == 1 || event->button == 2 || event->button == 3)
            && gtk_tree_selection_path_is_selected 
               (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path)) {

        row_selected_by_button_press_event = FALSE;
        }

        gtk_tree_path_free (path);
    } else {
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
    }
   
    return !(row_selected_by_button_press_event);
    
     
} /* file_button_press_event_cb */



gboolean
file_button_release_event_cb (GtkWidget 	*widget, 
		      	      GdkEventButton 	*event, 
		      	      gpointer 		data)
{


    if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW(interface.tree))) {
	    return FALSE;
    }
       
        if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION(interface.selection)) == 0) {
	    return FALSE;
        }
	    
    if (event->button == 1 || event->button == 2) {
	GtkTreePath *path;

	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW(interface.tree), event->x, event->y,
		&path, NULL, NULL, NULL)) {
		if ((event->state & GDK_SHIFT_MASK) || (event->state & GDK_CONTROL_MASK)) {
		    if (row_selected_by_button_press_event) {
                        gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
		    } else {
                       gtk_tree_selection_unselect_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
		    }
                } else {
	            gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
		}
	}	
	gtk_tree_path_free (path);
    }	
    if (event->button == 3) {	
      GtkTreePath *path;

  
        if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW(interface.tree), event->x, event->y,
		&path, NULL, NULL, NULL)) {

		if ((event->state & GDK_SHIFT_MASK) || (event->state & GDK_CONTROL_MASK)) {
		    if (row_selected_by_button_press_event) {
                        gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
                        click_find_cb(interface.find_button, NULL);
		    } else {
                       gtk_tree_selection_unselect_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
                       click_find_cb(interface.find_button, NULL);
		    }
                } else {
	            gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
                    click_find_cb(interface.find_button, NULL);
		}
    
        
        }
    }
    return FALSE;
} /* file_button_release_event_cb */

/*
gboolean
file_event_after_cb  (GtkWidget 	*widget, 
		      GdkEventButton 	*event, 
		      gpointer 		data)
{	
    GtkTreeIter 	 iter;

    if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW(interface.tree))) {
	    return FALSE;
    }
    
    if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION(interface.selection)) == 0) {
	    return FALSE;
    }
    return FALSE;
    
}*/

/*void  
drag_begin_file_cb  (GtkWidget          *widget,
		     GdkDragContext     *context,
		     GtkSelectionData   *selection_data,
		     guint               info,
		     guint               time,
		     gpointer            data)
{	
    if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION(interface.selection)) > 1) {
	    gtk_drag_set_icon_stock (context, GTK_STOCK_DND_MULTIPLE, 0, 0);
    }
    else {
	    gtk_drag_set_icon_stock (context, GTK_STOCK_DND, 0, 0);
    }
}
*/

   
gboolean
file_key_press_event_cb  (GtkWidget 		*widget, 
		    	  GdkEventKey	 	*event, 
		     	  gpointer 		data)
{		
    
    if (event->keyval == GDK_Escape) {
        GERB_MESSAGE("ESC unselected all\n");/*CHECK ME: do we need to check GTK_WIDGET_VISIBLE?*/
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
    
     } else  if (event->state & GDK_SHIFT_MASK) {
                                                   /*select multiple items by SHift-Up/Down*/
        if (event->keyval == GDK_Up)  {
    
        GtkTreeSelection  *tree_sel = NULL;    
        GList             *selection_list, *tmp_path;
                
        
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
        selection_list = gtk_tree_selection_get_selected_rows
                                            (GTK_TREE_SELECTION(tree_sel),
                                             (GtkTreeModel **)&interface.model);
        if (selection_list != NULL) {                                         
           tmp_path  = g_list_first(selection_list);
           gtk_tree_path_prev(tmp_path->data);   
           gtk_tree_selection_select_path  (tree_sel, tmp_path->data);
           gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(interface.tree),
                                                  tmp_path->data,
                                                  COLUMN_DESIGNATOR,
                                                  TRUE,
                                                  0.3,
                                                  0.5);
        }
   
        
        click_find_cb(interface.find_button, NULL);
    
     /*
      * g_free(tree_sel);CHECK ME:do we need to free them?
      */
        } else if (event->keyval == GDK_Down) {
            GtkTreeSelection  *tree_sel = NULL;
            GList             *selection_list, *tmp_path;
                
        
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
            selection_list = gtk_tree_selection_get_selected_rows
                                                (GTK_TREE_SELECTION(tree_sel),
                                                 (GtkTreeModel **)&interface.model);
            if (selection_list != NULL) {                                         
               tmp_path  = g_list_last(selection_list);
               gtk_tree_path_next(tmp_path->data);   
               gtk_tree_selection_select_path  (tree_sel, tmp_path->data);
               gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(interface.tree),
                                                      tmp_path->data,
                                                      COLUMN_DESIGNATOR,
                                                      TRUE,
                                                      0.3,
                                                      0.5);
                                                  
            }
            click_find_cb(interface.find_button, NULL);
    
         /*
          * g_free(tree_sel);CHECK ME:do we need to free them?
          */
        }
     } else  if ((event->keyval == GDK_space) || (event->keyval == GDK_downarrow) || (event->keyval == GDK_Down)) {
        /* We loop through the current selection, as soon we found the first, 
            * the following item will automatically  be selected and the rest unselected
            * and returned.
            */
        GtkTreeSelection  *tree_sel = NULL;
        GList             *selection_list, *tmp_path;
                
        
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
        selection_list = gtk_tree_selection_get_selected_rows
                                            (GTK_TREE_SELECTION(tree_sel),
                                             (GtkTreeModel **)&interface.model);
        if (selection_list != NULL) {                                         
           tmp_path  = g_list_first(selection_list);
           gtk_tree_selection_unselect_all  (tree_sel);
           gtk_tree_path_next(tmp_path->data);   
          // if (tmp_path->data != NULL) {
               gtk_tree_selection_select_path  (tree_sel, tmp_path->data);
               gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(interface.tree),
                                                      tmp_path->data,
                                                      COLUMN_DESIGNATOR,
                                                      TRUE,
                                                      0.3,
                                                      0.5);
          // }                                                      
                                                  
        }
        click_find_cb(interface.find_button, NULL);
    
     /*
      * g_free(tree_sel);CHECK ME:do we need to free them?
      */
    } else if (event->keyval == GDK_Return) {
       /* printf("enter key was pressed!\n");
        GtkTreeSelection  *tree_sel = NULL;   
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
        gtk_tree_selection_unselect_all (tree_sel);*/
        return TRUE;
    } else if ((event->keyval == GDK_Up) || (event->keyval == GDK_uparrow)) {
    
        GtkTreeSelection  *tree_sel = NULL; 
        GList             *selection_list, *tmp_path;
                

        
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
        selection_list = gtk_tree_selection_get_selected_rows
                                            (GTK_TREE_SELECTION(tree_sel),
                                             (GtkTreeModel **)&interface.model);
        if (selection_list != NULL) {                                         
           tmp_path  = g_list_first(selection_list);
           gtk_tree_selection_unselect_all  (tree_sel);
           gtk_tree_path_prev(tmp_path->data);   
           gtk_tree_selection_select_path  (tree_sel, tmp_path->data);
           gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(interface.tree),
                                                  tmp_path->data,
                                                  COLUMN_DESIGNATOR,
                                                  TRUE,
                                                  0.3,
                                                  0.5);
        }
   
        
        click_find_cb(interface.find_button, NULL);
    
        }
    
     /*
      * g_free(tree_sel);CHECK ME:do we need to free them?
      */      
       gtk_window_activate_focus(GTK_WINDOW(interface.main_window)); 
    return TRUE;
} /* file_key_press_event_cb */
#endif 
