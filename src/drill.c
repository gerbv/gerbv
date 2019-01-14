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

/** \file drill.c
    \brief Excellon drill parsing functions
    \ingroup libgerbv
*/

/*
 * 21 Feb 2007 patch for metric drill files:
 * 1) METRIC/INCH commands (partly) parsed to define units of the header
 * 2) units of the header and the program body are independent
 * 3) ICI command parsed in the header
 */

#include "gerbv.h"

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

#include "attribute.h"
#include "common.h"
#include "drill.h"
#include "drill_stats.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

#define MAXL 200
#define DRILL_READ_DOUBLE_SIZE 32

typedef enum {
    DRILL_NONE, DRILL_HEADER, DRILL_DATA
} drill_file_section_t;

typedef enum {
    DRILL_MODE_ABSOLUTE, DRILL_MODE_INCREMENTAL
} drill_coordinate_mode_t;

typedef enum {
    FMT_00_0000	/* INCH */,
    FMT_000_000	/* METRIC 6-digit, 1 um */,
    FMT_000_00	/* METRIC 5-digit, 10 um */,
    FMT_0000_00	/* METRIC 6-digit, 10 um */,
    FMT_USER	/* User defined format */
} number_fmt_t;

typedef struct drill_state {
    double curr_x;
    double curr_y;
    int current_tool;
    drill_file_section_t curr_section;
    drill_coordinate_mode_t coordinate_mode;
    double origin_x;
    double origin_y;
    gerbv_unit_t unit;
    /* number_format is used throughout the file itself.

       header_number_format is used to parse the tool definition C
       codes within the header.  It is fixed to FMT_00_0000 for INCH
       measures, and FMT_000_000 (1 um resolution) for metric
       measures. */
    number_fmt_t number_format, header_number_format;
    /* Used as a backup when temporarily switching to INCH. */
    number_fmt_t backup_number_format;

    /* 0 means we don't try to autodetect any of the other values */
    int autod;
    
    /* in FMT_USER this specifies the number of digits before the
     * decimal point when doing trailing zero suppression.  Otherwise
     * it is the number of digits *after* the decimal 
     * place in the file
     */
    int decimals;

} drill_state_t;

/* Local function prototypes */
static drill_g_code_t drill_parse_G_code(gerb_file_t *fd,
				gerbv_image_t *image, ssize_t file_line);
static drill_m_code_t drill_parse_M_code(gerb_file_t *fd, drill_state_t *state,
				gerbv_image_t *image, ssize_t file_line);
static int drill_parse_T_code(gerb_file_t *fd, drill_state_t *state,
				gerbv_image_t *image, ssize_t file_line);
static int drill_parse_header_is_metric(gerb_file_t *fd, drill_state_t *state,
				gerbv_image_t *image, ssize_t file_line);
static int drill_parse_header_is_inch(gerb_file_t *fd, drill_state_t *state,
				gerbv_image_t *image, ssize_t file_line);
static int drill_parse_header_is_ici(gerb_file_t *fd, drill_state_t *state,
				gerbv_image_t *image, ssize_t file_line);
static void drill_parse_coordinate(gerb_file_t *fd, char firstchar,
				gerbv_image_t *image, drill_state_t *state,
				ssize_t file_line);
static drill_state_t *new_state(drill_state_t *state);
static double read_double(gerb_file_t *fd, number_fmt_t fmt,
				gerbv_omit_zeros_t omit_zeros, int decimals);
static void eat_line(gerb_file_t *fd);
static char *get_line(gerb_file_t *fd);
static int file_check_str(gerb_file_t *fd, const char *str);

/* -------------------------------------------------------------- */
/* This is the list of specific attributes a drill file may have from
 * the point of view of parsing it.
 */

enum {
    SUP_NONE = 0,
    SUP_LEAD,
    SUP_TRAIL
};

static const char *suppression_list[] = {
    N_("None"),
    N_("Leading"),
    N_("Trailing"),
    0
};

static const char *units_list[] = {
    N_("inch"),
    N_("mm"),
    0
};

enum {
    HA_auto = 0,
    HA_suppression,
    HA_xy_units,
    HA_digits,
#if 0
    HA_tool_units,
#endif
};

static gerbv_HID_Attribute drill_attribute_list[] = {
    /* This should be first */
  {N_("autodetect"), N_("Try to autodetect the file format"),
   HID_Boolean, 0, 0, {1, 0, 0}, 0, 0},

  {N_("zero_suppression"), N_("Zero suppression"),
   HID_Enum, 0, 0, {0, 0, 0}, suppression_list, 0},

  {N_("units"), N_("Length units"),
   HID_Enum, 0, 0, {0, 0, 0}, units_list, 0},

  {N_("digits"), N_("Number of digits.  For trailing zero suppression,"
   " this is the number of digits before the decimal point.  "
   "Otherwise this is the number of digits after the decimal point."),
   HID_Integer, 0, 20, {5, 0, 0}, 0, 0},

#if 0
  {"tool_units", "Tool size units",
   HID_Enum, 0, 0, {0, 0, 0}, units_list, 0},
#endif
};

void
drill_attribute_merge (gerbv_HID_Attribute *dest, int ndest, gerbv_HID_Attribute *src, int nsrc)
{
    int i, j;

    /* Here is a brain dead merge algorithm which should make anyone cringe.
     * Still, it is simple and we won't merge many attributes and not
     * many times either.
     */

    for (i = 0 ; i < nsrc ; i++) {
	/* see if our destination wants this attribute */
	j = 0;
	while (j < ndest && strcmp (src[i].name, dest[j].name) != 0)
	    j++;

	/* if we wanted it and it is the same type, copy it over */
	if (j < ndest && src[i].type == dest[j].type) {
	    dest[j].default_val = src[i].default_val;
	} else {
	    GERB_MESSAGE("Ignoring \"%s\" attribute for drill file", src[i].name);
	}
    }
}

static void
drill_update_image_info_min_max_from_bbox(gerbv_image_info_t *info,
		const gerbv_render_size_t *bbox)
{
    info->min_x = MIN(info->min_x, bbox->left);
    info->min_y = MIN(info->min_y, bbox->bottom);
    info->max_x = MAX(info->max_x, bbox->right);
    info->max_y = MAX(info->max_y, bbox->top);
}

/*
 * Adds the actual drill hole to the drawing 
 */
static gerbv_net_t *
drill_add_drill_hole (gerbv_image_t *image, drill_state_t *state,
		gerbv_drill_stats_t *stats, gerbv_net_t *curr_net)
{
    gerbv_render_size_t *bbox;
    double r;

    /* Add one to drill stats  for the current tool */
    drill_stats_increment_drill_counter(image->drill_stats->drill_list,
	    state->current_tool);

    curr_net->next = g_new0(gerbv_net_t, 1);
    if (curr_net->next == NULL)
	GERB_FATAL_ERROR("malloc curr_net->next failed in %s()",
			__FUNCTION__);

    curr_net = curr_net->next;
    curr_net->layer = image->layers;
    curr_net->state = image->states;
    curr_net->start_x = state->curr_x;
    curr_net->start_y = state->curr_y;
    /* KLUDGE. This function isn't allowed to return anything
       but inches */
    if(state->unit == GERBV_UNIT_MM) {
	curr_net->start_x /= 25.4;
	curr_net->start_y /= 25.4;
	/* KLUDGE. All images, regardless of input format,
	   are returned in INCH format */
	curr_net->state->unit = GERBV_UNIT_INCH;
    }

    curr_net->stop_x = curr_net->start_x - state->origin_x;
    curr_net->stop_y = curr_net->start_y - state->origin_y;
    curr_net->aperture = state->current_tool;
    curr_net->aperture_state = GERBV_APERTURE_STATE_FLASH;

    /* Check if aperture is set. Ignore the below instead of
       causing SEGV... */
    if(image->aperture[state->current_tool] == NULL)
	return curr_net;

    bbox = &curr_net->boundingBox;
    r = image->aperture[state->current_tool]->parameter[0] / 2;

    /* Set boundingBox */
    bbox->left   = curr_net->start_x - r;
    bbox->right  = curr_net->start_x + r;
    bbox->bottom = curr_net->start_y - r;
    bbox->top    = curr_net->start_y + r;

    drill_update_image_info_min_max_from_bbox(image->info, bbox);

    return curr_net;
}

