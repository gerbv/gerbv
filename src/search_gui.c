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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

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

#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"
#include "draw.h"
#include "color.h"
#include "gerbv_screen.h"
#include "gerbv_icon.h"
#include "search.h"
#include "search_file.h"
#include "search_gui.h"
#include "search_mark.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "search_gui.h"
/* CHECKME - here gi18n is disabled */
#define _(String) (String)

//pnp_state_t *parsed_PNP_data;


void select_by_regex(GtkWidget    	*widget,  gpointer 		data) 
{
    GtkTreeIter 	 iter;
    gboolean no_files_found = FALSE;
    gchar *designator;
    gchar *comment;
    #ifdef HAVE_REGEX_H
    regex_t regexec_pattern; 
    #endif
    const gchar *entry; 
    gchar regex[MAXL]="";
    gboolean match, nomatch;
    gboolean designator_active, comment_active;
    
    
    entry = gtk_entry_get_text(GTK_ENTRY(interface.file_is_named_entry));

    designator_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_designator));
    comment_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(interface.check_comment));
    
    if ((!designator_active) && (!comment_active)) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(interface.check_designator), TRUE);  
        designator_active = TRUE;
    }    
    if(sscanf(entry, "%100s", regex)==1 && regex[0]) {
        // TODO transform * into .*, ? into .
    #ifdef HAVE_REGEX_H
          regcomp (&regexec_pattern, regex , REG_NOSUB);  //compiles regexp string
    #endif

        nomatch = FALSE;
    } else {
        nomatch = TRUE;
    }
    if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
        printf("empty interface.model\n");
        return;
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
            if(designator_active)
                match = (regexec (&regexec_pattern, designator, 0, 0, 0) != REG_NOMATCH);
            if(comment_active)
                match = match || (regexec (&regexec_pattern, comment, 0, 0, 0) != REG_NOMATCH);
           #else 
            if(designator_active)
                match = (strstr (designator, regex) != NULL);
            if(comment_active)
                match = match || (strstr (comment, regex) != NULL);    
           #endif     
        }
       // printf("%d %s %s: f\n", match, designator, comment);
      //  printf("%d %s %s: f\n", match, designator,  g_locale_from_utf8(comment, -1, NULL, NULL, NULL));
        if(match) {
            gtk_tree_selection_select_iter (GTK_TREE_SELECTION(interface.selection), &iter);
        } else {
            gtk_tree_selection_unselect_iter (GTK_TREE_SELECTION(interface.selection), &iter);
        }
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter));	
    g_free (designator);
    g_free (comment);


    return;
} /* select_by_regex */


GtkWidget *
create_search_results_section (void)
{
    GtkWidget 	  *label;
    GtkWidget 	  *vbox;
    GtkWidget	  *hbox;
    GtkWidget	  *window;	
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
                                    
    /*catch data loss on reopening select parts window HERE*/                                    
    //if (parsed_PNP_data->designator == NULL)
       interface.model = gtk_list_store_new (NUM_COLUMNS, 
					  G_TYPE_STRING, 
					  G_TYPE_STRING,
                                          G_TYPE_DOUBLE, 
					  G_TYPE_DOUBLE,
                                          G_TYPE_DOUBLE,
                                          G_TYPE_DOUBLE,
                                          G_TYPE_DOUBLE,
                                          G_TYPE_DOUBLE,
                                          G_TYPE_STRING,
					  G_TYPE_DOUBLE,
					  G_TYPE_STRING);
   // else
    //    printf("looks like we are using Search menu\n");                                 

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
    /*colour it green, actually only one Layer should be coloured CHECK ME*/
/*    g_object_set(renderer,
             "cell-background", "Green","cell-background-set", TRUE, NULL);
             printf("Green");*/
           
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
    GtkCellRenderer     *renderer;
    GtkWidget 	        *window;
    GtkTooltips         *tooltips = gtk_tooltips_new();
    GList               *layer_list = NULL;
//    int                 idx;
//    gchar               string[2];

    window = gtk_vbox_new (FALSE, 6);	
    gtk_container_set_border_width (GTK_CONTAINER(window), 12);

    hbox = gtk_hbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX(window), hbox, FALSE, FALSE, 0);
