/*
 * gEDA - GNU Electronic Design Automation
 *
 * selection.h -- this file is a part of gerbv.
 *
 *   Copyright (C) 2014 Sergey Alyoshin <alyoshin.s@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** \file selection.h
    \brief Header info for the selection support functions for libgerbv
    \ingroup libgerbv
*/

GArray *selection_new_array (void);
guint selection_length (gerbv_selection_info_t *sel_info);
void selection_add_item (gerbv_selection_info_t *sel_info,
					gerbv_selection_item_t *item);
gerbv_selection_item_t selection_get_item_by_index (
				gerbv_selection_info_t *sel_info, guint idx);
void selection_clear_item_by_index (
				gerbv_selection_info_t *sel_info, guint idx);
void selection_clear (gerbv_selection_info_t *sel_info);

