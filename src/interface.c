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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "support.h"
#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"
#include "draw.h"
#include "color.h"
#include "gerbv_screen.h"
#include "gerbv_icon.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "callbacks.h"
#include "interface.h"

#define WIN_TITLE "Gerber Viewer"

void
rename_main_window(char *filename, GtkWidget *main_win)
{
	GString *win_title=g_string_new(NULL);
	static GtkWidget *win=NULL;

	if( main_win != NULL )
	win = main_win;

	g_assert(win != NULL);

	g_string_printf (win_title,"%s%s: %s", WIN_TITLE, VERSION, filename);
	gtk_window_set_title(GTK_WINDOW(win), win_title->str);
	g_string_free(win_title,TRUE);			 
}

void
set_window_icon (GtkWidget * this_window)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	pixmap = gdk_pixmap_create_from_xpm_d (this_window->window, &mask,
	&this_window->style->bg[GTK_STATE_NORMAL], gerbv_icon_xpm);
	gdk_window_set_icon (this_window->window, NULL, pixmap, mask);

	return;
}

void
interface_create_gui (int req_width, int req_height)
{
	GtkWidget *window1;
	GtkWidget *vbox1;
	GtkWidget *menubar1;
	GtkWidget *menuitem7;
	GtkWidget *menuitem7_menu;
	GtkWidget *new1;
	GtkWidget *open_project1;
	GtkWidget *image26;
	GtkWidget *open_layer1;
	GtkWidget *revert1;
	GtkWidget *save1;
	GtkWidget *save_as1;
	GtkWidget *separatormenuitem1;
	GtkWidget *import1;
	GtkWidget *import1_menu;
	GtkWidget *postscript1;
	GtkWidget *png1;
	GtkWidget *pdf1;
	GtkWidget *svg1;
	GtkWidget *separator1;
	GtkWidget *print1;
	GtkWidget *separator2;
	GtkWidget *project_properties1;
	GtkWidget *gerber_properties1;
	GtkWidget *image27;
	GtkWidget *separator4;
	GtkWidget *quit1;
	GtkWidget *menuitem9;
	GtkWidget *menuitem9_menu;
	GtkWidget *zoom_in1;
	GtkWidget *zoom_out1;
	GtkWidget *separator5;
	GtkWidget *fit_to_window1;
	GtkWidget *tools1;
	GtkWidget *tools1_menu;
	GtkWidget *pointer_tool1;
	GtkWidget *zoom_tool1;
	GtkWidget *measure_tool1;
	GtkWidget *menuitem10;
	GtkWidget *menuitem10_menu;
	GtkWidget *online_manual1;
	GtkWidget *about1;
	GtkWidget *image28;
	GtkWidget *hbox2;
	GtkWidget *handlebox2;
	GtkWidget *toolbar1;
	GtkIconSize tmp_toolbar_icon_size;
	GtkWidget *toolbutton1;
	GtkWidget *toolbutton11;
	GtkWidget *toolbutton12;
	GtkWidget *toolbutton6;
	GtkWidget *separatortoolitem2;
	GtkWidget *toolbutton7;
	GtkWidget *separatortoolitem1;
	GtkWidget *toolbutton8;
	GtkWidget *toolbutton9;
	GtkWidget *tmp_image;
	GtkWidget *toolbutton13;
	GtkWidget *handlebox3;
	GtkWidget *toolbar2;
	GtkWidget *toolitem1;
	GtkWidget *togglebutton1;
	GtkWidget *image6;
	GtkWidget *toolitem2;
	GtkWidget *togglebutton2;
	GtkWidget *image5;
	GtkWidget *toolitem3;
	GtkWidget *togglebutton3;
	GtkWidget *image7;
	GtkWidget *hpaned1;
	GtkWidget *vbox3;
	GtkWidget *vbox10;
	GtkWidget *hbox4;
	GtkWidget *label1;
	GtkWidget *combobox1;
	GtkWidget *scrolledwindow1;
	GtkWidget *hbox1;
	GtkWidget *button4;
	GtkWidget *image8;
	GtkWidget *button5;
	GtkWidget *image9;
	GtkWidget *button6;
	GtkWidget *image10;
	GtkWidget *button7;
	GtkWidget *image11;
	GtkWidget *vbox2;
	GtkWidget *table1;
	GtkWidget *hruler1;
	GtkWidget *vruler1;
	GtkWidget *statusbar1;
	GtkAccelGroup *accel_group;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new ();

	accel_group = gtk_accel_group_new ();

	window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window1), _("window1"));

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (window1), vbox1);

	menubar1 = gtk_menu_bar_new ();
	gtk_widget_show (menubar1);
	gtk_box_pack_start (GTK_BOX (vbox1), menubar1, FALSE, FALSE, 0);

	menuitem7 = gtk_menu_item_new_with_mnemonic (_("_File"));
	gtk_widget_show (menuitem7);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem7);

	menuitem7_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem7), menuitem7_menu);

	new1 = gtk_image_menu_item_new_from_stock ("gtk-new", accel_group);
	gtk_widget_show (new1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), new1);
	gtk_widget_add_accelerator (new1, "activate", accel_group,
	                  GDK_n, (GdkModifierType) GDK_CONTROL_MASK,
	                  GTK_ACCEL_VISIBLE);

	open_project1 = gtk_image_menu_item_new_with_mnemonic (_("_Open Project"));
	gtk_widget_show (open_project1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), open_project1);
	gtk_tooltips_set_tip (tooltips, open_project1, _("Open an existing Gerber Viewer project"), NULL);

	image26 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image26);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (open_project1), image26);

	open_layer1 = gtk_menu_item_new_with_mnemonic (_("Open _Layer"));
	gtk_widget_show (open_layer1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), open_layer1);
	gtk_tooltips_set_tip (tooltips, open_layer1, _("Open a Gerber, drill, or pick and place file"), NULL);

	revert1 = gtk_image_menu_item_new_from_stock ("gtk-revert-to-saved", accel_group);
	gtk_widget_show (revert1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), revert1);

	save1 = gtk_image_menu_item_new_from_stock ("gtk-save", accel_group);
	gtk_widget_show (save1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), save1);

	save_as1 = gtk_image_menu_item_new_from_stock ("gtk-save-as", accel_group);
	gtk_widget_show (save_as1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), save_as1);

	separatormenuitem1 = gtk_separator_menu_item_new ();
	gtk_widget_show (separatormenuitem1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), separatormenuitem1);
	gtk_widget_set_sensitive (separatormenuitem1, FALSE);

	import1 = gtk_menu_item_new_with_mnemonic (_("_Export"));
	gtk_widget_show (import1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), import1);

	import1_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (import1), import1_menu);

	postscript1 = gtk_menu_item_new_with_mnemonic (_("PostScript"));
	gtk_widget_show (postscript1);
	gtk_container_add (GTK_CONTAINER (import1_menu), postscript1);
	gtk_tooltips_set_tip (tooltips, postscript1, _("Export project to a PostScript file"), NULL);

	png1 = gtk_menu_item_new_with_mnemonic (_("PNG"));
	gtk_widget_show (png1);
	gtk_container_add (GTK_CONTAINER (import1_menu), png1);
	gtk_tooltips_set_tip (tooltips, png1, _("Export project to a PNG file"), NULL);

	pdf1 = gtk_menu_item_new_with_mnemonic (_("PDF"));
	gtk_widget_show (pdf1);
	gtk_container_add (GTK_CONTAINER (import1_menu), pdf1);
	gtk_tooltips_set_tip (tooltips, pdf1, _("Export project to a PDF file"), NULL);

	svg1 = gtk_menu_item_new_with_mnemonic (_("SVG"));
	gtk_widget_show (svg1);
	gtk_container_add (GTK_CONTAINER (import1_menu), svg1);
	gtk_tooltips_set_tip (tooltips, svg1, _("Export project to a SVG file"), NULL);

	separator1 = gtk_separator_menu_item_new ();
	gtk_widget_show (separator1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), separator1);
	gtk_widget_set_sensitive (separator1, FALSE);

	print1 = gtk_image_menu_item_new_from_stock ("gtk-print", accel_group);
	gtk_widget_show (print1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), print1);

	separator2 = gtk_separator_menu_item_new ();
	gtk_widget_show (separator2);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), separator2);
	gtk_widget_set_sensitive (separator2, FALSE);

	project_properties1 = gtk_menu_item_new_with_mnemonic (_("Pro_ject Properties"));
	gtk_widget_show (project_properties1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), project_properties1);
	gtk_tooltips_set_tip (tooltips, project_properties1, _("View the overall properties of the Gerber project"), NULL);

	gerber_properties1 = gtk_image_menu_item_new_with_mnemonic (_("La_yer Properties"));
	gtk_widget_show (gerber_properties1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), gerber_properties1);
	gtk_tooltips_set_tip (tooltips, gerber_properties1, _("View the properties of the selected layer"), NULL);

	image27 = gtk_image_new_from_stock ("gtk-properties", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image27);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (gerber_properties1), image27);

	separator4 = gtk_separator_menu_item_new ();
	gtk_widget_show (separator4);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), separator4);
	gtk_widget_set_sensitive (separator4, FALSE);

	quit1 = gtk_image_menu_item_new_from_stock ("gtk-quit", accel_group);
	gtk_widget_show (quit1);
	gtk_container_add (GTK_CONTAINER (menuitem7_menu), quit1);

	menuitem9 = gtk_menu_item_new_with_mnemonic (_("_View"));
	gtk_widget_show (menuitem9);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem9);

	menuitem9_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem9), menuitem9_menu);

	zoom_in1 = gtk_image_menu_item_new_from_stock ("gtk-zoom-in", accel_group);
	gtk_widget_show (zoom_in1);
	gtk_container_add (GTK_CONTAINER (menuitem9_menu), zoom_in1);

	zoom_out1 = gtk_image_menu_item_new_from_stock ("gtk-zoom-out", accel_group);
	gtk_widget_show (zoom_out1);
	gtk_container_add (GTK_CONTAINER (menuitem9_menu), zoom_out1);

	separator5 = gtk_separator_menu_item_new ();
	gtk_widget_show (separator5);
	gtk_container_add (GTK_CONTAINER (menuitem9_menu), separator5);
	gtk_widget_set_sensitive (separator5, FALSE);

	fit_to_window1 = gtk_menu_item_new_with_mnemonic (_("_Fit to Window"));
	gtk_widget_show (fit_to_window1);
	gtk_container_add (GTK_CONTAINER (menuitem9_menu), fit_to_window1);
	gtk_tooltips_set_tip (tooltips, fit_to_window1, _("Zoom to fit the project within the window"), NULL);

	tools1 = gtk_menu_item_new_with_mnemonic (_("_Tools"));
	gtk_widget_show (tools1);
	gtk_container_add (GTK_CONTAINER (menubar1), tools1);

	tools1_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (tools1), tools1_menu);

	pointer_tool1 = gtk_menu_item_new_with_mnemonic (_("_Pointer Tool"));
	gtk_widget_show (pointer_tool1);
	gtk_container_add (GTK_CONTAINER (tools1_menu), pointer_tool1);
	gtk_tooltips_set_tip (tooltips, pointer_tool1, _("The default tool"), NULL);
	gtk_widget_add_accelerator (pointer_tool1, "activate", accel_group,
	                  GDK_F1, (GdkModifierType) 0,
	                  GTK_ACCEL_VISIBLE);

	zoom_tool1 = gtk_menu_item_new_with_mnemonic (_("_Zoom Tool"));
	gtk_widget_show (zoom_tool1);
	gtk_container_add (GTK_CONTAINER (tools1_menu), zoom_tool1);
	gtk_tooltips_set_tip (tooltips, zoom_tool1, _("Zoom by drawing a rectangle"), NULL);
	gtk_widget_add_accelerator (zoom_tool1, "activate", accel_group,
	                  GDK_F2, (GdkModifierType) 0,
	                  GTK_ACCEL_VISIBLE);

	measure_tool1 = gtk_menu_item_new_with_mnemonic (_("_Measure Tool"));
	gtk_widget_show (measure_tool1);
	gtk_container_add (GTK_CONTAINER (tools1_menu), measure_tool1);
	gtk_tooltips_set_tip (tooltips, measure_tool1, _("Measure distances"), NULL);
	gtk_widget_add_accelerator (measure_tool1, "activate", accel_group,
	                  GDK_F3, (GdkModifierType) 0,
	                  GTK_ACCEL_VISIBLE);

	menuitem10 = gtk_menu_item_new_with_mnemonic (_("_Help"));
	gtk_widget_show (menuitem10);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem10);

	menuitem10_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem10), menuitem10_menu);

	online_manual1 = gtk_menu_item_new_with_mnemonic (_("_Online Manual"));
	gtk_widget_show (online_manual1);
	gtk_container_add (GTK_CONTAINER (menuitem10_menu), online_manual1);
	gtk_tooltips_set_tip (tooltips, online_manual1, _("View the online help documentation"), NULL);

	about1 = gtk_image_menu_item_new_with_mnemonic (_("_About GerberViewer"));
	gtk_widget_show (about1);
	gtk_container_add (GTK_CONTAINER (menuitem10_menu), about1);
	gtk_tooltips_set_tip (tooltips, about1, _("View information about this software"), NULL);

	image28 = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image28);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (about1), image28);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 0);

	handlebox2 = gtk_handle_box_new ();
	gtk_widget_show (handlebox2);
	gtk_box_pack_start (GTK_BOX (hbox2), handlebox2, TRUE, TRUE, 0);

	toolbar1 = gtk_toolbar_new ();
	gtk_widget_show (toolbar1);
	gtk_container_add (GTK_CONTAINER (handlebox2), toolbar1);
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar1), GTK_TOOLBAR_ICONS);
	tmp_toolbar_icon_size = gtk_toolbar_get_icon_size (GTK_TOOLBAR (toolbar1));

	toolbutton1 = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-new");
	gtk_widget_show (toolbutton1);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton1);

	toolbutton11 = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-open");
	gtk_widget_show (toolbutton11);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton11);

	toolbutton12 = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-revert-to-saved");
	gtk_widget_show (toolbutton12);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton12);

	toolbutton6 = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-save");
	gtk_widget_show (toolbutton6);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton6);

	separatortoolitem2 = (GtkWidget*) gtk_separator_tool_item_new ();
	gtk_widget_show (separatortoolitem2);
	gtk_container_add (GTK_CONTAINER (toolbar1), separatortoolitem2);

	toolbutton7 = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-print");
	gtk_widget_show (toolbutton7);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton7);

	separatortoolitem1 = (GtkWidget*) gtk_separator_tool_item_new ();
	gtk_widget_show (separatortoolitem1);
	gtk_container_add (GTK_CONTAINER (toolbar1), separatortoolitem1);

	toolbutton8 = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-zoom-in");
	gtk_widget_show (toolbutton8);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton8);

	toolbutton9 = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-zoom-out");
	gtk_widget_show (toolbutton9);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton9);

	tmp_image = gtk_image_new_from_stock ("gtk-zoom-in", tmp_toolbar_icon_size);
	gtk_widget_show (tmp_image);
	toolbutton13 = (GtkWidget*) gtk_tool_button_new (tmp_image, "");
	gtk_widget_show (toolbutton13);
	gtk_container_add (GTK_CONTAINER (toolbar1), toolbutton13);

	handlebox3 = gtk_handle_box_new ();
	gtk_widget_show (handlebox3);
	gtk_box_pack_start (GTK_BOX (hbox2), handlebox3, TRUE, TRUE, 0);

	toolbar2 = gtk_toolbar_new ();
	gtk_widget_show (toolbar2);
	gtk_container_add (GTK_CONTAINER (handlebox3), toolbar2);
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar2), GTK_TOOLBAR_BOTH);
	tmp_toolbar_icon_size = gtk_toolbar_get_icon_size (GTK_TOOLBAR (toolbar2));

	toolitem1 = (GtkWidget*) gtk_tool_item_new ();
	gtk_widget_show (toolitem1);
	gtk_container_add (GTK_CONTAINER (toolbar2), toolitem1);

	togglebutton1 = gtk_toggle_button_new ();
	gtk_widget_show (togglebutton1);
	gtk_container_add (GTK_CONTAINER (toolitem1), togglebutton1);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (togglebutton1), TRUE);

	image6 = gtk_image_new_from_stock ("gtk-apply", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image6);
	gtk_container_add (GTK_CONTAINER (togglebutton1), image6);

	toolitem2 = (GtkWidget*) gtk_tool_item_new ();
	gtk_widget_show (toolitem2);
	gtk_container_add (GTK_CONTAINER (toolbar2), toolitem2);

	togglebutton2 = gtk_toggle_button_new ();
	gtk_widget_show (togglebutton2);
	gtk_container_add (GTK_CONTAINER (toolitem2), togglebutton2);

	image5 = gtk_image_new_from_stock ("gtk-zoom-in", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image5);
	gtk_container_add (GTK_CONTAINER (togglebutton2), image5);

	toolitem3 = (GtkWidget*) gtk_tool_item_new ();
	gtk_widget_show (toolitem3);
	gtk_container_add (GTK_CONTAINER (toolbar2), toolitem3);

	togglebutton3 = gtk_toggle_button_new ();
	gtk_widget_show (togglebutton3);
	gtk_container_add (GTK_CONTAINER (toolitem3), togglebutton3);

	image7 = gtk_image_new_from_stock ("gtk-bold", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image7);
	gtk_container_add (GTK_CONTAINER (togglebutton3), image7);

	hpaned1 = gtk_hpaned_new ();
	gtk_widget_show (hpaned1);
	gtk_box_pack_start (GTK_BOX (vbox1), hpaned1, TRUE, TRUE, 0);
	gtk_paned_set_position (GTK_PANED (hpaned1), 200);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_paned_pack1 (GTK_PANED (hpaned1), vbox3, FALSE, TRUE);
	gtk_widget_set_size_request (vbox3, 92, -1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 5);

	vbox10 = gtk_vbox_new (FALSE, 3);
	gtk_widget_show (vbox10);
	gtk_box_pack_start (GTK_BOX (vbox3), vbox10, TRUE, TRUE, 0);
	gtk_widget_set_size_request (vbox10, 82, -1);

	hbox4 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox4);
	gtk_box_pack_start (GTK_BOX (vbox10), hbox4, FALSE, FALSE, 0);

	label1 = gtk_label_new (_("Render style: "));
	gtk_widget_show (label1);
	gtk_box_pack_start (GTK_BOX (hbox4), label1, FALSE, FALSE, 0);

	combobox1 = gtk_combo_box_new_text ();
	gtk_widget_show (combobox1);
	gtk_box_pack_start (GTK_BOX (hbox4), combobox1, TRUE, TRUE, 0);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Normal"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Subtract"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Overlap"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Xor"));

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_box_pack_start (GTK_BOX (vbox10), scrolledwindow1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow1), 2);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow1),
		GTK_SHADOW_IN);

	hbox1 = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox10), hbox1, FALSE, FALSE, 0);

	button4 = gtk_button_new ();
	gtk_widget_show (button4);
	gtk_box_pack_start (GTK_BOX (hbox1), button4, FALSE, TRUE, 0);

	image8 = gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image8);
	gtk_container_add (GTK_CONTAINER (button4), image8);

	button5 = gtk_button_new ();
	gtk_widget_show (button5);
	gtk_box_pack_start (GTK_BOX (hbox1), button5, FALSE, TRUE, 0);

	image9 = gtk_image_new_from_stock ("gtk-go-down", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image9);
	gtk_container_add (GTK_CONTAINER (button5), image9);

	button6 = gtk_button_new ();
	gtk_widget_show (button6);
	gtk_box_pack_start (GTK_BOX (hbox1), button6, FALSE, TRUE, 0);

	image10 = gtk_image_new_from_stock ("gtk-go-up", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image10);
	gtk_container_add (GTK_CONTAINER (button6), image10);

	button7 = gtk_button_new ();
	gtk_widget_show (button7);
	gtk_box_pack_start (GTK_BOX (hbox1), button7, FALSE, TRUE, 0);

	image11 = gtk_image_new_from_stock ("gtk-remove", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image11);
	gtk_container_add (GTK_CONTAINER (button7), image11);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_paned_pack2 (GTK_PANED (hpaned1), vbox2, TRUE, TRUE);

	table1 = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (vbox2), table1, TRUE, TRUE, 0);

	hruler1 = gtk_hruler_new ();
	gtk_widget_show (hruler1);
	gtk_table_attach (GTK_TABLE (table1), hruler1, 1, 2, 0, 1,
	            (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
	            (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_ruler_set_range (GTK_RULER (hruler1), 0, 10, 1.17073, 10);

	vruler1 = gtk_vruler_new ();
	gtk_widget_show (vruler1);
	gtk_table_attach (GTK_TABLE (table1), vruler1, 0, 1, 1, 2,
	            (GtkAttachOptions) (GTK_FILL),
	            (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_ruler_set_range (GTK_RULER (vruler1), 0, 10, 0.393375, 10);

	statusbar1 = gtk_statusbar_new ();
	gtk_widget_show (statusbar1);
	gtk_box_pack_start (GTK_BOX (vbox2), statusbar1, FALSE, FALSE, 0);

	g_signal_connect ((gpointer) new1, "activate",
	            G_CALLBACK (on_new1_activate),
	            NULL);
	g_signal_connect ((gpointer) open_project1, "activate",
	            G_CALLBACK (on_open_project1_activate),
	            NULL);
	g_signal_connect ((gpointer) open_layer1, "activate",
	            G_CALLBACK (on_open_layer1_activate),
	            NULL);
	g_signal_connect ((gpointer) revert1, "activate",
	            G_CALLBACK (on_revert1_activate),
	            NULL);
	g_signal_connect ((gpointer) save1, "activate",
	            G_CALLBACK (on_save1_activate),
	            NULL);
	g_signal_connect ((gpointer) save_as1, "activate",
	            G_CALLBACK (on_save_as1_activate),
	            NULL);
	g_signal_connect ((gpointer) import1, "activate",
	            G_CALLBACK (on_import1_activate),
	            NULL);
	g_signal_connect ((gpointer) postscript1, "activate",
	            G_CALLBACK (on_postscript1_activate),
	            NULL);
	g_signal_connect ((gpointer) png1, "activate",
	            G_CALLBACK (on_png1_activate),
	            NULL);
	g_signal_connect ((gpointer) pdf1, "activate",
	            G_CALLBACK (on_pdf1_activate),
	            NULL);
	g_signal_connect ((gpointer) svg1, "activate",
	            G_CALLBACK (on_svg1_activate),
	            NULL);
	g_signal_connect ((gpointer) print1, "activate",
	            G_CALLBACK (on_print1_activate),
	            NULL);
	g_signal_connect ((gpointer) project_properties1, "activate",
	            G_CALLBACK (on_project_properties1_activate),
	            NULL);
	g_signal_connect ((gpointer) gerber_properties1, "activate",
	            G_CALLBACK (on_gerber_properties1_activate),
	            NULL);
	g_signal_connect ((gpointer) quit1, "activate",
	            G_CALLBACK (on_quit1_activate),
	            NULL);
	g_signal_connect ((gpointer) zoom_in1, "activate",
	            G_CALLBACK (on_zoom_in1_activate),
	            NULL);
	g_signal_connect ((gpointer) zoom_out1, "activate",
	            G_CALLBACK (on_zoom_out1_activate),
	            NULL);
	g_signal_connect ((gpointer) fit_to_window1, "activate",
	            G_CALLBACK (on_fit_to_window1_activate),
	            NULL);
	g_signal_connect ((gpointer) tools1, "activate",
	            G_CALLBACK (on_tools1_activate),
	            NULL);
	g_signal_connect ((gpointer) pointer_tool1, "activate",
	            G_CALLBACK (on_pointer_tool1_activate),
	            NULL);
	g_signal_connect ((gpointer) zoom_tool1, "activate",
	            G_CALLBACK (on_zoom_tool1_activate),
	            NULL);
	g_signal_connect ((gpointer) measure_tool1, "activate",
	            G_CALLBACK (on_measure_tool1_activate),
	            NULL);
	g_signal_connect ((gpointer) online_manual1, "activate",
	            G_CALLBACK (on_online_manual1_activate),
	            NULL);
	g_signal_connect ((gpointer) about1, "activate",
	            G_CALLBACK (on_about1_activate),
	            NULL);

  
	/* End of Glade generated code */

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox1), 0);
	                  
	GtkWidget *drawingarea1;
	gint width, height;              
              
	gtk_window_add_accel_group (GTK_WINDOW (window1), accel_group);

	drawingarea1 = gtk_drawing_area_new();
	screen.drawing_area=drawingarea1;
	gtk_table_attach (GTK_TABLE (table1), drawingarea1, 1, 2, 1, 2,
	                  (GtkAttachOptions) (GTK_FILL),
	                  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	            
	statusbar1 = gtk_statusbar_new ();

	/*
	screen.statusbar.msg = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(screen.statusbar.msg), GTK_JUSTIFY_LEFT);
	GtkStyle  *textStyle = gtk_style_new();
	textStyle->font_desc = pango_font_description_from_string(setup.status_fontname);
	gtk_widget_set_style(GTK_WIDGET(screen.statusbar.msg), textStyle);
	screen.statusbar.msgstr[0] = '\0';
	screen.statusbar.coordstr[0] = '\0';
	screen.statusbar.diststr[0] = '\0';
	gtk_box_pack_start(GTK_BOX(hbox), screen.statusbar.msg, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	*/  
  
  
	GtkListStore *list_store;

	GtkTreeIter iter;
	gint i;

	list_store = gtk_list_store_new (4,
	                              G_TYPE_STRING,
	                              G_TYPE_STRING,
	                              G_TYPE_STRING,
	                              G_TYPE_BOOLEAN);

	for (i = 0; i < 5; i++)	{

		/* Add a new row to the model */
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
		                        0, " File-xx.gbr",
		                        1, "blue",
		                        2, "Color",
		                        3,  TRUE,
		                        -1);

		/* As the store will keep a copy of the string internally, we
		      * free some_data.
		      */
	}

	/* Modify a particular row 
	path = gtk_tree_path_new_from_string ("4");
	gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store),
	                        &iter,
	                        path);
	gtk_tree_path_free (path);
	gtk_list_store_set (list_store, &iter,
	                  COLUMN_BOOLEAN, TRUE,
	                  -1); */


	GtkWidget *tree;

	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes ("Visible",
	                                                renderer,
	                                                "active", 3,
	                                                NULL);
	gtk_tree_view_column_set_min_width  ((GtkTreeViewColumn *)column,40);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Color",
	                                                renderer,
	                                                "background", 1,
	                                                "text", 2,
	                                                NULL);


	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Name",
	                                                renderer,
	                                                "text", 0,
	                                                NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	//gtk_tree_view_set_headers_visible   ((GtkTreeView *)tree, FALSE);

	gtk_container_add (GTK_CONTAINER (scrolledwindow1), tree);


	/*
	* Connect all events on drawing area 
	*/    
	gtk_signal_connect(GTK_OBJECT(drawingarea1), "expose_event",
		       GTK_SIGNAL_FUNC(callback_drawingarea_expose_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea1),"configure_event",
		       GTK_SIGNAL_FUNC(callback_drawingarea_configure_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea1), "motion_notify_event",
		       GTK_SIGNAL_FUNC(callback_drawingarea_motion_notify_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea1), "button_press_event",
		       GTK_SIGNAL_FUNC(callback_drawingarea_button_press_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea1), "button_release_event",
		       GTK_SIGNAL_FUNC(callback_drawingarea_button_release_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(window1), "key_press_event",
		       GTK_SIGNAL_FUNC(callback_window_key_press_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(window1), "key_release_event",
		       GTK_SIGNAL_FUNC(callback_window_key_release_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(window1), "scroll_event",
		       GTK_SIGNAL_FUNC(callback_window_scroll_event), NULL);

	gtk_widget_set_events(drawingarea1, GDK_EXPOSURE_MASK
			  | GDK_LEAVE_NOTIFY_MASK
			  | GDK_BUTTON_PRESS_MASK
			  | GDK_BUTTON_RELEASE_MASK
			  | GDK_KEY_PRESS_MASK
			  | GDK_KEY_RELEASE_MASK
			  | GDK_POINTER_MOTION_MASK
			  | GDK_POINTER_MOTION_HINT_MASK
			  | GDK_SCROLL_MASK
			  );

	/*
	* Set gtk error log handler
	*/
#if !defined (__MINGW32__)     
	g_log_set_handler (NULL, 
		G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION | G_LOG_LEVEL_MASK, 
		gerbv_gtk_log_handler, NULL); 
#endif     
  
	/*
	* Setup some GTK+ defaults
	*/
	screen.tooltips = gtk_tooltips_new();        
	screen.background = alloc_color(0, 0, 0, "white");
	screen.zoom_outline_color  = alloc_color(0, 0, 0, "gray");
	screen.dist_measure_color  = alloc_color(0, 0, 0, "lightblue");


	/* 
	* Good defaults according to Ales. Gives aspect ratio of 1.3333...
	*/
	if ((req_width != -1) && (req_height != -1)) {
		width = req_width;
		height = req_height;
		} 
	else {
		GdkScreen *screen;
		int nmonitors;

		screen = gdk_screen_get_default();
		nmonitors = gdk_screen_get_n_monitors(screen);

		width = gdk_screen_get_width(screen) * 3/4 / nmonitors;
		height = gdk_screen_get_height(screen) * 3/4 / nmonitors;
	}

	gtk_window_set_default_size((GtkWindow *)window1, width, height);
	rename_main_window("",window1);
	set_window_icon (window1);


	gtk_widget_show_all (window1);
	gtk_main();
}


