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
 * 2007-02-21 patch for metric drill files:
 * 1) METRIC/INCH commands (partly) parsed to define units of the header
 * 2) units of the header and the program body are independent
 * 3) ICI command parsed in the header
 *
 */
/* 2023-09-01 notes:
 *
 * Link to archive'd format specification:
 * https://web.archive.org/web/20071030075236/http://www.excellon.com/manuals/program.htm
 */

#include "gerbv.h"

#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <assert.h>
#include <math.h> /* pow(), fpclassify() */
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

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

// #define DEBUG_DRILL 1
#if defined(DEBUG_DRILL) && DEBUG_DRILL
#undef DEBUG
#define DEBUG 1
#endif

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf \
    if (DEBUG)  \
    printf

#define MAXL                   200
#define DRILL_READ_DOUBLE_SIZE 32

typedef enum {
    DRILL_NONE,
    DRILL_HEADER,
    DRILL_DATA,
} drill_file_section_t;

static const char* UNUSED drill_file_section_list[] = { "NONE", "HEADER", "DATA", NULL };

static const char* UNUSED
drill_file_section_to_string(drill_file_section_t section) {
    if (section < 0 || section > DRILL_DATA) {
        return "Invalid";
    }
    return drill_file_section_list[section];
}

typedef enum {
    DRILL_MODE_ABSOLUTE,
    DRILL_MODE_INCREMENTAL,
} drill_coordinate_mode_t;

static const char* UNUSED drill_coordinate_mode_list[] = { "ABSOLUTE", "INCREMENTAL", NULL };

static const char* UNUSED
drill_coordinate_mode_to_string(drill_coordinate_mode_t mode) {
    if (mode < 0 || mode > 1) {
        return "Invalid";
    }
    return drill_coordinate_mode_list[mode];
}

typedef enum {
    FMT_00_0000, /* INCH                  */
    FMT_000_000, /* METRIC 6-digit, 1 um  */
    FMT_000_00,  /* METRIC 5-digit, 10 um */
    FMT_0000_00, /* METRIC 6-digit, 10 um */
    FMT_USER,    /* METRIC User defined format   */
} number_fmt_t;

static const char* UNUSED number_fmt_list[] = { N_("2:4"), N_("3:3"), N_("3:2"), N_("4:2"), N_("USER"), NULL };

static const char* UNUSED
number_fmt_to_string(number_fmt_t fmt) {
    if (fmt < 0 || fmt > FMT_USER) {
        return "UNKNOWN";
    } else {
        return number_fmt_list[fmt];
    }
}

static bool UNUSED
number_fmt_implies_imperial(number_fmt_t fmt) {
    return fmt == FMT_00_0000;  // this is the only format that is in inches
}

static bool UNUSED
number_fmt_implies_metric(number_fmt_t fmt) {
    return (fmt == FMT_000_000) || (fmt == FMT_000_00) || (fmt == FMT_0000_00) || (fmt == FMT_USER);
}

typedef struct drill_state {
    double                  curr_x;
    double                  curr_y;
    int                     current_tool;
    drill_file_section_t    curr_section;
    drill_coordinate_mode_t coordinate_mode;
    double                  origin_x;  // NOTE: This is ALWAYS in inches, regardless of number format units
    double                  origin_y;  // NOTE: This is ALWAYS in inches, regardless of number format units
    gerbv_unit_t            unit;      // NOTE: Affects number_format, but not header_number_format

    /* header_number_format is valid only within the drill file header section,
     * where it is only used to parse tool definition "TxxC..." codes.
     * The header_number_format may only have one of two values:
     *      FMT_000_000 (1 um resolution) for metric measures; and
     *      FMT_00_0000 for INCH measures
     *
     * Note, however, that the stored measurements of tools in the
     * internal data structures is always in INCHES, regardless of
     * the header_number_format or units settings.
     */
    number_fmt_t header_number_format;

    /* number_format is used throughout the file itself. */
    number_fmt_t number_format;

    /* Used as a backup when temporarily switching to INCH. */
    number_fmt_t backup_number_format;

    /* TRUE / !0 means to autodetect number formats, LZ/TZ, etc.
     * FALSE / 0 means to use attributes provided in HID_attributes,
     *           or that a valid `;FILE_FORMAT=` line in header was found.
     *
     *
     * Default: true at allocation
     *
     * Set to false in only two locations:
     * 1. If incoming hid_attrs list says to NOT auto-detect the file format
     * 2. If `drill_parse_header_is_metric_comment()` returns TRUE
     *
     * BUGBUG -- ??? `drill_parse_header_is_inch()` does NOT set this to FALSE ???
     */
    bool autodetect_file_format;

    /* in FMT_USER this specifies the number of digits before the
     * decimal point when doing trailing zero suppression.  Otherwise
     * it is the number of digits *after* the decimal
     * place in the file
     */
    int leading_digits;
    int trailing_digits;

} drill_state_t;

/* Local function prototypes */
// clang-format off
static drill_state_t* new_state();
static drill_g_code_t drill_parse_G_code(                    gerb_file_t* fd, gerbv_image_t* image, ssize_t file_line);
static drill_m_code_t drill_parse_M_code(                    gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line);
static int            drill_parse_T_code(                    gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line);
static int            drill_parse_header_is_metric(          gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line);
static int            drill_parse_header_is_metric_comment(  gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line);
static int            drill_parse_header_is_inch(            gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line);
static int            drill_parse_header_is_ici(             gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line);
static void           drill_parse_coordinate(                gerb_file_t* fd, char firstchar, gerbv_image_t* image, drill_state_t* state, ssize_t file_line);
static void           eat_line(                              gerb_file_t* fd);
static void           eat_whitespace(                        gerb_file_t* fd);
static char*          get_line(                              gerb_file_t* fd);
static int            file_check_str(                        gerb_file_t* fd, const char* str);
static double         read_double(                           gerb_file_t* fd, number_fmt_t fmt, gerbv_omit_zeros_t omit_zeros, int leading_digits, int trailing_digits);

// clang-format on

/* -------------------------------------------------------------- */
/* This is the list of specific attributes a drill file may have from
 * the point of view of parsing it.
 */

// WARNING -- These values (which are exposed via HID_Attribute)
//            do NOT match the values in `gerbv_omit_zeros_t`.
//            Therefore, have to individually convert them at
//            program entry and exit.
enum {
    SUP_NONE = 0,
    SUP_LEAD,
    SUP_TRAIL,
};

// This list is used within HID_Attributes ... which does NOT match `gerbv_omit_zeros_t` values
static const char* UNUSED suppression_list[] = { N_("None"), N_("Leading"), N_("Trailing"), 0 };

// This list is used with `gerbv_omit_zeros_t` ... which does NOT match HID_Attributes values
static const char* UNUSED omit_zeros_list[] = { N_("Leading"), N_("Trailing"), N_("Explicit"), N_("Unspecified"),
                                                NULL };

static const char* UNUSED
omit_zeros_to_string(gerbv_omit_zeros_t omit_zeros) {
    if (omit_zeros < 0 || omit_zeros > 3) {
        return "Invalid";
    } else {
        return omit_zeros_list[omit_zeros];
    }
}

static const char* UNUSED units_list[] = { N_("inch"), N_("mm"), 0 };

static const char* UNUSED
unit_to_string(gerbv_unit_t unit) {
    if (unit < 0 || unit > 1) {
        return "Unspecified";
    } else {
        return units_list[unit];
    }
}

enum {
    HA_auto = 0,
    HA_suppression,
    HA_xy_units,
    HA_leading_digits,
    HA_trailing_digits,
#if 0
    HA_tool_units,
#endif
};

static gerbv_HID_Attribute drill_attribute_list[] = {
  /* Order defined by enum above */
  // clang-format off
    {      N_("autodetect"), N_("Try to autodetect the file format"), HID_Boolean, 0,  0, { 1, NULL, 0 },             NULL, NULL, 0 },
    {N_("zero_suppression"),                  N_("Zero suppression"),    HID_Enum, 0,  0, { 0, NULL, 0 }, suppression_list, NULL, 0 },
    {           N_("units"),                      N_("Length units"),    HID_Enum, 0,  0, { 0, NULL, 0 },       units_list, NULL, 0 },
    {  N_("leading_digits"),  N_("Number of digits before decimal."), HID_Integer, 0,  6, { 2, NULL, 0 },             NULL, NULL, 0 },
    { N_("trailing_digits"),   N_("Number of digits after decimal."), HID_Integer, 0,  6, { 4, NULL, 0 },             NULL, NULL, 0 },
#if 0
    {       N("tool_units"),                   N_("Tool size units"),    HID_Enum, 0,  0, { 0, NULL, 0 },       units_list, NULL, 0 },
#endif
  // clang-format on
};

#define CHK_STATE(state, image, fd, file_line)                                 \
    do {                                                                       \
        check_invariants(state, image, fd, file_line, __FUNCTION__, __LINE__); \
    } while (false)

#if DEBUG
#define DUMP_STATE(state, image, fd, file_line)                          \
    do {                                                                 \
        dump_state(state, image, fd, file_line, __FUNCTION__, __LINE__); \
    } while (false)

static void UNUSED
dump_state(
    const drill_state_t* state, const gerbv_image_t* image, const gerb_file_t* fd, ssize_t file_line,
    const char* function, int code_line
) {
    printf(
        "l: %-3zd"
        "   "
        " a: %c"
        " unit: %-4s (%d)"
        " number_fmts %4s / %4s / %4s"
        " omit_%s (%d)"
        "   "
        " l/t digits %d:%d"
        //" for drill file %s"
        "\n",
        file_line, state->autodetect_file_format ? 'T' : 'F', unit_to_string(state->unit), state->unit,
        number_fmt_to_string(state->header_number_format), number_fmt_to_string(state->number_format),
        number_fmt_to_string(state->backup_number_format), omit_zeros_to_string(image->format->omit_zeros),
        image->format->omit_zeros, state->leading_digits, state->trailing_digits
        //,fd ? fd->filename ? fd->filename : "n/a" : "n/a"
    );
}
#else
#define DUMP_STATE(state, image, fd, file_line)
#endif

// Calls GERB_FATAL_ERROR() to abort program if invariant is violated
static void UNUSED
check_floating_point(
    double value, const char* field_name, const drill_state_t* state, const gerbv_image_t* image, const gerb_file_t* fd,
    ssize_t file_line, const char* function, int code_line
) {
    int classification = fpclassify(value);

    // FP_NAN        Should always be an error in drill.c
    // FP_INFINITE   Should always be an error in drill.c
    // FP_SUBNORMAL  This *should* likely be OK, if unexpected.  Log a note to help future debug tracking.
    // FP_ZERO       OK ... +0 or -0
    // FP_NORMAL     OK ... not any of the above 4 classes

    if (classification == FP_NORMAL || classification == FP_ZERO) {
        return;
    } else if (classification == FP_NAN) {
        if (image->drill_stats && image->drill_stats->error_list) {
            gerbv_stats_printf(
                image->drill_stats->error_list, GERBV_MESSAGE_FATAL, -1,
                _("%s(): Floating point number %s is NaN (not a number) (%lf) while parsing drill file %s (drill.c "
                  "source line %d)\n"),
                function, field_name, value, fd ? fd->filename ? fd->filename : "n/a" : "n/a", code_line
            );
        }
        GERB_FATAL_ERROR(
            _("%s(): Floating point number %s is NaN (not a number) (%lf) while parsing drill file %s (drill.c source "
              "line %d)\n"),
            function, field_name, value, fd ? fd->filename ? fd->filename : "n/a" : "n/a", code_line
        );
    } else if (classification == FP_INFINITE) {
        if (image->drill_stats && image->drill_stats->error_list) {
            gerbv_stats_printf(
                image->drill_stats->error_list, GERBV_MESSAGE_FATAL, -1,
                _("%s(): Floating point number %s is +/- infinity (%lf) while parsing drill file %s (drill.c source "
                  "line %d)\n"),
                function, field_name, value, fd ? fd->filename ? fd->filename : "n/a" : "n/a", code_line
            );
        }
        GERB_COMPILE_ERROR(  // BUGBUG -- LimeSDR-QPCIe_1v2-RoundHoles.drl has this error ... revert to
                             // GERB_FATAL_ERROR() after fixing?
            _("%s(): Floating point number %s is +/- infinity (%lf) while parsing drill file %s (drill.c source line "
              "%d)\n"),
            function, field_name, value, fd ? fd->filename ? fd->filename : "n/a" : "n/a", code_line
        );
    } else if (classification == FP_SUBNORMAL) {
        if (image->drill_stats && image->drill_stats->error_list) {
            gerbv_stats_printf(
                image->drill_stats->error_list, GERBV_MESSAGE_FATAL, -1,
                _("%s(): Floating point number %s is subnormal (%lf) while parsing drill file %s (drill.c source line "
                  "%d)\n"),
                function, field_name, value, fd ? fd->filename ? fd->filename : "n/a" : "n/a", code_line
            );
        }
        GERB_FATAL_ERROR(
            _("%s(): Floating point number %s is subnormal (%lf) while parsing drill file %s (drill.c source line %d)\n"
            ),
            function, field_name, value, fd ? fd->filename ? fd->filename : "n/a" : "n/a", code_line
        );
    }
    GERB_FATAL_ERROR(
        _("%s(): Floating point number %s with value (%lf) has unknown classification %d (source code error) while "
          "parsing drill file %s (drill.c source line %d)\n"),
        function, field_name, value, classification, fd ? fd->filename ? fd->filename : "n/a" : "n/a", code_line
    );
}

