/*
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#include "gerber.h"


#define NOT_IMPL(fd, s) do { \
                             fprintf(stderr, "Not Implemented:%s\n", s); \
                             while (gerb_fgetc(fd) != '*'); \
                           } while(0)
	

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif


typedef struct gerb_state {
    int curr_x;
    int curr_y;
    int prev_x;
    int prev_y;
    int delta_cp_x;
    int delta_cp_y;
    int curr_aperture;
    int changed;
    enum aperture_state_t aperture_state;
    enum interpolation_t interpolation;
    enum interpolation_t prev_interpolation;
} gerb_state_t;


/* Local function prototypes */
static void parse_G_code(gerb_file_t *fd, gerb_state_t *state, 
			 gerb_format_t *format);
static void parse_D_code(gerb_file_t *fd, gerb_state_t *state);
static int parse_M_code(gerb_file_t *fd);
static void parse_rs274x(gerb_file_t *fd, gerb_image_t *image);
static int parse_aperture_definition(gerb_file_t *fd, 
				     gerb_aperture_t *aperture);
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
    char read;
    double x_scale, y_scale;
    double delta_cp_x, delta_cp_y;
    int error = 0;
    
    state = (gerb_state_t *)malloc(sizeof(gerb_state_t));
    if (state == NULL)
	err(1, "malloc state failed\n");
    bzero((void *)state, sizeof(gerb_state_t));

    image = new_gerb_image(image);
    if (image == NULL)
	err(1, "malloc image failed\n");
    curr_net = image->netlist;

    while ((read = gerb_fgetc(fd)) != EOF) {
	switch (read) {
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
		/*
		 * Do error check before returning image
		 */
		error = check_gerb(image);
		if (error) {
		    fprintf(stderr, "Parse error : ");
		    if (error & 1)
			fprintf(stderr, "Missing netlist ");
		    if (error & 2)
			fprintf(stderr, "Missing format ");
		    if (error & 4) 
			fprintf(stderr, "Missing apertures ");
		    if (error & 8)
			fprintf(stderr, "Missing info ");
		    fprintf(stderr, "\n");
		}
		return image;
		break;
	    default:
		err(1, "Strange M code found.\n");
	    }
	    break;
	case 'X':
	    state->curr_x = gerb_fgetint(fd);
	    state->changed = 1;
	    break;
	case 'Y':
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
	    parse_rs274x(fd, image);
	    while (gerb_fgetc(fd) != '%');
	    break;
	case '*':
	    if (state->changed == 0) break;
	    state->changed = 0;

	    if ( (state->curr_aperture == 0) &&
		 (state->interpolation != PAREA_START) &&
		 (state->interpolation != PAREA_FILL) &&
		 (state->interpolation != PAREA_END) ) break;

	    curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
	    curr_net = curr_net->next;
	    bzero((void *)curr_net, sizeof(gerb_net_t));
	    
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
		bzero((void *)curr_net->cirseg, sizeof(gerb_cirseg_t));
		calc_cirseg_sq(curr_net, 1, delta_cp_x, delta_cp_y);
		break;
	    case CCW_CIRCULAR :
		curr_net->cirseg = (gerb_cirseg_t *)malloc(sizeof(gerb_cirseg_t));
		bzero((void *)curr_net->cirseg, sizeof(gerb_cirseg_t));
		calc_cirseg_sq(curr_net, 0, delta_cp_x, delta_cp_y);
		break;
	    case MQ_CW_CIRCULAR :
		curr_net->cirseg = (gerb_cirseg_t *)malloc(sizeof(gerb_cirseg_t));
		bzero((void *)curr_net->cirseg, sizeof(gerb_cirseg_t));
		calc_cirseg_mq(curr_net, 1, delta_cp_x, delta_cp_y);
		break;
	    case MQ_CCW_CIRCULAR:
		curr_net->cirseg = (gerb_cirseg_t *)malloc(sizeof(gerb_cirseg_t));
		bzero((void *)curr_net->cirseg, sizeof(gerb_cirseg_t));
		calc_cirseg_mq(curr_net, 0, delta_cp_x, delta_cp_y);
		break;
	    default :
	    }

