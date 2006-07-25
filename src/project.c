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

/*! @file project.c
    @brief routines for loading and saving project files */ 


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>

#include "gerb_error.h"
#include "gerb_file.h"
#include "gerbv_screen.h"
#include "project.h"
#include "scheme-private.h"
#include "search_gui.h"



static project_list_t *plist_top = NULL;

static void
get_color(scheme *sc, pointer value, int *color)
{
    int i;
    pointer elem;

    if (!sc->vptr->is_vector(value)) {
	GERB_MESSAGE("Color parameter not a vector\n");
	return;
    }
    if (sc->vptr->vector_length(value) != 3) {
	GERB_MESSAGE("Color vector of incorrect length\n");
	return;
    }
    
    for (i = 0; i < 3; i++) {
	elem = sc->vptr->vector_elem(value, i);
	if (sc->vptr->is_integer(elem) && sc->vptr->is_number(elem))
	    color[i] = sc->vptr->ivalue(elem);
	else { 
	    color[i] = -1;
	    GERB_MESSAGE("Illegal color in projectfile\n");
	}
    }
    
    return;
} /* get_color */


static char *
get_value_string(scheme *sc, pointer value)
{
    if (!sc->vptr->is_string(value))
	return NULL;

    return sc->vptr->string_value(value);
} /* get_value_string */


/** Conversion of '\' into '/' and vice versa for compatibility under WIN32 platforms. */
char *
convert_path_separators(char* path, int conv_flag)
{
    char     *hit_in_path;
    
    switch (conv_flag) {
    
    case MINGW_UNIX:
        while   ((hit_in_path = strchr(path, '\\'))) {
            *hit_in_path = '/';
        }
        break;
    case UNIX_MINGW:
         while   ((hit_in_path = strchr(path, '/'))) {
            *hit_in_path = '\\';
        }
        break;
    }    
    return path;
}/* convert_path_separators */


static pointer
define_layer(scheme *sc, pointer args)
{
    pointer car_el, cdr_el, name, value;
    int layerno;
    project_list_t *plist_tmp = NULL;

    if (!sc->vptr->is_pair(args)){
	GERB_MESSAGE("define-layer!: Too few arguments\n");
	return sc->F;
    }

    car_el = sc->vptr->pair_car(args);
    cdr_el = sc->vptr->pair_cdr(args);

    if (!sc->vptr->is_integer(car_el) || !sc->vptr->is_number(car_el)) {
	GERB_MESSAGE("define-layer!: Layer number missing/incorrect\n");
	return sc->F;
    }

    layerno = sc->vptr->ivalue(car_el);

    car_el = sc->vptr->pair_car(cdr_el);
    cdr_el = sc->vptr->pair_cdr(cdr_el);

    plist_tmp = (project_list_t *)malloc(sizeof(project_list_t));
    memset(plist_tmp, 0, sizeof(project_list_t));
    plist_tmp->next = plist_top;
    plist_top = plist_tmp;
    plist_top->layerno = layerno;

    while (sc->vptr->is_pair(car_el)) {

	name = sc->vptr->pair_car(car_el);
	value =  sc->vptr->pair_cdr(car_el);

	if (!sc->vptr->is_symbol(name)) {
	    GERB_MESSAGE("define-layer!:non-symbol found, ignoring\n");
	    goto end_name_value_parse;
	}

	if (strcmp(sc->vptr->symname(name), "color") == 0) {
	    get_color(sc, value, plist_top->rgb);
	} else if (strcmp(sc->vptr->symname(name), "filename") == 0) {
#if defined (__MINGW32__)    
            plist_top->filename = convert_path_separators(strdup(get_value_string(sc, value)), UNIX_MINGW);
#else
            plist_top->filename = strdup(get_value_string(sc, value));
#endif
            plist_top->is_pnp = 0;
    } else if (strcmp(sc->vptr->symname(name), "pick_and_place") == 0) {
#if defined (__MINGW32__)    
            plist_top->filename = convert_path_separators(strdup(get_value_string(sc, value)), UNIX_MINGW);
#else
            plist_top->filename = strdup(get_value_string(sc, value));
#endif    
            plist_top->is_pnp = 1;
  	} else if (strcmp(sc->vptr->symname(name), "inverted") == 0) {
	    if (value ==  sc->F) {
		plist_top->inverted = 0;
	    } else if (value == sc->T) {
		plist_top->inverted = 1;
	    } else {
		GERB_MESSAGE("Argument to inverted must be #t or #f\n");
	    }
	}

    end_name_value_parse:
	car_el = sc->vptr->pair_car(cdr_el);
	cdr_el = sc->vptr->pair_cdr(cdr_el);
    }
    
    return sc->NIL;
} /* define_layer */


