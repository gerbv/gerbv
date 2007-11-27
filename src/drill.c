/*
 * gEDA - GNU Electronic Design Automation
 * drill.c
 * Copyright (C) 2000-2006 Andreas Andersson
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*21 Feb 2007 patch for metric drill files:
  1) METRIC/INCH commands (partly) parsed to define units of the header
  2) units of the header and the program body are independent
  3) ICI command parsed in the header
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

#include "common.h"
#include "drill.h"
#include "gerb_error.h"
#include "tooltable.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

#define NOT_IMPL(fd, s) do { \
                             GERB_MESSAGE("Not Implemented:%s\n", s); \
                           } while(0)

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
		     DRILL_M_CANNEDTEXT, DRILL_M_TIPCHECK, 
		     DRILL_M_METRICHEADER, DRILL_M_IMPERIALHEADER};

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
    enum unit_t unit, header_unit;
} drill_state_t;

/* Local function prototypes */
static enum unit_t drill_guess_format(gerb_file_t *fd, gerb_image_t *image);
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
    int read;
    double x_scale = 1, y_scale = 1;

    state = new_state(state);
    if (state == NULL)
	GERB_FATAL_ERROR("malloc state failed\n");

    image = new_gerb_image(image);
    if (image == NULL)
	GERB_FATAL_ERROR("malloc image failed\n");
    curr_net = image->netlist;

    state->unit = drill_guess_format(fd, image);
    state->header_unit = state->unit;

    dprintf ("%s:  drill_guess_format gave %s\n", __FUNCTION__,
            state->unit == MM ? "mm" : "inch");

    if (image && image->format ){
	x_scale = pow(10.0, (double)image->format->x_dec);
	y_scale = pow(10.0, (double)image->format->y_dec);
    }

    while ((read = gerb_fgetc(fd)) != EOF) {

	switch ((char)read) {
	case ';' :
	    /* Comment found. Eat rest of line */
	    eat_line(fd);
	    break;
	case 'F' :
	    /* Z axis feed speed. Silently ignored */
	    eat_line(fd);
	    break;
	case 'G':
	    /* Most G codes aren't used, for now */
	    switch(drill_parse_G_code(fd, image)) {
	    case DRILL_G_ROUT :
		GERB_COMPILE_ERROR("Rout mode data is not supported\n");
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
		    GERB_COMPILE_ERROR("Unexpected EOF\n");
		drill_parse_coordinate(fd, (char)read, 1.0 / x_scale, state);
		state->origin_x = state->curr_x;
		state->origin_y = state->curr_y;
		break;
	    default :
		eat_line(fd);
		break;
	    }
	    break;
	case 'I':
	    if (state->curr_section != DRILL_HEADER) break;
	    {int c = gerb_fgetc(fd);
	     	switch (c) {
	      	case 'N':
		    if ('C' == gerb_fgetc(fd)) 
		    if ('H' == gerb_fgetc(fd)) {
			eat_line(fd);
			state->header_unit = INCH;
		    }
		    break;
	      	case 'C':
		    if ('I' == gerb_fgetc(fd)) 
		    if (',' == gerb_fgetc(fd)) 
		    if ('O' == gerb_fgetc(fd)) { 
			if ('N' == (c = gerb_fgetc(fd)))
			    state->coordinate_mode = DRILL_MODE_INCREMENTAL;
			else if ('F' == c) if ('F' == gerb_fgetc(fd)) 
			    state->coordinate_mode = DRILL_MODE_ABSOLUTE;
			eat_line(fd);
		    }
		    break;
		}
	    }
	    eat_line(fd);
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
		return image;
		break;
	    case DRILL_M_METRICHEADER :
		state->header_unit = MM;
		break;
	    default:
		GERB_COMPILE_ERROR("Strange M code found.\n");
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
	    if (curr_net->next == NULL)
		GERB_FATAL_ERROR("malloc curr_net->next failed\n");
	    curr_net = curr_net->next;
	    memset((void *)curr_net, 0, sizeof(gerb_net_t));

	    curr_net->start_x = (double)state->curr_x;
	    curr_net->start_y = (double)state->curr_y;
	    /* KLUDGE. This function isn't allowed to return anything
	       but inches */
	    if(state->unit == MM) {
		curr_net->start_x /= 25.4;
		curr_net->start_y /= 25.4;
		/* KLUDGE. All images, regardless of input format,
		   are returned in INCH format */
		curr_net->unit = INCH;
	    }

	    curr_net->stop_x = curr_net->start_x - state->origin_x;
	    curr_net->stop_y = curr_net->start_y - state->origin_y;
	    curr_net->aperture = state->current_tool;
	    curr_net->aperture_state = FLASH;

	    /* Find min and max of image.
	       Mustn't forget (again) to add the hole radius */

 
	    /* Check if aperture is set. Ignore the below instead of
	       causing SEGV... */
	    if(image->aperture[state->current_tool] == NULL)
		break;

	    image->info->min_x =
		min(image->info->min_x,
		    (curr_net->start_x -
		     image->aperture[state->current_tool]->parameter[0] / 2));
	    image->info->min_y =
		min(image->info->min_y,
		    (curr_net->start_y -
		     image->aperture[state->current_tool]->parameter[0] / 2));
	    image->info->max_x =
		max(image->info->max_x,
		    (curr_net->start_x +
		     image->aperture[state->current_tool]->parameter[0] / 2));
	    image->info->max_y =
		max(image->info->max_y,
		    (curr_net->start_y +
		     image->aperture[state->current_tool]->parameter[0] / 2));
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
		GERB_COMPILE_ERROR(
			"Warning: Found ill fitting character '%c' [0x%02x] inside data, ignoring\n",
			read, read);
	    }
	}
    }

    GERB_COMPILE_ERROR("Warning: File is missing drill End-Of-File\n");

    free(state);

    return image;
} /* parse_drillfile */


