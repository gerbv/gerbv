/*
 * gEDA - GNU Electronic Design Automation
 * drill.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  /* pow() */

#include "drill.h"


#define NOT_IMPL(fd, s) do { \
                             fprintf(stderr, "Not Implemented:%s\n", s); \
                           } while(0)

/* Local function prototypes */
static void drill_parse_G_code(FILE *fd, drill_state_t *state, drill_format_t *format);
static int drill_parse_M_code(FILE *fd, drill_image_t *image);
static int drill_parse_T_code(FILE *fd, drill_state_t *state, drill_image_t *image);
static void drill_parse_coordinate(FILE *fd, char firstchar, drill_state_t *state);
static drill_image_t *new_image(drill_image_t *image);
static drill_state_t *new_state(drill_state_t *state);
/* static void parse_drill(FILE *fd, drill_state_t *state, drill_image_t *image); */
static int read_int(FILE *fd);
static double read_double(FILE *fd);
static void eat_line(FILE *fd);


drill_image_t *
parse_drillfile(FILE *fd)
{
    drill_state_t *state = NULL;
    drill_image_t *image = NULL;
    drill_hole_t *curr_hole = NULL;
    char read;
    double x_scale, y_scale;

    state = new_state(state);
    if (state == NULL)
	err(1, "malloc state failed\n");

    image = new_image(image);
    if (image == NULL)
	err(1, "malloc image failed\n");
    curr_hole = image->holes;

    while ((read = (char)fgetc(fd)) != EOF) {
	
	switch (read) {
	case ';' :
	    /* Comment found. Eat rest of line */
	    eat_line(fd);
	    break;
	case 'G':
/*	    drill_parse_G_code(fd, state, image->format); */
	    eat_line(fd);
	    
	    break;
	case 'T':
	    drill_parse_T_code(fd, state, image);
	    break;
	case 'M':
	    switch(drill_parse_M_code(fd, image)) {
	    case DRILL_M_HEADER :
	    case DRILL_M_METRIC :
	    case DRILL_M_IMPERIAL :
	    case DRILL_M_NOT_IMPLEMENTED :
	    case DRILL_M_ENDOFPATTERN :
		break;
	    case DRILL_M_END :
		free(state);
		return image;
		break;
	    default:
		err(1, "Strange M code found.\n");
	    }
	    break;

	case 'X':
	case 'Y':
	    /* Hole coordinate found. Do some parsing */
	    drill_parse_coordinate(fd, read, state);

/*	    if (image && image->format ){
		x_scale = pow(10.0, (double)image->format->x_dec);
		y_scale = pow(10.0, (double)image->format->y_dec);
	    }
*/
/*	    curr_hole->x = (double)state->curr_x / x_scale;
	    curr_hole->y = (double)state->curr_y / y_scale;
*/	    curr_hole->x = (double)state->curr_x;
	    curr_hole->y = (double)state->curr_y;
	    curr_hole->tool_num = state->current_tool;

	    /*
	     * Find min and max of image
	     */
	    if (image->info->min_x == 0.0 || image->info->min_x > curr_hole->x)
		image->info->min_x = curr_hole->x;
	    if (image->info->min_y == 0.0 || image->info->min_y > curr_hole->y)
		image->info->min_y = curr_hole->y;
	    if (image->info->max_x < curr_hole->x)
		image->info->max_x = curr_hole->x;
	    if (image->info->max_y < curr_hole->x)
		image->info->max_y = curr_hole->y;

	    curr_hole->next = (drill_hole_t *)malloc(sizeof(drill_hole_t));
	    curr_hole = curr_hole->next;
	    bzero((void *)curr_hole, sizeof(drill_hole_t));

	    break;

	case '%':
	    printf("Found start of data segment\n");

/*	    parse_drill(fd, state, image); */
	    break;
#if 0
	case '*':
/*			state->prev_x = state->curr_x;
			state->prev_y = state->curr_y;
*/
	    break;
#endif
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	    break;
	default:
	    fprintf(stderr, "Found unknown character (whitespace?) %c[%d]\n", read, read);
	}
    }

    fprintf(stderr, "File is missing drill End-Of-File\n");

    return image;
} /* parse_drillfile */


void
free_drill_image(drill_image_t *image)
{
    int i;
    drill_hole_t *hole, *tmp_hole;

    /*
     * Free tools
     */
    for (i = 0; i < TOOL_MAX - TOOL_MIN; i++) 
	if (image->tools[i] != NULL) 
	    free(image->tools[i]);
    /*
     * Free format
     */
    free(image->format);

    /*
     * Free info
     */
    free(image->info);

    /*
     * Free holelist
     */
    for (hole = image->holes; hole != NULL;
	 tmp_hole = hole, hole = hole->next, free(tmp_hole));

    /*
     * Free and reset the final image
     */
    free(image);
    image = NULL;

    return;
} /* free_drill_image */