// Calls GERB_FATAL_ERROR() to abort program if invariant is violated
static void UNUSED
check_min_max_int_value(
    int value, const char* field_name, int min_inclusive, int max_inclusive, const drill_state_t* state,
    const gerbv_image_t* image, const gerb_file_t* fd, ssize_t file_line, const char* function, int code_line
) {

    // most often, simply return immediately
    if (value >= min_inclusive && value <= max_inclusive) {
        return;
    }
    if (image->drill_stats && image->drill_stats->error_list) {
        gerbv_stats_printf(
            image->drill_stats->error_list, GERBV_MESSAGE_FATAL, -1,
            _("%s(): Value %d in field %s is outside valid range [%d, %d] while parsing drill file %s (drill.c source "
              "line %d)\n"),
            function, value, field_name, min_inclusive, max_inclusive, fd ? fd->filename ? fd->filename : "n/a" : "n/a",
            code_line
        );
    }
    GERB_FATAL_ERROR(
        _("%s(): Value %d in field %s is outside valid range [%d, %d] while parsing drill file %s (drill.c source line "
          "%d)\n"),
        function, value, field_name, min_inclusive, max_inclusive, fd ? fd->filename ? fd->filename : "n/a" : "n/a",
        code_line
    );
}

static void UNUSED
check_invariants(
    const drill_state_t* state, const gerbv_image_t* image, const gerb_file_t* fd, ssize_t file_line,
    const char* function, int code_line
) {

    // Unfortunately, there is no 100% safe ARRAY_SIZE macro for C, so static_assert() size of aperature array to flag
    // updates required
    static_assert(
        sizeof(image->aperture) == APERTURE_MAX * sizeof(image->aperture[0]), "image->aperture[] array size mismatch"
    );

    // Tools have maximum two decimal digits. Otherwise, it's impossible to differentiate between
    // the `Tool Selection` command T#, and the `Tool Selection With Compensation` command T#(#).
    // See XNC format specification.
    //
    // However, the code defines TOOL_MAX as 9999 ... which would result in ambiguity.
    // This is a bug and should be reduced to 99. (separate issue to be filed on that bug.)
    static const int max_valid_tool = (APERTURE_MAX - 1 > 99) ? 99 : APERTURE_MAX - 1;

    // These are manually defined values because there is no way to programatically detect
    // min/max values at compilation (or even detect that the values are contiguous).
    static_assert(
        sizeof(units_list) == (2 + 1) * sizeof(units_list[0]), "update of invariant checks for state->unit required"
    );
    static const int max_units = 2;  // GERBV_UNIT_INCH, GERBV_UNIT_MM

    static const int max_zero_suppression = 3;  // GERBV_OMIT_ZEROS_LEADING, GERBV_OMIT_ZEROS_TRAILING,
                                                // GERBV_OMIT_ZEROS_EXPLICIT, GERBV_OMIT_ZEROS_UNSPECIFIED
    static const int max_file_section = 2;      // DRILL_NONE, DRILL_HEADER, DRILL_DATA

    static const int max_coordinate_mode = 1;  // DRILL_MODE_ABSOLUTE, DRILL_MODE_INCREMENTAL

    static_assert(
        sizeof(number_fmt_list) == (5 + 1) * sizeof(number_fmt_list[0]),
        "update of invariant checks for state->number_format required"
    );
    static const int max_number_format = 5;  // FMT_00_0000, FMT_000_000, FMT_000_00, FMT_0000_00, FMT_USER

    // verify floats/doubles encode valid numbers
    check_floating_point(state->curr_x, "state->curr_x", state, image, fd, file_line, function, code_line);
    check_floating_point(state->curr_y, "state->curr_y", state, image, fd, file_line, function, code_line);
    check_floating_point(state->origin_x, "state->origin_x", state, image, fd, file_line, function, code_line);
    check_floating_point(state->origin_y, "state->origin_y", state, image, fd, file_line, function, code_line);

    // current_tool is used as an index into image->aperture[] array
    check_min_max_int_value(
        state->current_tool, "state->current_tool", 0, max_valid_tool, state, image, fd, file_line, function, code_line
    );

    // verify valid range of enums
    check_min_max_int_value(state->unit, "state->unit", 0, max_units, state, image, fd, file_line, function, code_line);
    check_min_max_int_value(
        state->curr_section, "state->curr_section", 0, max_file_section, state, image, fd, file_line, function,
        code_line
    );
    check_min_max_int_value(
        state->coordinate_mode, "state->coordinate_mode", 0, max_coordinate_mode, state, image, fd, file_line, function,
        code_line
    );
    check_min_max_int_value(
        state->number_format, "state->number_format", 0, max_number_format, state, image, fd, file_line, function,
        code_line
    );
    check_min_max_int_value(
        state->header_number_format, "state->header_number_format", 0, max_number_format, state, image, fd, file_line,
        function, code_line
    );
    check_min_max_int_value(
        state->backup_number_format, "state->backup_number_format", 0, max_number_format, state, image, fd, file_line,
        function, code_line
    );
    check_min_max_int_value(
        image->format->omit_zeros, "image->format->omit_zeros", 0, max_zero_suppression, state, image, fd, file_line,
        function, code_line
    );

    // header_number_format must be either FMT_00_0000 (inches) or FMT_000_000 (mm)
    if ((state->header_number_format != FMT_00_0000) && (state->header_number_format != FMT_000_000)) {

        if (image->drill_stats && image->drill_stats->error_list) {
            gerbv_stats_printf(
                image->drill_stats->error_list, GERBV_MESSAGE_FATAL, -1,
                _("%s(): Drill state header_number_format is invalid: %s (must be FMT_00_0000 or FMT_000_000) in drill "
                  "file %s line %zd)\n"),
                function, number_fmt_to_string(state->header_number_format),
                fd ? fd->filename ? fd->filename : "n/a" : "n/a", file_line
            );
        }
        GERB_FATAL_ERROR(
            _("%s(): Drill state header_number_format is invalid: %s (must be FMT_00_0000 or FMT_000_000) in drill "
              "file %s line %zd)\n"),
            function, number_fmt_to_string(state->header_number_format),
            fd ? fd->filename ? fd->filename : "n/a" : "n/a", file_line
        );
    }

    if (state->curr_section == DRILL_DATA) {
        // Once the header section is done, the number format and units must match.
        if (state->number_format == FMT_USER) {
            // unit may override the default of metric, even with FMT_USER
        } else if (state->unit == GERBV_UNIT_MM && !number_fmt_implies_metric(state->number_format)) {
            if (image->drill_stats && image->drill_stats->error_list) {
                gerbv_stats_printf(
                    image->drill_stats->error_list, GERBV_MESSAGE_ERROR, -1,
                    _("%s(): unit is %s but number format %s is not %s in drill file %s line %zd)\n"), function,
                    unit_to_string(state->unit), number_fmt_to_string(state->number_format), "metric",
                    fd ? fd->filename ? fd->filename : "n/a" : "n/a", file_line
                );
            }
            dprintf(  // BUGBUG -- convert to GERB_FATAL_ERROR after fixing empty.xnc test case?
                _("%s(): unit is %s but number format %s is not %s in drill file %s line %zd)\n"), function,
                unit_to_string(state->unit), number_fmt_to_string(state->number_format), "metric",
                fd ? fd->filename ? fd->filename : "n/a" : "n/a", file_line
            );
        } else if (state->unit == GERBV_UNIT_INCH && !number_fmt_implies_imperial(state->number_format)) {
            if (image->drill_stats && image->drill_stats->error_list) {
                gerbv_stats_printf(
                    image->drill_stats->error_list, GERBV_MESSAGE_ERROR, -1,
                    _("%s(): unit is inch but number format %s is not %s in drill file %s line %zd\n"), function,
                    number_fmt_to_string(state->number_format), "imperial",
                    fd ? fd->filename ? fd->filename : "n/a" : "n/a", file_line
                );
            }
            GERB_FATAL_ERROR(
                _("%s(): unit is inch but number format %s is not %s in drill file %s line %zd\n"), function,
                number_fmt_to_string(state->number_format), "imperial",
                fd ? fd->filename ? fd->filename : "n/a" : "n/a", file_line
            );
        }
    }

    // TODO: validate leading_digits and trailing_digits matches number_format
    // TODO: Ensure test case with M70/M71 (set MM / set INCH commands) + read_double() when units switched....
    // BUGBUG: Likely failure with M70/M71 switching units ... leading_digits / trailing_digits modified?!
}

void
drill_attribute_merge(gerbv_HID_Attribute* dest, int ndest, gerbv_HID_Attribute* src, int nsrc) {
    int i;
    int j;

    /* Here is a brain dead merge algorithm which should make anyone cringe.
     * Still, it is simple and we won't merge many attributes and not
     * many times either.
     */

    for (i = 0; i < nsrc; i++) {
        /* see if our destination wants this attribute */
        j = 0;
        while (j < ndest && strcmp(src[i].name, dest[j].name) != 0) {
            j++;
        }

        /* if we wanted it and it is the same type, copy it over */
        if (j < ndest && src[i].type == dest[j].type) {
            dest[j].default_val = src[i].default_val;
        } else {
            GERB_MESSAGE("Ignoring \"%s\" attribute for drill file", src[i].name);
        }
    }
}

static void
drill_update_image_info_min_max_from_bbox(gerbv_image_info_t* info, const gerbv_render_size_t* bbox) {
    info->min_x = MIN(info->min_x, bbox->left);
    info->min_y = MIN(info->min_y, bbox->bottom);
    info->max_x = MAX(info->max_x, bbox->right);
    info->max_y = MAX(info->max_y, bbox->top);
}

/*
 * Adds the actual drill hole to the drawing
 */
