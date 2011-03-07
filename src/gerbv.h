/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
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

//! \example example1.c
//! \example example2.c
//! \example example3.c
//! \example example4.c
//! \example example5.c
//! \example example6.c

//! \defgroup libgerbv libgerbv
//! \defgroup gerbv Gerbv

/** \file gerbv.h
    \brief The main header file for the libgerbv library
    \ingroup libgerbv
*/

/**
\mainpage Gerbv/libgerbv Index Page

\section intro_sec Introduction

Gerbv is a program which can display, edit, export, and do other manipulation of
file formats used in PCB design (RS274X, Excellon drill, and pick-and-place). The core
library (libgerbv) is available as a separate library, allowing other software to easily
incorporate advanced Gerber functionality.

This code documentation is mainly intended to help explain the libgerbv API to developers
wishing to use libgerbv in his/her own projects. The easiest way to learn to use libgerbv is
by reading through and compiling the example source files (click on "Examples" in the navigation
tree in the left pane, or look in the doc/example-code/ directory in CVS).

For help with using the standalone Gerbv software, please refer to the man page (using
the command "man gerbv") or go to the Gerbv homepage for documentation (http://gerbv.sourceforge.net).
*/

#ifndef __GERBV_H__
#define __GERBV_H__

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#ifndef RENDER_USING_GDK
#include <cairo.h>
#endif

#define APERTURE_MIN 10
#define APERTURE_MAX 9999

/*
 * Maximum number of aperture parameters is set by the outline aperture
 * macro. There (p. 28) is defined up to 50 points in polygon.
 * So 50 points with x and y plus two for holding extra data gives...
 */
#define APERTURE_PARAMETERS_MAX 102
#define INITIAL_SCALE 200
#define MAX_ERRMSGLEN 25
#define MAX_COORDLEN 28
#define MAX_DISTLEN 90
#define MAX_STATUSMSGLEN (MAX_ERRMSGLEN+MAX_COORDLEN+MAX_DISTLEN)

/* Macros to convert between unscaled gerber coordinates and other units */
/* XXX NOTE: Currently unscaled units are assumed as inch, this is not
   XXX necessarily true for all files */
#define COORD2MILS(c) ((c)*1000.0)
#define COORD2MMS(c) ((c)*25.4)

#define GERB_FATAL_ERROR(t...) g_log(NULL, G_LOG_LEVEL_ERROR, ##t);
#define GERB_COMPILE_ERROR(t...)  g_log(NULL, G_LOG_LEVEL_CRITICAL, ##t);
#define GERB_COMPILE_WARNING(t...)  g_log(NULL, G_LOG_LEVEL_WARNING, ##t);
#define GERB_MESSAGE(t...)  g_log(NULL, G_LOG_LEVEL_MESSAGE, ##t);

/*! The aperture macro commands */  
typedef enum {GERBV_OPCODE_NOP, /*!< no operation */
	      GERBV_OPCODE_PUSH, /*!< push the instruction onto the stack */
	      GERBV_OPCODE_PPUSH, /*!< push parameter onto stack */ 
	      GERBV_OPCODE_PPOP, /*!< pop parameter from stack */ 
	      GERBV_OPCODE_ADD, /*!< mathmatical add operation */
	      GERBV_OPCODE_SUB, /*!< mathmatical subtract operation */
	      GERBV_OPCODE_MUL, /*!< mathmatical multiply operation */
	      GERBV_OPCODE_DIV, /*!< mathmatical divide operation */
	      GERBV_OPCODE_PRIM /*!< draw macro primative */
} gerbv_opcodes_t;

/*! The different message types used in libgerbv */   
typedef enum {GERBV_MESSAGE_FATAL, /*!< processing cannot continue */
		GERBV_MESSAGE_ERROR, /*!< something went wrong, but processing can still continue */
		GERBV_MESSAGE_WARNING, /*!< something was encountered that may provide the wrong output */
		GERBV_MESSAGE_NOTE /*!< an irregularity was encountered, but needs no intervention */
} gerbv_message_type_t;

/*! The different aperture types available 
 *  Please keep these in sync with the aperture names defined by
 *  ap_names in callbacks.c */
