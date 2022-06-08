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

#include "gerbv.h"
#include "drill.h"

#if !defined(WIN32) && !defined(QUARTZ)
# include <gdk/gdkx.h>
#endif

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
#include "main.h"

#include "attribute.h"
#include "callbacks.h"
#include "interface.h"
#include "project.h"
#include "render.h"
#include "selection.h"
#include "table.h"

#include "draw-gdk.h"

#include "draw.h"
#ifdef WIN32
# include <cairo-win32.h>
#elif QUARTZ
# include <cairo-quartz.h>
#else
# include <cairo-xlib.h>
#endif


#define dprintf if(DEBUG) printf

/* This default extension should really not be changed, but if it absolutely
 * must change, the ../win32/gerbv.nsi.in *must* be changed to reflect that.
 * Just grep for the extension (gvp) and change it in two places in that file.
 */
#define GERBV_PROJECT_FILE_EXT ".gvp"
const char *gerbv_project_file_name = N_("Gerbv Project");
const char *gerbv_project_file_pat = "*" GERBV_PROJECT_FILE_EXT;

static gint callbacks_get_selected_row_index (void);
static void callbacks_units_changed (gerbv_gui_unit_t unit);
static void callbacks_update_statusbar_coordinates (gint x, gint y);
static void callbacks_update_ruler_scales (void);
static void callbacks_render_type_changed (void);
static void show_no_layers_warning (void);

static double screen_units(double);
static const char *screen_units_str(void);

static double line_length(double, double, double, double);
static double arc_length(double, double);

static void aperture_state_report (gerbv_net_t *,
		gerbv_image_t *, gerbv_project_t *);
static void aperture_report(gerbv_aperture_t *[], int,
		double, double, gerbv_image_t *, gerbv_project_t *);
static void drill_report(gerbv_aperture_t *[], int);
static void parea_report(gerbv_net_t *,
		gerbv_image_t *, gerbv_project_t *);
static void net_layer_file_report(gerbv_net_t *,
		gerbv_image_t *, gerbv_project_t *);
static void analyze_window_size_restore(GtkWidget *);
static void analyze_window_size_store(GtkWidget *, gpointer);

static void update_selected_object_message (gboolean userTriedToSelect);


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
	va_end(ap);
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
/**
  * The file -> new menu item was selected.  Create new
  * project.
  *
  */
void
callbacks_new_project_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	if (mainProject->last_loaded >= 0) {
		if (!interface_get_alert_dialog_response (
			_("Do you want to close any open layers "
			"and start a new project?"),
			_("Starting a new project will cause all currently "
			"open layers to be closed. Any unsaved changes "
			"will be lost."),
			FALSE, NULL, GTK_STOCK_CLOSE, GTK_STOCK_CANCEL))
			return;
	}
	/* Unload all layers and then clear layer window */
	gerbv_unload_all_layers (mainProject);
	callbacks_update_layer_tree ();
	selection_clear (&screen.selectionInfo);
	update_selected_object_message (FALSE);
	
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
void open_project(char *project_filename)
{

/* TODO: check if layers is modified and show it to user. */

	if (mainProject->last_loaded >= 0
	&&  !interface_get_alert_dialog_response (
		_("Do you want to close any open layers and load "
		"an existing project?"),
		_("Loading a project will cause all currently open "
		"layers to be closed. Any unsaved changes "
		"will be lost."),
		FALSE, NULL, GTK_STOCK_CLOSE, GTK_STOCK_CANCEL)) {

			return;
	}

	/* Update the last folder */
	g_free (mainProject->path);
	mainProject->path = g_strdup(project_filename);

	gerbv_unload_all_layers (mainProject);
	main_open_project_from_filename (mainProject, project_filename);
}


/* --------------------------------------------------------- */
/**
  * File -> open action requested
  * or file drop event happened.
  * Open a layer (or layers) or one Gerbv project from the files.
  * This function will show a question if the layer to be opened
  * is already open.
  */
void open_files(GSList *filenames)
{
	GSList *fns = NULL;		/* File names to ask */
	GSList *fns_is_mod = NULL;	/* File name layer is modified */
	GSList *fns_cnt = NULL;		/* File names count */
	GSList *fns_lay_num = NULL;	/* Layer number for fns */
	GSList *cnt = NULL;		/* File names count unsorted by layers,
					   0 -- file not yet loaded as layer */
	gint answer;

	if (filenames == NULL)
		return;

	/* Check if there is a Gerbv project in the list.
	 * If there is least open only that and ignore the rest. */
	for (GSList *fn = filenames; fn; fn = fn->next) {
		gboolean is_project = FALSE;
		if (0 == project_is_gerbv_project(fn->data, &is_project)
		&&  is_project) {
			open_project(fn->data);

			gerbv_render_zoom_to_fit_display(mainProject,
					&screenRenderInfo);
			render_refresh_rendered_image_on_screen();
			callbacks_update_layer_tree();

			return;
		}
	}

	/* Count opened filenames and place result in list */
	for (GSList *fn = filenames; fn; fn = fn->next) {
		gint c = 0;

		for (gint fidx = 0; fidx <= mainProject->last_loaded; ++fidx) {
			gchar *fpn = mainProject->file[fidx]->fullPathname;

			if (strlen(fpn) == strlen(fn->data)
			&&  0 == g_ascii_strncasecmp(fpn, fn->data,
						strlen(fn->data))) {
				c++;
			}
		}

		cnt = g_slist_append(cnt, GINT_TO_POINTER(c));
	}

	/* Make fns, fns_is_mod and fns_cnt lists sorted by layers */
	for (gint fidx = 0; fidx <= mainProject->last_loaded; ++fidx) {
		gchar *fpn = mainProject->file[fidx]->fullPathname;

		for (GSList *fn = filenames; fn; fn = fn->next) {
			if (strlen(fpn) == strlen(fn->data)
			&&  0 == g_ascii_strncasecmp(fpn, fn->data,
					strlen(fn->data))) {
				fns = g_slist_append(fns, fn->data);
				fns_is_mod = g_slist_append(fns_is_mod,
						GINT_TO_POINTER(mainProject->
							file[fidx]->
							layer_dirty));
				fns_cnt = g_slist_append(fns_cnt,
						g_slist_nth_data(cnt,
							g_slist_position(
								filenames,
								fn)));
				fns_lay_num = g_slist_append(fns_lay_num,
						GINT_TO_POINTER(fidx));

				break;
			}
		}
	}

	answer = GTK_RESPONSE_NONE;
	if (g_slist_length(fns) > 0)
		answer = interface_reopen_question(fns, fns_is_mod,
							fns_cnt, fns_lay_num);

	switch (answer) {

	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
		/* Dialog is closed or Esc is pressed, skip all */
		break;

	case GTK_RESPONSE_YES: /* Reload layer was selected */
		for (GSList *fn = fns; fn; fn = fn->next) {
			if (fn->data != NULL)
				gerbv_revert_file(mainProject,
					GPOINTER_TO_INT(
						g_slist_nth_data (fns_lay_num,
							g_slist_position (fns,
									fn))));
		}
		break;

	case GTK_RESPONSE_OK: /* Open as a new layer was selected */
		/* To open as new only _one_ instance of file, check filenames
		 * by selected files in fns */
		for (GSList *fn = filenames; fn; fn = fn->next) {
			if (NULL != g_slist_find (fns, fn->data))
				gerbv_open_layer_from_filename(mainProject,
								fn->data);
		}
		break;
	}

	/* Add not loaded files (cnt == 0) in the end */
	for (GSList *fn = filenames; fn; fn = fn->next) {
		if (0 == GPOINTER_TO_INT(g_slist_nth_data(cnt,
					g_slist_position(filenames, fn))))
			gerbv_open_layer_from_filename (mainProject, fn->data);
	}

	g_slist_free(fns);
	g_slist_free(fns_is_mod);
	g_slist_free(fns_cnt);
	g_slist_free(fns_lay_num);
	g_slist_free(cnt);

	gerbv_render_zoom_to_fit_display (mainProject, &screenRenderInfo);
	render_refresh_rendered_image_on_screen();
	callbacks_update_layer_tree();
}

/* --------------------------------------------------------- */
/**
  * The file -> open action was selected.  Open a
  * layer (or layers) or a project file.
  *
  */
void
callbacks_open_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GSList *fns = NULL;
	screen.win.gerber = 
		gtk_file_chooser_dialog_new (
				_("Open Gerbv project, Gerber, drill, "
				"or pick&place files"),
			NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_select_multiple(
			(GtkFileChooser *)screen.win.gerber, TRUE);
	gtk_file_chooser_set_current_folder(
			(GtkFileChooser *)screen.win.gerber, mainProject->path);
	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run ((GtkDialog*)screen.win.gerber) ==
			GTK_RESPONSE_ACCEPT) {
		fns = gtk_file_chooser_get_filenames(
				GTK_FILE_CHOOSER (screen.win.gerber));
		/* Update the last folder */
		g_free (mainProject->path);
		mainProject->path = gtk_file_chooser_get_current_folder(
				(GtkFileChooser *)screen.win.gerber);
	}
	gtk_widget_destroy (screen.win.gerber);

	open_files (fns);
	g_slist_free_full (fns, g_free);
}

/* --------------------------------------------------------- */
void
callbacks_revert_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	gerbv_revert_all_files (mainProject);
	selection_clear (&screen.selectionInfo);
	update_selected_object_message (FALSE);
	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree ();
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
	gerbv_image_t *out;
	gerbv_layertype_t layertype;
	struct l_image_info {
		gerbv_image_t *image;
		gerbv_user_transformation_t *transform;
	} *images;
	
	images=(struct l_image_info *)g_new0(struct l_image_info,1);
	out = NULL;
	switch(type) {
	case CALLBACKS_SAVE_FILE_DRILLM:
		layertype = GERBV_LAYERTYPE_DRILL;
		break;
	case CALLBACKS_SAVE_FILE_RS274XM:
		layertype = GERBV_LAYERTYPE_RS274X;
		break;
	default:
		GERB_COMPILE_ERROR(_("Unknown Layer type for merge"));
		goto err;
	}
	dprintf("Looking for matching files\n");
	for (i = img = filecount = 0; i < mainProject->max_files; ++i) {
		if (mainProject->file[i] &&  mainProject->file[i]->isVisible &&
		(mainProject->file[i]->image->layertype == layertype)) {
			++filecount;
			dprintf("Adding '%s'\n", mainProject->file[i]->name);
			images[img].image=mainProject->file[i]->image;
			images[img++].transform=&mainProject->file[i]->transform;
			images = (struct l_image_info *)g_renew(struct l_image_info, images, img+1);
		}
	}
	if (filecount < 2) {
		GERB_COMPILE_ERROR(_("Not Enough Files of same type to merge"));
		goto err;
	}
	dprintf("Now merging files\n");
	for (i = 0; i < img; ++i) {
		gerbv_user_transformation_t *thisTransform;
		gerbv_user_transformation_t identityTransform = {0,0,1,1,0,FALSE,FALSE,FALSE};
		thisTransform=images[i].transform;
		if (NULL == thisTransform)
			thisTransform = &identityTransform;
		if (0 == i)
			out = gerbv_image_duplicate_image(images[i].image, thisTransform);
		else
			gerbv_image_copy_image(images[i].image, thisTransform, out);
	}
err:
	g_free(images);
	return out;
}

/* --------------------------------------------------------- */
int
visible_file_name(gchar **file_name, gchar **dir_name,
		gerbv_layertype_t layer_type, /* -1 for all types */
		const gchar *file_extension,
		const gchar *untitled_file_extension)
{
	unsigned int count = 0;
	gerbv_fileinfo_t *first_vis_file = NULL;

	for (int i = 0; i < mainProject->max_files; ++i) {
		if (mainProject->file[i]
		&&  mainProject->file[i]->isVisible
		&&  (layer_type == (gerbv_layertype_t)-1
		  || layer_type == mainProject->file[i]->image->layertype)) {
			if (first_vis_file == NULL) {
				first_vis_file = mainProject->file[i];
				/* Always directory of first visible file */
				if (dir_name)
					*dir_name = g_path_get_dirname (
						first_vis_file->fullPathname);
			}

			if (++count == 2 && file_name) {
				*file_name = g_strdup_printf("%s%s",
						pgettext("file name",
							"untitled"),
						untitled_file_extension);
			}
		}
	}

	if (count == 1 && file_name)
		*file_name = g_strdup_printf("%s%s",
			first_vis_file->name, file_extension);

	return count;
}

