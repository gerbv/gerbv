/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
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
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LIBGEN_H
#include <libgen.h> /* dirname */
#endif

#include <math.h>
#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #include "draw.h"
  #include <cairo-xlib.h>
#endif

#include "color.h"
#include "gerbv_screen.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "callbacks.h"
#include "interface.h"

#include "render.h"

#ifdef EXPORT_PNG
#include "exportimage.h"
#endif /* EXPORT_PNG */

#define dprintf if(DEBUG) printf

#define SAVE_PROJECT 0
#define SAVE_AS_PROJECT 1
#define OPEN_PROJECT 2
#  define _(String) (String)

/**Global variable to keep track of what's happening on the screen.
   Declared extern in gerbv_screen.h
 */
extern gerbv_screen_t screen;

void load_project(project_list_t *project_list);
int open_image(char *filename, int idx, int reload);
void gerbv_open_layer_from_filename (gchar *filename);
void gerbv_open_project_from_filename (gchar *filename);
void gerbv_save_as_project_from_filename (gchar *filename);
void gerbv_save_project_from_filename (gchar *filename);
void gerbv_revert_all_files (void);
void gerbv_unload_all_layers (void);
void gerbv_unload_layer (int index);
void gerbv_export_to_png_file (int width, int height, gchar *filename);
void gerbv_export_to_pdf_file (gchar *filename);
void gerbv_export_to_postscript_file (gchar *filename);
void gerbv_export_to_svg_file (gchar *filename);
void gerbv_change_layer_order (gint oldPosition, gint newPosition);

/* --------------------------------------------------------- */
#if 0 //ndef RENDER_USING_GDK
static cairo_t *
callbacks_gdk_cairo_create (GdkDrawable *target)
{
	int width, height;
	int x_off=0, y_off=0;
	cairo_t *cr;
	cairo_surface_t *surface;
	GdkDrawable *drawable = target;
	GdkVisual *visual;

	if (GDK_IS_WINDOW(target)) {
	      /* query the window's backbuffer if it has one */
		GdkWindow *window = GDK_WINDOW(target);
	      gdk_window_get_internal_paint_info (window,
	                                          &drawable, &x_off, &y_off);
	}
	visual = gdk_drawable_get_visual (drawable);
	gdk_drawable_get_size (drawable, &width, &height);

	if (visual) {
	      surface = cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable),
	                                          GDK_DRAWABLE_XID (drawable),
	                                          GDK_VISUAL_XVISUAL (visual),
	                                          width, height);
	} else if (gdk_drawable_get_depth (drawable) == 1) {
	      surface = cairo_xlib_surface_create_for_bitmap
	      (GDK_PIXMAP_XDISPLAY (drawable),
	            GDK_PIXMAP_XID (drawable),
	            GDK_SCREEN_XSCREEN (gdk_drawable_get_screen (drawable)),
	            width, height);
	} else {
	      return NULL;
	}
	cairo_surface_set_device_offset (surface, -x_off, -y_off);
	cr = cairo_create (surface);
	cairo_surface_destroy (surface);
	return cr;
}
#endif

/* --------------------------------------------------------- */
void
callbacks_new_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	/* first, unload all files */
	if (screen.project) {
	    g_free(screen.project);
	    screen.project = NULL;
	}
	gerbv_unload_all_layers ();
	callbacks_generic_save_activate (menuitem, (gpointer) CALLBACKS_SAVE_FILE_AS);
}


/* --------------------------------------------------------- */
void
callbacks_open_project_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar *filename=NULL;

	screen.win.gerber = 
	gtk_file_chooser_dialog_new ("Open project file...",
				     NULL,
				     GTK_FILE_CHOOSER_ACTION_OPEN,
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
				     NULL);

	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filename =
		    gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (screen.win.gerber));
	}
	gtk_widget_destroy (screen.win.gerber);

	if (filename)
		gerbv_open_project_from_filename (filename);

	redraw_pixmap(screen.drawing_area, TRUE);
	callbacks_update_layer_tree();
	return;
}


/* --------------------------------------------------------- */
void
callbacks_open_layer_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GSList *filenames=NULL;
	GSList *filename=NULL;

	screen.win.gerber = 
	gtk_file_chooser_dialog_new ("Open Gerber file(s)...",
				     NULL,
				     GTK_FILE_CHOOSER_ACTION_OPEN,
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
				     NULL);

	g_object_set (screen.win.gerber, "select-multiple", TRUE, NULL);

	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filenames =
		    gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER (screen.win.gerber));
	}
	gtk_widget_destroy (screen.win.gerber);

	/* Now try to open all gerbers specified */
	for (filename=filenames; filename; filename=filename->next) {
		gerbv_open_layer_from_filename (filename->data);
	}
	g_slist_free(filenames);

	redraw_pixmap(screen.drawing_area, TRUE);
	callbacks_update_layer_tree();
	return;
}

/* --------------------------------------------------------- */
void
callbacks_revert_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gerbv_revert_all_files ();
	redraw_pixmap(screen.drawing_area, TRUE);
}

/* --------------------------------------------------------- */
void
callbacks_save_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (screen.project)
		gerbv_save_project_from_filename (screen.project);
	else
		callbacks_generic_save_activate (menuitem, (gpointer) CALLBACKS_SAVE_FILE_AS);
	redraw_pixmap(screen.drawing_area, TRUE);
	return;
}

/* --------------------------------------------------------- */
void
callbacks_generic_save_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar *filename=NULL;
	gint processType = (gint) user_data;
	gchar *windowTitle=NULL;
	
	if (processType == CALLBACKS_SAVE_FILE_AS)
		windowTitle = g_strdup ("Save project as...");
	else if (processType == CALLBACKS_SAVE_FILE_PS)
		windowTitle = g_strdup ("Export PS file as...");
	else if (processType == CALLBACKS_SAVE_FILE_PDF)
		windowTitle = g_strdup ("Export PDF file as...");
	else if (processType == CALLBACKS_SAVE_FILE_SVG)
		windowTitle = g_strdup ("Export SVG file as...");
	else if (processType == CALLBACKS_SAVE_FILE_PNG)
		windowTitle = g_strdup ("Export PNG file as...");
	
	screen.win.gerber = 
	gtk_file_chooser_dialog_new (windowTitle, NULL,
				     GTK_FILE_CHOOSER_ACTION_SAVE,
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     GTK_STOCK_SAVE,   GTK_RESPONSE_ACCEPT,
				     NULL);
	g_free (windowTitle);
	
	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (screen.win.gerber));
	}
	gtk_widget_destroy (screen.win.gerber);

	if (filename) {
		if (processType == CALLBACKS_SAVE_FILE_AS) {
			gerbv_save_as_project_from_filename (filename);
			rename_main_window(filename, NULL);
		}
		else if (processType == CALLBACKS_SAVE_FILE_PS)
			gerbv_export_to_postscript_file (filename);
		else if (processType == CALLBACKS_SAVE_FILE_PDF)
			gerbv_export_to_pdf_file (filename);
		else if (processType == CALLBACKS_SAVE_FILE_SVG)
			gerbv_export_to_svg_file (filename);
#ifdef EXPORT_PNG
		else if (processType == CALLBACKS_SAVE_FILE_PNG)
			gerbv_export_to_png_file (screen.drawing_area->allocation.width,
				screen.drawing_area->allocation.height, filename);
#endif
		redraw_pixmap(screen.drawing_area, TRUE);
	}
	return;
}