typedef enum {GERBV_APTYPE_NONE, /*!< no aperture used */
		GERBV_APTYPE_CIRCLE, /*!< a round aperture */
		GERBV_APTYPE_RECTANGLE, /*!< a rectangular aperture */
		GERBV_APTYPE_OVAL, /*!< an ovular (obround) aperture */
		GERBV_APTYPE_POLYGON, /*!< a polygon aperture */
		GERBV_APTYPE_MACRO, /*!< a RS274X macro */
		GERBV_APTYPE_MACRO_CIRCLE, /*!< a RS274X circle macro */
		GERBV_APTYPE_MACRO_OUTLINE, /*!< a RS274X outline macro */
		GERBV_APTYPE_MACRO_POLYGON, /*!< a RS274X polygon macro */
		GERBV_APTYPE_MACRO_MOIRE, /*!< a RS274X moire macro */
		GERBV_APTYPE_MACRO_THERMAL, /*!< a RS274X thermal macro */
		GERBV_APTYPE_MACRO_LINE20, /*!< a RS274X line (code 20) macro */
		GERBV_APTYPE_MACRO_LINE21, /*!< a RS274X line (code 21) macro */
		GERBV_APTYPE_MACRO_LINE22 /*!< a RS274X line (code 22) macro */
} gerbv_aperture_type_t;

/*! the current state of the aperture drawing tool */
typedef enum {GERBV_APERTURE_STATE_OFF, /*!< tool drawing is off, and nothing will be drawn */
		GERBV_APERTURE_STATE_ON, /*!< tool drawing is on, and something will be drawn */
		GERBV_APERTURE_STATE_FLASH /*!< tool is flashing, and will draw a single aperture */
} gerbv_aperture_state_t;

/*! the current unit used */
typedef enum {GERBV_UNIT_INCH, /*!< inches */
		GERBV_UNIT_MM, /*!< mm */
		GERBV_UNIT_UNSPECIFIED /*!< use default units */
} gerbv_unit_t;

/*! the different drawing polarities available */
typedef enum {GERBV_POLARITY_POSITIVE, /*!< draw "positive", using the current layer's polarity */
		GERBV_POLARITY_NEGATIVE, /*!< draw "negative", reversing the current layer's polarity */
		GERBV_POLARITY_DARK, /*!< add to the current rendering */
		GERBV_POLARITY_CLEAR /*!< subtract from the current rendering */
} gerbv_polarity_t;

/*! the decimal point parsing style used */
typedef enum {GERBV_OMIT_ZEROS_LEADING, /*!< omit extra zeros before the decimal point */
		GERBV_OMIT_ZEROS_TRAILING, /*!< omit extra zeros after the decimal point */
		GERBV_OMIT_ZEROS_EXPLICIT, /*!< explicitly specify how many decimal places are used */
		GERBV_OMIT_ZEROS_UNSPECIFIED /*!< use the default parsing style */
} gerbv_omit_zeros_t;

/*! the coordinate system used */
typedef enum {GERBV_COORDINATE_ABSOLUTE, /*!< all coordinates are absolute from a common origin */
		GERBV_COORDINATE_INCREMENTAL /*!< all coordinates are relative to the previous coordinate */
} gerbv_coordinate_t;

/*! the interpolation methods available */
typedef enum {GERBV_INTERPOLATION_LINEARx1, /*!< draw a line */
		GERBV_INTERPOLATION_x10, /*!< draw a line */
		GERBV_INTERPOLATION_LINEARx01, /*!< draw a line */
		GERBV_INTERPOLATION_LINEARx001, /*!< draw a line */
		GERBV_INTERPOLATION_CW_CIRCULAR, /*!< draw an arc in the clockwise direction */
		GERBV_INTERPOLATION_CCW_CIRCULAR, /*!< draw an arc in the counter-clockwise direction */
		GERBV_INTERPOLATION_PAREA_START, /*!< start a polygon draw */
		GERBV_INTERPOLATION_PAREA_END, /*!< end a polygon draw */
		GERBV_INTERPOLATION_DELETED /*!< the net has been deleted by the user, and will not be drawn */
} gerbv_interpolation_t;

typedef enum {GERBV_ENCODING_NONE,
		GERBV_ENCODING_ASCII,
		GERBV_ENCODING_EBCDIC,
		GERBV_ENCODING_BCD,
		GERBV_ENCODING_ISO_ASCII,
		GERBV_ENCODING_EIA
} gerbv_encoding_t;

/*! The different layer types used */
typedef enum {GERBV_LAYERTYPE_RS274X, /*!< the file is a RS274X file */
		GERBV_LAYERTYPE_DRILL, /*!< the file is an Excellon drill file */
		GERBV_LAYERTYPE_PICKANDPLACE /*!< the file is a CSV pick and place file */
} gerbv_layertype_t;