/* --------------------------------------------------------- */
void
callbacks_generic_save_activate (GtkMenuItem     *menuitem,
				 gpointer         user_data)
{
	gchar *new_file_name = NULL;
	gchar *file_name = NULL;
	gchar *dir_name = NULL;
	gboolean error_visible_layers = FALSE;
	gchar *windowTitle = NULL;
	gerbv_fileinfo_t *act_file;
	gint file_index;
	gint processType = GPOINTER_TO_INT (user_data);
	GtkFileFilter *filter;
	GtkSpinButton *spin_but;
	GtkTooltips *tooltips;
	GtkWidget *label;
	GtkWidget *hbox;
	static gint dpi = 0;
	
	file_index = callbacks_get_selected_row_index ();
	if (file_index < 0) {
		interface_show_alert_dialog (_("No layer is currently active"),
			_("Please select a layer and try again."),
			FALSE,
			NULL);
		return;
	}

	act_file = mainProject->file[file_index];

	screen.win.gerber = gtk_file_chooser_dialog_new ("", NULL,
			GTK_FILE_CHOOSER_ACTION_SAVE, NULL, NULL, NULL);
	GtkFileChooser *file_chooser_p =
			GTK_FILE_CHOOSER(screen.win.gerber);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser_p, TRUE);

	hbox = gtk_hbox_new (0, 0);
	spin_but = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range (0, 0, 1));
	label = gtk_label_new ("");
	tooltips = gtk_tooltips_new ();
	gtk_box_pack_end (GTK_BOX(hbox), GTK_WIDGET(spin_but), 0, 0, 1);
	gtk_box_pack_end (GTK_BOX(hbox), label, 0, 0, 5);
	gtk_box_pack_end (GTK_BOX(GTK_DIALOG(screen.win.gerber)->vbox),
			hbox, 0, 0, 2);

	switch (processType) {
	case CALLBACKS_SAVE_PROJECT_AS:
		windowTitle = g_strdup (_("Save project as..."));
		if (mainProject->project) {
			file_name = g_path_get_basename (mainProject->project);

			dir_name = g_path_get_dirname (mainProject->project);
		} else {
			file_name = g_strdup_printf("%s%s",
					pgettext("file name", "untitled"),
					GERBV_PROJECT_FILE_EXT);
			dir_name= g_path_get_dirname (act_file->fullPathname);
		}

		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _(gerbv_project_file_name));
		gtk_file_filter_add_pattern (filter, gerbv_project_file_pat);
		gtk_file_chooser_add_filter (file_chooser_p, filter);

		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("All"));
		gtk_file_filter_add_pattern (filter, "*");
		gtk_file_chooser_add_filter (file_chooser_p, filter);

		break;
	case CALLBACKS_SAVE_FILE_PS:
		windowTitle = g_strdup_printf (
				_("Export visible layers to %s file as..."),
				_("PS"));
		if (0 == visible_file_name(&file_name, &dir_name, -1,
							".ps", ".ps")) {
			error_visible_layers = TRUE;
			break;
		}


		break;
	case CALLBACKS_SAVE_FILE_PDF:
		windowTitle = g_strdup_printf (
				_("Export visible layers to %s file as..."),
				_("PDF"));
		if (0 == visible_file_name(&file_name, &dir_name, -1,
							".pdf", ".pdf")) {
			error_visible_layers = TRUE;
			break;
		}


		break;
	case CALLBACKS_SAVE_FILE_SVG:
		windowTitle = g_strdup_printf (
				_("Export visible layers to %s file as..."),
				_("SVG"));
		if (0 == visible_file_name(&file_name, &dir_name, -1,
							".svg", ".svg")) {
			error_visible_layers = TRUE;
			break;
		}


		break;
	case CALLBACKS_SAVE_FILE_DXF:
#if HAVE_LIBDXFLIB
		windowTitle = g_strdup_printf (
			_("Export \"%s\" layer #%d to DXF file as..."),
			act_file->name, file_index + 1);
		file_name = g_strconcat (act_file->name, ".dxf", NULL);
		dir_name =  g_path_get_dirname (act_file->fullPathname);