#ifdef EAGLECAD_KLUDGE
	    if ( (state->delta_cp_x == 0.0) && (state->delta_cp_y == 0.0) &&
		 (state->interpolation != PAREA_START) &&
		 (state->interpolation != PAREA_FILL) &&
		 (state->interpolation != PAREA_END)  )
		curr_net->interpolation = LINEARx1;
	    else
		curr_net->interpolation = state->interpolation;
#else
	    curr_net->interpolation = state->interpolation;
#endif
	    switch (state->interpolation) {
	    case PAREA_START :
		state->interpolation = PAREA_FILL;
		state->changed = 1;
		break;
	    case PAREA_END :
		state->interpolation = state->prev_interpolation;
		break;
	    default :
	    }

	    state->delta_cp_x = 0.0;
	    state->delta_cp_y = 0.0;
	    curr_net->aperture = state->curr_aperture;
	    curr_net->aperture_state = state->aperture_state;

	    
	    /*
	     * Find min and max of image
	     */
	    if (image->info->min_x == 0.0 || 
		image->info->min_x > curr_net->stop_x)
		image->info->min_x = curr_net->stop_x;
	    if (image->info->min_y == 0.0 || 
		image->info->min_y > curr_net->stop_y)
		image->info->min_y = curr_net->stop_y;
	    if (image->info->max_x < curr_net->stop_x)
		image->info->max_x = curr_net->stop_x;
	    if (image->info->max_y < curr_net->stop_x)
		image->info->max_y = curr_net->stop_y;
	    
	    state->prev_x = state->curr_x;
	    state->prev_y = state->curr_y;
	    
	    break;
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	    break;
	default:
	    fprintf(stderr, "Found unknown character (whitespace?) %c[%d]\n", read, read);
	}
    }
    
    fprintf(stderr, "File is missing gerber End-Of-File\n");
    
    return image;
} /* parse_gerb */


/*
 * Check that the parsed gerber image is complete.
 * Returned errorcodes are:
 * 0: No problems
 * 1: Missing netlist
 * 2: Missing format
 * 4: Missing apertures
 * 8: Missing info
 * It could be any of above or'ed together
 */
int 
check_gerb(gerb_image_t *image)
{
    int error = 0;
    int i;

    if (image->netlist == NULL) error |= 1;
    if (image->format == NULL)  error |= 2;
    if (image->info == NULL)    error |= 8;

    for (i = 0; i < APERTURE_MAX && image->aperture[i] == NULL; i++);
    if (i == APERTURE_MAX) error |= 4;
    
    return error;
}

