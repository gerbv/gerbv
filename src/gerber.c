/*
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
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

#include <stdlib.h>
#include <string.h>
#include <math.h>  /* pow() */
#include <glib.h>

#include "gerber.h"
#include "gerb_error.h"

#define NOT_IMPL(fd, s) do { \
                             GERB_MESSAGE("Not Implemented:%s\n", s); \
                             while (gerb_fgetc(fd) != (int)'*'); \
                           } while(0)
	

#define A2I(a,b) (((a & 0xff) << 8) + (b & 0xff))

typedef struct gerb_state {
    enum unit_t unit;
    int curr_x;
    int curr_y;
    int prev_x;
    int prev_y;
    int delta_cp_x;
    int delta_cp_y;
    int curr_aperture;
    int changed;
    enum polarity_t layer_polarity;
    enum aperture_state_t aperture_state;
    enum interpolation_t interpolation;
    enum interpolation_t prev_interpolation;
    gerb_net_t *parea_start_node;
    char *curr_layername;
    int in_parea_fill;
    int mq_on;
} gerb_state_t;


/* Local function prototypes */
static void parse_G_code(gerb_file_t *fd, gerb_state_t *state, 
			 gerb_format_t *format);
static void parse_D_code(gerb_file_t *fd, gerb_state_t *state);
static int parse_M_code(gerb_file_t *fd);
static void parse_rs274x(gerb_file_t *fd, gerb_image_t *image, 
			 gerb_state_t *state);
static int parse_aperture_definition(gerb_file_t *fd, 
				     gerb_aperture_t *aperture,
				     amacro_t *amacro);
static void calc_cirseg_sq(struct gerb_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);
static void calc_cirseg_mq(struct gerb_net *net, int cw, 
			   double delta_cp_x, double delta_cp_y);


