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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <math.h>  /* pow() */
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/mman.h>

#include "drill.h"


#define NOT_IMPL(fd, s) do { \
                             fprintf(stderr, "Not Implemented:%s\n", s); \
                           } while(0)

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif

/* I couldn't possibly code without these */
#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#undef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))

enum drill_file_section_t {DRILL_NONE, DRILL_HEADER, DRILL_DATA};
enum drill_coordinate_mode_t {DRILL_MODE_ABSOLUTE, DRILL_MODE_INCREMENTAL};

enum drill_m_code_t {DRILL_M_UNKNOWN, DRILL_M_NOT_IMPLEMENTED,
		     DRILL_M_END, DRILL_M_ENDREWIND,
		     DRILL_M_MESSAGE, DRILL_M_LONGMESSAGE,
		     DRILL_M_HEADER, DRILL_M_ENDHEADER,
		     DRILL_M_METRIC, DRILL_M_IMPERIAL,
		     DRILL_M_BEGINPATTERN, DRILL_M_ENDPATTERN,
		     DRILL_M_CANNEDTEXT, DRILL_M_TIPCHECK};

enum drill_g_code_t {DRILL_G_ABSOLUTE, DRILL_G_INCREMENTAL,
		     DRILL_G_ZEROSET, DRILL_G_UNKNOWN,
		     DRILL_G_ROUT, DRILL_G_DRILL,
		     DRILL_G_LINEARMOVE, DRILL_G_CWMOVE, DRILL_G_CCWMOVE};

typedef struct drill_state {
    double curr_x;
    double curr_y;
    int current_tool;
    int curr_section;
    int coordinate_mode;
    double origin_x;
    double origin_y;
} drill_state_t;

/* Local function prototypes */
static void drill_guess_format(gerb_file_t *fd, gerb_image_t *image);
static int drill_parse_G_code(gerb_file_t *fd, gerb_image_t *image);
static int drill_parse_M_code(gerb_file_t *fd, gerb_image_t *image);
static int drill_parse_T_code(gerb_file_t *fd, drill_state_t *state, gerb_image_t *image);
static void drill_parse_coordinate(gerb_file_t *fd, char firstchar, double scale_factor, drill_state_t *state);
static drill_state_t *new_state(drill_state_t *state);
static double read_double(gerb_file_t *fd, double scale_factor);
static void eat_line(gerb_file_t *fd);