/*
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
*/
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
    interface.table = gtk_table_new (2, 1, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(interface.table), 6);
    gtk_table_set_col_spacings (GTK_TABLE(interface.table), 12);
    gtk_container_add (GTK_CONTAINER(hbox), interface.table);
    
    
    interface.file_is_named_entry = gtk_entry_new ();
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
    gtk_table_attach (GTK_TABLE(interface.table),interface.invert_selection, 0, 1, 1, 2, GTK_FILL | GTK_SHRINK, 0, 0, 0);
    g_signal_connect (G_OBJECT(interface.invert_selection),"clicked",
		      G_CALLBACK(click_invert_selection_cb), NULL);                  
  
    interface.layer_active = gtk_combo_new();
 
   // for (idx =  1; idx < MAX_FILES; idx++) 
    //{
            
      //      sprintf(string, "%d", idx);
            /* printf("Layer %s is available", string);
             * printf("Layer number: %d is available\n", idx);
             * How to use Gtk_ComboBox
             * gtk_combo_box_append_text((GTK_COMBO_BOX(interface.layer_active)), string);*/
           
    //}
    
    layer_list = g_list_append (layer_list, "16");
    layer_list = g_list_append (layer_list, "17");
    layer_list = g_list_append (layer_list, "18");
    layer_list = g_list_append (layer_list, "19");
    
    gtk_combo_set_popdown_strings(GTK_COMBO(interface.layer_active), layer_list);
    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (interface.layer_active)->entry), "16");
   // g_free(layer_list);  
    gtk_table_attach (GTK_TABLE(interface.table),interface.layer_active, 1, 2, 1, 2, GTK_FILL | GTK_SHRINK, 0, 0, 0);
    g_signal_connect (G_OBJECT((GTK_COMBO(interface.layer_active)->entry)),"activate",
		      G_CALLBACK(click_layer_active_cb), NULL);  
    interface.layer_select_active = 16;               
                      
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
    gtk_tooltips_set_tip(tooltips, interface.check_comment, "Search in Comments", NULL);
    gtk_tooltips_set_tip(tooltips, interface.file_is_named_entry, "Regexp if regex.h was found", NULL);
    gtk_tooltips_set_tip(tooltips, interface.find_button, "Mark search results on screen", NULL); 
    gtk_tooltips_set_tip(tooltips, interface.layer_active, "Choose layer for selection", NULL); 
    
 
	
/*
    entry = interface.file_is_named_entry;		  
    locale_string = "R";
    utf8_string = g_filename_to_utf8 (locale_string, -1, NULL, NULL, NULL);
    
    gtk_entry_set_text (GTK_ENTRY(entry), utf8_string);
    g_free (locale_string);
    g_free (utf8_string);
*/			      
    
    vbox = gtk_vbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX (window), vbox, TRUE, TRUE, 0);
    
    interface.results = create_search_results_section ();
    // do it only during search : gtk_widget_set_sensitive (GTK_WIDGET (interface.results), FALSE);
    
    
        /**cell backgrounds*/
 /*    if (COLUMN_LAYER == "8") 
  //     {
       g_object_set(renderer,
             "cell-background", "Orange",
             "cell-background-set", TRUE,
             NULL);
       printf("Orange");      
       }
    else if (COLUMN_LAYER == "B") 
       {
       */
              
    

    gtk_box_pack_start (GTK_BOX (vbox), interface.results, TRUE, TRUE, 0);
    
    gtk_widget_set_sensitive (interface.results, TRUE);
    gtk_widget_set_sensitive (interface.find_button, TRUE);
    
    

    return window;
} /* create_main_search_window */