/* -------------------------------------------------------------- */
gerbv_image_t *
parse_drillfile(gerb_file_t *fd, gerbv_HID_Attribute *attr_list, int n_attr, int reload)
{
    drill_state_t *state = NULL;
    gerbv_image_t *image = NULL;
    gerbv_net_t *curr_net = NULL;
    gerbv_HID_Attribute *hid_attrs;
    int read;
    gerbv_drill_stats_t *stats;
    gchar *tmps;
    ssize_t file_line = 1;

    /* 
     * many locales redefine "." as "," and so on, so sscanf and strtod 
     * has problems when reading files using %f format.
     * Fixes bug #1963618 reported by Lorenzo Marcantonio.
     */
    setlocale(LC_NUMERIC, "C" );

    /* Create new image for this layer */
    dprintf("In parse_drillfile, about to create image for this layer\n");

    image = gerbv_create_image(image, "Excellon Drill File");
    if (image == NULL)
	GERB_FATAL_ERROR("malloc image failed in %s()", __FUNCTION__);

    if (reload && attr_list != NULL) {
      /* FIXME there should probably just be a function to copy an
	 attribute list including using strdup as needed */

	image->info->n_attr = n_attr;
	image->info->attr_list = gerbv_attribute_dup(attr_list, n_attr);

    } else {
	/* Copy in the default attribute list for drill files.  We make a
	 * copy here because we will allow per-layer editing of the
	 * attributes.
	 */
	image->info->n_attr = sizeof (drill_attribute_list) / sizeof (drill_attribute_list[0]);
	image->info->attr_list = gerbv_attribute_dup (drill_attribute_list, image->info->n_attr);

	/* now merge any project attributes */
	drill_attribute_merge (image->info->attr_list, image->info->n_attr,
			 attr_list, n_attr);
    }
    
    curr_net = image->netlist;
    curr_net->layer = image->layers;
    curr_net->state = image->states;
    image->layertype = GERBV_LAYERTYPE_DRILL;
    stats = gerbv_drill_stats_new();
    if (stats == NULL)
	GERB_FATAL_ERROR("malloc stats failed in %s()", __FUNCTION__);
    image->drill_stats = stats;

    /* Create local state variable to track photoplotter state */
    state = new_state(state);
    if (state == NULL)
	GERB_FATAL_ERROR("malloc state failed in %s()", __FUNCTION__);

    image->format = g_new0(gerbv_format_t, 1);
    if (image->format == NULL)
	GERB_FATAL_ERROR("malloc format failed in %s()", __FUNCTION__);

    image->format->omit_zeros = GERBV_OMIT_ZEROS_UNSPECIFIED;

    hid_attrs = image->info->attr_list;

    if (!hid_attrs[HA_auto].default_val.int_value) {
	state->autod = 0;
	state->number_format = FMT_USER;
	state->decimals = hid_attrs[HA_digits].default_val.int_value;

	if (GERBV_UNIT_MM == hid_attrs[HA_xy_units].default_val.int_value)
	    state->unit = GERBV_UNIT_MM;

	switch (hid_attrs[HA_suppression].default_val.int_value) {
	case SUP_LEAD:
	    image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
	    break;
	    
	case SUP_TRAIL:
	    image->format->omit_zeros = GERBV_OMIT_ZEROS_TRAILING;
	    break;

	default:
	    image->format->omit_zeros = GERBV_OMIT_ZEROS_EXPLICIT;
	    break;
	}
    }

    dprintf("%s():  Starting parsing of drill file \"%s\"\n",
		    __FUNCTION__, fd->filename);

    while ((read = gerb_fgetc(fd)) != EOF) {

	switch ((char) read) {

	case ';' :
	    /* Comment found. Eat rest of line */
	    tmps = get_line(fd);
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_NOTE, -1,
		    _("Comment \"%s\" at line %ld in file \"%s\""),
		    tmps, file_line, fd->filename);
	    dprintf("    Comment with ';' \"%s\" at line %ld\n",
		    tmps, file_line);
	    g_free(tmps);
	    break;

	case 'D' :
	    gerb_ungetc (fd);
	    tmps = get_line (fd);
	    if (strcmp (tmps, "DETECT,ON") == 0 ||
		strcmp (tmps, "DETECT,OFF") == 0) {
		gchar *tmps2;
		gchar *tmps3;
		if (strcmp (tmps, "DETECT,ON") == 0)
		    tmps3 = "ON";
		else
		    tmps3 = "OFF";

		/* broken tool detect on/off.  Silently ignored. */
		if (stats->detect) {
		    tmps2 = g_strdup_printf ("%s\n%s", stats->detect, tmps3);
		    g_free (stats->detect);
		} else {
		    tmps2 = g_strdup_printf ("%s", tmps3);
		}
		stats->detect = tmps2;
	    } else {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unrecognised string \"%s\" in header "
			    "at line %ld in file \"%s\""),
			tmps, file_line, fd->filename);
	    }
	    g_free (tmps);
	    break;

	case 'F' :
	    gerb_ungetc (fd);
	    tmps = get_line (fd);

	    /* Silently ignore FMAT,2.  Not sure what others are allowed */
	    if (0 == strcmp (tmps, "FMAT,2")) {
		g_free (tmps);
		break;
	    }

	    if (0 == strcmp (tmps, "FMAT,1")) {
		gerbv_stats_printf(stats->error_list,
			GERBV_MESSAGE_ERROR, -1,
			_("File in unsupported format 1 "
			    "at line %ld in file \"%s\""),
			file_line, fd->filename);

		gerbv_destroy_image(image);
		g_free (tmps);

		return NULL;
	    }
	    
	    gerbv_stats_printf(stats->error_list,
			    GERBV_MESSAGE_ERROR, -1,
			    _("Unrecognised string \"%s\" in header "
				    "at line %ld in file \"%s\""),
			    tmps, file_line, fd->filename);

	    g_free (tmps);
	    break;

	case 'G': {
	    /* Most G codes aren't used, for now */
	    drill_g_code_t  g_code;

	    switch (g_code = drill_parse_G_code(fd, image, file_line)) {

	    case DRILL_G_DRILL :
		/* Drill mode */
		break;

	    case DRILL_G_SLOT : {
		/* Parse drilled slot end coords */
		gerbv_render_size_t *bbox = &curr_net->boundingBox;
		double r;

		if (EOF == (read = gerb_fgetc(fd))) {
		    gerbv_stats_printf(stats->error_list,
			    GERBV_MESSAGE_ERROR, -1,
			    _("Unexpected EOF found in file \"%s\""),
			    fd->filename);
		    break;
		}

		drill_parse_coordinate(fd, read, image, state, file_line);

		/* Modify last curr_net as drilled slot */
		curr_net->stop_x = state->curr_x;
		curr_net->stop_y = state->curr_y;

		r = image->aperture[state->current_tool]->parameter[0]/2;

		/* Update boundingBox with drilled slot stop_x,y coords */
		bbox->left   = MIN(bbox->left,   curr_net->stop_x - r);
		bbox->right  = MAX(bbox->right,  curr_net->stop_x + r);
		bbox->bottom = MIN(bbox->bottom, curr_net->stop_y - r);
		bbox->top    = MAX(bbox->top,    curr_net->stop_y + r);

		drill_update_image_info_min_max_from_bbox(image->info, bbox);

		if (state->unit == GERBV_UNIT_MM) {
		    /* Convert to inches -- internal units */
		    curr_net->stop_x /= 25.4;
		    curr_net->stop_y /= 25.4;
		}
		curr_net->aperture_state = GERBV_APERTURE_STATE_ON;

		break;
	    }

	    case DRILL_G_ABSOLUTE :
		state->coordinate_mode = DRILL_MODE_ABSOLUTE;
		break;

	    case DRILL_G_INCREMENTAL :
		state->coordinate_mode = DRILL_MODE_INCREMENTAL;
		break;

	    case DRILL_G_ZEROSET :
		if (EOF == (read = gerb_fgetc(fd))) {
		    gerbv_stats_printf(stats->error_list,
			    GERBV_MESSAGE_ERROR, -1,
			    _("Unexpected EOF found in file \"%s\""),
			    fd->filename);
		    break;
		}

		drill_parse_coordinate(fd, (char)read, image,
			state, file_line);
		state->origin_x = state->curr_x;
		state->origin_y = state->curr_y;
		break;

	    case DRILL_G_UNKNOWN:
		tmps = get_line(fd);
		gerbv_stats_printf(stats->error_list,
			GERBV_MESSAGE_ERROR, -1,
			_("Unrecognized string \"%s\" found "
			    "at line %ld in file \"%s\""),
			tmps, file_line, fd->filename);
		g_free(tmps);
		break;

	    default:
		eat_line(fd);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unsupported G%02d (%s) code "
			    "at line %ld in file \"%s\""),
			g_code, drill_g_code_name(g_code),
			file_line, fd->filename);
		break;
	    }

	    break;
	}

	case 'I':
	    gerb_ungetc(fd); /* To compare full string in function or
				report full string  */
	    if (drill_parse_header_is_inch(fd, state, image, file_line))
		break;

	    if (drill_parse_header_is_ici(fd, state, image, file_line))
		break;

	    tmps = get_line(fd);
	    gerbv_stats_printf(stats->error_list,
		    GERBV_MESSAGE_ERROR, -1,
		    _("Unrecognized string \"%s\" found "
			"at line %ld in file \"%s\""),
		    tmps, file_line, fd->filename);
	    g_free(tmps);

	    break;

	case 'M': {
	    int m_code;

	    switch (m_code = drill_parse_M_code(fd, state, image, file_line)) {
	    case DRILL_M_HEADER :
		state->curr_section = DRILL_HEADER;
		break;
	    case DRILL_M_HEADEREND :
		state->curr_section = DRILL_DATA;

		if (image->format->omit_zeros == GERBV_OMIT_ZEROS_UNSPECIFIED) {
		    /* Excellon says they default to specify leading
		       zeros, i.e. omit trailing zeros.	 The Excellon
		       files floating around that don't specify the
		       leading/trailing zeros in the header seem to
		       contradict to this though.

		       XXX We should probably ask the user. */

		    gerbv_stats_printf(stats->error_list,
			    GERBV_MESSAGE_ERROR, -1,
			    _("End of Excellon header reached "
				"but no leading/trailing zero "
				"handling specified "
				"at line %ld in file \"%s\""),
			    file_line, fd->filename);
		    gerbv_stats_printf(stats->error_list,
			    GERBV_MESSAGE_WARNING, -1,
			    _("Assuming leading zeros in file \"%s\""),
			    fd->filename);
		    image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
		}
		break;
	    case DRILL_M_METRIC :
		if (state->unit == GERBV_UNIT_UNSPECIFIED
		&&  state->curr_section != DRILL_HEADER) {
		    double size;

		    gerbv_stats_printf(stats->error_list,
			    GERBV_MESSAGE_WARNING, -1,
			    _("M71 code found but no METRIC "
				"specification in header "
				"at line %ld in file \"%s\""),
			    file_line, fd->filename);
		    gerbv_stats_printf(stats->error_list,
			    GERBV_MESSAGE_WARNING, -1,
			    _("Assuming all tool sizes are MM in file \"%s\""),
			    fd->filename);

		    stats = image->drill_stats;
		    for (int tool_num = TOOL_MIN; tool_num < TOOL_MAX;
			    tool_num++) {
			if (image->aperture && image->aperture[tool_num]) {
			    /* First update stats.   Do this before changing drill dias.
			     * Maybe also put error into stats? */
			    size = image->aperture[tool_num]->parameter[0];
			    drill_stats_modify_drill_list(stats->drill_list, 
							  tool_num, 
							  size, 
							  "MM");
			    /* Now go back and update all tool dias, since
			     * tools are displayed in inch units
			     */
			    image->aperture[tool_num]->parameter[0] /= 25.4;
			}
		    }
		}
		if (state->autod) {
		    state->number_format = state->backup_number_format;
		    state->unit = GERBV_UNIT_MM;
		}
		break;
	    case DRILL_M_IMPERIAL :
		if (state->autod) {
		    if (state->number_format != FMT_00_0000)
			/* save metric format definition for later */
			state->backup_number_format = state->number_format;
		    state->number_format = FMT_00_0000;
		    state->decimals = 4;
		    state->unit = GERBV_UNIT_INCH;
		}

		break;
	    case DRILL_M_CANNEDTEXTX :
	    case DRILL_M_CANNEDTEXTY :
		tmps = get_line(fd);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_NOTE, -1,
			_("Canned text \"%s\" "
			    "at line %ld in drill file \"%s\""),
			tmps, file_line, fd->filename);
		g_free(tmps);
		break;
	    case DRILL_M_MESSAGELONG :
	    case DRILL_M_MESSAGE :
		tmps = get_line(fd);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_NOTE, -1,
			_("Message \"%s\" embedded "
			    "at line %ld in drill file \"%s\""),
			tmps, file_line, fd->filename);
		g_free(tmps);
		break;
	    case DRILL_M_PATTERNEND :
	    case DRILL_M_TOOLTIPCHECK :
		break;

	    case DRILL_M_END :
		/* M00 has optional arguments */
		eat_line(fd);

	    case DRILL_M_ENDREWIND :
		goto drill_parse_end;
		break;

	    case DRILL_M_UNKNOWN:
		gerb_ungetc(fd); /* To compare full string in function or
				    report full string  */
		if (drill_parse_header_is_metric(fd, state, image, file_line))
		    break;

		stats->M_unknown++;
		tmps = get_line(fd);
		gerbv_stats_printf(stats->error_list,
			GERBV_MESSAGE_ERROR, -1,
			_("Unrecognized string \"%s\" found "
			    "at line %ld in file \"%s\""),
			tmps, file_line, fd->filename);
		g_free(tmps);

		break;

	    default:
		stats->M_unknown++;
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unsupported M%02d (%s) code found "
			    "at line %ld in file \"%s\""),
			m_code, _(drill_m_code_name(m_code)),
			file_line, fd->filename);
		break;
	    } /* switch(m_code) */

	    break;
	} /* case 'M' */

	case 'R':
	    if (state->curr_section == DRILL_HEADER) {
		stats->unknown++;
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Not allowed 'R' code in the header "
			    "at line %ld in file \"%s\""),
			file_line, fd->filename);
	    } else {
	      double start_x, start_y;
	      double step_x, step_y;
	      int c;
	      int rcnt;
	      /*
	       * This is the "Repeat hole" command.  Format is:
	       * R##[X##][Y##]
	       * This repeats the previous hole stepping in the X and
	       * Y increments give.  Example:
	       * R04X0.1 -- repeats the drill hole 4 times, stepping
	       * 0.1" in the X direction.  Note that the X and Y step
	       * sizes default to zero if not given and that they use
	       * the same format and units as the normal X,Y
	       * coordinates.
	       */
	      stats->R++;

	      start_x = state->curr_x;
	      start_y = state->curr_y;

	      /* figure out how many repeats there are */
	      c = gerb_fgetc (fd);
	      rcnt = 0;
	      while ( '0' <= c && c <= '9') {
		rcnt = 10*rcnt + (c - '0');
		c = gerb_fgetc (fd);
	      }
	      dprintf ("working on R code (repeat) with a number of reps equal to %d\n", rcnt);

	      step_x = 0.0;
	      if (c == 'X') {
		step_x = read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
		c = gerb_fgetc (fd);
	      }

	      step_y = 0.0;
	      if( c == 'Y') {
		  step_y = read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
	      } else {
		gerb_ungetc (fd);
	      }
	      
	      dprintf ("Getting ready to repeat the drill %d times with delta_x = %g, delta_y = %g\n", rcnt, step_x, step_y);

	      /* spit out the drills */
	      for (c = 1 ; c <= rcnt ; c++) {
		state->curr_x = start_x + c*step_x;
		state->curr_y = start_y + c*step_y;
		dprintf ("    Repeat #%d — new location is (%g, %g)\n", c, state->curr_x, state->curr_y);
		curr_net = drill_add_drill_hole (image, state, stats, curr_net);
	      }
	      
	    }

	case 'S':
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_NOTE, -1,
		    _("Ignoring setting spindle speed "
			"at line %ld in drill file \"%s\""),
		    file_line, fd->filename);
	    eat_line(fd);
	    break;
	case 'T':
	    drill_parse_T_code(fd, state, image, file_line);
	    break;
	case 'V' :
	    gerb_ungetc (fd);
	    tmps = get_line (fd);
	    /* Silently ignore VER,1.  Not sure what others are allowed */
	    if (0 != strcmp (tmps, "VER,1")) {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_NOTE, -1,
			_("Undefined string \"%s\" in header "
			    "at line %ld in file \"%s\""),
			tmps, file_line, fd->filename);
	    }
	    g_free (tmps);
	    break;

	case 'X':
	case 'Y':
	    /* Hole coordinate found. Do some parsing */
	    drill_parse_coordinate(fd, read, image, state, file_line);
	    
	    /* add the new drill hole */
	    curr_net = drill_add_drill_hole (image, state, stats, curr_net);
	    break;

	case '%':
	    state->curr_section = DRILL_DATA;
	    break;

	case '\n' :
	    file_line++;

	    /* Get <CR> char, if any, from <LF><CR> pair */
	    read = gerb_fgetc(fd);
	    if (read != '\r' && read != EOF)
		    gerb_ungetc(fd);
	    break;

	case '\r' :
	    file_line++;

	    /* Get <LF> char, if any, from <CR><LF> pair */
	    read = gerb_fgetc(fd);
	    if (read != '\n' && read != EOF)
		    gerb_ungetc(fd);
	    break;

	case ' ' :	/* White space */
	case '\t' :
	    break;

	default:
	    stats->unknown++;

	    if (DRILL_HEADER == state->curr_section) {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Undefined code '%s' (0x%x) found in header "
			    "at line %ld in file \"%s\""),
			gerbv_escape_char(read), read,
			file_line, fd->filename);
		gerb_ungetc(fd);

		/* Unrecognised crap in the header is thrown away */
		tmps = get_line(fd);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
			_("Unrecognised string \"%s\" in header "
			    "at line %ld in file \"%s\""),
			tmps, file_line, fd->filename);
	  	g_free (tmps);
	    } else {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Ignoring undefined character '%s' (0x%x) "
			    "found inside data at line %ld in file \"%s\""),
			gerbv_escape_char(read), read, file_line, fd->filename);
	    }
	}
    }

    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
	    _("No EOF found in drill file \"%s\""), fd->filename);