gerb_image_t *
parse_gerb(gerb_file_t *fd)
{
    gerb_state_t *state = NULL;
    gerb_image_t *image = NULL;
    gerb_net_t *curr_net = NULL;
    int read;
    double x_scale = 0.0, y_scale = 0.0;
    double delta_cp_x = 0.0, delta_cp_y = 0.0;
    double aperture_size;
    double scale;
    
    state = (gerb_state_t *)malloc(sizeof(gerb_state_t));
    if (state == NULL)
	GERB_FATAL_ERROR("malloc state failed\n");

    /*
     * Set some defaults
     */
    memset((void *)state, 0, sizeof(gerb_state_t));
    state->layer_polarity = DARK;
    /* 
     * "Inches are assumed if units are not specified"
     * rs274xrevd_e.pdf, p. 39
     */
    state->unit = INCH;

    /* 
     * Create new image
     */
    image = new_gerb_image(image);
    if (image == NULL)
	GERB_FATAL_ERROR("malloc image failed\n");
    curr_net = image->netlist;

    /*
     * Start parsing
     */
    while ((read = gerb_fgetc(fd)) != EOF) {
	switch ((char)(read & 0xff)) {
	case 'G':
	    parse_G_code(fd, state, image->format);
	    break;
	case 'D':
	    parse_D_code(fd, state);
	    break;
	case 'M':
	    switch(parse_M_code(fd)) {
	    case 1 :
	    case 2 :
	    case 3 :
		free(state);
		return image;
		break;
	    default:
		GERB_FATAL_ERROR("Strange M code found.\n");
	    }
	    break;
	case 'X':
	    if (image->format->coordinate==INCREMENTAL)
	        state->curr_x += gerb_fgetint(fd);
	    else
	        state->curr_x = gerb_fgetint(fd);
	    state->changed = 1;
	    break;
	case 'Y':
	    if (image->format->coordinate==INCREMENTAL)
	        state->curr_y += gerb_fgetint(fd);
	    else
	        state->curr_y = gerb_fgetint(fd);
	    state->changed = 1;
	    break;
	case 'I':
	    state->delta_cp_x = gerb_fgetint(fd);
	    state->changed = 1;
	    break;
	case 'J':
	    state->delta_cp_y = gerb_fgetint(fd);
	    state->changed = 1;
	    break;
	case '%':
	    parse_rs274x(fd, image, state);
	    while (gerb_fgetc(fd) != '%');
	    break;
	case '*':
	    if (state->changed == 0) break;
	    state->changed = 0;

	    curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
	    curr_net = curr_net->next;
	    memset((void *)curr_net, 0, sizeof(gerb_net_t));

	    /*
	     * Scale to given coordinate format
	     * XXX only "omit leading zeros".
	     */
	    if (image && image->format ){
		x_scale = pow(10.0, (double)image->format->x_dec);
		y_scale = pow(10.0, (double)image->format->y_dec);
	    }
	    
	    curr_net->start_x = (double)state->prev_x / x_scale;
	    curr_net->start_y = (double)state->prev_y / y_scale;
	    curr_net->stop_x = (double)state->curr_x / x_scale;
	    curr_net->stop_y = (double)state->curr_y / y_scale;
	    delta_cp_x = (double)state->delta_cp_x / x_scale;
	    delta_cp_y = (double)state->delta_cp_y / y_scale;


	    switch (state->interpolation) {
	    case CW_CIRCULAR :
		curr_net->cirseg = (gerb_cirseg_t *)malloc(sizeof(gerb_cirseg_t));
		memset((void *)curr_net->cirseg, 0, sizeof(gerb_cirseg_t));
		if (state->mq_on)
		    calc_cirseg_mq(curr_net, 1, delta_cp_x, delta_cp_y);
		else
		    calc_cirseg_sq(curr_net, 1, delta_cp_x, delta_cp_y);
		break;
	    case CCW_CIRCULAR :
		curr_net->cirseg = (gerb_cirseg_t *)malloc(sizeof(gerb_cirseg_t));
		memset((void *)curr_net->cirseg, 0, sizeof(gerb_cirseg_t));
		if (state->mq_on)
		    calc_cirseg_mq(curr_net, 0, delta_cp_x, delta_cp_y);
		else
		    calc_cirseg_sq(curr_net, 0, delta_cp_x, delta_cp_y);
		break;
	    case PAREA_START :
		/* 
		 * To be able to get back and fill in number of polygon corners
		 */
		state->parea_start_node = curr_net;
		state->in_parea_fill = 1;
		break;
	    case PAREA_END :
		state->parea_start_node = NULL;
		state->in_parea_fill = 0;
		break;
	    default :
		break;
	    }

	    /* 
	     * Count number of points in Polygon Area 
	     */
	    if (state->in_parea_fill && state->parea_start_node) 
		state->parea_start_node->nuf_pcorners++;

	    curr_net->interpolation = state->interpolation;

	    /*
	     * If we detected the end of Polygon Area Fill we go back to
	     * the interpolation we had before that.
	     * Also if we detected any of the quadrant flags, since some
	     * gerbers don't reset the interpolation (EagleCad again).
	     */
	    if ((state->interpolation == PAREA_START) ||
		(state->interpolation == PAREA_END) ||
		(state->interpolation == MQ_START) ||
		(state->interpolation == MQ_END))
		state->interpolation = state->prev_interpolation;

	    /*
	     * Save layer polarity and unit
	     */
	    curr_net->layer_polarity = state->layer_polarity;
	    curr_net->unit = state->unit;	    

	    state->delta_cp_x = 0.0;
	    state->delta_cp_y = 0.0;
	    curr_net->aperture = state->curr_aperture;
	    curr_net->aperture_state = state->aperture_state;

	    /* 
	     * Make sure we don't hit any undefined aperture
	     * In macros the first parameter could be basically anything
	     */
	    if ((curr_net->aperture != 0) && 
		(image->aperture[curr_net->aperture] != NULL) &&
		(image->aperture[curr_net->aperture]->type != MACRO))
		aperture_size = image->aperture[curr_net->aperture]->parameter[0];
	    else 
		aperture_size = 0.0;

	    /*
	     * Find min and max of image with compensation for mm.
	     */
	    if (curr_net->unit == MM)
		scale = 25.4;
	    else 
		scale = 1.0;
	    if (image->info->min_x > curr_net->stop_x)
		image->info->min_x = (curr_net->stop_x - aperture_size) / scale;
	    if (image->info->min_y > curr_net->stop_y)
		image->info->min_y = (curr_net->stop_y - aperture_size) / scale;
	    if (image->info->max_x < curr_net->stop_x)
		image->info->max_x = (curr_net->stop_x + aperture_size) / scale;
	    if (image->info->max_y < curr_net->stop_y)
		image->info->max_y = (curr_net->stop_y + aperture_size) / scale;
	    
	    state->prev_x = state->curr_x;
	    state->prev_y = state->curr_y;
	    
	    break;
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	    break;
	default:
	    GERB_COMPILE_ERROR("Found unknown character (whitespace?) %c[%d]\n", read, read);
	}
    }
    
    GERB_COMPILE_ERROR("File is missing gerber End-Of-File\n");
    
    return image;
} /* parse_gerb */


