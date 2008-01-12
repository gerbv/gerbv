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
#ifndef WIN32
#include <gdk/gdkx.h>
#endif
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
//#include "gerb_aperture.h"

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #include "draw.h"
  #ifdef WIN32
    #include <cairo-win32.h>
  #else
    #include <cairo-xlib.h>
  #endif
#endif

#include "color.h"
#include "gerbv_screen.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "callbacks.h"
#include "interface.h"

#include "render.h"
#include "exportimage.h"

#define dprintf if(DEBUG) printf

#define SAVE_PROJECT 0
#define SAVE_AS_PROJECT 1
#define OPEN_PROJECT 2
#  define _(String) (String)

/**Global variable to keep track of what's happening on the screen.
   Declared extern in gerbv_screen.h
 */
extern gerbv_screen_t screen;
extern gerbv_render_info_t screenRenderInfo;

void load_project(project_list_t *project_list);
int open_image(char *filename, int idx, int reload);
void gerbv_open_layer_from_filename (gchar *filename);
void gerbv_open_project_from_filename (gchar *filename);
void gerbv_save_as_project_from_filename (gchar *filename);
void gerbv_save_project_from_filename (gchar *filename);
void gerbv_revert_all_files (void);
void gerbv_unload_all_layers (void);
void gerbv_unload_layer (int index);
void gerbv_change_layer_order (gint oldPosition, gint newPosition);

GtkWidget *
callbacks_generate_alert_dialog (gchar *primaryText, gchar *secondaryText){
	GtkWidget *dialog, *label;

	dialog = gtk_dialog_new_with_buttons (primaryText,
	                                    (GtkWindow *)screen.win.topLevelWindow,
	                                    GTK_DIALOG_DESTROY_WITH_PARENT,
	                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	                                    NULL);
	label = gtk_label_new (secondaryText);
	/* Add the label, and show everything we've added to the dialog. */
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
	                  label);
	gtk_widget_show_all (dialog);
	return dialog;
}

void
callbacks_new_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (screen.last_loaded >= 0) {
		if (!interface_get_alert_dialog_response ("Starting a new project will cause all currently open layers to be closed",
			"Do you want to proceed?"))
			return;
	}
	/* Unload all layers and then clear layer window */
	gerbv_unload_all_layers ();
	callbacks_update_layer_tree ();

	/* Destroy project info */
	if (screen.project) {
	    g_free(screen.project);
	    screen.project = NULL;
	}
	render_refresh_rendered_image_on_screen();
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
	gtk_file_chooser_set_current_folder ((GtkFileChooser *) screen.win.gerber,
		screen.path);
	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filename =
		    gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (screen.win.gerber));
		/* update the last folder */
		g_free (screen.path);
		screen.path = gtk_file_chooser_get_current_folder ((GtkFileChooser *) screen.win.gerber);
	}
	gtk_widget_destroy (screen.win.gerber);

	if (screen.last_loaded >= 0) {
		if (!interface_get_alert_dialog_response ("Opening a project will cause all currently open layers to be closed",
			"Do you want to proceed?"))
			return;
	}

	if (filename)
		gerbv_open_project_from_filename (filename);
	render_zoom_to_fit_display (&screenRenderInfo);
	render_refresh_rendered_image_on_screen();
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
	gtk_file_chooser_dialog_new ("Open Gerber, drill, or pick & place file(s)...",
				     NULL,
				     GTK_FILE_CHOOSER_ACTION_OPEN,
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
				     NULL);

	g_object_set (screen.win.gerber, "select-multiple", TRUE, NULL);
	gtk_file_chooser_set_current_folder ((GtkFileChooser *) screen.win.gerber,
		screen.path);
	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filenames =
		    gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER (screen.win.gerber));
		/* update the last folder */
		g_free (screen.path);
		screen.path = gtk_file_chooser_get_current_folder ((GtkFileChooser *) screen.win.gerber);
	}
	gtk_widget_destroy (screen.win.gerber);

	/* Now try to open all gerbers specified */
	for (filename=filenames; filename; filename=filename->next) {
		gerbv_open_layer_from_filename (filename->data);
	}
	g_slist_free(filenames);
	
	render_zoom_to_fit_display (&screenRenderInfo);
	render_refresh_rendered_image_on_screen();
	callbacks_update_layer_tree();

	return;
}

/* --------------------------------------------------------- */
void
callbacks_revert_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gerbv_revert_all_files ();
	render_refresh_rendered_image_on_screen();
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
	render_refresh_rendered_image_on_screen();
	return;
}

/* --------------------------------------------------------- */
void
callbacks_generic_save_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar *filename=NULL;
	gint processType = GPOINTER_TO_INT (user_data);
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
	free (windowTitle);
	
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
			exportimage_export_to_postscript_file (&screenRenderInfo, filename);
		else if (processType == CALLBACKS_SAVE_FILE_PDF)
			exportimage_export_to_pdf_file (&screenRenderInfo, filename);
		else if (processType == CALLBACKS_SAVE_FILE_SVG)
			exportimage_export_to_svg_file (&screenRenderInfo, filename);
#ifdef EXPORT_PNG
		else if (processType == CALLBACKS_SAVE_FILE_PNG)
			exportimage_export_to_png_file (&screenRenderInfo, filename);
#endif
		render_refresh_rendered_image_on_screen();
	}
	return;
}

/* --------------------------------------------------------- */
#if GTK_CHECK_VERSION(2,10,0)

static void
callbacks_begin_print (GtkPrintOperation *operation, GtkPrintContext   *context,
		gpointer user_data) {
	gtk_print_operation_set_n_pages     (operation, 1);
}


static void
callbacks_print_render_page (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           gint               page_nr,
           gpointer           user_data)
{
#ifndef RENDER_USING_GDK
	GtkPrintSettings *pSettings = gtk_print_operation_get_print_settings (operation);
	gerbv_render_info_t renderInfo = {1.0, 0, 0, 2,
		(gint) gtk_print_context_get_width (context),
		(gint) gtk_print_context_get_height (context)};
	cairo_t *cr;
	int i;
	
	/* have to assume x and y resolutions are the same for now, since we
	   don't support differing scales in the gerb_render_info_t struct yet */
	gdouble res = gtk_print_context_get_dpi_x (context);
	gdouble scalePercentage = gtk_print_settings_get_scale (pSettings);
	renderInfo.scaleFactor = scalePercentage / 100 * res;

	render_translate_to_fit_display (&renderInfo);
	cr = gtk_print_context_get_cairo_context (context);
	for(i = 0; i < screen.max_files; i++) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			//cairo_push_group (cr);
			render_layer_to_cairo_target (cr, screen.file[i], &renderInfo);
			//cairo_pop_group_to_source (cr);
			//cairo_paint_with_alpha (cr, screen.file[i]->alpha);		
		}
	}
#endif
}

/* --------------------------------------------------------- */
void
callbacks_print_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	GtkPrintOperation *print;
	GtkPrintOperationResult res;

	print = gtk_print_operation_new ();

	g_signal_connect (print, "begin_print", G_CALLBACK (callbacks_begin_print), NULL);
	g_signal_connect (print, "draw_page", G_CALLBACK (callbacks_print_render_page), NULL);

	//GtkPrintSettings *pSettings = gtk_print_operation_get_print_settings (print);
	
	res = gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                              (GtkWindow *) screen.win.topLevelWindow , NULL);

	g_object_unref (print);
}
#endif

/* --------------------------------------------------------- */
void
callbacks_zoom_in_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	render_zoom_display (ZOOM_IN, 0, 0, 0);
}

/* --------------------------------------------------------- */
void
callbacks_zoom_out_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	render_zoom_display (ZOOM_OUT, 0, 0, 0);
}