/* --------------------------------------------------------- */
#if GTK_CHECK_VERSION(2,10,0)
static void
callbacks_print_render_page (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           gint               page_nr,
           gpointer           user_data)
{
	//cairo_t *cr;
	
	//cr = gtk_print_context_get_cairo_context (context);
	//render_project_to_cairo_target (cr);
	//cairo_destroy (cr);
}

/* --------------------------------------------------------- */
void
callbacks_print_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkPrintOperation *print;
	GtkPrintOperationResult res;

	print = gtk_print_operation_new ();

	//g_signal_connect (print, "begin_print", G_CALLBACK (begin_print), NULL);
	g_signal_connect (print, "draw_page", G_CALLBACK (callbacks_print_render_page), NULL);

	res = gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                              (GtkWindow *) screen.win.topLevelWindow , NULL);

	g_object_unref (print);
}
#endif

void
callbacks_zoom_in_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gerbv_zoom_data_t z_data;

	z_data.z_dir = ZOOM_IN;
	zoom(NULL, &z_data);
}


void
callbacks_zoom_out_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gerbv_zoom_data_t z_data;

	z_data.z_dir = ZOOM_OUT;
	zoom(NULL, &z_data);
}


void
callbacks_fit_to_window_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	autoscale();
	redraw_pixmap(screen.drawing_area, TRUE);
}