drill_parse_end:
    dprintf ("%s():  Populating file attributes\n", __FUNCTION__);

    hid_attrs = image->info->attr_list;

    switch (state->unit) {
    case GERBV_UNIT_MM:
	hid_attrs[HA_xy_units].default_val.int_value = GERBV_UNIT_MM;
	break;

    default:
	hid_attrs[HA_xy_units].default_val.int_value = GERBV_UNIT_INCH;
	break;
    }

    switch (state->number_format) {
    case FMT_000_00:
    case FMT_0000_00:
	hid_attrs[HA_digits].default_val.int_value = 2;
	break;

    case FMT_000_000:
	hid_attrs[HA_digits].default_val.int_value = 3;
	break;
	
    case FMT_00_0000:
	hid_attrs[HA_digits].default_val.int_value = 4;
	break;
	
    case FMT_USER:
	dprintf ("%s():  Keeping user specified number of decimal places (%d)\n",
		 __FUNCTION__,
		 hid_attrs[HA_digits].default_val.int_value);
	break;
	
    default:
	break;
    }

    switch (image->format->omit_zeros) {
    case GERBV_OMIT_ZEROS_LEADING:
	hid_attrs[HA_suppression].default_val.int_value = SUP_LEAD;
	break;
	    
    case GERBV_OMIT_ZEROS_TRAILING:
	hid_attrs[HA_suppression].default_val.int_value = SUP_TRAIL;
	break;

    default:
	hid_attrs[HA_suppression].default_val.int_value = SUP_NONE;
	break;
    }

    g_free(state);

    return image;
} /* parse_drillfile */