static gerbv_net_t*
drill_add_drill_hole(gerbv_image_t* image, drill_state_t* state, gerbv_net_t* curr_net) {

    /* Add one to drill stats  for the current tool */
    drill_stats_increment_drill_counter(image->drill_stats->drill_list, state->current_tool);

    curr_net->next = g_new0(gerbv_net_t, 1);
    if (curr_net->next == NULL) {
        GERB_FATAL_ERROR("malloc curr_net->next failed in %s()", __FUNCTION__);
    }

    curr_net          = curr_net->next;
    curr_net->layer   = image->layers;
    curr_net->state   = image->states;
    curr_net->start_x = state->curr_x;
    curr_net->start_y = state->curr_y;
    /* KLUDGE. This function isn't allowed to return anything
       but inches */
    if (state->unit == GERBV_UNIT_MM) {
        curr_net->start_x /= 25.4;
        curr_net->start_y /= 25.4;
        /* KLUDGE. All images, regardless of input format,
           are returned in INCH format */
        curr_net->state->unit = GERBV_UNIT_INCH;
    }

    curr_net->stop_x         = curr_net->start_x - state->origin_x;
    curr_net->stop_y         = curr_net->start_y - state->origin_y;
    curr_net->aperture       = state->current_tool;
    curr_net->aperture_state = GERBV_APERTURE_STATE_FLASH;

    /* Check if aperture is set. Ignore the below instead of
       causing SEGV... */
    if (image->aperture[state->current_tool] == NULL) {
        return curr_net;
    }

    double               r    = image->aperture[state->current_tool]->parameter[0] / 2;
    gerbv_render_size_t* bbox = &curr_net->boundingBox;

    /* Set boundingBox */
    bbox->left   = curr_net->start_x - r;
    bbox->right  = curr_net->start_x + r;
    bbox->bottom = curr_net->start_y - r;
    bbox->top    = curr_net->start_y + r;

    drill_update_image_info_min_max_from_bbox(image->info, bbox);

    return curr_net;
}

/* -------------------------------------------------------------- */
gerbv_image_t*
parse_drillfile(gerb_file_t* fd, gerbv_HID_Attribute* attr_list, int n_attr, int reload) {
    /*
     * many locales redefine "." as "," and so on, so sscanf and strtod
     * has problems when reading files using %f format.
     * Fixes bug #1963618 reported by Lorenzo Marcantonio.
     */
    setlocale(LC_NUMERIC, "C");

    /* Create new image for this layer */
    dprintf("In parse_drillfile, about to create image for this layer, %s\n", fd->filename);

    gerbv_image_t* image = gerbv_create_image(NULL, "Excellon Drill File");
    if (image == NULL) {
        GERB_FATAL_ERROR("malloc image failed in %s()", __FUNCTION__);
    }

    if (reload && attr_list != NULL) {
        /* FIXME there should probably just be a function to copy an
         * attribute list including using strdup as needed
         */

        image->info->n_attr    = n_attr;
        image->info->attr_list = gerbv_attribute_dup(attr_list, n_attr);

    } else {
        /* Copy in the default attribute list for drill files.  We make a
         * copy here because we will allow per-layer editing of the
         * attributes.
         */
        image->info->n_attr    = sizeof(drill_attribute_list) / sizeof(drill_attribute_list[0]);
        image->info->attr_list = gerbv_attribute_dup(drill_attribute_list, image->info->n_attr);

        /* now merge any project attributes */
        drill_attribute_merge(image->info->attr_list, image->info->n_attr, attr_list, n_attr);
    }

    gerbv_net_t* curr_net = image->netlist;
    curr_net->layer       = image->layers;
    curr_net->state       = image->states;
    image->layertype      = GERBV_LAYERTYPE_DRILL;

    gerbv_drill_stats_t* stats = gerbv_drill_stats_new();
    if (stats == NULL) {
        GERB_FATAL_ERROR("malloc stats failed in %s()", __FUNCTION__);
    }
    image->drill_stats = stats;

    /* Create local state variable to track photoplotter state */
    drill_state_t* state = new_state();
    if (state == NULL) {
        GERB_FATAL_ERROR("malloc state failed in %s()", __FUNCTION__);
    }

    image->format = g_new0(gerbv_format_t, 1);
    if (image->format == NULL) {
        GERB_FATAL_ERROR("malloc format failed in %s()", __FUNCTION__);
    }
    image->format->omit_zeros = GERBV_OMIT_ZEROS_UNSPECIFIED;

    gerbv_HID_Attribute* hid_attrs = image->info->attr_list;
    if (!hid_attrs[HA_auto].default_val.int_value) {
        // incoming hid_attrs list says to NOT auto-detect the file format
        // therefore, have to apply the settings provided to this function
        // in the hid_attrs list.
        state->autodetect_file_format = false;
        state->number_format          = FMT_USER;
        state->leading_digits         = hid_attrs[HA_leading_digits].default_val.int_value;
        state->trailing_digits        = hid_attrs[HA_trailing_digits].default_val.int_value;

        if (GERBV_UNIT_MM == hid_attrs[HA_xy_units].default_val.int_value) {
            state->unit = GERBV_UNIT_MM;
        }

        // clang-format off
        switch (hid_attrs[HA_suppression].default_val.int_value) {
            case SUP_LEAD:  image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;  break;
            case SUP_TRAIL: image->format->omit_zeros = GERBV_OMIT_ZEROS_TRAILING; break;
            default:        image->format->omit_zeros = GERBV_OMIT_ZEROS_EXPLICIT; break;
        }
        // clang-format on
    }

    int     read      = EOF;
    ssize_t file_line = 1;

    dprintf("%s():  Starting parsing of drill file \"%s\"\n", __FUNCTION__, fd->filename);
    DUMP_STATE(state, image, fd, file_line);
    CHK_STATE(state, image, fd, file_line);

    while ((read = gerb_fgetc(fd)) != EOF) {

        DUMP_STATE(state, image, fd, file_line);
        CHK_STATE(state, image, fd, file_line);

        switch ((char)read) {
            case ';':
                /* Comment found. Eat rest of line */
                if (drill_parse_header_is_metric_comment(fd, state, image, file_line)) {
                    break;
                }
                gerb_ungetc(fd);
                gchar* tmps_semicolon = get_line(fd);
                gerbv_stats_printf(
                    stats->error_list, GERBV_MESSAGE_NOTE, -1, _("Comment \"%s\" at line %ld in file \"%s\""),
                    tmps_semicolon, file_line, fd->filename
                );
                dprintf("    Comment with ';' \"%s\" at line %ld\n", tmps_semicolon, file_line);
                g_free(tmps_semicolon);
                break;

            case 'D':
                gerb_ungetc(fd);
                gchar* tmps_D = get_line(fd);
                if (strcmp(tmps_D, "DETECT,ON") == 0 || strcmp(tmps_D, "DETECT,OFF") == 0) {
                    gchar* new_addition;
                    if (strcmp(tmps_D, "DETECT,ON") == 0) {
                        new_addition = "ON";
                    } else {
                        new_addition = "OFF";
                    }

                    /* broken tool detect on/off.  Silently ignored. */
                    gchar* appended_to_prior_stats;
                    if (stats->detect) {
                        appended_to_prior_stats = g_strdup_printf("%s\n%s", stats->detect, new_addition);
                        g_free(stats->detect);
                    } else {
                        appended_to_prior_stats = g_strdup_printf("%s", new_addition);
                    }
                    stats->detect = appended_to_prior_stats;
                } else {
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_ERROR, -1,
                        _("Unrecognised string \"%s\" in header "
                          "at line %ld in file \"%s\""),
                        tmps_D, file_line, fd->filename
                    );
                }
                g_free(tmps_D);
                break;

            case 'F':
                gerb_ungetc(fd);
                gchar* tmps_F = get_line(fd);

                /* Silently ignore FMAT,2.  Not sure what others are allowed */
                if (0 == strcmp(tmps_F, "FMAT,2")) {
                    g_free(tmps_F);
                    break;
                }

                if (0 == strcmp(tmps_F, "FMAT,1")) {
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_ERROR, -1,
                        _("File in unsupported format 1 "
                          "at line %ld in file \"%s\""),
                        file_line, fd->filename
                    );

                    // Interesting ... don't see this exit condition often
                    gerbv_destroy_image(image);
                    g_free(tmps_F);

                    return NULL;
                }

                gerbv_stats_printf(
                    stats->error_list, GERBV_MESSAGE_ERROR, -1,
                    _("Unrecognised string \"%s\" in header "
                      "at line %ld in file \"%s\""),
                    tmps_F, file_line, fd->filename
                );

                g_free(tmps_F);
                break;

            case 'G':
                {
                    /* Most G codes aren't used, for now */
                    drill_g_code_t g_code;

                    // TODO: Document position that file should be at when this returns a valid code
                    switch (g_code = drill_parse_G_code(fd, image, file_line)) {

                        case DRILL_G_DRILL:
                            /* Drill mode */
                            break;

                        case DRILL_G_SLOT:
                            {
                                // Command format: X#Y#G85X#Y#
                                // TODO -- does drill_parse_G_code() update curr_x and curr_y?
                                //
                                /* Parse drilled slot end coords */

                                // TODO -- hand-validate this code path
                                int read_after_G85_code_parsed = gerb_fgetc(fd);
                                if (EOF == read_after_G85_code_parsed) {
                                    gerbv_stats_printf(
                                        stats->error_list, GERBV_MESSAGE_ERROR, -1,
                                        _("Unexpected EOF found in file \"%s\""), fd->filename
                                    );
                                    break;
                                }

                                drill_parse_coordinate(fd, read_after_G85_code_parsed, image, state, file_line);

                                /* Modify last curr_net as drilled slot */
                                curr_net->stop_x = state->curr_x;
                                curr_net->stop_y = state->curr_y;

                                if (state->unit == GERBV_UNIT_MM) {
                                    /* Convert to inches -- internal units */
                                    curr_net->stop_x /= 25.4;
                                    curr_net->stop_y /= 25.4;
                                }

                                /* Update boundingBox with drilled slot stop_x,y coords */
                                double               r    = image->aperture[state->current_tool]->parameter[0] / 2;
                                gerbv_render_size_t* bbox = &curr_net->boundingBox;
                                bbox->left                = MIN(bbox->left, curr_net->stop_x - r);
                                bbox->right               = MAX(bbox->right, curr_net->stop_x + r);
                                bbox->bottom              = MIN(bbox->bottom, curr_net->stop_y - r);
                                bbox->top                 = MAX(bbox->top, curr_net->stop_y + r);

                                drill_update_image_info_min_max_from_bbox(image->info, bbox);

                                curr_net->aperture_state = GERBV_APERTURE_STATE_ON;

                                break;
                            }

                        case DRILL_G_ABSOLUTE: state->coordinate_mode = DRILL_MODE_ABSOLUTE; break;

                        case DRILL_G_INCREMENTAL: state->coordinate_mode = DRILL_MODE_INCREMENTAL; break;

                        case DRILL_G_ZEROSET:  // G93X#Y#
                            ;                  // label must be part of a statement, so use empty statement.
                            int read_after_G85_code_parsed = gerb_fgetc(fd);
                            if (EOF == read_after_G85_code_parsed) {
                                gerbv_stats_printf(
                                    stats->error_list, GERBV_MESSAGE_ERROR, -1,
                                    _("Unexpected EOF found in file \"%s\""), fd->filename
                                );
                                break;
                            }

                            drill_parse_coordinate(fd, (char)read_after_G85_code_parsed, image, state, file_line);
                            state->origin_x = state->curr_x;
                            state->origin_y = state->curr_y;
                            break;

                        case DRILL_G_UNKNOWN:  // not a real G-code
                            ;                  // label must be part of a statement, so use empty statement.
                            gchar* tmps_G_unknown = get_line(fd);
                            gerbv_stats_printf(
                                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                                _("Unrecognized string \"%s\" found "
                                  "at line %ld in file \"%s\""),
                                tmps_G_unknown, file_line, fd->filename
                            );
                            g_free(tmps_G_unknown);
                            break;

                        default:
                            eat_line(fd);
                            gerbv_stats_printf(
                                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                                _("Unsupported G%02d (%s) code "
                                  "at line %ld in file \"%s\""),
                                g_code, drill_g_code_name(g_code), file_line, fd->filename
                            );
                            break;
                    }

                    break;
                }

            case 'I':
                gerb_ungetc(fd); /* To compare full string in function or
                        report full string  */
                if (drill_parse_header_is_inch(fd, state, image, file_line)) {
                    break;
                }

                if (drill_parse_header_is_ici(fd, state, image, file_line)) {
                    break;
                }

                gchar* tmps_I = get_line(fd);
                gerbv_stats_printf(
                    stats->error_list, GERBV_MESSAGE_ERROR, -1,
                    _("Unrecognized string \"%s\" found "
                      "at line %ld in file \"%s\""),
                    tmps_I, file_line, fd->filename
                );
                g_free(tmps_I);

                break;

            case 'M':
                {
                    // TODO - document expectations of file pointer location after calling `drill_parse_M_code()`
                    int m_code = drill_parse_M_code(fd, state, image, file_line);

                    switch (m_code) {
                        case DRILL_M_HEADER: state->curr_section = DRILL_HEADER; break;
                        case DRILL_M_HEADEREND:
                            state->curr_section = DRILL_DATA;

                            if (image->format->omit_zeros == GERBV_OMIT_ZEROS_UNSPECIFIED) {
                                /* Excellon says they default to specify leading
                                   zeros, i.e. omit trailing zeros.	 The Excellon
                                   files floating around that don't specify the
                                   leading/trailing zeros in the header seem to
                                   contradict to this though.

                                   XXX We should probably ask the user. */

                                gerbv_stats_printf(
                                    stats->error_list, GERBV_MESSAGE_ERROR, -1,
                                    _("End of Excellon header reached "
                                      "but no leading/trailing zero "
                                      "handling specified "
                                      "at line %ld in file \"%s\""),
                                    file_line, fd->filename
                                );
                                gerbv_stats_printf(
                                    stats->error_list, GERBV_MESSAGE_WARNING, -1,
                                    _("Assuming leading zeros in file \"%s\""), fd->filename
                                );
                                image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
                            }
                            break;
                        case DRILL_M_METRIC:
                            if (state->unit == GERBV_UNIT_UNSPECIFIED && state->curr_section != DRILL_HEADER) {

                                gerbv_stats_printf(
                                    stats->error_list, GERBV_MESSAGE_WARNING, -1,
                                    _("M71 code found but no METRIC "
                                      "specification in header "
                                      "at line %ld in file \"%s\""),
                                    file_line, fd->filename
                                );
                                gerbv_stats_printf(
                                    stats->error_list, GERBV_MESSAGE_WARNING, -1,
                                    _("Assuming all tool sizes are MM in file \"%s\""), fd->filename
                                );

                                stats = image->drill_stats;
                                for (int tool_num = TOOL_MIN; tool_num < TOOL_MAX; tool_num++) {
                                    if (image->aperture && image->aperture[tool_num]) {
                                        /* First update stats.   Do this before changing drill dias.
                                         * Maybe also put error into stats? */
                                        double size = image->aperture[tool_num]->parameter[0];
                                        drill_stats_modify_drill_list(stats->drill_list, tool_num, size, "MM");
                                        /* Now go back and update all tool dias, since
                                         * tools are displayed in inch units
                                         */
                                        image->aperture[tool_num]->parameter[0] /= 25.4;
                                    }
                                }
                            }
                            if (state->autodetect_file_format) {
                                state->number_format = state->backup_number_format;
                                state->unit          = GERBV_UNIT_MM;
                            }
                            break;
                        case DRILL_M_IMPERIAL:
                            if (state->autodetect_file_format) {
                                if (state->number_format != FMT_00_0000) {
                                    /* save metric format definition for later */
                                    state->backup_number_format = state->number_format;
                                }
                                state->number_format   = FMT_00_0000;
                                state->leading_digits  = 2;
                                state->trailing_digits = 4;
                                state->unit            = GERBV_UNIT_INCH;
                            }

                            break;
                        case DRILL_M_CANNEDTEXTX:  // M97
                        case DRILL_M_CANNEDTEXTY:  // M98
                            ;                      // label must be part of a statement, so use empty statement.
                            gchar* tmps_M97_M98 = get_line(fd);
                            gerbv_stats_printf(
                                stats->error_list, GERBV_MESSAGE_NOTE, -1,
                                _("Canned text \"%s\" "
                                  "at line %ld in drill file \"%s\""),
                                tmps_M97_M98, file_line, fd->filename
                            );
                            g_free(tmps_M97_M98);
                            break;
                        case DRILL_M_MESSAGELONG:  // M45
                        case DRILL_M_MESSAGE:      // M47
                            ;                      // label must be part of a statement, so use empty statement.
                            gchar* tmps_M45_M47 = get_line(fd);
                            gerbv_stats_printf(
                                stats->error_list, GERBV_MESSAGE_NOTE, -1,
                                _("Message \"%s\" embedded "
                                  "at line %ld in drill file \"%s\""),
                                tmps_M45_M47, file_line, fd->filename
                            );
                            g_free(tmps_M45_M47);
                            break;
                        case DRILL_M_PATTERNEND:
                        case DRILL_M_TOOLTIPCHECK: break;

                        case DRILL_M_END:
                            /* M00 has optional arguments */
                            eat_line(fd);

                        case DRILL_M_ENDREWIND: goto drill_parse_end; break;

                        case DRILL_M_UNKNOWN:
                            gerb_ungetc(fd);
                            /* To compare full string in function or report full string  */
                            if (drill_parse_header_is_metric(fd, state, image, file_line)) {
                                break;
                            }

                            stats->M_unknown++;
                            gchar* tmps_M_UNKNOWN = get_line(fd);
                            gerbv_stats_printf(
                                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                                _("Unrecognized string \"%s\" found "
                                  "at line %ld in file \"%s\""),
                                tmps_M_UNKNOWN, file_line, fd->filename
                            );
                            g_free(tmps_M_UNKNOWN);

                            break;

                        default:
                            stats->M_unknown++;
                            gerbv_stats_printf(
                                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                                _("Unsupported M%02d (%s) code found "
                                  "at line %ld in file \"%s\""),
                                m_code, _(drill_m_code_name(m_code)), file_line, fd->filename
                            );
                            break;
                    } /* switch(m_code) */

                    break;
                } /* case 'M' */

            case 'R':
                if (state->curr_section == DRILL_HEADER) {
                    stats->unknown++;
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_ERROR, -1,
                        _("Not allowed 'R' code in the header "
                          "at line %ld in file \"%s\""),
                        file_line, fd->filename
                    );
                } else {

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

                    /* figure out how many repeats there are */
                    int c    = gerb_fgetc(fd);
                    int rcnt = 0;
                    while ('0' <= c && c <= '9') {
                        rcnt = 10 * rcnt + (c - '0');
                        c    = gerb_fgetc(fd);
                    }
                    dprintf("working on R code (repeat) with a number of reps equal to %d\n", rcnt);

                    // read optional X and Y values
                    double step_x = 0.0;
                    double step_y = 0.0;
                    if (c == 'X') {
                        step_x = read_double(
                            fd, state->number_format, image->format->omit_zeros, state->leading_digits,
                            state->trailing_digits
                        );
                        c = gerb_fgetc(fd);
                    }
                    if (c == 'Y') {
                        step_y = read_double(
                            fd, state->number_format, image->format->omit_zeros, state->leading_digits,
                            state->trailing_digits
                        );
                    } else {
                        gerb_ungetc(fd);
                    }
                    dprintf(
                        "Getting ready to repeat the drill %d times with delta_x = %g, delta_y = %g\n", rcnt, step_x,
                        step_y
                    );

                    /* spit out the drills */
                    double start_x = state->curr_x;
                    double start_y = state->curr_y;
                    for (c = 1; c <= rcnt; c++) {
                        state->curr_x = start_x + c * step_x;
                        state->curr_y = start_y + c * step_y;
                        dprintf("    Repeat #%d - new location is (%g, %g)\n", c, state->curr_x, state->curr_y);
                        curr_net = drill_add_drill_hole(image, state, curr_net);
                    }
                }

            case 'S':
                gerbv_stats_printf(
                    stats->error_list, GERBV_MESSAGE_NOTE, -1,
                    _("Ignoring setting spindle speed "
                      "at line %ld in drill file \"%s\""),
                    file_line, fd->filename
                );
                eat_line(fd);
                break;
            case 'T': drill_parse_T_code(fd, state, image, file_line); break;
            case 'V':
                gerb_ungetc(fd);

                gchar* tmps_V = get_line(fd);
                /* Silently ignore VER,1.  Not sure what others are allowed */
                if (0 != strcmp(tmps_V, "VER,1")) {
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_NOTE, -1,
                        _("Undefined string \"%s\" in header "
                          "at line %ld in file \"%s\""),
                        tmps_V, file_line, fd->filename
                    );
                }
                g_free(tmps_V);
                break;

            case 'X':
            case 'Y':
                /* Hole coordinate found. Do some parsing */
                drill_parse_coordinate(fd, read, image, state, file_line);

                /* add the new drill hole */
                curr_net = drill_add_drill_hole(image, state, curr_net);
                break;

            case '%': state->curr_section = DRILL_DATA; break;

            case '\n':
                file_line++;

                /* Get <CR> char, if any, from <LF><CR> pair */
                int possible_LF = gerb_fgetc(fd);
                if (possible_LF != '\r' && possible_LF != EOF) {
                    gerb_ungetc(fd);
                }
                break;

            case '\r':
                file_line++;

                /* Get <LF> char, if any, from <CR><LF> pair */
                int possible_ret = gerb_fgetc(fd);
                if (possible_ret != '\n' && possible_ret != EOF) {
                    gerb_ungetc(fd);
                }
                break;

            case ' ': /* White space */
            case '\t': break;

            default:
                stats->unknown++;

                if (DRILL_HEADER == state->curr_section) {
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_ERROR, -1,
                        _("Undefined code '%s' (0x%x) found in header "
                          "at line %ld in file \"%s\""),
                        gerbv_escape_char(read), read, file_line, fd->filename
                    );
                    gerb_ungetc(fd);

                    /* Unrecognised crap in the header is thrown away */
                    gchar* tmps_unknown_header_line = get_line(fd);
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_WARNING, -1,
                        _("Unrecognised string \"%s\" in header "
                          "at line %ld in file \"%s\""),
                        tmps_unknown_header_line, file_line, fd->filename
                    );
                    g_free(tmps_unknown_header_line);
                } else {
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_ERROR, -1,
                        _("Ignoring undefined character '%s' (0x%x) "
                          "found inside data at line %ld in file \"%s\""),
                        gerbv_escape_char(read), read, file_line, fd->filename
                    );
                }
        }
    }

    gerbv_stats_printf(
        stats->error_list, GERBV_MESSAGE_ERROR, -1, _("No EOF found in drill file \"%s\""), fd->filename
    );

