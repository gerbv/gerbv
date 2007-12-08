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

void
load_project(project_list_t *project_list);

int
open_image(char *filename, int idx, int reload);

typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, ZOOM_SET } gerbv_zoom_dir_t;
typedef struct {
    gerbv_zoom_dir_t z_dir;
    GdkEventButton *z_event;
    int scale;
} gerbv_zoom_data_t;

#ifndef RENDER_USING_GDK
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
		g_warning ("Using Cairo rendering requires the drawable argument to\n"
	                  "have a specified colormap. All windows have a colormap,\n"
	            "however, pixmaps only have colormap by default if they\n"
	                  "were created with a non-NULL window argument. Otherwise\n"
	                  "a colormap must be set on them with "
	                  "gdk_drawable_set_colormap");
	      return NULL;
	}
	cairo_surface_set_device_offset (surface, -x_off, -y_off);
	cr = cairo_create (surface);
	cairo_surface_destroy (surface);
	return cr;
}
#endif

void
on_new_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_open_project_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	screen.win.project = gtk_file_selection_new("Open project filename");

	if (screen.project)
		gtk_file_selection_set_filename
		    (GTK_FILE_SELECTION(screen.win.project), screen.project);
	else if (screen.path)
		gtk_file_selection_set_filename
		    (GTK_FILE_SELECTION(screen.win.project), screen.path);

	gtk_signal_connect (GTK_OBJECT(GTK_FILE_SELECTION(screen.win.project)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_project), user_data);
	gtk_signal_connect_object (GTK_OBJECT(GTK_FILE_SELECTION(screen.win.project)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.project);
	gtk_signal_connect_object (GTK_OBJECT(GTK_FILE_SELECTION(screen.win.project)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.project);


	gtk_widget_show(screen.win.project);

	gtk_grab_add(screen.win.project);
}


void
on_open_layer_activate                 (GtkMenuItem     *menuitem,
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

	g_object_set (screen.win.gerber,
		  /* GtkFileChooser */
		  "select-multiple", TRUE,
		  NULL);

	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filenames =
		    gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER (screen.win.gerber));
	}
	gtk_widget_destroy (screen.win.gerber);

	/* Now try to open all gerbers specified */
	for (filename=filenames; filename; filename=filename->next) {
		dprintf("Opening filename = %s\n", (gchar *) filename->data);

		if (open_image(filename->data, ++screen.last_loaded, FALSE) == -1) {
			GERB_MESSAGE("could not read %s[%d]", (gchar *) filename->data,
				 screen.last_loaded);
		} else {
			dprintf("     Successfully opened file!\n");	

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
						 (screen.layer_button[screen.last_loaded]),
						 TRUE);
		}
	}
	g_slist_free(filenames);

	redraw_pixmap(screen.drawing_area, TRUE);
	return;
}


void
on_revert_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save_as_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_export_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_postscript_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_png_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_pdf_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_svg_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_print_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_zoom_in_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_zoom_out_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_fit_to_window_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


