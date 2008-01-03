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

#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #include "draw.h"
#endif

#include "color.h"
#include "gerbv_screen.h"
#include "gerbv_icon.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "callbacks.h"
#include "interface.h"
#include "render.h"
#include "icons.h"

#define WIN_TITLE "Gerber Viewer"
#  define _(String) (String)

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
	GtkWidget *mainWindow;
	GtkWidget *vbox1;
	GtkWidget *menubar1;
	GtkWidget *menuitem_file;
	GtkWidget *menuitem_file_menu;
	GtkWidget *new;
	GtkWidget *open_project;
	GtkWidget *image33;
	GtkWidget *open_layer;
	GtkWidget *revert;
	GtkWidget *save;
	GtkWidget *save_as;
	GtkWidget *separatormenuitem1;
	GtkWidget *export;
	GtkWidget *export_menu;
	GtkWidget *png;
	GtkWidget *separator1;
	GtkWidget *quit;
	GtkWidget *menuitem_view;
	GtkWidget *menuitem_view_menu;
	GtkWidget *zoom_in;
	GtkWidget *zoom_out;
	GtkWidget *separator5;
	GtkWidget *fit_to_window;
	GtkWidget *menuitem_analyze;
	GtkWidget *menuitem_analyze_menu;
	GtkWidget *analyze_active_gerbers;
	GtkWidget *analyze_active_drill;
	GtkWidget *control_gerber_options;
	GtkWidget *menubar_tools;
	GtkWidget *menubar_tools_menu;
	GtkWidget *pointer_tool;
	GtkWidget *zoom_tool;
	GtkWidget *measure_tool;
	GtkWidget *menuitem10;
	GtkWidget *menuitem10_menu;
	GtkWidget *online_manual;
	GtkWidget *about;
	GtkWidget *image34;
	GtkWidget *toolbar_hbox;
	GtkWidget *handlebox;
	GtkWidget *button_toolbar;
	GtkIconSize tmp_toolbar_icon_size;
	GtkWidget *toolbutton_new;
	GtkWidget *toolbutton_open;
	GtkWidget *toolbutton_revert;
	GtkWidget *toolbutton_save;
	GtkWidget *separatortoolitem1;
#ifndef RENDER_USING_GDK
#if GTK_CHECK_VERSION(2,10,0)
	GtkWidget *print;
	GtkWidget *toolbutton_print;
	GtkWidget *separator2;
	GtkWidget *separatortoolitem2;
#endif
#endif
	GtkWidget *toolbutton_zoom_in;
	GtkWidget *toolbutton_zoom_out;
	GtkWidget *toolbutton_zoom_fit;
	GtkWidget *separatortoolitem3;
	GtkWidget *toolbutton_analyze;
	GtkWidget *toolbutton_validate;
	GtkWidget *toolbutton_control;
	GtkWidget *separatortoolitem4;
	GtkWidget *toggletoolbutton_pointer;
	GtkWidget *toggletoolbutton_zoom;
	GtkWidget *toggletoolbutton_measure;
	GtkWidget *hpaned1;
	GtkWidget *sidepane_vbox;
	GtkWidget *sidepane_notebook;
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
	GtkWidget *Layer_label;
	GtkWidget *vbox11;
	GtkWidget *messages_scrolledwindow;
	GtkWidget *message_textview;
	GtkWidget *clear_messages_button;
	GtkWidget *Message_label;
	GtkWidget *vbox2;
	GtkWidget *main_view_table;
	GtkWidget *hRuler;
	GtkWidget *vRuler;
	GtkWidget *hbox5;
	GtkWidget *statusbar_label_left;
	GtkWidget *combobox2;
	GtkWidget *statusbar_label_right;
	GtkWidget *drawingarea, *hAdjustment, *vAdjustment, *hScrollbar, *vScrollbar;
	
	GtkAccelGroup *accel_group;
	GtkTooltips *tooltips;

	/* Inline icons */
	GdkPixbuf *zoompixbuf;
	GtkWidget *zoomimage;
	GdkPixbuf *measurepixbuf;
	GtkWidget *measureimage;
	GdkPixbuf *movepixbuf;
	GtkWidget *moveimage;


	tooltips = gtk_tooltips_new();

	accel_group = gtk_accel_group_new ();

	mainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (mainWindow), _("Gerber Viewer"));

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (mainWindow), vbox1);

	menubar1 = gtk_menu_bar_new ();
	gtk_widget_show (menubar1);
	gtk_box_pack_start (GTK_BOX (vbox1), menubar1, FALSE, FALSE, 0);

	menuitem_file = gtk_menu_item_new_with_mnemonic (_("_File"));
	gtk_widget_show (menuitem_file);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem_file);

	menuitem_file_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem_file), menuitem_file_menu);

	new = gtk_image_menu_item_new_from_stock ("gtk-new", accel_group);
	gtk_widget_show (new);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), new);
	gtk_widget_add_accelerator (new, "activate", accel_group,
	                        GDK_n, (GdkModifierType) GDK_CONTROL_MASK,
	                        GTK_ACCEL_VISIBLE);

	open_project = gtk_image_menu_item_new_with_mnemonic (_("_Open Project"));
	gtk_widget_show (open_project);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), open_project);
	gtk_tooltips_set_tip (tooltips, open_project, _("Open an existing Gerber Viewer project"), NULL);

	image33 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image33);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (open_project), image33);

	open_layer = gtk_menu_item_new_with_mnemonic (_("Open _Layer(s)"));
	gtk_widget_show (open_layer);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), open_layer);
	gtk_tooltips_set_tip (tooltips, open_layer, _("Open Gerber, drill, or pick and place file(s)"), NULL);

	revert = gtk_image_menu_item_new_from_stock ("gtk-revert-to-saved", accel_group);
	gtk_widget_show (revert);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), revert);

	save = gtk_image_menu_item_new_from_stock ("gtk-save", accel_group);
	gtk_widget_show (save);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), save);

	save_as = gtk_image_menu_item_new_from_stock ("gtk-save-as", accel_group);
	gtk_widget_show (save_as);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), save_as);

	separatormenuitem1 = gtk_separator_menu_item_new ();
	gtk_widget_show (separatormenuitem1);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), separatormenuitem1);
	gtk_widget_set_sensitive (separatormenuitem1, FALSE);

	export = gtk_menu_item_new_with_mnemonic (_("_Export"));
	gtk_widget_show (export);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), export);

	export_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (export), export_menu);

	png = gtk_menu_item_new_with_mnemonic (_("PNG"));
	gtk_widget_show (png);
	gtk_container_add (GTK_CONTAINER (export_menu), png);
	gtk_tooltips_set_tip (tooltips, png, _("Export project to a PNG file"), NULL);