gerb_image_t *
parse_drillfile(gerb_file_t *fd)
{
    drill_state_t *state = NULL;
    gerb_image_t *image = NULL;
    gerb_net_t *curr_net = NULL;
    char read;
    double x_scale = 1, y_scale = 1;

    state = new_state(state);
    if (state == NULL)
	err(1, "malloc state failed\n");

    image = new_gerb_image(image);
    if (image == NULL)
	err(1, "malloc image failed\n");
    curr_net = image->netlist;

    drill_guess_format(fd, image);

    if (image && image->format ){
	x_scale = pow(10.0, (double)image->format->x_dec);
	y_scale = pow(10.0, (double)image->format->y_dec);
    }

    while ((read = gerb_fgetc(fd)) != EOF) {

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
	    switch(drill_parse_G_code(fd, image)) {
	    case DRILL_G_ROUT :
		err(1, "Rout mode data is not supported\n");
		break;
	    case DRILL_G_DRILL :
		break;
	    case DRILL_G_ABSOLUTE :
		state->coordinate_mode = DRILL_MODE_ABSOLUTE;
		break;
	    case DRILL_G_INCREMENTAL :
		state->coordinate_mode = DRILL_MODE_INCREMENTAL;
		break;
	    case DRILL_G_ZEROSET :
		if((read = gerb_fgetc(fd)) == EOF)
		    err(1, "Unexpected EOF\n");
		drill_parse_coordinate(fd, read, 1.0 / x_scale, state);
		state->origin_x = state->curr_x;
		state->origin_y = state->curr_y;
		break;
	    default :
		eat_line(fd);
		break;
	    }
	    break;
	case 'M':
	    switch(drill_parse_M_code(fd, image)) {
	    case DRILL_M_HEADER :
		state->curr_section = DRILL_HEADER;
		break;
	    case DRILL_M_ENDHEADER :
		state->curr_section = DRILL_DATA;
		break;
	    case DRILL_M_METRIC :
		/* This is taken care of elsewhere */
		break;
	    case DRILL_M_IMPERIAL :
		/* This is taken care of elsewhere */
		break;
	    case DRILL_M_LONGMESSAGE :
	    case DRILL_M_MESSAGE :
		/* Until we have a console, these are ignored */
	    case DRILL_M_CANNEDTEXT :
		/* Ignored, probably permanently */
		eat_line(fd);
		break;
	    case DRILL_M_NOT_IMPLEMENTED :
	    case DRILL_M_ENDPATTERN :
	    case DRILL_M_TIPCHECK :
		break;
	    case DRILL_M_END :
		/* M00 has optional arguments */
		eat_line(fd);
	    case DRILL_M_ENDREWIND :
		free(state);
		/* KLUDGE. All images, regardless of input format,
		   are returned in INCH format */
		image->info->unit = INCH;

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
	    drill_parse_coordinate(fd, read, 1.0 / x_scale, state);

	    curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
	    curr_net = curr_net->next;
	    memset((void *)curr_net, 0, sizeof(gerb_net_t));

	    curr_net->start_x = (double)state->curr_x;
	    curr_net->start_y = (double)state->curr_y;
	    /* KLUDGE. This function isn't allowed to return anything
	       but inches */
	    if(image->info->unit == MM) {
		(double)curr_net->start_x /= 25.4;
		(double)curr_net->start_y /= 25.4;
	    }
	    curr_net->stop_x = curr_net->start_x - state->origin_x;
	    curr_net->stop_y = curr_net->start_y - state->origin_y;
	    curr_net->aperture = state->current_tool;
	    curr_net->aperture_state = FLASH;

	    /* Find min and max of image */
	    image->info->min_x = min(image->info->min_x, curr_net->start_x);
	    image->info->min_y = min(image->info->min_y, curr_net->start_y);
	    image->info->max_x = max(image->info->max_x, curr_net->start_x);
	    image->info->max_y = max(image->info->max_y, curr_net->start_y);

	    break;
	case '%':
	    state->curr_section = DRILL_DATA;
	    break;
	case 10 :   /* White space */
	case 13 :
	case ' ' :
	case '\t' :
	    break;
	default:
	    if(state->curr_section == DRILL_HEADER) {
		/* Unrecognised crap in the header is thrown away */
		eat_line(fd);
	    } else {
		fprintf(stderr,
			"Found unknown character %c [0x%02x], ignoring\n",
			read, read);
	    }
	}
    }

    fprintf(stderr, "File is missing drill End-Of-File\n");

    return image;
} /* parse_drillfile */

/* Guess the format of the input file.
   Rewinds file when done */