typedef enum {GERBV_KNOCKOUT_TYPE_NOKNOCKOUT,
		GERBV_KNOCKOUT_TYPE_FIXEDKNOCK,
		GERBV_KNOCKOUT_TYPE_BORDER
} gerbv_knockout_type_t;

typedef enum {GERBV_MIRROR_STATE_NOMIRROR,
		GERBV_MIRROR_STATE_FLIPA,
		GERBV_MIRROR_STATE_FLIPB,
		GERBV_MIRROR_STATE_FLIPAB
} gerbv_mirror_state_t;

typedef enum {GERBV_AXIS_SELECT_NOSELECT,
		GERBV_AXIS_SELECT_SWAPAB
} gerbv_axis_select_t;

typedef enum {GERBV_JUSTIFY_NOJUSTIFY,
		GERBV_JUSTIFY_LOWERLEFT,
		GERBV_JUSTIFY_CENTERJUSTIFY
} gerbv_image_justify_type_t;

/*! The different selection modes available */
typedef enum {GERBV_SELECTION_EMPTY, /*!< the selection buffer is empty */
		GERBV_SELECTION_POINT_CLICK, /*!< the user clicked on a single point */
		GERBV_SELECTION_DRAG_BOX /*!< the user dragged a box to encompass one or more objects */
} gerbv_selection_t;

/*! The different rendering modes available to libgerbv */
typedef enum {GERBV_RENDER_TYPE_GDK, /*!< render using normal GDK drawing functions */
		GERBV_RENDER_TYPE_GDK_XOR, /*!< use the GDK_XOR mask to draw a pseudo-transparent scene */
		GERBV_RENDER_TYPE_CAIRO_NORMAL, /*!< use the cairo library */
		GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY, /*!< use the cairo library with the smoothest edges */
		GERBV_RENDER_TYPE_MAX /*!< End-of-enum indicator */
} gerbv_render_types_t;

/* 
 * The following typedef's are taken directly from src/hid.h in the
 * pcb project.  The names are kept the same to make it easier to
 * compare to pcb's sources.
 */
    
/* Used for HID attributes (exporting and printing, mostly).
   HA_boolean uses int_value, HA_enum sets int_value to the index and
   str_value to the enumeration string.  HID_Label just shows the
   default str_value.  HID_Mixed is a real_value followed by an enum,
   like 0.5in or 100mm. 
*/
typedef struct {
    int int_value;
    char *str_value;
    double real_value;
} gerbv_HID_Attr_Val;

typedef struct {
    char *name;
    char *help_text;
    enum
	{ HID_Label, HID_Integer, HID_Real, HID_String,
	  HID_Boolean, HID_Enum, HID_Mixed, HID_Path
	} type;
    int min_val, max_val;	/* for integer and real */
    gerbv_HID_Attr_Val default_val;	/* Also actual value for global attributes.  */
    const char **enumerations;
    /* If set, this is used for global attributes (i.e. those set
       statically with REGISTER_ATTRIBUTES below) instead of changing
       the default_val.  Note that a HID_Mixed attribute must specify a
       pointer to gerbv_HID_Attr_Val here, and HID_Boolean assumes this is
       "char *" so the value should be initialized to zero, and may be
       set to non-zero (not always one).  */
    void *value;
    int hash; /* for detecting changes. */
} gerbv_HID_Attribute;
/* end of HID attributes from PCB */

/*! A linked list of errors found in the files */   
typedef struct error_list {
    int layer;
    gchar *error_text;
    gerbv_message_type_t type;
    struct error_list *next;
} gerbv_error_list_t;

typedef struct instruction {
    gerbv_opcodes_t opcode;
    union {
	int ival;
	float fval;
    } data;
    struct instruction *next;
} gerbv_instruction_t;

typedef struct amacro {
    gchar *name;
    gerbv_instruction_t *program;
    unsigned int nuf_push;  /* Nuf pushes in program to estimate stack size */
    struct amacro *next;
} gerbv_amacro_t;

typedef struct gerbv_simplified_amacro {
    gerbv_aperture_type_t type;
    double parameter[APERTURE_PARAMETERS_MAX];
    struct gerbv_simplified_amacro *next;
} gerbv_simplified_amacro_t;

typedef struct gerbv_aperture {
    gerbv_aperture_type_t type;
    gerbv_amacro_t *amacro;
    gerbv_simplified_amacro_t *simplified;
    double parameter[APERTURE_PARAMETERS_MAX];
    int nuf_parameters;
    gerbv_unit_t unit;
} gerbv_aperture_t;

/* the gerb_aperture_list is used to keep track of 
 * apertures used in stats reporting */
