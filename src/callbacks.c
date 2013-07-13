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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** \file callbacks.c
    \brief Callback functions for the GUI widgets
    \ingroup gerbv
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#if !defined(WIN32) && !defined(QUARTZ)
#include <gdk/gdkx.h>
#endif
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>
#include "common.h"
#include "gerbv.h"
#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "attribute.h"
#include "render.h"

#include "draw-gdk.h"

#include "draw.h"
#ifdef WIN32
#include <cairo-win32.h>
#elif QUARTZ
#include <cairo-quartz.h>
#else
#include <cairo-xlib.h>
#endif


#define dprintf if(DEBUG) printf

/* This default extension should really not be changed, but if it absolutely
 * must change, the ../win32/gerbv.nsi.in *must* be changed to reflect that.
 * Just grep for the extension (gvp) and change it in two places in that file.
 */
#define GERBV_PROJECT_FILE_NAME N_("Gerbv Project")
#define GERBV_PROJECT_FILE_EXT ".gvp"
#define GERBV_PROJECT_FILE_PAT "*.gvp"

#define SAVE_PROJECT 0
#define SAVE_AS_PROJECT 1
#define OPEN_PROJECT 2

/**Global variable to keep track of what's happening on the screen.
   Declared extern in gerbv_screen.h
 */
extern gerbv_screen_t screen;
extern gerbv_render_info_t screenRenderInfo;


/* These are the names of the valid apertures.  These
 * values are used in several places in this file.
 * Please keep this in sync with the gerbv_aperture_type_t 
 * enum defined in gerbv.h */
char *ap_names[] = {"NONE",
		    "CIRCLE",
		    "RECTANGLE",
		    "OVAL",           /* an ovular (obround) aperture */
		    "POLYGON",        /* a polygon aperture */
		    "MACRO",          /* a RS274X macro */
		    "MACRO_CIRCLE",   /* a RS274X circle macro */
		    "MACRO_OUTLINE",  /* a RS274X outline macro */
		    "MACRO_POLYGON",  /* a RS274X polygon macro */
		    "MACRO_MOIRE",    /* a RS274X moire macro */
		    "MACRO_THERMAL",  /* a RS274X thermal macro */
		    "MACRO_LINE20",   /* a RS274X line (code 20) macro */
		    "MACRO_LINE21",   /* a RS274X line (code 21) macro */
		    "MACRO_LINE22"    /* a RS274X line (code 22) macro */
};

static gint callbacks_get_selected_row_index (void);
static void callbacks_units_changed (gerbv_gui_unit_t unit);
static void callbacks_update_statusbar_coordinates (gint x, gint y);
static void callbacks_update_ruler_scales (void);
static void callbacks_render_type_changed (void);
static void show_no_layers_warning (void);
static double screen_units(double);
static double line_length(double, double, double, double);
static double arc_length(double, double);
static void aperture_report(gerbv_aperture_t *[], int);


gchar *utf8_strncpy(gchar *dst, const gchar *src, gsize byte_len)
{
	/* -1 for '\0' in buffer */
	glong char_len = g_utf8_strlen(src, byte_len - 1);
	return g_utf8_strncpy(dst, src, char_len);
}

void utf8_snprintf(gchar *dst, gsize byte_len, const gchar *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	gchar *str = g_strdup_vprintf(fmt, ap); 
	utf8_strncpy(dst, str, byte_len);
	g_free(str);
}

/* --------------------------------------------------------- */

static void show_no_layers_warning (void) {
	gchar *str = g_new(gchar, MAX_DISTLEN);
	utf8_strncpy(str, _("No layers are currently loaded. A layer must be loaded first."), MAX_DISTLEN - 7);
	utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN, "<b>%s</b>", str);
	g_free(str);
		
	callbacks_update_statusbar();
}

/* --------------------------------------------------------- */
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

/* --------------------------------------------------------- */
/**
  * The file -> new menu item was selected.  Create new
  * project.
  *
  */
void
callbacks_new_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	if (mainProject->last_loaded >= 0) {
		if (!interface_get_alert_dialog_response (
		       _("Do you want to close any open layers and start a new project?"),
		       _("Starting a new project will cause all currently open layers to be closed. Any unsaved changes will be lost."),
		       FALSE,
		       NULL))
			return;
	}
	/* Unload all layers and then clear layer window */
	gerbv_unload_all_layers (mainProject);
	callbacks_update_layer_tree ();
	render_clear_selection_buffer ();
	
	/* Destroy project info */
	if (mainProject->project) {
	    g_free(mainProject->project);
	    mainProject->project = NULL;
	}
	render_refresh_rendered_image_on_screen();
}


/* --------------------------------------------------------- */
/**
  * The file -> open menu item was selected.  Open a
  * project file.
  *
  */
void
callbacks_open_project_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar *filename=NULL;
	GtkFileFilter * filter;

	if (mainProject->last_loaded >= 0) {
		if (!interface_get_alert_dialog_response (
                       _("Do you want to close any open layers and load an existing project?"),
		       _("Loading a project will cause all currently open layers to be closed. Any unsaved changes will be lost."),
			FALSE,
			NULL))
			return;
	}
	
	screen.win.gerber = 
	gtk_file_chooser_dialog_new (_("Open project file..."),
				     NULL,
				     GTK_FILE_CHOOSER_ACTION_OPEN,
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
				     NULL);
	gtk_file_chooser_set_current_folder ((GtkFileChooser *) screen.win.gerber,
		mainProject->path);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _(GERBV_PROJECT_FILE_NAME));
	gtk_file_filter_add_pattern(filter, GERBV_PROJECT_FILE_PAT);
	gtk_file_chooser_add_filter ((GtkFileChooser *) screen.win.gerber,
	        filter);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter ((GtkFileChooser *) screen.win.gerber,
	        filter);

	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filename =
		    gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (screen.win.gerber));
		/* update the last folder */
		g_free (mainProject->path);
		mainProject->path = gtk_file_chooser_get_current_folder ((GtkFileChooser *) screen.win.gerber);
	}
	gtk_widget_destroy (screen.win.gerber);

	if (filename) {
		gerbv_unload_all_layers (mainProject);
		main_open_project_from_filename (mainProject, filename);
	}
	gerbv_render_zoom_to_fit_display (mainProject, &screenRenderInfo);
	render_refresh_rendered_image_on_screen();
	callbacks_update_layer_tree();

	return;
}


/* --------------------------------------------------------- */
/**
  * The file -> open layer menu item was selected.  Open a
  * layer (or layers) from a file.
  *
  */
void
callbacks_open_layer_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GSList *filenames=NULL;
	GSList *filename=NULL;

	screen.win.gerber = 
	gtk_file_chooser_dialog_new (_("Open Gerber, drill, or pick & place file(s)..."),
				     NULL,
				     GTK_FILE_CHOOSER_ACTION_OPEN,
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
				     NULL);

	gtk_file_chooser_set_select_multiple((GtkFileChooser *) screen.win.gerber, TRUE);
	gtk_file_chooser_set_current_folder ((GtkFileChooser *) screen.win.gerber,
		mainProject->path);
	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filenames =
		    gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER (screen.win.gerber));
		/* update the last folder */
		g_free (mainProject->path);
		mainProject->path = gtk_file_chooser_get_current_folder ((GtkFileChooser *) screen.win.gerber);
	}
	gtk_widget_destroy (screen.win.gerber);

	/* Now try to open all gerbers specified */
	for (filename=filenames; filename; filename=filename->next) {
		gerbv_open_layer_from_filename (mainProject, filename->data);
	}
	g_slist_free(filenames);
	
	gerbv_render_zoom_to_fit_display (mainProject, &screenRenderInfo);
	render_refresh_rendered_image_on_screen();
	callbacks_update_layer_tree();

	return;
}

/* --------------------------------------------------------- */
void
callbacks_revert_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gerbv_revert_all_files (mainProject);
	render_clear_selection_buffer();
	callbacks_update_selected_object_message(FALSE);
	render_refresh_rendered_image_on_screen();
	callbacks_update_layer_tree();
}

/* --------------------------------------------------------- */
void
callbacks_save_project_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (mainProject->project)
    main_save_project_from_filename (mainProject, mainProject->project);
  else
    callbacks_generic_save_activate (menuitem, (gpointer) CALLBACKS_SAVE_PROJECT_AS);
  callbacks_update_layer_tree();
  return;
}

/* --------------------------------------------------------- */
void
callbacks_save_layer_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  /* first figure out which layer in the layer side menu is selected */
  gint index=callbacks_get_selected_row_index();
  
  /* Now save that layer */
  if (index >= 0) {
    if (!gerbv_save_layer_from_index (mainProject, index, mainProject->file[index]->fullPathname)) {
      interface_show_alert_dialog(_("Gerbv cannot export this file type"), 
				  NULL,
				  FALSE,
				  NULL);
      mainProject->file[index]->layer_dirty = FALSE;
      callbacks_update_layer_tree();
      return;
    }
  }
  callbacks_update_layer_tree();
  return;
}
struct l_image_info {
	gerbv_image_t *image;
	gerbv_user_transformation_t *transform;
};
/* --------------------------------------------------------- */
/**Go through each file and look at visibility, then type.  
Make sure we have at least 2 files.
*/

