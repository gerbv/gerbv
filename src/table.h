/*
 * gEDA - GNU Electronic Design Automation
 * table.h -- This file is a part of gerbv
 *
 * Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
 * Copyright (C) 2002-2020 sourceforge contributors
 * Copyright (C) 2020-2021 github contributors
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

/** \file table.h
    \brief Header info for GTK widgets table functions.
    \ingroup gerbv
 */ 

#ifndef GERBV_TABLE_H
#define GERBV_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

struct table {
	GtkWidget	*widget;	/* All table */
	GtkListStore	*list_store;
	GType		*types;		/* Column types array */
	GtkCellRenderer **renderers;	/* Column renderers pointers array */
	gint		column_nums;	/* Column number */
};

struct table *table_new_with_columns(gint col_nums, ...);
void table_destroy(struct table *table);
void table_set_sortable(struct table *table);
void table_set_column_align(struct table *table, gint column_num, gfloat align);
int table_add_row(struct table *table, ...);

#ifdef __cplusplus
}
#endif

#endif /* GERBV_TABLE_H */