#ifndef RENDER_USING_GDK
	GtkWidget *pdf;
	GtkWidget *svg;
	GtkWidget *postscript;
	
	pdf = gtk_menu_item_new_with_mnemonic (_("PDF"));
	gtk_widget_show (pdf);
	gtk_container_add (GTK_CONTAINER (export_menu), pdf);
	gtk_tooltips_set_tip (tooltips, pdf, _("Export project to a PDF file"), NULL);

	svg = gtk_menu_item_new_with_mnemonic (_("SVG"));
	gtk_widget_show (svg);
	gtk_container_add (GTK_CONTAINER (export_menu), svg);
	gtk_tooltips_set_tip (tooltips, svg, _("Export project to a SVG file"), NULL);
	
	postscript = gtk_menu_item_new_with_mnemonic (_("PostScript"));
	gtk_widget_show (postscript);
	gtk_container_add (GTK_CONTAINER (export_menu), postscript);
	gtk_tooltips_set_tip (tooltips, postscript, _("Export project to a PostScript file"), NULL);
#endif

	separator1 = gtk_separator_menu_item_new ();
	gtk_widget_show (separator1);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), separator1);
	gtk_widget_set_sensitive (separator1, FALSE);
#ifndef RENDER_USING_GDK
#if GTK_CHECK_VERSION(2,10,0)
	print = gtk_image_menu_item_new_from_stock ("gtk-print", accel_group);
	gtk_widget_show (print);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), print);

	separator2 = gtk_separator_menu_item_new ();
	gtk_widget_show (separator2);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), separator2);
	gtk_widget_set_sensitive (separator2, FALSE);