gerbv_image_t *merge_images (int type)
{
	gint i, filecount, img;
/*	struct l_image_info *images; */
	gerbv_image_t *out;
	struct l_image_info {
	gerbv_image_t *image;
	gerbv_user_transformation_t *transform;
	}*images;
	
	
	images=(struct l_image_info *)g_new0(struct l_image_info,1);
	out=NULL;
	switch(type){
		case CALLBACKS_SAVE_FILE_DRILLM:
				type=GERBV_LAYERTYPE_DRILL;
			break;
		case CALLBACKS_SAVE_FILE_RS274XM:
				type=GERBV_LAYERTYPE_RS274X;
			break;
		default:
			GERB_MESSAGE(_("Unknown Layer type for merge\n"));
			goto err;
	}
	dprintf(_("Looking for matching files\n")); 
	for (i=img=filecount=0;i<mainProject->max_files;++i){
		if (mainProject->file[i] &&  mainProject->file[i]->isVisible &&
	    (mainProject->file[i]->image->layertype == type) ) {
			++filecount;
			dprintf(_("Adding '%s'\n"),mainProject->file[i]->name); 
			images[img].image=mainProject->file[i]->image;
/*			printf("Adding transform\n"); */
		  images[img++].transform=&mainProject->file[i]->transform;
/*			printf("Realloc\n"); */
			images=(struct l_image_info *)g_renew(struct l_image_info, images,img+1);
		}
/*		printf("Done with add\n"); */
	}
	if(2>filecount){
		GERB_MESSAGE (_("Not Enough Files of same type to merge\n"));
		goto err;
	}
	dprintf(_("Now merging files\n"));
	for (i=0;i<img;++i){
		gerbv_user_transformation_t *thisTransform;
		gerbv_user_transformation_t identityTransform = {0,0,1,1,0,FALSE,FALSE,FALSE};
		thisTransform=images[i].transform;
		if (NULL == thisTransform ) 
			thisTransform = &identityTransform;
		if(0 == i)
			out = gerbv_image_duplicate_image (images[i].image, thisTransform);
		else
			gerbv_image_copy_image(images[i].image,thisTransform,out);
	}
err:
	g_free(images);
	return out;
}
/* --------------------------------------------------------- */
void
callbacks_generic_save_activate (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar *filename=NULL;
	gint processType = GPOINTER_TO_INT (user_data);
	gchar *windowTitle=NULL;
	GtkFileFilter * filter;
	
	gint index=callbacks_get_selected_row_index ();
	if (index < 0) {
		interface_show_alert_dialog(_("No layer is currently selected"),
			_("Please select a layer and try again."),
			FALSE,
			NULL);
		return;
	}
	
	if (processType == CALLBACKS_SAVE_PROJECT_AS)
		windowTitle = g_strdup (_("Save project as..."));
	else if (processType == CALLBACKS_SAVE_FILE_PS)
		windowTitle = g_strdup (_("Export PS file as..."));
	else if (processType == CALLBACKS_SAVE_FILE_PDF)
		windowTitle = g_strdup (_("Export PDF file as..."));
	else if (processType == CALLBACKS_SAVE_FILE_SVG)
		windowTitle = g_strdup (_("Export SVG file as..."));
	else if (processType == CALLBACKS_SAVE_FILE_PNG)
		windowTitle = g_strdup (_("Export PNG file as..."));
	else if (processType == CALLBACKS_SAVE_FILE_RS274X)
		windowTitle = g_strdup (_("Export RS-274X file as..."));
	else if (processType == CALLBACKS_SAVE_FILE_DRILL)
		windowTitle = g_strdup (_("Export Excellon drill file as..."));
	else if (processType == CALLBACKS_SAVE_FILE_RS274XM)
		windowTitle = g_strdup (_("Export RS-274Xm file as..."));
	else if (processType == CALLBACKS_SAVE_FILE_DRILLM)
		windowTitle = g_strdup (_("Export Excellon drillm file as..."));
	else if (processType == CALLBACKS_SAVE_LAYER_AS)
		windowTitle = g_strdup (_("Save layer as..."));
		
	screen.win.gerber = 
	gtk_file_chooser_dialog_new (windowTitle, NULL,
				     GTK_FILE_CHOOSER_ACTION_SAVE,
				     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     GTK_STOCK_SAVE,   GTK_RESPONSE_ACCEPT,
				     NULL);
	g_free (windowTitle);
	
	/* if we're saving or exporting a layer, start off in the location of the
	   loaded file */
	if (processType != CALLBACKS_SAVE_PROJECT_AS) {
		gint index=callbacks_get_selected_row_index();
		if (index >= 0) {
			gchar *dirName = g_path_get_dirname (mainProject->file[index]->fullPathname);
			gtk_file_chooser_set_current_folder ((GtkFileChooser *) screen.win.gerber,
				dirName);
			g_free (dirName);
		}
	}

	if (processType == CALLBACKS_SAVE_PROJECT_AS) {
	  filter = gtk_file_filter_new();
	  gtk_file_filter_set_name(filter, _(GERBV_PROJECT_FILE_NAME));
	  gtk_file_filter_add_pattern(filter, GERBV_PROJECT_FILE_PAT);
	  gtk_file_chooser_add_filter ((GtkFileChooser *) screen.win.gerber,
				       filter);

	  filter = gtk_file_filter_new();
	  gtk_file_filter_set_name(filter, _("All"));
	  gtk_file_filter_add_pattern(filter, "*");
	  gtk_file_chooser_add_filter ((GtkFileChooser *) screen.win.gerber,
				       filter);
	  
	  gtk_file_chooser_set_current_name ((GtkFileChooser *) screen.win.gerber, 
					     "untitled" GERBV_PROJECT_FILE_EXT );
	}

	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (screen.win.gerber));
	}
	gtk_widget_destroy (screen.win.gerber);

	if (filename) {
		if (processType == CALLBACKS_SAVE_PROJECT_AS) {
			main_save_as_project_from_filename (mainProject, filename);
			rename_main_window(filename, NULL);
		}
		else if (processType == CALLBACKS_SAVE_FILE_PS)
			gerbv_export_postscript_file_from_project_autoscaled (mainProject, filename);
		else if (processType == CALLBACKS_SAVE_FILE_PDF)
			gerbv_export_pdf_file_from_project_autoscaled (mainProject, filename);
		else if (processType == CALLBACKS_SAVE_FILE_SVG)
			gerbv_export_svg_file_from_project_autoscaled (mainProject, filename);
		else if (processType == CALLBACKS_SAVE_FILE_PNG)
			gerbv_export_png_file_from_project_autoscaled (mainProject,
				screenRenderInfo.displayWidth, screenRenderInfo.displayHeight,
				filename);
		else if (processType == CALLBACKS_SAVE_LAYER_AS) {
			gint index=callbacks_get_selected_row_index();
			
			gerbv_save_layer_from_index (mainProject, index, filename);
			/* rename the file path in the index, so future saves will reference the new file path */
			g_free (mainProject->file[index]->fullPathname);
			mainProject->file[index]->fullPathname = g_strdup (filename);
			g_free (mainProject->file[index]->name);
			mainProject->file[index]->name = g_path_get_basename (filename);
		}
		else if (processType == CALLBACKS_SAVE_FILE_RS274X) {
			gint index=callbacks_get_selected_row_index();

			gerbv_export_rs274x_file_from_image (filename, mainProject->file[index]->image,
				&mainProject->file[index]->transform);	
		}
		else if (processType == CALLBACKS_SAVE_FILE_DRILL) {
			gint index=callbacks_get_selected_row_index();
			
			gerbv_export_drill_file_from_image (filename, mainProject->file[index]->image,
				&mainProject->file[index]->transform);
		}	/**create new image....  */
		else if (processType == CALLBACKS_SAVE_FILE_RS274XM) {
			gerbv_image_t *image;
			gerbv_user_transformation_t t = {0,0,1,1,0,FALSE,FALSE,FALSE};
			if(NULL != (image=merge_images(processType)) ){
				/*printf("Preparing to export merge\n"); */
				gerbv_export_rs274x_file_from_image (filename, image,	&t);	
				gerbv_destroy_image(image);
				GERB_MESSAGE (_("Merged visible gerber layers and placed in '%s'\n"),filename);
			}
		}
		else if (processType == CALLBACKS_SAVE_FILE_DRILLM) {
			gerbv_image_t *image;
			gerbv_user_transformation_t t = {0,0,1,1,0,FALSE,FALSE,FALSE};
			if(NULL != (image=merge_images(processType)) ){
				gerbv_export_drill_file_from_image (filename, image,&t);
				gerbv_destroy_image(image);
				GERB_MESSAGE (_("Merged visible drill layers and placed in '%s'\n"),filename);
			}	
		}		
	}
	g_free (filename);
	callbacks_update_layer_tree();
	return;
}

/* --------------------------------------------------------- */
#if GTK_CHECK_VERSION(2,10,0)

static void
callbacks_begin_print (GtkPrintOperation *operation, GtkPrintContext   *context,
		gpointer user_data) {
	gtk_print_operation_set_n_pages     (operation, 1);
}


/* --------------------------------------------------------- */
static void
callbacks_print_render_page (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           gint               page_nr,
           gpointer           user_data)
{
	GtkPrintSettings *pSettings = gtk_print_operation_get_print_settings (operation);
	gerbv_render_info_t renderInfo = {1.0, 1.0, 0, 0, 3,
		(gint) gtk_print_context_get_width (context),
		(gint) gtk_print_context_get_height (context)};
	cairo_t *cr;
	
	/* have to assume x and y resolutions are the same for now, since we
	   don't support differing scales in the gerb_render_info_t struct yet */
	gdouble xres = gtk_print_context_get_dpi_x (context);
	gdouble yres = gtk_print_context_get_dpi_y (context);
	gdouble scalePercentage = gtk_print_settings_get_scale (pSettings);
	renderInfo.scaleFactorX = scalePercentage / 100 * xres;
	renderInfo.scaleFactorY = scalePercentage / 100 * yres;

	gerbv_render_translate_to_fit_display (mainProject, &renderInfo);
	cr = gtk_print_context_get_cairo_context (context);
	gerbv_render_all_layers_to_cairo_target_for_vector_output (mainProject, cr, &renderInfo);
}

/* --------------------------------------------------------- */
void
callbacks_print_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	GtkPrintOperation *print;
	/*GtkPrintOperationResult res;*/

	print = gtk_print_operation_new ();

	g_signal_connect (print, "begin_print", G_CALLBACK (callbacks_begin_print), NULL);
	g_signal_connect (print, "draw_page", G_CALLBACK (callbacks_print_render_page), NULL);

	//GtkPrintSettings *pSettings = gtk_print_operation_get_print_settings (print);
	
	(void) gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                              (GtkWindow *) screen.win.topLevelWindow , NULL);

	g_object_unref (print);
}
#endif /* GTK_CHECK_VERSION(2,10,0) */

/* --------------------------------------------------------- */
void
callbacks_fullscreen_toggled (GtkMenuItem *menuitem, gpointer user_data)
{
	//struct GtkWindow *win = (struct GtkWindow *)(screen.win.topLevelWindow);
	GdkWindowState state = gdk_window_get_state (gtk_widget_get_window(screen.win.topLevelWindow));
	if(state & GDK_WINDOW_STATE_FULLSCREEN)
		gtk_window_unfullscreen (GTK_WINDOW(screen.win.topLevelWindow));
	else
		gtk_window_fullscreen (GTK_WINDOW(screen.win.topLevelWindow));
}

/* --------------------------------------------------------- */
void
callbacks_show_toolbar_toggled (GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_widget_set_visible (user_data, GTK_CHECK_MENU_ITEM(menuitem)->active);
}

/* --------------------------------------------------------- */
void
callbacks_show_sidepane_toggled (GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_widget_set_visible (user_data, GTK_CHECK_MENU_ITEM(menuitem)->active);
}

/* --------------------------------------------------------- */
/** View/"Toggle visibility layer X" or Current layer/"Toggle visibility" menu item was activated.
  * Set the isVisible flag on file X and update the treeview and rendering.
*/
void
callbacks_toggle_layer_visibility_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	int i = GPOINTER_TO_INT(user_data);

	switch (i) {
	case LAYER_SELECTED:
		i = callbacks_get_selected_row_index ();
		/* No break */
	default:
		if (0 <= i && i <= mainProject->last_loaded) {
			mainProject->file[i]->isVisible = !mainProject->file[i]->isVisible;
		} else {
			/* Not in range */
			return;
		}
		break;
	case LAYER_ALL_ON:
		for (i = 0; i <= mainProject->last_loaded; i++) {
			mainProject->file[i]->isVisible = TRUE;
		}
		break;
	case LAYER_ALL_OFF:
		for (i = 0; i <= mainProject->last_loaded; i++) {
			mainProject->file[i]->isVisible = FALSE;
		}
		break;
	}

	/* Clear any selected items so they don't show after the layer is hidden */
	render_clear_selection_buffer ();
	callbacks_update_layer_tree ();

	if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
		render_refresh_rendered_image_on_screen ();
	} else {
		render_recreate_composite_surface (screen.drawing_area);
		callbacks_force_expose_event_for_screen ();
	}
}

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
	gerbv_render_zoom_to_fit_display (mainProject, &screenRenderInfo);
	render_refresh_rendered_image_on_screen();
}


/* --------------------------------------------------------- */
/**
  * The analyze -> analyze Gerbers  menu item was selected.  
  * Compile statistics on all open Gerber layers and then display
  * them.
  *
  */