static void 
parse_G_code(gerb_file_t *fd, gerb_state_t *state, gerb_format_t *format)
{
    int op[2];
    int op_int;
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	GERB_COMPILE_ERROR("Unexpected EOF found.\n");

    if ((op[0] < (int)'0') || (op[0] > (int)'9') || 
	(op[1] < (int)'0') || (op[1] > (int)'9'))
	GERB_COMPILE_ERROR("Non numerical G opcode found [%c%c]\n", op[0], op[1]);

    op_int = (op[0] - (int)'0');
    op_int = op_int * 10 + (op[1] - (int)'0');

    switch(op_int) {
    case 0:  /* Move */
	/* Is this doing anything really? */
	break;
    case 1:  /* Linear Interpolation (1X scale) */
	state->interpolation = LINEARx1;
	break;
    case 2:  /* Clockwise Linear Interpolation */
	state->interpolation = CW_CIRCULAR;
	break;
    case 3:  /* Counter Clockwise Linear Interpolation */
	state->interpolation = CCW_CIRCULAR;
	break;
    case 4:  /* Ignore Data Block */
	/* Don't do anything, just read 'til * */
	while (gerb_fgetc(fd) != '*');
	break;
    case 10: /* Linear Interpolation (10X scale) */
	state->interpolation = LINEARx10;
	break;
    case 11: /* Linear Interpolation (0.1X scale) */
	state->interpolation = LINEARx01;
	break;
    case 12: /* Linear Interpolation (0.01X scale) */
	state->interpolation = LINEARx001;
	break;
    case 36: /* Turn on Polygon Area Fill */
	state->prev_interpolation = state->interpolation;
	state->interpolation = PAREA_START;
	state->changed = 1;
	break;
    case 37: /* Turn off Polygon Area Fill */
	state->interpolation = PAREA_END;
	state->changed = 1;
	break;
    case 54: /* Tool prepare */
	/* XXX Maybe uneccesary??? */
	if (gerb_fgetc(fd) == 'D')
	    state->curr_aperture = gerb_fgetint(fd);
	else
	    GERB_COMPILE_WARNING("Strange code after G54\n");
	break;
    case 55: /* Prepare for flash */
	break;
    case 70: /* Specify inches */
	state->unit = INCH;
	break;
    case 71: /* Specify millimeters */
	state->unit = MM;
	break;
    case 74: /* Disable 360 circular interpolation */
	state->prev_interpolation = state->interpolation;
	state->interpolation = MQ_END;
	state->mq_on = 0;
	break;
    case 75: /* Enable 360 circular interpolation */
	state->prev_interpolation = state->interpolation;
	state->interpolation = MQ_START;
	state->mq_on = 1;
	break;
    case 90: /* Specify absolut format */
	if (format) format->coordinate = ABSOLUTE;
	break;
    case 91: /* Specify incremental format */
	if (format) format->coordinate = INCREMENTAL;
	break;
    default:
	GERB_COMPILE_ERROR("Strange/unhandled G code : %c%c\n", op[0], op[1]);
    }
    
    return;
} /* parse_G_code */