void
create_search_window(GtkWidget *widget, gpointer data)
{
    GtkWidget    *window;
    
    memset (&interface, 0, sizeof (interface));
    interface.geometry.min_height = MINIMUM_WINDOW_HEIGHT;
    interface.geometry.min_width  = MINIMUM_WINDOW_WIDTH;

    interface.main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    window = create_main_search_window ();
    gtk_container_add(GTK_CONTAINER(interface.main_window), window);
//	gnome_app_set_contents (GNOME_APP(interface.main_window), window);

    gtk_window_set_geometry_hints (GTK_WINDOW(interface.main_window), GTK_WIDGET(interface.main_window),
				   &interface.geometry, GDK_HINT_MIN_SIZE);

    gtk_window_set_position (GTK_WINDOW(interface.main_window), GTK_WIN_POS_CENTER);

    gtk_window_set_focus (GTK_WINDOW(interface.main_window), 
	    interface.file_is_named_entry);
    gtk_window_set_title(GTK_WINDOW(interface.main_window),
            _("Select parts"));

    GTK_WIDGET_SET_FLAGS (interface.find_button, GTK_CAN_DEFAULT);
//	GTK_WIDGET_SET_FLAGS (interface.stop_button, GTK_CAN_DEFAULT); 
    gtk_window_set_default (GTK_WINDOW(interface.main_window), interface.find_button); 


    gtk_widget_show_all (window);
    
    gtk_widget_show (interface.main_window);


    return;
} /* create_search_window */



void
click_check_button_cb (GtkWidget	*widget, 
	       	       gpointer 	data)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
    printf("click_check_button_cb ACTIVE %d\n", (int) data);
    } 
    else {
    printf("click_check_button_cb INACTIVE %d\n", (int) data);
    }
} /* click_check_button_cb */

void
click_invert_selection_cb(GtkWidget	*widget,
                              gpointer *data)
{

    GtkTreeIter iter;
    GtkTreeSelection *tree_sel;

    gtk_widget_set_sensitive (interface.invert_selection, FALSE);
    printf("inversion selected\n");
    if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
      printf("empty interface.model\n");
    }
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
    do {
    /* We loop through the current selection inverting 
     * all selections 
     */
       if (gtk_tree_selection_iter_is_selected(tree_sel,&iter)) {
          gtk_tree_selection_unselect_iter(tree_sel, &iter);
         // printf("Invertion done!\n");
       } else 
       if (!gtk_tree_selection_iter_is_selected(tree_sel,&iter)) {
               gtk_tree_selection_select_iter(tree_sel, &iter);
       }
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter));
   
    //g_free(tree_sel);    
    gtk_widget_set_sensitive (interface.invert_selection, TRUE);

} /* click_invert_selection_cb */

void
click_layer_active_cb(GtkWidget	*widget,
                              gpointer *data)
{
    gchar    *string = NULL;
    GtkEntry *entry;
    int      idx;
    
    gtk_widget_set_sensitive (interface.layer_active, FALSE);
    string = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (interface.layer_active)->entry));
    sscanf(string,"%d",&idx);
    interface.layer_select_active = idx;
   /* printf("Layer:%s selected",string); 
    printf("Layer number:%d ",idx);*/
      
    gtk_widget_set_sensitive (interface.layer_active, TRUE);

} /* click_layer_active_cb */
   

void
click_find_cb (GtkWidget	*widget, 
	       gpointer 	data)
{

    gboolean no_files_found = FALSE;
    gchar *utf8_name;
    gchar *utf8_path;


    gtk_widget_set_sensitive (interface.find_button, FALSE);
    create_marked_layer(interface.layer_select_active);
    
        /* Redraw the image(s) */
  
    redraw_pixmap(screen.drawing_area, TRUE);
        
 //   printf("click_find_cb run search and mark layer created\n");
  	
    if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION(interface.selection)) == 0) {
  	 gtk_widget_set_sensitive (interface.find_button, TRUE);
        return;
    }
	    
    gtk_widget_set_sensitive (interface.find_button, TRUE);
} /* click_find_cb */