static void
drill_guess_format(gerb_file_t *fd, gerb_image_t *image)
{
    int inch_score = 0;
    int metric_score = 0;
    int length, max_length = 0;
    int leading_zeros, max_leading_zeros = 0;
    int trailing_zeros, max_trailing_zeros = 0;
    char read;
    drill_state_t *state = NULL;
    int done = FALSE;
    int i;

    state = new_state(state);
    if (state == NULL)
	err(1, "malloc state failed\n");

    image->format = (gerb_format_t *)malloc(sizeof(gerb_format_t));
    if (image->format == NULL) 
	err(1, "malloc format failed\n");
    memset((void *)image->format, 0, sizeof(gerb_format_t));

    /* This is just a special case of the normal parser */
    while ((read = gerb_fgetc(fd)) != EOF && !done) {
	switch (read) {
	case ';' :
	case 'F' :
	case 'G':
	case 'S':
	case 'T':
	    eat_line(fd);
	    break;
	case 'M':
	    switch(drill_parse_M_code(fd, image)) {
	    case DRILL_M_METRIC :
		metric_score += 10;
		break;
	    case DRILL_M_IMPERIAL :
		inch_score += 10;
		break;
	    case DRILL_M_END :
		done = TRUE;
	    default:
		break;
	    }
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
		while ((read = gerb_fgetc(fd)) != EOF &&
		       (isdigit((int)read) || read == ',' || read == '.')) {
		    if(read != ',' && read != '.') length ++;
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
	    state->curr_section = DRILL_HEADER;
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

    /* A bit of a kludge (or maybe wild ass guess would be more correct)
       It seems to work, though */
    if(image->format->omit_zeros == LEADING &&
       image->format->x_dec <=3 && image->info->unit == INCH) {
	++image->format->x_dec ;
	++image->format->y_dec ;
    }

    /* Restore the necessary things back to their default state */
    for (i = 0; i < APERTURE_MAX; i++) {
	if (image->aperture[i] != NULL) {
	    free(image->aperture[i]);
	    image->aperture[i] = NULL;
	}
    }

    fd->ptr = 0;
}


/*
 * Checks for signs that this is a drill file
 * Returns 1 if it is, 0 if not.
 */
int
drill_file_p(gerb_file_t *fd)
{
    char read[2];

    while ((read[0] = gerb_fgetc(fd)) != EOF) {
        switch (read[0]) {
        case 'M':
            if(gerb_fgetc(fd) == '4') {
		if(gerb_fgetc(fd) == '8') {
                /* Drill file header command found,
                   this could very well be a drill file */
		fd->ptr = 0;
                return 1;
		}
	    }
	    eat_line(fd);
            break;
	case 'T':
	    /* Something that looks like a tool definition found.
	       Special case to allow files from Orcad, which sucks. */
	    fd->ptr = 0;
	    return 1;
	case 'X':
	case 'Y':
	    /* First coordinate reached without finding any sign of this
	       being a drill file. Stop parsing and return. */
	    fd->ptr = 0;
	    return 0;
        default :
            eat_line(fd);
            break;
        }
    }
    /* This is, in all likelyhood, not a drill file */
    fd->ptr = 0;
    return 0;
}


/* Parse tool definition. This can get a bit tricky since it can
   appear in the header and/or data section.
   Returns tool number on success, -1 on error */
static int
drill_parse_T_code(gerb_file_t *fd, drill_state_t *state, gerb_image_t *image)
{
    int tool_num;
    int done = FALSE;
    char temp;
    double size;

    tool_num = gerb_fgetint(fd);
    if ((tool_num < TOOL_MIN) || (tool_num >= TOOL_MAX)) 
	err(1, "Tool out of bounds: %d\n", tool_num);

    /* Set the current tool to the correct one */
    state->current_tool = tool_num;

    /* Check for a size definition */
    temp = gerb_fgetc(fd);

    while(!done) {
	
	switch(temp) {
	case 'C':

	    size = read_double(fd, 1);

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
		       are defined in without sneaking a peek in the rest of
		       the file first. That's done in drill_guess_format() */
		    image->aperture[tool_num]->parameter[0] = size;
		    image->aperture[tool_num]->type = CIRCLE;
		    image->aperture[tool_num]->nuf_parameters = 1;
		}
/*		printf("Tool %02d size %2.4g found\n", tool_num, size); */
	    }
	    break;

	case 'F':
	case 'S' :
	    /* Silently ignored. They're not important. */
	    gerb_fgetint(fd);
	    break;

	default:
	    /* Stop when finding anything but what's expected
	       (and put it back) */
	    gerb_ungetc(fd);
	    done = TRUE;
	    break;
	}
	if( (temp = gerb_fgetc(fd)) == EOF) {
	    err(1, "(very) Unexpected end of file found\n");
	}
    }
    
    return tool_num;
} /* drill_parse_T_code */


static int
drill_parse_M_code(gerb_file_t *fd, gerb_image_t *image)
{
    char op[3] = "  ";

    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);

    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");

/*    printf("M code: %2s\n", op); */

    if (strncmp(op, "00", 2) == 0) {
	return DRILL_M_END;
    } else if (strncmp(op, "01", 2) == 0) {
	return DRILL_M_ENDPATTERN;
    } else if (strncmp(op, "18", 2) == 0) {
	return DRILL_M_TIPCHECK;
    } else if (strncmp(op, "25", 2) == 0 || strncmp(op, "31", 2) == 0) {
	return DRILL_M_BEGINPATTERN;
    } else if (strncmp(op, "30", 2) == 0) {
	return DRILL_M_ENDREWIND;
    } else if (strncmp(op, "45", 2) == 0) {
	return DRILL_M_LONGMESSAGE;
    } else if (strncmp(op, "47", 2) == 0) {
	return DRILL_M_MESSAGE;
    } else if (strncmp(op, "48", 2) == 0) {
	return DRILL_M_HEADER;
    } else if (strncmp(op, "71", 2) == 0) {
	eat_line(fd);
	return DRILL_M_METRIC;
    } else if (strncmp(op, "72", 2) == 0) {
	eat_line(fd);
	return DRILL_M_IMPERIAL;
    } else if (strncmp(op, "95", 2) == 0) {
	return DRILL_M_ENDHEADER;
    } else if (strncmp(op, "97", 2) == 0 || strncmp(op, "98", 2) == 0) {
	return DRILL_M_CANNEDTEXT;
    }

    return DRILL_M_UNKNOWN;

} /* drill_parse_M_code */