/* -------------------------------------------------------------- */
/*
 * Checks for signs that this is a drill file
 * Returns TRUE if it is, FALSE if not.
 */
gboolean
drill_file_p(gerb_file_t *fd, gboolean *returnFoundBinary)
{
    char *buf=NULL, *tbuf;
    int len = 0;
    char *letter;
    int ascii;
    int zero = 48; /* ascii 0 */
    int nine = 57; /* ascii 9 */
    int i;
    gboolean found_binary = FALSE;
    gboolean found_M48 = FALSE;
    gboolean found_M30 = FALSE;
    gboolean found_percent = FALSE;
    gboolean found_T = FALSE;
    gboolean found_X = FALSE;
    gboolean found_Y = FALSE;
    gboolean end_comments=FALSE;
 
    tbuf = g_malloc(MAXL);
    if (tbuf == NULL) 
	GERB_FATAL_ERROR(
		"malloc buf failed while checking for drill file in %s()",
		__FUNCTION__);

    while (fgets(tbuf, MAXL, fd->fd) != NULL) {
	len = strlen(tbuf);
	buf = tbuf;
	/* check for comments at top of file.  */
	if(!end_comments){
		if(g_strstr_len(buf, len, ";")!=NULL){/* comments at top of file  */
			for (i = 0; i < len-1; ++i) {
				if (buf[i] == '\n'
				&&  buf[i+1] != ';'
				&&  buf[i+1] != '\r'
				&&  buf[i+1] != '\n') {
					end_comments = TRUE;
					/* Set rest of parser to end of
					 * comments */
					buf = &tbuf[i+1];
					
				}
			}
			if(!end_comments)
				continue;
		}			
		else 
			end_comments=TRUE;
	}

	/* First look through the file for indications of its type */
	len = strlen(buf);
	/* check that file is not binary (non-printing chars) */
	for (i = 0; i < len; i++) {
	    ascii = (int) buf[i];
	    if ((ascii > 128) || (ascii < 0)) {
		found_binary = TRUE;
	    }
	}

	/* Check for M48 = start of drill header */
	if (g_strstr_len(buf, len, "M48")) {
	    found_M48 = TRUE; 
	}

	/* Check for M30 = end of drill program */
	if (g_strstr_len(buf, len, "M30")) {
	    if (found_percent) {
		found_M30 = TRUE; /* Found M30 after % = good */
	    }
	}

	/* Check for % on its own line at end of header */
	if ((letter = g_strstr_len(buf, len, "%")) != NULL) {
	    if ((letter[1] ==  '\r') || (letter[1] ==  '\n'))
		found_percent = TRUE;
	}

	/* Check for T<number> */
	if ((letter = g_strstr_len(buf, len, "T")) != NULL) {
	    if (!found_T && (found_X || found_Y)) {
		found_T = FALSE;  /* Found first T after X or Y */
	    } else {
		if (isdigit( (int) letter[1])) { /* verify next char is digit */
		    found_T = TRUE;
		}
	    }
	}

	/* look for X<number> or Y<number> */
	if ((letter = g_strstr_len(buf, len, "X")) != NULL) {
	    ascii = (int) letter[1]; /* grab char after X */
	    if ((ascii >= zero) && (ascii <= nine)) {
		found_X = TRUE;
	    }
	}
	if ((letter = g_strstr_len(buf, len, "Y")) != NULL) {
	    ascii = (int) letter[1]; /* grab char after Y */
	    if ((ascii >= zero) && (ascii <= nine)) {
		found_Y = TRUE;
	    }
	}
    } /* while (fgets(buf, MAXL, fd->fd) */

    rewind(fd->fd);
    g_free(tbuf);
    *returnFoundBinary = found_binary;
    
    /* Now form logical expression determining if this is a drill file */
    if ( ((found_X || found_Y) && found_T) && 
	 (found_M48 || (found_percent && found_M30)) ) 
	return TRUE;
    else if (found_M48 && found_T && found_percent && found_M30)
	/* Pathological case of drill file with valid header 
	   and EOF but no drill XY locations. */
	return TRUE;
    else 
	return FALSE;
} /* drill_file_p */