/* --------------------------------------------------------- */
void
on_analyze_active_gerbers_activate(GtkMenuItem *menuitem, 
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
    GtkNotebook *notebook = gtk_notebook_new();
    
    gtk_notebook_append_page(notebook,
			     GTK_WIDGET(G_report_label),
			     gtk_label_new("G codes"));
    
    gtk_notebook_append_page(notebook,
			     GTK_WIDGET(D_report_label),
			     gtk_label_new("D codes"));
    
    gtk_notebook_append_page(notebook,
			     GTK_WIDGET(M_report_label),
			     gtk_label_new("M codes"));
    
    gtk_notebook_append_page(notebook,
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
on_analyze_active_drill_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_control_gerber_options_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_pointer_tool_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_zoom_tool_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_measure_tool_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_online_manual_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	int i;

	for (i = 0; i < MAX_FILES; i++) {
	if (screen.file[i] && screen.file[i]->image)
	    free_gerb_image(screen.file[i]->image);
	if (screen.file[i] && screen.file[i]->color)
	    g_free(screen.file[i]->color);
	if (screen.file[i] && screen.file[i]->name)
	    g_free(screen.file[i]->name);
	if (screen.file[i])
	    g_free(screen.file[i]);
	}

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


void
on_about_activate                     (GtkMenuItem     *menuitem,
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

	gtk_widget_show_all(aboutdialog1);
}


/* Zoom function */
void
zoom(GtkWidget *widget, gpointer data)
{
	double us_midx, us_midy;	/* unscaled translation for screen center */
	int half_w, half_h;		/* cache for half window dimensions */
	int mouse_delta_x = 0;	/* Delta for mouse to window center */
	int mouse_delta_y = 0;
	gerbv_zoom_data_t *z_data = (gerbv_zoom_data_t *) data;

	half_w = screen.drawing_area->allocation.width / 2;
	half_h = screen.drawing_area->allocation.height / 2;

	if (z_data->z_dir == ZOOM_IN_CMOUSE ||
		z_data->z_dir == ZOOM_OUT_CMOUSE) {
		mouse_delta_x = half_w - z_data->z_event->x;
		mouse_delta_y = half_h - z_data->z_event->y;
		screen.trans_x -= mouse_delta_x;
		screen.trans_y -= mouse_delta_y;
	}

	us_midx = (screen.trans_x + half_w)/(double) screen.transf->scale;
	us_midy = (screen.trans_y + half_h)/(double) screen.transf->scale;

	switch(z_data->z_dir) {
		case ZOOM_IN : /* Zoom In */
		case ZOOM_IN_CMOUSE : /* Zoom In Around Mouse Pointer */
		screen.transf->scale += screen.transf->scale/10;
		screen.trans_x = screen.transf->scale * us_midx - half_w;
		screen.trans_y = screen.transf->scale * us_midy - half_h;
		break;
		case ZOOM_OUT :  /* Zoom Out */
		case ZOOM_OUT_CMOUSE : /* Zoom Out Around Mouse Pointer */
		if (screen.transf->scale > 10) {
		    screen.transf->scale -= screen.transf->scale/10;
		    screen.trans_x = screen.transf->scale * us_midx - half_w;
		    screen.trans_y = screen.transf->scale * us_midy - half_h;
		}
		break;
		case ZOOM_FIT : /* Zoom Fit */
		autoscale();
		break;
		case ZOOM_SET : /*explicit scale set by user */
		screen.transf->scale = z_data->scale;
		screen.trans_x = screen.transf->scale * us_midx - half_w;
		screen.trans_y = screen.transf->scale * us_midy - half_h;
		      break;
		default :
		GERB_MESSAGE("Illegal zoom direction %ld\n", (long int)data);
	}

	if (z_data->z_dir == ZOOM_IN_CMOUSE ||
		z_data->z_dir == ZOOM_OUT_CMOUSE) {
		screen.trans_x += mouse_delta_x;
		screen.trans_y += mouse_delta_y;
	}

	/* Update clipping bbox */
	screen.clip_bbox.x1 = -screen.trans_x/(double)screen.transf->scale;
	screen.clip_bbox.y1 = -screen.trans_y/(double)screen.transf->scale;    

	/* Redraw screen */
	redraw_pixmap(screen.drawing_area, TRUE);

	return;
} /* zoom */


/** Will determine the outline of the zoomed regions.
In case region to be zoomed is too small (which correspondes e.g. to a double click) it is interpreted as a right-click and will be used to identify a part from the CURRENT selection, which is drawn on screen*/
void
zoom_outline(GtkWidget *widget, GdkEventButton *event)
{
	int x1, y1, x2, y2, dx, dy;	/* Zoom outline (UR and LL corners) */
	double us_x1, us_y1, us_x2, us_y2;
	int half_w, half_h;		/* cache for half window dimensions */

	half_w = screen.drawing_area->allocation.width / 2;
	half_h = screen.drawing_area->allocation.height / 2;

	x1 = MIN(screen.start_x, event->x);
	y1 = MIN(screen.start_y, event->y);
	x2 = MAX(screen.start_x, event->x);
	y2 = MAX(screen.start_y, event->y);
	dx = x2-x1;
	dy = y2-y1;

	if ((dx < 4) && (dy < 4)) {
		update_statusbar(&screen);
		goto zoom_outline_end;
	}

	if (screen.centered_outline_zoom) {
		/* Centered outline mode */
		x1 = screen.start_x - dx;
		y1 = screen.start_y - dy;
		dx *= 2;
		dy *= 2;
	}

	us_x1 = (screen.trans_x + x1)/(double) screen.transf->scale;
	us_y1 = (screen.trans_y + y1)/(double) screen.transf->scale;
	us_x2 = (screen.trans_x + x2)/(double) screen.transf->scale;
	us_y2 = (screen.trans_y + y2)/(double) screen.transf->scale;

	screen.transf->scale = MIN(screen.drawing_area->allocation.width/(double)(us_x2 - us_x1),
			       screen.drawing_area->allocation.height/(double)(us_y2 - us_y1));
	screen.trans_x = screen.transf->scale * (us_x1 + (us_x2 - us_x1)/2) - half_w;
	screen.trans_y = screen.transf->scale * (us_y1 + (us_y2 - us_y1)/2) - half_h;;
	screen.clip_bbox.x1 = -screen.trans_x/(double)screen.transf->scale;
	screen.clip_bbox.y1 = -screen.trans_y/(double)screen.transf->scale;

zoom_outline_end:
	/* Redraw screen */
	redraw_pixmap(screen.drawing_area, TRUE);
} /* zoom_outline */

static void
draw_zoom_outline(gboolean centered)
{
	GdkGC *gc;
	GdkGCValues values;
	GdkGCValuesMask values_mask;
	gint x1, y1, x2, y2, dx, dy;

	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
	values.foreground = *screen.zoom_outline_color;
	values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
	gc = gdk_gc_new_with_values(screen.drawing_area->window, &values, values_mask);

	x1 = MIN(screen.start_x, screen.last_x);
	y1 = MIN(screen.start_y, screen.last_y);
	x2 = MAX(screen.start_x, screen.last_x);
	y2 = MAX(screen.start_y, screen.last_y);
	dx = x2-x1;
	dy = y2-y1;

	if (centered) {
		/* Centered outline mode */
		x1 = screen.start_x - dx;
		y1 = screen.start_y - dy;
		dx *= 2;
		dy *= 2;
		x2 = x1+dx;
		y2 = y1+dy;
	}

	gdk_draw_rectangle(screen.drawing_area->window, gc, FALSE, x1, y1, dx, dy);
	gdk_gc_unref(gc);

	/* Draw actual zoom area in dashed lines */
	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
	values.foreground = *screen.dist_measure_color;
	values.line_style = GDK_LINE_ON_OFF_DASH;
	values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND | GDK_GC_LINE_STYLE;
	gc = gdk_gc_new_with_values(screen.drawing_area->window, &values,
				values_mask);

	if ((dy == 0) || ((double)dx/dy > (double)screen.drawing_area->allocation.width/screen.drawing_area->allocation.height)) {
		dy = dx * (double)screen.drawing_area->allocation.height/screen.drawing_area->allocation.width;
	} 
	else {
		dx = dy * (double)screen.drawing_area->allocation.width/screen.drawing_area->allocation.height;
	}

	gdk_draw_rectangle(screen.drawing_area->window, gc, FALSE, (x1+x2-dx)/2, (y1+y2-dy)/2, dx, dy);

	gdk_gc_unref(gc);
} /* draw_zoom_outline */


/** Displays a measured distance graphically on screen and in statusbar.
    activated when using SHIFT and mouse dragged to measure distances\n
    under win32 graphical annotations are currently disabled (GTK 2.47)*/
static void
draw_measure_distance(void)
{
#if !defined (__MINGW32__) 

	GdkGC *gc;
	GdkGCValues values;
	GdkGCValuesMask values_mask;
	GdkFont *font;
#endif   
	gint x1, y1, x2, y2;
	double delta, dx, dy;

	if (screen.state != MEASURE)
		return;
#if !defined (__MINGW32__) /*taken out because of different drawing behaviour under win32 resulting in a smear */
	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
	values.foreground = *screen.dist_measure_color;
	values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
	gc = gdk_gc_new_with_values(screen.drawing_area->window, &values,
				values_mask);
	font = gdk_font_load(setup.dist_fontname);
#endif
	x1 = MIN(screen.start_x, screen.last_x);
	y1 = MIN(screen.start_y, screen.last_y);
	x2 = MAX(screen.start_x, screen.last_x);
	y2 = MAX(screen.start_y, screen.last_y);

	dx = (x2 - x1)/(double) screen.transf->scale;
	dy = (y2 - y1)/(double) screen.transf->scale;
	delta = sqrt(dx*dx + dy*dy); /* Pythagoras */
    
#if !defined (__MINGW32__)
	gdk_draw_line(screen.drawing_area->window, gc, screen.start_x,
		  screen.start_y, screen.last_x, screen.last_y);
	if (font == NULL) {
		GERB_MESSAGE("Failed to load font '%s'\n", setup.dist_fontname);
	} 
	else {
		gchar string[65];
		gint lbearing, rbearing, width, ascent, descent;
		gint linefeed;	/* Pseudonym for inter line gap */

		snprintf(string, sizeof(string),
			 "[dist %7.1f, dX %7.1f, dY %7.1f] mils",
			 COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));

		gdk_string_extents(font, string, &lbearing, &rbearing, &width,
				   &ascent, &descent);
		gdk_draw_string(screen.drawing_area->window, font, gc,
				(x1+x2)/2-width/2, (y1+y2)/2, string);

		linefeed = ascent+descent;
		linefeed *= (double)1.2;

		snprintf(string, sizeof(string),
			 "[dist %7.2f, dX %7.2f, dY %7.2f] mm",
			 COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));

		gdk_string_extents(font, string, &lbearing, &rbearing, &width,
				   &ascent, &descent);
		gdk_draw_string(screen.drawing_area->window, font, gc,
				(x1+x2)/2 - width/2, (y1+y2)/2 + linefeed, string);

		gdk_font_unref(font);
#endif
		/*
		 * Update statusbar
		 */
		if (screen.unit == GERBV_MILS) {
		    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
			     " dist,dX,dY (%7.1f, %7.1f, %7.1f)mils",
			     COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));
		} 
		else /* unit is GERBV_MMS */ {
		    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
			     " dist,dX,dY (%7.2f, %7.2f, %7.2f)mm",
			     COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));
		}
		update_statusbar(&screen);