#endif
		break;
	case CALLBACKS_SAVE_FILE_PNG:
		windowTitle = g_strdup_printf (
				_("Export visible layers to %s file as..."),
				_("PNG"));
		if (0 == visible_file_name(&file_name, &dir_name, -1,
							".png", ".png")) {
			error_visible_layers = TRUE;
			break;
		}

		gtk_label_set_text (GTK_LABEL(label), _("DPI:"));
		gtk_spin_button_set_range (spin_but, 0, 6000);
		gtk_spin_button_set_increments (spin_but, 10, 100);
		gtk_tooltips_set_tip (tooltips, GTK_WIDGET(label),
				_("DPI value, autoscaling if 0"), NULL);
		gtk_tooltips_set_tip (tooltips, GTK_WIDGET(spin_but),
				_("DPI value, autoscaling if 0"), NULL);
		gtk_spin_button_set_value (spin_but, dpi);
		gtk_widget_show_all (hbox);

		break;
	case CALLBACKS_SAVE_FILE_RS274X:
		windowTitle = g_strdup_printf(
			_("Export \"%s\" layer #%d to "
				"RS-274X file as..."),
			act_file->name, file_index+1);

		if (GERBV_LAYERTYPE_RS274X != act_file->image->layertype)
			file_name = g_strconcat (act_file->name, ".gbr", NULL);
		else
			file_name = g_strdup (act_file->name);

		dir_name =  g_path_get_dirname (act_file->fullPathname);
		break;
	case CALLBACKS_SAVE_FILE_RS274XM:
		windowTitle = g_strdup (_("Export merged visible layers to "
					"RS-274X file as..."));
		if (2 > visible_file_name(&file_name, &dir_name,
					GERBV_LAYERTYPE_RS274X, "", ".gbr")) {
			error_visible_layers = TRUE;
			break;
		}
	case CALLBACKS_SAVE_FILE_DRILL:
		windowTitle = g_strdup_printf(
			_("Export \"%s\" layer #%d to "
				"Excellon drill file as..."),
			act_file->name, file_index+1);

		if (GERBV_LAYERTYPE_DRILL != act_file->image->layertype)
			file_name = g_strconcat (act_file->name, ".drl", NULL);
		else
			file_name = g_strdup (act_file->name);

		dir_name =  g_path_get_dirname (act_file->fullPathname);
		break;
	case CALLBACKS_SAVE_FILE_DRILLM:
		windowTitle = g_strdup (_("Export merged visible layers to "
					"Excellon drill file as..."));
		if (2 > visible_file_name(&file_name, &dir_name,
				GERBV_LAYERTYPE_DRILL, "", ".drl")) {
			error_visible_layers = TRUE;
		}
		break;
	case CALLBACKS_SAVE_FILE_IDRILL:
		windowTitle = g_strdup_printf(
			_("Export \"%s\" layer #%d to ISEL NCP drill file as..."),
			act_file->name, file_index+1);
		file_name = g_strconcat (act_file->name, ".ncp", NULL);
		dir_name =  g_path_get_dirname (act_file->fullPathname);

		break;
	case CALLBACKS_SAVE_FILE_GEDA_PCB:
		windowTitle = g_strdup_printf (
			_("Export \"%s\" layer #%d to gEDA PCB file as..."),
			act_file->name, file_index + 1);
		file_name = g_strconcat (act_file->name, ".pcb", NULL);
		dir_name =  g_path_get_dirname (act_file->fullPathname);
		break;
	case CALLBACKS_SAVE_LAYER_AS:
		windowTitle = g_strdup_printf (_("Save \"%s\" layer #%d as..."),
				act_file->name, file_index+1);
		file_name = g_strdup (act_file->name);
		dir_name =  g_path_get_dirname (act_file->fullPathname);
		break;
	}

	if (file_name != NULL) {
		gtk_file_chooser_set_current_name (file_chooser_p, file_name);
		g_free (file_name);
	}
	if (dir_name != NULL) {
		gtk_file_chooser_set_current_folder (file_chooser_p, dir_name);
		g_free (dir_name);
	}

	gtk_dialog_add_buttons (GTK_DIALOG(screen.win.gerber),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE,   GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_window_set_title (GTK_WINDOW(screen.win.gerber), windowTitle);
	g_free (windowTitle);

	if (error_visible_layers) {
		switch (processType) {
		case CALLBACKS_SAVE_FILE_RS274XM:
			interface_get_alert_dialog_response (
				_("Not enough Gerber layers are visible"),
				_("Two or more Gerber layers must be visible "
				"for export with merge."),
				FALSE, NULL, NULL, GTK_STOCK_CANCEL);
			break;
		case CALLBACKS_SAVE_FILE_DRILLM:
			interface_get_alert_dialog_response (
				_("Not enough Excellon layers are visible"),
				_("Two or more Excellon layers must be visible "
				"for export with merge."),
				FALSE, NULL, NULL, GTK_STOCK_CANCEL);
			break;
		default:
			interface_get_alert_dialog_response (
				_("No layers are visible"), _("One or more "
				"layers must be visible for export."),
				FALSE, NULL, NULL, GTK_STOCK_CANCEL);
		}

		gtk_widget_destroy (screen.win.gerber);
		callbacks_update_layer_tree ();

		return;
	}

	gtk_widget_show (screen.win.gerber);
	if (gtk_dialog_run (GTK_DIALOG(screen.win.gerber)) == GTK_RESPONSE_ACCEPT) {
		new_file_name = gtk_file_chooser_get_filename (file_chooser_p);
		dpi = gtk_spin_button_get_value_as_int (spin_but);
	}
	gtk_widget_destroy (screen.win.gerber);

	if (!new_file_name) {
		callbacks_update_layer_tree ();

		return;
	}

	switch (processType) {
	case CALLBACKS_SAVE_PROJECT_AS:
		main_save_as_project_from_filename (mainProject, new_file_name);
		rename_main_window(new_file_name, NULL);
		break;
	case CALLBACKS_SAVE_FILE_PS:
		gerbv_export_postscript_file_from_project_autoscaled (
				mainProject, new_file_name);
		break;
	case CALLBACKS_SAVE_FILE_PDF:
		gerbv_export_pdf_file_from_project_autoscaled (
				mainProject, new_file_name);
		break;
	case CALLBACKS_SAVE_FILE_SVG:
		gerbv_export_svg_file_from_project_autoscaled (
				mainProject, new_file_name);
		break;
	case CALLBACKS_SAVE_FILE_DXF:
#if HAVE_LIBDXFLIB
		if (gerbv_export_dxf_file_from_image(new_file_name,
				act_file->image, &act_file->transform)) {
			GERB_MESSAGE (
				_("\"%s\" layer #%d saved as DXF in \"%s\""),
				act_file->name, file_index + 1,
				new_file_name);
		}
#endif
		break;
	case CALLBACKS_SAVE_FILE_PNG:
		if (dpi == 0) {
			gerbv_export_png_file_from_project_autoscaled (
					mainProject,
					screenRenderInfo.displayWidth,
					screenRenderInfo.displayHeight,
					new_file_name);
		} else {	/* Non zero DPI */
			gerbv_render_size_t bb;
			gerbv_render_get_boundingbox (mainProject, &bb);
			gfloat w = bb.right - bb.left;
			gfloat h = bb.bottom - bb.top;
			gerbv_render_info_t renderInfo = {
				dpi, dpi,
				bb.left - (w*GERBV_DEFAULT_BORDER_COEFF)/2.0,
				bb.top - (h*GERBV_DEFAULT_BORDER_COEFF)/2.0,
				GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY,
				w*dpi*(1 + GERBV_DEFAULT_BORDER_COEFF),
				h*dpi*(1 + GERBV_DEFAULT_BORDER_COEFF),
			};
			gerbv_export_png_file_from_project (mainProject,
					&renderInfo, new_file_name);
		}

		break;
	case  CALLBACKS_SAVE_LAYER_AS:
		gerbv_save_layer_from_index (mainProject,
				file_index, new_file_name);

		/* Rename the file path in the index, so future saves will
		 * reference the new file path */
		g_free (act_file->fullPathname);
		act_file->fullPathname = g_strdup (new_file_name);
		g_free (act_file->name);
		act_file->name = g_path_get_basename (new_file_name);

		break;
	case CALLBACKS_SAVE_FILE_RS274X:
		if (gerbv_export_rs274x_file_from_image (new_file_name,
					act_file->image,
					&act_file->transform)) {
			GERB_MESSAGE (
				_("\"%s\" layer #%d saved as Gerber in \"%s\""),
				act_file->name, file_index + 1,
				new_file_name);
		}
		break;
	case CALLBACKS_SAVE_FILE_DRILL:
		if (gerbv_export_drill_file_from_image (new_file_name,
					act_file->image,
					&act_file->transform)) {
			GERB_MESSAGE (
				_("\"%s\" layer #%d saved as drill in \"%s\""),
				act_file->name, file_index + 1,
				new_file_name);
		}
		break;
	case CALLBACKS_SAVE_FILE_IDRILL:
		if (gerbv_export_isel_drill_file_from_image (new_file_name,
				act_file->image, &act_file->transform)) {
			GERB_MESSAGE (
				_("\"%s\" layer #%d saved as ISEL NCP drill "
				"in \"%s\""), act_file->name, file_index + 1,
				new_file_name);
		}
		break;
	case CALLBACKS_SAVE_FILE_GEDA_PCB:
		if (gerbv_export_geda_pcb_file_from_image(new_file_name,
				act_file->image, &act_file->transform)) {
			GERB_MESSAGE (
				_("\"%s\" layer #%d saved as gEDA PCB "
				"in \"%s\""), act_file->name, file_index + 1,
				new_file_name);
		}
		break;
	case CALLBACKS_SAVE_FILE_RS274XM: {
		gerbv_image_t *image;
		gerbv_user_transformation_t t = {0,0,1,1,0,FALSE,FALSE,FALSE};
		if (NULL != (image = merge_images (processType))) {
			if (gerbv_export_rs274x_file_from_image (
						new_file_name, image, &t)) {
				GERB_MESSAGE (_("Merged visible Gerber layers "
						"and saved in \"%s\""),
						new_file_name);
			}
			gerbv_destroy_image (image);
		}
		break;
	}
	case CALLBACKS_SAVE_FILE_DRILLM: {
		gerbv_image_t *image;
		gerbv_user_transformation_t t = {0,0,1,1,0,FALSE,FALSE,FALSE};
		if (NULL != (image = merge_images (processType))) {
			gerbv_export_drill_file_from_image (
					new_file_name, image, &t);
			gerbv_destroy_image (image);
			GERB_MESSAGE (_("Merged visible drill layers "
					"and saved in \"%s\""),
					new_file_name);
		}
		break;
	}
	}

	g_free (new_file_name);
	callbacks_update_layer_tree ();

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
	gerbv_render_info_t renderInfo = {1.0, 1.0, 0, 0,
		GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY,
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
void
callbacks_show_selection_on_invisible (GtkMenuItem *menuitem, gpointer user_data)
{
	mainProject->show_invisible_selection = GTK_CHECK_MENU_ITEM(menuitem)->active;
	render_refresh_rendered_image_on_screen();
}

/* --------------------------------------------------------- */
void
callbacks_show_cross_on_drill_holes (GtkMenuItem *menuitem, gpointer user_data)
{
	screenRenderInfo.show_cross_on_drill_holes = GTK_CHECK_MENU_ITEM(menuitem)->active;
	render_refresh_rendered_image_on_screen();
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

static const char *error_type_string(gerbv_message_type_t type) {
	switch (type) {
	case GERBV_MESSAGE_FATAL:
		return _("FATAL");
	case GERBV_MESSAGE_ERROR:
		return _("ERROR");
	case GERBV_MESSAGE_WARNING:
		return _("Warning");
	case GERBV_MESSAGE_NOTE:
		return _("Note");
	default:
		return "Unknown";
	}
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
	gerbv_aperture_list_t *aperture_list;
	gchar *str;
	int i;

	/* Get a report of stats & errors accumulated from all layers */
	gerbv_stats_t *stat = generate_gerber_analysis();

	/* General info report */
	GString *general_report_str = g_string_new(NULL);
	if (stat->layer_count == 0) {
		g_string_printf(general_report_str,
				_("No Gerber layers visible!"));
	} else {
		if (stat->error_list->error_text == NULL) {
			g_string_printf(general_report_str,
				ngettext("No errors found in %d visible "
					"Gerber layer.",
					"No errors found in %d visible "
					"Gerber layers.",
					stat->layer_count),
				stat->layer_count);
		} else {
			g_string_printf(general_report_str,
				ngettext("Found errors in %d visible "
					"Gerber layer.",
					"Found errors in %d visible "
					"Gerber layers.",
					stat->layer_count),
				stat->layer_count);
		}
	}

	GtkWidget *general_label = gtk_label_new(general_report_str->str);
	g_string_free(general_report_str, TRUE);
	gtk_misc_set_alignment(GTK_MISC(general_label), 0, 0);
	gtk_misc_set_padding(GTK_MISC(general_label), 7, 7);
	gtk_label_set_selectable(GTK_LABEL(general_label), TRUE);

	struct table *general_table;
	
	general_table = table_new_with_columns(3,
			_("Layer"), G_TYPE_UINT, _("Type"), G_TYPE_STRING,
			_("Description"), G_TYPE_STRING);
	table_set_column_align(general_table, 0, 1.0);

	for (i = 0; i <= mainProject->last_loaded; i++) {
		gerbv_fileinfo_t **files = mainProject->file;

		if (!files[i]
		||  !files[i]->isVisible
		||   files[i]->image->layertype != GERBV_LAYERTYPE_RS274X)
			continue;

		table_add_row(general_table, i + 1,
				_("RS-274X file"), files[i]->fullPathname);

		str = g_strdup_printf(_("%g x %g %s"),
				screen_units(fabs(files[i]->image->info->max_x -
						files[i]->image->info->min_x)),
				screen_units(fabs(files[i]->image->info->max_y -
						files[i]->image->info->min_y)),
				screen_units_str());
		table_add_row(general_table, i + 1, _("Bounding size"), str);
		g_free(str);

		/* Check error report on layer */
		if (stat->layer_count > 0
		&&  stat->error_list->error_text != NULL) {
			for (gerbv_error_list_t *err_list = stat->error_list;
					err_list != NULL;
					err_list = err_list->next) {

				if (i != err_list->layer - 1)
					continue;

				table_add_row(general_table, err_list->layer,
					error_type_string(err_list->type),
					err_list->error_text);
			}
		}
	}

	/* G codes on active layers */
	GtkWidget *G_report_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(G_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *G_table =
		table_new_with_columns(3, _("Code"), G_TYPE_STRING,
			pgettext("table", "Count"), G_TYPE_UINT,
			_("Note"), G_TYPE_STRING);
	table_set_column_align(G_table, 0, 1.0);
	table_set_column_align(G_table, 1, 1.0);
	gtk_tree_view_set_headers_clickable(
			GTK_TREE_VIEW(G_table->widget), TRUE);

	table_add_row(G_table, "G00", stat->G0,  _(gerber_g_code_name(0)));
	table_add_row(G_table, "G01", stat->G1,  _(gerber_g_code_name(1)));
	table_add_row(G_table, "G02", stat->G2,  _(gerber_g_code_name(2)));
	table_add_row(G_table, "G03", stat->G3,  _(gerber_g_code_name(3)));
	table_add_row(G_table, "G04", stat->G4,  _(gerber_g_code_name(4)));
	table_add_row(G_table, "G10", stat->G10, _(gerber_g_code_name(10)));
	table_add_row(G_table, "G11", stat->G11, _(gerber_g_code_name(11)));
	table_add_row(G_table, "G12", stat->G12, _(gerber_g_code_name(12)));
	table_add_row(G_table, "G36", stat->G36, _(gerber_g_code_name(36)));
	table_add_row(G_table, "G37", stat->G37, _(gerber_g_code_name(37)));
	table_add_row(G_table, "G54", stat->G54, _(gerber_g_code_name(54)));
	table_add_row(G_table, "G55", stat->G55, _(gerber_g_code_name(55)));
	table_add_row(G_table, "G70", stat->G70, _(gerber_g_code_name(70)));
	table_add_row(G_table, "G71", stat->G71, _(gerber_g_code_name(71)));
	table_add_row(G_table, "G74", stat->G74, _(gerber_g_code_name(74)));
	table_add_row(G_table, "G75", stat->G75, _(gerber_g_code_name(75)));
	table_add_row(G_table, "G90", stat->G90, _(gerber_g_code_name(90)));
	table_add_row(G_table, "G91", stat->G91, _(gerber_g_code_name(91)));
	table_add_row(G_table, "", stat->G_unknown, _("unknown G-codes"));

	table_set_sortable(G_table);
	gtk_container_add(GTK_CONTAINER(G_report_window), G_table->widget);

	/* D codes on active layers */
	GtkWidget *D_report_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(D_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *D_table =
		table_new_with_columns(3, _("Code"), G_TYPE_STRING,
			pgettext("table", "Count"), G_TYPE_UINT,
			_("Note"), G_TYPE_STRING);
	table_set_column_align(D_table, 0, 1.0);
	table_set_column_align(D_table, 1, 1.0);
	gtk_tree_view_set_headers_clickable(
			GTK_TREE_VIEW(D_table->widget), TRUE);
	table_add_row(D_table, "D01", stat->D1, _(gerber_d_code_name(1)));
	table_add_row(D_table, "D02", stat->D2, _(gerber_d_code_name(2)));
	table_add_row(D_table, "D03", stat->D3, _(gerber_d_code_name(3)));
	table_add_row(D_table, "", stat->D_unknown, _("unknown D-codes"));
	table_add_row(D_table, "", stat->D_error, _("D-code errors"));

	table_set_sortable(D_table);
	gtk_container_add(GTK_CONTAINER(D_report_window), D_table->widget);

	/* M codes on active layers */
	GtkWidget *M_report_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(M_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *M_table =
		table_new_with_columns(3, _("Code"), G_TYPE_STRING,
			pgettext("table", "Count"), G_TYPE_UINT,
			_("Note"), G_TYPE_STRING);
	table_set_column_align(M_table, 0, 1.0);
	table_set_column_align(M_table, 1, 1.0);
	gtk_tree_view_set_headers_clickable(
			GTK_TREE_VIEW(M_table->widget), TRUE);
	table_add_row(M_table, "M00", stat->M0, _(gerber_m_code_name(0)));
	table_add_row(M_table, "M01", stat->M1, _(gerber_m_code_name(1)));
	table_add_row(M_table, "M02", stat->M2, _(gerber_m_code_name(2)));
	table_add_row(M_table, "", stat->M_unknown, _("unknown M-codes"));

	table_set_sortable(M_table);
	gtk_container_add(GTK_CONTAINER(M_report_window), M_table->widget);

	/* Misc codes */
	GtkWidget *misc_report_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(misc_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *misc_table =
		table_new_with_columns(2, _("Code"), G_TYPE_STRING,
				pgettext("table", "Count"), G_TYPE_UINT);
	table_set_column_align(misc_table, 1, 1.0);
	gtk_tree_view_set_headers_clickable(
			GTK_TREE_VIEW(misc_table->widget), TRUE);
	table_add_row(misc_table, "X", stat->X);
	table_add_row(misc_table, "Y", stat->Y);
	table_add_row(misc_table, "I", stat->I);
	table_add_row(misc_table, "J", stat->J);
	table_add_row(misc_table, "*", stat->star);
	table_add_row(misc_table, _("Unknown"), stat->unknown);

	table_set_sortable(misc_table);
	gtk_container_add(GTK_CONTAINER(misc_report_window),
			misc_table->widget);

	/* Apertures definition in input files */
	GtkWidget *aperture_def_report_window =
		gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(aperture_def_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	if (stat->aperture_list->number == -1) {
		GtkWidget *aperture_def_label = gtk_label_new(
			_("No aperture definitions found in active Gerber file(s)!"));
		gtk_misc_set_alignment(GTK_MISC(aperture_def_label), 0, 0);
		gtk_misc_set_padding(GTK_MISC(aperture_def_label), 7, 7);
		gtk_label_set_selectable(GTK_LABEL(aperture_def_label), TRUE);
		gtk_scrolled_window_add_with_viewport(
				GTK_SCROLLED_WINDOW(aperture_def_report_window),
				aperture_def_label);
	} else {
		struct table *aperture_def_table = table_new_with_columns(6,
				_("Layer"), G_TYPE_UINT,
				_("D code"), G_TYPE_STRING,
				_("Aperture"), G_TYPE_STRING,
				_("Param[0]"), G_TYPE_DOUBLE,
				_("Param[1]"), G_TYPE_DOUBLE,
				_("Param[2]"), G_TYPE_DOUBLE);
		table_set_column_align(aperture_def_table, 0, 1.0);
		table_set_column_align(aperture_def_table, 1, 1.0);
		gtk_tree_view_set_headers_clickable(
				GTK_TREE_VIEW(aperture_def_table->widget), TRUE);
		gtk_tree_view_set_search_column(
				GTK_TREE_VIEW(aperture_def_table->widget), 1);

		GString *gstr = g_string_new(NULL);
		for (aperture_list = stat->aperture_list;
				aperture_list != NULL;
				aperture_list = aperture_list->next) {
			g_string_printf(gstr, "D%d", aperture_list->number);
			table_add_row(aperture_def_table,
				aperture_list->layer,
				gstr->str,
				_(gerbv_aperture_type_name(
					aperture_list->type)),
				aperture_list->parameter[0],
				aperture_list->parameter[1],
				aperture_list->parameter[2]);
		}
		g_string_free(gstr, TRUE);
		table_set_sortable(aperture_def_table);
		gtk_container_add(GTK_CONTAINER(aperture_def_report_window),
				aperture_def_table->widget);
	}

	/* Gerber aperture usage on active layers */
	GtkWidget *aperture_usage_report_window =
		gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(aperture_usage_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	unsigned int aperture_count = 0;

	if (stat->D_code_list->number == -1) {
		GtkWidget *aperture_usage_label = gtk_label_new(
			_("No apertures used in Gerber file(s)!"));
		gtk_misc_set_alignment(GTK_MISC(aperture_usage_label), 0, 0);
		gtk_misc_set_padding(GTK_MISC(aperture_usage_label), 7, 7);
		gtk_label_set_selectable(GTK_LABEL(aperture_usage_label), TRUE);
		gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(aperture_usage_report_window),
			aperture_usage_label);
	} else {
		struct table *aperture_usage_table = table_new_with_columns(2,
				_("Code"), G_TYPE_STRING,
				pgettext("table", "Count"), G_TYPE_UINT);
		table_set_column_align(aperture_usage_table, 0, 1.0);
		table_set_column_align(aperture_usage_table, 1, 1.0);
		gtk_tree_view_set_headers_clickable(
			GTK_TREE_VIEW(aperture_usage_table->widget), TRUE);

		GString *gstr = g_string_new(NULL);
		for (aperture_list = stat->D_code_list;
				aperture_list != NULL;
				aperture_list = aperture_list->next) {
			g_string_printf(gstr, "D%d", aperture_list->number);
			table_add_row(aperture_usage_table,
					gstr->str,
					aperture_list->count);
			aperture_count += aperture_list->count;
		}
		g_string_free(gstr, TRUE);
		table_add_row(aperture_usage_table, _("Total"), aperture_count);
		table_set_sortable(aperture_usage_table);
		gtk_container_add( GTK_CONTAINER(aperture_usage_report_window),
				aperture_usage_table->widget);
	}

	/* Create top level dialog window for report */
	GtkWidget *analyze_active_gerbers;
	analyze_active_gerbers = gtk_dialog_new_with_buttons(
			_("Gerber codes report on visible layers"),
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_container_set_border_width(GTK_CONTAINER (analyze_active_gerbers), 5);

	gtk_dialog_set_default_response (GTK_DIALOG(analyze_active_gerbers),
			GTK_RESPONSE_ACCEPT);
	g_signal_connect_after (G_OBJECT(analyze_active_gerbers),
			"response",
			G_CALLBACK (gtk_widget_destroy),
			GTK_WIDGET(analyze_active_gerbers));

	/* Put general report text into scrolled window */
	GtkWidget *general_report_window =
		gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(general_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	GtkWidget *vbox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), general_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), general_table->widget, 0, 0, 0);
	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(general_report_window), vbox);

	/* Create tabbed notebook widget and add report widgets. */
	GtkWidget *notebook = gtk_notebook_new();

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(general_report_window),
			gtk_label_new(_("General")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(G_report_window),
			gtk_label_new(_("G codes")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(D_report_window),
			gtk_label_new(_("D codes")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(M_report_window),
			gtk_label_new(_("M codes")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(misc_report_window),
			gtk_label_new(_("Misc. codes")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(aperture_def_report_window),
			gtk_label_new(_("Aperture definitions")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(aperture_usage_report_window),
			gtk_label_new(_("Aperture usage")));

	/* Now put notebook into dialog window and show the whole thing */
	gtk_container_add(
			GTK_CONTAINER(GTK_DIALOG(analyze_active_gerbers)->vbox),
			GTK_WIDGET(notebook));

	if (screen.settings) {
		analyze_window_size_restore(analyze_active_gerbers);
		g_signal_connect (G_OBJECT(analyze_active_gerbers), "response",
				G_CALLBACK (analyze_window_size_store),
				GTK_WIDGET(analyze_active_gerbers));
	} else {
		gtk_window_set_default_size(GTK_WINDOW(analyze_active_gerbers),
						640, 320);
	}

	gtk_widget_show_all(analyze_active_gerbers);

	gerbv_stats_destroy(stat);	
}

/* --------------------------------------------------------- */
/**
  * The analyze -> analyze drill file  menu item was selected.
  * Complie statistics on all open drill layers and then display
  * them.
  *
  */
void
callbacks_analyze_active_drill_activate(GtkMenuItem *menuitem,
					gpointer user_data)
{
	gchar *str;
	int i;

	gerbv_drill_stats_t *stat = generate_drill_analysis();

	/* General info report */
	GString *general_report_str = g_string_new(NULL);
	if (stat->layer_count == 0) {
		g_string_printf(general_report_str,
				_("No drill layers visible!"));
	} else {
		if (stat->error_list->error_text == NULL) {
			g_string_printf(general_report_str,
				ngettext("No errors found in %d visible "
					"drill layer.",
					"No errors found in %d visible "
					"drill layers.",
					stat->layer_count),
				stat->layer_count);
		} else {
			g_string_printf(general_report_str,
				ngettext("Found errors found in %d visible "
					"drill layer.",
					"Found errors found in %d visible "
					"drill layers.",
					stat->layer_count),
				stat->layer_count);
		}
	}

	GtkWidget *general_label = gtk_label_new(general_report_str->str);
	g_string_free(general_report_str, TRUE);
	gtk_misc_set_alignment(GTK_MISC(general_label), 0, 0);
	gtk_misc_set_padding(GTK_MISC(general_label), 7, 7);
	gtk_label_set_selectable(GTK_LABEL(general_label), TRUE);

	struct table *general_table;
	
	general_table = table_new_with_columns(3,
			_("Layer"), G_TYPE_UINT, _("Type"), G_TYPE_STRING,
			_("Description"), G_TYPE_STRING);
	table_set_column_align(general_table, 0, 1.0);

	for (i = 0; i <= mainProject->last_loaded; i++) {
		gerbv_fileinfo_t **files = mainProject->file;

		if (!files[i]
		||  !files[i]->isVisible
		||   files[i]->image->layertype != GERBV_LAYERTYPE_DRILL)
			continue;

		table_add_row(general_table, i + 1,
				_("Excellon file"), files[i]->fullPathname);

		str = g_strdup_printf(_("%g x %g %s"),
				screen_units(fabs(files[i]->image->info->max_x -
						files[i]->image->info->min_x)),
				screen_units(fabs(files[i]->image->info->max_y -
						files[i]->image->info->min_y)),
				screen_units_str());
		table_add_row(general_table, i + 1, _("Bounding size"), str);
		g_free(str);

		/* Check error report on layer */
		if (stat->layer_count > 0
		&&  stat->error_list->error_text != NULL) {
			for (gerbv_error_list_t *err_list = stat->error_list;
					err_list != NULL;
					err_list = err_list->next) {

				if (i != err_list->layer - 1)
					continue;

				table_add_row(general_table, err_list->layer,
					error_type_string(err_list->type),
					err_list->error_text);
			}
		}
	}

	/* G codes on active layers */
	GtkWidget *G_report_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(G_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *G_table =
		table_new_with_columns(3, _("Code"), G_TYPE_STRING,
			pgettext("table", "Count"), G_TYPE_UINT,
			_("Note"), G_TYPE_STRING);
	table_set_column_align(G_table, 0, 1.0);
	table_set_column_align(G_table, 1, 1.0);
	gtk_tree_view_set_headers_clickable(
			GTK_TREE_VIEW(G_table->widget), TRUE);

	table_add_row(G_table, "G00", stat->G00, _(drill_g_code_name(0)));
	table_add_row(G_table, "G01", stat->G01, _(drill_g_code_name(1)));
	table_add_row(G_table, "G02", stat->G02, _(drill_g_code_name(2)));
	table_add_row(G_table, "G03", stat->G03, _(drill_g_code_name(3)));
	table_add_row(G_table, "G04", stat->G04, _(drill_g_code_name(4)));
	table_add_row(G_table, "G05", stat->G05, _(drill_g_code_name(5)));
	table_add_row(G_table, "G85", stat->G85, _(drill_g_code_name(85)));
	table_add_row(G_table, "G90", stat->G90, _(drill_g_code_name(90)));
	table_add_row(G_table, "G91", stat->G91, _(drill_g_code_name(91)));
	table_add_row(G_table, "G93", stat->G93, _(drill_g_code_name(93)));
	table_add_row(G_table, "", stat->G_unknown, _("unknown G-codes"));

	table_set_sortable(G_table);
	gtk_container_add(GTK_CONTAINER(G_report_window), G_table->widget);

	/* M codes on active layers */
	GtkWidget *M_report_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(M_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *M_table =
		table_new_with_columns(3, _("Code"), G_TYPE_STRING,
			pgettext("table", "Count"), G_TYPE_UINT,
			_("Note"), G_TYPE_STRING);
	table_set_column_align(M_table, 0, 1.0);
	table_set_column_align(M_table, 1, 1.0);
	gtk_tree_view_set_headers_clickable(
			GTK_TREE_VIEW(M_table->widget), TRUE);
	table_add_row(M_table, "M00", stat->M00, _(drill_m_code_name(0)));
	table_add_row(M_table, "M01", stat->M01, _(drill_m_code_name(1)));
	table_add_row(M_table, "M18", stat->M18, _(drill_m_code_name(18)));
	table_add_row(M_table, "M25", stat->M25, _(drill_m_code_name(25)));
	table_add_row(M_table, "M30", stat->M30, _(drill_m_code_name(30)));
	table_add_row(M_table, "M45", stat->M45, _(drill_m_code_name(45)));
	table_add_row(M_table, "M47", stat->M47, _(drill_m_code_name(47)));
	table_add_row(M_table, "M48", stat->M48, _(drill_m_code_name(48)));
	table_add_row(M_table, "M71", stat->M71, _(drill_m_code_name(71)));
	table_add_row(M_table, "M72", stat->M72, _(drill_m_code_name(72)));
	table_add_row(M_table, "M95", stat->M95, _(drill_m_code_name(95)));
	table_add_row(M_table, "M97", stat->M97, _(drill_m_code_name(97)));
	table_add_row(M_table, "M98", stat->M98, _(drill_m_code_name(98)));
	table_add_row(M_table, "", stat->M_unknown, _("unknown M-codes"));

	table_set_sortable(M_table);
	gtk_container_add(GTK_CONTAINER(M_report_window), M_table->widget);

	/* Misc codes */
	GtkWidget *misc_report_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(misc_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *misc_table =
		table_new_with_columns(2,
				/* Count is string for value hide. */
				pgettext("table", "Count"), G_TYPE_STRING,
				_("Code"), G_TYPE_STRING);
	table_set_column_align(misc_table, 0, 1.0);
	str = g_strdup_printf("%d", stat->comment);
	table_add_row(misc_table, str,_("Comments"));
	g_free(str);
	str = g_strdup_printf("%d", stat->unknown);
	table_add_row(misc_table, str, _("Unknown codes"));
	g_free(str);
	str = g_strdup_printf("%d", stat->R);
	table_add_row(misc_table, str, _("Repeat hole (R)"));
	g_free(str);
	if (stat->detect != NULL ) {
		table_add_row(misc_table, "", stat->detect);
	}

	table_set_sortable(misc_table);
	gtk_container_add(GTK_CONTAINER(misc_report_window),
			misc_table->widget);

	/* Drill usage on active layers */
	GtkWidget *drill_usage_report_window =
			gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(drill_usage_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	struct table *drill_usage_table = table_new_with_columns(4,
			_("Drill no."), G_TYPE_UINT,
			_("Dia."), G_TYPE_DOUBLE,
			_("Units"), G_TYPE_STRING,
			pgettext("table", "Count"), G_TYPE_UINT);

	table_set_column_align(drill_usage_table, 0, 1.0);
	table_set_column_align(drill_usage_table, 3, 1.0);
	gtk_tree_view_set_headers_clickable(
		GTK_TREE_VIEW(drill_usage_table->widget), TRUE);

	gerbv_drill_list_t *drill_list;
	for (drill_list = stat->drill_list;
			drill_list != NULL; drill_list = drill_list->next) {
		if (drill_list->drill_num == -1)
			break;	/* No drill list */

		table_add_row(drill_usage_table,
				drill_list->drill_num,
				drill_list->drill_size,
				drill_list->drill_unit,
				drill_list->drill_count);
	}

	table_set_sortable(drill_usage_table);
	gtk_container_add(GTK_CONTAINER(drill_usage_report_window),
			drill_usage_table->widget);

	/* Create top level dialog window for report */
	GtkWidget *analyze_active_drill;
	analyze_active_drill = gtk_dialog_new_with_buttons(
			_("Drill codes report on visible layers"),
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (analyze_active_drill), 5);

	gtk_dialog_set_default_response (GTK_DIALOG(analyze_active_drill),
			GTK_RESPONSE_ACCEPT);
	g_signal_connect_after (G_OBJECT(analyze_active_drill),
			"response",
			G_CALLBACK (gtk_widget_destroy),
			GTK_WIDGET(analyze_active_drill));

	/* Put general report text into scrolled window */
	GtkWidget *general_report_window =
		gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(general_report_window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	GtkWidget *vbox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), general_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), general_table->widget, 0, 0, 0);
	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(general_report_window), vbox);

	/* Create tabbed notebook widget and add report widgets. */
	GtkWidget *notebook = gtk_notebook_new();

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(general_report_window),
			gtk_label_new(_("General")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(G_report_window),
			gtk_label_new(_("G codes")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(M_report_window),
			gtk_label_new(_("M codes")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(misc_report_window),
			gtk_label_new(_("Misc. codes")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			GTK_WIDGET(drill_usage_report_window),
			gtk_label_new(_("Drill usage")));

	/* Now put notebook into dialog window and show the whole thing */
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(analyze_active_drill)->vbox),
			GTK_WIDGET(notebook));

	if (screen.settings) {
		analyze_window_size_restore(analyze_active_drill);
		g_signal_connect (G_OBJECT(analyze_active_drill), "response",
				G_CALLBACK (analyze_window_size_store),
				GTK_WIDGET(analyze_active_drill));
	} else {
		gtk_window_set_default_size(GTK_WINDOW(analyze_active_drill),
						640, 320);
	}

	gtk_widget_show_all(analyze_active_drill);

	gerbv_drill_stats_destroy(stat);
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
		_("Quitting the program will cause any unsaved changes "
		"to be lost."),
		FALSE, NULL, GTK_STOCK_QUIT, GTK_STOCK_CANCEL)) {
    return TRUE; // stop propagation of the delete_event.
	// this would destroy the gui but not return from the gtk event loop.
  }

  /* Save background color */
  if (screen.settings && !screen.background_is_from_project) {
    guint clr;
    GdkColor *bg = &mainProject->background;

    clr = bg->red/257<<16 | bg->green/257<<8 | bg->blue/257;
    g_settings_set_uint (screen.settings, "background-color", clr);
  }

  /* Save main window size and postion */
  if (screen.settings) {
    GtkWindow *win = GTK_WINDOW(screen.win.topLevelWindow);
    gint32 xy[2];
    GVariant *var;
    gboolean is_max;

    is_max = FALSE != (GDK_WINDOW_STATE_MAXIMIZED & gdk_window_get_state (
			    gtk_widget_get_window (GTK_WIDGET(win))));
    g_settings_set_boolean (screen.settings, "window-maximized", is_max);

    if (!is_max) {
      gtk_window_get_size (win, (gint *)xy, (gint *)(xy+1));
      var = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32, xy, 2,
		      sizeof (xy[0]));
      g_settings_set_value (screen.settings, "window-size", var);

      gtk_window_get_position (win, (gint *)xy, (gint *)(xy+1));
      var = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32, xy, 2,
		      sizeof (xy[0]));
      g_settings_set_value (screen.settings, "window-position", var);
    }
  }

  gerbv_unload_all_layers (mainProject);
  gtk_main_quit();

  return FALSE;
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

	gchar *string = g_strdup_printf(_(
		"Gerbv — a Gerber (RS-274/X) viewer\n"
		"\n"
		"Version %s\n"
		"Compiled on %s at %s\n"
		"\n"
		"Gerbv was originally developed as part of the gEDA Project "
		"but is now separately maintained.\n"
		"\n"
		"For more information see:\n"
		"  gerbv homepage: https://gerbv.github.io/\n"
		"  gerbv repository: https://github.com/gerbv/gerbv"),
		VERSION, __DATE__, __TIME__);

#if GTK_CHECK_VERSION(2,6,0)
	gchar *license = g_strdup_printf(_(
		"Gerbv — a Gerber (RS-274/X) viewer\n"
		"\n"
		"Copyright (C) 2000—2007 Stefan Petersen\n"
		"\n"
		"This program is free software: you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation, either version 2 of the License, or\n"
		"(at your option) any later version.\n"
		"\n"
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
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG (aboutdialog1), "https://gerbv.github.io/");
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
    GtkWidget *bugs_dialog = gtk_dialog_new_with_buttons(_("Known bugs in Gerbv"),
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
    gtk_misc_set_padding(GTK_MISC(bugs_label), 7, 7);
    
    /* Put text into scrolled window */
    GtkWidget *bugs_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(bugs_window),
                                          GTK_WIDGET(bugs_label));
    gtk_widget_set_size_request(bugs_window, 600, 300);
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
callbacks_units_changed (gerbv_gui_unit_t unit)
{
	static gboolean isChanging = FALSE;

	if (isChanging)
		return;

	isChanging = TRUE;
	screen.unit = unit;

	if (unit == GERBV_MILS || unit == GERBV_MMS || unit == GERBV_INS) {
		gtk_combo_box_set_active (
				GTK_COMBO_BOX (screen.win.statusUnitComboBox),
				unit);
		gtk_check_menu_item_set_active (
				screen.win.menu_view_unit_group[unit], TRUE);
	}

	callbacks_update_ruler_scales ();
	callbacks_update_statusbar_coordinates (screen.last_x, screen.last_y);
	
	if (screen.tool == MEASURE)
		callbacks_update_statusbar_measured_distance (
				screen.measure_last_x,
				screen.measure_last_y);

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
	gerbv_render_info_t tempRenderInfo = {0, 0, 0, 0,
		GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY,
		screenRenderInfo.displayWidth,
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
static void
callbacks_layer_tree_visibility_toggled (gint index)
{
	mainProject->file[index]->isVisible =
		!mainProject->file[index]->isVisible;

	callbacks_update_layer_tree ();
	if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
		render_refresh_rendered_image_on_screen ();
	} else {
		render_recreate_composite_surface (screen.drawing_area);
		callbacks_force_expose_event_for_screen ();
	}
}

/* --------------------------------------------------------- */
static gint
callbacks_get_col_num_from_tree_view_col (GtkTreeViewColumn *col)
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
callbacks_add_layer_button_clicked (GtkButton *button, gpointer user_data)
{
	callbacks_open_activate (NULL, NULL);
}

/* --------------------------------------------------------- */
void
callbacks_unselect_all_tool_buttons (void) {

}

void
callbacks_switch_to_normal_tool_cursor (gint toolNumber)
{
	GdkWindow *drawing_area_window = screen.drawing_area->window;
	GdkCursor *cursor;

	switch (toolNumber) {
	case POINTER:
		gdk_window_set_cursor(drawing_area_window, GERBV_DEF_CURSOR);
		break;
	case PAN:
		cursor = gdk_cursor_new(GDK_FLEUR);
		gdk_window_set_cursor(drawing_area_window, cursor);
		gdk_cursor_destroy(cursor);
		break;
	case ZOOM:
		cursor = gdk_cursor_new(GDK_SIZING);
		gdk_window_set_cursor(drawing_area_window, cursor);
		gdk_cursor_destroy(cursor);
		break;
	case MEASURE:
		cursor = gdk_cursor_new(GDK_CROSSHAIR);
		gdk_window_set_cursor(drawing_area_window, cursor);
		gdk_cursor_destroy(cursor);
		break;
	default:
		break;
	}
}

/* --------------------------------------------------------- */
void
callbacks_switch_to_correct_cursor (void)
{
	GdkWindow *drawing_area_window = screen.drawing_area->window;
	GdkCursor *cursor;

	if (screen.state == IN_MOVE) {
		cursor = gdk_cursor_new(GDK_FLEUR);
		gdk_window_set_cursor(drawing_area_window, cursor);
		gdk_cursor_destroy(cursor);
		return;
	}
	else if (screen.state == IN_ZOOM_OUTLINE) {
		cursor = gdk_cursor_new(GDK_SIZING);
		gdk_window_set_cursor(drawing_area_window, cursor);
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
				_("Click to select objects in the active layer. "
					"Middle click and drag to pan."),
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
				_("Click and drag to measure a distance or select two apertures."),
				MAX_DISTLEN);

			/* To not show previous measure drag-line */
			screen.measure_start_x = 0;
			screen.measure_start_y = 0;
			screen.measure_stop_x =  0;
			screen.measure_stop_y =  0;

			/* If two items are selected, measure they distance */
			if (selection_length (&screen.selectionInfo) == 2) {
				gerbv_selection_item_t item[2];
				gerbv_net_t *net[2];

				item[0] = selection_get_item_by_index(
						&screen.selectionInfo, 0);
				item[1] = selection_get_item_by_index(
						&screen.selectionInfo, 1);
				net[0] = item[0].net;
				net[1] = item[1].net;

				if ((net[0]->aperture_state ==
						net[1]->aperture_state)
				 && (net[0]->aperture_state ==
						GERBV_APERTURE_STATE_FLASH)) {
					screen.measure_start_x = net[0]->stop_x;
					screen.measure_start_y = net[0]->stop_y;
					gerbv_transform_coord_for_image(
							&screen.measure_start_x,
							&screen.measure_start_y,
							item[0].image,
							mainProject);

					screen.measure_stop_x = net[1]->stop_x;
					screen.measure_stop_y = net[1]->stop_y;
					gerbv_transform_coord_for_image(
							&screen.measure_stop_x,
							&screen.measure_stop_y,
							item[1].image,
							mainProject);

					render_draw_measure_distance();
				}
			}
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
static void
callbacks_select_layer_row (gint rowIndex)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
			((GtkTreeView *) screen.win.layerTree);

	selection = gtk_tree_view_get_selection((GtkTreeView *) screen.win.layerTree);

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store),
				&iter, NULL, rowIndex)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

/* --------------------------------------------------------- */
/**
  * This fcn returns the index of selected layer (selected in
  * the layer window on left).
  *
  */
static gint
callbacks_get_selected_row_index (void)
{
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
callbacks_remove_layer_button_clicked (GtkButton *button, gpointer user_data)
{
	gint index = callbacks_get_selected_row_index ();
	
	if ((index >= 0) && (index <= mainProject->last_loaded)) {
		render_remove_selected_objects_belonging_to_layer (
				&screen.selectionInfo,
				mainProject->file[index]->image);
		update_selected_object_message (FALSE);

		gerbv_unload_layer (mainProject, index);
		callbacks_update_layer_tree ();

		index = MAX(0, index - 1);
		callbacks_select_layer_row (index);

		if (screenRenderInfo.renderType <= GERBV_RENDER_TYPE_GDK_XOR) {
			render_refresh_rendered_image_on_screen ();
		} else {
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
		callbacks_select_layer_row (index + 1);

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
		callbacks_select_layer_row (index - 1);
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
callbacks_reload_layer_clicked (GtkButton *button, gpointer user_data)
{
	gint index = callbacks_get_selected_row_index ();

	if (index < 0) {
		show_no_layers_warning ();
		return;
	}

	render_remove_selected_objects_belonging_to_layer (
			&screen.selectionInfo, mainProject->file[index]->image);
	update_selected_object_message (FALSE);

	gerbv_revert_file (mainProject, index);
	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree();
}

void
callbacks_change_layer_edit_clicked  (GtkButton *button, gpointer userData)
{
	gint index = callbacks_get_selected_row_index();
	gerbv_fileinfo_t **files = mainProject->file;
	gerbv_user_transformation_t **transforms;
	int i, j;

	if (index < 0) {
		show_no_layers_warning ();
		return;
	}

	/* last_loaded == 0 if only one file is loaded */
	transforms = g_new (gerbv_user_transformation_t *,
			mainProject->last_loaded +
			2 /* layer + NULL */ +
			1 /* if selected layer is visible */);

	/* [0] is selected layer */
	transforms[0] = &mainProject->file[index]->transform;

	/* Get visible Gerber files transformations */
	j = 1;	/* [0] is alerady used */
	for (i = 0; i <= mainProject->last_loaded; i++) {
		if (files[i] && files[i]->isVisible)
			transforms[j++] = &files[i]->transform;
	}

	/* Terminate array with NULL */
	transforms[j] = NULL;

	interface_show_layer_edit_dialog(transforms, screen.unit);
	g_free (transforms);
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
	type = N_("Unknown type");

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
		rc = interface_get_alert_dialog_response (
			_("This layer has changed!"),
			_("Editing the file type will reload the layer, "
			"destroying your changes.  Click OK to edit "
			"the file type and destroy your changes, "
			"or Cancel to leave."),
			TRUE, NULL, GTK_STOCK_OK, GTK_STOCK_CANCEL);
		if (rc == 0) return;  /* Return if user hit Cancel */
	    }

	    results = (gerbv_HID_Attr_Val *) malloc (n * sizeof (gerbv_HID_Attr_Val));
	    if (results == NULL)
		GERB_FATAL_ERROR("%s(): malloc failed", __FUNCTION__);
      
	    /* non-zero means cancel was picked */
	    if (attribute_interface_dialog (attr, n, results, 
					    _("Edit file format"), 
					    _(type)))
		{
		    return;
		}
          
    }

    dprintf ("%s(): reloading layer\n", __func__);
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
callbacks_file_drop_event(GtkWidget *widget, GdkDragContext *dc,
		gint x, gint y, GtkSelectionData *data,
		guint info, guint time, gpointer p)
{
	gchar **uris, **uri;
	GSList *fns = NULL;

	uris = gtk_selection_data_get_uris(data);
	if (!uris)
		return FALSE;

	for (uri = uris; *uri; uri++) {
		const char *prefix_str =
#ifdef WIN32
			"file:///";
#else
			"file://";
#endif
		if (g_strrstr(*uri, prefix_str) == *uri)
			fns = g_slist_append(fns, g_uri_unescape_string(
					*uri + strlen(prefix_str), NULL));
	}

	open_files(fns);
	g_slist_free_full(fns, g_free);
	g_strfreev(uris);

	return TRUE;
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
				idx = callbacks_get_col_num_from_tree_view_col (col);
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
					gpointer user_data)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeViewColumn *col;
	gint x,y;
	gint *indices;

	GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model (
			(GtkTreeView *) screen.win.layerTree);

	if (event->button == 1) {
		if (gtk_tree_view_get_path_at_pos ((GtkTreeView *) widget,
				event->x, event->y, &path, &col, &x, &y)
		&& gtk_tree_model_get_iter ((GtkTreeModel *)list_store,
				&iter, path)) {
			indices = gtk_tree_path_get_indices (path);
			if (indices && (indices[0] <= mainProject->last_loaded)) {
				switch (callbacks_get_col_num_from_tree_view_col (col)) {
				case 0:
					callbacks_select_layer_row (indices[0]);
					callbacks_layer_tree_visibility_toggled (indices[0]);
					return TRUE;
				case 1:
					callbacks_show_color_picker_dialog (indices[0]);
					/* don't propagate the signal, since drag and drop can
					   sometimes activated during color selection */
					return TRUE;
				}
			}
		}
	}
	/* don't pop up the menu if we don't have any loaded files */
	else if ((event->button == 3)&&(mainProject->last_loaded >= 0)) {
		gtk_menu_popup (GTK_MENU (screen.win.layerTreePopupMenu),
				NULL, NULL, NULL, NULL, 
				event->button, event->time);
	}

	/* always allow the click to propagate and make sure the line is activated */
	return FALSE;
}

/* --------------------------------------------------------------------------- */
void
callbacks_update_layer_tree (void)
{
	GtkListStore *list_store = (GtkListStore *) gtk_tree_view_get_model
			((GtkTreeView *) screen.win.layerTree);
	gint idx;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gint oldSelectedRow;

	if (screen.win.treeIsUpdating)
		return;

	screen.win.treeIsUpdating = TRUE;

	oldSelectedRow = callbacks_get_selected_row_index();
	if (oldSelectedRow < 0)
		oldSelectedRow = 0;

	gtk_list_store_clear (list_store);

	for (idx = 0; idx <= mainProject->last_loaded; idx++) {
		GdkPixbuf *pixbuf, *blackPixbuf;
		unsigned char red, green, blue, alpha;
		guint32 color;
		gchar *layerName;
		gerbv_fileinfo_t *file;
		
		file = mainProject->file[idx];
		if (!file)
			continue;

		red =   (unsigned char) (file->color.red * 255 / G_MAXUINT16);
		green = (unsigned char) (file->color.green * 255 / G_MAXUINT16);
		blue =  (unsigned char) (file->color.blue *255 / G_MAXUINT16);
		alpha = (unsigned char) (file->alpha * 255 / G_MAXUINT16);

		color = red*(256*256*256) + green*(256*256) + blue*256 + alpha;
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 20, 15);
		gdk_pixbuf_fill (pixbuf, color);

		blackPixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 20, 15);
		color = 100*(256*256*256) + 100*(256*256) + 100*256 + 150;
		gdk_pixbuf_fill (blackPixbuf, color);

		/* copy the color area into the black pixbuf */
		gdk_pixbuf_copy_area  (pixbuf, 1, 1, 18, 13, blackPixbuf, 1, 1);
		g_object_unref(pixbuf);

		gtk_list_store_append (list_store, &iter);

		gchar startChar[2],*modifiedCode;
		/* terminate the letter string */
		startChar[1] = 0;

		gint numberOfModifications = 0;
		if (file->transform.inverted) {
			startChar[0] = 'I';
			numberOfModifications++;
		}
		if (file->transform.mirrorAroundX
		||  file->transform.mirrorAroundY) {
			startChar[0] = 'M';
			numberOfModifications++;
		}
		if (fabs(file->transform.translateX)
					> GERBV_PRECISION_LINEAR_INCH
		||  fabs(file->transform.translateY)
					> GERBV_PRECISION_LINEAR_INCH) {
			startChar[0] = 'T';
			numberOfModifications++;
		}
		if (fabs(file->transform.scaleX - 1)
					> GERBV_PRECISION_LINEAR_INCH
		||  fabs(file->transform.scaleY - 1)
					> GERBV_PRECISION_LINEAR_INCH) {
			startChar[0] = 'S';
			numberOfModifications++;
		}
		if (fabs(file->transform.rotation)
					> GERBV_PRECISION_ANGLE_RAD) {
			startChar[0] = 'R';
			numberOfModifications++;
		}
		if (numberOfModifications > 1)
			startChar[0] = '*';
		if (numberOfModifications == 0)
			modifiedCode = g_strdup ("");
		else
			modifiedCode = g_strdup (startChar);

		/* Display any unsaved layers differently to show the user they
		 * are unsaved */
		if (file->layer_dirty == TRUE) {
			/* The layer has unsaved changes in it. Show layer name
			 * in italics. */
			layerName = g_strconcat ("*", "<i>", file->name,
					"</i>", NULL);
		} else {
			/* Layer is clean. Show layer name using normal font. */
			layerName = g_strdup (file->name);
		}

		gtk_list_store_set (list_store, &iter,
				    0, file->isVisible,
				    1, blackPixbuf,
				    2, layerName,
				    3, modifiedCode,
				    -1);
		g_free (layerName);
		g_free (modifiedCode);
		/* pixbuf has a refcount of 2 now, as the list store has added its own reference */
		g_object_unref(blackPixbuf);
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
	for (unsigned int i = 0;
			i < G_N_ELEMENTS(screen.win.curFileMenuItem); i++) {
		gtk_widget_set_sensitive (screen.win.curFileMenuItem[i], showItems);
	}
	screen.win.treeIsUpdating = FALSE;
}

/* --------------------------------------------------------------------------- */
void
callbacks_display_object_properties_clicked (GtkButton *button, gpointer user_data)
{
	gint index = callbacks_get_selected_row_index ();
	guint i;

	if (index < 0 || selection_length (&screen.selectionInfo) == 0) {
		interface_show_alert_dialog(_("No object is currently selected"),
			_("Objects must be selected using the pointer tool "
			"before you can view the object properties."),
			FALSE, NULL);
		return;
	}
	
	for (i = 0; i < selection_length (&screen.selectionInfo); i++){
		gerbv_selection_item_t sItem =
			selection_get_item_by_index (&screen.selectionInfo, i);

		gerbv_net_t *net = sItem.net;
		gerbv_image_t *image = sItem.image;

		if (net->interpolation == GERBV_INTERPOLATION_PAREA_START) {
			/* Spacing for a pretty display */
			if (i != 0)
				g_message (" ");

			g_message (_("Object type: Polygon"));
			parea_report (net, image, mainProject);
			net_layer_file_report (net, image, mainProject);
		} else {
			switch (net->aperture_state) {

			case GERBV_APERTURE_STATE_ON:
			case GERBV_APERTURE_STATE_FLASH:
				/* Spacing for a pretty display */
				if (i != 0)
					g_message (" ");
				break;

			default:
				break;
			}

			aperture_state_report (net, image, mainProject);
		}
	}
	/* Use separator for different report requests */
	g_message ("---------------------------------------");
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
		dprintf("Benchmark():  Starting redraw #%d\n", i);
		gerbv_render_to_pixmap_using_gdk (mainProject, renderedPixmap, renderInfo, NULL, NULL);
		now = time(NULL);
		dprintf("Elapsed time = %ld seconds\n", (long int) (now - start));
	}
	g_message(_("FAST (=GDK) mode benchmark: %d redraws "
				"in %ld seconds (%g redraws/second)"),
		      i, (long int) (now - start), (double) i / (double)(now - start));
	gdk_pixmap_unref(renderedPixmap);
	
	// run the cairo (normal) render mode
	i = 0;
	start = time(NULL);
	now = start;
	renderInfo->renderType = GERBV_RENDER_TYPE_CAIRO_NORMAL;
	while( now - 30 < start) {
		i++;
		dprintf("Benchmark():  Starting redraw #%d\n", i);
		cairo_surface_t *cSurface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
	                              renderInfo->displayWidth, renderInfo->displayHeight);
		cairo_t *cairoTarget = cairo_create (cSurface);
		gerbv_render_all_layers_to_cairo_target (mainProject, cairoTarget, renderInfo);
		cairo_destroy (cairoTarget);
		cairo_surface_destroy (cSurface);
		now = time(NULL);
		dprintf("Elapsed time = %ld seconds\n", (long int) (now - start));
	}
	g_message(_("NORMAL (=Cairo) mode benchmark: %d redraws "
				"in %ld seconds (%g redraws/second)"),
		      i, (long int) (now - start), (double) i / (double)(now - start));
}

/* --------------------------------------------------------------------------- */
void
callbacks_benchmark_clicked (GtkButton *button, gpointer   user_data)
{
	// prepare render size and options (canvas size width and height are last 2 variables)
	gerbv_render_info_t renderInfo = {1.0, 1.0, 0, 0,
					GERBV_RENDER_TYPE_GDK, 640, 480};

	if (!interface_get_alert_dialog_response(_("Performance benchmark"),
			_("Application will be unresponsive for 1 minute! "
			"Run performance benchmark?"),
			FALSE, NULL, GTK_STOCK_OK, GTK_STOCK_CANCEL))
		return;

	GERB_COMPILE_WARNING(_("Running full zoom benchmark..."));
	while (g_main_context_iteration(NULL, FALSE)) {} // update log widget

	// autoscale the image for now...maybe we don't want to do this in order to
	//   allow benchmarking of different zoom levels?
	gerbv_render_zoom_to_fit_display (mainProject, &renderInfo);
	callbacks_support_benchmark (&renderInfo);

	GERB_COMPILE_WARNING(_("Running x5 zoom benchmark..."));
	while (g_main_context_iteration(NULL, FALSE)) {} // update log widget

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
callbacks_live_edit (GtkWidget *button, gpointer user_data){
	GtkDialog *toplevel = GTK_DIALOG(gtk_widget_get_toplevel (button));
	gtk_dialog_response(toplevel, GTK_RESPONSE_APPLY);
}
/* --------------------------------------------------------------------------- */
void
callbacks_move_objects_clicked (GtkButton *button, gpointer   user_data){
	/* for testing, just hard code in some translations here */
	gerbv_image_move_selected_objects (screen.selectionInfo.selectedNodeArray, -0.050, 0.050);
	callbacks_update_layer_tree();
	selection_clear (&screen.selectionInfo);
	update_selected_object_message (FALSE);
	render_refresh_rendered_image_on_screen ();
}

/* --------------------------------------------------------------------------- */
void
callbacks_reduce_object_area_clicked  (GtkButton *button, gpointer user_data){
	/* for testing, just hard code in some parameters */
	gerbv_image_reduce_area_of_selected_objects (screen.selectionInfo.selectedNodeArray, 0.20, 3, 3, 0.01);
	selection_clear (&screen.selectionInfo);
	update_selected_object_message (FALSE);
	render_refresh_rendered_image_on_screen ();
}

/* --------------------------------------------------------------------------- */
void
callbacks_delete_objects_clicked (GtkButton *button, gpointer user_data)
{
	if (selection_length (&screen.selectionInfo) == 0) {
		interface_show_alert_dialog (
			_("No object is currently selected"),
			_("Objects must be selected using the pointer tool "
				"before they can be deleted."),
			FALSE,
			NULL);
		return;
	}

	gint index = callbacks_get_selected_row_index ();
	if (index < 0)
		return;

	if (mainProject->check_before_delete) {
		if (!interface_get_alert_dialog_response (
			_("Do you want to permanently delete "
			"the selected objects from <i>visible</i> layers?"),
			_("Gerbv currently has no undo function, so "
			"this action cannot be undone. This action "
			"will not change the saved file unless you "
			"save the file afterwards."),
			TRUE, &(mainProject->check_before_delete),
			GTK_STOCK_DELETE, GTK_STOCK_CANCEL)) {
				return;
		}
	}

	guint i;
	for (i = 0; i < selection_length (&screen.selectionInfo);) {
		gerbv_selection_item_t sel_item =
			selection_get_item_by_index (&screen.selectionInfo, i);
		gerbv_fileinfo_t *file_info =
			gerbv_get_fileinfo_for_image(sel_item.image, mainProject);

		/* Preserve currently invisible selection from deletion */
		if (!file_info->isVisible) {
			i++;
			continue;
		}

		file_info->layer_dirty = TRUE;
		selection_clear_item_by_index (&screen.selectionInfo, i);
		gerbv_image_delete_net (sel_item.net);
	}
	update_selected_object_message (FALSE);

	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree();
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
	
	if (GDK_IS_WINDOW(widget->window)) {
	      /* query the window's backbuffer if it has one */
		GdkWindow *window = GDK_WINDOW(widget->window);
	      gdk_window_get_internal_paint_info (window, &drawable, &x_off, &y_off);
	}
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
	GdkVisual *visual = gdk_drawable_get_visual (drawable);
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

	if (GDK_IS_WINDOW(widget->window)) {
	      /* query the window's backbuffer if it has one */
		GdkWindow *window = GDK_WINDOW(widget->window);
	      gdk_window_get_internal_paint_info (window,
	                                          &drawable, &x_off, &y_off);
	}
	gdk_drawable_get_size (drawable, &width, &height);

#if defined(WIN32) || defined(QUARTZ)
	/* FIXME */
	cr = gdk_cairo_create (GDK_WINDOW(widget->window));
#else      
	cairo_surface_t *buffert;
	
	GdkVisual *visual = gdk_drawable_get_visual (drawable);
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

const char *gerbv_coords_pattern_mils_str = N_("%8.2f %8.2f");

/* --------------------------------------------------------- */
static void
callbacks_update_statusbar_coordinates (gint x, gint y)
{
	gdouble X, Y;

	callbacks_screen2board (&X, &Y, x, y);

	switch (screen.unit) {
	case GERBV_MILS:
		utf8_snprintf (screen.statusbar.coordstr, MAX_COORDLEN,
				_(gerbv_coords_pattern_mils_str),
				COORD2MILS (X), COORD2MILS (Y));
		break;
	case GERBV_MMS:
		utf8_snprintf (screen.statusbar.coordstr, MAX_COORDLEN,
				_("%8.3f %8.3f"),
				COORD2MMS (X), COORD2MMS (Y));
		break;
	default:
		utf8_snprintf (screen.statusbar.coordstr, MAX_COORDLEN,
				_("%4.5f %4.5f"),
				COORD2INS (X), COORD2INS (Y));
	}

	callbacks_update_statusbar();
}

static void
update_selected_object_message (gboolean userTriedToSelect)
{
	if (screen.tool != POINTER)
		return;

	gint selectionLength = selection_length (&screen.selectionInfo);

	if (selectionLength == 0) {
		if (userTriedToSelect) {
			/* Update status bar message to make sure the user
			 * knows about needing to select the layer */
			gchar *str = g_new(gchar, MAX_DISTLEN);
			utf8_strncpy(str,
					_("No object selected. Objects can "
					"only be selected in the active "
					"layer."),
					MAX_DISTLEN - 7);
			utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
					"<b>%s</b>", str);
			g_free(str);
		} else {
			utf8_strncpy(screen.statusbar.diststr,
					_("Click to select objects in the "
					"active layer. Middle click and drag "
					"to pan."),
					MAX_DISTLEN);
		}
	} else {
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
			callbacks_screen2board(&(screen.measure_stop_x),
			    &(screen.measure_stop_y), x, y);
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
	GdkWindow *drawing_area_window = screen.drawing_area->window;
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
				screen.measure_stop_x = screen.measure_start_x;
				screen.measure_stop_y = screen.measure_start_y;
				/* force an expose event to clear any previous measure lines */
				callbacks_force_expose_event_for_screen ();
			}
			break;
		case 2 :
			screen.state = IN_MOVE;
			screen.last_x = event->x;
			screen.last_y = event->y;
			cursor = gdk_cursor_new(GDK_FLEUR);
			gdk_window_set_cursor(drawing_area_window, cursor);
			gdk_cursor_destroy(cursor);
			break;
		case 3 :
			if (screen.tool == POINTER) {
				/* if no items are selected, try and find the item the user
				   is pointing at */
				if (selection_length (&screen.selectionInfo) == 0) {
					gint index=callbacks_get_selected_row_index();
					if ((index >= 0) && 
					    (index <= mainProject->last_loaded) &&
					    (mainProject->file[index]->isVisible)) {
					  render_fill_selection_buffer_from_mouse_click(
							  event->x, event->y,
							  index, SELECTION_REPLACE);
					} else {
					    selection_clear (&screen.selectionInfo);
					    update_selected_object_message (FALSE);
					    render_refresh_rendered_image_on_screen ();
					}
				}

				/* only show the popup if we actually have something selected now */
				if (selection_length (&screen.selectionInfo) != 0) {
					update_selected_object_message (TRUE);
					gtk_menu_popup(GTK_MENU(screen.win.drawWindowPopupMenu), NULL, NULL, NULL, NULL, 
							event->button, event->time);
				}

			} else {
				/* Zoom outline mode initiated */
				screen.state = IN_ZOOM_OUTLINE;
				screen.start_x = event->x;
				screen.start_y = event->y;
				screen.centered_outline_zoom = event->state & GDK_SHIFT_MASK;
				cursor = gdk_cursor_new(GDK_SIZING);
				gdk_window_set_cursor(drawing_area_window, cursor);
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

static gboolean
check_align_files_possibility (gerbv_selection_info_t *sel_info)
{
	gerbv_fileinfo_t **f = mainProject->file;
	GtkMenuItem **menu_items = (GtkMenuItem **) screen.win.curEditAlingItem;
	gerbv_selection_item_t si[2];
	int id[2] = {-1, -1};
	int i;

	/* If has two objects, then can do files aligning */
	if (selection_length (sel_info) == 2) {
		si[0] = selection_get_item_by_index(sel_info, 0);
		si[1] = selection_get_item_by_index(sel_info, 1);

		for (i = 0; i <= mainProject->last_loaded; i++) {
			if (f[i]->image == si[0].image)
				id[0] = i;
						
			if (f[i]->image == si[1].image)
				id[1] = i;
		}

		/* Can align if on different files */
		if (id[0]*id[1] >= 0 && id[0] != id[1]) {
			gchar *str;

/* TODO: add color boxes for layers as hint */

			/* Update align menu items */
			str = g_strdup_printf (_("#_%i %s  >  #%i %s"),
					id[0]+1, f[id[0]]->name,
					id[1]+1, f[id[1]]->name);
			gtk_menu_item_set_label (menu_items[0], str);
			g_free (str);

			str = g_strdup_printf (_("#_%i %s  >  #%i %s"),
					id[1]+1, f[id[1]]->name,
					id[0]+1, f[id[0]]->name);
			gtk_menu_item_set_label (menu_items[1], str);
			g_free (str);

			gtk_widget_set_sensitive (
				screen.win.curEditAlingMenuItem, TRUE);

			return TRUE;
		}
	}

	/* Can't align, disable align menu */
	gtk_widget_set_sensitive (screen.win.curEditAlingMenuItem, FALSE);
	gtk_menu_item_set_label (menu_items[0], "");
	gtk_menu_item_set_label (menu_items[1], "");

	return FALSE;
}

/** The edit -> align layers menu item was selected.  Align first to second or
  * second to first layers by selected elements */
void
callbacks_align_files_from_sel_clicked (
		GtkMenuItem *menu_item, gpointer user_data)
{
	gerbv_fileinfo_t *fi[2];
	gerbv_selection_item_t item[2];
	gerbv_net_t *net;
	gerbv_selection_info_t *sel_info = &screen.selectionInfo;
	int align_second_to_first = GPOINTER_TO_INT(user_data);
	gdouble x[2], y[2];
	int i;

	if (selection_length (sel_info) != 2)
		return;

	item[0] = selection_get_item_by_index(sel_info, 0);
	item[1] = selection_get_item_by_index(sel_info, 1);

	fi[0] = gerbv_get_fileinfo_for_image (item[0].image, mainProject);
	fi[1] = gerbv_get_fileinfo_for_image (item[1].image, mainProject);

	if (fi[0] == NULL || fi[1] == NULL || fi[0] == fi[1])
		return;

	/* Calculate aligning coords */
	for (i = 0; i < 2; i++) {
		net = item[i].net;

		switch (net->aperture_state) {
		case GERBV_APERTURE_STATE_FLASH:
			x[i] = net->stop_x;
			y[i] = net->stop_y;
			break;
		case GERBV_APERTURE_STATE_ON:
			switch (net->interpolation) {
			case GERBV_INTERPOLATION_LINEARx1:
			case GERBV_INTERPOLATION_LINEARx10:
			case GERBV_INTERPOLATION_LINEARx01:
			case GERBV_INTERPOLATION_LINEARx001:
				x[i] = (net->stop_x + net->start_x)/2;
				y[i] = (net->stop_y + net->start_y)/2;
				break;
			case GERBV_INTERPOLATION_CW_CIRCULAR:
			case GERBV_INTERPOLATION_CCW_CIRCULAR:
				x[i] = net->cirseg->cp_x;
				y[i] = net->cirseg->cp_y;
				break;
			default:
				GERB_COMPILE_ERROR (_("Can't align by this "
							"type of object"));
				return;
			}
			break;
		default:
			GERB_COMPILE_ERROR (_("Can't align by this "
						"type of object"));
			return;
		}

		gerbv_transform_coord_for_image(x + i, y + i,
				item[i].image, mainProject);
	}

	if (align_second_to_first) {
		fi[1]->transform.translateX += x[0] - x[1];
		fi[1]->transform.translateY += y[0] - y[1];
	} else {
		fi[0]->transform.translateX += x[1] - x[0];
		fi[0]->transform.translateY += y[1] - y[0];
	}

	render_refresh_rendered_image_on_screen ();
	callbacks_update_layer_tree ();
}

/* --------------------------------------------------------- */
gboolean
callbacks_drawingarea_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
	gint index;

	if (event->type != GDK_BUTTON_RELEASE)
		return TRUE;

	switch (screen.state) {
	case IN_MOVE:
		screen.off_x = 0;
		screen.off_y = 0;
		render_refresh_rendered_image_on_screen ();
		callbacks_switch_to_normal_tool_cursor (screen.tool);
		break;

	case IN_ZOOM_OUTLINE:
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			render_zoom_display (ZOOM_OUT_CMOUSE, 0,
					event->x, event->y);
		}
		/* if the user just clicks without dragging, then simply
		   zoom in a preset amount */
		else if ((abs(screen.start_x - event->x) < 4) &&
				(abs(screen.start_y - event->y) < 4)) {
			render_zoom_display (ZOOM_IN_CMOUSE, 0,
					event->x, event->y);
		} else {
			render_calculate_zoom_from_outline (widget, event);
		}
		callbacks_switch_to_normal_tool_cursor (screen.tool);
		break;

	case IN_SELECTION_DRAG:
		/* selection will only work with cairo, so do nothing if it's
		   not compiled */
		index = callbacks_get_selected_row_index ();

		if ((index >= 0) && mainProject->file[index]->isVisible) {
			enum selection_action sel_action = SELECTION_REPLACE;
			
			if (event->state & GDK_SHIFT_MASK)
				sel_action = SELECTION_ADD;
			else if (event->state & GDK_CONTROL_MASK)
				sel_action = SELECTION_TOGGLE;

			/* determine if this was just a click or a box drag */
			if ((fabs((double)(screen.last_x - screen.start_x)) < 5)
			 && (fabs((double)(screen.last_y - screen.start_y)) < 5)) {
				render_fill_selection_buffer_from_mouse_click (
						event->x, event->y, index,
						sel_action);
			} else {
				render_fill_selection_buffer_from_mouse_drag (
						event->x, event->y,
						screen.start_x, screen.start_y,
						index, sel_action);
			}

			/* Check if anything was selected */
			update_selected_object_message (TRUE);

			check_align_files_possibility (&screen.selectionInfo);
		} else {
			render_refresh_rendered_image_on_screen ();
		}
		break;
	default:
		break;
	}

	screen.state = NORMAL;

	return TRUE;
} /* button_release_event */

/* --------------------------------------------------------- */
gboolean
callbacks_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (event->keyval) {
	case GDK_Escape:
		if (screen.tool == POINTER) {
			selection_clear (&screen.selectionInfo);
			update_selected_object_message (FALSE);
		}

		/* Escape may be used to abort outline zoom and just plain
		 * repaint */
		screen.state = NORMAL;
		render_refresh_rendered_image_on_screen();

		break;
	default:
		break;
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
	gdouble delta = hypot(dx, dy);
	
	if (screen.unit == GERBV_MILS) {
	    utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     _("Measured distance: %8.2f mils (%8.2f x, %8.2f y)"),
		     COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));
	} 
	else if (screen.unit == GERBV_MMS) {
	    utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     _("Measured distance: %8.3f mm (%8.3f x, %8.3f y)"),
		     COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));
	}
	else {
	    utf8_snprintf(screen.statusbar.diststr, MAX_DISTLEN,
		     _("Measured distance: %4.5f inches (%4.5f x, %4.5f y)"),
		     COORD2INS(delta), COORD2INS(dx), COORD2INS(dy));
	}
	callbacks_update_statusbar();
}

/* --------------------------------------------------------- */
void
callbacks_sidepane_render_type_combo_box_changed (GtkComboBox *widget, gpointer user_data) {
	gerbv_render_types_t type = gtk_combo_box_get_active (widget);
	
	dprintf ("%s():  type = %d\n", __FUNCTION__, type);

	if (type < 0 || type == screenRenderInfo.renderType)
		return;

	screenRenderInfo.renderType = type;
	callbacks_render_type_changed ();
}

/* --------------------------------------------------------- */
void
callbacks_viewmenu_rendertype_changed (GtkCheckMenuItem *widget, gpointer user_data) {
	gerbv_render_types_t type = GPOINTER_TO_INT(user_data);

	if (type == screenRenderInfo.renderType)
		return;

	dprintf ("%s():  type = %d\n", __FUNCTION__, type);

	screenRenderInfo.renderType = type;
	callbacks_render_type_changed ();
}

/* --------------------------------------------------------- */
void
callbacks_viewmenu_units_changed (GtkCheckMenuItem *widget, gpointer user_data) {
	gerbv_gui_unit_t unit = GPOINTER_TO_INT(user_data);

	if (unit < 0 || unit == screen.unit)
		return;

	dprintf ("%s():  unit = %d, screen.unit = %d\n", __FUNCTION__, unit, screen.unit);

	callbacks_units_changed (unit);
}

/* --------------------------------------------------------- */
void
callbacks_statusbar_unit_combo_box_changed (GtkComboBox *widget, gpointer user_data) {
	gerbv_gui_unit_t unit = gtk_combo_box_get_active (widget);
	int force_change = GPOINTER_TO_INT (user_data);

	if (!force_change && (unit < 0 || unit == screen.unit))
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
		      const gchar *message, gpointer user_data)
{
	GtkTextBuffer *textbuffer = NULL;
	GtkTextIter iter;
	GtkTextTag *tag;
	GtkTextMark *StartMark = NULL, *StopMark = NULL;
	GtkTextIter StartIter, StopIter;
	GtkWidget *dialog, *label;

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
	if (log_level & G_LOG_FLAG_FATAL) {
		fprintf(stderr, _("Fatal error: %s\n"), message);

		/* Try to show dialog box with error message */
		dialog = gtk_dialog_new_with_buttons(_("Fatal Error"),
			NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

		label = gtk_label_new(g_strdup_printf(_("Fatal error: %s"), message));
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
				label);
		gtk_label_set_selectable(GTK_LABEL(label), TRUE);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
				gtk_label_new(_("\nGerbv will be closed now!")));

		gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

		gtk_widget_show_all(dialog);
		gtk_dialog_run(GTK_DIALOG(dialog));
	}

	gtk_text_buffer_insert(textbuffer, &iter, message, -1);
	gtk_text_buffer_insert(textbuffer, &iter, "\n", -1);

	/* Scroll view to inserted text */
	g_signal_emit_by_name(textbuffer, "paste-done", NULL);

	gtk_text_buffer_get_end_iter(textbuffer, &iter);

	StopMark = gtk_text_buffer_create_mark(textbuffer,
					   "NewTextStop", &iter, TRUE);

	gtk_text_buffer_get_iter_at_mark(textbuffer, &StartIter, StartMark);
	gtk_text_buffer_get_iter_at_mark(textbuffer, &StopIter, StopMark);

	gtk_text_buffer_apply_tag(textbuffer, tag, &StartIter, &StopIter);
}

/* --------------------------------------------------------- */
void callbacks_force_expose_event_for_screen (void)
{
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

static double screen_units(double d)
{
	switch (screen.unit) {
	case GERBV_INS:
		return COORD2INS(d);
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

static const char *screen_units_str(void)
{
	/* NOTE: in order of gerbv_gui_unit_t */
	const char *units_str[] = {N_("mil"), N_("mm"), N_("in")};

	return _(units_str[screen.unit]);
}

static double line_length(double x0, double y0, double x1, double y1)
{
	double dx = x0 - x1;
	double dy = y0 - y1;

	return hypot(dx, dy);
}

static double arc_length(double dia, double angle)
{
	return M_PI*dia*(angle/360.0);
}

static void aperture_state_report (gerbv_net_t *net,
		gerbv_image_t *img, gerbv_project_t *prj)
{
	gerbv_layertype_t layer_type = img->layertype;

	gboolean show_length = FALSE;
	gboolean aperture_is_valid = FALSE;
	double x, y, len = 0;

	if (net->aperture > 0)
		aperture_is_valid = TRUE;

	switch (net->aperture_state) {

	case GERBV_APERTURE_STATE_OFF:
		break;

	case GERBV_APERTURE_STATE_ON:
		switch (net->interpolation) {

		case GERBV_INTERPOLATION_LINEARx1:
		case GERBV_INTERPOLATION_LINEARx10:
		case GERBV_INTERPOLATION_LINEARx01:
		case GERBV_INTERPOLATION_LINEARx001:
			if (layer_type != GERBV_LAYERTYPE_DRILL)
				g_message (_("Object type: Line"));
			else
				g_message (_("Object type: Slot (drilled)"));

			len = line_length(net->start_x, net->start_y,
					net->stop_x, net->stop_y);
			show_length = 1;

			break;

		case GERBV_INTERPOLATION_CW_CIRCULAR:
		case GERBV_INTERPOLATION_CCW_CIRCULAR:
			g_message (_("Object type: Arc"));
			len = arc_length(net->cirseg->width,
					fabs(net->cirseg->angle1 -
						net->cirseg->angle2));
			show_length = 1;

			break;
		default:
			g_message (_("Object type: Unknown"));
			break;
		}

		if (layer_type != GERBV_LAYERTYPE_DRILL)
			g_message (_("    Exposure: On"));

		if (aperture_is_valid) {
			if (layer_type != GERBV_LAYERTYPE_DRILL)
				aperture_report(img->aperture, net->aperture,
						net->start_x, net->start_y,
						img, prj);
			else
				drill_report(img->aperture, net->aperture);
		}

		x = net->start_x;
		y = net->start_y;
		gerbv_transform_coord_for_image(&x, &y, img, prj);
		g_message (_("    Start: (%g, %g) %s"),
				screen_units(x),
				screen_units(y),
				screen_units_str());

		x = net->stop_x;
		y = net->stop_y;
		gerbv_transform_coord_for_image(&x, &y, img, prj);
		g_message (_("    Stop: (%g, %g) %s"),
				screen_units(x),
				screen_units(y),
				screen_units_str());

		switch (net->interpolation) {

		case GERBV_INTERPOLATION_CW_CIRCULAR:
		case GERBV_INTERPOLATION_CCW_CIRCULAR:
			x = net->cirseg->cp_x;
			y = net->cirseg->cp_y;
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Center: (%g, %g) %s"),
					screen_units(x),
					screen_units(y),
					screen_units_str());

			x = net->cirseg->width/2;
			y = x;
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Radius: %g %s"),
					screen_units(x),
					screen_units_str());

			g_message (_("    Angle: %g deg"),
					fabs(net->cirseg->angle1 -
						net->cirseg->angle2));
			g_message (_("    Angles: (%g, %g) deg"),
					net->cirseg->angle1,
					net->cirseg->angle2);
			g_message (_("    Direction: %s"),
					(net->interpolation ==
					 GERBV_INTERPOLATION_CW_CIRCULAR)?
						_("CW"): _("CCW"));
			break;

		default:
			break;
		}

		if (show_length) {
			gerbv_aperture_t *aper = img->aperture[net->aperture];

			if (layer_type == GERBV_LAYERTYPE_DRILL
			&&  aperture_is_valid
			&&  aper->type == GERBV_APTYPE_CIRCLE) {
				double dia = aper->parameter[0];
				g_message (_("    Slot length: %g %s"),
						screen_units(len + dia),
						screen_units_str());
			}

			screen.length_sum += len;
			g_message (_("    Length: %g (sum: %g) %s"),
					screen_units(len),
					screen_units(screen.length_sum),
					screen_units_str());
		}

		net_layer_file_report (net, img, prj);

		break;

	case GERBV_APERTURE_STATE_FLASH:
		if (layer_type != GERBV_LAYERTYPE_DRILL)
			g_message (_("Object type: Flashed aperture"));
		else
			g_message (_("Object type: Drill"));

		if (aperture_is_valid) {
			if (layer_type != GERBV_LAYERTYPE_DRILL)
				aperture_report(img->aperture, net->aperture,
						net->stop_x, net->stop_y,
						img, prj);
			else
				drill_report(img->aperture, net->aperture);
		}

		x = net->stop_x;
		y = net->stop_y;
		gerbv_transform_coord_for_image(&x, &y, img, prj);
		g_message (_("    Location: (%g, %g) %s"),
				screen_units(x),
				screen_units(y),
				screen_units_str());

		net_layer_file_report (net, img, prj);

		break;
	}
}

static void
aperture_report(gerbv_aperture_t *apertures[], int aperture_num,
		double x, double y, gerbv_image_t *img, gerbv_project_t *prj)
{
	gerbv_aperture_type_t type = apertures[aperture_num]->type;
	double *params = apertures[aperture_num]->parameter;
	gerbv_simplified_amacro_t *sam = apertures[aperture_num]->simplified;

	g_message (_("    Aperture used: D%d"), aperture_num);
	g_message (_("    Aperture type: %s"),
		(type == GERBV_APTYPE_MACRO)?
			_(gerbv_aperture_type_name(sam->type)):
			_(gerbv_aperture_type_name(type)));

	switch (type) {
	case GERBV_APTYPE_CIRCLE:
		g_message (_("    Diameter: %g %s"),
			screen_units(params[0]),
			screen_units_str());
		break;
	case GERBV_APTYPE_RECTANGLE:
	case GERBV_APTYPE_OVAL:
		g_message (_("    Dimensions: %gx%g %s"),
			screen_units(params[0]),
			screen_units(params[1]),
			screen_units_str());
		break;
	case GERBV_APTYPE_MACRO: {
		switch (sam->type) {
		case GERBV_APTYPE_MACRO_CIRCLE:
			g_message (_("    Diameter: %g %s"),
				screen_units(sam->parameter[CIRCLE_DIAMETER]),
				screen_units_str());
			x += sam->parameter[CIRCLE_CENTER_X];
			y += sam->parameter[CIRCLE_CENTER_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Center: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			break;

		case GERBV_APTYPE_MACRO_OUTLINE:
			g_message (_("    Number of points: %g"),
				sam->parameter[OUTLINE_NUMBER_OF_POINTS]);
			x += sam->parameter[OUTLINE_FIRST_X];
			y += sam->parameter[OUTLINE_FIRST_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Start: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			g_message (_("    Rotation: %g deg"),
				sam->parameter[OUTLINE_ROTATION_IDX(sam->parameter)]);
			break;

		case GERBV_APTYPE_MACRO_POLYGON:
			g_message (_("    Number of points: %g"),
				sam->parameter[POLYGON_NUMBER_OF_POINTS]);
			g_message (_("    Diameter: %g %s"),
				screen_units(sam->parameter[POLYGON_DIAMETER]),
				screen_units_str());
			x += sam->parameter[POLYGON_CENTER_X];
			y += sam->parameter[POLYGON_CENTER_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Center: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			g_message (_("    Rotation: %g deg"),
				sam->parameter[POLYGON_ROTATION]);
			break;

		case GERBV_APTYPE_MACRO_MOIRE:
			g_message (_("    Outside diameter: %g %s"),
				screen_units(sam->parameter[MOIRE_OUTSIDE_DIAMETER]),
				screen_units_str());
			g_message (_("    Ring thickness: %g %s"),
				screen_units(sam->parameter[MOIRE_CIRCLE_THICKNESS]),
				screen_units_str());
			g_message (_("    Gap width: %g %s"),
				screen_units(sam->parameter[MOIRE_GAP_WIDTH]),
				screen_units_str());
			g_message (_("    Number of rings: %g"),
				sam->parameter[MOIRE_NUMBER_OF_CIRCLES]);
			g_message (_("    Crosshair thickness: %g %s"),
				screen_units(
					sam->parameter[MOIRE_CROSSHAIR_THICKNESS]),
				screen_units_str());
			g_message (_("    Crosshair length: %g %s"),
				screen_units(sam->parameter[MOIRE_CROSSHAIR_LENGTH]),
				screen_units_str());
			x += sam->parameter[MOIRE_CENTER_X];
			y += sam->parameter[MOIRE_CENTER_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Center: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			g_message (_("    Rotation: %g deg"),
				sam->parameter[MOIRE_ROTATION]);
			break;

		case GERBV_APTYPE_MACRO_THERMAL:
			g_message (_("    Outside diameter: %g %s"),
				screen_units(sam->parameter[THERMAL_OUTSIDE_DIAMETER]),
				screen_units_str());
			g_message (_("    Inside diameter: %g %s"),
				screen_units(sam->parameter[THERMAL_INSIDE_DIAMETER]),
				screen_units_str());
			g_message (_("    Crosshair thickness: %g %s"),
				screen_units(
					sam->parameter[THERMAL_CROSSHAIR_THICKNESS]),
				screen_units_str());
			x += sam->parameter[THERMAL_CENTER_X];
			y += sam->parameter[THERMAL_CENTER_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Center: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			g_message (_("    Rotation: %g deg"),
				sam->parameter[THERMAL_ROTATION]);
			break;

		case GERBV_APTYPE_MACRO_LINE20:
			g_message (_("    Width: %g %s"),
				screen_units(sam->parameter[LINE20_WIDTH]),
				screen_units_str());
			x += sam->parameter[LINE20_START_X];
			y += sam->parameter[LINE20_START_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Start: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			x += sam->parameter[LINE20_END_X];
			y += sam->parameter[LINE20_END_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Stop: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			g_message (_("    Rotation: %g deg"),
					sam->parameter[LINE20_ROTATION]);
			break;

		case GERBV_APTYPE_MACRO_LINE21:
			g_message (_("    Width: %g %s"),
				screen_units(sam->parameter[LINE21_WIDTH]),
				screen_units_str());
			g_message (_("    Height: %g %s"),
				screen_units(sam->parameter[LINE21_HEIGHT]),
				screen_units_str());
			x += sam->parameter[LINE21_CENTER_X];
			y += sam->parameter[LINE21_CENTER_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Center: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			g_message (_("    Rotation: %g deg"),
					sam->parameter[LINE21_ROTATION]);
			break;

		case GERBV_APTYPE_MACRO_LINE22:
			g_message (_("    Width: %g %s"),
				screen_units(sam->parameter[LINE22_WIDTH]),
				screen_units_str());
			g_message (_("    Height: %g %s"),
				screen_units(sam->parameter[LINE22_HEIGHT]),
				screen_units_str());
			x += sam->parameter[LINE22_LOWER_LEFT_X];
			y += sam->parameter[LINE22_LOWER_LEFT_Y];
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Lower left: (%g, %g) %s"),
				screen_units(x), screen_units(y),
				screen_units_str());
			g_message (_("    Rotation: %g deg"),
					sam->parameter[LINE22_ROTATION]);
			break;

		default:
			break;
		}
		break;
	}
	default:
		break;
	}
}

static void drill_report(gerbv_aperture_t *apertures[], int aperture_num)
{
	gerbv_aperture_type_t type = apertures[aperture_num]->type;
	double *params = apertures[aperture_num]->parameter;

	g_message (_("    Tool used: T%d"), aperture_num);
	if (type == GERBV_APTYPE_CIRCLE)
		g_message (_("    Diameter: %g %s"),
				screen_units(params[0]),
				screen_units_str());
}

static void parea_report (gerbv_net_t *net,
		gerbv_image_t *img, gerbv_project_t *prj)
{
	gerbv_net_t *n;
	unsigned int c = 0;
	gerbv_interpolation_t inter_prev;
	double x, y;

	if (net->interpolation != GERBV_INTERPOLATION_PAREA_START)
		return;

	/* Count vertices */
	for (gerbv_net_t *n = net->next; n != NULL; n = n->next) {
		if (n->interpolation == GERBV_INTERPOLATION_PAREA_END)
			break;
		c++;
	}

	g_message (_("    Number of vertices: %u"), c - 1);

	for (n = net->next, inter_prev = net->interpolation;
			n != NULL
			&& n->interpolation != GERBV_INTERPOLATION_PAREA_END;
			n = n->next) {

		switch (n->interpolation) {

		case GERBV_INTERPOLATION_LINEARx1:

			if (inter_prev != n->interpolation) {
				x = n->start_x;
				y = n->start_y;
				gerbv_transform_coord_for_image(&x, &y,
						img, prj);
				g_message (_("    Line from: (%g, %g) %s"),
						screen_units(x),
						screen_units(y),
						screen_units_str());
			}

			x = n->stop_x;
			y = n->stop_y;
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("        Line to: (%g, %g) %s"),
					screen_units(x), screen_units(y),
					screen_units_str());
			break;

		case GERBV_INTERPOLATION_CW_CIRCULAR:
		case GERBV_INTERPOLATION_CCW_CIRCULAR:

			x = n->start_x;
			y = n->start_y;
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("    Arc from: (%g, %g) %s"),
					screen_units(x), screen_units(y),
					screen_units_str());

			x = n->stop_x;
			y = n->stop_y;
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("        Arc to: (%g, %g) %s"),
					screen_units(x), screen_units(y),
					screen_units_str());

			x = n->cirseg->cp_x;
			y = n->cirseg->cp_y;
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("        Center: (%g, %g) %s"),
					screen_units(x), screen_units(y),
					screen_units_str());

			x = n->cirseg->width;
			y = n->cirseg->height;
			gerbv_transform_coord_for_image(&x, &y, img, prj);
			g_message (_("        Radius: %g %s"),
					screen_units(x)/2, screen_units_str());

			g_message (_("        Angle: %g deg"),
				fabs(n->cirseg->angle1 - n->cirseg->angle2));
			g_message (_("        Angles: (%g, %g) deg"),
					n->cirseg->angle1, n->cirseg->angle2);
			g_message (_("        Direction: %s"),
					(n->interpolation ==
					 GERBV_INTERPOLATION_CW_CIRCULAR)?
						_("CW"): _("CCW"));
			break;

		default:
			g_message("       Skipping interpolation: %s",
				_(gerbv_interpolation_name(n->interpolation)));
		}

		inter_prev = n->interpolation;
	}
}

static void net_layer_file_report(gerbv_net_t *net,
		gerbv_image_t *img, gerbv_project_t *prj)
{
	/* Don't report "no net" to keep log short */
	if (net->label != NULL)
		g_message (_("    Net label: %s"), net->label->str);

	/* Don't report "no layer name" to keep log short */
	if (net->layer->name != NULL)
		g_message (_("    Layer name: %s"), net->layer->name);
 
	/* Search file name in project files array */
	for (int i = 0; i <= prj->last_loaded; i++) {
		if (img == prj->file[i]->image)
			g_message (_("    In file: %s"), prj->file[i]->name);
	}
}

/* Restore report window size and postion */
static void
analyze_window_size_restore(GtkWidget *win)
{
	GVariant *var;
	const gint32 *xy;
	gsize num;

	if (!screen.settings)
		return;

	var = g_settings_get_value (screen.settings, "analyze-window-size");
	xy = g_variant_get_fixed_array (var, &num, sizeof (*xy));
	if (num == 2)
		gtk_window_set_default_size (GTK_WINDOW (win), xy[0], xy[1]);
	g_variant_unref (var);

	var = g_settings_get_value (screen.settings, "analyze-window-position");
	xy = g_variant_get_fixed_array (var, &num, sizeof (*xy));
	if (num == 2)
		gtk_window_move (GTK_WINDOW (win), xy[0], xy[1]);
	g_variant_unref (var);
}

/* Store report window size and postion */
static void
analyze_window_size_store(GtkWidget *win, gpointer user_data)
{
	gint32 xy[2];
	GVariant *var;
	gboolean is_max;

	if (!screen.settings)
		return;

	is_max = FALSE != (GDK_WINDOW_STATE_MAXIMIZED &
			gdk_window_get_state (gtk_widget_get_window (win)));
	if (is_max)
		return;

	gtk_window_get_size (GTK_WINDOW (win), (gint *)xy, (gint *)(xy+1));
	var = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32,
			xy, 2, sizeof (xy[0]));
	g_settings_set_value (screen.settings, "analyze-window-size", var);

	gtk_window_get_position (GTK_WINDOW (win),
			(gint *)xy, (gint *)(xy+1));
	var = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32,
			xy, 2, sizeof (xy[0]));
	g_settings_set_value (screen.settings, "analyze-window-position", var);
}