/* --------------------------------------------------------- */
void
callbacks_analyze_active_gerbers_activate(GtkMenuItem *menuitem, 
				 gpointer user_data)
{
    gerb_stats_t *stats_report;
    gchar *G_report_string;
    gchar *D_report_string;
    gchar *M_report_string;
    gchar *misc_report_string;
    
    stats_report = generate_gerber_analysis();

    G_report_string = g_strdup_printf("G code statistics   \n");
    G_report_string = g_strdup_printf("%sG0 = %d\n", G_report_string, stats_report->G0);
    G_report_string = g_strdup_printf("%sG1 = %d\n", G_report_string, stats_report->G1);
    G_report_string = g_strdup_printf("%sG2 = %d\n", G_report_string, stats_report->G2);
    G_report_string = g_strdup_printf("%sG3 = %d\n", G_report_string, stats_report->G3);
    G_report_string = g_strdup_printf("%sG4 = %d\n", G_report_string, stats_report->G4);
    G_report_string = g_strdup_printf("%sG10 = %d\n", G_report_string, stats_report->G10);
    G_report_string = g_strdup_printf("%sG11 = %d\n", G_report_string, stats_report->G11);
    G_report_string = g_strdup_printf("%sG12 = %d\n", G_report_string, stats_report->G12);
    G_report_string = g_strdup_printf("%sG36 = %d\n", G_report_string, stats_report->G36);
    G_report_string = g_strdup_printf("%sG37 = %d\n", G_report_string, stats_report->G37);
    G_report_string = g_strdup_printf("%sG54 = %d\n", G_report_string, stats_report->G54);
    G_report_string = g_strdup_printf("%sG55 = %d\n", G_report_string, stats_report->G55);
    G_report_string = g_strdup_printf("%sG70 = %d\n", G_report_string, stats_report->G70);
    G_report_string = g_strdup_printf("%sG71 = %d\n", G_report_string, stats_report->G71);
    G_report_string = g_strdup_printf("%sG74 = %d\n", G_report_string, stats_report->G74);
    G_report_string = g_strdup_printf("%sG75 = %d\n", G_report_string, stats_report->G75);
    G_report_string = g_strdup_printf("%sG90 = %d\n", G_report_string, stats_report->G90);
    G_report_string = g_strdup_printf("%sG91 = %d\n", G_report_string, stats_report->G91);
    G_report_string = g_strdup_printf("%sUnknown G codes = %d\n", 
				      G_report_string, stats_report->G_unknown);


    D_report_string = g_strdup_printf("D code statistics   \n");
    D_report_string = g_strdup_printf("%sD1 = %d\n", D_report_string, stats_report->D1);
    D_report_string = g_strdup_printf("%sD2 = %d\n", D_report_string, stats_report->D2);
    D_report_string = g_strdup_printf("%sD3 = %d\n", D_report_string, stats_report->D3);
    /* Insert stuff about user defined codes here */
    D_report_string = g_strdup_printf("%sUndefined D codes = %d\n", 
				      D_report_string, stats_report->D_unknown);
    D_report_string = g_strdup_printf("%sD code Errors = %d\n", 
				      D_report_string, stats_report->D_error);


    M_report_string = g_strdup_printf("M code statistics   \n");
    M_report_string = g_strdup_printf("%sM0 = %d\n", M_report_string, stats_report->M0);
    M_report_string = g_strdup_printf("%sM1 = %d\n", M_report_string, stats_report->M1);
    M_report_string = g_strdup_printf("%sM2 = %d\n", M_report_string, stats_report->M2);
    M_report_string = g_strdup_printf("%sUnknown M codes = %d\n", 
				      M_report_string, stats_report->M_unknown);


    misc_report_string = g_strdup_printf("Misc code statistics   \n");
    misc_report_string = g_strdup_printf("%sX = %d\n", misc_report_string, stats_report->X);
    misc_report_string = g_strdup_printf("%sY = %d\n", misc_report_string, stats_report->Y);
    misc_report_string = g_strdup_printf("%sI = %d\n", misc_report_string, stats_report->I);
    misc_report_string = g_strdup_printf("%sJ = %d\n", misc_report_string, stats_report->J);
    misc_report_string = g_strdup_printf("%s* = %d\n", misc_report_string, stats_report->star);
    misc_report_string = g_strdup_printf("%sUnknown codes = %d\n", 
				      misc_report_string, stats_report->unknown);



    /* Create top level dialog window for report */
    GtkWidget *analyze_active_gerbers;
    analyze_active_gerbers = gtk_dialog_new_with_buttons("Gerber codes report",
							NULL,
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
    gtk_container_set_border_width (GTK_CONTAINER (analyze_active_gerbers), 5);
    gtk_dialog_set_default_response (GTK_DIALOG(analyze_active_gerbers), 
				     GTK_RESPONSE_ACCEPT);
    g_signal_connect (G_OBJECT(analyze_active_gerbers),
		      "response",
		      G_CALLBACK (gtk_widget_destroy), 
		      GTK_WIDGET(analyze_active_gerbers));


    /* Create GtkLabel to hold G code text */
    GtkWidget *G_report_label = gtk_label_new (G_report_string);
    gtk_misc_set_alignment(GTK_MISC(G_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(G_report_label), 13, 13);
    g_free(G_report_string);

    /* Create GtkLabel to hold D code text */
    GtkWidget *D_report_label = gtk_label_new (D_report_string);
    gtk_misc_set_alignment(GTK_MISC(D_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(D_report_label), 13, 13);
    g_free(D_report_string);

    /* Create GtkLabel to hold M code text */
    GtkWidget *M_report_label = gtk_label_new (M_report_string);
    gtk_misc_set_alignment(GTK_MISC(M_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(M_report_label), 13, 13);
    g_free(M_report_string);

    /* Create GtkLabel to hold misc code text */
    GtkWidget *misc_report_label = gtk_label_new (misc_report_string);
    gtk_misc_set_alignment(GTK_MISC(misc_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(misc_report_label), 13, 13);
    g_free(misc_report_string);

    /* Create tabbed notebook widget and add report label widgets. */
    GtkWidget *notebook = gtk_notebook_new();
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(G_report_label),
			     gtk_label_new("G codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(D_report_label),
			     gtk_label_new("D codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(M_report_label),
			     gtk_label_new("M codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(misc_report_label),
			     gtk_label_new("Misc. codes"));
    
    
    /* Now put notebook into dialog window and show the whole thing */
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(analyze_active_gerbers)->vbox),
		      GTK_WIDGET(notebook));
    gtk_widget_show_all(analyze_active_gerbers);
	
    return;

}

/* --------------------------------------------------------- */
void
callbacks_analyze_active_drill_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    drill_stats_t *stats_report;
    gchar *G_report_string;
    gchar *M_report_string;
    gchar *misc_report_string;
    
    stats_report = (drill_stats_t *) generate_drill_analysis();

    G_report_string = g_strdup_printf("G code statistics   \n");
    G_report_string = g_strdup_printf("%sG00 = %d\n", 
				      G_report_string, stats_report->G00);
    G_report_string = g_strdup_printf("%sG01 = %d\n", 
				      G_report_string, stats_report->G01);
    G_report_string = g_strdup_printf("%sG02 = %d\n", 
				      G_report_string, stats_report->G02);
    G_report_string = g_strdup_printf("%sG03 = %d\n", 
				      G_report_string, stats_report->G03);
    G_report_string = g_strdup_printf("%sG05 = %d\n", 
				      G_report_string, stats_report->G05);
    G_report_string = g_strdup_printf("%sG90 = %d\n", 
				      G_report_string, stats_report->G90);
    G_report_string = g_strdup_printf("%sG91 = %d\n", 
				      G_report_string, stats_report->G91);
    G_report_string = g_strdup_printf("%sG93 = %d\n", 
				      G_report_string, stats_report->G93);
    G_report_string = g_strdup_printf("%sUnknown G codes = %d\n", 
				      G_report_string, stats_report->G_unknown);

    M_report_string = g_strdup_printf("M code statistics   \n");
    M_report_string = g_strdup_printf("%sM00 = %d\n", 
				      M_report_string, stats_report->M00);
    M_report_string = g_strdup_printf("%sM01 = %d\n", 
				      M_report_string, stats_report->M01);
    M_report_string = g_strdup_printf("%sM18 = %d\n", 
				      M_report_string, stats_report->M18);
    M_report_string = g_strdup_printf("%sM25 = %d\n", 
				      M_report_string, stats_report->M25);
    M_report_string = g_strdup_printf("%sM30 = %d\n", 
				      M_report_string, stats_report->M30);
    M_report_string = g_strdup_printf("%sM31 = %d\n", 
				      M_report_string, stats_report->M31);
    M_report_string = g_strdup_printf("%sM45 = %d\n", 
				      M_report_string, stats_report->M45);
    M_report_string = g_strdup_printf("%sM47 = %d\n", 
				      M_report_string, stats_report->M47);
    M_report_string = g_strdup_printf("%sM48 = %d\n", 
				      M_report_string, stats_report->M48);
    M_report_string = g_strdup_printf("%sM71 = %d\n", 
				      M_report_string, stats_report->M71);
    M_report_string = g_strdup_printf("%sM72 = %d\n", 
				      M_report_string, stats_report->M72);
    M_report_string = g_strdup_printf("%sM95 = %d\n", 
				      M_report_string, stats_report->M95);
    M_report_string = g_strdup_printf("%sM97 = %d\n", 
				      M_report_string, stats_report->M97);
    M_report_string = g_strdup_printf("%sM98 = %d\n", 
				      M_report_string, stats_report->M98);
    M_report_string = g_strdup_printf("%sUnknown M codes = %d\n", 
				      M_report_string, stats_report->M_unknown);


    misc_report_string = g_strdup_printf("Misc code statistics   \n");
    misc_report_string = g_strdup_printf("%scomments = %d\n", 
					 misc_report_string, stats_report->comment);
    misc_report_string = g_strdup_printf("%sUnknown codes = %d\n", 
					 misc_report_string, stats_report->unknown);



    /* Create top level dialog window for report */
    GtkWidget *analyze_active_drill;
    analyze_active_drill = gtk_dialog_new_with_buttons("Drill file codes report",
							NULL,
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_OK,
							GTK_RESPONSE_ACCEPT,
							NULL);
    gtk_container_set_border_width (GTK_CONTAINER (analyze_active_drill), 5);
    gtk_dialog_set_default_response (GTK_DIALOG(analyze_active_drill), 
				     GTK_RESPONSE_ACCEPT);
    g_signal_connect (G_OBJECT(analyze_active_drill),
		      "response",
		      G_CALLBACK (gtk_widget_destroy), 
		      GTK_WIDGET(analyze_active_drill));


    /* Create GtkLabel to hold G code text */
    GtkWidget *G_report_label = gtk_label_new (G_report_string);
    gtk_misc_set_alignment(GTK_MISC(G_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(G_report_label), 13, 13);
    g_free(G_report_string);

    /* Create GtkLabel to hold M code text */
    GtkWidget *M_report_label = gtk_label_new (M_report_string);
    gtk_misc_set_alignment(GTK_MISC(M_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(M_report_label), 13, 13);
    g_free(M_report_string);

    /* Create GtkLabel to hold misc code text */
    GtkWidget *misc_report_label = gtk_label_new (misc_report_string);
    gtk_misc_set_alignment(GTK_MISC(misc_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(misc_report_label), 13, 13);
    g_free(misc_report_string);

    /* Create tabbed notebook widget and add report label widgets. */
    GtkWidget *notebook = gtk_notebook_new();
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(G_report_label),
			     gtk_label_new("G codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(M_report_label),
			     gtk_label_new("M codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(misc_report_label),
			     gtk_label_new("Misc. codes"));
    
    
    /* Now put notebook into dialog window and show the whole thing */
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(analyze_active_drill)->vbox),
		      GTK_WIDGET(notebook));
    gtk_widget_show_all(analyze_active_drill);
	
    return;


}

/* --------------------------------------------------------- */
void
callbacks_control_gerber_options_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

/* --------------------------------------------------------- */
void
callbacks_pointer_tool_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

/* --------------------------------------------------------- */
void
callbacks_zoom_tool_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

/* --------------------------------------------------------- */
void
callbacks_measure_tool_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

/* --------------------------------------------------------- */
void
callbacks_online_manual_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

/* --------------------------------------------------------- */
void
callbacks_quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gerbv_unload_all_layers ();

	/* Free all colors allocated */
	if (screen.background)
		g_free(screen.background);
	if (screen.zoom_outline_color)
		g_free(screen.zoom_outline_color);
	if (screen.dist_measure_color)
		g_free(screen.dist_measure_color);

	setup_destroy();

	gtk_main_quit();
}

/* --------------------------------------------------------- */
void
callbacks_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *aboutdialog1;
	/* TRANSLATORS: Replace this string with your names, one name per line. */
	//gchar *translators = _("translator-credits");

	aboutdialog1 = gtk_about_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (aboutdialog1), 5);
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (aboutdialog1), VERSION);
	gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (aboutdialog1), _("Gerbv"));
	//gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (aboutdialog1), translators);

	gchar *string = g_strdup_printf ( "gerbv -- a Gerber (RS-274/X) viewer.\n\n"
	      "This is gerbv version %s\n"
	      "Compiled on %s at %s\n"
	      "\n"
	      "gerbv is part of the gEDA Project.\n"
	      "\n"
	      "For more information see:\n"
	      "  gerbv homepage: http://gerbv.sf.net\n"
	      "  gEDA homepage: http://www.geda.seul.org\n"
	      "  gEDA Wiki: http://geda.seul.org/dokuwiki/doku.php?id=geda\n\n",
	      VERSION, __DATE__, __TIME__);
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (aboutdialog1), string);
	g_free (string);
	/* Store pointers to all widgets, for use by lookup_widget(). */
	g_signal_connect (G_OBJECT(aboutdialog1),"response",
		      G_CALLBACK (gtk_widget_destroy), GTK_WIDGET(aboutdialog1));
	gtk_widget_show_all(aboutdialog1);
}

