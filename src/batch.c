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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GUILE_IN_USE 

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libguile.h>
#include <guile/gh.h> /* To be deprecated */

#include "gerber.h"
#include "drill.h"
#include "gerb_image.h"

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif


static SCM
scm_aperture2scm(enum aperture_t a)
{
    switch (a) {
    case CIRCLE :
	return gh_symbol2scm("circle\0");
    case RECTANGLE :
	return gh_symbol2scm("rectangle\0");
    case OVAL :
	return gh_symbol2scm("oval\0");
    case POLYGON :
	return gh_symbol2scm("polygon\0");
    case MACRO :
	return gh_symbol2scm("macro\0");
    default :
	err(1, "Unhandled aperture type\n");
    }
} /* scm_aperture2scm */


static SCM
scm_aperture_state2scm(enum aperture_state_t aperture_state)
{
    switch (aperture_state) {
    case OFF:
	return gh_symbol2scm("exposure-off\0");
    case ON:
	return gh_symbol2scm("exposure-on\0");
    case FLASH:
	return gh_symbol2scm("exposure-flash\0");
    default :
	err(1, "Unhandled aperture type\n");
    }
} /* scm_aperture_state2scm */


static SCM
scm_interpolation2scm(enum interpolation_t interpolation)
{
    switch (interpolation) {
    case LINEARx1 :
	return gh_symbol2scm("linear-x1");
    case LINEARx10 :
	return gh_symbol2scm("linear-x10");
    case LINEARx01 : 
	return gh_symbol2scm("linear-x01");
    case LINEARx001 : 
	return gh_symbol2scm("linear-x001");
    case CW_CIRCULAR : 
	return gh_symbol2scm("cw-circular");
    case CCW_CIRCULAR : 
	return gh_symbol2scm("ccw-circular");
    case PAREA_START :
	return gh_symbol2scm("polygon-area-start");
    case PAREA_END :
	return gh_symbol2scm("polygon-area-end");
    default :
	return SCM_BOOL_F;
    }
} /* scm_interpolation2scm */


static SCM
scm_omit_zeros2scm(enum omit_zeros_t omit_zeros)
{

    switch (omit_zeros) {
    case LEADING :
	return gh_symbol2scm("leading");
    case TRAILING :
	return gh_symbol2scm("trailing");
    case EXPLICIT :
	return gh_symbol2scm("explicit");
    default:
	return SCM_BOOL_F;
    }
} /* scm_omit_zeros2scm */


static SCM
scm_image2scm(gerbv_image_t *image, char *filename)
{
    gerbv_net_t *net;
    int i,j;
    SCM cirseg   = SCM_EOL;
    SCM netlist  = SCM_EOL;
    SCM aperture = SCM_EOL;
    SCM format   = SCM_EOL;
    SCM info     = SCM_EOL;
    
    /*
     * Convert the netlist 
     */
    for (net = image->netlist->next; net != NULL; net = net->next) {

	if (net->cirseg)
	    cirseg = scm_listify(scm_cons(scm_make_real(net->cirseg->cp_x),
					  scm_make_real(net->cirseg->cp_y)),
				 scm_cons(scm_make_real(net->cirseg->width),
					  scm_make_real(net->cirseg->height)),
				 scm_cons(SCM_MAKINUM(net->cirseg->angle1),
					  SCM_MAKINUM(net->cirseg->angle2)),
				 SCM_UNDEFINED);
	else
	    cirseg = SCM_EOL;

	netlist = scm_cons(scm_listify(scm_cons(scm_make_real(net->start_x), 
						scm_make_real(net->start_y)),
				       scm_cons(scm_make_real(net->stop_x), 
						scm_make_real(net->stop_y)),
				       scm_cons(SCM_MAKINUM(net->aperture), 
						scm_aperture_state2scm(net->aperture_state)),
				       scm_interpolation2scm(net->interpolation),
				       cirseg,
				       SCM_UNDEFINED),
			   netlist);
    }
    
    /*
     * Convert aperture definitions
     */
    for (i = 0; i < APERTURE_MAX; i++) {
	if (image->aperture[i] != NULL) {
	    SCM a = SCM_EOL;
	    for (j = image->aperture[i]->nuf_parameters; j != 0; j--)
		a = scm_cons(scm_make_real(image->aperture[i]->parameter[j - 1]), a);
	    a = scm_cons(scm_aperture2scm(image->aperture[i]->type), a);
	    a = scm_cons(SCM_MAKINUM(i), a);
	    aperture = scm_cons(a, aperture);
	}
    }
    /*
     * Convert format specification
     */
    if (image->format != NULL) {
	format = scm_listify(scm_cons(gh_symbol2scm("omit-zeros\0"),
				      scm_omit_zeros2scm(image->format->omit_zeros)),
			     scm_cons(gh_symbol2scm("coordinate\0"),
				      gh_symbol2scm((image->format->coordinate == ABSOLUTE) ? 
						    "absolute" : "incremental")),
			     scm_cons(gh_symbol2scm("x-integer\0"), 
				      SCM_MAKINUM(image->format->x_int)),
			     scm_cons(gh_symbol2scm("x-decimal\0"), 
				      SCM_MAKINUM(image->format->x_dec)),
			     scm_cons(gh_symbol2scm("y-integer\0"), 
				      SCM_MAKINUM(image->format->y_int)),
			     scm_cons(gh_symbol2scm("y-decimal\0"), 
				      SCM_MAKINUM(image->format->y_dec)),
			     SCM_UNDEFINED);
    }
    
    /*
     * Convert image information 
     */
    if (image->info->polarity == NEGATIVE) 
	info = scm_cons(scm_cons(gh_symbol2scm("polarity"),
				 gh_symbol2scm("negative")),
			info);
    else
	info = scm_cons(scm_cons(gh_symbol2scm("polarity"),
				 gh_symbol2scm("positive")),
			info);
    
    info = scm_cons(scm_cons(gh_symbol2scm("unit"),
			     gh_symbol2scm((image->info->unit == MM) ? "mm" : "inch")),
		    info);
    
    info = scm_cons(scm_cons(gh_symbol2scm("min-x"),
			     scm_make_real(image->info->min_x)),
		    info);
    info = scm_cons(scm_cons(gh_symbol2scm("min-y"),
			     scm_make_real(image->info->min_y)),
		    info);
    info = scm_cons(scm_cons(gh_symbol2scm("max-x"),
			     scm_make_real(image->info->max_x)),
		    info);
    info = scm_cons(scm_cons(gh_symbol2scm("max-y"),
			     scm_make_real(image->info->max_y)),
		    info);
    
    info = scm_cons(scm_cons(gh_symbol2scm("offset-a"),
			     scm_make_real(image->info->offset_a)),
		    info);
    
    info = scm_cons(scm_cons(gh_symbol2scm("offset-b"),
			     scm_make_real(image->info->offset_b)),
		    info);
    
    
    return scm_listify(netlist, aperture, info, format, 
		       scm_makfrom0str(filename), SCM_UNDEFINED);
    
} /* scm_image2scm */