#if !defined (__MINGW32__)
	}
	gdk_gc_unref(gc);
#endif     
} /* draw_measure_distance */

gboolean
callback_drawingarea_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
	return redraw_pixmap(widget, TRUE);
}


gboolean
callback_drawingarea_expose_event (GtkWidget *widget, GdkEventExpose *event)
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
#ifdef GERBV_DEBUG_OUTLINE
	double dx, dy;

	dx = screen.gerber_bbox.x2-screen.gerber_bbox.x1;
	dy = screen.gerber_bbox.y2-screen.gerber_bbox.y1;
	gdk_gc_set_foreground(gc, screen.dist_measure_color);
	gdk_draw_rectangle(widget->window, gc, FALSE, 
		       (screen.gerber_bbox.x1-1.1)*screen.scale - screen.trans_x,
		       ((screen.gerber_bbox.y1-0.6)*screen.scale - screen.trans_y),
		       dx*screen.scale,
		       dy*screen.scale);
#endif /* DEBUG_GERBV_OUTLINE */

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
	/*
	* Raise popup windows if they happen to disappear
	*/
	if (screen.win.load_file && screen.win.load_file->window)
		gdk_window_raise(screen.win.load_file->window);
	if (screen.win.color_selection && screen.win.color_selection->window)
		gdk_window_raise(screen.win.color_selection->window);
	if (screen.win.export_png && screen.win.export_png->window)
		gdk_window_raise(screen.win.export_png->window);
	if (screen.win.scale && screen.win.scale->window)
		gdk_window_raise(screen.win.scale->window);
	if (screen.win.log && screen.win.log->window)
		gdk_window_raise(screen.win.log->window);

	return FALSE;
    