/* -------------------------------------------------------------- */
/* Parse tool definition. This can get a bit tricky since it can
   appear in the header and/or data section.
   Returns tool number on success, -1 on error */
static int
drill_parse_T_code(gerb_file_t *fd, drill_state_t *state,
			gerbv_image_t *image, ssize_t file_line)
{
    int tool_num;
    gboolean done = FALSE;
    int temp;
    double size;
    gerbv_drill_stats_t *stats = image->drill_stats;
    gerbv_aperture_t *apert;
    gchar *tmps;
    gchar *string;

    dprintf("---> entering %s()...\n", __FUNCTION__);

    /* Sneak a peek at what's hiding after the 'T'. Ugly fix for
       broken headers from Orcad, which is crap */
    temp = gerb_fgetc(fd);
    dprintf("  Found a char '%s' (0x%02x) after the T\n",
	    gerbv_escape_char(temp), temp);
    
    /* might be a tool tool change stop switch on/off*/
    if((temp == 'C') && ((fd->ptr + 2) < fd->datalen)){
    	if(gerb_fgetc(fd) == 'S'){
    	    if (gerb_fgetc(fd) == 'T' ){
    	  	fd->ptr -= 4;
    	  	tmps = get_line(fd++);
    	  	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_NOTE, -1,
			_("Tool change stop switch found \"%s\" "
			    "at line %ld in file \"%s\""),
			tmps, file_line, fd->filename);
	  	g_free (tmps);

	  	return -1;
	    }
	    gerb_ungetc(fd);
	}
	gerb_ungetc(fd);
    }

    if( !(isdigit(temp) != 0 || temp == '+' || temp =='-') ) {
	if(temp != EOF) {
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		   _("OrCAD bug: Junk text found in place of tool definition"));
	    tmps = get_line(fd);
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
		    _("Junk text \"%s\" "
			"at line %ld in file \"%s\""),
		    tmps, file_line, fd->filename);
	    g_free (tmps);
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
				  _("Ignoring junk text"));
	}
	return -1;
    }
    gerb_ungetc(fd);

    tool_num = (int) gerb_fgetint(fd, NULL);
    dprintf ("  Handling tool T%d at line %ld\n", tool_num, file_line);

    if (tool_num == 0) 
	return tool_num; /* T00 is a command to unload the drill */

    if (tool_num < TOOL_MIN || tool_num >= TOOL_MAX) {
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Out of bounds drill number %d "
		    "at line %ld in file \"%s\""),
		tool_num, file_line, fd->filename);
    }

    /* Set the current tool to the correct one */
    state->current_tool = tool_num;

    /* Check for a size definition */
    temp = gerb_fgetc(fd);

    /* This bit of code looks for a tool definition by scanning for strings
     * of form TxxC, TxxF, TxxS.  */
    while (!done) {
	switch((char)temp) {
	case 'C':
	    size = read_double(fd, state->header_number_format, GERBV_OMIT_ZEROS_TRAILING, state->decimals);
	    dprintf ("  Read a size of %g\n", size);

	    if (state->unit == GERBV_UNIT_MM) {
		size /= 25.4;
	    } else if(size >= 4.0) {
		/* If the drill size is >= 4 inches, assume that this
		   must be wrong and that the units are mils.
		   The limit being 4 inches is because the smallest drill
		   I've ever seen used is 0,3mm(about 12mil). Half of that
		   seemed a bit too small a margin, so a third it is */

		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Read a drill of diameter %g inches "
			    "at line %ld in file \"%s\""),
			    size, file_line, fd->filename);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
			_("Assuming units are mils"));
		size /= 1000.0;
	    }

	    if (size <= 0. || size >= 10000.) {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Unreasonable drill size %g found for drill %d "
			    "at line %ld in file \"%s\""),
			    size, tool_num, file_line, fd->filename);
	    } else {
		apert = image->aperture[tool_num];
		if (apert != NULL) {
		    /* allow a redefine of a tool only if the new definition is exactly the same.
		     * This avoid lots of spurious complaints with the output of some cad
		     * tools while keeping complaints if there is a true problem
		     */
		    if (apert->parameter[0] != size
		    ||  apert->type != GERBV_APTYPE_CIRCLE
		    ||  apert->nuf_parameters != 1
		    ||  apert->unit != GERBV_UNIT_INCH) {

			gerbv_stats_printf(stats->error_list,
				GERBV_MESSAGE_ERROR, -1,
				_("Found redefinition of drill %d "
				"at line %ld in file \"%s\""),
				tool_num, file_line, fd->filename);
		    }
		} else {
		    apert = image->aperture[tool_num] =
						g_new0(gerbv_aperture_t, 1);
		    if (apert == NULL)
			GERB_FATAL_ERROR("malloc tool failed in %s()",
					__FUNCTION__);

		    /* There's really no way of knowing what unit the tools
		       are defined in without sneaking a peek in the rest of
		       the file first. That's done in drill_guess_format() */
		    apert->parameter[0] = size;
		    apert->type = GERBV_APTYPE_CIRCLE;
		    apert->nuf_parameters = 1;
		    apert->unit = GERBV_UNIT_INCH;
		}
	    }
	    
	    /* Add the tool whose definition we just found into the list
	     * of tools for this layer used to generate statistics. */
	    stats = image->drill_stats;
	    string = g_strdup_printf("%s", (state->unit == GERBV_UNIT_MM ? _("mm") : _("inch")));
	    drill_stats_add_to_drill_list(stats->drill_list, 
					  tool_num, 
					  state->unit == GERBV_UNIT_MM ? size*25.4 : size, 
					  string);
	    g_free(string);
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
	}  /* switch((char)temp) */

	temp = gerb_fgetc(fd);
	if (EOF == temp) {
	    gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		    _("Unexpected EOF encountered in header of "
			"drill file \"%s\""), fd->filename);

	/* Restore new line character for processing */
	if ('\n' == temp || '\r' == temp)
	    gerb_ungetc(fd);
	}
    }   /* while(!done) */  /* Done looking at tool definitions */

    /* Catch the tools that aren't defined.
       This isn't strictly a good thing, but at least something is shown */
    if (apert == NULL) {
        double dia;

	apert = image->aperture[tool_num] = g_new0(gerbv_aperture_t, 1);
	if (apert == NULL)
	    GERB_FATAL_ERROR("malloc tool failed in %s()", __FUNCTION__);

        /* See if we have the tool table */
        dia = gerbv_get_tool_diameter(tool_num);
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
            if (tool_num != 0) {
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
			_("Tool %02d used without being defined "
			    "at line %ld in file \"%s\""),
			tool_num, file_line, fd->filename);
		gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_WARNING, -1,
			_("Setting a default size of %g\""), dia);
            }
	}

	apert->type = GERBV_APTYPE_CIRCLE;
	apert->nuf_parameters = 1;
	apert->parameter[0] = dia;

	/* Add the tool whose definition we just found into the list
	 * of tools for this layer used to generate statistics. */
	if (tool_num != 0) {  /* Only add non-zero tool nums.  
			       * Zero = unload command. */
	    stats = image->drill_stats;
	    string = g_strdup_printf("%s", 
				     (state->unit == GERBV_UNIT_MM ? _("mm") : _("inch")));
	    drill_stats_add_to_drill_list(stats->drill_list, 
					  tool_num, 
					  state->unit == GERBV_UNIT_MM ? dia*25.4 : dia,
					  string);
	    g_free(string);
	}
    } /* if(image->aperture[tool_num] == NULL) */	
    
    dprintf("<----  ...leaving %s()\n", __FUNCTION__);

    return tool_num;
} /* drill_parse_T_code() */