/* --------------------------------------------------------- */
void
callbacks_fit_to_window_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	render_zoom_to_fit_display (&screenRenderInfo);
	render_refresh_rendered_image_on_screen();
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
    gchar *general_report_string;
    gchar *error_report_string;
    error_list_t *my_error_list;
    gchar *error_level = NULL;
    gchar *aperture_report_string;
    gerb_aperture_list_t *my_aperture_list;

    /* First get a report of stats & errors accumulated from all layers */
    stats_report = generate_gerber_analysis();

    /* General info report */
    general_report_string = g_strdup_printf("General information\n");
    general_report_string = g_strdup_printf("%sActive layer count = %d\n", 
					    general_report_string, stats_report->layer_count);

    /* Error report */
    if (stats_report->error_list->error_text == NULL) {
	error_report_string = g_strdup_printf("\n\nNo errors found in Gerber file(s)!\n"); 
    } else {
	error_report_string = g_strdup_printf("\n\nErrors found in Gerber file(s):\n"); 
	for(my_error_list = stats_report->error_list; 
	    my_error_list != NULL; 
	    my_error_list = my_error_list->next) {
	    switch(my_error_list->type) {
		case FATAL: /* We should never get this one since the 
			     * program should terminate first.... */
		    error_level = g_strdup_printf("FATAL: ");
		    break;
		case ERROR:
		    error_level = g_strdup_printf("ERROR: ");
		    break;
		case WARNING:
		    error_level = g_strdup_printf("WARNING: ");
		    break;
		case NOTE:
		    error_level = g_strdup_printf("NOTE: ");
		    break;
	    }
	    error_report_string = g_strdup_printf("%sLayer %d: %s %s", 
						  error_report_string,
						  my_error_list->layer,
						  error_level,
						  my_error_list->error_text);
	}
    }

    general_report_string = g_strdup_printf("%s%s", 
					    general_report_string,
					    error_report_string);

    /* Now compile stats related to reading G codes */
    G_report_string = g_strdup_printf("G code statistics   \n");
    G_report_string = g_strdup_printf("%sG0 = %-6d (%s)\n", 
				      G_report_string, stats_report->G0,
				      "Move");
    G_report_string = g_strdup_printf("%sG1 = %-6d (%s)\n", 
				      G_report_string, stats_report->G1,
				      "1X linear interpolation");
    G_report_string = g_strdup_printf("%sG2 = %-6d (%s)\n", 
				      G_report_string, stats_report->G2,
				      "CW interpolation");
    G_report_string = g_strdup_printf("%sG3 = %-6d (%s)\n", 
				      G_report_string, stats_report->G3,
				      "CCW interpolation");
    G_report_string = g_strdup_printf("%sG4 = %-6d (%s)\n", 
				      G_report_string, stats_report->G4,
				      "Ignore block");
    G_report_string = g_strdup_printf("%sG10 = %-6d (%s)\n", 
				      G_report_string, stats_report->G10,
				      "10X linear interpolation");
    G_report_string = g_strdup_printf("%sG11 = %-6d (%s)\n", 
				      G_report_string, stats_report->G11,
				      "0.1X linear interpolation");
    G_report_string = g_strdup_printf("%sG12 = %-6d (%s)\n", 
				      G_report_string, stats_report->G12,
				      "0.01X linear interpolation");
    G_report_string = g_strdup_printf("%sG36 = %-6d (%s)\n", 
				      G_report_string, stats_report->G36,
				      "Poly fill on");
    G_report_string = g_strdup_printf("%sG37 = %-6d (%s)\n", 
				      G_report_string, stats_report->G37,
				      "Poly fill off");
    G_report_string = g_strdup_printf("%sG54 = %-6d (%s)\n", 
				      G_report_string, stats_report->G54,
				      "Tool prepare");
    G_report_string = g_strdup_printf("%sG55 = %-6d (%s)\n", 
				      G_report_string, stats_report->G55,
				      "Flash prepare");
    G_report_string = g_strdup_printf("%sG70 = %-6d (%s)\n", 
				      G_report_string, stats_report->G70,
				      "Units = inches");
    G_report_string = g_strdup_printf("%sG71 = %-6d (%s)\n", 
				      G_report_string, stats_report->G71,
				      "Units = mm");
    G_report_string = g_strdup_printf("%sG74 = %-6d (%s)\n", 
				      G_report_string, stats_report->G74,
				      "Disable 360 circ. interpolation");
    G_report_string = g_strdup_printf("%sG75 = %-6d (%s)\n", 
				      G_report_string, stats_report->G75,
				      "Enable 360 circ. interpolation");
    G_report_string = g_strdup_printf("%sG90 = %-6d (%s)\n", 
				      G_report_string, stats_report->G90,
				      "Absolute units");
    G_report_string = g_strdup_printf("%sG91 = %-6d (%s)\n", 
				      G_report_string, stats_report->G91,
				      "Incremental units");
    G_report_string = g_strdup_printf("%sUnknown G codes = %d\n", 
				      G_report_string, stats_report->G_unknown);


    D_report_string = g_strdup_printf("D code statistics   \n");
    D_report_string = g_strdup_printf("%sD1 = %-6d (%s)\n", 
				      D_report_string, stats_report->D1,
				      "Exposure on");
    D_report_string = g_strdup_printf("%sD2 = %-6d (%s)\n", 
				      D_report_string, stats_report->D2,
				      "Exposure off");
    D_report_string = g_strdup_printf("%sD3 = %-6d (%s)\n", 
				      D_report_string, stats_report->D3,
				      "Flash aperture");
    /* Now add list of user-defined D codes (apertures) */
    if (stats_report->D_code_list->number != -1) {
      for(my_aperture_list = stats_report->D_code_list; 
	  my_aperture_list != NULL; 
	  my_aperture_list = my_aperture_list->next) {
	
	D_report_string = 
	  g_strdup_printf("%sD%d = %-6d (%s)\n",
			  D_report_string,
			  my_aperture_list->number,
			  my_aperture_list->count,
			  "User defined aperture"
			  );
      }
    }

    /* Insert stuff about user defined codes here */
    D_report_string = g_strdup_printf("%sUndefined D codes = %d\n", 
				      D_report_string, stats_report->D_unknown);
    D_report_string = g_strdup_printf("%sD code Errors = %d\n", 
				      D_report_string, stats_report->D_error);


    M_report_string = g_strdup_printf("M code statistics   \n");
    M_report_string = g_strdup_printf("%sM0 = %-6d (%s)\n", 
				      M_report_string, stats_report->M0,
				      "Program start");
    M_report_string = g_strdup_printf("%sM1 = %-6d (%s)\n", 
				      M_report_string, stats_report->M1,
				      "Program stop");
    M_report_string = g_strdup_printf("%sM2 = %-6d (%s)\n", 
				      M_report_string, stats_report->M2,
				      "Program end");
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

    /* Aperture report */
    dprintf("About to define ap_type\n");
    char *ap_type[] = {"CIRCLE", "RECTANGLE", "OVAL", "POLYGON", "MACRO"};

    dprintf("Done defining ap_type, now process it\n");
    if (stats_report->aperture_list->number == -1) {
	aperture_report_string = 
	    g_strdup_printf("\n\nNo aperture definitions found in Gerber file(s)!\n"); 
    } else {
	aperture_report_string = 
	    g_strdup_printf("\n\nApertures defined in Gerber file(s):\n"); 
	aperture_report_string = 
	    g_strdup_printf("%s %-6s %-8s %12s  %8s %8s %8s\n",
			    aperture_report_string,
			    "Layer",
			    "D code",
			    "Aperture",
			    "Param[0]",
			    "Param[1]",
			    "Param[2]"
		);
	for(my_aperture_list = stats_report->aperture_list; 
	    my_aperture_list != NULL; 
	    my_aperture_list = my_aperture_list->next) {

	    aperture_report_string = 
		g_strdup_printf("%s %-6d    D%-4d%13s  %8.3f %8.3f %8.3f\n", 
				aperture_report_string,
				my_aperture_list->layer,
				my_aperture_list->number,
				ap_type[my_aperture_list->type],
				my_aperture_list->parameter[0],
				my_aperture_list->parameter[1],
				my_aperture_list->parameter[2]
		    );
	}
    }

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

    /* Use fixed width font for all reports */
    PangoFontDescription *font = 
	pango_font_description_from_string ("monospace");

    /* Create GtkLabel to hold general report text */
    GtkWidget *general_report_label = gtk_label_new (general_report_string);
    gtk_misc_set_alignment(GTK_MISC(general_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(general_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(general_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(general_report_label),
			    font);
    g_free(general_report_string);

    /* Create GtkLabel to hold G code text */
    GtkWidget *G_report_label = gtk_label_new (G_report_string);
    gtk_misc_set_alignment(GTK_MISC(G_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(G_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(G_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(G_report_label),
			    font);
    g_free(G_report_string);

    /* Create GtkLabel to hold D code text */
    GtkWidget *D_report_label = gtk_label_new (D_report_string);
    gtk_misc_set_alignment(GTK_MISC(D_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(D_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(D_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(D_report_label),
			    font);
    g_free(D_report_string);

    /* Put D code report text into scrolled window */
    GtkWidget *D_code_report_window = gtk_scrolled_window_new (NULL, NULL);
    /* This throws a warning.  Must find different approach.... */
    gtk_widget_set_size_request(GTK_WIDGET(D_code_report_window),
				200,
				300);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(D_code_report_window),
					  GTK_WIDGET(D_report_label));

    /* Create GtkLabel to hold M code text */
    GtkWidget *M_report_label = gtk_label_new (M_report_string);
    gtk_misc_set_alignment(GTK_MISC(M_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(M_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(M_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(M_report_label),
			    font);
    g_free(M_report_string);

    /* Create GtkLabel to hold misc code text */
    GtkWidget *misc_report_label = gtk_label_new (misc_report_string);
    gtk_misc_set_alignment(GTK_MISC(misc_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(misc_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(misc_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(misc_report_label),
			    font);
    g_free(misc_report_string);

    /* Create GtkLabel to hold aperture defnintion text */
    GtkWidget *aperture_report_label = gtk_label_new (aperture_report_string);
    gtk_misc_set_alignment(GTK_MISC(aperture_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(aperture_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(aperture_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(aperture_report_label),
			    font);
    g_free(aperture_report_string);
    
    /* Put aperture definintion text into scrolled window */
    GtkWidget *aperture_report_window = gtk_scrolled_window_new (NULL, NULL);
    /* This throws a warning.  Must find different approach.... */
    gtk_widget_set_size_request(GTK_WIDGET(aperture_report_window),
				200,
				300);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(aperture_report_window),
					  GTK_WIDGET(aperture_report_label));

    /* Create tabbed notebook widget and add report label widgets. */
    GtkWidget *notebook = gtk_notebook_new();
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(general_report_label),
			     gtk_label_new("General"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(G_report_label),
			     gtk_label_new("G codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(D_code_report_window),
			     gtk_label_new("D codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(M_report_label),
			     gtk_label_new("M codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(misc_report_label),
			     gtk_label_new("Misc. codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(aperture_report_window),
			     gtk_label_new("Aperture definitions"));
    
    
    /* Now put notebook into dialog window and show the whole thing */
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(analyze_active_gerbers)->vbox),
		      GTK_WIDGET(notebook));
    
    gtk_widget_show_all(analyze_active_gerbers);
	
    return;

}

/* --------------------------------------------------------- */
void
callbacks_analyze_active_drill_activate(GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    drill_stats_t *stats_report;
    gchar *G_report_string;
    gchar *M_report_string;
    gchar *misc_report_string;
    drill_list_t *my_drill_list;
    gchar *drill_report_string;
    gchar *general_report_string;
    gchar *error_report_string;
    error_list_t *my_error_list;
    gchar *error_level = NULL;

    stats_report = (drill_stats_t *) generate_drill_analysis();
    
    /* General and error window strings */
    general_report_string = g_strdup_printf("General information\n");
    general_report_string = g_strdup_printf("%sActive layer count = %d\n", 
					    general_report_string, stats_report->layer_count);

    if (stats_report->error_list->error_text == NULL) {
	error_report_string = g_strdup_printf("\n\nNo errors found in drill file(s)!\n"); 
    } else {
	error_report_string = g_strdup_printf("\n\nErrors found in drill file(s):\n"); 
	for(my_error_list = stats_report->error_list; 
	    my_error_list != NULL; 
	    my_error_list = my_error_list->next) {
	    switch(my_error_list->type) {
		case FATAL: /* We should never get this one since the 
			     * program should terminate first.... */
		    error_level = g_strdup_printf("FATAL: ");
		    break;
		case ERROR:
		    error_level = g_strdup_printf("ERROR: ");
		    break;
		case WARNING:
		    error_level = g_strdup_printf("WARNING: ");
		    break;
		case NOTE:
		    error_level = g_strdup_printf("NOTE: ");
		    break;
	    }
	    error_report_string = g_strdup_printf("%sLayer %d: %s %s", 
						  error_report_string,
						  my_error_list->layer,
						  error_level,
						  my_error_list->error_text);
	}
    }

    general_report_string = g_strdup_printf("%s%s", 
					    general_report_string,
					    error_report_string);

    /* G code window strings */
    G_report_string = g_strdup_printf("G code statistics   \n");
    G_report_string = g_strdup_printf("%sG00 = %-6d (%s)\n", 
				      G_report_string, stats_report->G00,
				      "Rout mode");
    G_report_string = g_strdup_printf("%sG01 = %-6d (%s)\n", 
				      G_report_string, stats_report->G01,
				      "1X linear interpolation");
    G_report_string = g_strdup_printf("%sG02 = %-6d (%s)\n", 
				      G_report_string, stats_report->G02,
				      "CW interpolation");
    G_report_string = g_strdup_printf("%sG03 = %-6d (%s)\n", 
				      G_report_string, stats_report->G03,
				      "CCW interpolation");
    G_report_string = g_strdup_printf("%sG04 = %-6d (%s)\n", 
				      G_report_string, stats_report->G04,
				      "Variable dwell");
    G_report_string = g_strdup_printf("%sG05 = %-6d (%s)\n", 
				      G_report_string, stats_report->G05,
				      "Drill mode");
    G_report_string = g_strdup_printf("%sG90 = %-6d (%s)\n", 
				      G_report_string, stats_report->G90,
				      "Absolute units");
    G_report_string = g_strdup_printf("%sG91 = %-6d (%s)\n", 
				      G_report_string, stats_report->G91,
				      "Incremental units");
    G_report_string = g_strdup_printf("%sG93 = %-6d (%s)\n", 
				      G_report_string, stats_report->G93,
				      "Zero set");
    G_report_string = g_strdup_printf("%sUnknown G codes = %d\n", 
				      G_report_string, stats_report->G_unknown);

    /* M code window strings */
    M_report_string = g_strdup_printf("M code statistics   \n");
    M_report_string = g_strdup_printf("%sM00 = %-6d (%s)\n", 
				      M_report_string, stats_report->M00,
				      "End of program");
    M_report_string = g_strdup_printf("%sM01 = %-6d (%s)\n", 
				      M_report_string, stats_report->M01,
				      "End of pattern");
    M_report_string = g_strdup_printf("%sM18 = %-6d (%s)\n", 
				      M_report_string, stats_report->M18,
				      "Tool tip check");
    M_report_string = g_strdup_printf("%sM25 = %-6d (%s)\n", 
				      M_report_string, stats_report->M25,
				      "Begin pattern");
    M_report_string = g_strdup_printf("%sM30 = %-6d (%s)\n", 
				      M_report_string, stats_report->M30,
				      "End program rewind");
    M_report_string = g_strdup_printf("%sM31 = %-6d (%s)\n", 
				      M_report_string, stats_report->M31,
				      "Begin pattern");
    M_report_string = g_strdup_printf("%sM45 = %-6d (%s)\n", 
				      M_report_string, stats_report->M45,
				      "Long message");
    M_report_string = g_strdup_printf("%sM47 = %-6d (%s)\n", 
				      M_report_string, stats_report->M47,
				      "Operator message");
    M_report_string = g_strdup_printf("%sM48 = %-6d (%s)\n", 
				      M_report_string, stats_report->M48,
				      "Begin program header");
    M_report_string = g_strdup_printf("%sM71 = %-6d (%s)\n", 
				      M_report_string, stats_report->M71,
				      "Metric units");
    M_report_string = g_strdup_printf("%sM72 = %-6d (%s)\n", 
				      M_report_string, stats_report->M72,
				      "English units");
    M_report_string = g_strdup_printf("%sM95 = %-6d (%s)\n", 
				      M_report_string, stats_report->M95,
				      "End program header");
    M_report_string = g_strdup_printf("%sM97 = %-6d (%s)\n", 
				      M_report_string, stats_report->M97,
				      "Canned text");
    M_report_string = g_strdup_printf("%sM98 = %-6d (%s)\n", 
				      M_report_string, stats_report->M98,
				      "Canned text");
    M_report_string = g_strdup_printf("%sUnknown M codes = %d\n", 
				      M_report_string, stats_report->M_unknown);

    /* misc report strings */
    misc_report_string = g_strdup_printf("Misc code statistics   \n");
    misc_report_string = g_strdup_printf("%scomments = %d\n", 
					 misc_report_string, stats_report->comment);
    misc_report_string = g_strdup_printf("%sUnknown codes = %d\n", 
					 misc_report_string, stats_report->unknown);

    /* drill report window strings */
    drill_report_string = g_strdup_printf("%10s %8s %8s %8s\n", 
					  "Drill no.", "Dia.", "Units", "Count");
    for(my_drill_list = stats_report->drill_list; 
	my_drill_list != NULL; 
	my_drill_list = my_drill_list->next) {
	if (my_drill_list->drill_num == -1) break;  /* No dill list */
	drill_report_string = g_strdup_printf("%s%10d %8.3f %8s %8d\n", 
					      drill_report_string,
					      my_drill_list->drill_num,
					      my_drill_list->drill_size,
					      my_drill_list->drill_unit,
					      my_drill_list->drill_count);
    }

    /* Use fixed width font for all reports */
    PangoFontDescription *font = 
	pango_font_description_from_string ("monospace");

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

    /* Create GtkLabel to hold general report text */
    GtkWidget *general_report_label = gtk_label_new (general_report_string);
    gtk_misc_set_alignment(GTK_MISC(general_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(general_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(general_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(general_report_label),
			    font);
    g_free(general_report_string);

    /* Create GtkLabel to hold G code text */
    GtkWidget *G_report_label = gtk_label_new (G_report_string);
    gtk_misc_set_alignment(GTK_MISC(G_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(G_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(G_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(G_report_label),
			    font);
    g_free(G_report_string);

    /* Create GtkLabel to hold M code text */
    GtkWidget *M_report_label = gtk_label_new (M_report_string);
    gtk_misc_set_alignment(GTK_MISC(M_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(M_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(M_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(M_report_label),
			    font);
    g_free(M_report_string);

    /* Create GtkLabel to hold misc code text */
    GtkWidget *misc_report_label = gtk_label_new (misc_report_string);
    gtk_misc_set_alignment(GTK_MISC(misc_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(misc_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(misc_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(misc_report_label),
			    font);
    g_free(misc_report_string);

    /* Create GtkLabel to hold drills used text */
    GtkWidget *drill_report_label = gtk_label_new (drill_report_string);
    gtk_misc_set_alignment(GTK_MISC(drill_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(drill_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(drill_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(drill_report_label),
			    font);
    g_free(drill_report_string);

    /* Create tabbed notebook widget and add report label widgets. */
    GtkWidget *notebook = gtk_notebook_new();
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(general_report_label),
			     gtk_label_new("General"));

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(G_report_label),
			     gtk_label_new("G codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(M_report_label),
			     gtk_label_new("M codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(misc_report_label),
			     gtk_label_new("Misc. codes"));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(drill_report_label),
			     gtk_label_new("Drills used"));
    
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
	/* gchar *translators = _("translator-credits"); */

#ifdef RENDER_USING_GDK
#define RENDERER "gdk"
#else
#define RENDERER "cairo"
#endif
	gchar *string = g_strdup_printf ( "gerbv -- a Gerber (RS-274/X) viewer.\n\n"
					  "This is gerbv version %s\n"
					  "Compiled on %s at %s\n"
					  "Renderer (compile time setting):  %s\n"
					  "\n"
					  "gerbv is part of the gEDA Project.\n"
					  "\n"
					  "For more information see:\n"
					  "  gerbv homepage: http://gerbv.sf.net\n"
					  "  gEDA homepage: http://www.geda.seul.org\n"
					  "  gEDA Wiki: http://geda.seul.org/dokuwiki/doku.php?id=geda\n\n",
					  VERSION, __DATE__, __TIME__, RENDERER);

#undef RENDERER
#if GTK_CHECK_VERSION(2,6,0)
	gchar *license = g_strdup_printf("gerbv -- a Gerber (RS-274/X) viewer.\n\n"
					 "Copyright (C) 2000-2007 Stefan Petersen\n\n"
					 "This program is free software: you can redistribute it and/or modify\n"
					 "it under the terms of the GNU General Public License as published by\n"
					 "the Free Software Foundation, either version 2 of the License, or\n"
					 "(at your option) any later version.\n\n"
					 "This program is distributed in the hope that it will be useful,\n"
					 "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
					 "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
					 "GNU General Public License for more details.\n\n"
					 "You should have received a copy of the GNU General Public License\n"
					 "along with this program.  If not, see <http://www.gnu.org/licenses/>.");

	/* Note:  set_authors works strangely.... */
	const gchar *authors[15] = {"Project founder:  Stefan Petersen\n",
				    "Contributors:",
				    "Julian Lamb",
				    "Stuart Brorson",
				    "Dan McMahill",
				    "Joerg Wunsch",
				    "Andreas Andersson aka Pitch",
				    "Anders Eriksson",
				    "Juergen Haas",
				    "Tomasz Motylewski",
				    "Joost Witteveen",
				    "Trevor Blackwell",
				    "David Carr",
				    "... and many others.",
				    NULL};

	aboutdialog1 = gtk_about_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (aboutdialog1), 5);
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (aboutdialog1), VERSION);
	gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (aboutdialog1), _("Gerbv"));

	/* gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (aboutdialog1), translators); */
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (aboutdialog1), string);
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG (aboutdialog1), license);
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG (aboutdialog1), authors);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG (aboutdialog1), NULL);

	g_signal_connect (G_OBJECT(aboutdialog1),"response",
		      G_CALLBACK (gtk_widget_destroy), GTK_WIDGET(aboutdialog1));

	g_free (string);
	g_free (license);
#else
	aboutdialog1 = gtk_message_dialog_new (	GTK_WINDOW (screen.win.topLevelWindow),
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_INFO,
					       GTK_BUTTONS_CLOSE,
					       string
					       );

	gtk_window_set_title ( GTK_WINDOW (aboutdialog1), _("About Gerber Viewer"));

	/* Destroy the dialog when the user responds to it (e.g. clicks a button) */
	g_signal_connect_swapped (aboutdialog1, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  aboutdialog1);
	g_free (string);
#endif

	gtk_widget_show_all(GTK_WIDGET(aboutdialog1));

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

	xPosition = screenRenderInfo.lowerLeftX + (screen.last_x / screenRenderInfo.scaleFactor);
	yPosition = screenRenderInfo.lowerLeftY + ((screenRenderInfo.displayHeight - screen.last_y) / screenRenderInfo.scaleFactor);

	if (!((screen.unit == GERBV_MILS) && (screenRenderInfo.scaleFactor < 80))) {
		xPosition = callbacks_calculate_actual_distance (xPosition);
		yPosition = callbacks_calculate_actual_distance (yPosition);
	}
	g_object_set (G_OBJECT (screen.win.hRuler), "position", xPosition, NULL);
	g_object_set (G_OBJECT (screen.win.vRuler), "position", yPosition, NULL);
}

void callbacks_update_ruler_scales (void) {
	double xStart, xEnd, yStart, yEnd;

	xStart = screenRenderInfo.lowerLeftX;
	xEnd = screenRenderInfo.lowerLeftX + (screenRenderInfo.displayWidth / screenRenderInfo.scaleFactor);
	yStart = screenRenderInfo.lowerLeftY;
	yEnd = screenRenderInfo.lowerLeftY + (screenRenderInfo.displayHeight / screenRenderInfo.scaleFactor);

	/* mils can get super crowded with large boards, but inches are too
	   large for most boards.  So, we leave mils in for now and just switch
	   to inches if the scale factor gets too small */
	if (!((screen.unit == GERBV_MILS) && (screenRenderInfo.scaleFactor < 80))) {
		xStart = callbacks_calculate_actual_distance (xStart);
		xEnd = callbacks_calculate_actual_distance (xEnd);
		yStart = callbacks_calculate_actual_distance (yStart);
		yEnd = callbacks_calculate_actual_distance (yEnd);
	}
	/* make sure the widgets actually exist before setting (in case this gets
	   called before everything is realized */
	if (screen.win.hRuler)
		gtk_ruler_set_range (GTK_RULER (screen.win.hRuler), xStart, xEnd, 0, xEnd - xStart);
	/* reverse y min and max, since the ruler starts at the top */
	if (screen.win.vRuler)
		gtk_ruler_set_range (GTK_RULER (screen.win.vRuler), yEnd, yStart, 0, yEnd - yStart);
}

void callbacks_update_scrollbar_limits (void){
	gerbv_render_info_t tempRenderInfo = {1.0, 0, 0, 0, screenRenderInfo.displayWidth,
			screenRenderInfo.displayHeight};
	GtkAdjustment *hAdjust = (GtkAdjustment *)screen.win.hAdjustment;
	GtkAdjustment *vAdjust = (GtkAdjustment *)screen.win.vAdjustment;
	
	render_zoom_to_fit_display (&tempRenderInfo);
	hAdjust->lower = tempRenderInfo.lowerLeftX;
	hAdjust->upper = tempRenderInfo.lowerLeftX + (tempRenderInfo.displayWidth / tempRenderInfo.scaleFactor);
	hAdjust->page_size = screenRenderInfo.displayWidth / screenRenderInfo.scaleFactor;
	hAdjust->page_increment = hAdjust->page_size;
	hAdjust->step_increment = hAdjust->page_size / 10.0;
	
	vAdjust->lower = tempRenderInfo.lowerLeftY;
	vAdjust->upper = tempRenderInfo.lowerLeftY + (tempRenderInfo.displayHeight / tempRenderInfo.scaleFactor);
	vAdjust->page_size = screenRenderInfo.displayHeight / screenRenderInfo.scaleFactor;
	vAdjust->page_increment = vAdjust->page_size;
	vAdjust->step_increment = vAdjust->page_size / 10.0;
	
	callbacks_update_scrollbar_positions ();
}

void callbacks_update_scrollbar_positions (void){
	gdouble positionX,positionY;
	
	positionX = screenRenderInfo.lowerLeftX;
	if (positionX < ((GtkAdjustment *)screen.win.hAdjustment)->lower)
		positionX = ((GtkAdjustment *)screen.win.hAdjustment)->lower;
	if (positionX > (((GtkAdjustment *)screen.win.hAdjustment)->upper - ((GtkAdjustment *)screen.win.hAdjustment)->page_size))
		positionX = (((GtkAdjustment *)screen.win.hAdjustment)->upper - ((GtkAdjustment *)screen.win.hAdjustment)->page_size);
	
	gtk_adjustment_set_value ((GtkAdjustment *)screen.win.hAdjustment, positionX);
	
	positionY = ((GtkAdjustment *)screen.win.vAdjustment)->upper - (screenRenderInfo.lowerLeftY + (screenRenderInfo.displayHeight / screenRenderInfo.scaleFactor));
	if (positionY < ((GtkAdjustment *)screen.win.vAdjustment)->lower)
		positionY = ((GtkAdjustment *)screen.win.vAdjustment)->lower;
	if (positionY > (((GtkAdjustment *)screen.win.vAdjustment)->upper - ((GtkAdjustment *)screen.win.vAdjustment)->page_size))
		positionY = (((GtkAdjustment *)screen.win.vAdjustment)->upper - ((GtkAdjustment *)screen.win.vAdjustment)->page_size);
	gtk_adjustment_set_value ((GtkAdjustment *)screen.win.vAdjustment, positionY);

}

gboolean
callbacks_scrollbar_button_released (GtkWidget *widget, GdkEventButton *event){
	screen.off_x = 0;
	screen.off_y = 0;
	screen.state = NORMAL;
	render_refresh_rendered_image_on_screen();
	return FALSE;
}

gboolean
callbacks_scrollbar_button_pressed (GtkWidget *widget, GdkEventButton *event){
	//screen.last_x = ((GtkAdjustment *)screen.win.hAdjustment)->value;
	screen.state = SCROLLBAR;
	return FALSE;
}


void callbacks_hadjustment_value_changed (GtkAdjustment *adjustment, gpointer user_data){
	/* make sure we're actually using the scrollbar to make sure we don't reset
	   lowerLeftX during a scrollbar redraw during something else */
	if (screen.state == SCROLLBAR) {
		screenRenderInfo.lowerLeftX = gtk_adjustment_get_value (adjustment);
	}
}

void callbacks_vadjustment_value_changed (GtkAdjustment *adjustment, gpointer user_data){
	/* make sure we're actually using the scrollbar to make sure we don't reset
	   lowerLeftY during a scrollbar redraw during something else */
	if (screen.state == SCROLLBAR) {
		screenRenderInfo.lowerLeftY = adjustment->upper - gtk_adjustment_get_value (adjustment) - (screenRenderInfo.displayHeight / screenRenderInfo.scaleFactor);		
	}
}

/* --------------------------------------------------------- */
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
		if (screenRenderInfo.renderType < 2) {
			render_refresh_rendered_image_on_screen();
		}
#ifndef RENDER_USING_GDK
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
#endif 
	}
}

/* --------------------------------------------------------- */
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

/* --------------------------------------------------------- */
void
callbacks_add_layer_button_clicked  (GtkButton *button, gpointer   user_data) {
	callbacks_open_layer_activate (NULL, NULL);
}

/* --------------------------------------------------------- */
void
callbacks_unselect_all_tool_buttons (void) {

}

void
callbacks_switch_to_normal_tool_cursor (gint toolNumber) {
	GdkCursor *cursor;

	switch (toolNumber) {
		case POINTER:
			gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
						  GERBV_DEF_CURSOR);
			break;
		case PAN:
			cursor = gdk_cursor_new(GDK_FLEUR);
			gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
					  cursor);
			gdk_cursor_destroy(cursor);
			break;
		case ZOOM:
			cursor = gdk_cursor_new(GDK_SIZING);
			gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
					      cursor);
			gdk_cursor_destroy(cursor);
			break;
		case MEASURE:
			cursor = gdk_cursor_new(GDK_CROSSHAIR);
			gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
					  cursor);
			gdk_cursor_destroy(cursor);
			break;
		default:
			break;
	}
}

void
callbacks_switch_to_correct_cursor (void) {
	GdkCursor *cursor;

	if (screen.state == IN_MOVE) {
		cursor = gdk_cursor_new(GDK_FLEUR);
		gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
				  cursor);
		gdk_cursor_destroy(cursor);
		return;
	}
	else if (screen.state == IN_ZOOM_OUTLINE) {
		cursor = gdk_cursor_new(GDK_SIZING);
		gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
				  cursor);
		gdk_cursor_destroy(cursor);
		return;
	}
	callbacks_switch_to_normal_tool_cursor (screen.tool);
}

/* --------------------------------------------------------- */
void
callbacks_change_tool (GtkButton *button, gpointer   user_data) {
	gint toolNumber = GPOINTER_TO_INT (user_data);
	
	/* make sure se don't get caught in endless recursion here */
	if (screen.win.updatingTools)
		return;
	screen.win.updatingTools = TRUE;
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonPointer), FALSE);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonPan), FALSE);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonZoom), FALSE);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonMeasure), FALSE);
	switch (toolNumber) {
		case POINTER:
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonPointer), TRUE);
			screen.tool = POINTER;
			screen.state = NORMAL;
			snprintf(screen.statusbar.diststr, MAX_DISTLEN, 
				 "Click to select. Right click and drag to zoom.");
			break;
		case PAN:
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonPan), TRUE);
			screen.tool = PAN;
			screen.state = NORMAL;
			snprintf(screen.statusbar.diststr, MAX_DISTLEN, 
				 "Click and drag to pan. Right click and drag to zoom.");
			break;
		case ZOOM:
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonZoom), TRUE);
			screen.tool = ZOOM;
			screen.state = NORMAL;
			snprintf(screen.statusbar.diststr, MAX_DISTLEN, 
				 "Click and drag to zoom in. Shift+click to zoom out.");
			break;
		case MEASURE:
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonMeasure), TRUE);
			screen.tool = MEASURE;
			screen.state = NORMAL;
			snprintf(screen.statusbar.diststr, MAX_DISTLEN, "Click and drag to measure a distance.");
			break;
		default:
			break;
	}
	callbacks_switch_to_normal_tool_cursor (toolNumber);
	callbacks_update_statusbar();
	screen.win.updatingTools = FALSE;
	callbacks_force_expose_event_for_screen();
}