gdouble callbacks_calculate_actual_distance (gdouble inputDimension) {
	gdouble returnValue = 0.0;
	
	if (screen.unit == GERBV_MILS) {
	    returnValue = COORD2MILS(inputDimension);
	} else if (screen.unit == GERBV_MMS) {
	    returnValue = COORD2MMS(inputDimension);
	} else {
	    returnValue = COORD2MILS(inputDimension)/1000;
	}
	return returnValue;
}
void callbacks_update_ruler_pointers (void) {
	double xPosition, yPosition;

	xPosition = screen.gerber_bbox.x1 + (screen.last_x + screen.trans_x)/(double)screen.transf->scale;
	yPosition = screen.gerber_bbox.y2 + (screen.last_y - screen.trans_y)/(double)screen.transf->scale;

	xPosition = callbacks_calculate_actual_distance (xPosition);
	yPosition = callbacks_calculate_actual_distance (yPosition);
	//g_object_set (G_OBJECT (screen.win.hRuler), "position", xPosition, NULL);
	//g_object_set (G_OBJECT (screen.win.vRuler), "position", yPosition, NULL);
}

void callbacks_update_ruler_scales (void) {
	double xStart, xEnd, yStart, yEnd;

	xStart = screen.gerber_bbox.x1 + (screen.trans_x)/(double)screen.transf->scale;
	xEnd = screen.gerber_bbox.x2 + (screen.trans_x)/(double)screen.transf->scale;
	yStart = (screen.gerber_bbox.y1 + (screen.trans_y)/(double)screen.transf->scale);
	yEnd = (screen.gerber_bbox.y2 + (screen.trans_y)/(double)screen.transf->scale);
	
	xStart = callbacks_calculate_actual_distance (xStart);
	xEnd = callbacks_calculate_actual_distance (xEnd);
	yStart = callbacks_calculate_actual_distance (yStart);
	yEnd = callbacks_calculate_actual_distance (yEnd);
	
	//gtk_ruler_set_range (GTK_RULER (screen.win.hRuler), xStart, xEnd, 0, xEnd - xStart);
	//gtk_ruler_set_range (GTK_RULER (screen.win.vRuler), yStart, yEnd, 0, yEnd - yStart);
}

void callbacks_hadjustment_value_changed (GtkAdjustment *adjustment,
			gpointer user_data){
	gdouble newValue;
	
	newValue = gtk_adjustment_get_value (adjustment);
}

void callbacks_vadjustment_value_changed (GtkAdjustment *adjustment,
			gpointer user_data){
	gdouble newValue;
	
	newValue = gtk_adjustment_get_value (adjustment);			
}

void
callbacks_layer_tree_visibility_button_toggled (GtkCellRendererToggle *cell_renderer,
                                                        gchar *path,
                                                        gpointer user_data){
	GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
			((GtkTreeView *) screen.win.layerTree);
	GtkTreeIter iter;
	gboolean newVisibility=TRUE;
	gint index;
	
	gtk_tree_model_get_iter_from_string ((GtkTreeModel *)list_store, &iter, path);
	
	GtkTreePath *treePath = gtk_tree_path_new_from_string (path);
	if (gtk_tree_model_get_iter((GtkTreeModel *)list_store, &iter, treePath)) {
	      gint *indeces;
	      
	      indeces = gtk_tree_path_get_indices (treePath);
	      index = indeces[0];
		if (screen.file[index]->isVisible)
			 newVisibility = FALSE;
		screen.file[index]->isVisible = newVisibility;

	      callbacks_update_layer_tree ();
#ifdef RENDER_USING_GDK
		redraw_pixmap(screen.drawing_area, TRUE);
#else
		render_recreate_composite_surface (screen.drawing_area);
#endif 
	}
}