drill_parse_end:
    dprintf("%s():  Populating file attributes\n", __FUNCTION__);

    hid_attrs = image->info->attr_list;

    // update the stored attributes ... allow drill state to be restored to same point
    // Note that HA_auto (state->autodetect_file_format) does not get updated,
    // which means that autodetection will be re-enabled in later drill files.
    // BUGBUG -- Verify this is the intended behavior.
    //           The alternative is to set HA_auto to 0 here, which would
    //           ensure that a later drill file must use the same format and units?
    switch (state->unit) {
        // clang-format off
        case GERBV_UNIT_INCH: hid_attrs[HA_xy_units].default_val.int_value = GERBV_UNIT_INCH; break;
        case GERBV_UNIT_MM:   hid_attrs[HA_xy_units].default_val.int_value = GERBV_UNIT_MM;   break;
        default:              hid_attrs[HA_xy_units].default_val.int_value = GERBV_UNIT_INCH; break;
            // clang-format on
    }

    switch (state->number_format) {
        case FMT_00_0000:
            hid_attrs[HA_leading_digits].default_val.int_value  = 2;
            hid_attrs[HA_trailing_digits].default_val.int_value = 4;
            break;
        case FMT_000_000:
            hid_attrs[HA_leading_digits].default_val.int_value  = 3;
            hid_attrs[HA_trailing_digits].default_val.int_value = 3;
        case FMT_000_00:
            hid_attrs[HA_leading_digits].default_val.int_value  = 3;
            hid_attrs[HA_trailing_digits].default_val.int_value = 2;
        case FMT_0000_00:
            hid_attrs[HA_leading_digits].default_val.int_value  = 4;
            hid_attrs[HA_trailing_digits].default_val.int_value = 2;
        case FMT_USER:
            hid_attrs[HA_leading_digits].default_val.int_value  = state->leading_digits;
            hid_attrs[HA_trailing_digits].default_val.int_value = state->trailing_digits;
            break;

        default: break;
    }

    // clang-format off
    switch (image->format->omit_zeros) {
        case GERBV_OMIT_ZEROS_LEADING:     hid_attrs[HA_suppression].default_val.int_value = SUP_LEAD;  break;
        case GERBV_OMIT_ZEROS_TRAILING:    hid_attrs[HA_suppression].default_val.int_value = SUP_TRAIL; break;
        case GERBV_OMIT_ZEROS_EXPLICIT:    hid_attrs[HA_suppression].default_val.int_value = SUP_NONE;  break;
        case GERBV_OMIT_ZEROS_UNSPECIFIED: hid_attrs[HA_suppression].default_val.int_value = SUP_NONE;  break;
        default:                           hid_attrs[HA_suppression].default_val.int_value = SUP_NONE;  break;
    }
    // clang-format on

    g_free(state);

    return image;
} /* parse_drillfile */