#else   
    
	cairo_t *cr;
	int i;
	int retval = TRUE;

	/* get a cairo_t */
	cr = callbacks_gdk_cairo_create (widget->window);
	
	/* translate the draw area before drawing */
	cairo_translate (cr,-screen.trans_x-(screen.gerber_bbox.x1*(float) screen.transf->scale),-screen.trans_y+(screen.gerber_bbox.y2*(float) screen.transf->scale));
	/* scale the drawing by the specified scale factor (inverting y since
	 * cairo y axis points down)
	 */ 
	cairo_scale (cr,(float) screen.transf->scale,-(float) screen.transf->scale);
	
	/* fill the background with the appropriate color */
	cairo_set_source_rgba (cr, (double) screen.background->red/G_MAXUINT16,
		(double) screen.background->green/G_MAXUINT16,
		(double) screen.background->blue/G_MAXUINT16, 1);
	cairo_paint (cr);
	
	/* 
	* This now allows drawing several layers on top of each other.
	* Higher layer numbers have higher priority in the Z-order. 
	*/
	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i] && screen.file[i]->isVisible && screen.file[i]->privateRenderData) {
			enum polarity_t polarity;

			if (screen.file[i]->inverted) {
				if (screen.file[i]->image->info->polarity == POSITIVE)
				polarity = NEGATIVE;
				else
				polarity = POSITIVE;
			} else {
				polarity = screen.file[i]->image->info->polarity;
			}
			
                  cairo_set_source (cr,(cairo_pattern_t *)screen.file[i]->privateRenderData);
			if ((double) screen.file[i]->alpha < 65535) {				
				cairo_paint_with_alpha(cr,(double) screen.file[i]->alpha/G_MAXUINT16);
			}
			else {
				cairo_paint (cr);
			}
		}
	}
	cairo_destroy (cr);
	return retval;
