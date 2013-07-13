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

/** \file interface.h
    \brief Header info for the GUI building functions for Gerber Viewer
    \ingroup gerbv
*/

/** Sets the key acceleration of a menu item.
First tries to lookup the given STOCK_ID with gtk_stock_lookup.
If this succeeds and the retrieved GtkStockItem has an accelerator defined this accelerator is used.
Otherwise the defaults given below are used.

\warning There has to be a GtkStockItem variable 'stock' in scope where this macro is used.
*/
#define SET_ACCELS_FROM_STOCK(MENU_ITEM, STOCK_ID, GERBV_ACCEL_ID)\
gtk_menu_item_set_accel_path (GTK_MENU_ITEM (MENU_ITEM), GERBV_ACCEL_ID ## _PATH);\
if(gtk_stock_lookup (STOCK_ID, &stock) && stock.keyval != GDK_VoidSymbol && stock.keyval != 0)\
	gtk_accel_map_add_entry (GERBV_ACCEL_ID ## _PATH, stock.keyval, stock.modifier);\
else\
	gtk_accel_map_add_entry (GERBV_ACCEL_ID ## _PATH, GERBV_ACCEL_ID ## _KEY, GERBV_ACCEL_ID ## _MOD)

#define SET_ACCELS(MENU_ITEM, GERBV_ACCEL_ID)\
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM (MENU_ITEM), GERBV_ACCEL_ID ## _PATH);\
	gtk_accel_map_add_entry (GERBV_ACCEL_ID ## _PATH, GERBV_ACCEL_ID ## _KEY, GERBV_ACCEL_ID ## _MOD)

/* If stock items/IDs are used the ACCEL_*_PATH macros have to match the labels of the stock items.
Otherwise the (persistent) accelerators are broken. One workaround would be to look the labels up. */
#define GERBV_ACCELS_RELPATH ".gnome2/accels/gerbv"
#define ACCEL_ROOT						"<main>/"
#define ACCEL_FILE						ACCEL_ROOT "file"
#define ACCEL_FILE_NEW_PATH				ACCEL_FILE "/New"
#define ACCEL_FILE_NEW_KEY				GDK_n
#define ACCEL_FILE_NEW_MOD				(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_FILE_REVERT_PATH			ACCEL_FILE "/Revert"
#define ACCEL_FILE_REVERT_KEY			GDK_F5
#define ACCEL_FILE_REVERT_MOD			(GdkModifierType) 0
#define ACCEL_FILE_OPEN_LAYER_PATH		ACCEL_FILE "/Open layer(s)..."
#define ACCEL_FILE_OPEN_LAYER_KEY		GDK_O
#define ACCEL_FILE_OPEN_LAYER_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_FILE_SAVE_LAYER_PATH		ACCEL_FILE "/Save active layer"
#define ACCEL_FILE_SAVE_LAYER_KEY		GDK_S
#define ACCEL_FILE_SAVE_LAYER_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_FILE_SAVE_LAYER_AS_PATH	ACCEL_FILE "/Save active layer as..."
#define ACCEL_FILE_SAVE_LAYER_AS_KEY	GDK_S
#define ACCEL_FILE_SAVE_LAYER_AS_MOD	(GdkModifierType) GDK_CONTROL_MASK | GDK_SHIFT_MASK
#define ACCEL_FILE_EXPORT				ACCEL_FILE "/Export"
#define ACCEL_FILE_PRINT_PATH			ACCEL_FILE "/Print..."
#define ACCEL_FILE_PRINT_KEY			GDK_P
#define ACCEL_FILE_PRINT_MOD			(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_FILE_QUIT_PATH			ACCEL_FILE "/Quit"
#define ACCEL_FILE_QUIT_KEY				GDK_Q
#define ACCEL_FILE_QUIT_MOD				(GdkModifierType) GDK_CONTROL_MASK

#define ACCEL_EDIT						ACCEL_ROOT "edit"
#define ACCEL_EDIT_PROPERTIES_PATH		ACCEL_EDIT "/Display properties of selected object(s)"
#define ACCEL_EDIT_PROPERTIES_KEY		GDK_Return
#define ACCEL_EDIT_PROPERTIES_MOD		(GdkModifierType) GDK_MOD1_MASK
#define ACCEL_EDIT_DELETE_PATH			ACCEL_EDIT "/Delete selected object(s)"
#define ACCEL_EDIT_DELETE_KEY			GDK_Delete
#define ACCEL_EDIT_DELETE_MOD			(GdkModifierType) 0

#define ACCEL_VIEW						ACCEL_ROOT "view"
#define ACCEL_VIEW_FULLSCREEN_PATH		ACCEL_VIEW "/Fullscreen"
#define ACCEL_VIEW_FULLSCREEN_KEY		GDK_F11
#define ACCEL_VIEW_FULLSCREEN_MOD		(GdkModifierType) 0
#define ACCEL_VIEW_TOOLBAR_PATH			ACCEL_VIEW "/Show Toolbar"
#define ACCEL_VIEW_TOOLBAR_KEY			GDK_F7
#define ACCEL_VIEW_TOOLBAR_MOD			(GdkModifierType) 0
#define ACCEL_VIEW_SIDEPANE_PATH		ACCEL_VIEW "/Show Sidepane"
#define ACCEL_VIEW_SIDEPANE_KEY			GDK_F9
#define ACCEL_VIEW_SIDEPANE_MOD			(GdkModifierType) 0
#define ACCEL_VIEW_VIS					ACCEL_VIEW "/Toggle layer visibilty"
#define ACCEL_VIEW_VIS_LAYER1_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 1"
#define ACCEL_VIEW_VIS_LAYER1_KEY		GDK_1
#define ACCEL_VIEW_VIS_LAYER1_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER2_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 2"
#define ACCEL_VIEW_VIS_LAYER2_KEY		GDK_2
#define ACCEL_VIEW_VIS_LAYER2_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER3_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 3"
#define ACCEL_VIEW_VIS_LAYER3_KEY		GDK_3
#define ACCEL_VIEW_VIS_LAYER3_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER4_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 4"
#define ACCEL_VIEW_VIS_LAYER4_KEY		GDK_4
#define ACCEL_VIEW_VIS_LAYER4_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER5_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 5"
#define ACCEL_VIEW_VIS_LAYER5_KEY		GDK_5
#define ACCEL_VIEW_VIS_LAYER5_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER6_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 6"
#define ACCEL_VIEW_VIS_LAYER6_KEY		GDK_6
#define ACCEL_VIEW_VIS_LAYER6_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER7_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 7"
#define ACCEL_VIEW_VIS_LAYER7_KEY		GDK_7
#define ACCEL_VIEW_VIS_LAYER7_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER8_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 8"
#define ACCEL_VIEW_VIS_LAYER8_KEY		GDK_8
#define ACCEL_VIEW_VIS_LAYER8_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER9_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 9"
#define ACCEL_VIEW_VIS_LAYER9_KEY		GDK_9
#define ACCEL_VIEW_VIS_LAYER9_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_VIS_LAYER10_PATH		ACCEL_VIEW_VIS "/Toggle visibility of layer 10"
#define ACCEL_VIEW_VIS_LAYER10_KEY		GDK_0
#define ACCEL_VIEW_VIS_LAYER10_MOD		(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_VIEW_ZOOM_IN_PATH			ACCEL_VIEW "/Zoom In"
#define ACCEL_VIEW_ZOOM_IN_KEY			GDK_z
#define ACCEL_VIEW_ZOOM_IN_MOD			(GdkModifierType) 0
#define ACCEL_VIEW_ZOOM_OUT_PATH		ACCEL_VIEW "/Zoom Out"
#define ACCEL_VIEW_ZOOM_OUT_KEY			GDK_z
#define ACCEL_VIEW_ZOOM_OUT_MOD			(GdkModifierType) GDK_SHIFT_MASK
#define ACCEL_VIEW_ZOOM_FIT_PATH		ACCEL_VIEW "/Best Fit"
#define ACCEL_VIEW_ZOOM_FIT_KEY			GDK_f
#define ACCEL_VIEW_ZOOM_FIT_MOD			(GdkModifierType) 0
#define ACCEL_VIEW_RENDER				ACCEL_VIEW "/Rendering"
#define ACCEL_VIEW_UNITS				ACCEL_VIEW "/Units"

#define ACCEL_LAYER						ACCEL_ROOT "layer"
#define ACCEL_LAYER_COLOR_PATH			ACCEL_LAYER "/Change color"
#define ACCEL_LAYER_COLOR_KEY			GDK_F6
#define ACCEL_LAYER_COLOR_MOD			(GdkModifierType) 0
#define ACCEL_LAYER_UP_PATH				ACCEL_LAYER "/Move up"
#define ACCEL_LAYER_UP_KEY				GDK_Up
#define ACCEL_LAYER_UP_MOD				(GdkModifierType) GDK_CONTROL_MASK
#define ACCEL_LAYER_DOWN_PATH			ACCEL_LAYER "/Move down"
#define ACCEL_LAYER_DOWN_KEY			GDK_Down
#define ACCEL_LAYER_DOWN_MOD			(GdkModifierType) GDK_CONTROL_MASK

#define ACCEL_ANAL						ACCEL_ROOT "analyze"

#define ACCEL_TOOLS						ACCEL_ROOT "tools"
#define ACCEL_TOOLS_POINTER_PATH		ACCEL_TOOLS "/Pointer Tool"
#define ACCEL_TOOLS_POINTER_KEY			GDK_1
#define ACCEL_TOOLS_POINTER_MOD			(GdkModifierType) 0
#define ACCEL_TOOLS_PAN_PATH			ACCEL_TOOLS "/Pan Tool"
#define ACCEL_TOOLS_PAN_KEY				GDK_2
#define ACCEL_TOOLS_PAN_MOD				(GdkModifierType) 0
#define ACCEL_TOOLS_ZOOM_PATH			ACCEL_TOOLS "/Zoom Tool"
#define ACCEL_TOOLS_ZOOM_KEY			GDK_3
#define ACCEL_TOOLS_ZOOM_MOD			(GdkModifierType) 0
#define ACCEL_TOOLS_MEASURE_PATH		ACCEL_TOOLS "/Measure Tool"
#define ACCEL_TOOLS_MEASURE_KEY			GDK_4
#define ACCEL_TOOLS_MEASURE_MOD			(GdkModifierType) 0

#define ACCEL_HELP						ACCEL_ROOT "help"

void
interface_create_gui (int req_width, int req_height);

void
interface_set_render_type (int);

void rename_main_window(char const* filename, GtkWidget* main_win);

void
set_window_icon (GtkWidget * this_window);

gboolean
interface_get_alert_dialog_response (gchar *primaryText, 
				     gchar *secondaryText,
                                     gboolean show_checkbox, 
				     gboolean *ask_to_show_again );

void
interface_show_alert_dialog (gchar *primaryText, 
			     gchar *secondaryText, 
			     gboolean show_checkbox, 
			     gboolean *ask_to_show_again );

void
interface_show_modify_orientation_dialog (gerbv_user_transformation_t *transform, gerbv_unit_t screenUnit);