#endif
#endif
	quit = gtk_image_menu_item_new_from_stock ("gtk-quit", accel_group);
	gtk_widget_show (quit);
	gtk_container_add (GTK_CONTAINER (menuitem_file_menu), quit);

	menuitem_view = gtk_menu_item_new_with_mnemonic (_("_View"));
	gtk_widget_show (menuitem_view);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem_view);

	menuitem_view_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem_view), menuitem_view_menu);

	zoom_in = gtk_image_menu_item_new_from_stock ("gtk-zoom-in", accel_group);
	gtk_widget_show (zoom_in);
	gtk_container_add (GTK_CONTAINER (menuitem_view_menu), zoom_in);

	zoom_out = gtk_image_menu_item_new_from_stock ("gtk-zoom-out", accel_group);
	gtk_widget_show (zoom_out);
	gtk_container_add (GTK_CONTAINER (menuitem_view_menu), zoom_out);

	separator5 = gtk_separator_menu_item_new ();
	gtk_widget_show (separator5);
	gtk_container_add (GTK_CONTAINER (menuitem_view_menu), separator5);
	gtk_widget_set_sensitive (separator5, FALSE);

	fit_to_window = gtk_image_menu_item_new_from_stock ("gtk-zoom-fit", accel_group);
	gtk_widget_show (fit_to_window);
	gtk_container_add (GTK_CONTAINER (menuitem_view_menu), fit_to_window);

	menuitem_analyze = gtk_menu_item_new_with_mnemonic (_("_Analyze"));
	gtk_widget_show (menuitem_analyze);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem_analyze);

	menuitem_analyze_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem_analyze), menuitem_analyze_menu);

	analyze_active_gerbers = gtk_menu_item_new_with_mnemonic (_("_Analyze active Gerber layers"));
	gtk_widget_show (analyze_active_gerbers);
	gtk_container_add (GTK_CONTAINER (menuitem_analyze_menu), analyze_active_gerbers);

	analyze_active_drill = gtk_menu_item_new_with_mnemonic (_("_Analyze active drill layers"));
	gtk_widget_show (analyze_active_drill);
	gtk_container_add (GTK_CONTAINER (menuitem_analyze_menu), analyze_active_drill);

	control_gerber_options = gtk_menu_item_new_with_mnemonic (_("Control Gerber options"));
	gtk_widget_show (control_gerber_options);
	gtk_container_add (GTK_CONTAINER (menuitem_analyze_menu), control_gerber_options);

	menubar_tools = gtk_menu_item_new_with_mnemonic (_("_Tools"));
	gtk_widget_show (menubar_tools);
	gtk_container_add (GTK_CONTAINER (menubar1), menubar_tools);

	menubar_tools_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar_tools), menubar_tools_menu);

	pointer_tool = gtk_menu_item_new_with_mnemonic (_("_Pointer Tool"));
	gtk_widget_show (pointer_tool);
	gtk_container_add (GTK_CONTAINER (menubar_tools_menu), pointer_tool);
	gtk_tooltips_set_tip (tooltips, pointer_tool, _("The default tool"), NULL);
	gtk_widget_add_accelerator (pointer_tool, "activate", accel_group,
	                        GDK_F1, (GdkModifierType) 0,
	                        GTK_ACCEL_VISIBLE);

	zoom_tool = gtk_menu_item_new_with_mnemonic (_("_Zoom Tool"));
	gtk_widget_show (zoom_tool);
	gtk_container_add (GTK_CONTAINER (menubar_tools_menu), zoom_tool);
	gtk_tooltips_set_tip (tooltips, zoom_tool, _("Zoom by drawing a rectangle"), NULL);
	gtk_widget_add_accelerator (zoom_tool, "activate", accel_group,
	                        GDK_F2, (GdkModifierType) 0,
	                        GTK_ACCEL_VISIBLE);

	measure_tool = gtk_menu_item_new_with_mnemonic (_("_Measure Tool"));
	gtk_widget_show (measure_tool);
	gtk_container_add (GTK_CONTAINER (menubar_tools_menu), measure_tool);
	gtk_tooltips_set_tip (tooltips, measure_tool, _("Measure distances"), NULL);
	gtk_widget_add_accelerator (measure_tool, "activate", accel_group,
	                        GDK_F3, (GdkModifierType) 0,
	                        GTK_ACCEL_VISIBLE);

	menuitem10 = gtk_menu_item_new_with_mnemonic (_("_Help"));
	gtk_widget_show (menuitem10);
	gtk_container_add (GTK_CONTAINER (menubar1), menuitem10);

	menuitem10_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem10), menuitem10_menu);

	online_manual = gtk_menu_item_new_with_mnemonic (_("_Online Manual"));
	gtk_widget_show (online_manual);
	gtk_container_add (GTK_CONTAINER (menuitem10_menu), online_manual);
	gtk_tooltips_set_tip (tooltips, online_manual, _("View the online help documentation"), NULL);

	about = gtk_image_menu_item_new_with_mnemonic (_("_About GerberViewer"));
	gtk_widget_show (about);
	gtk_container_add (GTK_CONTAINER (menuitem10_menu), about);
	gtk_tooltips_set_tip (tooltips, about, _("View information about this software"), NULL);

	image34 = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image34);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (about), image34);

	toolbar_hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (toolbar_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), toolbar_hbox, FALSE, FALSE, 0);

	handlebox = gtk_handle_box_new ();
	gtk_widget_show (handlebox);
	gtk_box_pack_start (GTK_BOX (toolbar_hbox), handlebox, TRUE, TRUE, 0);

	button_toolbar = gtk_toolbar_new ();
	gtk_widget_show (button_toolbar);
	gtk_container_add (GTK_CONTAINER (handlebox), button_toolbar);
	gtk_toolbar_set_style (GTK_TOOLBAR (button_toolbar), GTK_TOOLBAR_ICONS);
	tmp_toolbar_icon_size = gtk_toolbar_get_icon_size (GTK_TOOLBAR (button_toolbar));

	toolbutton_new = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-new");
	gtk_widget_show (toolbutton_new);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_new);

	toolbutton_open = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-open");
	gtk_widget_show (toolbutton_open);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_open);

	toolbutton_revert = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-revert-to-saved");
	gtk_widget_show (toolbutton_revert);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_revert);

	toolbutton_save = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-save");
	gtk_widget_show (toolbutton_save);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_save);

	separatortoolitem1 = (GtkWidget*) gtk_separator_tool_item_new ();
	gtk_widget_show (separatortoolitem1);
	gtk_container_add (GTK_CONTAINER (button_toolbar), separatortoolitem1);