#endif
}

gboolean
callback_drawingarea_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
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

	if (screen.pixmap != NULL) {
		double X, Y;

		X = screen.gerber_bbox.x1 + (x+screen.trans_x)/(double)screen.transf->scale;
		Y = (screen.gerber_bbox.y2 - (y+screen.trans_y)/(double)screen.transf->scale);
		if (screen.unit == GERBV_MILS) {
		    snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
			     "X,Y (%7.1f, %7.1f)mils",
			     COORD2MILS(X), COORD2MILS(Y));
		} else /* unit is GERBV_MMS */ {
		    snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
			     "X,Y (%7.2f, %7.2f)mm",
			     COORD2MMS(X), COORD2MMS(Y));
		}
		update_statusbar(&screen);

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
		     gtk_widget_draw (widget, &update_rect);
		    //gdk_window_invalidate_rect(widget->window, &widget->allocation,FALSE);

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
	}

	return TRUE;
} /* motion_notify_event */

gboolean
callback_drawingarea_button_press_event (GtkWidget *widget, GdkEventButton *event)
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
callback_drawingarea_button_release_event (GtkWidget *widget, GdkEventButton *event)
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
callback_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
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
		update_statusbar(&screen);

		update_rect.x = 0, update_rect.y = 0;
		update_rect.width =	widget->allocation.width;
		update_rect.height = widget->allocation.height;

		/*
		 * Calls expose_event
		 */
		 gtk_widget_draw (widget, &update_rect);
		//gdk_window_invalidate_rect(widget->window, &widget->allocation,FALSE);
	}

	return TRUE;
} /* key_press_event */

gboolean
callback_window_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (screen.state) {
		case NORMAL:
			if((event->keyval == GDK_Shift_L) ||
			   (event->keyval == GDK_Shift_R)) {
			    gdk_window_set_cursor(gtk_widget_get_parent_window(screen.drawing_area),
						  GERBV_DEF_CURSOR);
			}
			break;
		case ALT_PRESSED:
			if ((event->keyval == GDK_Alt_L) ||
			     (event->keyval == GDK_Alt_R)) {
			    if ((screen.selected_layer != -1) &&
				(screen.selected_layer < MAX_FILES)){
				//TOGGLE_BUTTON(screen.layer_button[screen.selected_layer]);
			    }
			    screen.state = NORMAL;
			}
		default:
			break;
	}

	return TRUE;
} /* key_release_event */

/* Scroll wheel */
gboolean
callback_window_scroll_event(GtkWidget *widget, GdkEventScroll *event)
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



void 
swap_layers(GtkWidget *widget, gpointer data)
{
	gerbv_fileinfo_t *temp_file;
	GtkStyle *s1, *s2;
	GtkTooltipsData *td1, *td2;
	int idx;

	if (screen.file[screen.curr_index] == NULL) return;

	if (data == 0) {
		/* Moving Up */
		if (screen.curr_index == 0) return;
		if (screen.file[screen.curr_index - 1] == NULL) return;
		idx = -1;
	}
	else { 
		/* Moving Down */
		if (screen.curr_index == MAX_FILES - 1) return;
		if (screen.file[screen.curr_index + 1] == NULL) return;
		idx = 1;
	}

	/* Swap file */
	temp_file = screen.file[screen.curr_index];
	screen.file[screen.curr_index] = screen.file[screen.curr_index + idx];
	screen.file[screen.curr_index + idx] = temp_file;

	/* Swap color on button */
	s1 = gtk_widget_get_style(screen.layer_button[screen.curr_index]);
	s2 = gtk_widget_get_style(screen.layer_button[screen.curr_index + idx]);
	gtk_widget_set_style(screen.layer_button[screen.curr_index + idx], s1);
	gtk_widget_set_style(screen.layer_button[screen.curr_index], s2);

	/* Swap tooltips on button */
	td1 = gtk_tooltips_data_get(screen.layer_button[screen.curr_index]);
	td2 = gtk_tooltips_data_get(screen.layer_button[screen.curr_index + idx]);
	gtk_tooltips_set_tip(td1->tooltips, td1->widget,
			 screen.file[screen.curr_index]->name, NULL);
	gtk_tooltips_set_tip(td2->tooltips, td2->widget,
			 screen.file[screen.curr_index + idx]->name, NULL);


	redraw_pixmap(screen.drawing_area, TRUE);
    
} /* swap_layers */