/* --------------------------------------------------------- */
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

/* --------------------------------------------------------- */
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

/* --------------------------------------------------------- */
void
callbacks_remove_layer_button_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	
	if ((index >= 0) && (index <= screen.last_loaded)) {
		gerbv_unload_layer (index);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (0);

		if (screenRenderInfo.renderType < 2) {
			render_refresh_rendered_image_on_screen();
		}
#ifndef RENDER_USING_GDK
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
#endif 
	}
}

/* --------------------------------------------------------- */
void
callbacks_move_layer_down_button_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	
	if ((index >= 0) && (index < screen.last_loaded)) {
		gerbv_change_layer_order (index, index + 1);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (index + 1);
		if (screenRenderInfo.renderType < 2) {
			render_refresh_rendered_image_on_screen();
		}
#ifndef RENDER_USING_GDK
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
#endif 
	}
}

/* --------------------------------------------------------- */
void
callbacks_move_layer_up_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	
	if (index > 0) {
		gerbv_change_layer_order (index, index - 1);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (index - 1);
		if (screenRenderInfo.renderType < 2) {
			render_refresh_rendered_image_on_screen();
		}
#ifndef RENDER_USING_GDK
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
#endif 
	}
}

/* --------------------------------------------------------- */
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

			if (screenRenderInfo.renderType < 2) {
				render_refresh_rendered_image_on_screen();
			}
