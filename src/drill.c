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
#include <ctype.h>

#include "drill.h"


#define NOT_IMPL(fd, s) do { \
                             fprintf(stderr, "Not Implemented:%s\n", s); \
                           } while(0)

/* Local function prototypes */
static void drill_guess_format(FILE *fd, gerb_image_t *image);
static int drill_parse_M_code(FILE *fd, gerb_image_t *image);
static int drill_parse_T_code(FILE *fd, drill_state_t *state, gerb_image_t *image);
static void drill_parse_coordinate(FILE *fd, char firstchar, drill_state_t *state);
static gerb_image_t *new_image(gerb_image_t *image);
static drill_state_t *new_state(drill_state_t *state);
static int read_int(FILE *fd);
static double read_double(FILE *fd);
static void eat_line(FILE *fd);

gerb_image_t *
parse_drillfile(FILE *fd)
{
    drill_state_t *state = NULL;
    gerb_image_t *image = NULL;
    gerb_net_t *curr_net = NULL;
    char read;
    double x_scale = 1, y_scale = 1;

    state = new_state(state);
    if (state == NULL)
	err(1, "malloc state failed\n");

    image = new_image(image);
    if (image == NULL)
	err(1, "malloc image failed\n");
    curr_net = image->netlist;

    drill_guess_format(fd, image);

    if (image && image->format ){
	x_scale = pow(10.0, (double)image->format->x_dec);
	y_scale = pow(10.0, (double)image->format->y_dec);

	/* KLUDGE. I can't get the scale right, somehow... */
	if(image->info->unit == MM) {
	    x_scale *= 25.4;
	    y_scale *= 25.4;
	}
    }

    while ((read = (char)fgetc(fd)) != EOF) {

	switch (read) {
	case ';' :
	    /* Comment found. Eat rest of line */
	    eat_line(fd);
	    break;
	case 'F' :
	    /* Z axis feed speed. Silently ignored */
	    eat_line(fd);
	    break;
	case 'G':
	    /* G codes aren't used, for now */
/*	    drill_parse_G_code(fd, state, image->format); */
	    eat_line(fd);
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

	case 'S':
	    /* Spindle speed. Silently ignored */
	    eat_line(fd);
	    break;
	case 'T':
	    drill_parse_T_code(fd, state, image);
	    break;
	case 'X':
	case 'Y':
	    /* Hole coordinate found. Do some parsing */
	    drill_parse_coordinate(fd, read, state);

	    curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
	    curr_net = curr_net->next;
	    bzero((void *)curr_net, sizeof(gerb_net_t));

	    curr_net->start_x = (double)state->curr_x / x_scale;
	    curr_net->start_y = (double)state->curr_y / y_scale;
	    curr_net->stop_x = (double)state->curr_x / x_scale;
	    curr_net->stop_y = (double)state->curr_y / y_scale;

	    curr_net->aperture = state->current_tool;
	    curr_net->aperture_state = FLASH;

	    /* Find min and max of image */
	    image->info->min_x = min(image->info->min_x, curr_net->start_x);
	    image->info->min_y = min(image->info->min_y, curr_net->start_y);
	    image->info->max_x = max(image->info->max_x, curr_net->start_x);
	    image->info->max_y = max(image->info->max_y, curr_net->start_y);

	    break;

	case '%':
/*	    printf("Found start of data segment\n"); */
	    break;
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	    break;
	default:
	    fprintf(stderr, "Found unknown character %c [0x%02x], ignoring\n",
		    read, read);
	}
    }

    fprintf(stderr, "File is missing drill End-Of-File\n");

    return image;
} /* parse_drillfile */

/* Guess the format of the input file.
   Rewinds file when done */