void
batch(char const* backend, char const* filename)
{
    char         *path[3];
    char 	 *complete_path;
    char         *home;
    int		  i;
    gerb_file_t  *fd;
    gerbv_image_t *image;
    gerb_verify_error_t error = GERB_IMAGE_OK;
    SCM	          scm_image;

    if ((home = getenv("HOME")) == NULL)
	err(1, "HOME not set\n");

    /*
     * Define the paths to look for backend in:
     * path[0] = current directory where gerbv was invoked from
     * path[1] = $HOME/.gerbv/scheme
     * path[2] = BACKEND_DIR defined with ./configure --with-backend-dir=XX,
     *           default /usr/local/share/gerbv/scheme
     */
    if ((path[0] = (char *)g_malloc(strlen(".") + 1)) == NULL)
	err(1, "Malloc failed\n");
    strcpy(path[0], ".");

    if ((path[1] = (char *)g_malloc(strlen(home) + strlen("/.gerbv/scheme") + 1)) == NULL)
	err(1, "Malloc failed\n");
    strcpy(path[1], home);
    strcat(path[1], "/.gerbv/scheme");

    if ((path[2] = (char *)g_malloc(strlen(BACKEND_DIR) + 1)) == NULL)
	err(1, "Malloc failed\n");
    strcpy(path[2], BACKEND_DIR);

    /*
     * Search for backend along the paths. Break when you find one.
     */
    for (i = 0; i < sizeof(path)/sizeof(path[0]); i++) {
	complete_path = (char *)g_malloc(strlen(path[i]) + strlen(backend) + 2);
	strcpy(complete_path, path[i]);
	strcat(complete_path, "/");
	strcat(complete_path, backend);
	if (access(complete_path, R_OK) != -1)
	    break;
	g_free(complete_path);
	complete_path = NULL;
    }

    /*
     * Make sure all allocated path strings above are deallocated
     */
    for (i = 0; i < sizeof(path)/sizeof(path[0]); i++) {
	g_free(path[i]);
	path[i] = NULL;
    }

    /*
     * Did we find a backend?
     */
    if (complete_path == NULL) {
	printf("Backend not found\n");
	return;
    } else {
	printf("Backend is [%s]\n", complete_path);
    }

    /*
     * Read and parse Gerberfile
     */
    fd = gerb_fopen(filename);
    if (fd == NULL) {
	fprintf(stderr, "Trying to open %s: ", filename);
	perror("gerbv:batch");
	return;
    }

    if (drill_file_p(fd))
	image = parse_drillfile(fd, NULL, 0, 0);
    else
	image = parse_gerb(fd);
    
    gerb_fclose(fd);

    /*
     * Do error check before continuing
     */
    error = gerbv_image_verify(image);
    if (error) {
	fprintf(stderr, "%s: Parse error: ", filename);
	if (error & GERB_IMAGE_MISSING_NETLIST)
	    fprintf(stderr, "Missing netlist ");
	if (error & GERB_IMAGE_MISSING_FORMAT)
	    fprintf(stderr, "Missing format ");
	if (error & GERB_IMAGE_MISSING_APERTURES) 
	    fprintf(stderr, "Missing apertures/drill sizes ");
	if (error & GERB_IMAGE_MISSING_INFO)
	    fprintf(stderr, "Missing info ");
	fprintf(stderr, "\n");
	exit(1);
    }
   
    /*
     * Convert it to Scheme
     */
    scm_image = scm_image2scm(image, filename);
    
    /*
     * Call external Scheme function in found backend
     */
    scm_primitive_load(scm_makfrom0str(complete_path));
    scm_apply(scm_eval(gh_symbol2scm("main")), scm_image, SCM_EOL);
    
    /*
     * Cleanup
     */
    g_free(complete_path);
    complete_path = NULL;
    
    free_gerb_image(image);
    
    return;
} /* batch */

#endif /* GUILE_IN_USE */