typedef struct gerbv_aperture_list {
    int number;
    int layer;
    int count;
    gerbv_aperture_type_t type;
    double parameter[5];
    struct gerbv_aperture_list *next;
} gerbv_aperture_list_t;

/*! Contains statistics on the various codes used in a RS274X file */
typedef struct {
    gerbv_error_list_t *error_list;
    gerbv_aperture_list_t *aperture_list;
    gerbv_aperture_list_t *D_code_list;

    int layer_count;
    int G0;
    int G1;
    int G2;
    int G3;
    int G4;
    int G10;
    int G11;
    int G12;
    int G36;
    int G37;
    int G54;
    int G55;
    int G70;
    int G71;
    int G74;
    int G75;
    int G90;
    int G91;
    int G_unknown;

    int D1;
    int D2;
    int D3;
/*    GHashTable *D_user_defined; */
    int D_unknown;
    int D_error;

    int M0;
    int M1;
    int M2;
    int M_unknown;

    int X;
    int Y;
    int I;
    int J;

    /* Must include % RS-274 codes */
    int star;
    int unknown;

} gerbv_stats_t;

/*! Linked list of drills found in active layers.  Used in reporting statistics */
typedef struct drill_list {
    int drill_num;
    double drill_size;
    gchar *drill_unit;
    int drill_count;
    struct drill_list *next;
} gerbv_drill_list_t;

/*! Struct holding statistics of drill commands used.  Used in reporting statistics */
typedef struct {
    int layer_count;
    
    gerbv_error_list_t *error_list;
    gerbv_drill_list_t *drill_list;
    int comment;
    int F;

    int G00;
    int G01;
    int G02;
    int G03;
    int G04;
    int G05;
    int G90;
    int G91;
    int G93;
    int G_unknown;

    int M00;
    int M01;
    int M18;
    int M25;
    int M30;
    int M31;
    int M45;
    int M47;
    int M48;
    int M71;
    int M72;
    int M95;
    int M97;
    int M98;
    int M_unknown;

    int R;

    int unknown;

    /* used to total up the drill count across all layers/sizes */
    int total_count;

    char *detect;

} gerbv_drill_stats_t;

typedef struct {
	gpointer image;
	gpointer net;
} gerbv_selection_item_t;

/*! Struct holding info about the last selection */
typedef struct {
	gerbv_selection_t type;
	gdouble lowerLeftX;
	gdouble lowerLeftY;
	gdouble upperRightX;
	gdouble upperRightY;
	GArray *selectedNodeArray;
} gerbv_selection_info_t;

/*!  Stores image transformation information, used to modify the rendered
position/scale/etc of an image. */
typedef struct {
    gdouble translateX; /*!< the X translation (in inches) */
    gdouble translateY; /*!< the Y translation (in inches) */
    gdouble scaleX; /*!< the X scale factor (1.0 is default) */
    gdouble scaleY; /*!< the Y scale factor (1.0 is default) */
    gdouble rotation; /*!< the rotation of the layer around the origin (in radians) */
    gboolean mirrorAroundX;  /*!< TRUE if the layer is mirrored around the X axis (vertical flip) */
    gboolean mirrorAroundY;  /*!< TRUE if the layer is mirrored around the Y axis (vertical flip) */
    gboolean inverted; /*!< TRUE if the image should be rendered "inverted" (light is dark and vice versa) */
} gerbv_user_transformation_t;

/*!  This defines a box location and size (used to rendering logic) */
typedef struct {
    double left; /*!< the X coordinate of the left side */
    double right; /*!< the X coordinate of the right side */
    double bottom; /*!< the Y coordinate of the bottom side */
    double top; /*!< the Y coordinate of the top side */
} gerbv_render_size_t;

typedef struct gerbv_cirseg {
    double cp_x;
    double cp_y;
    double width;  /* of oval */
    double height; /* of oval */
    double angle1;
    double angle2;
} gerbv_cirseg_t;

typedef struct gerbv_step_and_repeat { /* SR parameters */
    int X;
    int Y;
    double dist_X;
    double dist_Y;
} gerbv_step_and_repeat_t;
 
typedef struct {
    gboolean firstInstance;
    gerbv_knockout_type_t type;
    gerbv_polarity_t polarity;
    gdouble lowerLeftX;
    gdouble lowerLeftY;
    gdouble width;
    gdouble height;
    gdouble border;
} gerbv_knockout_t;
 