static int
drill_parse_G_code(gerb_file_t *fd, gerb_image_t *image)
{
    char op[3] = "  ";

    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);

    if ((op[0] == EOF) || (op[1] == EOF))
	err(1, "Unexpected EOF found.\n");

/*    printf("G code: %2s\n", op); */

    if (strncmp(op, "00", 2) == 0) {
	return DRILL_G_ROUT;
    } else if (strncmp(op, "01", 2) == 0) {
	return DRILL_G_LINEARMOVE;
    } else if (strncmp(op, "02", 2) == 0) {
	return DRILL_G_CWMOVE;
    } else if (strncmp(op, "03", 2) == 0) {
	return DRILL_G_CCWMOVE;
    } else if (strncmp(op, "05", 2) == 0) {
	return DRILL_G_DRILL;
    } else if (strncmp(op, "90", 2) == 0) {
	return DRILL_G_ABSOLUTE;
    } else if (strncmp(op, "91", 2) == 0) {
	return DRILL_G_INCREMENTAL;
    } else if (strncmp(op, "93", 2) == 0) {
	return DRILL_G_ZEROSET;
    }
    return DRILL_G_UNKNOWN;

} /* drill_parse_G_code */


/* Parse on drill file coordinate.
   Returns nothing, but modifies state */
static void
drill_parse_coordinate(gerb_file_t *fd, char firstchar,
		       double scale_factor, drill_state_t *state)

{
    char read;

    if(state->coordinate_mode == DRILL_MODE_ABSOLUTE) {
	if(firstchar == 'X') {
	    state->curr_x = read_double(fd, scale_factor);
	    if((read = gerb_fgetc(fd)) == 'Y') {
		state->curr_y = read_double(fd, scale_factor);
	    }
	} else {
	    state->curr_y = read_double(fd, scale_factor);
	}
    } else if(state->coordinate_mode == DRILL_MODE_INCREMENTAL) {
	if(firstchar == 'X') {
	    state->curr_x += read_double(fd, scale_factor);
	    if((read = gerb_fgetc(fd)) == 'Y') {
		state->curr_y += read_double(fd, scale_factor);
	    }
	} else {
	    state->curr_y += read_double(fd, scale_factor);
	}
    }

} /* drill_parse_coordinate */


/* Allocates and returns a new drill_state structure
   Returns state pointer on success, NULL on ERROR */
static drill_state_t *
new_state(drill_state_t *state)
{
    state = (drill_state_t *)malloc(sizeof(drill_state_t));
    if (state != NULL) {
	/* Init structure */
	memset((void *)state, 0, sizeof(drill_state_t));
	state->curr_section = DRILL_NONE;
	state->coordinate_mode = DRILL_MODE_ABSOLUTE;
	state->origin_x = 0.0;
	state->origin_y = 0.0;
    }
    return state;
} /* new_state */


/* Reads one double from fd and returns it.
   If a decimal point is found, the scale factor is not used. */
/* Too suspect. To be removed or improved. spe */
static double
read_double(gerb_file_t *fd, double scale_factor)
{
    char read;
    char temp[0x20];
    int i = 0;
    double result = 0;
    int decimal_point = FALSE;

    memset(temp, 0, sizeof(temp));

    read = gerb_fgetc(fd);
    while(read != EOF && i < sizeof(temp) &&
	  (isdigit((int)read) || read == '.' || read == '+' || read == '-')) {
	if(read == ',' || read == '.') decimal_point = TRUE;
	temp[i++] = read;
	read = gerb_fgetc(fd);
    }

    gerb_ungetc(fd);
    result = strtod(temp, NULL);
    if(!decimal_point) result *= scale_factor;

    return result;
} /* read_double */


/* Eats all characters up to and including 
   the first one of CR or LF */
static void
eat_line(gerb_file_t *fd)
{
    char read = gerb_fgetc(fd);
    
    while(read != 10 && read != 13) {
	if (read == EOF) return;
	read = gerb_fgetc(fd);
    }
} /* eat_line */
