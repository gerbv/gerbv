/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
 *   Copyright (C) 2008 Dan McMahill
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
 
/** \file project.h
    \brief routines for loading and saving project files
    \ingroup gerbv
 */ 

#ifndef PROJECT_H
#define PROJECT_H

typedef struct project_list_t {
    int layerno;
    char *filename;
    int rgb[3];
    char inverted;
    char is_pnp;
    char visible;
    gerbv_HID_Attribute *attr_list;
    int n_attr;
    struct project_list_t *next;
} project_list_t;


enum conv_type {
    MINGW_UNIX = 0,
    UNIX_MINGW = 1
};


/*
 * Reads a project from a file and returns a linked list describing the project
 */
project_list_t *read_project_file(char const* filename);


/* Writes a description of a project to a file
 * that can be parsed by read_project above */
int write_project_file(gerbv_project_t *gerbvProject, char const* filename, project_list_t *project);

void
project_destroy_project_list (project_list_t *projectList);
#endif /* PROJECT_H */