#ifndef RENDER_USING_GDK
			else {
				render_recreate_composite_surface (screen.drawing_area);
				callbacks_force_expose_event_for_screen ();
			}
#endif 	
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
callbacks_show_color_picker_dialog (gint index){
	screen.win.colorSelectionDialog = NULL;
	GtkColorSelectionDialog *cs= (GtkColorSelectionDialog *) gtk_color_selection_dialog_new ("Select a color");
	GtkColorSelection *colorsel = (GtkColorSelection *) cs->colorsel;
	
	screen.win.colorSelectionDialog = (GtkWidget *) cs;
	screen.win.colorSelectionIndex = index;
	gtk_color_selection_set_current_color (colorsel, &screen.file[index]->color);
#ifndef RENDER_USING_GDK
	if (screenRenderInfo.renderType >= 2) {
		gtk_color_selection_set_has_opacity_control (colorsel, TRUE);
		gtk_color_selection_set_current_alpha (colorsel, screen.file[index]->alpha);
	}
#endif
	gtk_widget_show_all((GtkWidget *)cs);
	if (gtk_dialog_run ((GtkDialog*)cs) == GTK_RESPONSE_OK) {
		GtkColorSelection *colorsel = (GtkColorSelection *) cs->colorsel;
		gint rowIndex = screen.win.colorSelectionIndex;
		
		gtk_color_selection_get_current_color (colorsel, &screen.file[rowIndex]->color);
		if (screenRenderInfo.renderType >= 2) {
			screen.file[rowIndex]->alpha = gtk_color_selection_get_current_alpha (colorsel);
		}
		gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.file[rowIndex]->color, FALSE, TRUE);
		callbacks_update_layer_tree ();
		render_refresh_rendered_image_on_screen();
	}
	gtk_widget_destroy ((GtkWidget *)cs);
	screen.win.colorSelectionDialog = NULL;
}