static void 
drill_parse_G_code(FILE *fd, drill_state_t *state, drill_format_t *format)
{
    char op[2];

    op[0] = fgetc(fd);
    op[1] = fgetc(fd);

    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");

    if (strncmp(op, "00", 2) == 0) { 	/* Move */
	/* Is this doing anything really? */
    } else if (strncmp(op, "01", 2) == 0) { /* Linear Interpolation (1X scale) */
//		state->interpolation = LINEARx1;
	return;
    } else if (strncmp(op, "02", 2) == 0) { /* Clockwise Linear Interpolation */
/*		if (state->interpolation == MQ_CW_CIRCULAR ||
		state->interpolation == MQ_CCW_CIRCULAR)
		state->interpolation = MQ_CW_CIRCULAR;
		else 
		state->interpolation = CW_CIRCULAR;
*/		return;
    } else if (strncmp(op, "03", 2) == 0) { /* Counter Clockwise Linear Interpolation */
/*		if (state->interpolation == MQ_CW_CIRCULAR ||
		state->interpolation == MQ_CCW_CIRCULAR)
		state->interpolation = MQ_CCW_CIRCULAR;
		else 
		state->interpolation = CCW_CIRCULAR;
*/		return;
    } else if (strncmp(op, "04", 2) == 0) { /* Ignore Data Block */
	/* Don't do anything, just read 'til * below */
    } else if (strncmp(op, "10", 2) == 0) { /* Linear Interpolation (10X scale) */
//		state->interpolation = LINEARx10;
	return;
    } else if (strncmp(op, "11", 2) == 0) { /* Linear Interpolation (0.1X scale) */
//		state->interpolation = LINEARx01;
	return;
    } else if (strncmp(op, "12", 2) == 0) { /* Linear Interpolation (0.01X scale) */
//		state->interpolation = LINEARx001;
	return;
    } else if (strncmp(op, "36", 2) == 0) { /* Turn on Polygon Area Fill */
	NOT_IMPL(fd, "G36");
    } else if (strncmp(op, "37", 2) == 0) { /* Turn off Polygon Area Fill */
	NOT_IMPL(fd, "G37");
    } else if (strncmp(op, "54", 2) == 0) { /* Tool prepare */
	if (fgetc(fd) == 'D') {  /* XXX Check return value */
//			state->curr_aperture = read_int(fd);
	} else {
	    err(1, "Strange code after G54\n");
	}

    } else if (strncmp(op, "70", 2) == 0) { /* Specify inches */
	NOT_IMPL(fd, "G70");
    } else if (strncmp(op, "71", 2) == 0) { /* Specify millimeters */
	NOT_IMPL(fd, "G71");
    } else if (strncmp(op, "74", 2) == 0) { /* Disable 360 circular interpolation */
/*		if (state->interpolation == MQ_CW_CIRCULAR)
		state->interpolation = CW_CIRCULAR;
		else
		state->interpolation = CCW_CIRCULAR;
*/		return;
    } else if (strncmp(op, "75", 2) == 0) { /* Enable 360 circular interpolation */
//		state->interpolation = MQ_CW_CIRCULAR;
	return;
    } else if (strncmp(op, "90", 2) == 0) { /* Specify absolut format */
//		if (format) format->coordinate = ABSOLUTE;
	return;
    } else if (strncmp(op, "91", 2) == 0) { /* Specify incremental format */
//		if (format) format->coordinate = INCREMENTAL;
	return;
    } else {
	err(1, "Strange G code : %c%c\n", op[0], op[1]);
    }
	
    while (fgetc(fd) != '*'); /* XXX Check return value */

    return;
} /* drill_parse_G_code */

static int
drill_parse_T_code(FILE *fd, drill_state_t *state, drill_image_t *image)
{
    int tool_num;
    char temp;

    tool_num = read_int(fd);
    if ((tool_num < TOOL_MIN && tool_num != 0) || (tool_num >= TOOL_MAX)) 
	err(1, "Tool out of bounds: %d\n", tool_num);
    
    /* Set the current tool to the correct one */
    state->current_tool = tool_num;

    /* Check for a size definition */
    temp = fgetc(fd);
    if(temp != 'C') {
	ungetc(temp, fd);
    } else {
	char text_size[0x40];
	double size;
	
	size = read_double(fd);
	if(size <= 0 || size >= 1000) {
	    err(1, "Tool is wrong size: %h\n", size);
	} else {
	    if(image->tools[tool_num] != NULL) {
		err(1, "Tool is already defined\n");
	    } else {
		image->tools[tool_num] =
		    (drill_tool_t *)malloc(sizeof(drill_tool_t));
		if (image->tools[tool_num] == NULL) {
		    err(1, "malloc tool failed\n");
		}
		image->tools[tool_num]->diameter = size;
		/* There's really no way of knowing this */
		image->tools[tool_num]->unit = MM;
		
	    }
	    
	    printf("Tool %02d size %2.4g found\n", tool_num, size);
	}
    }

    return tool_num;
} /* drill_parse_T_code */