static void
drill_guess_format(FILE *fd, gerb_image_t *image)
{
    int inch_score = 0;
    int metric_score = 0;
    int length, max_length = 0;
    int leading_zeros, max_leading_zeros = 0;
    int trailing_zeros, max_trailing_zeros = 0;
    char read;
    drill_state_t *state = NULL;
    gerb_net_t curr_net;
    int done = FALSE;
    int i;

    state = new_state(state);
    if (state == NULL)
	err(1, "malloc state failed\n");

    image->format = (gerb_format_t *)malloc(sizeof(gerb_format_t));
    if (image->format == NULL) 
	err(1, "malloc format failed\n");
    bzero((void *)image->format, sizeof(gerb_format_t));

    /* This is just a special case of the normal parser */
    while ((read = (char)fgetc(fd)) != EOF && !done) {
	switch (read) {
	case ';' :
	    /* Comment found. Eat rest of line */
	    eat_line(fd);
	    break;
	case 'F' :
	    /* Z axis feed speed. Silently ignored */
	case 'S':
	    /* Spindle speed. Silently ignored */
	    eat_line(fd);
	    break;
	case 'G':
	    /* G codes aren't used, for now */
	    eat_line(fd);
	    break;
	case 'M':
	    switch(drill_parse_M_code(fd, image)) {
	    case DRILL_M_METRIC :
		metric_score++;
		break;
	    case DRILL_M_IMPERIAL :
		inch_score++;
		break;
	    case DRILL_M_END :
		done = TRUE;
		break;
	    default:
		break;
	    }
	    break;

	case 'T':
	    drill_parse_T_code(fd, state, image);
	    break;
	case 'X':
	case 'Y':
	    /* How many leading zeros? */
	    length = 0;
	    leading_zeros = 0;
	    trailing_zeros = 0;
	    {
		/* This state machine is a bit ugly, so it'll probably
		   have to be rewritten sometime */
		int local_state = 0;
		while ((read = (char)fgetc(fd)) != EOF && isdigit(read)) {
		    
		    length ++;
		    switch (local_state) {
		    case 0:
			if(read == '0') {
			    leading_zeros++;
			} else {
			    local_state++;
			}
			break;
		    case 1:
			if(read =='0') {
			    trailing_zeros++;
			}
		    }
		}
	    }
	    max_length = max(max_length, length);
	    max_leading_zeros = max(max_leading_zeros, leading_zeros);
	    max_trailing_zeros = max(max_trailing_zeros, trailing_zeros);
	    break;

	case '%':
	    break;
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	default:
	    break;
	}
    }

    /* Unfortunately, inches seem more common, so that's the default */
    if(metric_score > inch_score) {
	image->info->unit = MM;
    } else {
	image->info->unit = INCH;
    }

    /* Knowing about trailing zero suppression is more important,
       so it takes precedence here. */
    if (max_trailing_zeros == 0) {
	/* No trailing zero anywhere. It's probable they're suppressed */
	image->format->omit_zeros = TRAILING;
    } else if(max_leading_zeros == 0) {
	/* No leading zero anywhere. It's probable they're suppressed */
	image->format->omit_zeros = LEADING;
    } else if (max_trailing_zeros >= max_leading_zeros ) {
	image->format->omit_zeros = TRAILING;
    } else {
 	image->format->omit_zeros = LEADING;
    }

    /* Almost every file seems to use 2.x format (where x is 3-4) */
    image->format->x_dec = max_length - 2;
    image->format->y_dec = max_length - 2;

    /* Restore the necessary things back to their default state */
    for (i = 0; i < APERTURE_MAX; i++) {
	    free(image->aperture[i]);
	    image->aperture[i] = NULL;
    }

    rewind(fd);
}

/* Parse tool definition. This can get a bit tricky since it can
   appear in the header and/or the data.
   Returns tool number on success, -1 on error */