static void 
parse_G_code(gerb_file_t *fd, gerb_state_t *state, gerb_format_t *format)
{
    char op[2];
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");
    
    if (strncmp(op, "00", 2) == 0) { 	/* Move */
	/* Is this doing anything really? */
    } else if (strncmp(op, "01", 2) == 0) { /* Linear Interpolation (1X scale) */
	state->interpolation = LINEARx1;
	return;
    } else if (strncmp(op, "02", 2) == 0) { /* Clockwise Linear Interpolation */
	if (state->interpolation == MQ_CW_CIRCULAR ||
	    state->interpolation == MQ_CCW_CIRCULAR)
	    state->interpolation = MQ_CW_CIRCULAR;
	else 
	    state->interpolation = CW_CIRCULAR;
	return;
    } else if (strncmp(op, "03", 2) == 0) { /* Counter Clockwise Linear Interpolation */
	if (state->interpolation == MQ_CW_CIRCULAR ||
	    state->interpolation == MQ_CCW_CIRCULAR)
	    state->interpolation = MQ_CCW_CIRCULAR;
	else 
	    state->interpolation = CCW_CIRCULAR;
	return;
    } else if (strncmp(op, "04", 2) == 0) { /* Ignore Data Block */
	/* Don't do anything, just read 'til * */
	while (gerb_fgetc(fd) != '*');
    } else if (strncmp(op, "10", 2) == 0) { /* Linear Interpolation (10X scale) */
	state->interpolation = LINEARx10;
	return;
    } else if (strncmp(op, "11", 2) == 0) { /* Linear Interpolation (0.1X scale) */
	state->interpolation = LINEARx01;
	return;
    } else if (strncmp(op, "12", 2) == 0) { /* Linear Interpolation (0.01X scale) */
	state->interpolation = LINEARx001;
	return;
    } else if (strncmp(op, "36", 2) == 0) { /* Turn on Polygon Area Fill */
	state->prev_interpolation = state->interpolation;
	state->interpolation = PAREA_START;
	state->changed = 1;
    } else if (strncmp(op, "37", 2) == 0) { /* Turn off Polygon Area Fill */
	state->interpolation = PAREA_END;
	state->changed = 1;
    } else if (strncmp(op, "54", 2) == 0) { /* Tool prepare */
	if (gerb_fgetc(fd) == 'D')   /* XXX Check return value */
	    state->curr_aperture = gerb_fgetint(fd);
	else
	    err(1, "Strange code after G54\n");
	
    } else if (strncmp(op, "70", 2) == 0) { /* Specify inches */
	NOT_IMPL(fd, "G70");
    } else if (strncmp(op, "71", 2) == 0) { /* Specify millimeters */
	NOT_IMPL(fd, "G71");
    } else if (strncmp(op, "74", 2) == 0) { /* Disable 360 circular interpolation */
	if (state->interpolation == MQ_CW_CIRCULAR)
	    state->interpolation = CW_CIRCULAR;
	else
	    state->interpolation = CCW_CIRCULAR;
	return;
    } else if (strncmp(op, "75", 2) == 0) { /* Enable 360 circular interpolation */
	state->interpolation = MQ_CW_CIRCULAR;
	return;
    } else if (strncmp(op, "90", 2) == 0) { /* Specify absolut format */
	if (format) format->coordinate = ABSOLUTE;
	return;
    } else if (strncmp(op, "91", 2) == 0) { /* Specify incremental format */
	if (format) format->coordinate = INCREMENTAL;
	return;
    } else {
	err(1, "Strange G code : %c%c\n", op[0], op[1]);
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
	    err(1, "Aperture out of bounds:%d\n", a);
	state->changed = 0;
    }
    
    return;
} /* parse_D_code */


static int
parse_M_code(gerb_file_t *fd)
{
    char op[2];
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");
    
    if (strncmp(op, "00", 2) == 0)      /* Program stop */
	return 1;
    else if (strncmp(op, "01", 2) == 0) /* Optional stop */
	return 2;
    else if (strncmp(op, "02", 2) == 0) /* End of program */
	return 3;
    else
	return 0;
} /* parse_M_code */