#ifndef RENDER_USING_GDK
#if GTK_CHECK_VERSION(2,10,0)
	toolbutton_print = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-print");
	gtk_widget_show (toolbutton_print);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_print);

	separatortoolitem2 = (GtkWidget*) gtk_separator_tool_item_new ();
	gtk_widget_show (separatortoolitem2);
	gtk_container_add (GTK_CONTAINER (button_toolbar), separatortoolitem2);
#endif
#endif
	toolbutton_zoom_in = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-zoom-in");
	gtk_widget_show (toolbutton_zoom_in);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_zoom_in);

	toolbutton_zoom_out = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-zoom-out");
	gtk_widget_show (toolbutton_zoom_out);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_zoom_out);

	toolbutton_zoom_fit = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-zoom-fit");
	gtk_widget_show (toolbutton_zoom_fit);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_zoom_fit);

	separatortoolitem3 = (GtkWidget*) gtk_separator_tool_item_new ();
	gtk_widget_show (separatortoolitem3);
	gtk_container_add (GTK_CONTAINER (button_toolbar), separatortoolitem3);

	toolbutton_analyze = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-apply");
	gtk_widget_show (toolbutton_analyze);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_analyze);

	toolbutton_validate = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-apply");
	gtk_widget_show (toolbutton_validate);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_validate);

	toolbutton_control = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-apply");
	gtk_widget_show (toolbutton_control);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toolbutton_control);

	separatortoolitem4 = (GtkWidget*) gtk_separator_tool_item_new ();
	gtk_widget_show (separatortoolitem4);
	gtk_container_add (GTK_CONTAINER (button_toolbar), separatortoolitem4);

	toggletoolbutton_pointer = (GtkWidget*) gtk_toggle_tool_button_new();
	movepixbuf = gdk_pixbuf_new_from_inline(-1, move, FALSE, NULL);
	moveimage = gtk_image_new_from_pixbuf(movepixbuf);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(toggletoolbutton_pointer),
					moveimage);
	gtk_widget_show (toggletoolbutton_pointer);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toggletoolbutton_pointer);

	toggletoolbutton_zoom = (GtkWidget*) gtk_toggle_tool_button_new();
	zoompixbuf = gdk_pixbuf_new_from_inline(-1, lzoom, FALSE, NULL);
	zoomimage = gtk_image_new_from_pixbuf(zoompixbuf);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(toggletoolbutton_zoom),
					zoomimage);
	gtk_widget_show (toggletoolbutton_zoom);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toggletoolbutton_zoom);

	toggletoolbutton_measure = (GtkWidget*) gtk_toggle_tool_button_new();
	measurepixbuf = gdk_pixbuf_new_from_inline(-1, ruler, FALSE, NULL);
	measureimage = gtk_image_new_from_pixbuf(measurepixbuf);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(toggletoolbutton_measure),
					measureimage);
	gtk_widget_show (toggletoolbutton_measure);
	gtk_container_add (GTK_CONTAINER (button_toolbar), toggletoolbutton_measure);
	
	hpaned1 = gtk_hpaned_new ();
	gtk_widget_show (hpaned1);
	gtk_box_pack_start (GTK_BOX (vbox1), hpaned1, TRUE, TRUE, 0);
	gtk_paned_set_position (GTK_PANED (hpaned1), 225);

	sidepane_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (sidepane_vbox);
	gtk_paned_pack1 (GTK_PANED (hpaned1), sidepane_vbox, FALSE, TRUE);
	gtk_widget_set_size_request (sidepane_vbox, 92, -1);
	gtk_container_set_border_width (GTK_CONTAINER (sidepane_vbox), 5);

	sidepane_notebook = gtk_notebook_new ();
	gtk_widget_show (sidepane_notebook);
	gtk_box_pack_start (GTK_BOX (sidepane_vbox), sidepane_notebook, TRUE, TRUE, 0);

	vbox10 = gtk_vbox_new (FALSE, 3);
	gtk_widget_show (vbox10);
	gtk_container_add (GTK_CONTAINER (sidepane_notebook), vbox10);
	gtk_widget_set_size_request (vbox10, 82, -1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox10), 4);

	hbox4 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox4);
	gtk_box_pack_start (GTK_BOX (vbox10), hbox4, FALSE, FALSE, 0);

	label1 = gtk_label_new (_("Rendering: "));
	gtk_widget_show (label1);
	gtk_box_pack_start (GTK_BOX (hbox4), label1, FALSE, FALSE, 0);

	combobox1 = gtk_combo_box_new_text ();
	gtk_widget_show (combobox1);
	gtk_box_pack_start (GTK_BOX (hbox4), combobox1, TRUE, TRUE, 0);