static void 
parse_D_code(gerb_file_t *fd, gerb_state_t *state)
{
    int a;
    
    a = gerb_fgetint(fd);
    switch(a) {
    case 1 : /* Exposure on */
	state->aperture_state = ON;
	state->changed = 1;
	break;
    case 2 : /* Exposure off */
	state->aperture_state = OFF;
	state->changed = 1;
	break;
    case 3 : /* Flash aperture */
	state->aperture_state = FLASH;
	state->changed = 1;
	break;
    default: /* Aperture in use */
	if ((a >= APERTURE_MIN) && (a <= APERTURE_MAX)) 
	    state->curr_aperture = a;
	else
	    GERB_COMPILE_ERROR("Aperture out of bounds:%d\n", a);
	state->changed = 0;
    }
    
    return;
} /* parse_D_code */


static int
parse_M_code(gerb_file_t *fd)
{
    int op[2];
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	GERB_COMPILE_ERROR("Unexpected EOF found.\n");
    
    if (op[0] != (int)'0')
	GERB_COMPILE_ERROR("Strange M code [%c%c]\n", (char)op[0], (char)op[1]);

    switch (op[1]) {
    case '0':  /* Program stop */
	return 1;
    case '1':  /* Optional stop */
	return 2;
    case '2':  /* End of program */
	return 3;
    default:
	GERB_COMPILE_ERROR("Strange M code [%c%c]\n", (char)op[0], (char)op[1]);
    }
    return 0;
} /* parse_M_code */