void
callbacks_analyze_active_gerbers_activate(GtkMenuItem *menuitem, 
				 gpointer user_data)
{
    gerbv_stats_t *stats_report;
    GString *G_report_string = g_string_new(NULL);
    GString *D_report_string = g_string_new(NULL);
    GString *M_report_string = g_string_new(NULL);
    GString *misc_report_string = g_string_new(NULL);
    GString *general_report_string = g_string_new(NULL);
    GString *error_report_string = g_string_new(NULL);
    gerbv_error_list_t *my_error_list;
    gchar *error_level = NULL;
    GString *aperture_def_report_string = g_string_new(NULL);
    GString *aperture_use_report_string = g_string_new(NULL);
    gerbv_aperture_list_t *my_aperture_list;
    int idx;
    int aperture_count = 0;

    /* First get a report of stats & errors accumulated from all layers */
    stats_report = generate_gerber_analysis();

    /* General info report */
    g_string_printf(general_report_string, 
		    _("General information\n"));
    g_string_append_printf(general_report_string, 
			   _("  Active layer count = %d\n"), 
			   stats_report->layer_count);
    g_string_append_printf(general_report_string,
			   "\n\n%-45s   %-10s\n",
			   _("Files processed"),
			   _("Layer number"));
    for (idx = 0; idx <= mainProject->last_loaded; idx++) {
	if (mainProject->file[idx] &&
	    mainProject->file[idx]->isVisible &&
	    (mainProject->file[idx]->image->layertype == GERBV_LAYERTYPE_RS274X) ) {
	    g_string_append_printf(general_report_string, 
				   "  %-45s   %-10d\n", mainProject->file[idx]->name, idx+1);
	}
    }

    /* Error report (goes into general report tab) */
    if (stats_report->layer_count == 0) {
	g_string_printf(error_report_string, 
			_("\n\nNo Gerber files active (visible)!\n"));
    } else if (stats_report->error_list->error_text == NULL) {
	g_string_printf(error_report_string, 
			_("\n\nNo errors found in active Gerber file(s)!\n")); 
    } else {
	g_string_printf(error_report_string, 
			_("\n\nErrors found in active Gerber file(s):\n")); 
	for(my_error_list = stats_report->error_list; 
	    my_error_list != NULL; 
	    my_error_list = my_error_list->next) {
	    switch(my_error_list->type) {
		case GERBV_MESSAGE_FATAL: /* We should never get this one since the 
                			   * program should terminate first.... */
		    error_level = g_strdup_printf(_("FATAL: "));
		    break;
		case GERBV_MESSAGE_ERROR:
		    error_level = g_strdup_printf(_("ERROR: "));
		    break;
		case GERBV_MESSAGE_WARNING:
		    error_level = g_strdup_printf(_("WARNING: "));
		    break;
		case GERBV_MESSAGE_NOTE:
		    error_level = g_strdup_printf(_("NOTE: "));
		    break;
	    }
	    g_string_append_printf(error_report_string,
				   _("  Layer %d: %s %s"), 
				   my_error_list->layer,
				   error_level,
				   my_error_list->error_text );
	    g_free(error_level);
	}
    }


    g_string_append_printf(general_report_string, 
			   "%s", 
			   error_report_string->str);
    g_string_free(error_report_string, TRUE);
    
    /* Now compile stats related to reading G codes */
    g_string_printf(G_report_string, 
		    _("G code statistics (all active layers)\n"));
    g_string_append_printf(G_report_string, 
			   _("<code> = <number of incidences>\n"));
    g_string_append_printf(G_report_string,
			   "G0 = %-6d (%s)\n", 
			   stats_report->G0,
			   _("Move"));
    g_string_append_printf(G_report_string,
			   "G1 = %-6d (%s)\n", 
			   stats_report->G1,
			   _("1X linear interpolation"));
    g_string_append_printf(G_report_string,
			   "G2 = %-6d (%s)\n", 
			   stats_report->G2,
			   _("CW interpolation"));
    g_string_append_printf(G_report_string,
			   "G3 = %-6d (%s)\n", 
			   stats_report->G3,
			   _("CCW interpolation"));
    g_string_append_printf(G_report_string,
			   "G4 = %-6d (%s)\n", 
			   stats_report->G4,
			   _("Comment/ignore block"));
    g_string_append_printf(G_report_string,
			   "G10 = %-6d (%s)\n", 
			   stats_report->G10,
			   _("10X linear interpolation"));
    g_string_append_printf(G_report_string,
			   "G11 = %-6d (%s)\n", 
			   stats_report->G11,
			   _("0.1X linear interpolation"));
    g_string_append_printf(G_report_string,
			   "G12 = %-6d (%s)\n", 
			   stats_report->G12,
			   _("0.01X linear interpolation"));
    g_string_append_printf(G_report_string,
			   "G36 = %-6d (%s)\n", 
			   stats_report->G36,
			   _("Poly fill on"));
    g_string_append_printf(G_report_string,
			   "G37 = %-6d (%s)\n", 
			   stats_report->G37,
			   _("Poly fill off"));
    g_string_append_printf(G_report_string,
			   "G54 = %-6d (%s)\n", 
			   stats_report->G54,
			   _("Tool prepare"));
    g_string_append_printf(G_report_string,
			   "G55 = %-6d (%s)\n", 
			   stats_report->G55,
			   _("Flash prepare"));
    g_string_append_printf(G_report_string,
			   "G70 = %-6d (%s)\n", 
			   stats_report->G70,
			   _("Units = inches"));
    g_string_append_printf(G_report_string,
			   "G71 = %-6d (%s)\n", 
			   stats_report->G71,
			   _("Units = mm"));
    g_string_append_printf(G_report_string,
			   "G74 = %-6d (%s)\n", 
			   stats_report->G74,
			   _("Disable 360 circ. interpolation"));
    g_string_append_printf(G_report_string,
			   "G75 = %-6d (%s)\n", 
			   stats_report->G75,
			   _("Enable 360 circ. interpolation"));
    g_string_append_printf(G_report_string,
			   "G90 = %-6d (%s)\n", 
			   stats_report->G90,
			   _("Absolute units"));
    g_string_append_printf(G_report_string,
			   "G91 = %-6d (%s)\n", 
			   stats_report->G91,
			   _("Incremental units"));
    g_string_append_printf(G_report_string,
			   _("Unknown G codes = %d\n"), 
			   stats_report->G_unknown);
    

    g_string_printf(D_report_string, _("D code statistics (all active layers)\n"));
    g_string_append_printf(D_report_string,
			   _("<code> = <number of incidences>\n"));
    g_string_append_printf(D_report_string,
			   "D1 = %-6d (%s)\n", 
			   stats_report->D1,
			   _("Exposure on"));
    g_string_append_printf(D_report_string,
			   "D2 = %-6d (%s)\n", 
			   stats_report->D2,
			   _("Exposure off"));
    g_string_append_printf(D_report_string, 
			   "D3 = %-6d (%s)\n", 
			   stats_report->D3,
			   _("Flash aperture"));
    g_string_append_printf(D_report_string, 
			   _("Undefined D codes = %d\n"), 
			   stats_report->D_unknown);
    g_string_append_printf(D_report_string,
			   _("D code Errors = %d\n"), 
			   stats_report->D_error);
    

    g_string_printf(M_report_string, _("M code statistics (all active layers)\n"));
    g_string_append_printf(M_report_string, 
			   _("<code> = <number of incidences>\n"));
    g_string_append_printf(M_report_string, 
			   "M0 = %-6d (%s)\n", 
			   stats_report->M0,
			   _("Program start"));
    g_string_append_printf(M_report_string, 
			   "M1 = %-6d (%s)\n", 
			   stats_report->M1,
			   _("Program stop"));
    g_string_append_printf(M_report_string, 
			   "M2 = %-6d (%s)\n", 
			   stats_report->M2,
			   _("Program end"));
    g_string_append_printf(M_report_string, 
			   _("Unknown M codes = %d\n"), 
			   stats_report->M_unknown);
    

    g_string_printf(misc_report_string, _("Misc code statistics (all active layers)\n"));
    g_string_append_printf(misc_report_string, 
			   _("<code> = <number of incidences>\n"));
    g_string_append_printf(misc_report_string, 
			   "X = %d\n", stats_report->X);
    g_string_append_printf(misc_report_string, 
			   "Y = %d\n", stats_report->Y);
    g_string_append_printf(misc_report_string, 
			   "I = %d\n", stats_report->I);
    g_string_append_printf(misc_report_string, 
			   "J = %d\n", stats_report->J);
    g_string_append_printf(misc_report_string, 
			   "* = %d\n", stats_report->star);
    g_string_append_printf(misc_report_string, 
			   _("Unknown codes = %d\n"), 
			   stats_report->unknown);
    
    /* Report apertures defined in input files. */

    if (stats_report->aperture_list->number == -1) {
	g_string_printf(aperture_def_report_string,
			_("No aperture definitions found in Gerber file(s)!\n")); 
    } else {
	g_string_printf(aperture_def_report_string,
			_("Apertures defined in Gerber file(s) (by layer)\n")); 
	g_string_append_printf(aperture_def_report_string, 
			" %-6s %-8s %12s  %8s %8s %8s\n",
			_("Layer"),
			_("D code"),
			_("Aperture"),
			_("Param[0]"),
			_("Param[1]"),
			_("Param[2]")
	    );
	for(my_aperture_list = stats_report->aperture_list; 
	    my_aperture_list != NULL; 
	    my_aperture_list = my_aperture_list->next) {

	    g_string_append_printf(aperture_def_report_string,
				   " %-6d    D%-4d%13s  %8.3f %8.3f %8.3f\n", 
				   my_aperture_list->layer,
				   my_aperture_list->number,
				   ap_names[my_aperture_list->type],
				   my_aperture_list->parameter[0],
				   my_aperture_list->parameter[1],
				   my_aperture_list->parameter[2]
		);
	}
    }

    /* Report apertures usage count in input files. */
    if (stats_report->D_code_list->number == -1) {
      g_string_printf(aperture_use_report_string,
		      _("No apertures used in Gerber file(s)!\n")); 
    } else {
    	
      /* Now add list of user-defined D codes (apertures) */
      
      g_string_printf(aperture_use_report_string,
		      _("Apertures used in Gerber file(s) (all active layers)\n")); 
      g_string_append_printf(aperture_use_report_string,
			     _("<aperture code> = <number of uses>\n"));
      for (my_aperture_list = stats_report->D_code_list; 
	   my_aperture_list != NULL; 
	   my_aperture_list = my_aperture_list->next) {
	
	   g_string_append_printf(aperture_use_report_string,
				  " D%d = %-6d\n",
				  my_aperture_list->number,
				  my_aperture_list->count
	       );
	   aperture_count += my_aperture_list->count;
      }
    }
    g_string_append_printf(aperture_use_report_string,
         _("\nTotal number of aperture uses: %d\n"), aperture_count);


    /* Create top level dialog window for report */
    GtkWidget *analyze_active_gerbers;
    analyze_active_gerbers = gtk_dialog_new_with_buttons(_("Gerber codes report"),
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
    GtkWidget *general_report_label = gtk_label_new (general_report_string->str);
    g_string_free (general_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(general_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(general_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(general_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(general_report_label),
			    font);
    /* Put general report text into scrolled window */
    GtkWidget *general_code_report_window = gtk_scrolled_window_new (NULL, NULL);
    /* This throws a warning.  Must find different approach.... */
    gtk_widget_set_size_request(GTK_WIDGET(general_code_report_window),
				200,
				300);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(general_code_report_window),
					  GTK_WIDGET(general_report_label));

    /* Create GtkLabel to hold G code text */
    GtkWidget *G_report_label = gtk_label_new (G_report_string->str);
    g_string_free (G_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(G_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(G_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(G_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(G_report_label),
			    font);

    /* Create GtkLabel to hold D code text */
    GtkWidget *D_report_label = gtk_label_new (D_report_string->str);
    g_string_free (D_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(D_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(D_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(D_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(D_report_label),
			    font);

    /* Create GtkLabel to hold M code text */
    GtkWidget *M_report_label = gtk_label_new (M_report_string->str);
    g_string_free (M_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(M_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(M_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(M_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(M_report_label),
			    font);

    /* Create GtkLabel to hold misc code text */
    GtkWidget *misc_report_label = gtk_label_new (misc_report_string->str);
    g_string_free (misc_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(misc_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(misc_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(misc_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(misc_report_label),
			    font);

    /* Create GtkLabel to hold aperture defintion text */
    GtkWidget *aperture_def_report_label = gtk_label_new (aperture_def_report_string->str);
    g_string_free (aperture_def_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(aperture_def_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(aperture_def_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(aperture_def_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(aperture_def_report_label),
			    font);
    /* Put aperture definintion text into scrolled window */
    GtkWidget *aperture_def_report_window = gtk_scrolled_window_new (NULL, NULL);
    /* This throws a warning.  Must find different approach.... */
    gtk_widget_set_size_request(GTK_WIDGET(aperture_def_report_window),
				200,
				300);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(aperture_def_report_window),
					  GTK_WIDGET(aperture_def_report_label));

    /* Create GtkLabel to hold aperture use text */
    GtkWidget *aperture_use_report_label = gtk_label_new (aperture_use_report_string->str);
    g_string_free (aperture_use_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(aperture_use_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(aperture_use_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(aperture_use_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(aperture_use_report_label),
			    font);
    /* Put aperture definintion text into scrolled window */
    GtkWidget *aperture_use_report_window = gtk_scrolled_window_new (NULL, NULL);
    /* This throws a warning.  Must find different approach.... */
    gtk_widget_set_size_request(GTK_WIDGET(aperture_use_report_window),
				200,
				300);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(aperture_use_report_window),
					  GTK_WIDGET(aperture_use_report_label));

    /* Create tabbed notebook widget and add report label widgets. */
    GtkWidget *notebook = gtk_notebook_new();
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(general_code_report_window),
			     gtk_label_new(_("General")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(G_report_label),
			     gtk_label_new(_("G codes")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(D_report_label),
			     gtk_label_new(_("D codes")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(M_report_label),
			     gtk_label_new(_("M codes")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(misc_report_label),
			     gtk_label_new(_("Misc. codes")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(aperture_def_report_window),
			     gtk_label_new(_("Aperture definitions")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(aperture_use_report_window),
			     gtk_label_new(_("Aperture usage")));
    
    
    /* Now put notebook into dialog window and show the whole thing */
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(analyze_active_gerbers)->vbox),
		      GTK_WIDGET(notebook));
    
    gtk_widget_show_all(analyze_active_gerbers);
    
    /* free the stats report */
    gerbv_stats_destroy (stats_report);	
    return;

}

/* --------------------------------------------------------- */
/**
  * The analyze -> analyze drill file  menu item was selected.  
  * Complie statistics on all open drill layers and then display
  * them.
  *
  */
void
callbacks_analyze_active_drill_activate(GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    gerbv_drill_stats_t *stats_report;
    GString *G_report_string = g_string_new(NULL);
    GString *M_report_string = g_string_new(NULL);
    GString *misc_report_string = g_string_new(NULL);
    gerbv_drill_list_t *my_drill_list;
    GString *drill_report_string = g_string_new(NULL);
    GString *general_report_string = g_string_new(NULL);
    GString *error_report_string = g_string_new(NULL);
    gerbv_error_list_t *my_error_list;
    gchar *error_level = NULL;
    int idx;

    stats_report = (gerbv_drill_stats_t *) generate_drill_analysis();
    
    /* General and error window strings */
    g_string_printf(general_report_string, _("General information\n"));
    g_string_append_printf(general_report_string, 
			   _("  Active layer count = %d\n"), 
			   stats_report->layer_count);
    
    g_string_append_printf(general_report_string, 
			   _("\n\nFiles processed:\n"));
    for (idx = mainProject->last_loaded; idx >= 0; idx--) {
	if (mainProject->file[idx] &&
	    mainProject->file[idx]->isVisible &&
	    (mainProject->file[idx]->image->layertype == GERBV_LAYERTYPE_DRILL) ) {
	    g_string_append_printf(general_report_string, 
				   "  %s\n", 
				   mainProject->file[idx]->name);
	}
    }


    if (stats_report->layer_count == 0) {
        g_string_printf(error_report_string, _("\n\nNo drill files active (visible)!\n")); 
    } else if (stats_report->error_list->error_text == NULL) {
	g_string_printf(error_report_string, 
			_("\n\nNo errors found in active drill file(s)!\n")); 
    } else {
	g_string_printf(error_report_string, 
			_("\n\nErrors found in active drill file(s):\n")); 
	for(my_error_list = stats_report->error_list; 
	    my_error_list != NULL; 
	    my_error_list = my_error_list->next) {
	    switch(my_error_list->type) {
		case GERBV_MESSAGE_FATAL: /* We should never get this one since the 
			     * program should terminate first.... */
		    error_level = g_strdup_printf(_("FATAL: "));
		    break;
		case GERBV_MESSAGE_ERROR:
		    error_level = g_strdup_printf(_("ERROR: "));
		    break;
		case GERBV_MESSAGE_WARNING:
		    error_level = g_strdup_printf(_("WARNING: "));
		    break;
		case GERBV_MESSAGE_NOTE:
		    error_level = g_strdup_printf(_("NOTE: "));
		    break;
	    }
	    g_string_append_printf(error_report_string, 
				   _("  Layer %d: %s %s"), 
				   my_error_list->layer,
				   error_level,
				   my_error_list->error_text);
	}
    }

    g_string_append_printf(general_report_string,
			   "%s", error_report_string->str);
    g_string_free(error_report_string, TRUE);


    /* G code window strings */
    g_string_printf(G_report_string, _("G code statistics (all active layers)\n"));
    g_string_append_printf(G_report_string, 
			   _("<code> = <number of incidences>\n"));
    g_string_append_printf(G_report_string, 
			   "G00 = %-6d (%s)\n", 
			   stats_report->G00,
			   _("Rout mode"));
    g_string_append_printf(G_report_string, 
			   "G01 = %-6d (%s)\n", 
			   stats_report->G01,
			   _("1X linear interpolation"));
    g_string_append_printf(G_report_string, 
			   "G02 = %-6d (%s)\n", 
			   stats_report->G02,
			   _("CW interpolation"));
    g_string_append_printf(G_report_string, 
			   "G03 = %-6d (%s)\n", 
			   stats_report->G03,
			   _("CCW interpolation"));
    g_string_append_printf(G_report_string, 
			   "G04 = %-6d (%s)\n", 
			   stats_report->G04,
			   _("Variable dwell"));
    g_string_append_printf(G_report_string, 
			   "G05 = %-6d (%s)\n", 
			   stats_report->G05,
			   _("Drill mode"));
    g_string_append_printf(G_report_string, 
			   "G90 = %-6d (%s)\n", 
			   stats_report->G90,
			   _("Absolute units"));
    g_string_append_printf(G_report_string, 
			   "G91 = %-6d (%s)\n", 
			   stats_report->G91,
			   _("Incremental units"));
    g_string_append_printf(G_report_string, 
			   "G93 = %-6d (%s)\n", 
			   stats_report->G93,
			   _("Zero set"));
    g_string_append_printf(G_report_string, 
			   _("Unknown G codes = %d\n"), 
			   stats_report->G_unknown);
    
    /* M code window strings */
    g_string_printf(M_report_string, _("M code statistics (all active layers)\n"));
    g_string_append_printf(M_report_string, 
			   _("<code> = <number of incidences>\n"));
    g_string_append_printf(M_report_string, 
			   "M00 = %-6d (%s)\n", 
			   stats_report->M00,
			   _("End of program"));
    g_string_append_printf(M_report_string, 
			   "M01 = %-6d (%s)\n", 
			   stats_report->M01,
			   _("End of pattern"));
    g_string_append_printf(M_report_string, 
			   "M18 = %-6d (%s)\n", 
			   stats_report->M18,
			   _("Tool tip check"));
    g_string_append_printf(M_report_string, 
			   "M25 = %-6d (%s)\n", 
			   stats_report->M25,
			   _("Begin pattern"));
    g_string_append_printf(M_report_string, 
			   "M30 = %-6d (%s)\n", 
			   stats_report->M30,
			   _("End program rewind"));
    g_string_append_printf(M_report_string, 
			   "M31 = %-6d (%s)\n", 
			   stats_report->M31,
			   _("Begin pattern"));
    g_string_append_printf(M_report_string, 
			   "M45 = %-6d (%s)\n", 
			   stats_report->M45,
			   _("Long message"));
    g_string_append_printf(M_report_string, 
			   "M47 = %-6d (%s)\n", 
			   stats_report->M47,
			   _("Operator message"));
    g_string_append_printf(M_report_string, 
			   "M48 = %-6d (%s)\n", 
			   stats_report->M48,
			   _("Begin program header"));
    g_string_append_printf(M_report_string, 
			   "M71 = %-6d (%s)\n", 
			   stats_report->M71,
			   _("Metric units"));
    g_string_append_printf(M_report_string, 
			   "M72 = %-6d (%s)\n", 
			   stats_report->M72,
			   _("English units"));
    g_string_append_printf(M_report_string, 
			   "M95 = %-6d (%s)\n", 
			   stats_report->M95,
			   _("End program header"));
    g_string_append_printf(M_report_string, 
			   "M97 = %-6d (%s)\n", 
			   stats_report->M97,
			   _("Canned text"));
    g_string_append_printf(M_report_string, 
			   "M98 = %-6d (%s)\n", 
			   stats_report->M98,
			   _("Canned text"));
    g_string_append_printf(M_report_string, 
			   _("Unknown M codes = %d\n"), 
			   stats_report->M_unknown);
    
    
    /* misc report strings */
    g_string_printf(misc_report_string, _("Misc code statistics (all active layers)\n"));
    g_string_append_printf(misc_report_string, 
			   _("<code> = <number of incidences>\n"));
    g_string_append_printf(misc_report_string, 
			   _("comments = %d\n"), 
			   stats_report->comment);
    g_string_append_printf(misc_report_string, 
			   _("Unknown codes = %d\n"), 
			   stats_report->unknown);
    
    g_string_append_printf(misc_report_string, 
			   "R = %-6d (%s)\n", 
			   stats_report->R,
			   _("Repeat hole"));

    if (stats_report->detect != NULL ) {
	g_string_append_printf(misc_report_string, 
			       "\n%s\n", 
			       stats_report->detect);
    }	
    /* drill report window strings */
    g_string_printf(drill_report_string, _("Drills used (all active layers)\n"));
    g_string_append_printf(drill_report_string, "%10s %8s %8s %8s\n", 
			   _("Drill no."), _("Dia."), _("Units"), _("Count"));
    for(my_drill_list = stats_report->drill_list; 
	my_drill_list != NULL; 
	my_drill_list = my_drill_list->next) {
	if (my_drill_list->drill_num == -1) break;  /* No drill list */
	g_string_append_printf(drill_report_string, 
			       "%10d %8.3f %8s %8d\n", 
			       my_drill_list->drill_num,
			       my_drill_list->drill_size,
			       my_drill_list->drill_unit,
			       my_drill_list->drill_count);
    }

    g_string_append_printf(drill_report_string, _("Total drill count %d\n"), 
			   stats_report->total_count);

    /* Use fixed width font for all reports */
    PangoFontDescription *font = 
	pango_font_description_from_string ("monospace");

    /* Create top level dialog window for report */
    GtkWidget *analyze_active_drill;
    analyze_active_drill = gtk_dialog_new_with_buttons(_("Drill file codes report"),
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
    GtkWidget *general_report_label = gtk_label_new (general_report_string->str);
    g_string_free(general_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(general_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(general_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(general_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(general_report_label),
			    font);

    /* Create GtkLabel to hold G code text */
    GtkWidget *G_report_label = gtk_label_new (G_report_string->str);
    g_string_free(G_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(G_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(G_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(G_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(G_report_label),
			    font);

    /* Create GtkLabel to hold M code text */
    GtkWidget *M_report_label = gtk_label_new (M_report_string->str);
    g_string_free(M_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(M_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(M_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(M_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(M_report_label),
			    font);

    /* Create GtkLabel to hold misc code text */
    GtkWidget *misc_report_label = gtk_label_new (misc_report_string->str);
    g_string_free(misc_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(misc_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(misc_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(misc_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(misc_report_label),
			    font);

    /* Create GtkLabel to hold drills used text */
    GtkWidget *drill_report_label = gtk_label_new (drill_report_string->str);
    g_string_free(drill_report_string, TRUE);
    gtk_misc_set_alignment(GTK_MISC(drill_report_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(drill_report_label), 13, 13);
    gtk_label_set_selectable(GTK_LABEL(drill_report_label), TRUE);
    gtk_widget_modify_font (GTK_WIDGET(drill_report_label),
			    font);

    /* Create tabbed notebook widget and add report label widgets. */
    GtkWidget *notebook = gtk_notebook_new();
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(general_report_label),
			     gtk_label_new(_("General")));

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(G_report_label),
			     gtk_label_new(_("G codes")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(M_report_label),
			     gtk_label_new(_("M codes")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(misc_report_label),
			     gtk_label_new(_("Misc. codes")));
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(drill_report_label),
			     gtk_label_new(_("Drills used")));
    
    /* Now put notebook into dialog window and show the whole thing */
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(analyze_active_drill)->vbox),
		      GTK_WIDGET(notebook));
    gtk_widget_show_all(analyze_active_drill);
    gerbv_drill_stats_destroy (stats_report);	
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
/**
  * The file -> quit menu item was selected or
  * the user requested the main window to be closed by other means.
  * Check that all changes have been saved, and then quit.
  *
  */
gboolean
callbacks_quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gboolean layers_dirty = FALSE;
  gint idx;

  for (idx = 0; idx<=mainProject->last_loaded; idx++) {
    if (mainProject->file[idx] == NULL) break;
    layers_dirty = layers_dirty || mainProject->file[idx]->layer_dirty;
  }

  if (layers_dirty &&
      !interface_get_alert_dialog_response(
            _("Do you want to close all open layers and quit the program?"),
            _("Quitting the program will cause any unsaved changes to be lost."),
	    FALSE,
	    NULL)) {
    return TRUE; // stop propagation of the delete_event.
	// this would destroy the gui but not return from the gtk event loop.
  }
  gerbv_unload_all_layers (mainProject);
  gtk_main_quit();
  return FALSE; // more or less... meaningless :)
}

/* --------------------------------------------------------- */
/**
  * The help -> about menu item was selected.  
  * Show the about dialog.
  *
  */
void
callbacks_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *aboutdialog1;
	/* TRANSLATORS: Replace this string with your names, one name per line. */
	gchar *translators = _("translator-credits");

	gchar *string = g_strdup_printf(_("gerbv -- a Gerber (RS-274/X) viewer.\n\n"
					  "This is gerbv version %s\n"
					  "Compiled on %s at %s\n"
					  "\n"
					  "gerbv is part of the gEDA Project.\n"
					  "\n"
					  "For more information see:\n"
					  "  gEDA homepage: http://geda-project.org/\n"
					  "  gEDA Wiki: http://wiki.geda-project.org/"),
					  VERSION, __DATE__, __TIME__);
#if GTK_CHECK_VERSION(2,6,0)
	gchar *license = g_strdup_printf(_("gerbv -- a Gerber (RS-274/X) viewer.\n\n"
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
					 "along with this program.  If not, see <http://www.gnu.org/licenses/>."));
	#include "authors.c"
	int a_size, i;
	gchar **a;

	aboutdialog1 = gtk_about_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (aboutdialog1), 5);
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (aboutdialog1), VERSION);
	gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (aboutdialog1), _("Gerbv"));

	gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (aboutdialog1), translators);
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (aboutdialog1), string);
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG (aboutdialog1), license);

	/* The authors.c file is autogenerated at build time */
	a_size = sizeof(authors_string_array)/sizeof(authors_string_array[0]);
	a = g_new(gchar *, a_size);
	for (i = 0; i < a_size; i++)
		a[i] = _(authors_string_array[i]);

	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG (aboutdialog1), (const gchar **)a);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG (aboutdialog1), "http://gerbv.geda-project.org/");
	g_free(a);

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

	gtk_window_set_title ( GTK_WINDOW (aboutdialog1), _("About Gerbv"));

	/* Destroy the dialog when the user responds to it (e.g. clicks a button) */
	g_signal_connect_swapped (aboutdialog1, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  aboutdialog1);
	g_free (string);
#endif

	gtk_widget_show_all(GTK_WIDGET(aboutdialog1));

}

/* --------------------------------------------------------- */
/**
  * The help -> bugs menu item was selected.  
  * Show the known bugs window
  *
  */
void
callbacks_bugs_activate (GtkMenuItem     *menuitem,
                             gpointer         user_data)
{
    int i;
    #include "bugs.c"

    /* Create the top level dialog widget with an OK button */
    GtkWidget *bugs_dialog = gtk_dialog_new_with_buttons(_("Known bugs in gerbv"),
							 NULL,
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
							 NULL);
    gtk_container_set_border_width (GTK_CONTAINER (bugs_dialog), 5);
    gtk_dialog_set_default_response (GTK_DIALOG(bugs_dialog),
				     GTK_RESPONSE_ACCEPT);
    g_signal_connect (G_OBJECT(bugs_dialog), "response",
		      G_CALLBACK (gtk_widget_destroy), GTK_WIDGET(bugs_dialog));
    
    /* First create single bugs_string from bugs_string_array */
    GString *bugs_string = g_string_new(NULL);
    for (i=0; bugs_string_array[i] != NULL; i++) {
	/* gettext("") will return info string */
	g_string_append_printf(bugs_string, "%s\n",
		(bugs_string_array[i][0] == '\0') ? "" : _(bugs_string_array[i]));
    }
    
    /* Create GtkLabel to hold text */
    GtkWidget *bugs_label = gtk_label_new (bugs_string->str);
    g_string_free(bugs_string, FALSE);
    gtk_misc_set_alignment(GTK_MISC(bugs_label), 0, 0);
    gtk_misc_set_padding(GTK_MISC(bugs_label), 13, 13);
    
    /* Put text into scrolled window */
    GtkWidget *bugs_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(bugs_window),
                                          GTK_WIDGET(bugs_label));
    gtk_widget_set_size_request(GTK_WIDGET(bugs_window), 600, 300);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(bugs_dialog)->vbox),
                      GTK_WIDGET(bugs_window));

    gtk_widget_show_all(GTK_WIDGET(bugs_dialog));
    gtk_dialog_run(GTK_DIALOG(bugs_dialog));
    
}

/* --------------------------------------------------------- */
gdouble callbacks_calculate_actual_distance (gdouble inputDimension) {
	return screen_units(inputDimension);
}

/* --------------------------------------------------------- */
void callbacks_update_ruler_pointers (void) {
	double xPosition, yPosition;
	xPosition = screenRenderInfo.lowerLeftX + (screen.last_x / screenRenderInfo.scaleFactorX);
	yPosition = screenRenderInfo.lowerLeftY + ((screenRenderInfo.displayHeight - screen.last_y) / screenRenderInfo.scaleFactorY);

	if (!((screen.unit == GERBV_MILS) && ((screenRenderInfo.scaleFactorX < 80)||(screenRenderInfo.scaleFactorY < 80)))) {
		xPosition = callbacks_calculate_actual_distance (xPosition);
		yPosition = callbacks_calculate_actual_distance (yPosition);
	}
	g_object_set (G_OBJECT (screen.win.hRuler), "position", xPosition, NULL);
	g_object_set (G_OBJECT (screen.win.vRuler), "position", yPosition, NULL);
}

/* --------------------------------------------------------- */
static void
callbacks_render_type_changed () {
	static gboolean isChanging = FALSE;
	if (isChanging)
		return;

	isChanging = TRUE;
	gerbv_render_types_t type = screenRenderInfo.renderType;
	GtkCheckMenuItem *check_item = screen.win.menu_view_render_group[type];
	dprintf ("%s():  type = %d, check_item = %p\n", __FUNCTION__, type, check_item);
	gtk_check_menu_item_set_active (check_item, TRUE);
	gtk_combo_box_set_active (screen.win.sidepaneRenderComboBox, type);

	render_refresh_rendered_image_on_screen();
	isChanging = FALSE;
}

/* --------------------------------------------------------- */
static void
callbacks_units_changed (gerbv_gui_unit_t unit) {
	static gboolean isChanging = FALSE;

	if (isChanging)
		return;

	isChanging = TRUE;
	screen.unit = unit;

	if (unit == GERBV_MILS){
		gtk_combo_box_set_active (GTK_COMBO_BOX (screen.win.statusUnitComboBox), GERBV_MILS);
		gtk_check_menu_item_set_active (screen.win.menu_view_unit_group[GERBV_MILS], TRUE);
	} else if (unit == GERBV_MMS){
		gtk_combo_box_set_active (GTK_COMBO_BOX (screen.win.statusUnitComboBox), GERBV_MMS);
		gtk_check_menu_item_set_active (screen.win.menu_view_unit_group[GERBV_MMS], TRUE);
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (screen.win.statusUnitComboBox), GERBV_INS);
		gtk_check_menu_item_set_active (screen.win.menu_view_unit_group[GERBV_INS], TRUE);
	}

	callbacks_update_ruler_scales ();
	callbacks_update_statusbar_coordinates (screen.last_x, screen.last_y);
	
	if (screen.tool == MEASURE)
		callbacks_update_statusbar_measured_distance (screen.win.lastMeasuredX, screen.win.lastMeasuredY);
	
	isChanging = FALSE;
}

/* --------------------------------------------------------- */
static void
callbacks_update_ruler_scales (void) {
	double xStart, xEnd, yStart, yEnd;

	xStart = screenRenderInfo.lowerLeftX;
	yStart = screenRenderInfo.lowerLeftY;
	xEnd = screenRenderInfo.lowerLeftX + (screenRenderInfo.displayWidth / screenRenderInfo.scaleFactorX);
	yEnd = screenRenderInfo.lowerLeftY + (screenRenderInfo.displayHeight / screenRenderInfo.scaleFactorY);
	/* mils can get super crowded with large boards, but inches are too
	   large for most boards.  So, we leave mils in for now and just switch
	   to inches if the scale factor gets too small */
	if (!((screen.unit == GERBV_MILS) && ((screenRenderInfo.scaleFactorX < 80)||(screenRenderInfo.scaleFactorY < 80)))) {
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

/* --------------------------------------------------------- */
void callbacks_update_scrollbar_limits (void){
	gerbv_render_info_t tempRenderInfo = {0, 0, 0, 0, 3, screenRenderInfo.displayWidth,
			screenRenderInfo.displayHeight};

	GtkAdjustment *hAdjust = (GtkAdjustment *)screen.win.hAdjustment;
	GtkAdjustment *vAdjust = (GtkAdjustment *)screen.win.vAdjustment;
	gerbv_render_zoom_to_fit_display (mainProject, &tempRenderInfo);
	hAdjust->lower = tempRenderInfo.lowerLeftX;
	hAdjust->page_increment = hAdjust->page_size;
	hAdjust->step_increment = hAdjust->page_size / 10.0;
	vAdjust->lower = tempRenderInfo.lowerLeftY;
	vAdjust->page_increment = vAdjust->page_size;
	vAdjust->step_increment = vAdjust->page_size / 10.0;
	hAdjust->upper = tempRenderInfo.lowerLeftX + (tempRenderInfo.displayWidth / tempRenderInfo.scaleFactorX);
	hAdjust->page_size = screenRenderInfo.displayWidth / screenRenderInfo.scaleFactorX;
	vAdjust->upper = tempRenderInfo.lowerLeftY + (tempRenderInfo.displayHeight / tempRenderInfo.scaleFactorY);
	vAdjust->page_size = screenRenderInfo.displayHeight / screenRenderInfo.scaleFactorY;
	callbacks_update_scrollbar_positions ();
}

/* --------------------------------------------------------- */
void callbacks_update_scrollbar_positions (void){
	gdouble positionX,positionY;

	positionX = screenRenderInfo.lowerLeftX;
	if (positionX < ((GtkAdjustment *)screen.win.hAdjustment)->lower)
		positionX = ((GtkAdjustment *)screen.win.hAdjustment)->lower;
	if (positionX > (((GtkAdjustment *)screen.win.hAdjustment)->upper - ((GtkAdjustment *)screen.win.hAdjustment)->page_size))
		positionX = (((GtkAdjustment *)screen.win.hAdjustment)->upper - ((GtkAdjustment *)screen.win.hAdjustment)->page_size);
	gtk_adjustment_set_value ((GtkAdjustment *)screen.win.hAdjustment, positionX);
	
	positionY = ((GtkAdjustment *)screen.win.vAdjustment)->upper - screenRenderInfo.lowerLeftY -
		((GtkAdjustment *)screen.win.vAdjustment)->page_size +
		((GtkAdjustment *)screen.win.vAdjustment)->lower;
	if (positionY < ((GtkAdjustment *)screen.win.vAdjustment)->lower)
		positionY = ((GtkAdjustment *)screen.win.vAdjustment)->lower;
	if (positionY > (((GtkAdjustment *)screen.win.vAdjustment)->upper - ((GtkAdjustment *)screen.win.vAdjustment)->page_size))
		positionY = (((GtkAdjustment *)screen.win.vAdjustment)->upper - ((GtkAdjustment *)screen.win.vAdjustment)->page_size);
	gtk_adjustment_set_value ((GtkAdjustment *)screen.win.vAdjustment, positionY);
}

/* --------------------------------------------------------- */
gboolean
callbacks_scrollbar_button_released (GtkWidget *widget, GdkEventButton *event){
	screen.off_x = 0;
	screen.off_y = 0;
	screen.state = NORMAL;
	render_refresh_rendered_image_on_screen();
	return FALSE;
}

/* --------------------------------------------------------- */
gboolean
callbacks_scrollbar_button_pressed (GtkWidget *widget, GdkEventButton *event){
	//screen.last_x = ((GtkAdjustment *)screen.win.hAdjustment)->value;
	screen.state = SCROLLBAR;
	return FALSE;
}

/* --------------------------------------------------------- */
void callbacks_hadjustment_value_changed (GtkAdjustment *adjustment, gpointer user_data){
	/* make sure we're actually using the scrollbar to make sure we don't reset
	   lowerLeftX during a scrollbar redraw during something else */
	if (screen.state == SCROLLBAR) {
		screenRenderInfo.lowerLeftX = gtk_adjustment_get_value (adjustment);
	}
}

/* --------------------------------------------------------- */
void callbacks_vadjustment_value_changed (GtkAdjustment *adjustment, gpointer user_data){
	/* make sure we're actually using the scrollbar to make sure we don't reset
	   lowerLeftY during a scrollbar redraw during something else */
	if (screen.state == SCROLLBAR) {
		screenRenderInfo.lowerLeftY = adjustment->upper -
			(gtk_adjustment_get_value (adjustment) + adjustment->page_size) + adjustment->lower;
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
	      gint *indices;
	      
	      indices = gtk_tree_path_get_indices (treePath);
	      index = indices[0];
		if (mainProject->file[index]->isVisible)
			 newVisibility = FALSE;
		mainProject->file[index]->isVisible = newVisibility;
		/* clear any selected items so they don't show after the layer is hidden */
		render_clear_selection_buffer();

	      callbacks_update_layer_tree ();
		if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
			render_refresh_rendered_image_on_screen();
		}
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
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

/* --------------------------------------------------------- */
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
			utf8_strncpy(screen.statusbar.diststr,
				_("Click to select objects in the current layer. Middle click and drag to pan."),
				MAX_DISTLEN);
			break;
		case PAN:
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonPan), TRUE);
			screen.tool = PAN;
			screen.state = NORMAL;
			utf8_strncpy(screen.statusbar.diststr,
				_("Click and drag to pan. Right click and drag to zoom."),
				MAX_DISTLEN);
			break;
		case ZOOM:
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonZoom), TRUE);
			screen.tool = ZOOM;
			screen.state = NORMAL;
			utf8_strncpy(screen.statusbar.diststr,
				_("Click and drag to zoom in. Shift+click to zoom out."),
				MAX_DISTLEN);
			break;
		case MEASURE:
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (screen.win.toolButtonMeasure), TRUE);
			screen.tool = MEASURE;
			screen.state = NORMAL;
			utf8_strncpy(screen.statusbar.diststr,
				_("Click and drag to measure a distance."),
				MAX_DISTLEN);
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
/**
  * This fcn returns the index of selected layer (selected in
  * the layer window on left).
  *
  */
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
	
	if ((index >= 0) && (index <= mainProject->last_loaded)) {
		render_remove_selected_objects_belonging_to_layer (index);
		gerbv_unload_layer (mainProject, index);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (0);

		if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
			render_refresh_rendered_image_on_screen();
		}
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
	}
}

/* --------------------------------------------------------- */
void
callbacks_move_layer_down_menu_activate (GtkMenuItem *menuitem, gpointer user_data) {
	callbacks_move_layer_down_button_clicked(NULL, NULL);
	gtk_widget_grab_focus (screen.win.layerTree);
}

/* --------------------------------------------------------- */
void
callbacks_move_layer_down_button_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	if (index < 0) {
		show_no_layers_warning ();
		return;
	}

	if (index < mainProject->last_loaded) {
		gerbv_change_layer_order (mainProject, index, index + 1);
	      callbacks_update_layer_tree ();
	      callbacks_select_row (index + 1);
		if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
			render_refresh_rendered_image_on_screen ();
		}
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
	}
}

/* --------------------------------------------------------- */
void
callbacks_move_layer_up_menu_activate (GtkMenuItem *menuitem, gpointer user_data) {
	callbacks_move_layer_up_button_clicked (NULL, NULL);
	gtk_widget_grab_focus (screen.win.layerTree);
}

/* --------------------------------------------------------- */
void
callbacks_move_layer_up_button_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	if (index < 0) {
		show_no_layers_warning ();
		return;
	}
	if (index > 0) {
		gerbv_change_layer_order (mainProject, index, index - 1);
		callbacks_update_layer_tree ();
		callbacks_select_row (index - 1);
		if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
			render_refresh_rendered_image_on_screen();
		}
		else {
			render_recreate_composite_surface (screen.drawing_area);
			callbacks_force_expose_event_for_screen ();
		}
	}
}

/* --------------------------------------------------------- */
void callbacks_layer_tree_row_inserted (GtkTreeModel *tree_model, GtkTreePath  *path,
                              GtkTreeIter  *oIter, gpointer user_data) {
	gint *indices=NULL,oldPosition,newPosition;
      
	if ((!screen.win.treeIsUpdating)&&(path != NULL)) {
		indices = gtk_tree_path_get_indices (path);
		if (indices) {
			newPosition = indices[0];
			oldPosition = callbacks_get_selected_row_index ();
			/* compensate for the fact that the old row has already
			   been removed */
			if (oldPosition < newPosition)
				newPosition--;
			else
				oldPosition--;
			gerbv_change_layer_order (mainProject, oldPosition, newPosition);

			if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
				render_refresh_rendered_image_on_screen();
			}
			else {
				render_recreate_composite_surface (screen.drawing_area);
				callbacks_force_expose_event_for_screen ();
			}
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

/* --------------------------------------------------------- */
void
callbacks_show_color_picker_dialog (gint index){
	screen.win.colorSelectionDialog = NULL;
	GtkColorSelectionDialog *cs= (GtkColorSelectionDialog *) gtk_color_selection_dialog_new (_("Select a color"));
	GtkColorSelection *colorsel = (GtkColorSelection *) cs->colorsel;
	
	screen.win.colorSelectionDialog = (GtkWidget *) cs;
	screen.win.colorSelectionIndex = index;
	if (index >= 0)
		gtk_color_selection_set_current_color (colorsel, &mainProject->file[index]->color);
	else
		gtk_color_selection_set_current_color (colorsel, &mainProject->background);
	if ((screenRenderInfo.renderType >= GERBV_RENDER_TYPE_CAIRO_NORMAL)&&(index >= 0)) {
		gtk_color_selection_set_has_opacity_control (colorsel, TRUE);
		gtk_color_selection_set_current_alpha (colorsel, mainProject->file[index]->alpha);
	}
	gtk_widget_show_all((GtkWidget *)cs);
	if (gtk_dialog_run ((GtkDialog*)cs) == GTK_RESPONSE_OK) {
		GtkColorSelection *colorsel = (GtkColorSelection *) cs->colorsel;
		gint rowIndex = screen.win.colorSelectionIndex;
		
		if (index >= 0) {
			gtk_color_selection_get_current_color (colorsel, &mainProject->file[rowIndex]->color);
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &mainProject->file[rowIndex]->color, FALSE, TRUE);
		}
		else {
			gtk_color_selection_get_current_color (colorsel, &mainProject->background);
			gdk_colormap_alloc_color(gdk_colormap_get_system(), &mainProject->background, FALSE, TRUE);
		}
		if ((screenRenderInfo.renderType >= GERBV_RENDER_TYPE_CAIRO_NORMAL)&&(index >= 0)) {
			mainProject->file[rowIndex]->alpha = gtk_color_selection_get_current_alpha (colorsel);
		}
		
		callbacks_update_layer_tree ();
		render_refresh_rendered_image_on_screen();
	}
	gtk_widget_destroy ((GtkWidget *)cs);
	screen.win.colorSelectionDialog = NULL;
}

/* --------------------------------------------------------- */
void
callbacks_invert_layer_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	if (index < 0) {
		show_no_layers_warning ();
		return;
	}
	mainProject->file[index]->transform.inverted = !mainProject->file[index]->transform.inverted;
	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree ();
}

/* --------------------------------------------------------- */
void
callbacks_change_layer_color_clicked  (GtkButton *button, gpointer   user_data) {
	gint index=callbacks_get_selected_row_index();
	if (index < 0) {
		show_no_layers_warning ();
		return;
	}
	callbacks_show_color_picker_dialog (index);
}

void
callbacks_change_background_color_clicked  (GtkButton *button, gpointer   user_data) {
	callbacks_show_color_picker_dialog (-1);
}

/* --------------------------------------------------------------------------- */					
void
callbacks_reload_layer_clicked  (GtkButton *button, gpointer   user_data) {
	gint index = callbacks_get_selected_row_index();
	if (index < 0) {
		show_no_layers_warning ();
		return;
	}
	render_remove_selected_objects_belonging_to_layer (index);
	gerbv_revert_file (mainProject, index);
	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree();
}

void
callbacks_change_layer_orientation_clicked  (GtkButton *button, gpointer userData){
	gint index = callbacks_get_selected_row_index();

	if (index < 0) {
		show_no_layers_warning ();
		return;
	}

	interface_show_modify_orientation_dialog(&mainProject->file[index]->transform,screen.unit);
	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree ();	
}

/* --------------------------------------------------------------------------- */
void
callbacks_change_layer_format_clicked  (GtkButton *button, gpointer   user_data)
{
    gerbv_HID_Attribute *attr = NULL;
    int n = 0;
    int i;
    gerbv_HID_Attr_Val * results = NULL;
    gint index = callbacks_get_selected_row_index();
    gchar *type;
    gint rc;
	if (index < 0) {
		show_no_layers_warning ();
		return;
	}
    dprintf ("%s(): index = %d\n", __FUNCTION__, index);
    attr = mainProject->file[index]->image->info->attr_list;
    n =  mainProject->file[index]->image->info->n_attr;
    type =  mainProject->file[index]->image->info->type;
    if (type == NULL) 
	type = N_("Unknown");

    if (attr == NULL || n == 0) 
	{
	  interface_show_alert_dialog(_("This file type does not currently have any editable features"), 
				      _("Format editing is currently only supported for Excellon drill file formats."),
				      FALSE,
				      NULL);
	  return;
	}

    dprintf ("%s(): n = %d, attr = %p\n", __FUNCTION__, n, attr);
    if (n > 0)
	{
	    if (mainProject->file[index]->layer_dirty) {
		rc = interface_get_alert_dialog_response (_("This layer has changed!"), 
							  _("Editing the file type will reload the layer, destroying your changes.  Click OK to edit the file type and destroy your changes, or Cancel to leave."),
							  TRUE,
							  NULL);
		if (rc == 0) return;  /* Return if user hit Cancel */
	    }

	    results = (gerbv_HID_Attr_Val *) malloc (n * sizeof (gerbv_HID_Attr_Val));
	    if (results == NULL)
		{
		    fprintf (stderr, "%s() -- malloc failed\n", __FUNCTION__);
		    exit (1);
		}
      
	    /* non-zero means cancel was picked */
	    if (attribute_interface_dialog (attr, n, results, 
					    _("Edit file format"), 
					    _(type)))
		{
		    return;
		}
          
    }

    dprintf (_("%s():  Reloading layer\n"), __FUNCTION__);
    gerbv_revert_file (mainProject, index);

    for (i = 0; i < n; i++)
	{
	    if (results[i].str_value)
		free (results[i].str_value);
	}
    
    if (results)
	free (results);
    render_refresh_rendered_image_on_screen();
    callbacks_update_layer_tree();
}

/* --------------------------------------------------------------------------- */
gboolean
callbacks_layer_tree_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	/* if space is pressed while a color picker icon is in focus,
	show the color picker dialog. */
	if(event->keyval == GDK_space){
		GtkTreeView *tree;
		GtkTreePath *path;
		GtkTreeViewColumn *col;
		gint *indices;
		gint idx;

		tree = (GtkTreeView *) screen.win.layerTree;
		gtk_tree_view_get_cursor (tree, &path, &col);
		if (path) {
			indices = gtk_tree_path_get_indices (path);
			if (indices) {
				idx = callbacks_get_col_number_from_tree_view_column (col);
				if ((idx == 1) && (indices[0] <= mainProject->last_loaded)){
					callbacks_show_color_picker_dialog (indices[0]);
				}
			}
			gtk_tree_path_free (path);
		}
	}
	/* by default propagate the key press */
	return FALSE;
}

/* --------------------------------------------------------------------------- */
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
		      	gint *indices;
		      	indices = gtk_tree_path_get_indices (path);
		      	if (indices) {
					columnIndex = callbacks_get_col_number_from_tree_view_column (column);
					if ((columnIndex == 1) && (indices[0] <= mainProject->last_loaded)){
						callbacks_show_color_picker_dialog (indices[0]);
						/* don't propagate the signal, since drag and drop can
					   	sometimes activated during color selection */
						return TRUE;
					}
				}
			}
		}
	}
	/* don't pop up the menu if we don't have any loaded files */
	else if ((event->button == 3)&&(mainProject->last_loaded >= 0)) {
		gtk_menu_popup(GTK_MENU(screen.win.layerTreePopupMenu), NULL, NULL, NULL, NULL, 
			   event->button, event->time);
	}
	/* always allow the click to propagate and make sure the line is activated */
	return FALSE;
}

/* --------------------------------------------------------------------------- */
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

		for (idx = 0; idx <= mainProject->last_loaded; idx++) {
			if (mainProject->file[idx]) {
				GdkPixbuf    *pixbuf,*blackPixbuf;
				guint32 color;
				
				unsigned char red, green, blue, alpha;
				
				red = (unsigned char) (mainProject->file[idx]->color.red * 255 / G_MAXUINT16) ;
				green = (unsigned char) (mainProject->file[idx]->color.green * 255 / G_MAXUINT16) ;
				blue = (unsigned char) (mainProject->file[idx]->color.blue *255 / G_MAXUINT16) ;
				alpha = (unsigned char) (mainProject->file[idx]->alpha * 255 / G_MAXUINT16) ;
				
				color = (red )* (256*256*256) + (green ) * (256*256) + (blue )* (256) + (alpha );
				pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 20, 15);
				gdk_pixbuf_fill (pixbuf, color);
				
				blackPixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 20, 15);
				color = (100 )* (256*256*256) + (100 ) * (256*256) + (100 )* (256) + (150 );
				gdk_pixbuf_fill (blackPixbuf, color);
				
				/* copy the color area into the black pixbuf */
				gdk_pixbuf_copy_area  (pixbuf, 1, 1, 18, 13, blackPixbuf, 1, 1);
				/* free the color buffer, since we don't need it anymore */
				g_object_unref(pixbuf);
                        
				gtk_list_store_append (list_store, &iter);
				
				gchar startChar[2],*modifiedCode;
				/* terminate the letter string */
				startChar[1] = 0;
				
				gint numberOfModifications = 0;
				if (mainProject->file[idx]->transform.inverted) {
					startChar[0] = 'I';
					numberOfModifications++;
				}
				if (mainProject->file[idx]->transform.mirrorAroundX ||
						mainProject->file[idx]->transform.mirrorAroundY) {
					startChar[0] = 'M';
					numberOfModifications++;
				}
				if ((fabs(mainProject->file[idx]->transform.translateX) > 0.000001) ||
						(fabs(mainProject->file[idx]->transform.translateY) > 0.000001)) {
					startChar[0] = 'T';
					numberOfModifications++;
				}
				if ((fabs(mainProject->file[idx]->transform.scaleX - 1) > 0.000001) ||
						(fabs(mainProject->file[idx]->transform.scaleY - 1) > 0.000001)) {
					startChar[0] = 'S';
					numberOfModifications++;
				}
				if ((fabs(mainProject->file[idx]->transform.rotation) > 0.000001)) {
					startChar[0] = 'R';
					numberOfModifications++;
				}
				if (numberOfModifications > 1)
					startChar[0] = '*';
				if (numberOfModifications == 0)
					modifiedCode = g_strdup ("");
				else
					modifiedCode = g_strdup (startChar);
				
				/* display any unsaved layers differently to show the user they are
				   unsaved */
				gchar *layerName;
				if (mainProject->file[idx]->layer_dirty == TRUE) {
				  /* The layer has unsaved changes in it. Show layer name in italics. */
				  layerName = g_strconcat ("*","<i>",mainProject->file[idx]->name,"</i>",NULL);
				}
				else
				    /* layer is clean.  Show layer name using normal font. */
				    layerName = g_strdup (mainProject->file[idx]->name);
				
				gtk_list_store_set (list_store, &iter,
						    0, mainProject->file[idx]->isVisible,
						    1, blackPixbuf,
						    2, layerName,
						    3, modifiedCode,
						    -1);
				g_free (layerName);
				g_free (modifiedCode);
				/* pixbuf has a refcount of 2 now, as the list store has added its own reference */
				g_object_unref(blackPixbuf);
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
		gboolean showItems = (mainProject->last_loaded >= 0);
		gtk_widget_set_sensitive (screen.win.curLayerMenuItem, showItems);
		gtk_widget_set_sensitive (screen.win.curAnalyzeMenuItem, showItems);
		gtk_widget_set_sensitive (screen.win.curEditMenuItem, showItems);
		gtk_widget_set_sensitive (screen.win.curFileMenuItem1, showItems);
		gtk_widget_set_sensitive (screen.win.curFileMenuItem2, showItems);
		gtk_widget_set_sensitive (screen.win.curFileMenuItem3, showItems);
		gtk_widget_set_sensitive (screen.win.curFileMenuItem4, showItems);
		gtk_widget_set_sensitive (screen.win.curFileMenuItem5, showItems);
		gtk_widget_set_sensitive (screen.win.curFileMenuItem6, showItems);
		gtk_widget_set_sensitive (screen.win.curFileMenuItem7, showItems);
		screen.win.treeIsUpdating = FALSE;
	}
}

/* --------------------------------------------------------------------------- */
void
callbacks_display_object_properties_clicked (GtkButton *button, gpointer user_data) {
	int i;
	gchar *layer_name;
	gchar *net_label;
	gboolean validAperture;
	double length = 0;

	gint index=callbacks_get_selected_row_index ();
	if (index < 0 || screen.selectionInfo.type == GERBV_SELECTION_EMPTY) {
		interface_show_alert_dialog(_("No object is currently selected"),
			_("Objects must be selected using the pointer tool before you can view the object properties."),
			FALSE,
			NULL);
		return;
	}
	
	for (i=0; i<screen.selectionInfo.selectedNodeArray->len; i++){
		gerbv_selection_item_t sItem = g_array_index (screen.selectionInfo.selectedNodeArray,
						  gerbv_selection_item_t, i);

		gerbv_net_t *net = sItem.net;
		gerbv_image_t *image = sItem.image;
		gboolean show_length;

		/* get the aperture definition for the selected item */
		if (net->aperture > 0) {
			validAperture = TRUE;
		} else {
			validAperture = FALSE;
		}

		/* Also get layer name specified in file by %LN directive
		* (if it exists).  */
		if (net->layer->name == NULL) {
			layer_name = g_strdup(_("<unnamed layer>"));
		} else {
			layer_name = g_strdup(net->layer->name);
		}

		if (net->label == NULL) {
			net_label = g_strdup(_("<unlabeled net>"));
		} else {
			net_label = g_strdup((gchar *)net->label);
		}
		if (net->interpolation == GERBV_INTERPOLATION_PAREA_START) {
			g_message (_("Object type: Polygon\n"));
		}
		else {
			switch (net->aperture_state){
				case GERBV_APERTURE_STATE_OFF:
					break;
				case GERBV_APERTURE_STATE_ON:
					if (i!=0) g_message ("\n");  /* Spacing for a pretty display */
					show_length = 0;
					switch (net->interpolation) {
						case GERBV_INTERPOLATION_x10 :
						case GERBV_INTERPOLATION_LINEARx01 :
						case GERBV_INTERPOLATION_LINEARx001 :
						case GERBV_INTERPOLATION_LINEARx1 :
							g_message (_("Object type: Line\n"));
							length = line_length(net->start_x, net->start_y, net->stop_x, net->stop_y);
							show_length = 1;
							break;
						case GERBV_INTERPOLATION_CW_CIRCULAR :
						case GERBV_INTERPOLATION_CCW_CIRCULAR :
							g_message (_("Object type: Arc\n"));
							if (net->cirseg->width == net->cirseg->height) {
								length = arc_length(net->cirseg->width,
										fabs(net->cirseg->angle1 - net->cirseg->angle2));
								show_length = 1;
							}

							break;
						default :
							g_message (_("Object type: Unknown\n"));
							break;
					}
					g_message (_("    Exposure: On\n"));
					if (validAperture) {
						aperture_report(image->aperture, net->aperture);
					}
					g_message (_("    Start location: (%g, %g)\n"),
							screen_units(net->start_x), screen_units(net->start_y));
					g_message (_("    Stop location: (%g, %g)\n"),
							screen_units(net->stop_x), screen_units(net->stop_y));
					if (show_length) {
						screen.length_sum += length;
						g_message (_("    Length: %g (sum: %g)\n"),
							screen_units(length), screen_units(screen.length_sum));
					}
					g_message (_("    Layer name: %s\n"), layer_name);
					g_message (_("    Net label: %s\n"), net_label);
					g_message (_("    In file: %s\n"), mainProject->file[index]->name);
					break;
				case GERBV_APERTURE_STATE_FLASH:
					if (i!=0) g_message ("\n");  /* Spacing for a pretty display */
					g_message (_("Object type: Flashed aperture\n"));
					if (validAperture) {
						aperture_report(image->aperture, net->aperture);
					}
					g_message (_("    Location: (%g, %g)\n"),
							screen_units(net->stop_x), screen_units(net->stop_y));
					g_message (_("    Layer name: %s\n"), layer_name);
					g_message (_("    Net label: %s\n"), net_label);
					g_message (_("    In file: %s\n"), mainProject->file[index]->name);
					break;
			}
		}
		g_free (net_label);
		g_free (layer_name);
	}
	/* Use separator for different report requests */
	g_message ("---------------------------------------\n");
}

void
callbacks_support_benchmark (gerbv_render_info_t *renderInfo) {
	int i;
	time_t start, now;
	GdkPixmap *renderedPixmap = gdk_pixmap_new (NULL, renderInfo->displayWidth,
								renderInfo->displayHeight, 24);
								
	// start by running the GDK (fast) rendering test
	i = 0;
	start = time(NULL);
	now = start;
	while( now - 30 < start) {
		i++;
		dprintf(_("Benchmark():  Starting redraw #%d\n"), i);
		gerbv_render_to_pixmap_using_gdk (mainProject, renderedPixmap, renderInfo, NULL, NULL);
		now = time(NULL);
		dprintf(_("Elapsed time = %ld seconds\n"), (long int) (now - start));
	}
	g_message(_("FAST (=GDK) mode benchmark: %d redraws in %ld seconds (%g redraws/second)\n"),
		      i, (long int) (now - start), (double) i / (double)(now - start));
	gdk_pixmap_unref(renderedPixmap);
	
	// run the cairo (normal) render mode
	i = 0;
	start = time(NULL);
	now = start;
	renderInfo->renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL;
	while( now - 30 < start) {
		i++;
		dprintf(_("Benchmark():  Starting redraw #%d\n"), i);
		cairo_surface_t *cSurface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
	                              renderInfo->displayWidth, renderInfo->displayHeight);
		cairo_t *cairoTarget = cairo_create (cSurface);
		gerbv_render_all_layers_to_cairo_target (mainProject, cairoTarget, renderInfo);
		cairo_destroy (cairoTarget);
		cairo_surface_destroy (cSurface);
		now = time(NULL);
		dprintf(_("Elapsed time = %ld seconds\n"), (long int) (now - start));
	}
	g_message(_("NORMAL (=Cairo) mode benchmark: %d redraws in %ld seconds (%g redraws/second)\n"),
		      i, (long int) (now - start), (double) i / (double)(now - start));
}

/* --------------------------------------------------------------------------- */
void
callbacks_benchmark_clicked (GtkButton *button, gpointer   user_data)
{
	// prepare render	size and options (canvas size width and height are last 2 variables)		
	gerbv_render_info_t renderInfo = {1.0, 1.0, 0, 0, 0, 640, 480};
	// autoscale the image for now...maybe we don't want to do this in order to
	//   allow benchmarking of different zoom levels?
	gerbv_render_zoom_to_fit_display (mainProject, &renderInfo);

	g_message(_("Full zoom benchmarks\n"));
	callbacks_support_benchmark (&renderInfo);

		   
	g_message(_("x5 zoom benchmarks\n"));
	renderInfo.lowerLeftX += (screenRenderInfo.displayWidth /
				screenRenderInfo.scaleFactorX) / 3.0;
	renderInfo.lowerLeftY += (screenRenderInfo.displayHeight /
				screenRenderInfo.scaleFactorY) / 3.0;			
	renderInfo.scaleFactorX *= 5;
	renderInfo.scaleFactorY *= 5;
	callbacks_support_benchmark (&renderInfo);
}

/* --------------------------------------------------------------------------- */
void
callbacks_edit_object_properties_clicked (GtkButton *button, gpointer   user_data){
}

/* --------------------------------------------------------------------------- */
void
callbacks_move_objects_clicked (GtkButton *button, gpointer   user_data){
	/* for testing, just hard code in some translations here */
	gerbv_image_move_selected_objects (screen.selectionInfo.selectedNodeArray, -0.050, 0.050);
	callbacks_update_layer_tree();
	render_clear_selection_buffer ();
	render_refresh_rendered_image_on_screen ();
}

/* --------------------------------------------------------------------------- */
void
callbacks_reduce_object_area_clicked  (GtkButton *button, gpointer user_data){
	/* for testing, just hard code in some parameters */
	gerbv_image_reduce_area_of_selected_objects (screen.selectionInfo.selectedNodeArray, 0.20, 3, 3, 0.01);
	render_clear_selection_buffer ();
	render_refresh_rendered_image_on_screen ();
}

/* --------------------------------------------------------------------------- */
void
callbacks_delete_objects_clicked (GtkButton *button, gpointer   user_data){
	if (screen.selectionInfo.type == GERBV_SELECTION_EMPTY) {
		interface_show_alert_dialog(_("No object is currently selected"),
		                        _("Objects must be selected using the pointer tool before they can be deleted."),
		                        FALSE,
		                        NULL);
		return;
	}

	gint index=callbacks_get_selected_row_index();
	if (index < 0)
		return;

	if (mainProject->check_before_delete == TRUE) {
		if (!interface_get_alert_dialog_response(
						     _("Do you want to permanently delete the selected objects?"),
						     _("Gerbv currently has no undo function, so this action cannot be undone. This action will not change the saved file unless you save the file afterwards."),
						     TRUE,
						     &(mainProject->check_before_delete)))
		return;
	}

	gerbv_image_delete_selected_nets (mainProject->file[index]->image,
				    screen.selectionInfo.selectedNodeArray); 
	render_refresh_rendered_image_on_screen ();
	/* Set layer_dirty flag to TRUE */
	mainProject->file[index]->layer_dirty = TRUE;
	callbacks_update_layer_tree();

	render_clear_selection_buffer ();
	callbacks_update_selected_object_message(FALSE);
}

/* --------------------------------------------------------------------------- */
gboolean
callbacks_drawingarea_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
	GdkDrawable *drawable = widget->window;
	
	gdk_drawable_get_size (drawable, &screenRenderInfo.displayWidth, &screenRenderInfo.displayHeight);

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
	if (screen.windowSurface)
		cairo_surface_destroy ((cairo_surface_t *)
			screen.windowSurface);

#if defined(WIN32) || defined(QUARTZ)
	cairo_t *cairoTarget = gdk_cairo_create (GDK_WINDOW(widget->window));
	
	screen.windowSurface = cairo_get_target (cairoTarget);
	/* increase surface reference by one so it isn't freed when the cairo_t
	   is destroyed next */
	screen.windowSurface = cairo_surface_reference (screen.windowSurface);
	cairo_destroy (cairoTarget);
#else
	screen.windowSurface = (gpointer) cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable),
                                          GDK_DRAWABLE_XID (drawable),
                                          GDK_VISUAL_XVISUAL (visual),
                                          screenRenderInfo.displayWidth,
                                          screenRenderInfo.displayHeight);
#endif
	/* if this is the first time, go ahead and call autoscale even if we don't
	   have a model loaded */
	if ((screenRenderInfo.scaleFactorX < 0.001)||(screenRenderInfo.scaleFactorY < 0.001)) {
		gerbv_render_zoom_to_fit_display (mainProject, &screenRenderInfo);
	}
	render_refresh_rendered_image_on_screen();
	return TRUE;
}

/* --------------------------------------------------------- */
gboolean
callbacks_drawingarea_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
		GdkPixmap *new_pixmap;
		GdkGC *gc = gdk_gc_new(widget->window);

		/*
		* Create a pixmap with default background
		*/
		new_pixmap = gdk_pixmap_new(widget->window,
					widget->allocation.width,
					widget->allocation.height,
					-1);

		gdk_gc_set_foreground(gc, &mainProject->background);

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
		if (screen.tool == MEASURE && screen.state != IN_MEASURE) {
			render_toggle_measure_line();
		}
 
		return FALSE;
	}

	cairo_t *cr;
	int width, height;
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