/* Guess the format of the input file. Rewinds file when done */
static enum unit_t
drill_guess_format(gerb_file_t *fd, gerb_image_t *image)
{
    int inch_score = 0;
    int metric_score = 0;
    int length, max_length = 0;
    int leading_zeros, max_leading_zeros = 0;
    int trailing_zeros, max_trailing_zeros = 0;
    int t_precision = 0;
    int read;
    drill_state_t *state = NULL;
    int done = FALSE;
    int i;
    enum unit_t unit;

    dprintf ("drill_guess_format(%p, %p)\n", fd, image);

    state = new_state(state);
    if (state == NULL)
	GERB_FATAL_ERROR("malloc state failed\n");

    image->format = (gerb_format_t *)malloc(sizeof(gerb_format_t));
    if (image->format == NULL) 
	GERB_FATAL_ERROR("malloc format failed\n");
    memset((void *)image->format, 0, sizeof(gerb_format_t));

    /* This is just a special case of the normal parser */
    while ((read = gerb_fgetc(fd)) != EOF && !done) {
        dprintf( "%s:  read = \'%c\'\n", __FUNCTION__, read);
	switch ((char)read) {
	case ';' :
	case 'F' :
	case 'G':
	case 'S':
	    eat_line(fd);
	    break;

	case 'T':
            while (1) {
                read = gerb_fgetc(fd);
                if (!((read == 'C') || ((read >= '0') && (read <= '9'))))
		    break;
            }
            if (read == '.') {
                t_precision = 0;
                while (1) {
                    read = gerb_fgetc(fd);
                    if ((read >= '0') && (read <= '9')) {
                        t_precision++;
                    } else {
                        break;
                    }
                }
            }
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
		       (isdigit(read) || read == '+' || read == '-'
			|| read == ',' || read == '.')) {
		    if ((read != '+') && (read != '-') && 
			(read != ',') && (read != '.')) 
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
    dprintf ("%s: metric_score = %d, inch_score = %d\n", __FUNCTION__,
            metric_score, inch_score);

    if(metric_score > inch_score) {
	unit = MM;
    } else {
	unit = INCH;
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

    if (t_precision > 0) {
        /* 
	 * If drill sizes were specified with a T command, 
	 * assume precision is the same 
	 */
        image->format->x_dec = t_precision;
        image->format->y_dec = t_precision;
    } else {
        /* Almost every file seems to use 2.x format (where x is 3-4) */
        image->format->x_dec = max_length - 2;
        image->format->y_dec = max_length - 2;
    }


    /* A bit of a kludge (Or maybe wild ass guess would be more correct.
       It seems to work, though.) It tries to cover all cases I've
       found where the format has to be adjusted from the above
       calculation. */
    if( (image->format->omit_zeros == LEADING ||
	 (max_leading_zeros == 0 && max_trailing_zeros == 0) ) &&
	image->format->x_dec <=3 && unit == INCH) {

	image->format->x_dec += 1;
	image->format->y_dec += 1;
    }

    /* Restore the necessary things back to their default state */
    for (i = 0; i < APERTURE_MAX; i++) {
	if (image->aperture[i] != NULL) {
	    free(image->aperture[i]);
	    image->aperture[i] = NULL;
	}
    }

    free(state);
    fd->ptr = 0;

    return unit;
}


/*
 * Checks for signs that this is a drill file
 * Returns 1 if it is, 0 if not.
 */
int
drill_file_p(gerb_file_t *fd)
{
    int read;

    while ((read = gerb_fgetc(fd)) != EOF) {
        switch ((char)read) {
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
	case 10 :  /* Ignore CR/LF */
	case 13 :
	    break;
        default : /* Eat every line that starts with something uninteresting */
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
    int temp;
    double size;

    /* Sneak a peek at what's hiding after the 'T'. Ugly fix for
       broken headers from Orcad, which is crap */
    temp = gerb_fgetc(fd);
    if( !(isdigit(temp) != 0 || temp == '+' || temp =='-') ) {
	if(temp != EOF) {
	    GERB_COMPILE_ERROR(
		    "Warning: Junk text found in place of tool definition.\n"
		    "         Please ask your CAD vendor to stop doing that.\n");
	    eat_line(fd);
	}
	return -1;
    }
    gerb_ungetc(fd);

    tool_num = gerb_fgetint(fd, NULL);
    if ((tool_num < TOOL_MIN) || (tool_num >= TOOL_MAX)) 
	GERB_COMPILE_ERROR("Tool out of bounds: %d\n", tool_num);

    /* Set the current tool to the correct one */
    dprintf ("%s: tool_num = %d\n", __FUNCTION__, tool_num);
    state->current_tool = tool_num;

    /* Check for a size definition */
    temp = gerb_fgetc(fd);

    while(!done) {
	
	switch((char)temp) {
	case 'C':

	    size = read_double(fd, 1);

	    if(state->header_unit == MM) {
            dprintf ("%s: Read a size of %g %s\n", __FUNCTION__, size,
                    state->header_unit == MM ? "mm" : "inch");
		size /= 25.4;
	    } else if(size >= 4.0) {
		/* If the drill size is >= 4 inches, assume that this
		   must be wrong and that the units are mils.
		   The limit being 4 inches is because the smallest drill
		   I've ever seen used is 0,3mm(about 12mil). Half of that
		   seemed a bit too small a margin, so a third it is */
		GERB_COMPILE_WARNING("Read a tool diameter of %g. ", size);
		GERB_COMPILE_WARNING("Assuming this is mils and your CAD tool is broken.\n");
		size /= 1000.0;
	    }

	    if(size <= 0 || size >= 10000) {
		GERB_COMPILE_ERROR("Tool is wrong size: %g\n", size);
	    } else {
		if(image->aperture[tool_num] != NULL) {
		    GERB_COMPILE_ERROR("Tool is already defined\n");
		} else {
		    image->aperture[tool_num] =
			(gerb_aperture_t *)malloc(sizeof(gerb_aperture_t));
		    if (image->aperture[tool_num] == NULL) {
			GERB_FATAL_ERROR("malloc tool failed\n");
		    }
		    /* There's really no way of knowing what unit the tools
		       are defined in without sneaking a peek in the rest of
		       the file first. That's done in drill_guess_format() */
		    image->aperture[tool_num]->parameter[0] = size;
		    image->aperture[tool_num]->type = CIRCLE;
		    image->aperture[tool_num]->nuf_parameters = 1;
		    image->aperture[tool_num]->unit = INCH;
		}
	    }
	    break;

	case 'F':
	case 'S' :
	    /* Silently ignored. They're not important. */
	    gerb_fgetint(fd, NULL);
	    break;

	default:
	    /* Stop when finding anything but what's expected
	       (and put it back) */
	    gerb_ungetc(fd);
	    done = TRUE;
	    break;
	}
	if( (temp = gerb_fgetc(fd)) == EOF) {
	    GERB_COMPILE_ERROR("(very) Unexpected end of file found\n");
	}
    }

    /* Catch the tools that aren't defined.
       This isn't strictly a good thing, but at least something is shown */
    if(image->aperture[tool_num] == NULL) {
        double dia;

	image->aperture[tool_num] =
	    (gerb_aperture_t *)malloc(sizeof(gerb_aperture_t));
	if (image->aperture[tool_num] == NULL) {
	    GERB_FATAL_ERROR("malloc tool failed\n");
	}

        /* See if we have the tool table */
        dia = GetToolDiameter_Inches(tool_num);
        if (dia <= 0) {
            /*
             * There is no tool. So go out and make some.
             * This size calculation is, of course, totally bogus.
             */
            dia = (double)(16 + 8 * tool_num) / 1000;
            /*
             * Oooh, this is sooo ugly. But some CAD systems seem to always
             * use T00 at the end of the file while others that don't have
             * tool definitions inside the file never seem to use T00 at all.
             */
            if(tool_num != 0) {
                GERB_COMPILE_ERROR(
		    "Warning: Tool %02d used without being defined\n",
		    tool_num);
                GERB_COMPILE_ERROR("Setting a default size of %g\"\n", dia);
            }
	}

	image->aperture[tool_num]->type = CIRCLE;
	image->aperture[tool_num]->nuf_parameters = 1;
        image->aperture[tool_num]->parameter[0] = dia;
    }
    
    return tool_num;
} /* drill_parse_T_code */


static int
drill_parse_M_code(gerb_file_t *fd, gerb_image_t *image)
{
    char op[3] = "  ";
    int  read[3];

    read[0] = gerb_fgetc(fd);
    read[1] = gerb_fgetc(fd);

    if ((read[0] == EOF) || (read[1] == EOF))
	GERB_COMPILE_WARNING("Unexpected EOF found.\n");

    op[0] = read[0], op[1] = read[1], op[2] = 0;
 
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
    } else if (strncmp(op, "ET", 2) == 0) {
	if ('R' == gerb_fgetc(fd)) 
	if ('I' == gerb_fgetc(fd)) 
	if ('C' == gerb_fgetc(fd)) {
		eat_line(fd);
		return DRILL_M_METRICHEADER;
	}
    } 
    return DRILL_M_UNKNOWN;

} /* drill_parse_M_code */

static int
drill_parse_G_code(gerb_file_t *fd, gerb_image_t *image)
{
    char op[3] = "  ";
    int  read[3];

    read[0] = gerb_fgetc(fd);
    read[1] = gerb_fgetc(fd);

    if ((read[0] == EOF) || (read[1] == EOF))
	GERB_COMPILE_ERROR("Unexpected EOF found.\n");

    op[0] = read[0], op[1] = read[1], op[2] = 0;

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
	    if((read = (char)gerb_fgetc(fd)) == 'Y') {
		state->curr_y = read_double(fd, scale_factor);
	    }
	} else {
	    state->curr_y = read_double(fd, scale_factor);
	}
    } else if(state->coordinate_mode == DRILL_MODE_INCREMENTAL) {
	if(firstchar == 'X') {
	    state->curr_x += read_double(fd, scale_factor);
	    if((read = (char)gerb_fgetc(fd)) == 'Y') {
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
	state->unit = INCH;
	state->header_unit = INCH;
    }
    return state;
} /* new_state */


/* Reads one double from fd and returns it.
   If a decimal point is found, the scale factor is not used. */
static double
read_double(gerb_file_t *fd, double scale_factor)
{
    int read;
    char temp[0x20];
    int i = 0;
    double result = 0;
    int decimal_point = FALSE;

    memset(temp, 0, sizeof(temp));

    read = gerb_fgetc(fd);
    while(read != EOF && i < sizeof(temp) &&
	  (isdigit(read) || read == '.' || read == '+' || read == '-')) {
	if(read == ',' || read == '.') decimal_point = TRUE;
	temp[i++] = (char)read;
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
    int read = gerb_fgetc(fd);
    
    while(read != 10 && read != 13) {
	if (read == EOF) return;
	read = gerb_fgetc(fd);
    }
} /* eat_line */