gint
callbacks_get_col_number_from_tree_view_column (GtkTreeViewColumn *col)
{
	GList *cols;
	gint   num;
	
	g_return_val_if_fail ( col != NULL, -1 );
	g_return_val_if_fail ( col->tree_view != NULL, -1 );
	cols = gtk_tree_view_get_columns(GTK_TREE_VIEW(col->tree_view));
	num = g_list_index(cols, (gpointer) col);
	g_list_free(cols);
	return num;
}

void
callbacks_color_selector_cancel_clicked (GtkWidget *widget, gpointer user_data) {
	gtk_widget_destroy (screen.win.colorSelectionDialog);
	screen.win.colorSelectionDialog = NULL;
}

void
callbacks_add_layer_button_clicked  (GtkButton *button, gpointer   user_data) {
	callbacks_open_layer_activate (NULL, NULL);
}

void
callbacks_select_row (gint rowIndex) {
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
			((GtkTreeView *) screen.win.layerTree);

	selection = gtk_tree_view_get_selection((GtkTreeView *) screen.win.layerTree);
	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store), &iter, NULL, rowIndex)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}
}


gint
callbacks_get_selected_row_index  (void) {
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
			((GtkTreeView *) screen.win.layerTree);
	gint index=-1,i=0;
	
	/* This will only work in single or browse selection mode! */
	selection = gtk_tree_view_get_selection((GtkTreeView *) screen.win.layerTree);
	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		while (gtk_tree_model_iter_nth_child ((GtkTreeModel *)list_store,
				&iter, NULL, i)){
			if (gtk_tree_selection_iter_is_selected (selection, &iter)) {
				return i;
			}
			i++;
     		}	
	}
	return index;
}

void
callbacks_remove_layer_button_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	
	if ((index >= 0) && (index <= screen.last_loaded)) {
		gerbv_unload_layer (index);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (index);
	      redraw_pixmap(screen.drawing_area, TRUE);	
	}
}

void
callbacks_move_layer_down_button_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	
	if ((index >= 0) && (index < screen.last_loaded)) {
		gerbv_change_layer_order (index, index + 1);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (index + 1);
	      redraw_pixmap(screen.drawing_area, TRUE);
	}
}

void
callbacks_move_layer_up_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	
	if (index > 0) {
		gerbv_change_layer_order (index, index - 1);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (index - 1);
	      redraw_pixmap(screen.drawing_area, TRUE);	
	}
}

void callbacks_layer_tree_row_inserted (GtkTreeModel *tree_model, GtkTreePath  *path,
                              GtkTreeIter  *oIter, gpointer user_data) {
      gint *indeces=NULL,oldPosition,newPosition;
      
	if ((!screen.win.treeIsUpdating)&&(path != NULL)) {
		indeces = gtk_tree_path_get_indices (path);
		if (indeces) {
			newPosition = indeces[0];
			oldPosition = callbacks_get_selected_row_index ();
			/* compensate for the fact that the old row has already
			   been removed */
			if (oldPosition < newPosition)
				newPosition--;
			else
				oldPosition--;
			gerbv_change_layer_order (oldPosition, newPosition);

			redraw_pixmap(screen.drawing_area, TRUE);
			
			/* select the new line */
			GtkTreeSelection *selection;
			GtkTreeIter iter;
			GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
				((GtkTreeView *) screen.win.layerTree);
			
			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(screen.win.layerTree));
			if (gtk_tree_model_get_iter ((GtkTreeModel *)list_store, &iter, path))
				gtk_tree_selection_select_iter (selection, &iter);
		}
	}
}

void
callbacks_color_selector_ok_clicked (GtkWidget *widget, gpointer user_data) {
	GtkColorSelectionDialog *cs = (GtkColorSelectionDialog *) screen.win.colorSelectionDialog;
	GtkColorSelection *colorsel = (GtkColorSelection *) cs->colorsel;
	gint rowIndex = screen.win.colorSelectionIndex;
#ifndef RENDER_USING_GDK
	screen.file[rowIndex]->alpha = gtk_color_selection_get_current_alpha (colorsel);
#endif
	gtk_color_selection_get_current_color (colorsel, screen.file[rowIndex]->color);
	gtk_widget_destroy (screen.win.colorSelectionDialog);
	screen.win.colorSelectionDialog = NULL;
	callbacks_update_layer_tree ();
      redraw_pixmap(screen.drawing_area, TRUE);
}