#if defined(WIN32) || defined(QUARTZ)
	/* FIXME */
	cr = gdk_cairo_create (GDK_WINDOW(widget->window));
#else      
	cairo_surface_t *buffert;
	
	buffert = (gpointer) cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY (drawable),
	                                          GDK_DRAWABLE_XID (drawable),
	                                          GDK_VISUAL_XVISUAL (visual),
	                                          event->area.width, event->area.height);
	cr = cairo_create (buffert);
#endif
	cairo_translate (cr, -event->area.x + screen.off_x, -event->area.y + screen.off_y);
	render_project_to_cairo_target (cr);
	cairo_destroy (cr);
#if !defined(WIN32) && !defined(QUARTZ)
	cairo_surface_destroy (buffert);
#endif

	if (screen.tool == MEASURE)
		render_toggle_measure_line();
	return FALSE;
}

/* Transforms screen coordinates to board ones */
static void
callbacks_screen2board(gdouble *X, gdouble *Y, gint x, gint y) {

	/* make sure we don't divide by zero (which is possible if the gui
	   isn't displayed yet */
	if ((screenRenderInfo.scaleFactorX > 0.001)||(screenRenderInfo.scaleFactorY > 0.001)) {
		*X = screenRenderInfo.lowerLeftX + (x / screenRenderInfo.scaleFactorX);
		*Y = screenRenderInfo.lowerLeftY + ((screenRenderInfo.displayHeight - y)
			/ screenRenderInfo.scaleFactorY);
	}
	else {
		*X = *Y = 0.0;
	}
}