void
callbacks_invert_layer_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	
	screen.file[index]->inverted = !screen.file[index]->inverted;
	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree ();
}

void
callbacks_change_layer_color_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();

	callbacks_show_color_picker_dialog (index);
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
      if (event->button == 1) {
	      if (gtk_tree_view_get_path_at_pos  ((GtkTreeView *) widget, event->x, event->y,
	      	&path, &column, &x, &y)) {
		      if (gtk_tree_model_get_iter((GtkTreeModel *)list_store, &iter, path)) {
		      	gint *indeces;
		      	indeces = gtk_tree_path_get_indices (path);
		      	if (indeces) {
					columnIndex = callbacks_get_col_number_from_tree_view_column (column);
					if ((columnIndex == 1) && (indeces[0] <= screen.last_loaded)){
						callbacks_show_color_picker_dialog (indeces[0]);
						/* don't propagate the signal, since drag and drop can
					   	sometimes activated during color selection */
						return TRUE;
					}
				}
			}
		}
	}
	/* don't pop up the menu if we don't have any loaded files */
	else if ((event->button == 3)&&(screen.last_loaded >= 0)) {
		gtk_menu_popup(GTK_MENU(screen.win.layerTreePopupMenu), NULL, NULL, NULL, NULL, 
			   event->button, event->time);
	}
	/* always allow the click to propagate and make sure the line is activated */
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

		for (idx = 0; idx < screen.max_files; idx++) {
			if (screen.file[idx]) {
				GdkPixbuf    *pixbuf;
				guint32 color;
				
				unsigned char red, green, blue, alpha;
				
				red = (unsigned char) (screen.file[idx]->color.red * 255 / G_MAXUINT16) ;
				green = (unsigned char) (screen.file[idx]->color.green * 255 / G_MAXUINT16) ;
				blue = (unsigned char) (screen.file[idx]->color.blue *255 / G_MAXUINT16) ;
				alpha = (unsigned char) (screen.file[idx]->alpha * 255 / G_MAXUINT16) ;
				
				color = (red )* (256*256*256) + (green ) * (256*256) + (blue )* (256) + (alpha );
				pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 20, 15);
				gdk_pixbuf_fill (pixbuf, color);

				gtk_list_store_append (list_store, &iter);
				
				gchar *modifiedCode;
				if (screen.file[idx]->inverted) {
					modifiedCode = g_strdup ("I");
				}
				else
					modifiedCode = g_strdup ("");
				gtk_list_store_set (list_store, &iter,
							0, screen.file[idx]->isVisible,
							1, pixbuf,
			                        2, screen.file[idx]->name,
			                        3, modifiedCode,
			                        -1);
			      g_free (modifiedCode);
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
	GdkDrawable *drawable = widget->window;
	
	gdk_drawable_get_size (drawable, &screenRenderInfo.displayWidth, &screenRenderInfo.displayHeight);

#ifndef RENDER_USING_GDK
	/* set this up if cairo is compiled, since we may need to switch over to
	   using the surface at any time */
	int x_off=0, y_off=0;
	GdkVisual *visual;
	
	if (GDK_IS_WINDOW(widget->window)) {
	      /* query the window's backbuffer if it has one */
		GdkWindow *window = GDK_WINDOW(widget->window);
	      gdk_window_get_internal_paint_info (window, &drawable, &x_off, &y_off);
	}
	visual = gdk_drawable_get_visual (drawable);
	if (!screen.windowSurface) {
#ifdef WIN32
		/* FIXME */
		screen.windowSurface = (gpointer) cairo_win32_surface_create (0);
#else
		screen.windowSurface = (gpointer) cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable),
	                                          GDK_DRAWABLE_XID (drawable),
	                                          GDK_VISUAL_XVISUAL (visual),
	                                          screenRenderInfo.displayWidth,
	                                          screenRenderInfo.displayHeight);