void 
invert_color(GtkWidget *widget, gpointer data)
{
	if (!screen.file[screen.curr_index])
		return;

	if (screen.file[screen.curr_index]->inverted)
		screen.file[screen.curr_index]->inverted = 0;
	else
		screen.file[screen.curr_index]->inverted = 1;

	redraw_pixmap(screen.drawing_area, TRUE);

	return;
} /* invert_color */


void
cb_ok_load_file(GtkWidget *widget, GtkFileSelection *fs)
{
	char *filename;

	filename = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
	if (open_image(filename, ++screen.last_loaded, FALSE) != -1) {

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
					     (screen.layer_button[screen.last_loaded]),
					     TRUE);

#ifdef HAVE_LIBGEN_H    

		/*
		 * Remember where we loaded file from last time
		 */
		filename = dirname(filename);
#endif        
		if (screen.path)
		    g_free(screen.path);
		screen.path = (char *)malloc(strlen(filename) + 2);
		if (screen.path == NULL)
		    GERB_FATAL_ERROR("malloc screen.path failed\n");
		strcpy(screen.path, filename);
		screen.path = strncat(screen.path, "/", 1);

		/* Make loaded image appear on screen */
		redraw_pixmap(screen.drawing_area, TRUE);
	}

	gtk_grab_remove(screen.win.load_file);
	gtk_widget_destroy(screen.win.load_file);
	screen.win.load_file = NULL;

	return;
} /* cb_ok_load_file */


void
cb_cancel_load_file(GtkWidget *widget, gpointer data)
{
	gtk_grab_remove(screen.win.load_file);
	gtk_widget_destroy(screen.win.load_file);
	screen.win.load_file = NULL;

	return;
} /* cb_cancel_load_file */


void
load_file_popup(GtkWidget *widget, gpointer data)
{
	screen.win.load_file = gtk_file_selection_new("Select File To View");

	if (screen.path)
		gtk_file_selection_set_filename
		    (GTK_FILE_SELECTION(screen.win.load_file), screen.path);

	gtk_signal_connect(GTK_OBJECT(screen.win.load_file), "destroy",
		       GTK_SIGNAL_FUNC(cb_cancel_load_file),
		       NULL);
	gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.load_file)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_load_file), 
	 (gpointer)screen.win.load_file);
	gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.load_file)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_cancel_load_file), 
	 (gpointer)screen.win.load_file);

	gtk_widget_show(screen.win.load_file);

	gtk_grab_add(screen.win.load_file);

	return;
} /* load_file_popup */


#ifdef EXPORT_PNG
void
cb_ok_export_png(GtkWidget *widget, GtkFileSelection *fs)
{
	char *filename;
	gboolean result;
	GdkWindow *window;

	filename = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

	/* This might be lengthy, show that we're busy by changing the pointer */
	window = gtk_widget_get_parent_window(widget);
	if (window) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new(GDK_WATCH);
		gdk_window_set_cursor(window, cursor);
		gdk_cursor_destroy(cursor);
	}

	/* Export PNG */
#ifdef EXPORT_DISPLAYED_IMAGE
	result = png_export(screen.pixmap, filename);
#else
	result = png_export(NULL, filename);
#endif /* EXPORT_DISPLAYED_IMAGE */
	if (!result) {
		GERB_MESSAGE("Failed to save PNG at %s\n", filename);
	}

	/* Return default pointer shape */
	if (window) {
		gdk_window_set_cursor(window, GERBV_DEF_CURSOR);
	}

	/*
	* Remember where we loaded file from last time
	*/
#ifdef HAVE_LIBGEN_H     
	filename = dirname(filename);