static void 
parse_rs274x(gerb_file_t *fd, gerb_image_t *image)
{
    char op[3];
    gerb_aperture_t *a = NULL;
    int ano;
    
    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    
    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");
    
    /* Directive parameters */
    if (strncmp(op, "AS", 2) == 0) {        /* Axis Select */
	NOT_IMPL(fd, "%AS%");
    } else if (strncmp(op, "FS", 2) == 0) { /* Format Statement */
	image->format = (gerb_format_t *)malloc(sizeof(gerb_format_t));
	if (image->format == NULL) 
	    err(1, "Failed malloc for format\n");
	bzero((void *)image->format, sizeof(gerb_format_t));
	
	op[0] = gerb_fgetc(fd);
	if (op[0] == 'L')
	    image->format->omit_zeros = LEADING;
	else if (op[0] == 'T')
	    image->format->omit_zeros = TRAILING;
	else if (op[0] == 'D')
	    image->format->omit_zeros = EXPLICIT;
	else {
#ifdef EAGLECAD_KLUDGE
	    fprintf(stderr,"EagleCad bug detected: Defaults to omit leading zeroes\n");
	    gerb_ungetc(fd);
	    image->format->omit_zeros = LEADING;
#else
	    err(1, "Format error: omit_zeros = %c\n", op[0]);
#endif
	}
	
	op[0] = gerb_fgetc(fd);
	if (op[0] == 'A')
	    image->format->coordinate = ABSOLUTE;
	else if (op[0] == 'T')
	    image->format->coordinate = INCREMENTAL;
	else
	    err(1, "Format error: coordinate = %c\n", op[0]);
	
	/* XXX Here are other definitions possible but we currently silently ignores them */
	while (gerb_fgetc(fd) != 'X') 
	    fprintf(stderr, "Not handled  type of format statement\n");
	op[0] = gerb_fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->x_int = (int)(op[0] - '0');
	op[0] = gerb_fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->x_dec = (int)(op[0] - '0');
	
	if (gerb_fgetc(fd) != 'Y') 
	    fprintf(stderr, "Not handled  type of format statement\n");
	op[0] = gerb_fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->y_int = (int)(op[0] - '0');
	op[0] = gerb_fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->y_dec = (int)(op[0] - '0');
	
    } else if (strncmp(op, "MI", 2) == 0) { /* Mirror Image */
	NOT_IMPL(fd, "%MI%");
    } else if (strncmp(op, "MO", 2) == 0) { /* Mode of Units */
	
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF))
	    err(1, "Unexpected EOF found.\n");
	
	if (strncmp(op, "IN", 2) == 0)
	    image->info->unit = INCH;
	else if (strncmp(op, "MM", 2) == 0)
	    image->info->unit = MM;
	else
	    err(1, "Illegal unit:%c%c\n", op[0], op[1]);
	
    } else if (strncmp(op, "OF", 2) == 0) { /* Offset */
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
		err(1, "Wrong character in offset:%c\n", op[0]);
	    }
	    op[0] = gerb_fgetc(fd);
	}
	return;
    } else if (strncmp(op, "SF", 2) == 0) { /* Scale Factor */
	NOT_IMPL(fd, "%SF%");
	
	/* Image parameters */
    } else if (strncmp(op, "IJ", 2) == 0) { /* Image Justify */
	NOT_IMPL(fd, "%IJ%");
    } else if (strncmp(op, "IN", 2) == 0) { /* Image Name */
	NOT_IMPL(fd, "%IN%");
    } else if (strncmp(op, "IO", 2) == 0) { /* Image Offset */
	NOT_IMPL(fd, "%IO%");
    } else if (strncmp(op, "IP", 2) == 0) { /* Image Polarity */
	
	op[0] = gerb_fgetc(fd);
	op[1] = gerb_fgetc(fd);
	op[2] = gerb_fgetc(fd);
	
	if ((op[0] == EOF) || (op[1] == EOF) || (op[2] == EOF))
	    err(1, "Unexpected EOF found.\n");
	
	if (strncmp(op, "POS", 3) == 0) 
	    image->info->polarity = POSITIVE;
	else if (strncmp(op, "NEG", 3) == 0)
	    image->info->polarity = NEGATIVE;
	else 
	    err(1, "Strange polarity : %c%c%c\n", op[0], op[1], op[2]);
	
    } else if (strncmp(op, "IR", 2) == 0) { /* Image Rotation */
	NOT_IMPL(fd, "%IR%");
    } else if (strncmp(op, "PF", 2) == 0) { /* Plotter Film */
	NOT_IMPL(fd, "%PF%");
	
	/* Aperture parameters */
    } else if (strncmp(op, "AD", 2) == 0) { /* Aperture Description */
	a = (gerb_aperture_t *)malloc(sizeof(gerb_aperture_t));
	bzero((void *)a, sizeof(gerb_aperture_t));
	ano = parse_aperture_definition(fd, a);
	if ((ano >= APERTURE_MIN) && (ano <= APERTURE_MAX)) 
	    image->aperture[ano] = a;
	else
	    err(1, "Aperture number out of bounds : %d\n", ano);
	
    } else if (strncmp(op, "AM", 2) == 0) { /* Aperture Macro */
	NOT_IMPL(fd, "%AM%");
	return;
	
	/* Layer */
    } else if (strncmp(op, "LN", 2) == 0) { /* Layer Name */
	NOT_IMPL(fd, "%LN%");
    } else if (strncmp(op, "LP", 2) == 0) { /* Layer Polarity */
	NOT_IMPL(fd, "%LP%");
    } else if (strncmp(op, "KO", 2) == 0) { /* Knock Out */
	NOT_IMPL(fd, "%KO%");
    } else if (strncmp(op, "SR", 2) == 0) { /* Step and Repeat */
	NOT_IMPL(fd, "%SR%");
    } else if (strncmp(op, "RO", 2) == 0) { /* Rotate */
	NOT_IMPL(fd, "%RO%");
    } else {
	fprintf(stderr, "Unknown extension found %%%c%c%%\n", op[0], op[1]);
    }
    
    return;
} /* parse_rs274x */