#endif

	}
#endif
	/* if this is the first time, go ahead and call autoscale even if we don't
	   have a model loaded */
	if (screenRenderInfo.scaleFactor < 0.001) {
		render_zoom_to_fit_display (&screenRenderInfo);
	}
	render_refresh_rendered_image_on_screen();
	return TRUE;
}


gboolean
callbacks_drawingarea_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	if (screenRenderInfo.renderType < 2) {
		GdkPixmap *new_pixmap;
		GdkGC *gc = gdk_gc_new(widget->window);

		/*
		* Create a pixmap with default background
		*/
		new_pixmap = gdk_pixmap_new(widget->window,
					widget->allocation.width,
					widget->allocation.height,
					-1);

		gdk_gc_set_foreground(gc, &screen.background);

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
				event->area.x - screen.off_x, 
				event->area.y - screen.off_y, 
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
		if (screen.state == IN_ZOOM_OUTLINE) {
			render_draw_zoom_outline(screen.centered_outline_zoom);
		}
		else if (screen.state == IN_MEASURE) {
			render_draw_measure_distance();
		}

		return FALSE;
	}
#ifndef RENDER_USING_GDK

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

#ifdef WIN32
	/* FIXME */
	buffert = (gpointer) cairo_win32_surface_create (0);
#else      
	buffert = (gpointer) cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable),
	                                          GDK_DRAWABLE_XID (drawable),
	                                          GDK_VISUAL_XVISUAL (visual),
	                                          event->area.width, event->area.height);