/*!  The structure used to keep track of RS274X layer groups */
typedef struct {
    gerbv_step_and_repeat_t stepAndRepeat; /*!< the current step and repeat group (refer to RS274X spec) */
    gerbv_knockout_t knockout; /*!< the current knockout group (refer to RS274X spec) */
    gdouble rotation; /*!< the current rotation around the origin */
    gerbv_polarity_t polarity; /*!< the polarity of this layer */
    gchar *name; /*!< the layer name (NULL for none) */
    gpointer next; /*!< the next layer group in the array */
} gerbv_layer_t;

/*!  The structure used to keep track of RS274X state groups */
typedef struct {
    gerbv_axis_select_t axisSelect; /*!< the AB to XY coordinate mapping (refer to RS274X spec) */
    gerbv_mirror_state_t mirrorState; /*!< any mirroring around the X or Y axis */
    gerbv_unit_t unit; /*!< the current length unit */
    gdouble offsetA; /*!< the offset along the A axis (usually this is the X axis) */
    gdouble offsetB; /*!< the offset along the B axis (usually this is the Y axis) */
    gdouble scaleA; /*!< the scale factor in the A axis (usually this is the X axis) */
    gdouble scaleB; /*!< the scale factor in the B axis (usually this is the Y axis) */
    gpointer next; /*!< the next state group in the array */
} gerbv_netstate_t;

/*!  The structure used to hold a geometric entity (line/polygon/etc)*/
typedef struct gerbv_net {
    double start_x; /*!< the X coordinate of the start point */
    double start_y; /*!< the Y coordinate of the start point */
    double stop_x; /*!< the X coordinate of the end point */
    double stop_y; /*!< the Y coordinate of the end point */
    gerbv_render_size_t boundingBox; /*!< the bounding box containing this net (used for rendering optimizations) */
    int aperture; /*!< the index of the aperture used for this entity */
    gerbv_aperture_state_t aperture_state; /*!< the state of the aperture tool (on/off/etc) */
    gerbv_interpolation_t interpolation; /*!< the path interpolation method (linear/etc) */
    gerbv_cirseg_t *cirseg; /*!< information for arc nets */
    struct gerbv_net *next; /*!< the next net in the array */
    GString *label; /*!< a label string for this net */
    gerbv_layer_t *layer; /*!< the RS274X layer this net belongs to */
    gerbv_netstate_t *state; /*!< the RS274X state this net belongs to */
} gerbv_net_t;

/*! Struct holding info about interpreting the Gerber files read
 *  e.g. leading zeros, etc.  */
typedef struct gerbv_format {
    gerbv_omit_zeros_t omit_zeros;
    gerbv_coordinate_t coordinate;
    int x_int;
    int x_dec;
    int y_int;
    int y_dec;
    int lim_seqno; /* Length limit for codes of sequence number */
    int lim_gf;    /* Length limit for codes of general function */
    int lim_pf;    /* Length limit for codes of plot function */
    int lim_mf;    /* Length limit for codes of miscellaneous function */
} gerbv_format_t;
	

/*! Struct holding info about a particular image */
typedef struct gerbv_image_info {
    char *name;
    gerbv_polarity_t polarity;
    double min_x; /* Always in inches */
    double min_y;
    double max_x;
    double max_y;
    double offsetA;
    double offsetB;
    gerbv_encoding_t encoding;
    double imageRotation;
    gerbv_image_justify_type_t imageJustifyTypeA;
    gerbv_image_justify_type_t imageJustifyTypeB;
    gdouble imageJustifyOffsetA;
    gdouble imageJustifyOffsetB;
    gdouble imageJustifyOffsetActualA;
    gdouble imageJustifyOffsetActualB;
    gchar *plotterFilm;

    /* Descriptive string for the type of file (rs274-x, drill, etc)
     * that this is
     */
    gchar *type;

    /* Attribute list that is used to hold all sorts of information
     * about how the layer is to be parsed.
    */ 
    gerbv_HID_Attribute *attr_list;
    int n_attr;
} gerbv_image_info_t;

/*!  The structure used to hold a layer (RS274X, drill, or pick-and-place data) */
typedef struct {
  gerbv_layertype_t layertype; /*!< the type of layer (RS274X, drill, or pick-and-place) */
  gerbv_aperture_t *aperture[APERTURE_MAX]; /*!< an array with all apertures used */
  gerbv_layer_t *layers; /*!< an array of all RS274X layers used (only used in RS274X types) */
  gerbv_netstate_t *states; /*!< an array of all RS274X states used (only used in RS274X types) */
  gerbv_amacro_t *amacro; /*!< an array of all macros used (only used in RS274X types) */
  gerbv_format_t *format; /*!< formatting info */
  gerbv_image_info_t *info; /*!< miscellaneous info regarding the layer such as overall size, etc */
  gerbv_net_t *netlist; /*!< an array of all geometric entities in the layer */
  gerbv_stats_t *gerbv_stats; /*!< RS274X statistics for the layer */
  gerbv_drill_stats_t *drill_stats;  /*!< Excellon drill statistics for the layer */
} gerbv_image_t;