gboolean
callbacks_layer_tree_button_press (GtkWidget *widget, GdkEventButton *event,
                                   gpointer user_data) {
      GtkTreePath *path;
      GtkTreeIter iter;
      GtkTreeViewColumn *column;
      gint x,y;
      gint columnIndex;
      
      GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
			((GtkTreeView *) screen.win.layerTree);
      
      if (gtk_tree_view_get_path_at_pos  ((GtkTreeView *) widget, event->x, event->y,
      	&path, &column, &x, &y)) {
	      if (gtk_tree_model_get_iter((GtkTreeModel *)list_store, &iter, path)) {
	      	gint *indeces;
	      	indeces = gtk_tree_path_get_indices (path);
	      	if (indeces) {
				//gtk_tree_model_get((GtkTreeModel *)list_store, &iter, 0, &rowIndex, -1);
				columnIndex = callbacks_get_col_number_from_tree_view_column (column);
				if ((columnIndex == 1) && (indeces[0] <= screen.last_loaded)){
					if (!screen.win.colorSelectionDialog) {
						GtkColorSelectionDialog *cs= (GtkColorSelectionDialog *) gtk_color_selection_dialog_new ("Select a color");
						GtkColorSelection *colorsel = (GtkColorSelection *) cs->colorsel;
						
						screen.win.colorSelectionDialog = (GtkWidget *) cs;
						screen.win.colorSelectionIndex = indeces[0];
						gtk_color_selection_set_current_color (colorsel,
							screen.file[indeces[0]]->color);
#ifndef RENDER_USING_GDK
						gtk_color_selection_set_has_opacity_control (colorsel, TRUE);
						gtk_color_selection_set_current_alpha (colorsel, screen.file[indeces[0]]->alpha);
#endif
						gtk_widget_show((GtkWidget *)cs);
						g_signal_connect (G_OBJECT(cs->ok_button),"clicked",
							G_CALLBACK (callbacks_color_selector_ok_clicked), NULL);
						g_signal_connect (G_OBJECT(cs->cancel_button),"clicked",
							G_CALLBACK (callbacks_color_selector_cancel_clicked), NULL);
						/* stop signal propagation if the user clicked on the color swatch */
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

void
callbacks_update_layer_tree (void) {
	GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
			((GtkTreeView *) screen.win.layerTree);
	gint idx;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gint oldSelectedRow;
	
	if (!screen.win.treeIsUpdating) {
		screen.win.treeIsUpdating = TRUE;
		
		oldSelectedRow = callbacks_get_selected_row_index();
		if (oldSelectedRow < 0)
			oldSelectedRow = 0;
		gtk_list_store_clear (list_store);

		for (idx = 0; idx < MAX_FILES; idx++) {
			if (screen.file[idx]) {
				GdkPixbuf    *pixbuf;
				guint32 color;
				
				unsigned char red, green, blue, alpha;
				
				red = (unsigned char) (screen.file[idx]->color->red * 255 / G_MAXUINT16) ;
				green = (unsigned char) (screen.file[idx]->color->green * 255 / G_MAXUINT16) ;
				blue = (unsigned char) (screen.file[idx]->color->blue *255 / G_MAXUINT16) ;
				alpha = (unsigned char) (screen.file[idx]->alpha * 255 / G_MAXUINT16) ;
				
				color = (red )* (256*256*256) + (green ) * (256*256) + (blue )* (256) + (alpha );
				pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 20, 15);
				gdk_pixbuf_fill (pixbuf, color);

				gtk_list_store_append (list_store, &iter);
				
				/* strip the filename to the base */
				gchar *baseName = g_path_get_basename (screen.file[idx]->name);
				gtk_list_store_set (list_store, &iter,
							0, screen.file[idx]->isVisible,
							1, pixbuf,
			                        2, baseName,
			                        -1);
			      g_free (baseName);
			      /* pixbuf has a refcount of 2 now, as the list store has added its own reference */
			      g_object_unref(pixbuf);
			}
		}
		
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(screen.win.layerTree));
		
		/* if no line is selected yet, select the first row (if it has data) */
		/* or, select the line that was previously selected */
		
		if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
			if (gtk_tree_model_iter_nth_child ((GtkTreeModel *) list_store,
							&iter, NULL, oldSelectedRow)) {
				gtk_tree_selection_select_iter (selection, &iter);
			}
		}
		screen.win.treeIsUpdating = FALSE;
	}
}

gboolean
callbacks_drawingarea_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
#ifndef RENDER_USING_GDK
	
	GdkDrawable *drawable = widget->window;
	
	int x_off=0, y_off=0;
	GdkVisual *visual;
	if (GDK_IS_WINDOW(widget->window)) {
	      /* query the window's backbuffer if it has one */
		GdkWindow *window = GDK_WINDOW(widget->window);
	      gdk_window_get_internal_paint_info (window,
	                                          &drawable, &x_off, &y_off);
	}
	visual = gdk_drawable_get_visual (drawable);
	gdk_drawable_get_size (drawable, &screen.canvasWidth, &screen.canvasHeight);
	if (!screen.windowSurface) {
		screen.windowSurface = (gpointer) cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable),
	                                          GDK_DRAWABLE_XID (drawable),
	                                          GDK_VISUAL_XVISUAL (visual),
	                                          screen.canvasWidth, screen.canvasHeight);
	}
#endif
	if (screen.transf->scale < 0.001) {
		autoscale();
	}
	return redraw_pixmap(widget, TRUE);
}


gboolean
callbacks_drawingarea_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
#ifdef RENDER_USING_GDK

	GdkPixmap *new_pixmap;
	GdkGC *gc = gdk_gc_new(widget->window);

	/*
	* Create a pixmap with default background
	*/
	new_pixmap = gdk_pixmap_new(widget->window,
				widget->allocation.width,
				widget->allocation.height,
				-1);

	gdk_gc_set_foreground(gc, screen.background);

	gdk_draw_rectangle(new_pixmap, gc, TRUE, 
		       event->area.x, event->area.y,
		       event->area.width, event->area.height);

	/*
	* Copy gerber pixmap onto background if we have one to copy.
	* Do translation at the same time.
	*/
	if (screen.pixmap != NULL) {
	gdk_draw_pixmap(new_pixmap,
			widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			screen.pixmap, 
			event->area.x + screen.off_x, 
			event->area.y + screen.off_y, 
			event->area.x, event->area.y,
			event->area.width, event->area.height);
	}

	/*
	* Draw the whole thing onto screen
	*/
	gdk_draw_pixmap(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    new_pixmap,
		    event->area.x, event->area.y,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);

	gdk_pixmap_unref(new_pixmap);
	gdk_gc_unref(gc);

	/*
	* Draw Zooming outline if we are in that mode
	*/
	if (screen.state == ZOOM_OUTLINE) {
		draw_zoom_outline(screen.centered_outline_zoom);
	}
	else if (screen.state == MEASURE) {
		draw_measure_distance();
	}

	return FALSE;

#else
	cairo_t *cr;
	int width, height;
	cairo_surface_t *buffert;
	int x_off=0, y_off=0;
	GdkDrawable *drawable = widget->window;
	GdkVisual *visual;

	if (GDK_IS_WINDOW(widget->window)) {
	      /* query the window's backbuffer if it has one */
		GdkWindow *window = GDK_WINDOW(widget->window);
	      gdk_window_get_internal_paint_info (window,
	                                          &drawable, &x_off, &y_off);
	}
	visual = gdk_drawable_get_visual (drawable);
	gdk_drawable_get_size (drawable, &width, &height);

	buffert = (gpointer) cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable),
	                                          GDK_DRAWABLE_XID (drawable),
	                                          GDK_VISUAL_XVISUAL (visual),
	                                          event->area.width, event->area.height);
		       
	cr = cairo_create (buffert);
	cairo_translate (cr, -event->area.x - screen.off_x, -event->area.y - screen.off_y);
	render_project_to_cairo_target (cr);
	cairo_destroy (cr);
	cairo_surface_destroy (buffert);
        
	return FALSE;
#endif

}