/* -------------------------------------------------------------- */
static drill_m_code_t
drill_parse_M_code(gerb_file_t *fd, drill_state_t *state,
	gerbv_image_t *image, ssize_t file_line)
{
    gerbv_drill_stats_t *stats = image->drill_stats;
    drill_m_code_t m_code;
    char op[3];

    dprintf("---> entering %s() ...\n", __FUNCTION__);

    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    op[2] = '\0';

    if (op[0] == EOF
    ||  op[1] == EOF) {
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unexpected EOF found while parsing M-code in file \"%s\""),
		fd->filename);

	return DRILL_M_UNKNOWN;
    }

    dprintf("  Compare M-code \"%s\" at line %ld\n", op, file_line);

    switch (m_code = atoi(op)) {
    case 0:
	/* atoi() return 0 in case of error, recheck string */
	if (0 != strncmp(op, "00", 2)) {
	    m_code = DRILL_M_UNKNOWN;
	    gerb_ungetc(fd);
	    gerb_ungetc(fd);
	    break;
	}
	stats->M00++;
	break;
    case 1:
	stats->M01++;
	break;
    case 18:
	stats->M18++;
	break;
    case 25:
	stats->M25++;
	break;
    case 30:
	stats->M30++;
	break;
    case 45:
	stats->M45++;
	break;
    case 47:
	stats->M47++;
	break;
    case 48:
	stats->M48++;
	break;
    case 71:
	stats->M71++;
	eat_line(fd);
	break;
    case 72:
	stats->M72++;
	eat_line(fd);
	break;
    case 95:
	stats->M95++;
	break;
    case 97:
	stats->M97++;
	break;
    case 98:
	stats->M98++;
	break;

    default:
    case DRILL_M_UNKNOWN:
	break;
    }


    dprintf("<----  ...leaving %s()\n", __FUNCTION__);

    return m_code;
} /* drill_parse_M_code() */

/* -------------------------------------------------------------- */
static int
drill_parse_header_is_metric(gerb_file_t *fd, drill_state_t *state,
			gerbv_image_t *image, ssize_t file_line)
{
    gerbv_drill_stats_t *stats = image->drill_stats;
    char c, op[3];

    dprintf("    %s(): entering\n", __FUNCTION__);

    /* METRIC is not an actual M code but a command that is only
     * acceptable within the header.
     *
     * The syntax is
     * METRIC[,{TZ|LZ}][,{000.000|000.00|0000.00}]
     */

    if (DRILL_HEADER != state->curr_section)
	return 0;

    switch (file_check_str(fd, "METRIC")) {
    case -1:
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unexpected EOF found while parsing \"%s\" string "
		    "in file \"%s\""), "METRIC", fd->filename);
	return 0;

    case 0:
	return 0;
    }

header_again:

    if (',' != gerb_fgetc(fd)) {
	gerb_ungetc(fd);
	eat_line(fd);
    } else {
	/* Is it TZ, LZ, or zerofmt? */
	switch (c = gerb_fgetc(fd)) {
	case 'T':
	case 'L':
	    if ('Z' != gerb_fgetc(fd))
		goto header_junk;

	    if (c == 'L') {
		dprintf ("    %s(): Detected a file that probably has "
			"trailing zero suppression\n", __FUNCTION__);
		if (state->autod)
		    image->format->omit_zeros = GERBV_OMIT_ZEROS_TRAILING;
	    } else {
		dprintf ("    %s(): Detected a file that probably has "
			"leading zero suppression\n", __FUNCTION__);
		if (state->autod)
		    image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
	    }

	    if (state->autod) {
		/* Default metric number format is 6-digit, 1 um
		 * resolution.  The header number format (for T#C#
		 * definitions) is fixed to that, while the number
		 * format within the file can differ. */
		state->header_number_format =
		    state->number_format = FMT_000_000;
		state->decimals = 3;
	    }

	    if (',' == gerb_fgetc(fd))
		/* Anticipate number format will follow */
		goto header_again;

	    gerb_ungetc(fd);

	    break;

	case '0':
	    if ('0' != gerb_fgetc(fd)
	    ||  '0' != gerb_fgetc(fd))
		goto header_junk;

	    /* We just parsed three 0s, the remainder options
	       so far are: .000 | .00 | 0.00 */
	    op[0] = gerb_fgetc(fd);
	    op[1] = gerb_fgetc(fd);
	    op[2] = '\0';
	    if (EOF == op[0]
	    ||  EOF == op[1])
		goto header_junk;

	    if (0 == strcmp(op, "0.")) {
		/* expecting FMT_0000_00,
		   two trailing 0s must follow */
		if ('0' != gerb_fgetc(fd)
		||  '0' != gerb_fgetc(fd))
		    goto header_junk;

		eat_line(fd);

		if (state->autod) {
		    state->number_format = FMT_0000_00;
		    state->decimals = 2;
		}
		break;
	    }

	    if (0 != strcmp(op, ".0"))
		goto header_junk;

	    /* Must be either FMT_000_000 or FMT_000_00, depending
	     * on whether one or two 0s are following */
	    if ('0' != gerb_fgetc(fd))
		goto header_junk;

	    if ('0' == gerb_fgetc(fd)
	    &&  state->autod) {
		state->number_format = FMT_000_000;
		state->decimals = 3;
	    } else {
		gerb_ungetc(fd);

		if (state->autod) {
		    state->number_format = FMT_000_00;
		    state->decimals = 2;
		}
	    }

	    eat_line(fd);
	    break;

	default:
header_junk:
	    gerb_ungetc(fd);
	    eat_line(fd);

	    gerbv_stats_printf(stats->error_list,
		    GERBV_MESSAGE_WARNING, -1,
		    _("Found junk after METRIC command "
			"at line %ld in file \"%s\""),
		    file_line, fd->filename);
	    break;
	}
    }

    state->unit = GERBV_UNIT_MM;

    return 1;
} /* drill_parse_header_is_metric() */

/* -------------------------------------------------------------- */
static int
drill_parse_header_is_inch(gerb_file_t *fd, drill_state_t *state,
			gerbv_image_t *image, ssize_t file_line)
{
    gerbv_drill_stats_t *stats = image->drill_stats;
    char c;

    dprintf("    %s(): entering\n", __FUNCTION__);

    if (DRILL_HEADER != state->curr_section) 
	return 0;

    switch (file_check_str(fd, "INCH")) {
    case -1:
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unexpected EOF found while parsing \"%s\" string "
		    "in file \"%s\""), "INCH", fd->filename);
	return 0;

    case 0:
	return 0;
    }

    /* Look for TZ/LZ */
    if (',' != gerb_fgetc(fd)) {
	/* Unget the char in case we just advanced past a new line char */
	gerb_ungetc (fd);
    } else {
	c = gerb_fgetc(fd);
	if (c != EOF && 'Z' == gerb_fgetc(fd)) {
	    switch (c) {
	    case 'L':
		if (state->autod) {
		    image->format->omit_zeros = GERBV_OMIT_ZEROS_TRAILING;
		    state->header_number_format =
			state->number_format = FMT_00_0000;
		    state->decimals = 4;
		}
		break;

	    case 'T':
		if (state->autod) {
		    image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
		    state->header_number_format =
			state->number_format = FMT_00_0000;
		    state->decimals = 4;
		}
		break;

	    default:
		gerbv_stats_printf(stats->error_list,
			GERBV_MESSAGE_WARNING, -1,
			_("Found junk '%s' after "
			    "INCH command "
			    "at line %ld in file \"%s\""),
			gerbv_escape_char(c),
			file_line, fd->filename);
		break;
	    }
	} else {
	    gerbv_stats_printf(stats->error_list,
		    GERBV_MESSAGE_WARNING, -1,
		    _("Found junk '%s' after INCH command "
			"at line %ld in file \"%s\""),
		    gerbv_escape_char(c),
		    file_line, fd->filename);
	}
    }

    state->unit = GERBV_UNIT_INCH;

    return 1;
} /* drill_parse_header_is_inch() */

/* -------------------------------------------------------------- */
/* Check "ICI" incremental input of coordinates */
static int
drill_parse_header_is_ici(gerb_file_t *fd, drill_state_t *state,
			gerbv_image_t *image, ssize_t file_line)
{
    gerbv_drill_stats_t *stats = image->drill_stats;

    switch (file_check_str(fd, "ICI,ON")) {
    case -1:
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unexpected EOF found while parsing \"%s\" string "
		    "in file \"%s\""), "ICI,ON", fd->filename);
	return 0;

    case 1:
	state->coordinate_mode = DRILL_MODE_INCREMENTAL;
	return 1;
    }

    switch (file_check_str(fd, "ICI,OFF")) {
    case -1:
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unexpected EOF found while parsing \"%s\" string "
		    "in file \"%s\""), "ICI,OFF", fd->filename);
	return 0;

    case 1:
	state->coordinate_mode = DRILL_MODE_ABSOLUTE;
	return 1;
    }

    return 0;
} /* drill_parse_header_is_ici() */

