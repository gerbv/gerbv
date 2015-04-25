/*
 * gEDA - GNU Electronic Design Automation
 *
 * selection.c -- this file is a part of gerbv.
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

/** \file selection.c
    \brief Selection support functions for libgerbv
    \ingroup libgerbv
*/

#include "gerbv.h"

GArray *selection_new_array (void)
{
	return g_array_new (FALSE, FALSE, sizeof (gerbv_selection_item_t));
}

gchar *selection_free_array (gerbv_selection_info_t *sel_info)
{
	return g_array_free (sel_info->selectedNodeArray, FALSE);
}

inline guint selection_length (gerbv_selection_info_t *sel_info)
{
	return sel_info->selectedNodeArray->len;
}

inline gerbv_selection_item_t selection_get_item_by_index (
			gerbv_selection_info_t *sel_info, guint idx)
{
	return g_array_index (sel_info->selectedNodeArray,
				gerbv_selection_item_t, idx);
}

inline void selection_clear_item_by_index (
			gerbv_selection_info_t *sel_info, guint idx)
{
	g_array_remove_index (sel_info->selectedNodeArray, idx);
}

inline void selection_clear (gerbv_selection_info_t *sel_info)
{
	g_array_remove_range (sel_info->selectedNodeArray, 0,
			sel_info->selectedNodeArray->len);
}

inline void selection_add_item (gerbv_selection_info_t *sel_info,
				gerbv_selection_item_t *item)
{
	g_array_append_val (sel_info->selectedNodeArray, *item);
}

