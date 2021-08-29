/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
 *   Copyright (c) 2008 Dan McMahill
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
    \brief Routines for loading and saving project files.
    \ingroup gerbv
 */ 


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <ctype.h>
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
#include <math.h>

#include "common.h"
#include "gerbv.h"
#include "gerb_file.h"
#include "lrealpath.h"
#include "project.h"
#include "scheme-private.h"
#include "main.h"
#include "interface.h"
#include "render.h"


/*
 * update this if the project file format changes.
 *
 * The format *must* be major.minor[A-Z]
 *
 * Do *not* update this simply because we have a new gerbv
 * version.
 *
 * If you bump this version, then you must also bump the 
 * version of gerbv.  For example, supplse the file version
 * is 2.0A and gerbv has 2.4B in configure.ac.  If you change
 * the file format version, you should change both the version
 * here *and* configure.ac to 2.4C.
 */

#define GERBV_PROJECT_FILE_VERSION "2.0A"

/* default version for project files that do not specify a version.
 * This is assumed for all older project files.
 */
#define GERBV_DEFAULT_PROJECT_FILE_VERSION "1.9A"

/* 
 * List of versions that we can load with this version of 
 * gerbv 
 */
static const char * known_versions[] = {
  "1.9A",
  "2.0A",
  NULL
};

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

static project_list_t *project_list_top = NULL;

static const int alpha_def_value = 177*257;
  
/* When a project file is loaded, this variable is set to the
 * version of the project file.  That can be used by various 
 * functions which may need to do something different.
 */
static int current_file_version = 0;


/* 
 * Converts a string like "2.1A" "2.12C" or "3.2ZA" to the int
 * we use internally
 */
static int
version_str_to_int( const char * str)
{
  int r = 0;
  gchar *dup, *tmps, *ptr;

  if(str == NULL) {
    return -1;
  } else {
    dprintf("%s(\"%s\")\n", __FUNCTION__, str);


    /* 
     * Extract out the major number (versions are strings like 2.1A)
     * and we want the "2" here.
     */
    tmps = g_strdup(str);
    ptr = tmps;
    while(*ptr != '\0' && *ptr != '.') { ptr++; }
    if( *ptr == '\0' ) {
      /* this should not have happened */
      return -1;
    }

    *ptr = '\0';
    r = 10000 * atoi(tmps);
    dprintf("%s():  Converted \"%s\" to r = %d\n", __FUNCTION__, tmps, r);

    g_free(tmps);

    /* 
     * Extract out the minor number (versions are strings like 2.1A)
     * and we want the "1" here.
     */
    dup = g_strdup(str);
    tmps = dup;
    ptr = tmps;

    while(*ptr != '\0' && *ptr != '.') { ptr++; }
    if( *ptr == '\0' ) {
      /* this should not have happened */
      return -1;
    }
    ptr++;
    tmps = ptr;

    while(*ptr != '\0' && isdigit( (int) *ptr)) { ptr++; }
    if( *ptr == '\0' ) {
      /* this should not have happened */
      return -1;
    }

    *ptr = '\0';
    r += 100 * atoi(tmps);
    dprintf("%s():  Converted \"%s\" to r = %d\n", __FUNCTION__, tmps, r);

    g_free(dup);

    /* 
     * Extract out the revision letter(s) (versions are strings like 2.1A)
     * and we want the "A" here.
     */

    dup = g_strdup(str);
    tmps = dup;
    ptr = tmps;

    while(*ptr != '\0' && (isdigit( (int) *ptr) || *ptr == '.') ) { ptr++; }
    if( *ptr == '\0' ) {
      /* this should not have happened */
      return -1;
    }
    tmps = ptr;

    dprintf("%s():  Processing \"%s\"\n", __FUNCTION__, tmps);

    if( strlen(tmps) == 1) {
      r += *tmps - 'A' + 1;
      dprintf( "%s():  Converted \"%s\" to r = %d\n", __FUNCTION__, tmps, r);
    } else if( strlen(tmps) == 2 ) {
      if( *tmps == 'Z' ) {
	r += 26;
	tmps++;
	r += *tmps - 'A' + 1;
      } else {
	/* this should not have happened */
	return -1;
      }
    } else {
      /* this should not have happened */
      return -1;
    }
      
    g_free(dup);

  }
  return r;

}

