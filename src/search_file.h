/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2004 Juergen Haas (juergenhaas@gmx.net)
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

#ifndef SEARCH_FILE_H
#define SEARCH_FILE_H

#ifdef USE_GTK2
#include <stdio.h>
#include <gtk/gtk.h>

typedef struct pnp_file {
    FILE *fd;
    int   fileno;
    char *data;
    int   datalen;
    int   ptr;
    GtkListStore     	*model;	

} pnp_file_t;


pnp_file_t *pnp_fopen(char *filename);
int pnp_fgetc(pnp_file_t *fd);
int pnp_fgetint(pnp_file_t *fd);
double pnp_fgetdouble(pnp_file_t *fd);
char *pnp_fgetstring(pnp_file_t *fd, char term);
void pnp_ungetc(pnp_file_t *fd);
void pnp_fclose(pnp_file_t *fd);

#endif /* USE_GTK2 */
#endif /* SEARCH_H */