gboolean
callbacks_drawingarea_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
	int x, y;
	GdkModifierType state;
	GdkRectangle update_rect;

	if (event->is_hint)
		gdk_window_get_pointer (event->window, &x, &y, &state);
	else {
		x = event->x;
		y = event->y;
		state = event->state;
	}

	//if (screen.pixmap != NULL) {
		double X, Y;

		X = screen.gerber_bbox.x1 + (x+screen.trans_x)/(double)screen.transf->scale;
		Y = (screen.gerber_bbox.y2 - (y+screen.trans_y)/(double)screen.transf->scale);
		if (screen.unit == GERBV_MILS) {
		    snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
			     "(%7.1f, %7.1f)",
			     COORD2MILS(X), COORD2MILS(Y));
		} else if (screen.unit == GERBV_MMS) {
		    snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
			     "(%7.2f, %7.2f)",
			     COORD2MMS(X), COORD2MMS(Y));
		} else {
		    snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
			     "(%3.4f, %3.4f)",
			     COORD2MILS(X) / 1000.0, COORD2MILS(Y) / 1000.0);
		}
		callbacks_update_statusbar();

		switch (screen.state) {
		case MOVE: {
		    
		    x = widget->allocation.height - x;
		    y = widget->allocation.width - y;
		    
		    if (screen.last_x != 0 || screen.last_y != 0) {
			screen.trans_x = screen.trans_x + x - screen.last_x;
			screen.trans_y = screen.trans_y + y - screen.last_y;

			screen.clip_bbox.x1 = -screen.trans_x/(double)screen.transf->scale;
			screen.clip_bbox.y1 = -screen.trans_y/(double)screen.transf->scale;
			
			/* Move pixmap to get a snappier feel of movement */
			screen.off_x += x - screen.last_x;
			screen.off_y += y - screen.last_y;
		    }
		    screen.last_x = x;
		    screen.last_y = y;
		    
		    update_rect.x = 0, update_rect.y = 0;
		    update_rect.width  = widget->allocation.width;
		    update_rect.height = widget->allocation.height;

		    /*
		     * Calls expose_event
		     */
		 gdk_window_invalidate_rect (widget->window, &update_rect, FALSE);

		    break;
		}
		case ZOOM_OUTLINE: {
		    if (screen.last_x || screen.last_y)
			draw_zoom_outline(screen.centered_outline_zoom);

		    screen.last_x = x;
		    screen.last_y = y;

		    draw_zoom_outline(screen.centered_outline_zoom);
		    break;
		}
		case MEASURE: {
		    if (screen.last_x || screen.last_y)
			draw_measure_distance();

		    screen.last_x = x;
		    screen.last_y = y;

		    draw_measure_distance();
		    break;
		}
		default:
		    break;
		}
	//}
	callbacks_update_ruler_pointers ();
	return TRUE;
} /* motion_notify_event */

gboolean
callbacks_drawingarea_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	gerbv_zoom_data_t data;
	gboolean do_zoom = FALSE;

	switch (event->button) {
		case 1 :
			if((event->state & GDK_SHIFT_MASK) == 0) {
			    /* Plain panning */
			    screen.state = MOVE;
			    screen.last_x = widget->allocation.height - event->x;
			    screen.last_y = widget->allocation.width  - event->y;
			} else {
			    GdkCursor *cursor;
			    
			    screen.state = MEASURE;
			    screen.start_x = event->x;
			    screen.start_y = event->y;
			    
			    cursor = gdk_cursor_new(GDK_CROSSHAIR);
			    gdk_window_set_cursor(gtk_widget_get_parent_window(widget),
						  cursor);
			    gdk_cursor_destroy(cursor);
			    
			}
			break;
		case 2 :
			/* And now, some Veribest-like mouse commands for
			   all us who dislike scroll wheels ;) */
			do_zoom = TRUE;
			if((event->state & GDK_SHIFT_MASK) != 0) {
			    /* Middle button + shift == zoom in */
			    data.z_dir = ZOOM_IN_CMOUSE;
			} else {
			    /* Only middle button == zoom out */
			    data.z_dir = ZOOM_OUT_CMOUSE;
			}
			break;
		case 3 :
			/* Zoom outline mode initiated */
			screen.state = ZOOM_OUTLINE;
			screen.start_x = event->x;
			screen.start_y = event->y;
			screen.centered_outline_zoom = event->state & GDK_SHIFT_MASK;
			break;
		case 4 : /* Scroll wheel */
			data.z_dir = ZOOM_IN_CMOUSE;
			do_zoom = TRUE;
			break;
		case 5 :  /* Scroll wheel */
			data.z_dir = ZOOM_OUT_CMOUSE;
			do_zoom = TRUE;
			break;
		default:
			break;
	}
    
	if (do_zoom) {
		data.z_event = event;
		zoom(widget, &data);
	}

	return TRUE;
} /* button_press_event */


gboolean
callbacks_drawingarea_button_release_event (GtkWidget *widget, GdkEventButton *event)
{ 
	if (event->type == GDK_BUTTON_RELEASE) {
		if (screen.state == MOVE) {
		    screen.state = NORMAL;
		    /* Redraw the image(s) */
		    screen.off_x = 0;
		    screen.off_y = 0;
		    redraw_pixmap(screen.drawing_area, TRUE);
		}
		else if (screen.state == ZOOM_OUTLINE) {
		    screen.state = NORMAL;
		    zoom_outline(widget, event);
		}
		else if (!(event->state & GDK_SHIFT_MASK)) {
		    gdk_window_set_cursor(gtk_widget_get_parent_window(widget),
					  GERBV_DEF_CURSOR);
		}
		screen.last_x = screen.last_y = 0;
		screen.state = NORMAL;
	}

	return TRUE;
} /* button_release_event */


gboolean
callbacks_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	GdkCursor *cursor;
	gerbv_zoom_data_t z_data;

	switch (screen.state) {
		case NORMAL:
			switch(event->keyval) {
				case GDK_Shift_L:
				case GDK_Shift_R:
				    cursor = gdk_cursor_new(GDK_CROSSHAIR);
				    gdk_window_set_cursor(gtk_widget_get_parent_window(screen.drawing_area),
							  cursor);
				    gdk_cursor_destroy(cursor);
				    break;
				case GDK_Alt_L:
				case GDK_Alt_R: 
				    screen.state = ALT_PRESSED;
				    screen.selected_layer = -1;
				    break;
				case GDK_f:
				case GDK_F:
				    autoscale();
				    redraw_pixmap(screen.drawing_area, TRUE);
				    break;
				case GDK_z:
				    z_data.z_dir = ZOOM_IN;
				    zoom(widget, &z_data);
				    break;
				case GDK_Z:
				    z_data.z_dir = ZOOM_OUT;
				    zoom(widget, &z_data);
				    break;
				default:
				    break;
			}
			break;
		case ALT_PRESSED:
			if ((event->keyval >= GDK_KP_0) &&
			    (event->keyval <= GDK_KP_9)) {
			    if (screen.selected_layer == -1) 
				screen.selected_layer = event->keyval - GDK_KP_0;
			    else
				screen.selected_layer = 10 * screen.selected_layer + 
				    (event->keyval - GDK_KP_0);
			}
			break;
		default:
			break;
	}
	    
	/* Escape may be used to abort outline zoom and just plain repaint */
	if (event->keyval == GDK_Escape) {
		GdkRectangle update_rect;

		screen.state = NORMAL;

		screen.statusbar.diststr[0] = '\0';
		callbacks_update_statusbar();

		update_rect.x = 0, update_rect.y = 0;
		update_rect.width =	widget->allocation.width;
		update_rect.height = widget->allocation.height;

		/*
		 * Calls expose_event
		 */
		gdk_window_invalidate_rect (widget->window, &update_rect, FALSE);
	}

	return TRUE;
} /* key_press_event */