#endif
      
	cr = cairo_create (buffert);
	cairo_translate (cr, -event->area.x + screen.off_x, -event->area.y + screen.off_y);
	render_project_to_cairo_target (cr);
	cairo_destroy (cr);
	cairo_surface_destroy (buffert);
        
#endif
	return FALSE;
}

void
callbacks_update_statusbar_coordinates (gint x, gint y) {
	double X, Y;

	/* make sure we don't divide by zero (which is possible if the gui
	   isn't displayed yet */
	if (screenRenderInfo.scaleFactor > 0.001) {
		X = screenRenderInfo.lowerLeftX + (x / screenRenderInfo.scaleFactor);
		Y = screenRenderInfo.lowerLeftY + ((screenRenderInfo.displayHeight - y)
			/ screenRenderInfo.scaleFactor);
	}
	else {
		X = Y = 0.0;
	}
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
}

gboolean
callbacks_drawingarea_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
	int x, y;
	GdkModifierType state;

	if (event->is_hint)
		gdk_window_get_pointer (event->window, &x, &y, &state);
	else {
		x = event->x;
		y = event->y;
		state = event->state;
	}

	switch (screen.state) {
		case IN_MOVE: {
		    if (screen.last_x != 0 || screen.last_y != 0) {
			/* Move pixmap to get a snappier feel of movement */
			screen.off_x += x - screen.last_x;
			screen.off_y += y - screen.last_y;
		    }
		    screenRenderInfo.lowerLeftX -= ((x - screen.last_x) / screenRenderInfo.scaleFactor);
		    screenRenderInfo.lowerLeftY += ((y - screen.last_y) / screenRenderInfo.scaleFactor);
		    callbacks_force_expose_event_for_screen ();
		    callbacks_update_scrollbar_positions ();
			screen.last_x = x;
			screen.last_y = y;
		    break;
		}
		case IN_ZOOM_OUTLINE: {
			if (screen.last_x || screen.last_y)
				render_draw_zoom_outline(screen.centered_outline_zoom);
			screen.last_x = x;
			screen.last_y = y;
			render_draw_zoom_outline(screen.centered_outline_zoom);
			break;
		}
		case IN_MEASURE: {
			/* clear the previous drawn line by drawing over it */
			if (screen.last_x || screen.last_y)
				render_draw_measure_distance();
			screen.last_x = x;
			screen.last_y = y;
			/* draw the new line */
			render_draw_measure_distance();
			break;
		}
		default:
			screen.last_x = x;
			screen.last_y = y;
			break;
	}
	callbacks_update_statusbar_coordinates (x, y);
	callbacks_update_ruler_pointers ();
	return TRUE;
} /* motion_notify_event */

gboolean
callbacks_drawingarea_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	GdkCursor *cursor;
	
	switch (event->button) {
		case 1 :
			if (screen.tool == POINTER) {
				/* select */
			        break; /* No op for now */
			}
			else if (screen.tool == PAN) {
				/* Plain panning */
				screen.state = IN_MOVE;
				screen.last_x = event->x;
				screen.last_y = event->y;
			}
			else if (screen.tool == ZOOM) {
				screen.state = IN_ZOOM_OUTLINE;
				/* Zoom outline mode initiated */
				screen.start_x = event->x;
				screen.start_y = event->y;
				screen.centered_outline_zoom = event->state;
			}
			else if (screen.tool == MEASURE) {
				screen.state = IN_MEASURE;
				screen.start_x = event->x;
				screen.start_y = event->y;
				/* force an expose event to clear any previous measure lines */
				callbacks_force_expose_event_for_screen ();
			}
			break;
		case 2 :
			screen.state = IN_MOVE;
			screen.last_x = event->x;
			screen.last_y = event->y;
			cursor = gdk_cursor_new(GDK_FLEUR);
			gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
					  cursor);
			gdk_cursor_destroy(cursor);
			break;
		case 3 :
			/* Zoom outline mode initiated */
			screen.state = IN_ZOOM_OUTLINE;
			screen.start_x = event->x;
			screen.start_y = event->y;
			screen.centered_outline_zoom = event->state & GDK_SHIFT_MASK;
			cursor = gdk_cursor_new(GDK_SIZING);
			gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
					  cursor);
			gdk_cursor_destroy(cursor);
			break;
		case 4 : /* Scroll wheel */
			render_zoom_display (ZOOM_IN_CMOUSE, 0, event->x, event->y);
			break;
		case 5 :  /* Scroll wheel */
			render_zoom_display (ZOOM_OUT_CMOUSE, 0, event->x, event->y);
			break;
		default:
			break;
	}
	callbacks_switch_to_correct_cursor ();
	return TRUE;
}