/* -------------------------------------------------------------- */
static drill_g_code_t 
drill_parse_G_code(gerb_file_t *fd, gerbv_image_t *image, ssize_t file_line)
{
    char op[3];
    drill_g_code_t g_code;
    gerbv_drill_stats_t *stats = image->drill_stats;
    
    dprintf("---> entering %s()...\n", __FUNCTION__);

    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    op[2] = '\0';

    if (op[0] == EOF
    ||  op[1] == EOF) {
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Unexpected EOF found while parsing G-code in file \"%s\""),
		fd->filename);
	return DRILL_G_UNKNOWN;
    }

    dprintf("  Compare G-code \"%s\" at line %ld\n", op, file_line);

    switch (g_code = atoi(op)) {
    case 0:
	/* atoi() return 0 in case of error, recheck string */
	if (0 != strncmp(op, "00", 2)) {
	    g_code = DRILL_G_UNKNOWN;
	    gerb_ungetc(fd);
	    gerb_ungetc(fd);
	    break;
	}
	stats->G00++;
	break;
    case 1:
	stats->G01++;
	break;
    case 2:
	stats->G02++;
	break;
    case 3:
	stats->G03++;
	break;
    case 5:
	stats->G05++;
	break;
    case 85:
	stats->G85++;
	break;
    case 90:
	stats->G90++;
	break;
    case 91:
	stats->G91++;
	break;
    case 93:
	stats->G93++;
	break;

    case DRILL_G_UNKNOWN:
    default:
	stats->G_unknown++;
	break;
    }

    dprintf("<----  ...leaving %s()\n", __FUNCTION__);

    return g_code;
} /* drill_parse_G_code() */


/* -------------------------------------------------------------- */
/* Parse on drill file coordinate.
   Returns nothing, but modifies state */
static void
drill_parse_coordinate(gerb_file_t *fd, char firstchar,
		       gerbv_image_t *image, drill_state_t *state,
		       ssize_t file_line)
{
    int read;
    gerbv_drill_stats_t *stats = image->drill_stats;

    if(state->coordinate_mode == DRILL_MODE_ABSOLUTE) {
	if (firstchar == 'X') {
	    state->curr_x = read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
	    if ((read = (char)gerb_fgetc(fd)) == 'Y') {
		state->curr_y = read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
	    } else {
		gerb_ungetc(fd);
	    }
	} else if (firstchar == 'Y') {
	    state->curr_y = read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
	}
    } else if(state->coordinate_mode == DRILL_MODE_INCREMENTAL) {
	if (firstchar == 'X') {
	    state->curr_x += read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
	    if((read = (char)gerb_fgetc(fd)) == 'Y') {
		state->curr_y += read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
	    } else {
		gerb_ungetc(fd);
	    }
	} else if (firstchar == 'Y') {
	    state->curr_y += read_double(fd, state->number_format, image->format->omit_zeros, state->decimals);
	}
    } else {
	gerbv_stats_printf(stats->error_list, GERBV_MESSAGE_ERROR, -1,
		_("Coordinate mode is not absolute and not incremental "
		    "at line %ld in file \"%s\""),
		file_line, fd->filename);
    }

} /* drill_parse_coordinate */


/* Allocates and returns a new drill_state structure
   Returns state pointer on success, NULL on ERROR */
static drill_state_t *
new_state(drill_state_t *state)
{
    state = g_new0(drill_state_t, 1);
    if (state != NULL) {
	/* Init structure */
	state->curr_section = DRILL_NONE;
	state->coordinate_mode = DRILL_MODE_ABSOLUTE;
	state->origin_x = 0.0;
	state->origin_y = 0.0;
	state->unit = GERBV_UNIT_UNSPECIFIED;
	state->backup_number_format = FMT_000_000; /* only used for METRIC */
	state->header_number_format = state->number_format = FMT_00_0000; /* i. e. INCH */
	state->autod = 1;
	state->decimals = 4;
    }

    return state;
} /* new_state */


/* -------------------------------------------------------------- */
/* Reads one double from fd and returns it.
   If a decimal point is found, fmt is not used. */
static double
read_double(gerb_file_t *fd, number_fmt_t fmt, gerbv_omit_zeros_t omit_zeros, int decimals)
{
    int read;
    char temp[DRILL_READ_DOUBLE_SIZE];
    int i = 0, ndigits = 0;
    double result;
    gboolean decimal_point = FALSE;
    gboolean sign_prepend = FALSE;

    memset(temp, 0, sizeof(temp));

    read = gerb_fgetc(fd);
    while(read != EOF && i < (DRILL_READ_DOUBLE_SIZE -1) &&
	  (isdigit(read) || read == '.' || read == ',' || read == '+' || read == '-')) {
      if(read == ',' || read == '.') decimal_point = TRUE;
      
      /*
       * FIXME -- if we are going to do this, don't we need a
       * locale-independent strtod()?  I think pcb has one.
       */
      if(read == ',')
	    read = '.'; /* adjust for strtod() */
      
      if(isdigit(read)) ndigits++;
      
	if(read == '-' || read == '+')
	    sign_prepend = TRUE;

      temp[i++] = (char)read;
      read = gerb_fgetc(fd);
    }

    temp[i] = 0;
    gerb_ungetc(fd);

    if (decimal_point) {
	result = strtod(temp, NULL);
    } else {
	int wantdigits;
	double scale;
	char tmp2[DRILL_READ_DOUBLE_SIZE];

	memset(tmp2, 0, sizeof(tmp2));

	/* Nothing to take care for when leading zeros are
	   omitted. */
	if (omit_zeros == GERBV_OMIT_ZEROS_TRAILING) {
	    switch (fmt) {
	    case FMT_00_0000:
	      wantdigits = 2;
	      break;

	    case FMT_000_000:
	      wantdigits = 3;
	      break;

	    case FMT_0000_00:
		wantdigits = 4;
		break;

	    case FMT_000_00:
		wantdigits = 3;
		break;

	    case FMT_USER:
		wantdigits = decimals;
		break;

	    default:
	      /* cannot happen, just plugs a compiler warning */
	      fprintf(stderr, _("%s():  omit_zeros == GERBV_OMIT_ZEROS_TRAILING but fmt = %d.\n"
		      "This should never have happened\n"), __FUNCTION__, fmt);
	      return 0;
	    }
	    
	    /* need to add an extra char for '+' or '-' */
	    if (sign_prepend)
	      wantdigits++;


	    /* 
	     * we need at least wantdigits + one for the decimal place
	     * + one for the terminating null character
	     */
	    if (wantdigits > sizeof(tmp2) - 2) {
	      fprintf(stderr, _("%s():  wantdigits = %d which exceeds the maximum allowed size\n"),
		      __FUNCTION__, wantdigits);
	      return 0;
	    }

	    /*
	     * After we have read the correct number of digits
	     * preceeding the decimal point, insert a decimal point
	     * and append the rest of the digits.
	     */
	    dprintf("%s():  wantdigits = %d, strlen(\"%s\") = %ld\n",
		    __FUNCTION__, wantdigits, temp, (long) strlen(temp));
	    for (i = 0 ; i < wantdigits && i < strlen(temp) ; i++) {
	      tmp2[i] = temp[i];
	    }
	    for ( ; i < wantdigits ; i++) {
	      tmp2[i] = '0';
	    }
	    tmp2[i++] = '.';
	    for ( ; i <= strlen(temp) ; i++) {
	      tmp2[i] = temp[i-1];
	    }
	    dprintf("%s():  After dealing with trailing zero suppression, convert \"%s\"\n", __FUNCTION__, tmp2);
	    scale = 1.0;
	    
	    for (i = 0 ; i <= strlen(tmp2) && i < sizeof (temp) ; i++) {
	      temp[i] = tmp2[i];
	    }

	} else {

	  /*
	   * figure out the scale factor when we are not suppressing
	   * trailing zeros.
	   */
	  switch (fmt) {
	  case FMT_00_0000:
	    scale = 1E-4;
	    break;
	    
	  case FMT_000_000:
	    scale = 1E-3;
	    break;
	    
	  case FMT_000_00:
	  case FMT_0000_00:
	    scale = 1E-2;
	    break;
	    
	  case FMT_USER:
	    scale = pow (10.0, -1.0*decimals);
	    break;
	    
	  default:
	    /* cannot happen, just plugs a compiler warning */
	    fprintf (stderr, _("%s(): Unhandled fmt ` %d\n"), __FUNCTION__, fmt);
	    exit (1);
	  }
	}

	result = strtod(temp, NULL) * scale;
    }

    dprintf("    %s()=%f: fmt=%d, omit_zeros=%d, decimals=%d \n",
		    __FUNCTION__, result, fmt, omit_zeros, decimals);

    return result;
} /* read_double */