static int
drill_parse_T_code(FILE *fd, drill_state_t *state, gerb_image_t *image)
{
    int tool_num;
    int done = FALSE;
    char temp;
    double size;

    tool_num = read_int(fd);
    if (tool_num == 0) return tool_num;
    if ((tool_num < TOOL_MIN) || (tool_num >= TOOL_MAX)) 
	err(1, "Tool out of bounds: %d\n", tool_num);

    /* Set the current tool to the correct one */
    state->current_tool = tool_num;

    /* Check for a size definition */
    temp = fgetc(fd);

    while(!done) {
	
	switch(temp) {
	case 'C':

	    size = read_double(fd);
	    if(image->info->unit == MM) {
		size /= 25.4;
	    }

	    if(size <= 0 || size >= 10000) {
		err(1, "Tool is wrong size: %g\n", size);
	    } else {
		if(image->aperture[tool_num] != NULL) {
		    err(1, "Tool is already defined\n");
		} else {
		    image->aperture[tool_num] =
			(gerb_aperture_t *)malloc(sizeof(gerb_aperture_t));
		    if (image->aperture[tool_num] == NULL) {
			err(1, "malloc tool failed\n");
		    }
		    /* There's really no way of knowing what unit the tools
		       are defined in without sneaking a peek in the rest 
		       of the file first. Will have to be done. */
		    image->aperture[tool_num]->parameter[0] = size;
		    image->aperture[tool_num]->type = CIRCLE;
		}
/*		printf("Tool %02d size %2.4g found\n", tool_num, size); */
	    }
	    break;
	    
	case 'F':
	case 'S' :
	    /* Silently ignored. They're not important. */
	    read_int(fd);
	    break;
	    
	default:
	    /* Stop when finding anything but what's expected
	       (and put it back) */
	    ungetc(temp, fd);
	    done = TRUE;
	    break;
	}
	if( (temp = (char)fgetc(fd)) == EOF) {
	    err(1, "(very) Unexpected end of file found\n");
	}
    }
    
    return tool_num;
} /* drill_parse_T_code */


static int
drill_parse_M_code(FILE *fd, gerb_image_t *image)
{
    char op[3] = "  ";

    op[0] = fgetc(fd);
    op[1] = fgetc(fd);

    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");

/*    printf("M code: %2s\n", op); */

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
	image->info->unit = MM;
	eat_line(fd);
	return DRILL_M_METRIC;
    } else if (strncmp(op, "72", 2) == 0) {
	image->info->unit = INCH;
	eat_line(fd);
	return DRILL_M_IMPERIAL;
    }
    return DRILL_M_UNKNOWN;

} /* drill_parse_M_code */

/* Parse on drill file coordinate.
   Returns nothing, but modifies state */
static void
drill_parse_coordinate(FILE *fd, char firstchar, drill_state_t *state)

{
    char read;

    if(firstchar == 'X') {
	state->curr_x = read_double(fd);
	if((read = fgetc(fd)) != 'Y') return;
    }
    state->curr_y = read_double(fd);

} /* drill_parse_coordinate */


/* Allocates a new gerb_image structure
   Returns gerb_image pointer on success, NULL on ERROR */
static gerb_image_t *
new_image(gerb_image_t *image)
{

    image = (gerb_image_t *)malloc(sizeof(gerb_image_t));
    if (image != NULL) {

	bzero((void *)image, sizeof(gerb_image_t));

	image->netlist = (gerb_net_t *)malloc(sizeof(gerb_net_t));
	if (image->netlist != NULL) {

	    bzero((void *)image->netlist, sizeof(gerb_net_t));
	    
	    image->info = (gerb_image_info_t *)malloc(sizeof(gerb_image_info_t));
	    if (image->info != NULL) {
		bzero((void *)image->info, sizeof(gerb_image_info_t));

		image->info->min_x = HUGE_VAL;
		image->info->min_y = HUGE_VAL;
		image->info->max_x = -HUGE_VAL;
		image->info->max_y = -HUGE_VAL;
		
		return image;
	    }
	    
	    free(image->netlist);
	    image->netlist = NULL;
	}
	free(image);
	image = NULL;
    }
    
    return NULL;
}

/* Allocates and returns a new drill_state structure
   Returns state pointer on success, NULL on ERROR */
static drill_state_t *
new_state(drill_state_t *state)
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

/* Reads one double from fd and returns it */
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

/* Eats all characters up to and including 
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