#endif    
	if (screen.path)
		g_free(screen.path);
	screen.path = (char *)malloc(strlen(filename) + 2);
	if (screen.path == NULL)
		GERB_FATAL_ERROR("malloc screen.path failed\n");
	strcpy(screen.path, filename);
	screen.path = strncat(screen.path, "/", 1);

	gtk_grab_remove(screen.win.export_png);

	screen.win.export_png = NULL;

	return;
} /* cb_ok_export_png */

/* ------------------------------------------------------------------ */
void
export_png_popup(GtkWidget *widget, gpointer data)
{
	screen.win.export_png = gtk_file_selection_new("Save PNG filename");

	if (screen.path)
		gtk_file_selection_set_filename
		    (GTK_FILE_SELECTION(screen.win.export_png), screen.path);

	gtk_signal_connect
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.export_png)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(cb_ok_export_png), 
	 (gpointer)screen.win.export_png);
	gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.export_png)->ok_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.export_png);
	gtk_signal_connect_object
	(GTK_OBJECT(GTK_FILE_SELECTION(screen.win.export_png)->cancel_button),
	 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), 
	 (gpointer)screen.win.export_png);

	gtk_widget_show(screen.win.export_png);

	gtk_grab_add(screen.win.export_png);

	return;
} /* export_png_popup */

#endif /* EXPORT_PNG */

/* ------------------------------------------------------------------ */
/** Displays additional information in the statusbar.
    The Statusbar is divided into three sections:\n
    statusbar.coordstr for coords\n
    statusbar.diststr for displaying measured distances or the designator 
    (right click on a graphically marked and also actively selected part)\n
    statusbar.msg for e.g. showing progress of actions*/
void 
update_statusbar(gerbv_screen_t *scr)
{
	char str[MAX_STATUSMSGLEN+1];

	snprintf(str, MAX_STATUSMSGLEN, " %-*s|%-*s|%.*s",
	     MAX_COORDLEN-1, scr->statusbar.coordstr,
	     MAX_DISTLEN-1, scr->statusbar.diststr,
	     MAX_ERRMSGLEN-1, scr->statusbar.msgstr);
	if (scr->statusbar.msg != NULL) {
		gtk_label_set_text(GTK_LABEL(scr->statusbar.msg), str);
	}
} /* update_statusbar */

/* ------------------------------------------------------------------ */
void
reload_files(GtkWidget *widget, gpointer data)
{
	int idx;

	for (idx = 0; idx < MAX_FILES; idx++) {
	    if (screen.file[idx] && screen.file[idx]->name) {
	        if (open_image(screen.file[idx]->name, idx, TRUE) == -1)
		    return;
	    }
	}

	redraw_pixmap(screen.drawing_area, TRUE);

	return;
} /* reload_files */

/* ------------------------------------------------------------------ */
void
unload_file(GtkWidget *widget, gpointer data)
{
	int         idx = screen.curr_index;
	GtkStyle   *defstyle;

	if (screen.file[idx] == NULL)
		return;

	/*
	* Deselect the layer we're unloading so it's not left in the pixmap
	*/
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(screen.layer_button[idx]),
				 FALSE);

	/* 
	* Remove color on layer button (set default style) 
	*/
	defstyle = gtk_widget_get_default_style();
	gtk_widget_set_style(screen.layer_button[idx], defstyle);

	/* 
	* Remove tool tips 
	*/
	gtk_tooltips_set_tip(screen.tooltips, screen.layer_button[idx], 
			 NULL, NULL); 

	/* 
	* Remove data struct 
	*/
	free_gerb_image(screen.file[idx]->image);  screen.file[idx]->image = NULL;
	g_free(screen.file[idx]->color);  screen.file[idx]->color = NULL;
	g_free(screen.file[idx]->name);  screen.file[idx]->name = NULL;
	g_free(screen.file[idx]);  screen.file[idx] = NULL;

	return;
} /* unload_file */

/*
void
project_save_cb(GtkWidget *widget, gpointer data)
{

    if (!screen.project) 
        project_popup(widget, (gpointer) SAVE_AS_PROJECT);
    else 
        cb_ok_project(widget, (gpointer) SAVE_PROJECT);

}
*/