/* --------------------------------------------------------- */
static void
callbacks_update_statusbar_coordinates (gint x, gint y) {
	gdouble X, Y;

	callbacks_screen2board(&X, &Y, x, y);
	if (screen.unit == GERBV_MILS) {
	    utf8_snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
		     _("(%8.2f, %8.2f)"),
		     COORD2MILS(X), COORD2MILS(Y));
	} else if (screen.unit == GERBV_MMS) {
	    utf8_snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
		     _("(%8.3f, %8.3f)"),
		     COORD2MMS(X), COORD2MMS(Y));
	} else {
	    utf8_snprintf(screen.statusbar.coordstr, MAX_COORDLEN,
		     _("(%4.5f, %4.5f)"),
		     COORD2MILS(X) / 1000.0, COORD2MILS(Y) / 1000.0);
	}
	callbacks_update_statusbar();
}

void
callbacks_update_selected_object_message (gboolean userTriedToSelect) {
	if (screen.tool != POINTER)
		return;

	gint selectionLength = screen.selectionInfo.selectedNodeArray->len;
	if ((selectionLength == 0)&&(userTriedToSelect)) {
		/* update status bar message to make sure the user knows
		   about needing to select the layer */
		gchar *str = g_new(gchar, MAX_DISTLEN);
		utf8_strncpy(str, _("No object selected. Objects can only be selected in the active layer."),
				MAX_DISTLEN - 7);
		utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN, "<b>%s</b>", str);
		g_free(str);
	}
	else if (selectionLength == 0) {
		utf8_strncpy(screen.statusbar.diststr,
			_("Click to select objects in the current layer. Middle click and drag to pan."),
			MAX_DISTLEN);
	}
	else if (selectionLength == 1) {
		utf8_strncpy(screen.statusbar.diststr,
			_("1 object is currently selected"),
			MAX_DISTLEN);
	}
	else {
		utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
			ngettext("%d object are currently selected",
				"%d objects are currently selected",
				selectionLength), selectionLength);
	}
	callbacks_update_statusbar();
}
			