static void 
parse_rs274x(gerb_file_t *fd, gerb_image_t *image, gerb_state_t *state)
{
    int op[2];
    char str[3];
    gerb_aperture_t *a = NULL;
    amacro_t *tmp_amacro;
    int ano;
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	GERB_COMPILE_ERROR("Unexpected EOF found.\n");

    switch (A2I(op[0], op[1])){

      /* Directive parameters */
    case A2I('A','S'): /* Axis Select */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    GERB_COMPILE_ERROR("Unexpected EOF found.\n");
	
	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
	    NOT_IMPL(fd, "%MI with reversed axis not supported%");
	    break;
	}

	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);

	if ((op[0] == EOF) || (op[1] == EOF))
	    GERB_COMPILE_ERROR("Unexpected EOF found.\n");

	if (((op[0] == 'A') && (op[1] == 'Y')) ||
	    ((op[0] == 'B') && (op[1] == 'X'))) {
	    NOT_IMPL(fd, "%MI with reversed axis not supported%");
	    break;
	}
	break;
    case A2I('F','S'): /* Format Statement */
	image->format = (gerb_format_t *)malloc(sizeof(gerb_format_t));
	if (image->format == NULL) 
	    GERB_FATAL_ERROR("Failed malloc for format\n");
	memset((void *)image->format, 0, sizeof(gerb_format_t));
	
	switch (gerb_fgetc(fd)) {
	case 'L':
	    image->format->omit_zeros = LEADING;
	    break;
	case 'T':
	    image->format->omit_zeros = TRAILING;
	    break;
	case 'D':
	    image->format->omit_zeros = EXPLICIT;
	    break;
	default:
	    GERB_MESSAGE("EagleCad bug detected: Defaults to omit leading zeroes\n");
	    gerb_ungetc(fd);
	    image->format->omit_zeros = LEADING;
	}
	
	switch (gerb_fgetc(fd)) {
	case 'A':
	    image->format->coordinate = ABSOLUTE;
	    break;
	case 'I':
	    image->format->coordinate = INCREMENTAL;
	    break;
	default:
	    GERB_COMPILE_ERROR("Format error: coordinate = %c\n", op[0]);
	}

	while((op[0] = gerb_fgetc(fd)) != '*') {
	    switch (op[0]) {
	    case 'N':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_seqno = op[0] - '0';
		break;
	    case 'G':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_gf = op[0] - '0';
		break;
	    case 'D':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_pf = op[0] - '0';
		break;
	    case 'M':
		op[0] = (char)gerb_fgetc(fd);
		image->format->lim_mf = op[0] - '0';
		break;
	    case 'X' :
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    GERB_COMPILE_ERROR("Illegal format size : %c\n", (char)op[0]);
		image->format->x_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    GERB_COMPILE_ERROR("Illegal format size : %c\n", (char)op[0]);
		image->format->x_dec = op[0] - '0';
		break;
	    case 'Y':
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    GERB_COMPILE_ERROR("Illegal format size : %c\n", (char)op[0]);
		image->format->y_int = op[0] - '0';
		op[0] = gerb_fgetc(fd);
		if ((op[0] < '0') || (op[0] > '6'))
		    GERB_COMPILE_ERROR("Illegal format size : %c\n", (char)op[0]);
		image->format->y_dec = op[0] - '0';
		break;
	    default :
		GERB_COMPILE_ERROR("Not handled  type of format statement [%c]\n", op[0]);
	    }
	}
	break;
    case A2I('M','I'): /* Mirror Image */
	NOT_IMPL(fd, "%MI%");
	break;
    case A2I('M','O'): /* Mode of Units */

	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    GERB_COMPILE_ERROR("Unexpected EOF found.\n");

	switch (A2I(op[0],op[1])) {
	case A2I('I','N'):
	    state->unit = INCH;
	    break;
	case A2I('M','M'):
	    state->unit = MM;
	    break;
	default:
	    GERB_COMPILE_ERROR("Illegal unit:%c%c\n", op[0], op[1]);
	}
	break;
    case A2I('O','F'): /* Offset */
	op[0] = gerb_fgetc(fd);
	while (op[0] != '*') {
	    switch (op[0]) {
	    case 'A' :
		image->info->offset_a = gerb_fgetdouble(fd);
		break;
	    case 'B' :
		image->info->offset_b = gerb_fgetdouble(fd);
		break;
	    default :
		GERB_COMPILE_ERROR("Wrong character in offset:%c\n", op[0]);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	return;
    case A2I('S','F'): /* Scale Factor */
	if (gerb_fgetc(fd) == 'A')
	    image->info->scale_factor_A = gerb_fgetdouble(fd);
	else 
	    gerb_ungetc(fd);
	if (gerb_fgetc(fd) == 'B')
	    image->info->scale_factor_B = gerb_fgetdouble(fd);
	else 
	    gerb_ungetc(fd);
	if ((fabs(image->info->scale_factor_A - 1.0) > 0.00001) ||
	    (fabs(image->info->scale_factor_B - 1.0) > 0.00001))
	    NOT_IMPL(fd, "%SF% != 1.0");
	break;
    case A2I('I','C'): /* Input Code */
	/* Thanks to Stephen Adam for providing this information. As he writes:
	 *      btw, here's a logic puzzle for you.  If you need to
	 * read the gerber file to see how it's encoded, then
	 * how can you read it?
	 */
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    GERB_COMPILE_ERROR("Unexpected EOF found.\n");

	switch (A2I(op[0],op[1])) {
	case A2I('A','S'):
	    image->info->encoding = ASCII;
	    break;
	case A2I('E','B'):
	    image->info->encoding = EBCDIC;
	    break;
	case A2I('B','C'):
	    image->info->encoding = BCD;
	    break;
	case A2I('I','S'):
	    image->info->encoding = ISO_ASCII;
	    break;
	case A2I('E','I'):
	    image->info->encoding = EIA;
	    break;
	default:
	    GERB_COMPILE_ERROR("Strange inputcode : %c%c\n", op[0], op[1]);
	}
	break;

	/* Image parameters */
    case A2I('I','J'): /* Image Justify */
	NOT_IMPL(fd, "%IJ%");
	break;
    case A2I('I','N'): /* Image Name */
	image->info->name = gerb_fgetstring(fd, '*');
	break;
    case A2I('I','O'): /* Image Offset */
	NOT_IMPL(fd, "%IO%");
	break;
    case A2I('I','P'): /* Image Polarity */
	
	for (ano = 0; ano < 3; ano++) {
	    op[0] = gerb_fgetc(fd);
	    if (op[0] == EOF)
		GERB_COMPILE_ERROR("Unexpected EOF found.\n");
	    str[ano] = (char)op[0];
	}
	
	if (strncmp(str, "POS", 3) == 0) 
	    image->info->polarity = POSITIVE;
	else if (strncmp(str, "NEG", 3) == 0)
	    image->info->polarity = NEGATIVE;
	else 
	    GERB_COMPILE_ERROR("Strange polarity : %c%c%c\n", str[0], str[1], str[2]);
	
	break;
    case A2I('I','R'): /* Image Rotation */
	NOT_IMPL(fd, "%IR%");
	break;
    case A2I('P','F'): /* Plotter Film */
	NOT_IMPL(fd, "%PF%");
	break;
	
	/* Aperture parameters */
    case A2I('A','D'): /* Aperture Description */
	a = (gerb_aperture_t *)malloc(sizeof(gerb_aperture_t));
	memset((void *)a, 0, sizeof(gerb_aperture_t));
	ano = parse_aperture_definition(fd, a, image->amacro);
	if ((ano >= APERTURE_MIN) && (ano <= APERTURE_MAX)) {
	    a->unit = state->unit;
	    image->aperture[ano] = a;
	} else
	    GERB_COMPILE_ERROR("Aperture number out of bounds : %d\n", ano);
	break;
    case A2I('A','M'): /* Aperture Macro */
	tmp_amacro = image->amacro;
	image->amacro = parse_aperture_macro(fd);
	image->amacro->next = tmp_amacro;
#ifdef AMACRO_DEBUG
	print_program(image->amacro);
#endif
	break;

	/* Layer */
    case A2I('L','N'): /* Layer Name */
	state->curr_layername = gerb_fgetstring(fd, '*');
	break;
    case A2I('L','P'): /* Layer Polarity */
	switch (gerb_fgetc(fd)) {
	case 'D': /* Dark Polarity (default) */
	    state->layer_polarity = DARK;
	    break;
	case 'C': /* Clear Polarity */
	    state->layer_polarity = CLEAR;
	    break;
	default:
	    GERB_COMPILE_WARNING("Strange Layer Polarity: %c\n", op[0]);
	}
	break;
    case A2I('K','O'): /* Knock Out */
	NOT_IMPL(fd, "%KO%");
	break;
    case A2I('S','R'): /* Step and Repeat */
	op[0] = gerb_fgetc(fd);
	if (op[0] == '*') { /* Disable previous SR parameters */
	    image->info->step_and_repeat.X = 1;
	    image->info->step_and_repeat.Y = 1;
	    image->info->step_and_repeat.dist_X = 0.0;
	    image->info->step_and_repeat.dist_Y = 0.0;
	    break;
	}
	while (op[0] != '*') { 
	    switch (op[0]) {
	    case 'X':
		image->info->step_and_repeat.X = gerb_fgetint(fd);
		break;
	    case 'Y':
		image->info->step_and_repeat.Y = gerb_fgetint(fd);
		break;
	    case 'I':
		image->info->step_and_repeat.dist_X = gerb_fgetdouble(fd);
		break;
	    case 'J':
		image->info->step_and_repeat.dist_Y = gerb_fgetdouble(fd);
		break;
	    default:
		GERB_COMPILE_ERROR("Step-and-repeat parameter error\n");
	    }
	    op[0] = gerb_fgetc(fd);
	}
	if ((image->info->step_and_repeat.X != 1) || 
	    (image->info->step_and_repeat.Y != 1) ||
	    (fabs(image->info->step_and_repeat.dist_X) > 0.000001) ||
	    (fabs(image->info->step_and_repeat.dist_Y) > 0.000001))
	    NOT_IMPL(fd, "%SR%");
	break;	    
    case A2I('R','O'): /* Rotate */
	NOT_IMPL(fd, "%RO%");
	break;
    default:
	GERB_COMPILE_ERROR("Unknown extension found %%%c%c%%\n", op[0], op[1]);
    }
    
    return;
} /* parse_rs274x */