/*!  Holds information related to an individual layer that is part of a project */
typedef struct {
  gerbv_image_t *image; /*!< the image holding all the geometry of the layer */
  GdkColor color; /*!< the color to render this layer with */
  guint16 alpha; /*!< the transparency to render this layer with */
  gboolean isVisible; /*!< TRUE if this layer should be rendered with the project */
  gpointer privateRenderData; /*!< private data holder for the rendering backend */
  gchar *fullPathname; /*!< this full pathname to the file */
  gchar *name; /*!< the name used when referring to this layer (e.g. in a layer selection menu) */
  gerbv_user_transformation_t transform; /*!< user-specified transformation for this layer (mirroring, translating, etc) */
  gboolean layer_dirty;  /*!< True if layer has been modified since last save */
} gerbv_fileinfo_t;

typedef struct {
	double x1, y1;
	double x2, y2;
} gerbv_bbox_t;

/*!  The top-level structure used in libgerbv.  A gerbv_project_t groups together
any number of layers, while keeping track of other basic paramters needed for rendering */
typedef struct {
  GdkColor  background; /*!< the background color used for rendering */
  int max_files; /*!< the current number of fileinfos in the file array */
  gerbv_fileinfo_t **file; /*!< the array for holding the child fileinfos */
  int curr_index; /*!< the index of the currently active fileinfo */
  int last_loaded; /*!< the number of fileinfos currently in the project */
  int renderType; /*!< the type of renderer to use */
  gboolean check_before_delete;  /*!< TRUE to ask before deleting objects */
  gchar *path; /*!< the default path to load new files from */
  gchar *execpath;    /*!< the path to executed version of gerbv */
  gchar *execname;    /*!< the path plus executible name for gerbv */
  gchar *project;     /*!< the default name for the private project file */
} gerbv_project_t;

/*! Color of layer */
typedef struct{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
}gerbv_layer_color;

/*!  This contains the rendering info for a scene */
typedef struct {
	gdouble scaleFactorX; /*!< the X direction scale factor */
	gdouble scaleFactorY; /*!< the Y direction scale factor */
	gdouble lowerLeftX; /*!< the X coordinate of the lower left corner (in real world coordinates, in inches) */
	gdouble lowerLeftY; /*!< the Y coordinate of the lower left corner (in real world coordinates, in inches) */
	gerbv_render_types_t renderType; /*!< the type of rendering to use */
	gint displayWidth; /*!< the width of the scene (in pixels, or points depending on the surface type) */
	gint displayHeight; /*!< the height of the scene (in pixels, or points depending on the surface type) */
} gerbv_render_info_t;

//! Allocate a new gerbv_image structure
//! \return the newly created image
gerbv_image_t *gerbv_create_image(gerbv_image_t *image, /*!< the old image to free or NULL */
		const gchar *type /*!< the type of image to create */
);

//! Free an image structure
void gerbv_destroy_image(gerbv_image_t *image /*!< the image to free */
);

//! Copy an image into an existing image, effectively merging the two together
void
gerbv_image_copy_image (gerbv_image_t *sourceImage, /*!< the source image */
	gerbv_user_transformation_t *transform, /*!< the transformation to apply to the new image, or NULL for none */
	gerbv_image_t *destinationImage /*!< the destination image to copy to */
);

//! Duplicate an existing image and return the new copy
//! \return the newly created image
gerbv_image_t *
gerbv_image_duplicate_image (gerbv_image_t *sourceImage, /*!< the source image */
	gerbv_user_transformation_t *transform /*!< the transformation to apply to the new image, or NULL for none */
);

//! Delete a net in an existing image
void
gerbv_image_delete_net (gerbv_net_t *currentNet /*!< the net to delete */
);

void
gerbv_image_delete_selected_nets (gerbv_image_t *sourceImage, GArray *selectedNodeArray);

gboolean
gerbv_image_reduce_area_of_selected_objects (GArray *selectionArray, gdouble areaReduction, gint paneRows,
		gint paneColumns, gdouble paneSeparation);

gboolean
gerbv_image_move_selected_objects (GArray *selectionArray, gdouble translationX,
		gdouble translationY);