static int
drill_parse_M_code(FILE *fd, drill_image_t *image)
{
    char op[3] = "  ";

    op[0] = fgetc(fd);
    op[1] = fgetc(fd);

    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");

    printf("M code: %2s\n", op);

    if (strncmp(op, "00", 2) == 0 || strncmp(op, "30", 2) == 0) {
	/* Program stop */
	return DRILL_M_END;
    } else if (strncmp(op, "01", 2) == 0) {
	return DRILL_M_ENDOFPATTERN;
    } else if (strncmp(op, "48", 2) == 0) {
	return DRILL_M_HEADER;
    } else if (strncmp(op, "47", 2) == 0) {
	return DRILL_M_FILENAME;
    } else if (strncmp(op, "48", 2) == 0) {
	return DRILL_M_HEADER;
    } else if (strncmp(op, "71", 2) == 0) {
	image->format->unit = MM;
	eat_line(fd);
	return DRILL_M_METRIC;
    } else if (strncmp(op, "72", 2) == 0) {
	image->format->unit = INCH;
	eat_line(fd);
	return DRILL_M_IMPERIAL;
    }
    return DRILL_M_UNKNOWN;

} /* drill_parse_M_code */


static void
drill_parse_coordinate(FILE *fd, char firstchar, drill_state_t *state)

{
    char read;

    if(firstchar == 'X') {
	state->curr_x = read_double(fd);
	if((read = fgetc(fd)) != 'Y') return;
    }
    state->curr_y = read_double(fd);

//    eat_line(fd);

} /* drill_parse_coordinate */


