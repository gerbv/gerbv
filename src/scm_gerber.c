/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 * Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#ifndef NO_GUILE

#include <libguile.h>
#include <guile/gh.h> /* To be deprecated */

#include "gerber.h"

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
}


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
}


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
    case MQ_CW_CIRCULAR : 
	return gh_symbol2scm("mq-cw-circular");
    case MQ_CCW_CIRCULAR :
	return gh_symbol2scm("mq-ccw-circular");
    default :
	return SCM_BOOL_F;
    }
}


SCM
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
}


SCM
scm_image2scm(gerb_image_t *image, char *filename)
{
    gerb_net_t *net;
    int i,j;
    SCM netlist  = SCM_EOL;
    SCM aperture = SCM_EOL;
    SCM format   = SCM_EOL;
    SCM info     = SCM_EOL;
    
    /*
     * Convert the netlist 
     */
    for (net = image->netlist->next; net->next != NULL; net = net->next) {
	netlist = scm_cons(scm_listify(scm_cons(scm_make_real(net->start_x), 
						scm_make_real(net->start_y)),
				       scm_cons(scm_make_real(net->stop_x), 
						scm_make_real(net->stop_y)),
				       scm_cons(scm_make_real(net->arc_start_x), 
						scm_make_real(net->arc_start_y)),
				       scm_cons(SCM_MAKINUM(net->aperture), 
						scm_aperture_state2scm(net->aperture_state)),
				       scm_interpolation2scm(net->interpolation),
				       SCM_UNDEFINED),
			   netlist);
    }
    
    /*
     * Convert aperture definitions
     */
    for (i = 0; i < APERTURE_MAX - APERTURE_MIN; i++) {
	if (image->aperture[i] != NULL) {
	    SCM a = SCM_EOL;
	    for (j = image->aperture[i]->nuf_parameters; j != 0; j--)
		a = scm_cons(scm_make_real(image->aperture[i]->parameter[j - 1]), a);
	    a = scm_cons(scm_aperture2scm(image->aperture[i]->type), a);
	    a = scm_cons(SCM_MAKINUM(i + APERTURE_MIN), a);
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

#endif /* NO_GUILE */
