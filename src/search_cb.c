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
 
#ifdef USE_GTK2
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#ifdef HAVE_LIBGEN_H
#include <libgen.h> /* dirname */
#endif


#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef USE_GTK2
#include <pango/pango.h>
#endif

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
#include "search_cb.h"
#include "log.h"
#include "setup.h"
#include "project.h"

//pnp_state_t *parsed_PNP_data;/*Global storage place for pick and place data*/


static void
cb_ok_load_pnp_file(GtkWidget *widget, GtkFileSelection *fs)
{
    char *filename;

     filename = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
    screen.file[16] = (gerbv_fileinfo_t *)malloc(sizeof(gerbv_fileinfo_t));/*allocate memory for new layer data*/
    memset((void *)screen.file[16], 0, sizeof(gerbv_fileinfo_t));
    screen.file[16]->name = (char *)malloc(strlen(filename) + 1);
    strcpy(screen.file[16]->name, filename);/*entry for pnp file also getting saved on "save project"*/
   // screen.file[16]->inverted = 0;
    
    create_search_window(widget, NULL);
    
    if (open_pnp(filename, screen.curr_index, FALSE) != -1) {

#ifdef HAVE_LIBGEN_H
        /*
         * Remember where we loaded file from last time
         */
        filename = dirname(filename);
#endif
        if (screen.path) 
	        free(screen.path);
           
        screen.path = (char *)malloc(strlen(filename) + 1);
        strcpy(screen.path, filename);
        screen.path = strncat(screen.path, "/", 1);
    }
    gtk_grab_remove(screen.win.load_file);
    gtk_widget_destroy(screen.win.load_file);
    screen.win.load_file = NULL;

    return;
} /* cb_ok_load_pnp_file */



static void
cb_cancel_load_file(GtkWidget *widget, gpointer data)
{
    gtk_grab_remove(screen.win.load_file);
    gtk_widget_destroy(screen.win.load_file);
    screen.win.load_file = NULL;

    return;
} /* cb_cancel_load_file */


void
load_pnp_file_popup(GtkWidget *widget, gpointer data)
{

    screen.win.load_file = gtk_file_selection_new("Select 'Pick and Place' File");
    
    if (screen.path)
	gtk_file_selection_set_filename
	    (GTK_FILE_SELECTION(screen.win.load_file), screen.path);

    gtk_signal_connect(GTK_OBJECT(screen.win.load_file), "destroy",
		       GTK_SIGNAL_FUNC(cb_cancel_load_file),
		       NULL);

    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.load_file)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_load_pnp_file), 
	 (gpointer)screen.win.load_file);
         
    gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.load_file)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_cancel_load_file), 
	 (gpointer)screen.win.load_file);
    
    gtk_widget_show(screen.win.load_file);

    gtk_grab_add(screen.win.load_file);
       
    return;
} /* load_pnp_file_popup */

int
open_pnp(char *filename, int idx, int reload)
{
    pnp_file_t  *fd;
    int          r, g, b;
    GtkStyle    *defstyle, *newstyle;
    pnp_state_t *temp_state;
   
    char *cptr;


    if (idx >= MAX_FILES) {
	GERB_MESSAGE("Couldn't open %s. Maximum number of files opened.\n",
		     filename);
	return -1;
    }
    //printf("About to open file:%s\n",filename);
    fd = pnp_fopen(filename);
    if (fd == NULL) {
	GERB_MESSAGE("Trying to open %s:%s\n", filename, strerror(errno));
	return -1;
    }
    temp_state = parse_pnp(fd);

    pnp_fclose(fd);
         
    if(!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(interface.model), &interface.iter)) {
        GERB_MESSAGE("You tried to load a non compatible PNP file\n change format to entry1,entry2,... !\n");
        gtk_widget_destroy(interface.main_window);
        free_pnp_state(parsed_PNP_data);
    }
    return 0;
    /*
     * Do error check before continuing
     */
    /*FIX ME include error checking for PNP files
     *FIX ME RELOAD 
     */
    cptr = strrchr(filename, '/');
    if (cptr) {
	int len;

	len = strlen(cptr);
	screen.file[idx]->basename = (char *)malloc(len + 1);
	if (screen.file[idx]->basename) {
	    strncpy(screen.file[idx]->basename, cptr+1, len);
	    screen.file[idx]->basename[len] = '\0';
	} else {
	    screen.file[idx]->basename = screen.file[idx]->name;
	}
    } else {
	screen.file[idx]->basename = screen.file[idx]->name;
    }

    /*
     * Calculate a "clever" random color based on index.
     */
    r = (12341 + 657371 * idx) % (int)(MAX_COLOR_RESOLUTION);
    g = (23473 + 434382 * idx) % (int)(MAX_COLOR_RESOLUTION);
    b = (90341 + 123393 * idx) % (int)(MAX_COLOR_RESOLUTION);

    screen.file[idx]->color = alloc_color(r, g, b, NULL);

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
			 filename, NULL); 

    return 0;
    
} /* open_pnp */
#endif /* USE_GTK2 */