#if 0 /* This second stage probably isn't needed with NC drill files */
static void 
parse_drill(FILE *fd, drill_state_t *state, drill_image_t *image)
{
    char op[3];
    drill_tool_t *tool = NULL;
    int tool_num;

    op[0] = fgetc(fd);
    op[1] = fgetc(fd);

    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");

    /* Directive parameters */
    if (strncmp(op, "AS", 2) == 0) {        /* Axis Select */
	NOT_IMPL(fd, "%AS%");
    } else if (strncmp(op, "FS", 2) == 0) { /* Format Statement */
	image->format = (drill_format_t *)malloc(sizeof(drill_format_t));
	if (image->format == NULL) 
	    err(1, "Failed malloc for format\n");
	bzero((void *)image->format, sizeof(drill_format_t));

	op[0] = fgetc(fd);
	if (op[0] == 'L')
	    image->format->omit_zeros = LEADING;
	else if (op[0] == 'T')
	    image->format->omit_zeros = TRAILING;
	else
	    err(1, "Format error: omit_zeros = %c\n", op[0]);

	op[0] = fgetc(fd);
	if (op[0] == 'A')
	    image->format->coordinate = ABSOLUTE;
	else if (op[0] == 'T')
	    image->format->coordinate = INCREMENTAL;
	else
	    err(1, "Format error: coordinate = %c\n", op[0]);

	/* XXX Here are other definitions possible but we currently silently ignore them */
	while (fgetc(fd) != 'X') 
	    fprintf(stderr, "Not handled  type of format statement\n");
	op[0] = fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->x_int = (int)(op[0] - '0');
	op[0] = fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->x_dec = (int)(op[0] - '0');

	if (fgetc(fd) != 'Y') 
	    fprintf(stderr, "Not handled  type of format statement\n");
	op[0] = fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->y_int = (int)(op[0] - '0');
	op[0] = fgetc(fd);
	if ((op[0] < '0') || (op[0] > '6'))
	    err(1,  "Illegal format size : %c\n", op[0]);
	image->format->y_dec = (int)(op[0] - '0');
			
    } else if (strncmp(op, "MI", 2) == 0) { /* Mirror Image */
	NOT_IMPL(fd, "%MI%");
    } else if (strncmp(op, "MO", 2) == 0) { /* Mode of Units */

	op[0] = fgetc(fd);
	op[1] = fgetc(fd);

	if ((op[0] == EOF) || (op[1] == EOF))
	    err(1, "Unexpected EOF found.\n");

	if (strncmp(op, "IN", 2) == 0)
	    image->format->unit = INCH;
	else if (strncmp(op, "MM", 2) == 0)
	    image->format->unit = MM;
	else
	    err(1, "Illegal unit:%c%c\n", op[0], op[1]);

    } else if (strncmp(op, "OF", 2) == 0) { /* Offset */
	op[0] = fgetc(fd);
	while (op[0] != '*') {
	    switch (op[0]) {
	    case 'A' :
		fscanf(fd, "%lf", &(image->info->offset_a));
		break;
	    case 'B' :
		fscanf(fd, "%lf", &(image->info->offset_b));
		break;
	    default :
		err(1, "Wrong character in offset:%c\n", op[0]);
	    }
	    op[0] = fgetc(fd);
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
    } else if (strncmp(op, "IR", 2) == 0) { /* Image Rotation */
	NOT_IMPL(fd, "%IR%");
    } else if (strncmp(op, "PF", 2) == 0) { /* Plotter Film */
	NOT_IMPL(fd, "%PF%");

	/* Tool parameters */
    } else if (strncmp(op, "AD", 2) == 0) { /* Tool Description */
	tool = (drill_tool_t *)malloc(sizeof(drill_tool_t));
	bzero((void *)tool, sizeof(drill_tool_t));
	tool_num = drill_parse_T_code(fd, state, image);
	if ((tool_num >= TOOL_MIN) && (tool_num <= TOOL_MAX)) 
	    image->tools[tool_num - TOOL_MIN] = tool;
	else
	    err(1, "Tool number out of bounds : %d\n", tool_num);

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
} /* parse_drill */
#endif


static drill_image_t *new_image(drill_image_t *image)
{
    image = (drill_image_t *)malloc(sizeof(drill_image_t));
    if (image != NULL) {
	
	bzero((void *)image, sizeof(drill_image_t));

	image->holes = (drill_hole_t *)malloc(sizeof(drill_hole_t));
	if (image->holes != NULL) {
	    
	    bzero((void *)image->holes, sizeof(drill_hole_t));

	    image->info = (drill_image_info_t *)malloc(sizeof(drill_image_info_t));
	    if (image->info != NULL) {
		bzero((void *)image->info, sizeof(drill_image_info_t));
		image->info->min_x = HUGE_VAL;
		image->info->min_y = HUGE_VAL;
		image->info->max_x = -HUGE_VAL;
		image->info->max_y = -HUGE_VAL;
		
		image->format = (drill_format_t *)malloc(sizeof(drill_format_t));
		if (image->format != NULL) {
		    bzero((void *)image->format, sizeof(drill_format_t));
		    return image;
		}
		free(image->info);
		image->info = NULL;
	    }
	    free(image->holes);
	    image->holes = NULL;
	}
	free(image);
	image = NULL;
    }
    
    return image;
} /* new_image */

static drill_state_t *new_state(drill_state_t *state)
{
    state = (drill_state_t *)malloc(sizeof(drill_state_t));
    if (state != NULL) {
	/* Init structure */
	bzero((void *)state, sizeof(drill_state_t));
    }
    return state;
} /* new_state */

/* This is a special read_int used in this file only.
   Do not let it pollute the namespace by defining it in the .h-file */
static int
read_int(FILE *fd)
{
    char read;
    int i = 0;
    int neg = 0;

    read = fgetc(fd); /* XXX Should check return value */

    if (read == '-') {
	neg = 1;
	read = fgetc(fd); /* XXX Should check return value */
    }

    while (read >= '0' && read <= '9') {
	i = i*10 + ((int)read - '0');
	read = fgetc(fd); /* XXX Should check return value */
    }
	
    if (ungetc(read, fd) != read) {
	perror("read_int:ungetc");
	exit(1);
    }

    if (neg)
	return -i;
    else
	return i;
} /* read_int */

static double
read_double(FILE *fd)
{
    char read;
    char temp[0x20];
    int i = 0;
    double result = 0;

    bzero(temp, sizeof(temp));

    read = fgetc(fd);
    while(read != EOF && i < sizeof(temp) && (isdigit(read) || read == '.' || read == '+' || read == '-')) {
	temp[i++] = read;
	read = fgetc(fd);
    }
    
    ungetc(read, fd);
    result = strtod(temp, NULL);

    return result;
} /* read_double */

/* This function eats all characters up to and including 
   the first one of CR or LF */
static void
eat_line(FILE *fd)
{
    char read = fgetc(fd);

    while(read != 10 && read != 13) {
	if (read == EOF) err(1, "Unexpected EOF found.\n");
	read = fgetc(fd);
    }
} /* eat_line */