gboolean
callbacks_drawingarea_button_release_event (GtkWidget *widget, GdkEventButton *event)
{ 
	if (event->type == GDK_BUTTON_RELEASE) {
		if (screen.state == IN_MOVE) {  
			screen.off_x = 0;
			screen.off_y = 0;
			render_refresh_rendered_image_on_screen();
			callbacks_switch_to_normal_tool_cursor (screen.tool);
		}
		else if (screen.state == IN_ZOOM_OUTLINE) {
			if ((event->state & GDK_SHIFT_MASK) != 0) {
				render_zoom_display (ZOOM_OUT_CMOUSE, 0, event->x, event->y);
			}
			/* if the user just clicks without dragging, then simply
			   zoom in a preset amount */
			else if ((abs(screen.start_x - event->x) < 4) &&
					(abs(screen.start_y - event->y) < 4)) {
				render_zoom_display (ZOOM_IN_CMOUSE, 0, event->x, event->y);
			}
			else
				render_calculate_zoom_from_outline (widget, event);
			callbacks_switch_to_normal_tool_cursor (screen.tool);
		}
		if (screen.tool == POINTER) {
		}
		else if (screen.tool == PAN) {
			screen.state = NORMAL;
		}
		else if (screen.tool == ZOOM) {
		}
		else if (screen.tool == MEASURE) {
		}
		screen.last_x = screen.last_y = 0;
		screen.state = NORMAL;
	}
	return TRUE;
} /* button_release_event */


gboolean
callbacks_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (screen.state) {
		case NORMAL:
			switch(event->keyval) {
				case GDK_f:
				case GDK_F:
					render_zoom_to_fit_display (&screenRenderInfo);
					render_refresh_rendered_image_on_screen();
					break;
				case GDK_z:
					render_zoom_display (ZOOM_IN, 0, 0, 0);
					break;
				case GDK_Z:
					render_zoom_display (ZOOM_OUT, 0, 0, 0);
					break;
				case GDK_F1:
					callbacks_change_tool (NULL, (gpointer) 0);
					break;
				case GDK_F2:
					callbacks_change_tool (NULL, (gpointer) 1);
					break;
				case GDK_F3:
					callbacks_change_tool (NULL, (gpointer) 2);
					break;
				case GDK_F4:
					callbacks_change_tool (NULL, (gpointer) 3);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	    
	/* Escape may be used to abort outline zoom and just plain repaint */
	if (event->keyval == GDK_Escape) {
		screen.state = NORMAL;
		render_refresh_rendered_image_on_screen();
	}

	return TRUE;
} /* key_press_event */

gboolean
callbacks_window_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
	return TRUE;
} /* key_release_event */

/* Scroll wheel */
gboolean
callbacks_window_scroll_event(GtkWidget *widget, GdkEventScroll *event)
{
	switch (event->direction) {
		case GDK_SCROLL_UP:
			render_zoom_display (ZOOM_IN_CMOUSE, 0, event->x, event->y);
			break;
		case GDK_SCROLL_DOWN:
			render_zoom_display (ZOOM_OUT_CMOUSE, 0, event->x, event->y);
			break;
		case GDK_SCROLL_LEFT: 
			/* Ignore */
		case GDK_SCROLL_RIGHT:
			/* Ignore */
		default:
			return TRUE;
	}
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
	if ((screen.statusbar.coordstr != NULL)&&(GTK_IS_LABEL(screen.win.statusMessageLeft))) {
		gtk_label_set_text(GTK_LABEL(screen.win.statusMessageLeft), screen.statusbar.coordstr);
	}
	if ((screen.statusbar.diststr != NULL)&&(GTK_IS_LABEL(screen.win.statusMessageRight))) {
		gtk_label_set_text(GTK_LABEL(screen.win.statusMessageRight), screen.statusbar.diststr);
	}
}

void
callbacks_update_statusbar_measured_distance (gdouble dx, gdouble dy){
	gdouble delta = sqrt(dx*dx + dy*dy);
	
	if (screen.unit == GERBV_MILS) {
	    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     "Measured distance: %7.1f mils (%7.1f x, %7.1f y)",
		     COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));
	} 
	else if (screen.unit == GERBV_MMS) {
	    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     "Measured distance: %7.2f mms (%7.2f x, %7.2f y)",
		     COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));
	}
	else {
	    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     "Measured distance: %3.4f inches (%3.4f x, %3.4f y)",
		     COORD2MILS(delta) / 1000.0, COORD2MILS(dx) / 1000.0,
		     COORD2MILS(dy) / 1000.0);
	}
	callbacks_update_statusbar();
}

void
callbacks_sidepane_render_type_combo_box_changed (GtkComboBox *widget, gpointer user_data) {
	int activeRow = gtk_combo_box_get_active (widget);
	
	screenRenderInfo.renderType = activeRow;
	render_refresh_rendered_image_on_screen();
}

void
callbacks_statusbar_unit_combo_box_changed (GtkComboBox *widget, gpointer user_data) {
	int activeRow = gtk_combo_box_get_active (widget);
	
	if (activeRow >= 0) {
		screen.unit = activeRow;	
	}
	callbacks_update_ruler_scales();
	callbacks_update_statusbar_coordinates (screen.last_x, screen.last_y);
	
	if (screen.tool == MEASURE)
		callbacks_update_statusbar_measured_distance (screen.win.lastMeasuredX,
							screen.win.lastMeasuredY);
		
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
	      gtk_notebook_set_current_page(GTK_NOTEBOOK(screen.win.sidepane_notebook), 1);
	      gtk_widget_show(screen.win.sidepane_notebook);
		break;
	case G_LOG_LEVEL_CRITICAL:
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "red_foreground");
	      gtk_notebook_set_current_page(GTK_NOTEBOOK(screen.win.sidepane_notebook), 1);
	      gtk_widget_show(screen.win.sidepane_notebook);
		break;
	case G_LOG_LEVEL_WARNING:
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "darkred_foreground");
	      gtk_notebook_set_current_page(GTK_NOTEBOOK(screen.win.sidepane_notebook), 1);
	      gtk_widget_show(screen.win.sidepane_notebook);
		break;
	case G_LOG_LEVEL_MESSAGE:
	      tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
	                                    "darkblue_foreground");
	      gtk_notebook_set_current_page(GTK_NOTEBOOK(screen.win.sidepane_notebook), 1);
	      gtk_widget_show(screen.win.sidepane_notebook);
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


void callbacks_force_expose_event_for_screen (void){

	GdkRectangle update_rect;
	
	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = screenRenderInfo.displayWidth;
	update_rect.height = screenRenderInfo.displayHeight;

	/* Calls expose_event */
	gdk_window_invalidate_rect (screen.drawing_area->window, &update_rect, FALSE);
	
	/* update other gui things that could have changed */
	callbacks_update_ruler_scales();
	callbacks_update_scrollbar_limits();
	callbacks_update_scrollbar_positions();
}