//! Return the next net entry which corresponds to a unique visible object
gerbv_net_t *
gerbv_image_return_next_renderable_object (gerbv_net_t *oldNet);

//! Create a new project structure and initialize some important variables
gerbv_project_t *
gerbv_create_project (void);

//! Free a project and all related variables
void
gerbv_destroy_project (gerbv_project_t *gerbvProject /*!< the project to destroy */
);

//! Open a file, parse the contents, and add a new layer to an existing project
void 
gerbv_open_layer_from_filename (
	gerbv_project_t *gerbvProject, /*!< the existing project to add the new layer to */
	gchar *filename /*!< the full pathname of the file to be parsed */
);

//! Open a file, parse the contents, and add a new layer to an existing project while setting the color of the layer
void 
gerbv_open_layer_from_filename_with_color(gerbv_project_t *gerbvProject, /*!< the existing project to add the new layer to */
	gchar *filename, /*!< the full pathname of the file to be parsed */
	guint16 red, /*!< the value for the red color component */
	guint16 green, /*!< the value for the green color component */
	guint16 blue, /*!< the value for the blue color component */
	guint16 alpha /*!< the value for the alpha color component */
);

//! Free a fileinfo structure
void
gerbv_destroy_fileinfo (gerbv_fileinfo_t *fileInfo /*!< the fileinfo to free */
);

gboolean 
gerbv_save_layer_from_index(gerbv_project_t *gerbvProject, gint index, gchar *filename);

int
gerbv_revert_file(gerbv_project_t *gerbvProject, int idx);

void 
gerbv_revert_all_files(gerbv_project_t *gerbvProject);

void 
gerbv_unload_layer(gerbv_project_t *gerbvProject, int index);

void 
gerbv_unload_all_layers (gerbv_project_t *gerbvProject);

void 
gerbv_change_layer_order(gerbv_project_t *gerbvProject, gint oldPosition, gint newPosition);

gint
gerbv_add_parsed_image_to_project (gerbv_project_t *gerbvProject, gerbv_image_t *parsed_image,
			gchar *filename, gchar *baseName, int idx, int reload);
int
gerbv_open_image(gerbv_project_t *gerbvProject, char *filename, int idx, int reload,
		gerbv_HID_Attribute *fattr, int n_fattr, gboolean forceLoadFile);
		
void
gerbv_render_get_boundingbox(gerbv_project_t *gerbvProject, gerbv_render_size_t *boundingbox);

//! Calculate the zoom and translations to fit the rendered scene inside the given scene size
void
gerbv_render_zoom_to_fit_display (gerbv_project_t *gerbvProject, /*!< the project to use for calculating */
		gerbv_render_info_t *renderInfo /*!< the scene render pointer (updates the values in this parameter) */
);

void
gerbv_render_translate_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo);

void
gerbv_render_to_pixmap_using_gdk (gerbv_project_t *gerbvProject, GdkPixmap *pixmap,
		gerbv_render_info_t *renderInfo, gerbv_selection_info_t *selectionInfo,
		GdkColor *selectionColor);

#ifndef RENDER_USING_GDK
void
gerbv_render_all_layers_to_cairo_target_for_vector_output (gerbv_project_t *gerbvProject,
		cairo_t *cr, gerbv_render_info_t *renderInfo);

void
gerbv_render_all_layers_to_cairo_target (gerbv_project_t *gerbvProject, cairo_t *cr,
			gerbv_render_info_t *renderInfo);

//! Render a layer to a cairo context
void
gerbv_render_layer_to_cairo_target (cairo_t *cr, /*!< the cairo context */
		gerbv_fileinfo_t *fileInfo, /*!< the layer fileinfo pointer */
		gerbv_render_info_t *renderInfo /*!< the scene render info */
);
						
void
gerbv_render_cairo_set_scale_and_translation(cairo_t *cr, gerbv_render_info_t *renderInfo);

void
gerbv_render_layer_to_cairo_target_without_transforming(cairo_t *cr, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo, gboolean pixelOutput );
#endif

double
gerbv_get_tool_diameter(int toolNumber
);

int
gerbv_process_tools_file(const char *toolFileName
);

//! Render a project to a PNG file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_png_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		int widthInPixels, /*!< the width of the rendered picture (in pixels) */
		int heightInPixels, /*!< the height of the rendered picture (in pixels) */
		gchar const* filename  /*!< the filename for the exported PNG file */
);

//! Render a project to a PNG file using user-specified render info
void
gerbv_export_png_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported PNG file */
);

//! Render a project to a PDF file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_pdf_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gchar const* filename  /*!< the filename for the exported PDF file */
);