static int 
parse_aperture_definition(gerb_file_t *fd, gerb_aperture_t *aperture,
			  amacro_t *amacro)
{
    int ano, i;
    char *ad;
    char *token;
    amacro_t *curr_amacro;
    
    if (gerb_fgetc(fd) != 'D')
	return -1;
    
    /*
     * Get aperture no
     */
    ano = gerb_fgetint(fd);

    /*
     * Read in the whole aperture defintion and tokenize it
     */
    ad = gerb_fgetstring(fd, '*');
    token = strtok(ad, ",");

    if (strlen(token) == 1) {
	switch (token[0]) {
	case 'C':
	    aperture->type = CIRCLE;
	    break;
	case 'R' :
	    aperture->type = RECTANGLE;
	    break;
	case 'O' :
	    aperture->type = OVAL;
	    break;
	case 'P' :
	    aperture->type = POLYGON;
	    break;
	}
	/* Here a should a T be defined, but I don't know what it represents */
    } else {
	aperture->type = MACRO;
	/*
	 * In aperture definition, point to the aperture macro 
	 * used in the defintion
	 */
	curr_amacro = amacro;
	while (curr_amacro) {
	    if ((strlen(curr_amacro->name) == strlen(token)) &&
		(strcmp(curr_amacro->name, token) == 0)) {
		aperture->amacro = curr_amacro;
		break;
	    }
	    curr_amacro = curr_amacro->next;
	}
    }

    /*
     * Parse all parameters
     */
    for (token = strtok(NULL, "X"), i = 0; token != NULL; 
	 token = strtok(NULL, "X"), i++)
	aperture->parameter[i] = strtod(token, NULL);
	    
    aperture->nuf_parameters = i;

    gerb_ungetc(fd);

    free(ad);

    return ano;
} /* parse_aperture_definition */