#ifdef RENDER_USING_GDK
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Normal"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("And"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Or"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Xor"));
#else
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Normal"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("High speed"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("High quality"));
#endif
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

	Layer_label = gtk_label_new (_("Layers"));
	gtk_widget_show (Layer_label);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (sidepane_notebook), 
				    gtk_notebook_get_nth_page (GTK_NOTEBOOK (sidepane_notebook), 0), 
				    Layer_label);

	vbox11 = gtk_vbox_new (FALSE, 2);
	gtk_widget_show (vbox11);
	gtk_container_add (GTK_CONTAINER (sidepane_notebook), vbox11);
	gtk_container_set_border_width (GTK_CONTAINER (vbox11), 3);

	messages_scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (messages_scrolledwindow);
	gtk_box_pack_start (GTK_BOX (vbox11), messages_scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (messages_scrolledwindow), 
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	message_textview = gtk_text_view_new ();
	gtk_widget_show (message_textview);
	gtk_container_add (GTK_CONTAINER (messages_scrolledwindow), message_textview);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (message_textview), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (message_textview), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (message_textview), FALSE);

	clear_messages_button = gtk_button_new_from_stock ("gtk-clear");
	gtk_widget_show (clear_messages_button);
	gtk_box_pack_start (GTK_BOX (vbox11), clear_messages_button, FALSE, FALSE, 0);

	Message_label = gtk_label_new (_("Messages"));
	gtk_widget_show (Message_label);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (sidepane_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (sidepane_notebook), 1), Message_label);

	vbox2 = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox2);
	gtk_paned_pack2 (GTK_PANED (hpaned1), vbox2, TRUE, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 4);
	
	main_view_table = gtk_table_new (3, 3, FALSE);
	gtk_widget_show (main_view_table);
	gtk_box_pack_start (GTK_BOX (vbox2), main_view_table, TRUE, TRUE, 0);

	hRuler = gtk_hruler_new ();
	gtk_widget_show (hRuler);
	gtk_table_attach (GTK_TABLE (main_view_table), hRuler, 1, 2, 0, 1,
	                  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
	                  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_ruler_set_range (GTK_RULER (hRuler), 0, 100, 8.56051, 1000);

	vRuler = gtk_vruler_new ();
	gtk_widget_show (vRuler);
	gtk_table_attach (GTK_TABLE (main_view_table), vRuler, 0, 1, 1, 2,
	                  (GtkAttachOptions) (GTK_FILL),
	                  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_ruler_set_range (GTK_RULER (vRuler), 0, 100, 8.37341, 1000);

	drawingarea = gtk_drawing_area_new();
	gtk_table_attach (GTK_TABLE (main_view_table), drawingarea, 1, 2, 1, 2,
	                  (GtkAttachOptions) (GTK_FILL),
	                  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	
	hAdjustment = (GtkWidget *) gtk_adjustment_new (0.0, -1000.0, 1000.0, 1000.0, 1000.0, 500.0);
	vAdjustment = (GtkWidget *) gtk_adjustment_new (0.0, -1000.0, 1000.0, 1000.0, 1000.0, 500.0);
	
	hScrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (hAdjustment));
	gtk_table_attach (GTK_TABLE (main_view_table), hScrollbar, 1, 2, 2, 3,
	                  (GtkAttachOptions) (GTK_FILL),
	                  (GtkAttachOptions) (GTK_FILL), 0, 0);
	                  
	vScrollbar = gtk_vscrollbar_new (GTK_ADJUSTMENT (vAdjustment));
	gtk_table_attach (GTK_TABLE (main_view_table), vScrollbar, 2, 3, 1, 2,
	                  (GtkAttachOptions) (GTK_FILL),
	                  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
	hbox5 = gtk_hbox_new (FALSE, 10);
	gtk_widget_show (hbox5);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox5, FALSE, FALSE, 0);

	statusbar_label_left = gtk_label_new (_("(   0.0,  0.0 )"));
	gtk_widget_show (statusbar_label_left);
	gtk_box_pack_start (GTK_BOX (hbox5), statusbar_label_left, FALSE, FALSE, 0);
	gtk_widget_set_size_request (statusbar_label_left, 130, -1);
	gtk_label_set_justify ((GtkLabel *) statusbar_label_left, GTK_JUSTIFY_RIGHT);
	
	combobox2 = gtk_combo_box_new_text ();
	gtk_widget_show (combobox2);
	gtk_box_pack_start (GTK_BOX (hbox5), combobox2, FALSE, FALSE, 0);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox2), _("mil"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox2), _("mm"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox2), _("in"));

	statusbar_label_right = gtk_label_new (_(""));
	gtk_widget_show (statusbar_label_right);
	gtk_box_pack_start (GTK_BOX (hbox5), statusbar_label_right, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (statusbar_label_right), 0, 0.5);