/* --------------------------------------------------------- */
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
    		    screenRenderInfo.lowerLeftX -= ((x - screen.last_x) / screenRenderInfo.scaleFactorX);
		    screenRenderInfo.lowerLeftY += ((y - screen.last_y) / screenRenderInfo.scaleFactorY);
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
			render_toggle_measure_line();
			callbacks_screen2board(&(screen.measure_last_x), &(screen.measure_last_y),
							x, y);
			/* screen.last_[xy] are updated to move the ruler pointers */
			screen.last_x = x;
			screen.last_y = y;
			/* draw the new line and write the new distance */
			render_draw_measure_distance();
			break;
		}
		case IN_SELECTION_DRAG: {
			if (screen.last_x || screen.last_y)
				render_draw_selection_box_outline();
			screen.last_x = x;
			screen.last_y = y;
			render_draw_selection_box_outline();
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

/* --------------------------------------------------------- */
gboolean
callbacks_drawingarea_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	GdkCursor *cursor;
	
	switch (event->button) {
		case 1 :
			if (screen.tool == POINTER) {
				/* select */
				/* selection will only work with cairo, so do nothing if it's
				   not compiled */
				screen.state = IN_SELECTION_DRAG;
				screen.start_x = event->x;
				screen.start_y = event->y;
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
				callbacks_screen2board(&(screen.measure_start_x), &(screen.measure_start_y),
								event->x, event->y);
				screen.measure_last_x = screen.measure_start_x;
				screen.measure_last_y = screen.measure_start_y;
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
			if (screen.tool == POINTER) {
				/* if no items are selected, try and find the item the user
				   is pointing at */
				if (screen.selectionInfo.type == GERBV_SELECTION_EMPTY) {
					gint index=callbacks_get_selected_row_index();
					if ((index >= 0) && 
					    (index <= mainProject->last_loaded) &&
					    (mainProject->file[index]->isVisible)) {
					  render_fill_selection_buffer_from_mouse_click(event->x,event->y,index,TRUE);
					} else {
					    render_clear_selection_buffer ();
					    render_refresh_rendered_image_on_screen ();
					}
				}
				/* only show the popup if we actually have something selected now */
				if (screen.selectionInfo.type != GERBV_SELECTION_EMPTY)
					gtk_menu_popup(GTK_MENU(screen.win.drawWindowPopupMenu), NULL, NULL, NULL, NULL, 
			  	 		event->button, event->time);
			} else {
				/* Zoom outline mode initiated */
				screen.state = IN_ZOOM_OUTLINE;
				screen.start_x = event->x;
				screen.start_y = event->y;
				screen.centered_outline_zoom = event->state & GDK_SHIFT_MASK;
				cursor = gdk_cursor_new(GDK_SIZING);
				gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window),
						  cursor);
				gdk_cursor_destroy(cursor);
			}
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

/* --------------------------------------------------------- */
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
		else if (screen.state == IN_SELECTION_DRAG) {
			/* selection will only work with cairo, so do nothing if it's
			   not compiled */
			gint index=callbacks_get_selected_row_index();
			/* determine if this was just a click or a box drag */
			if ((index >= 0) && 
			    (mainProject->file[index]->isVisible)) {
				gboolean eraseOldSelection = TRUE;
				
				if ((event->state & GDK_SHIFT_MASK) ||
				   (event->state & GDK_CONTROL_MASK)) {
					eraseOldSelection = FALSE;
				}
				if ((fabs((double)(screen.last_x - screen.start_x)) < 5) &&
					 (fabs((double)(screen.last_y - screen.start_y)) < 5))
					render_fill_selection_buffer_from_mouse_click(event->x,event->y,index,eraseOldSelection);
				else
					render_fill_selection_buffer_from_mouse_drag(event->x,event->y,
						screen.start_x,screen.start_y,index,eraseOldSelection);
				/* check if anything was selected */
				callbacks_update_selected_object_message (TRUE);
			} else {
			    render_clear_selection_buffer ();
			    render_refresh_rendered_image_on_screen ();
			}
		}
		screen.last_x = screen.last_y = 0;
		screen.state = NORMAL;
	}
	return TRUE;
} /* button_release_event */

