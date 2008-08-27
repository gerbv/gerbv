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

/*! \file project.c
    \brief routines for loading and saving project files
    \ingroup gerbv
 */ 


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

#include "gerbv.h"
#include "gerb_file.h"
#include "project.h"
#include "scheme-private.h"
#include "main.h"
#include "interface.h"
#include "render.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

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


/** Conversion of '\' into '/' and vice versa for compatibility under WIN32 
    platforms. */
static char *
convert_path_separators(char* path, int conv_flag)
{
#if defined (__MINGW32__)        
    char     *hit_in_path;

    switch (conv_flag) {
	
    case MINGW_UNIX:
        while ((hit_in_path = strchr(path, '\\'))) {
            *hit_in_path = '/';
        }
        break;
    case UNIX_MINGW:
	while ((hit_in_path = strchr(path, '/'))) {
            *hit_in_path = '\\';
        }
        break;
    }    
#endif

    return path;
}/* convert_path_separators */


static pointer
define_layer(scheme *sc, pointer args)
{
    pointer car_el, cdr_el, name, value;
    int layerno;
    project_list_t *plist_tmp = NULL;

    dprintf("--> entering project.c:define_layer\n");

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
    dprintf("    layerno = %d\n", layerno);
    
    car_el = sc->vptr->pair_car(cdr_el);
    cdr_el = sc->vptr->pair_cdr(cdr_el);
    
    plist_tmp = (project_list_t *)g_malloc(sizeof(project_list_t));
    memset(plist_tmp, 0, sizeof(project_list_t));
    plist_tmp->next = plist_top;
    plist_top = plist_tmp;
    plist_top->layerno = layerno;
    plist_top->visible = 1;
    plist_top->n_attr = 0;
    plist_top->attr_list = NULL;

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
	    
            plist_top->filename = g_strdup(get_value_string(sc, value));
	    plist_top->filename = convert_path_separators(plist_top->filename, 
							  UNIX_MINGW);
            plist_top->is_pnp = 0;
	} else if (strcmp(sc->vptr->symname(name), "pick_and_place") == 0) {

	    plist_top->filename = g_strdup(get_value_string(sc, value));
	    plist_top->filename = convert_path_separators(plist_top->filename, 
							  UNIX_MINGW);
	    plist_top->is_pnp = 1;
	} else if (strcmp(sc->vptr->symname(name), "inverted") == 0) {
	    if (value == sc->F) {
		plist_top->inverted = 0;
	    } else if (value == sc->T) {
		plist_top->inverted = 1;
	    } else {
		GERB_MESSAGE("Argument to inverted must be #t or #f\n");
	    }
	} else if (strcmp(sc->vptr->symname(name), "visible") == 0) {
	    if (value == sc->F) {
		plist_top->visible = 0;
	    } else if (value == sc->T) {
		plist_top->visible = 1;
	    } else {
		GERB_MESSAGE("Argument to visible must be #t or #f\n");
	    }
       	} else if (strcmp(sc->vptr->symname(name), "attribs") == 0) {
	    pointer attr_car_el, attr_cdr_el;
	    pointer attr_name, attr_type, attr_value;
	    char *type;

	    dprintf ("Parsing file attributes\n");

	    attr_car_el = sc->vptr->pair_car (value);
	    attr_cdr_el = sc->vptr->pair_cdr (value);
	    while (sc->vptr->is_pair(attr_car_el)) {
		int p = plist_top->n_attr;
		plist_top->n_attr++;
		plist_top->attr_list = (gerbv_HID_Attribute *) 
		    realloc (plist_top->attr_list, 
			     plist_top->n_attr * sizeof (gerbv_HID_Attribute));
		if (plist_top->attr_list == NULL ) {
		    fprintf (stderr, "%s():  realloc failed\n", __FUNCTION__);
		    exit (1);
		}								  

		/* car */
		attr_name = sc->vptr->pair_car(attr_car_el);

		/* cadr */
		attr_type =  sc->vptr->pair_cdr (attr_car_el);
		attr_type =  sc->vptr->pair_car (attr_type);

		/* caddr */
		attr_value =  sc->vptr->pair_cdr (attr_car_el);
		attr_value =  sc->vptr->pair_cdr (attr_value);
		attr_value =  sc->vptr->pair_car (attr_value);

		dprintf ("  attribute %s, type is %s, value is ", 
			 sc->vptr->symname(attr_name),
			 sc->vptr->symname(attr_type));

		plist_top->attr_list[p].name = strdup (sc->vptr->symname (attr_name));

		type = sc->vptr->symname (attr_type);

		if (strcmp (type, "label") == 0) {
		    dprintf ("%s", sc->vptr->string_value (attr_value));
		    plist_top->attr_list[p].type = HID_Label;
		    plist_top->attr_list[p].default_val.str_value = 
			strdup (sc->vptr->string_value (attr_value));

		} else if (strcmp (type, "integer") == 0) {
		    dprintf ("%ld", sc->vptr->ivalue (attr_value));
		    plist_top->attr_list[p].type = HID_Integer;
		    plist_top->attr_list[p].default_val.int_value = 
			sc->vptr->ivalue (attr_value);

		} else if (strcmp (type, "real") == 0) {
		    dprintf ("%g", sc->vptr->rvalue (attr_value));
		    plist_top->attr_list[p].type = HID_Real;
		    plist_top->attr_list[p].default_val.real_value = 
			sc->vptr->rvalue (attr_value);

		} else if (strcmp (type, "string") == 0) {
		    dprintf ("%s", sc->vptr->string_value (attr_value));
		    plist_top->attr_list[p].type = HID_String;
		    plist_top->attr_list[p].default_val.str_value = 
			strdup (sc->vptr->string_value (attr_value));

		} else if (strcmp (type, "boolean") == 0) {
		    dprintf ("%ld", sc->vptr->ivalue (attr_value));
		    plist_top->attr_list[p].type = HID_Boolean;
		    plist_top->attr_list[p].default_val.int_value = 
			sc->vptr->ivalue (attr_value);

		} else if (strcmp (type, "enum") == 0) {
		    dprintf ("%ld", sc->vptr->ivalue (attr_value));
		    plist_top->attr_list[p].type = HID_Enum;
		    plist_top->attr_list[p].default_val.int_value = 
			sc->vptr->ivalue (attr_value);

		} else if (strcmp (type, "mixed") == 0) {
		    plist_top->attr_list[p].type = HID_Mixed;
		    plist_top->attr_list[p].default_val.str_value = NULL;
		    fprintf (stderr, "%s():  WARNING:  HID_Mixed is not yet supported\n",
			     __FUNCTION__);

		} else if (strcmp (type, "path") == 0) {
		    dprintf ("%s", sc->vptr->string_value (attr_value));
		    plist_top->attr_list[p].type = HID_Path;
		    plist_top->attr_list[p].default_val.str_value = 
			strdup (sc->vptr->string_value (attr_value));
		} else {
		    fprintf (stderr, "%s():  Unknown attribute type: \"%s\"\n",
			     __FUNCTION__, type);
		}
		dprintf ("\n");

		attr_car_el = sc->vptr->pair_car(attr_cdr_el);
		attr_cdr_el = sc->vptr->pair_cdr(attr_cdr_el);
	    }

	}
	
    end_name_value_parse:
	car_el = sc->vptr->pair_car(cdr_el);
	cdr_el = sc->vptr->pair_cdr(cdr_el);
    }
    
    return sc->NIL;
} /* define_layer */