/* ----------------------------------------------------------------------- */
/*
 *  Connect signals to widgets
 */
	g_signal_connect ((gpointer) new, "activate",
	                  G_CALLBACK (callbacks_new_activate),
	                  NULL);
	g_signal_connect ((gpointer) open_project, "activate",
	                  G_CALLBACK (callbacks_open_project_activate),
	                  NULL);
	g_signal_connect ((gpointer) open_layer, "activate",
	                  G_CALLBACK (callbacks_open_layer_activate),
	                  NULL);
	g_signal_connect ((gpointer) revert, "activate",
	                  G_CALLBACK (callbacks_revert_activate),
	                  NULL);
	g_signal_connect ((gpointer) save, "activate",
	                  G_CALLBACK (callbacks_save_activate),
	                  NULL);
	g_signal_connect ((gpointer) save_as, "activate",
	                  G_CALLBACK (callbacks_generic_save_activate),
	                  (gpointer) CALLBACKS_SAVE_FILE_AS);
#ifdef EXPORT_PNG
	g_signal_connect ((gpointer) png, "activate",
	                  G_CALLBACK (callbacks_generic_save_activate),
	                 (gpointer)  CALLBACKS_SAVE_FILE_PNG);
#endif

#ifndef RENDER_USING_GDK
	g_signal_connect ((gpointer) pdf, "activate",
	                 G_CALLBACK (callbacks_generic_save_activate),
	                  (gpointer) CALLBACKS_SAVE_FILE_PDF);
	g_signal_connect ((gpointer) svg, "activate",
	                  G_CALLBACK (callbacks_generic_save_activate),
	                  (gpointer) CALLBACKS_SAVE_FILE_SVG);
	g_signal_connect ((gpointer) postscript, "activate",
	                  G_CALLBACK (callbacks_generic_save_activate),
	                  (gpointer) CALLBACKS_SAVE_FILE_PS);


#if GTK_CHECK_VERSION(2,10,0)
	g_signal_connect ((gpointer) print, "activate",
	                  G_CALLBACK (callbacks_print_activate),
	                  NULL);