/* ------------------------------------------------------------------ */
void
cb_ok_project(GtkWidget *widget, gpointer data)
{
    char *filename = NULL;
    project_list_t *project_list = NULL, *tmp;
    int idx;
    
    if (screen.win.project) {
	filename = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(screen.win.project));

	/*
	 * Remember where we loaded file from last time
	 */
	if (screen.path)
	    g_free(screen.path);
	screen.path = (char *)malloc(strlen(filename) + 2);
	if (screen.path == NULL)
	    GERB_FATAL_ERROR("malloc screen.path failed\n");
	strcpy(screen.path, filename);
#ifdef HAVE_LIBGEN_H        
	dirname(screen.path);
#endif        
	screen.path = strncat(screen.path, "/", 1);
    }

    switch ((long)data) {
    case OPEN_PROJECT:
	
	project_list = read_project_file(filename);

	if (project_list) {
	    load_project(project_list);
	    /*
	     * Save project filename for later use
	     */
	    if (screen.project) {
		g_free(screen.project);
		screen.project = NULL;
	    }
	    screen.project = (char *)malloc(strlen(filename) + 1);
	    if (screen.project == NULL)
		GERB_FATAL_ERROR("malloc screen.project failed\n");
	    memset((void *)screen.project, 0, strlen(filename) + 1);
	    strncpy(screen.project, filename, strlen(filename));
            rename_main_window(filename, NULL);
	    redraw_pixmap(screen.drawing_area, TRUE);
	} else {
	    GERB_MESSAGE("Failed to load project\n");
	    goto cb_ok_project_end;
	}
	//all_layers_on(NULL, NULL);
	break;
    case SAVE_AS_PROJECT:
	/*
	 * Save project filename for later use
	 */
	if (screen.project) {
	    g_free(screen.project);
	    screen.project = NULL;
	}
	screen.project = (char *)malloc(strlen(filename) + 1);
	if (screen.project == NULL)
	    GERB_FATAL_ERROR("malloc screen.project failed\n");
	memset((void *)screen.project, 0, strlen(filename) + 1);
	strncpy(screen.project, filename, strlen(filename));
	rename_main_window(filename, NULL);
	
    case SAVE_PROJECT:
	if (!screen.project) {
	    GERB_MESSAGE("Missing project filename\n");
	    goto cb_ok_project_end;
	}
	
	if (screen.path) {
	    project_list = (project_list_t *)malloc(sizeof(project_list_t));
	    if (project_list == NULL)
		GERB_FATAL_ERROR("malloc project_list failed\n");
	    memset(project_list, 0, sizeof(project_list_t));
	    project_list->next = project_list;
	    project_list->layerno = -1;
	    project_list->filename = screen.path;
	    project_list->rgb[0] = screen.background->red;
	    project_list->rgb[1] = screen.background->green;
	    project_list->rgb[2] = screen.background->blue;
	    project_list->next = NULL;
	}
	
	for (idx = 0; idx < MAX_FILES; idx++) {
	    if (screen.file[idx] &&  screen.file[idx]->color) {
		tmp = (project_list_t *)malloc(sizeof(project_list_t));
		if (tmp == NULL) 
		    GERB_FATAL_ERROR("malloc tmp failed\n");
		memset(tmp, 0, sizeof(project_list_t));
		tmp->next = project_list;
		tmp->layerno = idx;
		
		tmp->filename = screen.file[idx]->name;
		tmp->rgb[0] = screen.file[idx]->color->red;
		tmp->rgb[1] = screen.file[idx]->color->green;
		tmp->rgb[2] = screen.file[idx]->color->blue;
		tmp->inverted = screen.file[idx]->inverted;
		project_list = tmp;
	    }
	}
	
	if (write_project_file(screen.project, project_list)) {
	    GERB_MESSAGE("Failed to write project\n");
	    goto cb_ok_project_end;
	}
	break;
    default:
	GERB_FATAL_ERROR("Unknown operation in cb_ok_project\n");
    }
#ifdef HAVE_LIBGEN_H
    /*
     * Remember where we loaded file from last time
     */
    filename = dirname(filename);
#endif 
    if (screen.path)
	   g_free(screen.path);
    screen.path = (char *)malloc(strlen(filename) + 2);
    if (screen.path == NULL)
	GERB_FATAL_ERROR("malloc screen.path failed\n");
    strcpy(screen.path, filename);
    screen.path = strncat(screen.path, "/", 1);

 cb_ok_project_end:
    if (screen.win.project) {
	gtk_grab_remove(screen.win.project);
    	screen.win.project = NULL;
    }

    return;
} /* cb_ok_project */