/* -------------------------------------------------------------- */
/*
 * Checks for signs that this is a drill file
 * Returns TRUE if it is, FALSE if not.
 */
gboolean
drill_file_p(gerb_file_t* fd, gboolean* returnFoundBinary) {
    char *   buf = NULL, *tbuf;
    int      len = 0;
    char*    letter;
    int      ascii;
    int      zero = 48; /* ascii 0 */
    int      nine = 57; /* ascii 9 */
    int      i;
    gboolean found_binary  = FALSE;
    gboolean found_M48     = FALSE;
    gboolean found_M30     = FALSE;
    gboolean found_percent = FALSE;
    gboolean found_T       = FALSE;
    gboolean found_X       = FALSE;
    gboolean found_Y       = FALSE;
    gboolean end_comments  = FALSE;

    tbuf = g_malloc(MAXL);
    if (tbuf == NULL) {
        GERB_FATAL_ERROR("malloc buf failed while checking for drill file in %s()", __FUNCTION__);
    }

    while (fgets(tbuf, MAXL, fd->fd) != NULL) {
        len = strlen(tbuf);
        buf = tbuf;
        /* check for comments at top of file.  */
        if (!end_comments) {
            if (g_strstr_len(buf, len, ";") != NULL) { /* comments at top of file  */
                for (i = 0; i < len - 1; ++i) {
                    if (buf[i] == '\n' && buf[i + 1] != ';' && buf[i + 1] != '\r' && buf[i + 1] != '\n') {
                        end_comments = TRUE;
                        /* Set rest of parser to end of
                         * comments */
                        buf = &tbuf[i + 1];
                    }
                }
                if (!end_comments) {
                    continue;
                }
            } else {
                end_comments = TRUE;
            }
        }

        /* First look through the file for indications of its type */
        len = strlen(buf);
        /* check that file is not binary (non-printing chars) */
        for (i = 0; i < len; i++) {
            ascii = (int)buf[i];
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
            if ((letter[1] == '\r') || (letter[1] == '\n')) {
                found_percent = TRUE;
            }
        }

        /* Check for T<number> */
        if ((letter = g_strstr_len(buf, len, "T")) != NULL) {
            if (!found_T && (found_X || found_Y)) {
                found_T = FALSE; /* Found first T after X or Y */
            } else {
                if (isdigit((int)letter[1])) { /* verify next char is digit */
                    found_T = TRUE;
                }
            }
        }

        /* look for X<number> or Y<number> */
        if ((letter = g_strstr_len(buf, len, "X")) != NULL) {
            ascii = (int)letter[1]; /* grab char after X */
            if ((ascii >= zero) && (ascii <= nine)) {
                found_X = TRUE;
            }
        }
        if ((letter = g_strstr_len(buf, len, "Y")) != NULL) {
            ascii = (int)letter[1]; /* grab char after Y */
            if ((ascii >= zero) && (ascii <= nine)) {
                found_Y = TRUE;
            }
        }
    } /* while (fgets(buf, MAXL, fd->fd) */

    rewind(fd->fd);
    g_free(tbuf);
    *returnFoundBinary = found_binary;

    /* Now form logical expression determining if this is a drill file */
    if (((found_X || found_Y) && found_T) && (found_M48 || (found_percent && found_M30))) {
        return TRUE;
    } else if (found_M48 && found_percent && found_M30) {
        /* Pathological case of drill file with valid header
           and EOF but no drill XY locations. */
        return TRUE;
    } else {
        return FALSE;
    }
} /* drill_file_p */

static bool
drill_parse_T_is_TCST(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {

    // called with file pointer pointing just after the initial 'T'
    (void)state; /* unused parameter ... keeps stack the same */

    int ch2 = gerb_fgetc(fd);
    if (ch2 == 'C') {
        int ch3 = gerb_fgetc(fd);
        if (ch3 == 'S') {
            int ch4 = gerb_fgetc(fd);
            if (ch4 == 'T') {
                // rewind four characters, then get the whole line
                // for storing in the statistics' error list
                fd->ptr -= 4;
                gchar* tcst_line = get_line(fd++);
                gerbv_stats_printf(
                    image->drill_stats->error_list, GERBV_MESSAGE_NOTE, -1,
                    _("Tool change stop switch found \"%s\" "
                      "at line %ld in file \"%s\""),
                    tcst_line, file_line, fd->filename
                );
                g_free(tcst_line);
                return true;
            }
            if (ch4 != EOF) {
                gerb_ungetc(fd);
            }
        }
        if (ch3 != EOF) {
            gerb_ungetc(fd);
        }
    }
    if (ch2 != EOF) {
        gerb_ungetc(fd);
    }
    // and the file pointer is back to where it was when this function was called
    return false;
}

static bool
drill_parse_T_needs_Orcad_Hack_or_EOF(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {
    // this will always leave the file pointer at the same location as it was when this function was called
    int temp = gerb_fgetc(fd);

    if (temp == EOF) {
        return true;
    }

    // Orcad has a bug where it will put junk text in place of a tool definition
    // Make sure the next character is a digit or a sign
    if (!(isdigit(temp) != 0 || temp == '+' || temp == '-')) {
        gerbv_stats_printf(
            image->drill_stats->error_list, GERBV_MESSAGE_ERROR, -1,
            _("OrCAD bug: Junk text found in place of tool definition")
        );
        gchar* orcad_junk_text = get_line(fd);
        gerbv_stats_printf(
            image->drill_stats->error_list, GERBV_MESSAGE_WARNING, -1, _("Junk text \"%s\" at line %ld in file \"%s\""),
            orcad_junk_text, file_line, fd->filename
        );
        g_free(orcad_junk_text);
        gerbv_stats_printf(image->drill_stats->error_list, GERBV_MESSAGE_WARNING, -1, _("Ignoring junk text"));
        return true;
    }
    // return the file pointer to where it was when this function was called
    if (temp != EOF) {
        // yes, the above if statement isn't needed this time.
        // Keeping as it's a good habit for gerb_file_t manipulation.
        gerb_ungetc(fd);
    }
    return false;
}

/* -------------------------------------------------------------- */
/* Parse tool definition. This can get a bit tricky since it can
   appear in the header and/or data section.
   Returns tool number on success, -1 on error */
static int
drill_parse_T_code(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {

    if (drill_parse_T_is_TCST(fd, state, image, file_line)) {
        return -1;
    }
    if (drill_parse_T_needs_Orcad_Hack_or_EOF(fd, state, image, file_line)) {
        return -1;
    }
    gerbv_error_list_t* error_list = image->drill_stats->error_list;

    dprintf("---> entering %s()...\n", __FUNCTION__);

    int tool_num = (int)gerb_fgetint(fd, NULL);
    dprintf("  Handling tool T%d at line %ld\n", tool_num, file_line);

    if (tool_num == 0) {
        return tool_num; /* T00 is a command to unload the drill */
    }
    /* Limit valid tools to be [1..9999] */
    if (tool_num < TOOL_MIN || tool_num >= TOOL_MAX) {
        gerbv_stats_printf(
            error_list, GERBV_MESSAGE_ERROR, -1,
            _("Out of bounds drill number %d "
              "at line %ld in file \"%s\""),
            tool_num, file_line, fd->filename
        );
        return -1;
    }

    /* Set the current tool to the correct one */
    state->current_tool     = tool_num;
    gerbv_aperture_t* apert = image->aperture[tool_num];

    /* Check for a size definition */

    /* Repeatedly scan for tool definitions of form T#(F#|S#|C#)*
     * At this point, have read 'T#' ...
     */
    int      next_char = gerb_fgetc(fd);
    gboolean done      = FALSE;
    while (!done) {
        switch ((char)next_char) {
            case 'F':
            case 'S':
                /* Silently ignored F/S and the integer that follows. They're not important. */
                gerb_fgetint(fd, NULL);
                break;

            case 'C':  // C# provides a drill size
                ;      // label must be part of a statement, so use empty statement.
                double size = read_double(
                    fd, state->header_number_format, GERBV_OMIT_ZEROS_TRAILING, state->leading_digits,
                    state->trailing_digits
                );
                dprintf("  Read a size of %g\n", size);

                if (state->unit == GERBV_UNIT_MM) {
                    size /= 25.4;
                } else if (size >= 4.0) {
                    /* If the drill size is >= 4 inches, assume that this
                       must be wrong and that the units are mils.
                       The limit being 4 inches is because the smallest drill
                       I've ever seen used is 0,3mm(about 12mil). Half of that
                       seemed a bit too small a margin, so a third it is */

                    gerbv_stats_printf(
                        error_list, GERBV_MESSAGE_ERROR, -1,
                        _("Read a drill of diameter %g inches "
                          "at line %ld in file \"%s\""),
                        size, file_line, fd->filename
                    );
                    gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1, _("Assuming units are mils"));
                    size /= 1000.0;
                }

                if (size <= 0. || size >= 10000.) {
                    gerbv_stats_printf(
                        error_list, GERBV_MESSAGE_ERROR, -1,
                        _("Unreasonable drill size %g found for drill %d "
                          "at line %ld in file \"%s\""),
                        size, tool_num, file_line, fd->filename
                    );
                } else {
                    if (apert != NULL) {
                        /* allow a redefine of a tool only if the new definition is exactly the same.
                         * This avoid lots of spurious complaints with the output of some cad
                         * tools while keeping complaints if there is a true problem
                         */
                        if (apert->parameter[0] != size || apert->type != GERBV_APTYPE_CIRCLE
                            || apert->nuf_parameters != 1 || apert->unit != GERBV_UNIT_INCH) {

                            gerbv_stats_printf(
                                error_list, GERBV_MESSAGE_ERROR, -1,
                                _("Found redefinition of drill %d "
                                  "at line %ld in file \"%s\""),
                                tool_num, file_line, fd->filename
                            );
                        }
                    } else {
                        apert = image->aperture[tool_num] = g_new0(gerbv_aperture_t, 1);
                        if (apert == NULL) {
                            GERB_FATAL_ERROR("malloc tool failed in %s()", __FUNCTION__);
                        }

                        /* There's really no way of knowing what unit the tools
                           are defined in without sneaking a peek in the rest of
                           the file first. That's done in drill_guess_format() */
                        apert->parameter[0]   = size;
                        apert->type           = GERBV_APTYPE_CIRCLE;
                        apert->nuf_parameters = 1;
                        apert->unit           = GERBV_UNIT_INCH;
                    }
                }

                /* Add the tool whose definition we just found into the list
                 * of tools for this layer used to generate statistics. */
                gchar* tool_description = g_strdup_printf("%s", (state->unit == GERBV_UNIT_MM ? _("mm") : _("inch")));
                drill_stats_add_to_drill_list(
                    image->drill_stats->drill_list, tool_num, state->unit == GERBV_UNIT_MM ? size * 25.4 : size,
                    tool_description
                );
                g_free(tool_description);
                break;

            default:
                /* Stop when finding anything but what's expected
                   (and put it back) */
                gerb_ungetc(fd);
                done = TRUE;
                break;
        } /* switch((char)temp) */

        next_char = gerb_fgetc(fd);
        if (EOF == next_char) {
            gerbv_stats_printf(
                error_list, GERBV_MESSAGE_ERROR, -1,
                _("Unexpected EOF encountered in header of "
                  "drill file \"%s\""),
                fd->filename
            );
            done = TRUE;
        } else if (next_char == '\n' || next_char == '\r') {
            gerb_ungetc(fd);
            done = TRUE;
        }
    } /* while(!done) */ /* Done looking at tool definitions */

    /* Catch the tools that aren't defined.
       This isn't strictly a good thing, but at least something is shown */
    if (apert == NULL) {

        apert = image->aperture[tool_num] = g_new0(gerbv_aperture_t, 1);
        if (apert == NULL) {
            GERB_FATAL_ERROR("malloc tool failed in %s()", __FUNCTION__);
        }

        /* See if we have the tool table */
        double dia = gerbv_get_tool_diameter(tool_num);
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
             * tool_num already verified non-zero and in range [1..9999], above.
             */
            gerbv_stats_printf(
                error_list, GERBV_MESSAGE_ERROR, -1,
                _("Tool %02d used without being defined "
                  "at line %ld in file \"%s\""),
                tool_num, file_line, fd->filename
            );
            gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1, _("Setting a default size of %g\""), dia);
        }

        apert->type           = GERBV_APTYPE_CIRCLE;
        apert->nuf_parameters = 1;
        apert->parameter[0]   = dia;

        /* Add the tool whose definition we just found into the list
         * of tools for this layer used to generate statistics.
         * tool_num already verified non-zero and in range [1..9999], above.
         */

        gerbv_drill_list_t* drill_list = image->drill_stats->drill_list;
        gchar* tool_unit_string        = g_strdup_printf("%s", (state->unit == GERBV_UNIT_MM ? _("mm") : _("inch")));
        drill_stats_add_to_drill_list(
            drill_list, tool_num, state->unit == GERBV_UNIT_MM ? dia * 25.4 : dia, tool_unit_string
        );
        g_free(tool_unit_string);
    } /* if(image->aperture[tool_num] == NULL) */

    dprintf("<----  ...leaving %s()\n", __FUNCTION__);

    return tool_num;
} /* drill_parse_T_code() */