//! Render a project to a PDF file using user-specified render info
void
gerbv_export_pdf_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported PDF file */
);

//! Render a project to a Postscript file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_postscript_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gchar const* filename  /*!< the filename for the exported Postscript file */
);

//! Render a project to a Postscript file using user-specified render info
void
gerbv_export_postscript_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported Postscript file */
);

//! Render a project to a SVG file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_svg_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gchar const* filename  /*!< the filename for the exported   file */
);

//! Render a project to a   file using user-specified render info
void
gerbv_export_svg_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported   file */
);

//! Parse a RS274X file and return the parsed image
//! \return the new gerbv_image_t, or NULL if not successful
gerbv_image_t *
gerbv_create_rs274x_image_from_filename (gchar *filename /*!< the filename of the file to be parsed*/
);

//! Export an image to a new file in RS274X format
//! \return TRUE if successful, or FALSE if not
gboolean
gerbv_export_rs274x_file_from_image (gchar *filename, /*!< the filename for the new file */
		gerbv_image_t *image, /*!< the image to export */
		gerbv_user_transformation_t *transform /*!< the transformation to apply before exporting */
);

//! Export an image to a new file in Excellon drill format
//! \return TRUE if successful, or FALSE if not
gboolean
gerbv_export_drill_file_from_image (gchar *filename, /*!< the filename for the new file */
		gerbv_image_t *image, /*!< the image to export */
		gerbv_user_transformation_t *transform /*!< the transformation to apply before exporting */
);

//! Draw a line on the specified image
void
gerbv_image_create_line_object (gerbv_image_t *image, /*!< the image to draw to */
		gdouble startX, /*!< the starting X coordinate */
		gdouble startY, /*!< the starting Y coordinate */
		gdouble endX, /*!< the ending X coordinate */
		gdouble endY, /*!< the ending Y coordinate */
		gdouble lineWidth, /*!< the width of the line to draw */
		gerbv_aperture_type_t apertureType  /*!< the type of aperture to use (e.g. CIRCLE) */
);

//! Draw an arc on the specified image
void
gerbv_image_create_arc_object (gerbv_image_t *image, /*!< the image to draw to */
		gdouble centerX, /*!< the center X coordinate */
		gdouble centerY, /*!< the center Y coordinate */
		gdouble radius, /*!< the arc radius */
		gdouble startAngle, /*!< the start angle (in CCW degrees) */
		gdouble endAngle, /*!< the start angle (in CCW degrees) */
		gdouble lineWidth, /*!< the width of the line to draw */
		gerbv_aperture_type_t apertureType  /*!< the type of aperture to use (e.g. CIRCLE) */
);
		
//! Draw a filled rectangle on the specified image	
void
gerbv_image_create_rectangle_object (gerbv_image_t *image, /*!< the image to draw to */
		gdouble coordinateX, /*!< the X coordinate of the lower left corner */
		gdouble coordinateY, /*!< the Y coordinate of the lower left corner */
		gdouble width, /*!< the width of the drawn rectangle */
		gdouble height /*!< the height of the drawn rectangle */
);

//! Create any missing apertures in the specified image
void
gerbv_image_create_dummy_apertures (gerbv_image_t *parsed_image /*!< the image to repair */
);
		
/*! Create new struct for holding drill stats */
gerbv_drill_stats_t *
gerbv_drill_stats_new(void);

/*! Free the memory for a drill stats struct */
void
gerbv_drill_stats_destroy(gerbv_drill_stats_t *);

/*! Add stats gathered from specified layer to accumulatedd drill stats
 *  compiled from all layers */
void
gerbv_drill_stats_add_layer(gerbv_drill_stats_t *accum_stats,
		gerbv_drill_stats_t *input_stats,
		int this_layer
);

/*! Create new struct for holding Gerber stats */
gerbv_stats_t *
gerbv_stats_new(void);

/*! Free the memory for a stats struct */
void
gerbv_stats_destroy(gerbv_stats_t *);

/*! Add stats gathered from specified layer to accumulated Gerber stats
 *  compiled from all layers */
void
gerbv_stats_add_layer(gerbv_stats_t *accum_stats, 
		gerbv_stats_t *input_stats,
		int this_layer
);

void
gerbv_attribute_destroy_HID_attribute (gerbv_HID_Attribute *attributeList, int n_attr);

gerbv_HID_Attribute *
gerbv_attribute_dup (gerbv_HID_Attribute *, int);

#if defined(__cplusplus)
}
#endif

#endif /* __GERBV_H__ */