/* -------------------------------------------------------------- */
/* Eats all characters up to and including 
   the first one of CR or LF */
static void
eat_line(gerb_file_t *fd)
{
    int read;

    do {
	read = gerb_fgetc(fd);
    } while (read != '\n' && read != '\r' && read != EOF);

    /* Restore new line character for processing */
    if (read != EOF)
	gerb_ungetc(fd);
} /* eat_line */

/* -------------------------------------------------------------- */
static char *
get_line(gerb_file_t *fd)
{
	int read;
	gchar *retstring;
	gchar *tmps=g_strdup("");

	read = gerb_fgetc(fd);
	while (read != '\n' && read != '\r' && read != EOF) {
		retstring = g_strdup_printf("%s%c", tmps, read);

		/* since g_strdup_printf allocates memory, we need to free it */
		if (tmps)  {
			g_free (tmps);
			tmps = NULL;
		}
		tmps = retstring;
		read = gerb_fgetc(fd);
	}

	/* Restore new line character for processing */
	if (read != EOF)
	    gerb_ungetc(fd);

	return tmps;
} /* get_line */

/* -------------------------------------------------------------- */
static int
file_check_str(gerb_file_t *fd, const char *str)
{
    char c;

    for (int i = 0; str[i] != '\0'; i++) {

	c = gerb_fgetc(fd);

	if (c == EOF)
	    return -1;

	if (c != str[i]) {
	    do {
		/* Restore checked string */
		gerb_ungetc(fd);
	    } while (i--);

	    return 0;
	}
    }

    return 1;
}

/* -------------------------------------------------------------- */
/** Return drill G-code name by code number. */
const char *drill_g_code_name(drill_g_code_t g_code)
{
    switch (g_code) {
    case DRILL_G_ROUT:
	return N_("rout mode");
    case DRILL_G_LINEARMOVE:
	return N_("linear mode");
    case DRILL_G_CWMOVE:
	return N_("circular CW mode");
    case DRILL_G_CCWMOVE:
	return N_("circular CCW mode");
    case DRILL_G_VARIABLEDWELL:
	return N_("variable dwell");
    case DRILL_G_DRILL:
	return N_("drill mode");
    case DRILL_G_OVERRIDETOOLSPEED:
	return N_("override tool feed or speed");
    case DRILL_G_ROUTCIRCLE:
	return N_("routed CW circle");
    case DRILL_G_ROUTCIRCLECCW:
	return N_("routed CCW circle");
    case DRILL_G_VISTOOL:
	return N_("select vision tool");
    case DRILL_G_VISSINGLEPOINTOFFSET:
	return N_("single point vision offset");
    case DRILL_G_VISMULTIPOINTTRANS:
	return N_("multipoint vision translation");
    case DRILL_G_VISCANCEL:
	return N_("cancel vision translation or offset");
    case DRILL_G_VISCORRHOLEDRILL:
	return N_("vision corrected single hole drilling");
    case DRILL_G_VISAUTOCALIBRATION:
	return N_("vision system autocalibration");
    case DRILL_G_CUTTERCOMPOFF:
	return N_("cutter compensation off");
    case DRILL_G_CUTTERCOMPLEFT:
	return N_("cutter compensation left");
    case DRILL_G_CUTTERCOMPRIGHT:
	return N_("cutter compensation right");
    case DRILL_G_VISSINGLEPOINTOFFSETREL:
	return N_("single point vision relative offset");
    case DRILL_G_VISMULTIPOINTTRANSREL:
	return N_("multipoint vision relative translation");
    case DRILL_G_VISCANCELREL:
	return N_("cancel vision relative translation or offset");
    case DRILL_G_VISCORRHOLEDRILLREL:
	return N_("vision corrected single hole relative drilling");
    case DRILL_G_PACKDIP2:
	return N_("dual in line package");
    case DRILL_G_PACKDIP:
	return N_("dual in line package");
    case DRILL_G_PACK8PINL:
	return N_("eight pin L package");
    case DRILL_G_CIRLE:
	return N_("canned circle");
    case DRILL_G_SLOT:
	return N_("canned slot");
    case DRILL_G_ROUTSLOT:
	return N_("routed step slot");
    case DRILL_G_ABSOLUTE:
	return N_("absolute input mode");
    case DRILL_G_INCREMENTAL:
	return N_("incremental input mode");
    case DRILL_G_ZEROSET:
	return N_("zero set");

    case DRILL_G_UNKNOWN:
    default:
	return N_("unknown G-code");
    }
} /* drill_g_code_name() */

/* -------------------------------------------------------------- */
/** Return drill M-code name by code number. */
const char *drill_m_code_name(drill_m_code_t m_code)
{
    switch (m_code) {
    case DRILL_M_END:
	return N_("end of program");
    case DRILL_M_PATTERNEND:
	return N_("pattern end");
    case DRILL_M_REPEATPATTERNOFFSET:
	return N_("repeat pattern offset");
    case DRILL_M_STOPOPTIONAL:
	return N_("stop optional");
    case DRILL_M_SANDREND:
	return N_("step and repeat end");
    case DRILL_M_STOPINSPECTION:
	return N_("stop for inspection");
    case DRILL_M_ZAXISROUTEPOSITIONDEPTHCTRL:
	return N_("Z-axis rout position with depth control");
    case DRILL_M_ZAXISROUTEPOSITION:
	return N_("Z-axis rout position");
    case DRILL_M_RETRACTCLAMPING:
	return N_("retract with clamping");
    case DRILL_M_RETRACTNOCLAMPING:
	return N_("retract without clamping");
    case DRILL_M_TOOLTIPCHECK:
	return N_("tool tip check");
    case DRILL_M_PATTERN:
	return N_("pattern start");
    case DRILL_M_ENDREWIND:
	return N_("end of program with rewind");
    case DRILL_M_MESSAGELONG:
	return N_("long operator message");
    case DRILL_M_MESSAGE:
	return N_("operator message");
    case DRILL_M_HEADER:
	return N_("header start");
    case DRILL_M_VISANDRPATTERN:
	return N_("vision step and repeat pattern start");
    case DRILL_M_VISANDRPATTERNREWIND:
	return N_("vision step and repeat rewind");
    case DRILL_M_VISANDRPATTERNOFFSETCOUNTERCTRL:
	return N_("vision step and repeat offset counter control");
    case DRILL_M_REFSCALING:
	return N_("reference scaling on");
    case DRILL_M_REFSCALINGEND:
	return N_("reference scaling off");
    case DRILL_M_PECKDRILLING:
	return N_("peck drilling on");
    case DRILL_M_PECKDRILLINGEND:
	return N_("peck drilling off");
    case DRILL_M_SWAPAXIS:
	return N_("swap axes");
    case DRILL_M_METRIC:
	return N_("metric measuring mode");
    case DRILL_M_IMPERIAL:
	return N_("inch measuring mode");
    case DRILL_M_MIRRORX:
	return N_("mirror image X-axis");
    case DRILL_M_MIRRORY:
	return N_("mirror image Y-axis");
    case DRILL_M_HEADEREND:
	return N_("header end");
    case DRILL_M_CANNEDTEXTX:
	return N_("canned text along X-axis");
    case DRILL_M_CANNEDTEXTY:
	return N_("canned text along Y-axis");
    case DRILL_M_USERDEFPATTERN:
	return N_("user defined stored pattern");

    case DRILL_M_UNKNOWN:
    default:
	return N_("unknown M-code");
    }
} /* drill_m_code_name() */