void
size_allocate_cb (GtkWidget	*widget,
		  GtkAllocation *allocation,
	      	  gpointer 	data)
{
/*
	gtk_widget_set_size_request (interface.add_button,
			      	     allocation->width,
			             allocation->height);
                                    */
     printf("size_allocate_cb\n");
} /* size_allocate_cb */


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
        printf("ESC unselected all\n");/*CHECK ME: do we need to check GTK_WIDGET_VISIBLE?*/
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
    }  
    if  (event->keyval == GDK_Return) {  
         if ((GTK_WIDGET_VISIBLE (interface.find_button)) && 
	    (GTK_WIDGET_SENSITIVE (interface.find_button))) {
            select_by_regex(interface.find_button, NULL);
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
        // printf("path \n%s",gtk_tree_path_to_string(path));
        printf("file_button_press_event_cb selected\n");
        gtk_tree_path_free (path);
    } else {
        printf("file_button_press_event_cb unselected all\n");
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
                       printf("file_button_release_event_cb gtk_tree_selection_unselect_path\n");
                       gtk_tree_selection_unselect_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
		    }
                } else {
		    printf("file_button_release_event_cb gtk_tree_selection_unselect_all\n");
	            gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)), path);
		}
	}	
	gtk_tree_path_free (path);
    }	
   /* if (event->button == 3) {	
    
//    GtkWidget *popup;
    GList *list;
    
    list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION(interface.selection),
						 (GtkTreeModel **)&interface.model);
    
    gtk_tree_model_get_iter (GTK_TREE_MODEL(interface.model), &iter, 
			     g_list_first (list)->data);
			     
    gtk_tree_model_get (GTK_TREE_MODEL(interface.model), &iter,
			    COLUMN_NO_FILES_FOUND, &no_files_found,
			   -1);		    
			  
    g_list_free (list);
    }
    */	
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
/*
gboolean
key_press_cb (GtkWidget    	*widget, 
	      GdkEventKey	*event, 
	      gpointer 		data)
{
    g_return_val_if_fail (GTK_IS_WIDGET(widget), FALSE);

    if (event->keyval == GDK_Escape) {
    // remove all selection 
       }
    else if (event->keyval == GDK_F10) {
	    if (event->state & GDK_SHIFT_MASK) {
		    gboolean no_files_found = FALSE;
		    GtkWidget *popup;
		    GtkTreeIter iter;
		    GList *list;
		    
		    if (gtk_tree_selection_count_selected_rows (GTK_TREE_SELECTION(interface.selection)) == 0) {
			    return FALSE;
		    }
	    
		    list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION(interface.selection),
						 (GtkTreeModel **)&interface.model);
		    
		    gtk_tree_model_get_iter (GTK_TREE_MODEL(interface.model), &iter, 
					     g_list_first(list)->data);
	    
		    gtk_tree_model_get (GTK_TREE_MODEL(interface.model), &iter,
					COLUMN_NO_FILES_FOUND, &no_files_found,
				    	-1);
				
		    if (!no_files_found) {
		    }

	    }
    }
    return FALSE;
}
*/

   
gboolean
file_key_press_event_cb  (GtkWidget 		*widget, 
		    	  GdkEventKey	 	*event, 
		     	  gpointer 		data)
{		
    
    if (event->keyval == GDK_Escape) {
        printf("ESC unselected all\n");/*CHECK ME: do we need to check GTK_WIDGET_VISIBLE?*/
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW(interface.tree)));
    }
    if (event->keyval == GDK_space || event->keyval == GDK_Return) {
        if (event->keyval == GDK_space) {
            GtkTreeSelection  *tree_sel = NULL;    
            GtkTreeIter iter, tmp_iter;
            int               tmp_scroll_line_number, scroll_line_number = 1; 
                    
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree)); 
    
        if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &iter)) {
          printf("empty interface.model\n");
          return TRUE;
        }
        do {
           /* We loop through the current selection, as soon we found the first, 
            * the following item will automatically  be selected and the rest unselected
            * and returned.
            */
            scroll_line_number++;    
        
            if (gtk_tree_selection_iter_is_selected(tree_sel,&iter)) {
            
                if (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter)) {
                    tmp_iter = iter;
                    tmp_scroll_line_number = scroll_line_number;
                    gtk_tree_selection_unselect_all (tree_sel);
                    gtk_tree_selection_select_iter(tree_sel, &tmp_iter);                           
                    //     printf("There was at least one item selected!\n");
                
                } else {
                    tmp_scroll_line_number = scroll_line_number;
                    break;/*dont fall over the edge:-)*/
                 }    
            }
          
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(interface.model), &iter));	
        
        gtk_tree_view_scroll_to_point (GTK_TREE_VIEW(interface.tree), -1,(int)(tmp_scroll_line_number*(6245/scroll_line_number)));
        
        click_find_cb(interface.find_button, NULL);
        
     /*
      * g_free(tree_sel);CHECK ME:do we need to free them?
      */
        }else 
        if (event->keyval == GDK_Return) {
        return TRUE;
        printf("enter key was pressed!\n");
        }
    }
    return TRUE;
} /* file_key_press_event_cb */
 
