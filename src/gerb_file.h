/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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

/** \file gerb_file.h
    \brief Header info for the file parsing support functions
    \ingroup libgerbv
*/

#ifndef GERB_FILE_H
#define GERB_FILE_H

#include <stdio.h>

typedef struct file {
    FILE *fd;     /* File descriptor */
    int   fileno; /* The integer version of fd */
    char *data;   /* Pointer to data mmaped in. May not be changed, use ptr */
    int   datalen;/* Length of mmaped data ie file length */
    int   ptr;    /* Index in data where we are reading */
    char *filename;  /* File name */
} gerb_file_t;


gerb_file_t *gerb_fopen(char const* filename);
int gerb_fgetc(gerb_file_t *fd);
int gerb_fgetint(gerb_file_t *fd, int *len); /* If len != NULL, returns number
						of chars parsed in len */
double gerb_fgetdouble(gerb_file_t *fd);
char *gerb_fgetstring(gerb_file_t *fd, char term);
void gerb_ungetc(gerb_file_t *fd);
void gerb_fclose(gerb_file_t *fd);

extern
const char path_separator;

/*
 * Search for files in directories pointed out by paths, a NULL terminated
 * list of directories to search. If a string in paths starts with a $, then
 * characters to / (or string end if no /) is interpreted as a environment
 * variable.
 */
char *gerb_find_file(char const * filename, char **paths);

#endif /* GERB_FILE_H */