/* 
 * convert the internal int we use for version numbers
 * to the string that users can deal with
 */
static char *
version_int_to_str( int ver )
{
  int major, minor, teeny;
  char l[3];
  char *str;

  l[0] = '\0';
  l[1] = '\0';
  l[2] = '\0';

  major = ver / 10000;
  minor = (ver - 10000*major) / 100;
  teeny = (ver - 10000*major - 100*minor);
  if(teeny >= 1 && teeny <= 26) {
    l[0] = 'A' + teeny - 1;
  }  else if(teeny > 26 && teeny <= 52) {
    l[0] = 'Z';
    l[1] = 'A' + teeny - 26 - 1;
  }

  str = g_strdup_printf("%d.%d%s", major, minor, l);
  return str;
}

static int
check_vector_and_length(scheme *sc, pointer value,
		unsigned int length, const char *item)
{
    if (!sc->vptr->is_vector(value)) {
	GERB_MESSAGE(_("'%s' parameter not a vector"), item);

	return 1;
    }

    if (sc->vptr->vector_length(value) != length) {
	GERB_MESSAGE(_("'%s' vector of incorrect length"), item);

	return 2;
    }

    return 0;
}

static void
get_color(scheme *sc, pointer value, int *color)
{
    int i;
    pointer elem;

    if (check_vector_and_length(sc, value, 3, "color"))
	    return;
    
    for (i = 0; i < 3; i++) {
	elem = sc->vptr->vector_elem(value, i);
	if (sc->vptr->is_integer(elem) && sc->vptr->is_number(elem))
	    color[i] = sc->vptr->ivalue(elem);
	else { 
	    color[i] = -1;
	    GERB_MESSAGE(_("Illegal color in projectfile"));
	}
    }
    
    return;
} /* get_color */

static void
get_alpha(scheme *sc, pointer value, int *alpha)
{
    pointer elem;

    if (check_vector_and_length(sc, value, 1, "alpha"))
	return;

    elem = sc->vptr->vector_elem(value, 0);
    if (sc->vptr->is_integer(elem) && sc->vptr->is_number(elem)) {
        *alpha = sc->vptr->ivalue(elem);
        return;
    }

    GERB_MESSAGE(_("Illegal alpha value in projectfile"));
} /* get_alpha */

static void
get_double(scheme *sc, pointer value, char *item, double *x, double def)
{
    pointer elem;

    if (check_vector_and_length(sc, value, 1, item))
	return;

    elem = sc->vptr->vector_elem(value, 0);
    if (sc->vptr->is_real(elem) && sc->vptr->is_number(elem)) {
	*x = sc->vptr->rvalue(elem);
    } else {
	*x = def;
	GERB_MESSAGE(_("Illegal %s in projectfile"), item);
    }
} /* get_double */

static void
get_double_pair(scheme *sc, pointer value, char *item,
	double *x, double *y, double def)
{
    pointer elem;

    if (check_vector_and_length(sc, value, 2, item))
	return;

    elem = sc->vptr->vector_elem(value, 0);
    if (sc->vptr->is_real(elem) && sc->vptr->is_number(elem)) {
	*x = sc->vptr->rvalue(elem);
    } else {
	*x = def;
	GERB_MESSAGE(_("Illegal %s in projectfile"), item);
    }

    elem = sc->vptr->vector_elem(value, 1);
    if (sc->vptr->is_real(elem) && sc->vptr->is_number(elem)) {
	*y = sc->vptr->rvalue(elem);
    } else {
	*y = def;
	GERB_MESSAGE(_("Illegal %s in projectfile"), item);
    }
} /* get_double_pair */

static void
get_bool_pair(scheme *sc, pointer value, char *item,
		char *x, char *y, char def)
{
    pointer elem;

    if (check_vector_and_length(sc, value, 2, item))
	return;

    elem = sc->vptr->vector_elem(value, 0);
    if (elem == sc->F) {
	*x = 0;
    } else if (elem == sc->T) {
	*x = 1;
    } else {
	*x = def;
	GERB_MESSAGE(_("Illegal %s in projectfile"), item);
    }

    elem = sc->vptr->vector_elem(value, 1);
    if (elem == sc->F) {
	*y = 0;
    } else if (elem == sc->T) {
	*y = 1;
    } else {
	*y = def;
	GERB_MESSAGE(_("Illegal %s in projectfile"), item);
    }
} /* get_bool_pair */