#endif
#endif
	g_signal_connect ((gpointer) quit, "activate",
	                  G_CALLBACK (callbacks_quit_activate),
	                  NULL);
	g_signal_connect ((gpointer) zoom_in, "activate",
	                  G_CALLBACK (callbacks_zoom_in_activate),
	                  NULL);
	g_signal_connect ((gpointer) zoom_out, "activate",
	                  G_CALLBACK (callbacks_zoom_out_activate),
	                  NULL);
	g_signal_connect ((gpointer) fit_to_window, "activate",
	                  G_CALLBACK (callbacks_fit_to_window_activate),
	                  NULL);
	g_signal_connect ((gpointer) analyze_active_gerbers, "activate",
	                  G_CALLBACK (callbacks_analyze_active_gerbers_activate),
	                  NULL);
	g_signal_connect ((gpointer) analyze_active_drill, "activate",
	                  G_CALLBACK (callbacks_analyze_active_drill_activate),
	                  NULL);
	g_signal_connect ((gpointer) control_gerber_options, "activate",
	                  G_CALLBACK (callbacks_control_gerber_options_activate),
	                  NULL);
	g_signal_connect ((gpointer) pointer_tool, "activate",
	                  G_CALLBACK (callbacks_change_tool), (gpointer) 0);
	g_signal_connect ((gpointer) zoom_tool, "activate",
	                  G_CALLBACK (callbacks_change_tool), (gpointer) 1);
	g_signal_connect ((gpointer) measure_tool, "activate",
	                  G_CALLBACK (callbacks_change_tool), (gpointer) 2);
	g_signal_connect ((gpointer) online_manual, "activate",
	                  G_CALLBACK (callbacks_online_manual_activate),
	                  NULL);
	g_signal_connect ((gpointer) about, "activate",
	                  G_CALLBACK (callbacks_about_activate),
	                  NULL);

	/* End of Glade generated code */
	g_signal_connect ((gpointer) toolbutton_new, "clicked",
	                  G_CALLBACK (callbacks_new_activate),
	                  NULL);
	g_signal_connect ((gpointer) toolbutton_save, "clicked",
	                  G_CALLBACK (callbacks_save_activate),
	                  NULL);
	g_signal_connect ((gpointer) toolbutton_open, "clicked",
	                  G_CALLBACK (callbacks_open_project_activate),
	                  NULL);
	g_signal_connect ((gpointer) toolbutton_revert, "clicked",
	                  G_CALLBACK (callbacks_revert_activate),
	                  NULL);
	g_signal_connect ((gpointer) clear_messages_button, "clicked",
	                  G_CALLBACK (callbacks_clear_messages_button_clicked),
	                  NULL);
#ifndef RENDER_USING_GDK
#if GTK_CHECK_VERSION(2,10,0)
	g_signal_connect ((gpointer) toolbutton_print, "clicked",
	                  G_CALLBACK (callbacks_print_activate),
	                  NULL);