/* --------------------------------------------------------- */
gboolean
callbacks_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	switch(event->keyval) {
		case GDK_Escape:
			if (screen.tool == POINTER) {
				utf8_strncpy(screen.statusbar.diststr,
					_("No objects are currently selected"),
					MAX_DISTLEN);
		 		callbacks_update_statusbar();
		 		render_clear_selection_buffer ();
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

/* --------------------------------------------------------- */
gboolean
callbacks_window_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
	return TRUE;
} /* key_release_event */

/* --------------------------------------------------------- */
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
		gtk_label_set_markup(GTK_LABEL(screen.win.statusMessageRight), screen.statusbar.diststr);
	}
}

/* --------------------------------------------------------- */
void
callbacks_update_statusbar_measured_distance (gdouble dx, gdouble dy){
	gdouble delta = sqrt(dx*dx + dy*dy);
	
	if (screen.unit == GERBV_MILS) {
	    utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     _("Measured distance: %8.2f mils (%8.2f x, %8.2f y)"),
		     COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));
	} 
	else if (screen.unit == GERBV_MMS) {
	    utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     _("Measured distance: %8.3f mms (%8.3f x, %8.3f y)"),
		     COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));
	}
	else {
	    utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     _("Measured distance: %4.5f inches (%4.5f x, %4.5f y)"),
		     COORD2MILS(delta) / 1000.0, COORD2MILS(dx) / 1000.0,
		     COORD2MILS(dy) / 1000.0);
	}
	callbacks_update_statusbar();
}