static void 
calc_cirseg_sq(struct gerb_net *net, int cw, 
	       double delta_cp_x, double delta_cp_y)
{
    double d1x, d1y, d2x, d2y;
    double alfa, beta;
    int quadrant = 0;

    
    /*
     * Quadrant detection (based on ccw, coverted below if cw)
     *  Y ^
     *   /!\
     *    !
     *    ---->X
     */
    if (net->start_x > net->stop_x)
	/* 1st and 2nd quadrant */
	if (net->start_y < net->stop_y)
	    quadrant = 1;
	else
	    quadrant = 2;
    else
	/* 3rd and 4th quadrant */
	if (net->start_y > net->stop_y)
	    quadrant = 3;
	else
	    quadrant = 4;

    /* 
     * If clockwise, rotate quadrant
     */
    if (cw) {
	switch (quadrant) {
	case 1 : 
	    quadrant = 3;
	    break;
	case 2 : 
	    quadrant = 4;
	    break;
	case 3 : 
	    quadrant = 1;
	    break;
	case 4 : 
	    quadrant = 2;
	    break;
	default : 
	    GERB_COMPILE_ERROR("Unknow quadrant value while converting to cw\n");
	}
    }

    /*
     * Calculate arc center point
     */
    switch (quadrant) {
    case 1 :
	net->cirseg->cp_x = net->start_x - delta_cp_x;
	net->cirseg->cp_y = net->start_y - delta_cp_y;
	break;
    case 2 :
	net->cirseg->cp_x = net->start_x + delta_cp_x;
	net->cirseg->cp_y = net->start_y - delta_cp_y;
	break;
    case 3 : 
	net->cirseg->cp_x = net->start_x + delta_cp_x;
	net->cirseg->cp_y = net->start_y + delta_cp_y;
	break;
    case 4 :
	net->cirseg->cp_x = net->start_x - delta_cp_x;
	net->cirseg->cp_y = net->start_y + delta_cp_y;
	break;
    default :
	GERB_COMPILE_ERROR("Strange quadrant : %d\n", quadrant);
    }

    /*
     * Some good values 
     */
    d1x = fabs(net->start_x - net->cirseg->cp_x);
    d1y = fabs(net->start_y - net->cirseg->cp_y);
    d2x = fabs(net->stop_x - net->cirseg->cp_x);
    d2y = fabs(net->stop_y - net->cirseg->cp_y);

    alfa = atan2(d1y, d1x);
    beta = atan2(d2y, d2x);

    /*
     * Avoid divide by zero when sin(0) = 0 and cos(90) = 0
     */
    net->cirseg->width = alfa < beta ? 
	2 * (d1x / cos(alfa)) : 2 * (d2x / cos(beta));
    net->cirseg->height = alfa > beta ? 
	2 * (d1y / sin(alfa)) : 2 * (d2y / sin(beta));

    if (alfa < 0.000001 && beta < 0.000001) {
	net->cirseg->height = 0;
    }

#define RAD2DEG(a) (a * 180 / M_PI) 
    
    switch (quadrant) {
    case 1 :
	net->cirseg->angle1 = RAD2DEG(alfa);
	net->cirseg->angle2 = RAD2DEG(beta);
	break;
    case 2 :
	net->cirseg->angle1 = 180.0 - RAD2DEG(alfa);
	net->cirseg->angle2 = 180.0 - RAD2DEG(beta);
	break;
    case 3 : 
	net->cirseg->angle1 = 180.0 + RAD2DEG(alfa);
	net->cirseg->angle2 = 180.0 + RAD2DEG(beta);
	break;
    case 4 :
	net->cirseg->angle1 = 360.0 - RAD2DEG(alfa);
	net->cirseg->angle2 = 360.0 - RAD2DEG(beta);
	break;
    default :
	GERB_COMPILE_ERROR("Strange quadrant : %d\n", quadrant);
    }

    if (net->cirseg->width < 0.0)
	GERB_COMPILE_WARNING("Negative width [%f] in quadrant %d [%f][%f]\n", 
			     net->cirseg->width, quadrant, alfa, beta);
    
    if (net->cirseg->height < 0.0)
	GERB_COMPILE_WARNING("Negative height [%f] in quadrant %d [%f][%f]\n", 
	     net->cirseg->height, quadrant, RAD2DEG(alfa), RAD2DEG(beta));

    return;

} /* calc_cirseg_sq */