#endif
#endif
	g_signal_connect ((gpointer) toolbutton_zoom_in, "clicked",
	                  G_CALLBACK (callbacks_zoom_in_activate),
	                  NULL);
	g_signal_connect ((gpointer) toolbutton_zoom_out, "clicked",
	                  G_CALLBACK (callbacks_zoom_out_activate),
	                  NULL);
	g_signal_connect ((gpointer) toolbutton_zoom_fit, "clicked",
	                  G_CALLBACK (callbacks_fit_to_window_activate),
	                  NULL);
	g_signal_connect ((gpointer) toggletoolbutton_pointer, "clicked",
	                  G_CALLBACK (callbacks_change_tool), (gpointer) 0);
	g_signal_connect ((gpointer) toggletoolbutton_zoom, "clicked",
	                  G_CALLBACK (callbacks_change_tool), (gpointer) 1);
	g_signal_connect ((gpointer) toggletoolbutton_measure, "clicked",
	                  G_CALLBACK (callbacks_change_tool), (gpointer) 2);
	g_signal_connect ((gpointer) combobox1, "changed",
	                  G_CALLBACK (callbacks_sidepane_render_type_combo_box_changed),
	                  NULL);
	g_signal_connect ((gpointer) combobox2, "changed",
	                  G_CALLBACK (callbacks_statusbar_unit_combo_box_changed),
	                  NULL);
	                  
	g_signal_connect ((gpointer) button4, "clicked",
	                  G_CALLBACK (callbacks_add_layer_button_clicked), NULL);
	g_signal_connect ((gpointer) button7, "clicked",
	                  G_CALLBACK (callbacks_remove_layer_button_clicked), NULL);
	g_signal_connect ((gpointer) button5, "clicked",
	                  G_CALLBACK (callbacks_move_layer_down_button_clicked), NULL);
	g_signal_connect ((gpointer) button6, "clicked",
	                  G_CALLBACK (callbacks_move_layer_up_clicked), NULL);

	g_signal_connect ((gpointer) hAdjustment, "value-changed",
	                  G_CALLBACK (callbacks_hadjustment_value_changed), NULL);
	g_signal_connect ((gpointer) vAdjustment, "value-changed",
	                  G_CALLBACK (callbacks_vadjustment_value_changed), NULL);
	g_signal_connect ((gpointer) hScrollbar, "button-press-event",
	                  G_CALLBACK (callbacks_scrollbar_button_pressed), NULL);                 
	g_signal_connect ((gpointer) hScrollbar, "button-release-event",
	                  G_CALLBACK (callbacks_scrollbar_button_released), NULL);
	g_signal_connect ((gpointer) vScrollbar, "button-press-event",
	                  G_CALLBACK (callbacks_scrollbar_button_pressed), NULL);                 
	g_signal_connect ((gpointer) vScrollbar, "button-release-event",
	                  G_CALLBACK (callbacks_scrollbar_button_released), NULL);               
	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox1), 0);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox2), 0);
	   
	gint width, height;
              
	gtk_window_add_accel_group (GTK_WINDOW (mainWindow), accel_group);

	             
	GtkListStore *list_store;

	list_store = gtk_list_store_new (3,	G_TYPE_BOOLEAN,
		GDK_TYPE_PIXBUF, G_TYPE_STRING);
		

		
	GtkWidget *tree;

	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes ("Visible",
	                                                renderer,
	                                                "active", 0,
	                                                NULL);
	gtk_tree_view_column_set_min_width  ((GtkTreeViewColumn *)column,25);
	gtk_signal_connect(GTK_OBJECT(renderer), "toggled",
		       GTK_SIGNAL_FUNC(callbacks_layer_tree_visibility_button_toggled), NULL);
		       
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("Color",
	                                                renderer,
	                                                "pixbuf", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Name",
	                                                renderer,
	                                                "text", 2,
	                                                NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

	gtk_tree_view_set_headers_visible   ((GtkTreeView *)tree, FALSE);
	gtk_signal_connect(GTK_OBJECT(tree), "button-press-event",
		GTK_SIGNAL_FUNC(callbacks_layer_tree_button_press), NULL);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), tree);
	
	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tree), FALSE);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (tree), TRUE);

	g_signal_connect (G_OBJECT(list_store), "row-inserted",
			  G_CALLBACK (callbacks_layer_tree_row_inserted), NULL);
			  		
	/*
	* Connect all events on drawing area 
	*/    
	gtk_signal_connect(GTK_OBJECT(drawingarea), "expose_event",
		       GTK_SIGNAL_FUNC(callbacks_drawingarea_expose_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea),"configure_event",
		       GTK_SIGNAL_FUNC(callbacks_drawingarea_configure_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea), "motion_notify_event",
		       GTK_SIGNAL_FUNC(callbacks_drawingarea_motion_notify_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea), "button_press_event",
		       GTK_SIGNAL_FUNC(callbacks_drawingarea_button_press_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea), "button_release_event",
		       GTK_SIGNAL_FUNC(callbacks_drawingarea_button_release_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(mainWindow), "key_press_event",
		       GTK_SIGNAL_FUNC(callbacks_window_key_press_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(mainWindow), "key_release_event",
		       GTK_SIGNAL_FUNC(callbacks_window_key_release_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(mainWindow), "scroll_event",
		       GTK_SIGNAL_FUNC(callbacks_window_scroll_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(mainWindow), "delete_event",
		       GTK_SIGNAL_FUNC(callbacks_quit_activate), NULL);       
	
	gtk_widget_set_events(drawingarea, GDK_EXPOSURE_MASK
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
		       callbacks_handle_log_messages, NULL); 
#endif     
  
	/*
	* Setup some GTK+ defaults
	*/
	screen.tooltips = gtk_tooltips_new();
	GdkColor color1 = {0, 0, 0, 0}, color2 = {0, 50000, 50000, 50000},
			color3 = {0, 60000, 30000, 65000};       
	screen.background = color1;
	screen.zoom_outline_color  = color2;
	screen.dist_measure_color  = color3;
#ifdef RENDER_USING_GDK
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.background, FALSE, TRUE);
#endif
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.zoom_outline_color, FALSE, TRUE);
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.dist_measure_color, FALSE, TRUE);
	screen.drawing_area=drawingarea;
	screen.win.hAdjustment = hAdjustment;
	screen.win.vAdjustment = vAdjustment;
	screen.win.hRuler = hRuler;
	screen.win.vRuler = vRuler;	
	screen.win.sidepane_notebook = sidepane_notebook;

	screen.win.toolButtonPointer = toggletoolbutton_pointer;
	screen.win.toolButtonZoom = toggletoolbutton_zoom;
	screen.win.toolButtonMeasure = toggletoolbutton_measure;
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

	gtk_window_set_default_size((GtkWindow *)mainWindow, width, height);

	gtk_widget_show_all (mainWindow);
	screen.win.topLevelWindow = mainWindow;
	screen.win.messageTextView = message_textview;
	screen.win.statusMessageLeft = statusbar_label_left;
	screen.win.statusMessageRight = statusbar_label_right;
	screen.win.statusUnitComboBox = combobox2;
	screen.win.layerTree = tree;
	screen.win.treeIsUpdating = FALSE;
	callbacks_change_tool (NULL, 0);
	rename_main_window("",mainWindow);

	set_window_icon (mainWindow);
	callbacks_update_layer_tree ();
	
	gtk_main();
}