/* ----------------------------------------------------------------------
 * Figure out the canonical name of the executed program
 * and fix up the defaults for various paths.  This is largely
 * taken from InitPaths() in main.c from pcb.
 */
static char *bindir = NULL;
static char *exec_prefix = NULL;
static char *pkgdatadir = NULL;
static gchar *scmdatadir = NULL;

/* this really should not be needed but it could
 * be hooked in to appease malloc debuggers as
 * we don't otherwise free these variables.  However,
 * they only get malloc-ed once ever so this
 * is a fixed leak of a small size.
 */
#if 0
void
destroy_paths ()
{
  if (bindir != NULL) {
    free (bindir);
    bindir = NULL;
  }

  if (exec_prefix != NULL) {
    free (exec_prefix);
    exec_prefix = NULL;
  }

  if (pkgdatadir != NULL) {
    free (pkgdatadir);
    pkgdatadir = NULL;
  }

  if (scmdatadir != NULL) {
    g_free (scmdatadir);
    scmdatadir = NULL;
  }


}
#endif

static void
init_paths (char *argv0)
{
  size_t l;
  int haspath;
  char *t1, *t2;
  int found_bindir = 0;

  /* Only do this stuff once */
  if (bindir != NULL )
    return;

  /* see if argv0 has enough of a path to let lrealpath give the
   * real path.  This should be the case if you invoke gerbv with
   * something like /usr/local/bin/gerbv or ./gerbv or ./foo/gerbv
   * but if you just use gerbv and it exists in your path, you'll
   * just get back gerbv again.
   */
  
  haspath = 0;
  for (unsigned int i = 0; i < strlen (argv0) ; i++)
    {
      if (argv0[i] == GERBV_DIR_SEPARATOR_C) 
        haspath = 1;
    }
  
  dprintf("%s (%s): haspath = %d\n", __FUNCTION__, argv0, haspath);
  if (haspath)
    {
      bindir = strdup (lrealpath (argv0));
      found_bindir = 1;
    }
  else
    {
      char *path, *p, *tmps;
      struct stat sb;
      int r;
      
      tmps = getenv ("PATH");
      
      if (tmps != NULL)
        {
          path = strdup (tmps);
	  
          /* search through the font path for a font file */
          for (p = strtok (path, GERBV_PATH_DELIMETER); p && *p;
               p = strtok (NULL, GERBV_PATH_DELIMETER))
            {
	      dprintf ("Looking for %s in %s\n", argv0, p);
              if ( (tmps = malloc ( (strlen (argv0) + strlen (p) + 2) * sizeof (char))) == NULL )
                {
                  fprintf (stderr, "malloc failed in %s()\n", __FUNCTION__);
                  exit (1);
                }
              sprintf (tmps, "%s%s%s", p, GERBV_DIR_SEPARATOR_S, argv0);
              r = stat (tmps, &sb);
              if (r == 0)
                {
		  dprintf ("Found it:  \"%s\"\n", tmps);
                  bindir = lrealpath (tmps);
                  found_bindir = 1;
                  free (tmps);
                  break;
                }
              free (tmps);
            }
          free (path);
        }
    }
  dprintf ("%s():  bindir = \"%s\"\n", __FUNCTION__, bindir);
  

  if (found_bindir)
    {
      /* strip off the executible name leaving only the path */
      t2 = NULL;
      t1 = strchr (bindir, GERBV_DIR_SEPARATOR_C);
      while (t1 != NULL && *t1 != '\0')
        {
          t2 = t1;
          t1 = strchr (t2 + 1, GERBV_DIR_SEPARATOR_C);
        }
      if (t2 != NULL)
        *t2 = '\0';
      dprintf ("After stripping off the executible name, we found\n");
      dprintf ("bindir = \"%s\"\n", bindir);
      
    }
  else
    {
      /* we have failed to find out anything from argv[0] so fall back to the original
       * install prefix
       */
       bindir = strdup (BINDIR);
    }
    
  /* now find the path to exec_prefix */
  l = strlen (bindir) + 1 + strlen (BINDIR_TO_EXECPREFIX) + 1;
  if ( (exec_prefix = (char *) malloc (l * sizeof (char) )) == NULL )
    {
      fprintf (stderr, "malloc failed in %s()\n", __FUNCTION__);
      exit (1);
    }
  sprintf (exec_prefix, "%s%s%s", bindir, GERBV_DIR_SEPARATOR_S,
           BINDIR_TO_EXECPREFIX);
  
  /* now find the path to PKGDATADIR */
  l = strlen (bindir) + 1 + strlen (BINDIR_TO_PKGDATADIR) + 1;
  if ( (pkgdatadir = (char *) malloc (l * sizeof (char) )) == NULL )
    {
      fprintf (stderr, "malloc failed in %s()\n", __FUNCTION__);
      exit (1);
    }
  sprintf (pkgdatadir, "%s%s%s", bindir, GERBV_DIR_SEPARATOR_S,
           BINDIR_TO_PKGDATADIR);
  
  scmdatadir = g_strdup_printf ("%s%s%s", pkgdatadir, GERBV_DIR_SEPARATOR_S, SCMSUBDIR);

  dprintf ("%s():  bindir      = %s\n", __FUNCTION__, bindir);
  dprintf ("%s():  exec_prefix = %s\n", __FUNCTION__, exec_prefix);
  dprintf ("%s():  pkgdatadir  = %s\n", __FUNCTION__, pkgdatadir);
  dprintf ("%s():  scmdatadir  = %s\n", __FUNCTION__, scmdatadir);
  
}


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
    if (path == NULL) return NULL;

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
    project_list_t *plist;
    const char *str;
    int layerno;

    dprintf("--> entering %s: %s\n", __FILE__, __func__);

    if (!sc->vptr->is_pair(args)) {
	GERB_MESSAGE(_("%s(): too few arguments"), __func__);

	return sc->F;
    }

    car_el = sc->vptr->pair_car(args);
    cdr_el = sc->vptr->pair_cdr(args);

    if (!sc->vptr->is_integer(car_el) || !sc->vptr->is_number(car_el)) {
	GERB_MESSAGE(_("%s(): layer number missing/incorrect"), __func__);

	return sc->F;
    }

    layerno = sc->vptr->ivalue(car_el);
    dprintf("    layerno = %d\n", layerno);
    
    car_el = sc->vptr->pair_car(cdr_el);
    cdr_el = sc->vptr->pair_cdr(cdr_el);
    
    plist = g_new0(project_list_t, 1);
    plist->next = project_list_top;
    project_list_top = plist;
    plist->layerno = layerno;
    plist->visible = 1;
    plist->n_attr = 0;
    plist->attr_list = NULL;
    plist->translate_x = plist->translate_y = 0.0;
    plist->scale_x = plist->scale_y = 1.0;
    plist->mirror_x = plist->mirror_y = 0;

    /* Set default alpha value, if alpha value is not in project file */
    plist->alpha = alpha_def_value;

    while (sc->vptr->is_pair(car_el)) {
	
	name = sc->vptr->pair_car(car_el);
	value =  sc->vptr->pair_cdr(car_el);
	
	if (!sc->vptr->is_symbol(name)) {
	    GERB_MESSAGE(_("%s(): non-symbol found, ignoring"), __func__);
	    goto end_name_value_parse;
	}

	str = sc->vptr->symname(name);
	if (strcmp(str, "color") == 0) {
	    get_color(sc, value, plist->rgb);
	} else if (strcmp(str, "alpha") == 0) {
	    get_alpha(sc, value, &plist->alpha);
	} else if (strcmp(str, "translate") == 0) {
	    get_double_pair(sc, value, "translate",
		    &plist->translate_x, &plist->translate_y, 0.0);
	} else if (strcmp(str, "rotation") == 0) {
	    get_double(sc, value, "rotation", &plist->rotation, 0.0);
	} else if (strcmp(str, "scale") == 0) {
	    get_double_pair(sc, value, "scale",
		    &plist->scale_x, &plist->scale_y, 1.0);
	} else if (strcmp(str, "mirror") == 0) {
	    get_bool_pair(sc, value, "mirror",
		    &plist->mirror_x, &plist->mirror_y, 0);
	} else if (strcmp(str, "filename") == 0) {
            plist->filename = g_strdup(get_value_string(sc, value));
	    plist->filename = convert_path_separators(plist->filename,
			    UNIX_MINGW);
            plist->is_pnp = 0;
	} else if (strcmp(str, "pick_and_place") == 0) {
	    plist->filename = g_strdup(get_value_string(sc, value));
	    plist->filename = convert_path_separators(plist->filename,
			    UNIX_MINGW);
	    plist->is_pnp = 1;
	} else if (strcmp(str, "inverted") == 0) {
	    if (value == sc->F) {
		plist->inverted = 0;
	    } else if (value == sc->T) {
		plist->inverted = 1;
	    } else {
		GERB_MESSAGE(_("Argument to inverted must be #t or #f"));
	    }
	} else if (strcmp(str, "visible") == 0) {
	    if (value == sc->F) {
		plist->visible = 0;
	    } else if (value == sc->T) {
		plist->visible = 1;
	    } else {
		GERB_MESSAGE(_("Argument to visible must be #t or #f"));
	    }
       	} else if (strcmp(str, "attribs") == 0) {
	    pointer attr_car_el, attr_cdr_el;
	    pointer attr_name, attr_type, attr_value;
	    char *type;

	    dprintf ("Parsing file attributes\n");

	    attr_car_el = sc->vptr->pair_car (value);
	    attr_cdr_el = sc->vptr->pair_cdr (value);
	    while (sc->vptr->is_pair(attr_car_el)) {
		int p = plist->n_attr;
		plist->n_attr++;
		plist->attr_list = (gerbv_HID_Attribute *)
		    realloc (plist->attr_list,
			     plist->n_attr * sizeof (gerbv_HID_Attribute));
		if (plist->attr_list == NULL ) {
		    fprintf (stderr, _("%s():  realloc failed\n"), __FUNCTION__);
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

		plist->attr_list[p].name = strdup (sc->vptr->symname (attr_name));

		type = sc->vptr->symname (attr_type);

		if (strcmp (type, "label") == 0) {
		    dprintf ("%s", sc->vptr->string_value (attr_value));
		    plist->attr_list[p].type = HID_Label;
		    plist->attr_list[p].default_val.str_value =
			strdup (sc->vptr->string_value (attr_value));

		} else if (strcmp (type, "integer") == 0) {
		    dprintf ("%ld", sc->vptr->ivalue (attr_value));
		    plist->attr_list[p].type = HID_Integer;
		    plist->attr_list[p].default_val.int_value =
			sc->vptr->ivalue (attr_value);

		} else if (strcmp (type, "real") == 0) {
		    dprintf ("%g", sc->vptr->rvalue (attr_value));
		    plist->attr_list[p].type = HID_Real;
		    plist->attr_list[p].default_val.real_value =
			sc->vptr->rvalue (attr_value);

		} else if (strcmp (type, "string") == 0) {
		    dprintf ("%s", sc->vptr->string_value (attr_value));
		    plist->attr_list[p].type = HID_String;
		    plist->attr_list[p].default_val.str_value =
			strdup (sc->vptr->string_value (attr_value));

		} else if (strcmp (type, "boolean") == 0) {
		    dprintf ("%ld", sc->vptr->ivalue (attr_value));
		    plist->attr_list[p].type = HID_Boolean;
		    plist->attr_list[p].default_val.int_value =
			sc->vptr->ivalue (attr_value);

		} else if (strcmp (type, "enum") == 0) {
		    dprintf ("%ld", sc->vptr->ivalue (attr_value));
		    plist->attr_list[p].type = HID_Enum;
		    plist->attr_list[p].default_val.int_value =
			sc->vptr->ivalue (attr_value);

		} else if (strcmp (type, "mixed") == 0) {
		    plist->attr_list[p].type = HID_Mixed;
		    plist->attr_list[p].default_val.str_value = NULL;
		    fprintf (stderr, _("%s():  WARNING:  HID_Mixed is not yet supported\n"),
			     __FUNCTION__);

		} else if (strcmp (type, "path") == 0) {
		    dprintf ("%s", sc->vptr->string_value (attr_value));
		    plist->attr_list[p].type = HID_Path;
		    plist->attr_list[p].default_val.str_value =
			strdup (sc->vptr->string_value (attr_value));
		} else {
		    fprintf (stderr, _("%s():  Unknown attribute type: \"%s\"\n"),
			     __FUNCTION__, type);
		}
		dprintf ("\n");

		attr_car_el = sc->vptr->pair_car(attr_cdr_el);
		attr_cdr_el = sc->vptr->pair_cdr(attr_cdr_el);
	    }
	} else {
            GERB_MESSAGE(_("Ignoring \"%s\" in project file"), str);
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
    pointer car_el;
    int r;

    dprintf("--> entering project.c:%s()\n", __FUNCTION__);

    if (!sc->vptr->is_pair(args)){
	GERB_MESSAGE(_("set-render-type!: Too few arguments"));
	return sc->F;
    }

    car_el = sc->vptr->pair_car(args);

    r = sc->vptr->ivalue (car_el);
    dprintf ("%s():  Setting render type to %d\n", __FUNCTION__, r);
    interface_set_render_type (r);

    return sc->NIL;
} /* set_render_type */

static pointer
gerbv_file_version(scheme *sc, pointer args)
{
    pointer car_el;
    int r;
    char *vstr;
    char *tmps;

    dprintf("--> entering project.c:%s()\n", __FUNCTION__);

    if (!sc->vptr->is_pair(args)){
	GERB_MESSAGE(_("gerbv-file-version!: Too few arguments"));
	return sc->F;
    }

    car_el = sc->vptr->pair_car(args);
    vstr = get_value_string(sc, car_el);
    
    /* find our internal integer code */
    r = version_str_to_int( vstr );

    if( r == -1) {
      r = version_str_to_int( GERBV_DEFAULT_PROJECT_FILE_VERSION );
      GERB_MESSAGE(_("The project file you are attempting to load has specified that it\n"
		   "uses project file version \"%s\" but this string is not\n"
		   "a valid version.  Gerbv will attempt to load the file using\n"
		   "version \"%s\".  You may experience unexpected results."),
		   vstr, version_int_to_str( r ));
      vstr = GERBV_DEFAULT_PROJECT_FILE_VERSION;
    }
    if( DEBUG ) {
      tmps = version_int_to_str( r );
      printf (_("%s():  Read a project file version of %s (%d)\n"), __FUNCTION__, vstr, r);
      printf (_("      Translated back to \"%s\"\n"), tmps);
      g_free (tmps);
    }

    dprintf ("%s():  Read a project file version of %s (%d)\n", __FUNCTION__, vstr, r);

    if ( r > version_str_to_int( GERBV_PROJECT_FILE_VERSION )) {
        /* The project file we're trying to load is too new for this version of gerbv */
	GERB_MESSAGE(_("The project file you are attempting to load is version \"%s\"\n"
	    "but this copy of gerbv is only capable of loading project files\n"
	    "using version \"%s\" or older.  You may experience unexpected results."),
		     vstr, GERBV_PROJECT_FILE_VERSION);
    } else {
      int i = 0;
      int vok = 0;

      while( known_versions[i] != NULL ) {
	if( strcmp( known_versions[i], vstr) == 0 ) {
	  vok = 1;
	}
	i++;
      }

      if( ! vok ) {
	/* The project file we're trying to load is not too new
	 * but it is unknown to us
	 */
	GERB_MESSAGE(_("The project file you are attempting to load is version \"%s\"\n"
		     "which is an unknown version.\n"
		     "You may experience unexpected results."),
		     vstr);
	
      }
    }

    /*
     * store the version of the file we're currently loading.  This variable is used
     * by the different functions called by the project file to do anything which is
     * version specific.
     */
    current_file_version = r;

    return sc->NIL;
} /* gerbv_file_version */

/** Checks whether the supplied file look like a gerbv project by
 * reading the first line and checking if it contains gerbv-file-version
 *
 * Returns 0 on success -1 on open error
  */
int
project_is_gerbv_project(const char *filename, gboolean *ret)
{
	FILE *fd;
	*ret = FALSE;
	char *buf;
	const gsize buf_size = 200;

	fd = fopen(filename, "rb");
	if (fd == NULL) {
		GERB_MESSAGE(_("Failed to open \"%s\" for reading: %s"),
				filename, strerror(errno));
		return -1;
	}

	buf = (char *) g_malloc(buf_size);
	if (buf == NULL)
		GERB_FATAL_ERROR("malloc buf failed while checking for "
				"Gerbv project in %s()", __FUNCTION__);

	if (fgets(buf, buf_size, fd) != NULL)
		*ret = (g_strrstr(buf, "gerbv-file-version") != NULL);

	fclose(fd);
	g_free(buf);

	return 0;
}

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
    /* always let the environment variable win so one can force
     * a particular init.scm.  Then we use the default installed
     * directory based on where the binary has been installed to
     * (including the possibility of relocation).  Then use the
     * default compiled in directory.  After that try the directory
     * where the binary lives and finally the current directory.
     */
    char *initdirs[] = {"$GERBV_SCHEMEINIT","", BACKEND_DIR, 
                        mainProject->execpath, ".", 
			NULL};
    char *initfile;


    /*
     * Figure out some directories so we can find init.scm
     */
    init_paths(mainProject->execname);
    initdirs[1] = scmdatadir;

#if defined(DEBUG) 
    if (DEBUG > 0)
      {
	int i=0;
	
	while(initdirs[i] != NULL) {
	  printf("%s():  initdirs[%d] = \"%s\"\n", __FUNCTION__, i, initdirs[i]);
	  i++;
	}
      }
#endif 

    /*
     * set the current version of the project file to 1 day before we started adding
     * versioning to the files.  While the file is being loaded, this will
     * be set to the correct version on newer files and ignored on older files
     */
    current_file_version =
		version_str_to_int(GERBV_DEFAULT_PROJECT_FILE_VERSION);

    if (stat(filename, &stat_info) || !S_ISREG(stat_info.st_mode)) {
	GERB_MESSAGE(_("Failed to read %s"), filename);

	return NULL;
    }

    sc = scheme_init_new();
    scheme_set_output_port_file(sc, stdout);

    if(!sc){
	GERB_FATAL_ERROR(_("Couldn't init scheme"));
	exit(1);
    }

    errno = 0;
    initfile = gerb_find_file("init.scm", initdirs);
    if (initfile == NULL) {
	scheme_deinit(sc);
	GERB_MESSAGE(_("Problem loading init.scm (%s)"), strerror(errno));
	return NULL;
    }
    dprintf("%s():  initfile = \"%s\"\n", __FUNCTION__, initfile);

    if ((fd = fopen(initfile, "r")) == NULL) {
	scheme_deinit(sc);
	GERB_MESSAGE(_("Couldn't open %s (%s)"), initfile, strerror(errno));
	return NULL;
    }

    /* Force gerbv to input decimals as dots */
    setlocale(LC_NUMERIC, "C");

    sc->vptr->load_file(sc, fd);
    fclose(fd);

    sc->vptr->scheme_define(sc, sc->global_env, 
			    sc->vptr->mk_symbol(sc, "define-layer!"),
			    sc->vptr->mk_foreign_func(sc, define_layer));

    sc->vptr->scheme_define(sc, sc->global_env, 
			    sc->vptr->mk_symbol(sc, "set-render-type!"),
			    sc->vptr->mk_foreign_func(sc, set_render_type));

    sc->vptr->scheme_define(sc, sc->global_env, 
			    sc->vptr->mk_symbol(sc, "gerbv-file-version!"),
			    sc->vptr->mk_foreign_func(sc, gerbv_file_version));

    if ((fd = fopen(filename, "r")) == NULL) {
	setlocale(LC_NUMERIC, "");	/* Default locale */
	scheme_deinit(sc);
	GERB_MESSAGE(_("Couldn't open project file %s (%s)"), filename,
		     strerror(errno));

	return NULL;
    }

    project_list_top = NULL;

    scheme_load_file(sc, fd);
    fclose(fd);

    setlocale(LC_NUMERIC, "");	/* Default locale */
    scheme_deinit(sc);

    return project_list_top;
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
    const float min_val = GERBV_PRECISION_LINEAR_INCH;
    int i;

    if ((fd = fopen(filename, "w")) == NULL) {
	    GERB_MESSAGE(_("Couldn't save project %s"), filename);
	    return -1;
    }

    /* Force gerbv to input decimals as dots */
    setlocale(LC_NUMERIC, "C");

    fprintf(fd, "(gerbv-file-version! \"%s\")\n", GERBV_PROJECT_FILE_VERSION);

    while (p) {
	fprintf(fd, "(define-layer! %d ", p->layerno);
	
	fprintf(fd, "(cons 'filename \"%s\")\n",
		convert_path_separators(p->filename, MINGW_UNIX));
    
	if (p->inverted)
	    fprintf(fd, "\t(cons 'inverted #t)\n");

	if (p->layerno >= 0) {
	    fprintf(fd, "\t(cons 'visible #%c)\n", p->visible? 't': 'f');
	}

	fprintf(fd, "\t(cons 'color #(%d %d %d))\n",
		p->rgb[0], p->rgb[1], p->rgb[2]);

	if (p->layerno >= 0) {
	    if (p->alpha != alpha_def_value)
		fprintf(fd, "\t(cons 'alpha #(%d))\n", p->alpha);

	    /* Check if there is transformation. Write if so. */
	    if ((fabs(p->translate_x) > min_val)
		    || (fabs(p->translate_y) > min_val)) {
		fprintf(fd, "\t(cons 'translate #(%f %f))\n",
			p->translate_x, p->translate_y);
	    }
	    if (fabs(p->rotation) > GERBV_PRECISION_ANGLE_RAD) {
		fprintf(fd, "\t(cons 'rotation #(%f))\n", p->rotation);
	    }
	    if ((fabs(p->scale_x - 1.0) > min_val)
		    || (fabs(p->scale_y - 1.0) > min_val)) {
		fprintf(fd, "\t(cons 'scale #(%f %f))\n",
				p->scale_x, p->scale_y);
	    }
	    if (p->mirror_x || p->mirror_y) {
		fprintf(fd, "\t(cons 'mirror #(#%c #%c))\n",
			p->mirror_x? 't': 'f', p->mirror_y? 't': 'f');
	    }
	}
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
	    fprintf(fd, "\t(cons 'attribs (list\n");
	}
	for (i = 0; i < n_attr ; i++) {
	    switch (attr_list[i].type) {
	    case HID_Label:
		  fprintf(fd, "\t\t(list '%s 'Label \"%s\")\n", attr_list[i].name,
			  attr_list[i].default_val.str_value);
		  break;
		  
	      case HID_Integer:
		  fprintf(fd, "\t\t(list '%s 'Integer %d)\n", attr_list[i].name,
			  attr_list[i].default_val.int_value);
		  break;
		  
	      case HID_Real:
		  fprintf(fd, "\t\t(list '%s 'Real %g)\n", attr_list[i].name,
			  attr_list[i].default_val.real_value);
		  break;
		  
	      case HID_String:
		  fprintf(fd, "\t\t(list '%s 'String \"%s\")\n", attr_list[i].name,
			  attr_list[i].default_val.str_value);
		  break;
		  
	      case HID_Boolean:
		  fprintf(fd, "\t\t(list '%s 'Boolean %d)\n", attr_list[i].name,
			  attr_list[i].default_val.int_value);
		  break;
		  
	      case HID_Enum:
		  fprintf(fd, "\t\t(list '%s 'Enum %d)\n", attr_list[i].name,
			  attr_list[i].default_val.int_value);
		  break;

	      case HID_Mixed:
		  dprintf ("HID_Mixed\n");
		  fprintf (stderr, _("%s():  WARNING:  HID_Mixed is not yet supported.\n"),
			   __FUNCTION__);
		  break;

	      case HID_Path:
		  fprintf(fd, "\t\t(list '%s 'Path \"%s\")\n", attr_list[i].name,
			  attr_list[i].default_val.str_value);
		  break;

	      default:
		  fprintf (stderr, _("%s: unknown type of HID attribute (%d)\n"),
			   __FUNCTION__, attr_list[i].type);
		  break;
	      }
	}
	if (n_attr > 0) {
	    fprintf (fd, "\t))\n");
	}

	fprintf(fd, ")\n");
	p = p->next;
    }

    fprintf (fd, "(set-render-type! %d)\n", screenRenderInfo.renderType);

    setlocale(LC_NUMERIC, "");	/* Default locale */

    fclose(fd);

    return 0;
} /* write_project */