/* --------------------------------------------------------- */
void
callbacks_sidepane_render_type_combo_box_changed (GtkComboBox *widget, gpointer user_data) {
	int type = gtk_combo_box_get_active (widget);
	
	dprintf ("%s():  type = %d\n", __FUNCTION__, type);

	if (type < 0 || type == screenRenderInfo.renderType)
		return;

	screenRenderInfo.renderType = type;
	callbacks_render_type_changed ();
}

/* --------------------------------------------------------- */
void
callbacks_viewmenu_rendertype_changed (GtkCheckMenuItem *widget, gpointer user_data) {
	gint type = GPOINTER_TO_INT(user_data);

	if (type == screenRenderInfo.renderType)
		return;

	dprintf ("%s():  type = %d\n", __FUNCTION__, type);

	screenRenderInfo.renderType = type;
	callbacks_render_type_changed ();
}

/* --------------------------------------------------------- */
void
callbacks_viewmenu_units_changed (GtkCheckMenuItem *widget, gpointer user_data) {
	gint unit = GPOINTER_TO_INT(user_data);

	if (unit < 0 || unit == screen.unit)
		return;

	dprintf ("%s():  unit = %d, screen.unit = %d\n", __FUNCTION__, unit, screen.unit);

	callbacks_units_changed (unit);
}

/* --------------------------------------------------------- */
void
callbacks_statusbar_unit_combo_box_changed (GtkComboBox *widget, gpointer user_data) {
	int unit = gtk_combo_box_get_active (widget);
	
	if (unit < 0 || unit == screen.unit)
		return;

	callbacks_units_changed (unit);
}

/* --------------------------------------------------------- */
void
callbacks_clear_messages_button_clicked  (GtkButton *button, gpointer   user_data) {
	GtkTextBuffer *textbuffer;
	GtkTextIter start, end;

	screen.length_sum = 0;

	textbuffer = gtk_text_view_get_buffer((GtkTextView*)screen.win.messageTextView);
	gtk_text_buffer_get_start_iter(textbuffer, &start);
	gtk_text_buffer_get_end_iter(textbuffer, &end);
	gtk_text_buffer_delete (textbuffer, &start, &end);
}
            
/* --------------------------------------------------------- */
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
	                                    "darkgreen_foreground");
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
		fprintf(stderr, _("Fatal error : %s\n"), message);

	gtk_text_buffer_insert(textbuffer, &iter, message, -1);

	gtk_text_buffer_get_end_iter(textbuffer, &iter);

	StopMark = gtk_text_buffer_create_mark(textbuffer,
					   "NewTextStop", &iter, TRUE);

	gtk_text_buffer_get_iter_at_mark(textbuffer, &StartIter, StartMark);
	gtk_text_buffer_get_iter_at_mark(textbuffer, &StopIter, StopMark);

	gtk_text_buffer_apply_tag(textbuffer, tag, &StartIter, &StopIter);
}

/* --------------------------------------------------------- */
void callbacks_force_expose_event_for_screen (void){

	GdkRectangle update_rect;
	
	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = screenRenderInfo.displayWidth;
	update_rect.height = screenRenderInfo.displayHeight;

	/* Calls expose_event */
	gdk_window_invalidate_rect (screen.drawing_area->window, &update_rect, FALSE);
	
	/* update other gui things that could have changed */
	callbacks_update_ruler_scales ();
	callbacks_update_scrollbar_limits ();
	callbacks_update_scrollbar_positions ();
}

static double screen_units(double d) {
	switch (screen.unit) {
	case GERBV_INS:
		break;
	case GERBV_MILS:
		return COORD2MILS(d);
		break;
	case GERBV_MMS:
		return COORD2MMS(d);
		break;
	}

	return d;
}

static double line_length(double x0, double y0, double x1, double y1) {
	double dx = x0 - x1;
	double dy = y0 - y1;

	return sqrt(dx*dx + dy*dy);
}

static double arc_length(double dia, double angle) {
	return M_PI*dia*(angle/360.0);
}

static void aperture_report(gerbv_aperture_t *apertures[], int aperture_num) {
	gerbv_aperture_type_t type = apertures[aperture_num]->type;
	double *params = apertures[aperture_num]->parameter;

	g_message (_("    Aperture used: D%d\n"), aperture_num);
	g_message (_("    Aperture type: %s\n"), ap_names[type]);

	switch (type) {
		case GERBV_APTYPE_CIRCLE:
			g_message (_("    Diameter: %g\n"),
				screen_units(params[0]));
			break;
		case GERBV_APTYPE_RECTANGLE:
		case GERBV_APTYPE_OVAL:
			g_message (_("    Dimensions: %gx%g\n"),
				screen_units(params[0]),
				screen_units(params[1]));
			break;
		default:
			break;
	}
}