static void 
calc_cirseg_mq(struct gerb_net *net, int cw, 
	       double delta_cp_x, double delta_cp_y)
{
    double d1x, d1y, d2x, d2y;
    double alfa, beta;

    net->cirseg->cp_x = net->start_x + delta_cp_x;
    net->cirseg->cp_y = net->start_y + delta_cp_y;

    /*
     * Some good values 
     */
    d1x = net->start_x - net->cirseg->cp_x;
    d1y = net->start_y - net->cirseg->cp_y;
    d2x = net->stop_x - net->cirseg->cp_x;
    d2y = net->stop_y - net->cirseg->cp_y;
    
    alfa = atan2(d1y, d1x);
    beta = atan2(d2y, d2x);

    net->cirseg->width = sqrt(delta_cp_x*delta_cp_x + delta_cp_y*delta_cp_y);
    net->cirseg->width *= 2.0;
    net->cirseg->height = net->cirseg->width;

    net->cirseg->angle1 = RAD2DEG(alfa);
    net->cirseg->angle2 = RAD2DEG(beta);

    /*
     * Make sure it's always positive angles
     */
    if (net->cirseg->angle1 < 0) {
	net->cirseg->angle1 = 360.0 + net->cirseg->angle1;
	net->cirseg->angle2 = 360.0 + net->cirseg->angle2;
    }

    if (net->cirseg->angle2 < 0.0) 
	net->cirseg->angle2 = 360.0 + net->cirseg->angle2;

    if(fabs(net->cirseg->angle2) < 0.0001) 
	net->cirseg->angle2 = 360.0;

    /*
     * If ccw we must make sure angle2-angle1 are always positive
     * We should really return one angle and the difference as GTK
     * uses them. But what the heck, it works for me.
     */
    if ((net->cirseg->angle1 > net->cirseg->angle2) && (cw == 0))
	net->cirseg->angle2 = 360.0 + net->cirseg->angle2;
    
    /*
     * If angles are equal it should be a circle, but as gerbv
     * currently is designed it becomes a point. Maybe I should
     * save start angle and how many degrees to draw?
     */
    if (fabs(net->cirseg->angle2 - net->cirseg->angle1) < 0.00001)
	net->cirseg->angle2 = 360.0 + net->cirseg->angle2;

    return;
} /* calc_cirseg_mq */
