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

#ifndef SETUP_H
#define SETUP_H

#define DEF_DISTFONTNAME "-*-helvetica-bold-r-normal--*-120-*-*-*-*-iso8859-1"
#define DEF_STATUSFONTNAME "-*-fixed-*-*-normal--*-100-*-*-*-*-iso8859-1"

typedef struct {
    char *status_fontname;
    char *dist_fontname;
    struct {
        int to_file;      /* Log to file */
        char *filename;
    } log;
} setup_t;

extern setup_t setup;

void setup_init(void);
void setup_destroy(void);

#endif /* SETUP_H */