/** Reads the content of a project file.
  *  Global:\n
  *    Background color,\n
  *    global path,\n
  *    corresponding pick and place file: labelled 'picknplace'\n
  *   Per layer:\n
  *    layer color,\n
  *    layer filename
  */
project_list_t *
read_project_file(char *filename)
{
    struct stat stat_info;
    scheme *sc;
    FILE *fd;
    char *initdirs[] = {BACKEND_DIR, screen.execpath, ".", 
			"$GERBV_SCHEMEINIT", NULL};
    char *initfile;

    if (stat(filename, &stat_info)) {
	GERB_MESSAGE("Failed to read %s\n", filename);
	return NULL;
    }

    if (!S_ISREG(stat_info.st_mode)) {
	GERB_MESSAGE("Failed to read %s\n", filename);
	return NULL;
    }

    sc = scheme_init_new();
    scheme_set_output_port_file(sc, stdout);

    if(!sc){
	GERB_FATAL_ERROR("Couldn't init scheme\n");
	exit(1);
    }

    errno = 0;
    initfile = gerb_find_file("init.scm", initdirs);
    if (initfile == NULL) {
	scheme_deinit(sc);
	GERB_MESSAGE("Problem loading init.scm (%s)\n", strerror(errno));
	return NULL;
    }
    if ((fd = fopen(initfile, "r")) == NULL) {
	scheme_deinit(sc);
	GERB_MESSAGE("Couldn't open %s (%s)\n", initfile, strerror(errno));
	return NULL;
    }
    sc->vptr->load_file(sc, fd);
    fclose(fd);

    sc->vptr->scheme_define(sc, sc->global_env, 
			    sc->vptr->mk_symbol(sc, "define-layer!"),
			    sc->vptr->mk_foreign_func(sc, define_layer));

    if ((fd = fopen(filename, "r")) == NULL) {
	scheme_deinit(sc);
	GERB_MESSAGE("Couldn't open project file %s (%s)\n", filename,
		     strerror(errno));
	return NULL;
    }

    plist_top = NULL;

    scheme_load_file(sc, fd);
    fclose(fd);

    scheme_deinit(sc);

    return plist_top;
} /* read_project */


/*
 * Writes a description of a project to a file 
 * that can be parsed by read_project above
 */
int 
write_project_file(char *filename, project_list_t *project)
{
    FILE *fd;
    project_list_t *p = project, *tmp;

    if ((fd = fopen(filename, "w")) == NULL) {
	    GERB_MESSAGE("Couldn't save project %s\n", filename);
	    return(-1);
    }
    while (p) {
	fprintf(fd, "(define-layer! %d ", p->layerno);
	
        if ((interface.pnp_filename != NULL) && (strncmp(p->filename, interface.pnp_filename, strlen(interface.pnp_filename)) == 0)) 
#if defined (__MINGW32__)    
    	    fprintf(fd, "(cons 'pick_and_place \"%s\")", convert_path_separators(p->filename, MINGW_UNIX));
	
#else
	    fprintf(fd, "(cons 'pick_and_place \"%s\")", p->filename);
        else
#endif
#if defined (__MINGW32__)
            fprintf(fd, "(cons 'filename \"%s\")", convert_path_separators(p->filename, MINGW_UNIX));    
#else
            fprintf(fd, "(cons 'filename \"%s\")", p->filename);    
#endif
    
	if (p->inverted)
	    fprintf(fd, "(cons 'inverted #t)");
	fprintf(fd, "(cons 'color #(%d %d %d)))", p->rgb[0], p->rgb[1],	p->rgb[2]);
	fprintf(fd, "\n");
	tmp = p;
	p = p->next;
	free(tmp);
	tmp = NULL;
    }

    fclose(fd);

    return(0);
} /* write_project */