gboolean
callbacks_window_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (screen.state) {
		case NORMAL:
			if((event->keyval == GDK_Shift_L) ||
			   (event->keyval == GDK_Shift_R)) {
			    gdk_window_set_cursor(gtk_widget_get_parent_window(screen.drawing_area),
						  GERBV_DEF_CURSOR);
			}
			break;
		default:
			break;
	}

	return TRUE;
} /* key_release_event */

/* Scroll wheel */
gboolean
callbacks_window_scroll_event(GtkWidget *widget, GdkEventScroll *event)
{
	gerbv_zoom_data_t data;

	switch (event->direction) {
		case GDK_SCROLL_UP:
			data.z_dir = ZOOM_IN_CMOUSE;
			break;
		case GDK_SCROLL_DOWN:
			data.z_dir = ZOOM_OUT_CMOUSE;
			break;
		case GDK_SCROLL_LEFT: 
			/* Ignore */
			return TRUE;
		case GDK_SCROLL_RIGHT:
			/* Ignore */
			return TRUE;
		default:
			return TRUE;
	}

	/* XXX Ugly hack */
	data.z_event = (GdkEventButton *)event;
	zoom(widget, &data);

	return TRUE;
} /* scroll_event */


/* ------------------------------------------------------------------ */
/** Displays additional information in the statusbar.
    The Statusbar is divided into three sections:\n
    statusbar.coordstr for coords\n
    statusbar.diststr for displaying measured distances or the designator 
    (right click on a graphically marked and also actively selected part)\n
    statusbar.msg for e.g. showing progress of actions*/
void 
callbacks_update_statusbar(void)
{
	if (screen.statusbar.coordstr != NULL) {
		gtk_label_set_text(GTK_LABEL(screen.win.statusMessageLeft), screen.statusbar.coordstr);
	}
	if (screen.statusbar.diststr != NULL) {
		gtk_label_set_text(GTK_LABEL(screen.win.statusMessageRight), screen.statusbar.diststr);
	}
}

void
callbacks_statusbar_unit_combo_box_changed (GtkComboBox *widget, gpointer user_data) {
	int activeRow = gtk_combo_box_get_active (widget);
	
	if (activeRow >= 0) {
		screen.unit = activeRow;	
	}
}

void
callbacks_clear_messages_button_clicked  (GtkButton *button, gpointer   user_data) {
	GtkTextBuffer *textbuffer;
	GtkTextIter start, end;

	textbuffer = gtk_text_view_get_buffer((GtkTextView*)screen.win.messageTextView);
	gtk_text_buffer_get_start_iter(textbuffer, &start);
	gtk_text_buffer_get_end_iter(textbuffer, &end);
	gtk_text_buffer_delete (textbuffer, &start, &end);
}
                                                        
void
callbacks_handle_log_messages(const gchar *log_domain, GLogLevelFlags log_level,
		      const gchar *message, gpointer user_data) {
	GtkTextBuffer *textbuffer = NULL;
	GtkTextIter iter;
	GtkTextTag *tag;
	GtkTextMark *StartMark = NULL, *StopMark = NULL;
	GtkTextIter StartIter, StopIter;

	if (!screen.win.messageTextView)
		return;
		
	textbuffer = gtk_text_view_get_buffer((GtkTextView*)screen.win.messageTextView);

	/* create a mark for the end of the text. */
	gtk_text_buffer_get_end_iter(textbuffer, &iter);

	/* get the current end position of the text (it will be the
	      start of the new text. */
	StartMark = gtk_text_buffer_create_mark(textbuffer,
					    "NewTextStart", &iter, TRUE);

	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                              "blue_foreground");
	/* the tag does not exist: create it and let them exist in the tag table.*/
	if (tag == NULL)    {
		tag = gtk_text_buffer_create_tag(textbuffer, "black_foreground",
		                              "foreground", "black", NULL);
		tag = gtk_text_buffer_create_tag(textbuffer, "blue_foreground",
		                              "foreground", "blue", NULL);
		tag = gtk_text_buffer_create_tag(textbuffer, "red_foreground",
		                              "foreground", "red", NULL);
		tag = gtk_text_buffer_create_tag(textbuffer, "darkred_foreground",
		                              "foreground", "darkred", NULL);
		tag = gtk_text_buffer_create_tag(textbuffer, "darkblue_foreground",
		                              "foreground", "darkblue", NULL);
		tag = gtk_text_buffer_create_tag (textbuffer, "darkgreen_foreground",
		                              "foreground", "darkgreen", NULL);
		tag = gtk_text_buffer_create_tag (textbuffer,
		                              "saddlebrown_foreground",
		                              "foreground", "saddlebrown", NULL);
	}

	/* 
	* See rgb.txt for the color names definition 
	* (on my PC it is on /usr/X11R6/lib/X11/rgb.txt)
	*/
	switch (log_level & G_LOG_LEVEL_MASK) {
	case G_LOG_LEVEL_ERROR:
	/* a message of this kind aborts the application calling abort() */
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "red_foreground");

	break;
	case G_LOG_LEVEL_CRITICAL:
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "red_foreground");
	break;
	case G_LOG_LEVEL_WARNING:
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "darkred_foreground");
	break;
	case G_LOG_LEVEL_MESSAGE:
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "darkblue_foreground");
	break;
	case G_LOG_LEVEL_INFO:
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "drakgreen_foreground");
	break;
	case G_LOG_LEVEL_DEBUG:
	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
					"saddlebrown_foreground");
	break;
	default:
	      tag = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table(textbuffer),
	                                    "black_foreground");
	break;
	}

	/*
	* Fatal aborts application. We will try to get the message out anyhow.
	*/
	if (log_level & G_LOG_FLAG_FATAL)
		fprintf(stderr, "Fatal error : %s\n", message);

	gtk_text_buffer_insert(textbuffer, &iter, message, -1);

	gtk_text_buffer_get_end_iter(textbuffer, &iter);

	StopMark = gtk_text_buffer_create_mark(textbuffer,
					   "NewTextStop", &iter, TRUE);

	gtk_text_buffer_get_iter_at_mark(textbuffer, &StartIter, StartMark);
	gtk_text_buffer_get_iter_at_mark(textbuffer, &StopIter, StopMark);

	gtk_text_buffer_apply_tag(textbuffer, tag, &StartIter, &StopIter);
}