/* -------------------------------------------------------------- */
static drill_m_code_t
drill_parse_M_code(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {
    gerbv_drill_stats_t* stats = image->drill_stats;
    drill_m_code_t       m_code;
    char                 op[3];

    dprintf("---> entering %s() ...\n", __FUNCTION__);

    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    op[2] = '\0';

    if (op[0] == EOF || op[1] == EOF) {
        gerbv_stats_printf(
            stats->error_list, GERBV_MESSAGE_ERROR, -1, _("Unexpected EOF found while parsing M-code in file \"%s\""),
            fd->filename
        );

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
        case 1: stats->M01++; break;
        case 18: stats->M18++; break;
        case 25: stats->M25++; break;
        case 30: stats->M30++; break;
        case 45: stats->M45++; break;
        case 47: stats->M47++; break;
        case 48: stats->M48++; break;
        case 71:
            stats->M71++;
            eat_line(fd);
            break;
        case 72:
            stats->M72++;
            eat_line(fd);
            break;
        case 95: stats->M95++; break;
        case 97: stats->M97++; break;
        case 98: stats->M98++; break;

        default:
        case DRILL_M_UNKNOWN: break;
    }

    dprintf("<----  ...leaving %s()\n", __FUNCTION__);

    return m_code;
} /* drill_parse_M_code() */

/* -------------------------------------------------------------- */
static int
drill_parse_header_is_metric(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {
    gerbv_drill_stats_t* stats = image->drill_stats;
    char                 c;
    char                 op[3];

    dprintf("    %s(): entering\n", __FUNCTION__);

    /* METRIC is not an actual M code but a command that is only
     * acceptable within the header.
     *
     * The syntax is
     * METRIC[,{TZ|LZ}][,{000.000|000.00|0000.00}]
     */

    if (DRILL_HEADER != state->curr_section) {
        return 0;
    }

    switch (file_check_str(fd, "METRIC")) {
        case -1:
            gerbv_stats_printf(
                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                _("Unexpected EOF found while parsing \"%s\" string "
                  "in file \"%s\""),
                "METRIC", fd->filename
            );
            return 0;

        case 0: return 0;
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
                if ('Z' != gerb_fgetc(fd)) {
                    goto header_junk;
                }

                if (c == 'L') {
                    // NOTE: `LZ` means _include_ the leading zeros; thus, `LZ` means to _omit_ trailing zeros.
                    //       Would have been preferable to directly mirror file format logic, by tracking
                    //       whether leading zeros or trailing zeros are KEPT.  Que cera cera.
                    dprintf(
                        "    %s(): Detected a file with trailing trailing zero suppression / keeping leading zeros "
                        "(`LZ` found)\n",
                        __FUNCTION__
                    );
                    if (state->autodetect_file_format) {
                        image->format->omit_zeros = GERBV_OMIT_ZEROS_TRAILING;
                    }
                } else {
                    // NOTE: `TZ` means _include_ the trailing zeros; thus, `TZ` means to _omit_ leading zeros.
                    //       Would have been preferable to directly mirror file format logic, by tracking
                    //       whether leading zeros or trailing zeros are KEPT.  Que cera cera.
                    dprintf(
                        "    %s(): Detected a file with leading zero suppression / keeping trailing zeros (`TZ` "
                        "found)\n",
                        __FUNCTION__
                    );
                    if (state->autodetect_file_format) {
                        image->format->omit_zeros = GERBV_OMIT_ZEROS_LEADING;
                    }
                }

                // The `METRIC` line was already found.
                // From prior comments, anytime zero suppression is enabled,
                // the header's number format should become hard-coded to FMT_000_000.
                // However, this is **_ONLY_** done when `autodetect_file_format` is true
                // **_AND_** the number_format was not already set to a user-specified format.
                // (e.g., via a `;FILE_FORMAT=3:4` line)
                //
                // TODO -- determine if this is the intended behavior?
                if (state->autodetect_file_format && state->number_format != FMT_USER) {
                    /* Default metric number format is 6-digit, 1 um
                     * resolution.  The header number format (for T#C#
                     * definitions) is fixed to that, while the number
                     * format within the file can differ.  If the
                     * number_format is already FMT_USER, that means that
                     * we have set the number format in another way, maybe
                     * with one of the altium FILE_FORMAT= style comments,
                     * so don't do this default. */
                    state->header_number_format = FMT_000_000;  // May only be FMT_00_0000 (inches) or FMT_000_000 (mm)
                    state->number_format        = FMT_000_000;
                    state->leading_digits       = 3;
                    state->trailing_digits      = 3;
                }

                if (',' == gerb_fgetc(fd)) {
                    /* Anticipate number format will follow */
                    goto header_again;
                }

                gerb_ungetc(fd);

                break;

            case '0':
                /* METRIC allows optionally specifying the number format:
                 *     METRIC[,{TZ|LZ}][,{000.000|000.00|0000.00}
                 *
                 * All three valid formats start with `000`,
                 * so check for those next two zeros....
                 */
                if ('0' != gerb_fgetc(fd)) {
                    // read one extra character that didn't match
                    gerb_ungetc(fd);
                    goto header_junk;
                }
                if ('0' != gerb_fgetc(fd)) {
                    // read two extra characters that didn't match
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    goto header_junk;
                }

                /* We just parsed "000" from the file.
                 * The remainder valid metric options are:    .000 |    .00 |    0.00
                 *                   which corresponds to: 000.000 | 000.00 | 0000.00
                 */
                op[0] = gerb_fgetc(fd);
                op[1] = gerb_fgetc(fd);
                op[2] = '\0';
                if (EOF == op[0]) {
                    // read three extra characters that didn't match (EOF doesn't move ptr)
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    goto header_junk;
                }
                if (EOF == op[1]) {
                    // read four extra characters that didn't match (EOF doesn't move ptr)
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    goto header_junk;
                }

                /* We just parsed "000" from the file and have two more characters in op[].
                 * The remainder valid metric options are:    .000 |    .00 |    0.00
                 *                   which corresponds to: 000.000 | 000.00 | 0000.00
                 * First, handle the 0000.00 case, as the only one where op[] == "0."
                 */
                if (0 == strcmp(op, "0.")) {
                    /* We just parsed "0000." from the file.
                     * The remainder valid metric options are:      00
                     *                   which corresponds to: 0000.00
                     */
                    if ('0' != gerb_fgetc(fd) || '0' != gerb_fgetc(fd)) {
                        // read four extra characters that didn't match (EOF doesn't move ptr)
                        gerb_ungetc(fd);
                        gerb_ungetc(fd);
                        gerb_ungetc(fd);
                        gerb_ungetc(fd);
                        goto header_junk;
                    }
                    /* No further options are allowed after matching the number format */
                    eat_line(fd);
                    if (state->autodetect_file_format) {
                        state->number_format   = FMT_0000_00;
                        state->leading_digits  = 4;
                        state->trailing_digits = 2;
                    }
                    break;
                }

                /* We just parsed "000" from the file and have two more characters in op[].
                 * The possibility of "0000.00" was excluded above.
                 * The remainder valid metric options are:    .000 |    .00
                 *                   which corresponds to: 000.000 | 000.00
                 * Thus, exit if the first two characters in `op[]` are not ".0".
                 */
                if (0 != strcmp(op, ".0")) {
                    // read four extra characters that didn't match (EOF doesn't move ptr)
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    goto header_junk;
                }

                /* We just parsed "000.0" from the file.
                 * The remainder valid metric options are:      00 |      0
                 *                   which corresponds to: 000.000 | 000.00
                 * Thus, exit if the next character in the file is not "0".
                 */
                if ('0' != gerb_fgetc(fd)) {
                    // read five characters, so undo five of them.
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    gerb_ungetc(fd);
                    goto header_junk;
                }

                /* We just parsed "000.00" from the file.
                 * The remainder valid metric options are:       0 | (nothing)
                 *                   which corresponds to: 000.000 | 000.00
                 * The result is thus either FMT_000_000 or FMT_000_00,
                 * depending on if there is another 0.
                 *
                 * No longer will rewind to beginning, as either option
                 * results in an accepted value.
                 */
                int last_char = gerb_fgetc(fd);
                if (last_char == '0') {
                    if (state->autodetect_file_format) {
                        state->number_format   = FMT_000_000;
                        state->leading_digits  = 3;
                        state->trailing_digits = 3;
                    }
                } else {
                    // un-read the character...
                    if (last_char != EOF) {
                        gerb_ungetc(fd);
                    }
                    if (state->autodetect_file_format) {
                        state->number_format   = FMT_000_00;
                        state->leading_digits  = 3;
                        state->trailing_digits = 2;
                    }
                }

                // BUGBUG -- Should report / warn if there is additional junk here.
                eat_line(fd);
                break;

            default:
header_junk:
                // fd->ptr should point to the junk + one character
                gerb_ungetc(fd);
                // now fd->ptr points to the start of the junk section

                gerbv_stats_printf(
                    stats->error_list, GERBV_MESSAGE_WARNING, -1,
                    _("Found junk after METRIC command "
                      "at line %ld in file \"%s\""),
                    file_line, fd->filename
                );

                eat_line(fd);
                break;
        }
    }

    // When the line starts with `METRIC`, even if has junk afterwards,
    // set the units to metric (even before parsing additional options).
    state->unit = GERBV_UNIT_MM;

    return 1;
} /* drill_parse_header_is_metric() */

/* -------------------------------------------------------------- */
/* Look for a comment like FILE_FORMAT=4:4 and interpret it as if the
 * format will be 4 digits before the decimal point and 4 after.
 * Return non-zero if we find a FILE_FORMAT header, otherwise return
 * 0. */
static int
drill_parse_header_is_metric_comment(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {
    gerbv_drill_stats_t* stats = image->drill_stats;

    dprintf("    %s(): entering\n", __FUNCTION__);
    /* The leading semicolon is already gone. */
    if (DRILL_HEADER != state->curr_section) {
        return 0;
    }

    switch (file_check_str(fd, "FILE_FORMAT")) {
        case -1:
            gerbv_stats_printf(
                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                _("Unexpected EOF found while parsing \"%s\" string "
                  "in file \"%s\" on line %ld"),
                "FILE_FORMAT", fd->filename, file_line
            );
            return 0;
        case 0: return 0;
    }
    eat_whitespace(fd);

    if (file_check_str(fd, "=") != 1) {
        gerbv_stats_printf(
            stats->error_list, GERBV_MESSAGE_ERROR, -1,
            _("Expected '=' while parsing \"%s\" string "
              "in file \"%s\" on line %ld"),
            "FILE_FORMAT", fd->filename, file_line
        );
        return 0;
    }
    eat_whitespace(fd);

    int detect_error_leading = -1;
    int digits_before        = gerb_fgetint(fd, &detect_error_leading);
    if (detect_error_leading < 1) {
        /* We've failed to read a number. */
        gerbv_stats_printf(
            stats->error_list, GERBV_MESSAGE_ERROR, -1,
            _("Expected integer after '=' while parsing \"%s\" string "
              "in file \"%s\" on line %ld"),
            "FILE_FORMAT", fd->filename, file_line
        );
        return 0;
    }
    eat_whitespace(fd);

    if (file_check_str(fd, ":") != 1) {
        gerbv_stats_printf(
            stats->error_list, GERBV_MESSAGE_ERROR, -1,
            _("Expected ':' while parsing \"%s\" string "
              "in file \"%s\" on line %ld"),
            "FILE_FORMAT", fd->filename, file_line
        );
        return 0;
    }
    eat_whitespace(fd);

    int detect_error_trailing = -1;
    int digits_after          = gerb_fgetint(fd, &detect_error_trailing);
    if (detect_error_trailing < 1) {
        gerbv_stats_printf(
            stats->error_list, GERBV_MESSAGE_ERROR, -1,
            _("Expected integer after ':' while parsing \"%s\" string "
              "in file \"%s\" on line %ld"),
            "FILE_FORMAT", fd->filename, file_line
        );
        /* We've failed to read a number. */
        return 0;
    }

    // BUGBUG -- Function name may be misleading (and unnecessarily long).
    //           Function isn't checking for a metric comment.
    //           Function is checking for a FILE_FORMAT comment.
    //           This does ***NOT*** mean that the file is metric.

    // Per excellon specification, header_number_format must be one of
    // exactly two values, and which is selected is based on the units.
    if (state->unit == GERBV_UNIT_INCH) {
        state->header_number_format = FMT_00_0000;
    } else {
        state->header_number_format = FMT_000_000;
    }
    state->number_format          = FMT_USER;
    state->leading_digits         = digits_before;
    state->trailing_digits        = digits_after;
    state->autodetect_file_format = false;
    return 1;
} /* drill_parse_header_is_metric_comment() */

/* -------------------------------------------------------------- */
static int
drill_parse_header_is_inch(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {
    gerbv_drill_stats_t* stats = image->drill_stats;
    char                 c;

    dprintf("    %s(): entering\n", __FUNCTION__);

    if (DRILL_HEADER != state->curr_section) {
        return 0;
    }

    switch (file_check_str(fd, "INCH")) {
        case -1:
            gerbv_stats_printf(
                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                _("Unexpected EOF found while parsing \"%s\" string "
                  "in file \"%s\""),
                "INCH", fd->filename
            );
            return 0;

        case 0: return 0;
    }

    /* Look for TZ/LZ */
    if (',' != gerb_fgetc(fd)) {
        /* Unget the char in case we just advanced past a new line char */
        gerb_ungetc(fd);
    } else {
        c = gerb_fgetc(fd);
        if (c != EOF && 'Z' == gerb_fgetc(fd)) {
            switch (c) {
                case 'L':
                    if (state->autodetect_file_format) {
                        // NOTE: `LZ` means _include_ the leading zeros; thus, `LZ` means to _omit_ trailing zeros.
                        //       Would have been preferable to directly mirror file format logic, by tracking
                        //       whether leading zeros or trailing zeros are KEPT.  Que cera cera.
                        image->format->omit_zeros   = GERBV_OMIT_ZEROS_TRAILING;
                        state->header_number_format = FMT_00_0000;
                        state->number_format        = FMT_00_0000;
                        state->leading_digits       = 2;
                        state->trailing_digits      = 4;
                    }
                    break;

                case 'T':
                    if (state->autodetect_file_format) {
                        // NOTE: `TZ` means _include_ the trailing zeros; thus, `TZ` means to _omit_ leading zeros.
                        //       Would have been preferable to directly mirror file format logic, by tracking
                        //       whether leading zeros or trailing zeros are KEPT.  Que cera cera.
                        image->format->omit_zeros   = GERBV_OMIT_ZEROS_LEADING;
                        state->header_number_format = FMT_00_0000;
                        state->number_format        = FMT_00_0000;
                        state->leading_digits       = 2;
                        state->trailing_digits      = 4;
                    }
                    break;

                default:
                    gerbv_stats_printf(
                        stats->error_list, GERBV_MESSAGE_WARNING, -1,
                        _("Found junk '%s' after "
                          "INCH command "
                          "at line %ld in file \"%s\""),
                        gerbv_escape_char(c), file_line, fd->filename
                    );
                    break;
            }
        } else {
            gerbv_stats_printf(
                stats->error_list, GERBV_MESSAGE_WARNING, -1,
                _("Found junk '%s' after INCH command "
                  "at line %ld in file \"%s\""),
                gerbv_escape_char(c), file_line, fd->filename
            );
        }
    }
    // NOTE: This routine only sets the number format when
    //       also detected leading or trailing zero suppression.
    //       In such cases, the number_format is set to FMT_00_0000.
    state->unit = GERBV_UNIT_INCH;

    return 1;
} /* drill_parse_header_is_inch() */

/* -------------------------------------------------------------- */
/* Check "ICI" incremental input of coordinates */
static int
drill_parse_header_is_ici(gerb_file_t* fd, drill_state_t* state, gerbv_image_t* image, ssize_t file_line) {
    gerbv_drill_stats_t* stats = image->drill_stats;

    switch (file_check_str(fd, "ICI,ON")) {
        case -1:
            gerbv_stats_printf(
                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                _("Unexpected EOF found while parsing \"%s\" string "
                  "in file \"%s\""),
                "ICI,ON", fd->filename
            );
            return 0;

        case 1: state->coordinate_mode = DRILL_MODE_INCREMENTAL; return 1;
    }

    switch (file_check_str(fd, "ICI,OFF")) {
        case -1:
            gerbv_stats_printf(
                stats->error_list, GERBV_MESSAGE_ERROR, -1,
                _("Unexpected EOF found while parsing \"%s\" string "
                  "in file \"%s\""),
                "ICI,OFF", fd->filename
            );
            return 0;

        case 1: state->coordinate_mode = DRILL_MODE_ABSOLUTE; return 1;
    }

    return 0;
} /* drill_parse_header_is_ici() */

/* -------------------------------------------------------------- */
static drill_g_code_t
drill_parse_G_code(gerb_file_t* fd, gerbv_image_t* image, ssize_t file_line) {
    char                 op[3];
    drill_g_code_t       g_code;
    gerbv_drill_stats_t* stats = image->drill_stats;

    dprintf("---> entering %s()...\n", __FUNCTION__);

    op[0] = gerb_fgetc(fd);
    op[1] = gerb_fgetc(fd);
    op[2] = '\0';

    if (op[0] == EOF || op[1] == EOF) {
        gerbv_stats_printf(
            stats->error_list, GERBV_MESSAGE_ERROR, -1, _("Unexpected EOF found while parsing G-code in file \"%s\""),
            fd->filename
        );
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
        case 1: stats->G01++; break;
        case 2: stats->G02++; break;
        case 3: stats->G03++; break;
        case 5: stats->G05++; break;
        case 85: stats->G85++; break;
        case 90: stats->G90++; break;
        case 91: stats->G91++; break;
        case 93: stats->G93++; break;

        case DRILL_G_UNKNOWN:
        default: stats->G_unknown++; break;
    }

    dprintf("<----  ...leaving %s()\n", __FUNCTION__);

    return g_code;
} /* drill_parse_G_code() */

/* -------------------------------------------------------------- */
/* Parse on drill file coordinate.
   Returns nothing, but modifies state */
static void
drill_parse_coordinate(gerb_file_t* fd, char firstchar, gerbv_image_t* image, drill_state_t* state, ssize_t file_line) {
    gerbv_drill_stats_t* stats = image->drill_stats;

    double   x       = 0;
    gboolean found_x = FALSE;
    double   y       = 0;
    gboolean found_y = FALSE;

    while (TRUE) {
        if (firstchar == 'X') {
            x = read_double(
                fd, state->number_format, image->format->omit_zeros, state->leading_digits, state->trailing_digits
            );
            found_x = TRUE;
        } else if (firstchar == 'Y') {
            y = read_double(
                fd, state->number_format, image->format->omit_zeros, state->leading_digits, state->trailing_digits
            );
            found_y = TRUE;
        } else {
            gerb_ungetc(fd);
            break;
        }
        eat_whitespace(fd);
        firstchar = gerb_fgetc(fd);
    }
    if (state->coordinate_mode == DRILL_MODE_ABSOLUTE) {
        if (found_x) {
            state->curr_x = x;
        }
        if (found_y) {
            state->curr_y = y;
        }
    } else if (state->coordinate_mode == DRILL_MODE_INCREMENTAL) {
        if (found_x) {
            state->curr_x += x;
        }
        if (found_y) {
            state->curr_y += y;
        }
    } else {
        gerbv_stats_printf(
            stats->error_list, GERBV_MESSAGE_ERROR, -1,
            _("Coordinate mode is not absolute and not incremental "
              "at line %ld in file \"%s\""),
            file_line, fd->filename
        );
    }
} /* drill_parse_coordinate */

/* Allocates and returns a new drill_state structure
   Returns state pointer on success, NULL on ERROR */
static drill_state_t*
new_state() {
    drill_state_t* state = g_new0(drill_state_t, 1);
    if (state != NULL) {
        /* Init structure */
        state->curr_section           = DRILL_NONE;
        state->coordinate_mode        = DRILL_MODE_ABSOLUTE;
        state->origin_x               = 0.0;
        state->origin_y               = 0.0;
        state->unit                   = GERBV_UNIT_UNSPECIFIED;
        state->header_number_format   = FMT_00_0000; /* i. e. INCH */
        state->number_format          = FMT_00_0000; /* i. e. INCH */
        state->backup_number_format   = FMT_000_000; /* only used for METRIC */
        state->autodetect_file_format = true;
        state->leading_digits         = 2;
        state->trailing_digits        = 4;
    }

    return state;
} /* new_state */

typedef struct _drill_read_double_buffer_t {
    char values[DRILL_READ_DOUBLE_SIZE];
    int  ndigits;
    bool decimal_point;
    bool sign_prepend;
} drill_read_double_buffer_t;

/// @brief Read a double from the file as a decimal value,
///        counting the number of decimal digits found,
///        whether a leading sign (+/-) was found,
///        and whether a decimal point was found.
///        Ensures the string is null-terminated.
/// @param fd the file to read from
/// @param buffer the `drill_read_double_buffer_t` structure
///               to populate with the results.
static void
read_double_buffered(gerb_file_t* fd, drill_read_double_buffer_t* buffer) {

    int read = EOF;
    int i    = 0;
    memset(buffer, 0, sizeof(drill_read_double_buffer_t));

    read = gerb_fgetc(fd);
    while (read != EOF && i < (DRILL_READ_DOUBLE_SIZE - 1)) {

        /*
         * TODO: Allow a decimal point only once, else the number is corrupt.
         * TODO: Allow only a single comma in place of a single decimal point, else the number is corrupt.
         * TODO: Allow a leading sign (+/-) only once, else the number is corrupt.
         * TODO: Allow the leading sign only as the first character, else the number is corrupt.
         */
        if (!(isdigit(read) || read == '.' || read == ',' || read == '+' || read == '-')) {
            break;
        }

        if (read == ',' || read == '.') {
            buffer->decimal_point = TRUE;
        }

        /*
         * FIXME -- if we are going to do this, don't we need a
         * locale-independent strtod()?  I think pcb has one.
         */
        if (read == ',') {
            read = '.'; /* adjust for strtod() */
        }

        if (isdigit(read)) {
            buffer->ndigits++;
        }

        if (read == '-' || read == '+') {
            buffer->sign_prepend = TRUE;
        }

        buffer->values[i++] = (char)read;
        read                = gerb_fgetc(fd);
    }

    buffer->values[i] = 0;
    gerb_ungetc(fd);  // ensures line count remains accurate
    return;
}

static double
read_double_impl(
    const drill_read_double_buffer_t* buffer, number_fmt_t fmt, gerbv_omit_zeros_t omit_zeros, int leading_digits,
    int trailing_digits
) {

    // determine the scale (power of ten) to apply to the raw value
    double power_of_ten;
    if (buffer->decimal_point) {
        power_of_ten = 0.0;
    } else if (omit_zeros != GERBV_OMIT_ZEROS_TRAILING) {
        // whether GERBV_OMIT_ZEROS_LEADING or GERBV_OMIT_ZEROS_NONE,
        // this is the simple case, because the number of trailing
        // digits is guaranteed to have been provided in the file.
        // clang-format off
        switch (fmt) {
            case FMT_0000_00: power_of_ten =             -2.0; break;
            case FMT_000_00:  power_of_ten =             -2.0; break;
            case FMT_00_0000: power_of_ten =             -4.0; break;
            case FMT_000_000: power_of_ten =             -3.0; break;
            case FMT_USER:    power_of_ten = -trailing_digits; break;
            default:
                GERB_FATAL_ERROR(_("%s(): Unhandled fmt %d\n"), __FUNCTION__, fmt);
                return INFINITY;
        }
        // clang-format on
    } else if (omit_zeros == GERBV_OMIT_ZEROS_TRAILING) {
        power_of_ten = leading_digits - buffer->ndigits;
    } else {
        power_of_ten = -trailing_digits;
    }

    double raw    = strtod(buffer->values, NULL);
    double scale  = pow(10.0, power_of_ten);
    double result = raw * scale;
    dprintf(
        "    %s()=%lf: fmt=%s, omit_zeros=%s, leading/trailing=%d/%d \n", __FUNCTION__, result,
        number_fmt_to_string(fmt), omit_zeros_to_string(omit_zeros), leading_digits, trailing_digits
    );
    return result;
} /* read_double */

/// @brief Reads one double from file's current position.
///        If a decimal point is found, the value is returned directly.
///        Else, parses it according to the provided formatting options.
/// @param fd the file to read from
/// @param fmt the format to parse the number as
/// @param omit_zeros whether to omit leading or trailing zeros (or neither)
/// @param leading_digits when fmt == FMT_USER, the number of leading digits to expect
/// @param trailing_digits when fmt == FMT_USER, the number of trailing digits to expect
/// @return the double value read from the file
static double
read_double(gerb_file_t* fd, number_fmt_t fmt, gerbv_omit_zeros_t omit_zeros, int leading_digits, int trailing_digits) {
    drill_read_double_buffer_t buffer;
    read_double_buffered(fd, &buffer);
    double new_result = read_double_impl(&buffer, fmt, omit_zeros, leading_digits, trailing_digits);
    return new_result;
} /* read_double */

/* -------------------------------------------------------------- */
/* Eats all characters up to and including
   the first one of CR or LF */
static void
eat_line(gerb_file_t* fd) {
    int read;

    do {
        read = gerb_fgetc(fd);
    } while (read != '\n' && read != '\r' && read != EOF);

    /* Restore new line character for processing */
    if (read != EOF) {
        gerb_ungetc(fd);
    }
} /* eat_line */

/* -------------------------------------------------------------- */
/* Eats all tabs and spaces. */
static void
eat_whitespace(gerb_file_t* fd) {
    int read;

    do {
        read = gerb_fgetc(fd);
    } while ((read == ' ' || read == '\t') && read != EOF);

    /* Restore the non-whitespace character for processing */
    if (read != EOF) {
        gerb_ungetc(fd);
    }
} /* eat_whitespace */

/* -------------------------------------------------------------- */
static char*
get_line(gerb_file_t* fd) {
    int    read;
    gchar* retstring;
    gchar* tmps = g_strdup("");

    read = gerb_fgetc(fd);
    while (read != '\n' && read != '\r' && read != EOF) {
        retstring = g_strdup_printf("%s%c", tmps, read);

        /* since g_strdup_printf allocates memory, we need to free it */
        if (tmps) {
            g_free(tmps);
            tmps = NULL;
        }
        tmps = retstring;
        read = gerb_fgetc(fd);
    }

    /* Restore new line character for processing */
    if (read != EOF) {
        gerb_ungetc(fd);
    }

    return tmps;
} /* get_line */

/* -------------------------------------------------------------- */
/* Look for str in the file fd.  If found, return 1.  If not, return
 * 0.  If EOF is reached while searching, return -1. If the find
 * fails, rewinds the file descriptor.  Otherwise, it doesn't and the
 * string is consumed.
 */
static int
file_check_str(gerb_file_t* fd, const char* str) {
    char c;

    for (int i = 0; str[i] != '\0'; i++) {

        c = gerb_fgetc(fd);

        if (c == EOF) {
            return -1;
        }

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
const char*
drill_g_code_name(drill_g_code_t g_code) {
    // clang-format off
    switch (g_code) {
        case DRILL_G_ROUT:                    return N_("rout mode");
        case DRILL_G_LINEARMOVE:              return N_("linear mode");
        case DRILL_G_CWMOVE:                  return N_("circular CW mode");
        case DRILL_G_CCWMOVE:                 return N_("circular CCW mode");
        case DRILL_G_VARIABLEDWELL:           return N_("variable dwell");
        case DRILL_G_DRILL:                   return N_("drill mode");
        case DRILL_G_OVERRIDETOOLSPEED:       return N_("override tool feed or speed");
        case DRILL_G_ROUTCIRCLE:              return N_("routed CW circle");
        case DRILL_G_ROUTCIRCLECCW:           return N_("routed CCW circle");
        case DRILL_G_VISTOOL:                 return N_("select vision tool");
        case DRILL_G_VISSINGLEPOINTOFFSET:    return N_("single point vision offset");
        case DRILL_G_VISMULTIPOINTTRANS:      return N_("multipoint vision translation");
        case DRILL_G_VISCANCEL:               return N_("cancel vision translation or offset");
        case DRILL_G_VISCORRHOLEDRILL:        return N_("vision corrected single hole drilling");
        case DRILL_G_VISAUTOCALIBRATION:      return N_("vision system autocalibration");
        case DRILL_G_CUTTERCOMPOFF:           return N_("cutter compensation off");
        case DRILL_G_CUTTERCOMPLEFT:          return N_("cutter compensation left");
        case DRILL_G_CUTTERCOMPRIGHT:         return N_("cutter compensation right");
        case DRILL_G_VISSINGLEPOINTOFFSETREL: return N_("single point vision relative offset");
        case DRILL_G_VISMULTIPOINTTRANSREL:   return N_("multipoint vision relative translation");
        case DRILL_G_VISCANCELREL:            return N_("cancel vision relative translation or offset");
        case DRILL_G_VISCORRHOLEDRILLREL:     return N_("vision corrected single hole relative drilling");
        case DRILL_G_PACKDIP2:                return N_("dual in line package");
        case DRILL_G_PACKDIP:                 return N_("dual in line package");
        case DRILL_G_PACK8PINL:               return N_("eight pin L package");
        case DRILL_G_CIRLE:                   return N_("canned circle");
        case DRILL_G_SLOT:                    return N_("canned slot");
        case DRILL_G_ROUTSLOT:                return N_("routed step slot");
        case DRILL_G_ABSOLUTE:                return N_("absolute input mode");
        case DRILL_G_INCREMENTAL:             return N_("incremental input mode");
        case DRILL_G_ZEROSET:                 return N_("zero set");
        case DRILL_G_UNKNOWN:                 return N_("unknown G-code");
        default:                              return N_("unknown G-code");
    }
    // clang-format on
} /* drill_g_code_name() */

/* -------------------------------------------------------------- */
/** Return drill M-code name by code number. */
const char*
drill_m_code_name(drill_m_code_t m_code) {
    // clang-format off
    switch (m_code) {
        case DRILL_M_END:                             return N_("end of program");
        case DRILL_M_PATTERNEND:                      return N_("pattern end");
        case DRILL_M_REPEATPATTERNOFFSET:             return N_("repeat pattern offset");
        case DRILL_M_STOPOPTIONAL:                    return N_("stop optional");
        case DRILL_M_SANDREND:                        return N_("step and repeat end");
        case DRILL_M_STOPINSPECTION:                  return N_("stop for inspection");
        case DRILL_M_ZAXISROUTEPOSITIONDEPTHCTRL:     return N_("Z-axis rout position with depth control");
        case DRILL_M_ZAXISROUTEPOSITION:              return N_("Z-axis rout position");
        case DRILL_M_RETRACTCLAMPING:                 return N_("retract with clamping");
        case DRILL_M_RETRACTNOCLAMPING:               return N_("retract without clamping");
        case DRILL_M_TOOLTIPCHECK:                    return N_("tool tip check");
        case DRILL_M_PATTERN:                         return N_("pattern start");
        case DRILL_M_ENDREWIND:                       return N_("end of program with rewind");
        case DRILL_M_MESSAGELONG:                     return N_("long operator message");
        case DRILL_M_MESSAGE:                         return N_("operator message");
        case DRILL_M_HEADER:                          return N_("header start");
        case DRILL_M_VISANDRPATTERN:                  return N_("vision step and repeat pattern start");
        case DRILL_M_VISANDRPATTERNREWIND:            return N_("vision step and repeat rewind");
        case DRILL_M_VISANDRPATTERNOFFSETCOUNTERCTRL: return N_("vision step and repeat offset counter control");
        case DRILL_M_REFSCALING:                      return N_("reference scaling on");
        case DRILL_M_REFSCALINGEND:                   return N_("reference scaling off");
        case DRILL_M_PECKDRILLING:                    return N_("peck drilling on");
        case DRILL_M_PECKDRILLINGEND:                 return N_("peck drilling off");
        case DRILL_M_SWAPAXIS:                        return N_("swap axes");
        case DRILL_M_METRIC:                          return N_("metric measuring mode");
        case DRILL_M_IMPERIAL:                        return N_("inch measuring mode");
        case DRILL_M_MIRRORX:                         return N_("mirror image X-axis");
        case DRILL_M_MIRRORY:                         return N_("mirror image Y-axis");
        case DRILL_M_HEADEREND:                       return N_("header end");
        case DRILL_M_CANNEDTEXTX:                     return N_("canned text along X-axis");
        case DRILL_M_CANNEDTEXTY:                     return N_("canned text along Y-axis");
        case DRILL_M_USERDEFPATTERN:                  return N_("user defined stored pattern");
        case DRILL_M_UNKNOWN:                         return N_("unknown M-code");
        default:                                      return N_("unknown M-code");
    }
    // clang-format on

} /* drill_m_code_name() */

/* Well, this is a bit awkward.
 * __attribute__((unused)) does not appear to suppress `-Wunused-function`.
 * Therefore, have a dummy function that's never used, and which calls
 * all the functions that (at least in some configurations) are never used.
 * Even so, define the attribute that GCC states it would use.
 * This should automatically suppress this warning if GCC is updated.
 */
void UNUSED
reduce_unused_function_warnings_as_a_workaround(void) {
    // wrapped in always-false if statement, should get optimized away
    if (number_fmt_implies_imperial(FMT_00_0000) && number_fmt_implies_metric(FMT_00_0000)) {
        number_fmt_to_string(FMT_00_0000);
        omit_zeros_to_string(GERBV_OMIT_ZEROS_LEADING);
        unit_to_string(GERBV_UNIT_INCH);
        drill_file_section_to_string(DRILL_NONE);
        drill_coordinate_mode_to_string(DRILL_MODE_ABSOLUTE);
    }
}