static pointer
set_render_type(scheme *sc, pointer args)
{
    pointer car_el, cdr_el;
    int r;

    dprintf("--> entering project.c:%s()\n", __FUNCTION__);

    if (!sc->vptr->is_pair(args)){
	GERB_MESSAGE("set-render-type!: Too few arguments\n");
	return sc->F;
    }

    car_el = sc->vptr->pair_car(args);
    cdr_el = sc->vptr->pair_cdr(args);

    r = sc->vptr->ivalue (car_el);
    dprintf ("%s():  Setting render type to %d\n", __FUNCTION__, r);
    interface_set_render_type (r);

    return sc->NIL;
} /* set_render_type */


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
read_project_file(char const* filename)
{
    struct stat stat_info;
    scheme *sc;
    FILE *fd;
    char *initdirs[] = {BACKEND_DIR, mainProject->execpath, ".", 
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

    sc->vptr->scheme_define(sc, sc->global_env, 
			    sc->vptr->mk_symbol(sc, "set-render-type!"),
			    sc->vptr->mk_foreign_func(sc, set_render_type));

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


void
project_destroy_project_list (project_list_t *projectList){
	project_list_t *tempP,*tempP2;
	
	for (tempP = projectList; tempP != NULL; ){
		tempP2 = tempP->next;
		
		g_free (tempP->filename);
		gerbv_attribute_destroy_HID_attribute (tempP->attr_list, tempP->n_attr);
		tempP->attr_list = NULL;
		tempP = tempP2;
	}
}

/*
 * Writes a description of a project to a file 
 * that can be parsed by read_project above
 */
int 
write_project_file(gerbv_project_t *gerbvProject, char const* filename, project_list_t *project)
{
    FILE *fd;
    project_list_t *p = project;
    int n_attr = 0;
    gerbv_HID_Attribute *attr_list = NULL;
    int i;

    if ((fd = fopen(filename, "w")) == NULL) {
	    GERB_MESSAGE("Couldn't save project %s\n", filename);
	    return(-1);
    }
    while (p) {
	fprintf(fd, "(define-layer! %d ", p->layerno);
	
#if defined (__MINGW32__)
	fprintf(fd, "(cons 'filename \"%s\")", convert_path_separators(p->filename, MINGW_UNIX));    
#else
	fprintf(fd, "(cons 'filename \"%s\")", p->filename);    
#endif
    
	if (p->inverted)
	    fprintf(fd, "(cons 'inverted #t)");
	if (p->visible)
	    fprintf(fd, "(cons 'visible #t)");
	else
	    fprintf(fd, "(cons 'visible #f)");

	fprintf(fd, "(cons 'color #(%d %d %d))", p->rgb[0], p->rgb[1],	p->rgb[2]);

	/* now write out the attribute list which specifies the
	 * file format 
	 */
	if (p->layerno < 0) {
	    attr_list = NULL;
	    n_attr = 0;
	} else {
	    attr_list = gerbvProject->file[p->layerno]->image->info->attr_list;
	    n_attr =  gerbvProject->file[p->layerno]->image->info->n_attr;
	}

	if (n_attr > 0) {
	    fprintf (fd, "(cons 'attribs (list");
	}
	for (i = 0; i < n_attr ; i++) {
	    switch (attr_list[i].type) {
	    case HID_Label:
		  fprintf(fd, " (list '%s 'Label \"%s\")", attr_list[i].name,
			  attr_list[i].default_val.str_value);
		  break;
		  
	      case HID_Integer:
		  fprintf(fd, " (list '%s 'Integer %d)", attr_list[i].name,
			  attr_list[i].default_val.int_value);
		  break;
		  
	      case HID_Real:
		  fprintf(fd, " (list '%s 'Real %g)", attr_list[i].name,
			  attr_list[i].default_val.real_value);
		  break;
		  
	      case HID_String:
		  fprintf(fd, " (list '%s 'String \"%s\")", attr_list[i].name,
			  attr_list[i].default_val.str_value);
		  break;
		  
	      case HID_Boolean:
		  fprintf(fd, " (list '%s 'Boolean %d)", attr_list[i].name,
			  attr_list[i].default_val.int_value);
		  break;
		  
	      case HID_Enum:
		  fprintf(fd, " (list '%s 'Enum %d)", attr_list[i].name,
			  attr_list[i].default_val.int_value);
		  break;

	      case HID_Mixed:
		  dprintf ("HID_Mixed\n");
		  fprintf (stderr, "%s():  WARNING:  HID_Mixed is not yet supported.\n",
			   __FUNCTION__);
		  break;

	      case HID_Path:
		  fprintf(fd, " (list '%s 'Path \"%s\")", attr_list[i].name,
			  attr_list[i].default_val.str_value);
		  break;

	      default:
		  fprintf (stderr, "%s: unknown type of HID attribute (%d)\n", 
			   __FUNCTION__, attr_list[i].type);
		  break;
	      }
	}
	if (n_attr > 0) {
	    fprintf (fd, "))");
	}

	fprintf(fd, ")\n");
	p = p->next;
    }

    fprintf (fd, "(set-render-type! %d)\n", screenRenderInfo.renderType);
	     
    fclose(fd);

    return(0);
} /* write_project */