static int 
parse_aperture_definition(gerb_file_t *fd, gerb_aperture_t *aperture)
{
    int ano, i;
    char read;
    char type[50]; /* XXX */
    
    if (gerb_fgetc(fd) != 'D')
	return -1;
    
    bzero(type, sizeof(type)/sizeof(type[0]));
    
    ano = gerb_fgetint(fd);
    
    for (i = 0, type[i] = gerb_fgetc(fd); type[i] != ','; i++, type[i] = gerb_fgetc(fd));
    
    if (i == 1) {
	switch (type[0]) {
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
	type[i] = '\0';
	fprintf(stderr, "Aperture using macro %s[%d] ignored.\n", type, i);
	return ano;
    }
    
    for (read = 'X', i = 0; (read == 'X') && i < 5; read = gerb_fgetc(fd), i++)
	aperture->parameter[i] = gerb_fgetdouble(fd);
    
    aperture->nuf_parameters = i;
    
    gerb_ungetc(fd);

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
	    err(1, "Unknow quadrant value while converting to cw\n");
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
	err(1, "Strange quadrant : %d\n", quadrant);
    }

    /*
     * Some good values 
     */
#define DIFF(a, b) ((a > b) ? a - b : b - a)
    d1x = DIFF(net->start_x, net->cirseg->cp_x);
    d1y = DIFF(net->start_y, net->cirseg->cp_y);
    d2x = DIFF(net->stop_x, net->cirseg->cp_x);
    d2y = DIFF(net->stop_y, net->cirseg->cp_y);
    
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
	printf("FOO\n");
	net->cirseg->height = 0;
    }

#define RAD2DEG(a) (int)ceil(a * 180 / M_PI) 
    
    switch (quadrant) {
    case 1 :
	net->cirseg->angle1 = RAD2DEG(alfa);
	net->cirseg->angle2 = RAD2DEG(beta);
	break;
    case 2 :
	net->cirseg->angle1 = 180 - RAD2DEG(alfa);
	net->cirseg->angle2 = 180 - RAD2DEG(beta);
	break;
    case 3 : 
	net->cirseg->angle1 = 180 + RAD2DEG(alfa);
	net->cirseg->angle2 = 180 + RAD2DEG(beta);
	break;
    case 4 :
	net->cirseg->angle1 = 360 - RAD2DEG(alfa);
	net->cirseg->angle2 = 360 - RAD2DEG(beta);
	break;
    default :
	err(1, "Strange quadrant : %d\n", quadrant);
    }

    if (net->cirseg->width < 0.0)
	fprintf(stderr, "Negative width [%f] in quadrant %d [%f][%f]\n", 
		net->cirseg->width, quadrant, alfa, beta);
    
    if (net->cirseg->height < 0.0)
	fprintf(stderr, "Negative height [%f] in quadrant %d [%d][%d]\n", 
		net->cirseg->height, quadrant, RAD2DEG(alfa), RAD2DEG(beta));
    
    return;

} /* calc_cirseg_sq */


static void 
calc_cirseg_mq(struct gerb_net *net, int cw, 
	       double delta_cp_x, double delta_cp_y)
{
    double d1x, d1y, d2x, d2y;
    double alfa, beta;
    int quadrant = 0;

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


#define ABS(a) (a>0.0?a:-a)
#define MAX(a,b) (a>b?a:b)
    net->cirseg->width = MAX(ABS(delta_cp_x), ABS(delta_cp_y)) * 2.0;
    net->cirseg->height = MAX(ABS(delta_cp_x), ABS(delta_cp_y)) * 2.0 ;

    net->cirseg->angle1 = RAD2DEG(alfa);
    net->cirseg->angle2 = RAD2DEG(beta);

    /*
     * Make sure it's always positive angles
     */
    if (net->cirseg->angle1 < 0) 
	net->cirseg->angle1 = 360 + net->cirseg->angle1;

    if (net->cirseg->angle2 < 0) 
	net->cirseg->angle2 = 360 + net->cirseg->angle2;

    if(net->cirseg->angle2 == 0)
	net->cirseg->angle2 = 360;

    return;
} /* calc_cirseg_mq */
