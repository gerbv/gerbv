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
 
#ifndef GERBV_H
#define GERBV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "attribute.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

/* from old gerb_error */
enum error_type_t {FATAL, GRB_ERROR, WARNING, NOTE};

typedef struct error_list_t {
    int layer;
    char *error_text;
    enum error_type_t type;
    struct error_list_t *next;
} error_list_t;

#define GERB_FATAL_ERROR(t...) g_log(NULL, G_LOG_LEVEL_ERROR, ##t);
#define GERB_COMPILE_ERROR(t...)  g_log(NULL, G_LOG_LEVEL_CRITICAL, ##t);
#define GERB_COMPILE_WARNING(t...)  g_log(NULL, G_LOG_LEVEL_WARNING, ##t);
#define GERB_MESSAGE(t...)  g_log(NULL, G_LOG_LEVEL_MESSAGE, ##t);

/* from old amacro.h */
enum opcodes {NOP,      /* No Operation */
	      PUSH,     /* Ordinary stack operations. Uses float */
	      PPUSH,    /* Parameter onto stack, 1 is first parameter and 
			    so on (compare gerber $1, $2 and so on */ 
	      PPOP,     /* Data on stack popped onto parameter register.
			   First parameter popped from stack is which register
			   to store data and second parameter popped is actual
			   data. */
	      ADD, SUB, /* Mathematical operations */
	      MUL, DIV, 
	      PRIM};    /* Defines what primitive to draw. Inparameters
			   should be on the stack. */


typedef struct instruction {
    enum opcodes opcode;
    union {
	int ival;
	float fval;
    } data;
    struct instruction *next;
} instruction_t;

typedef struct amacro {
    char *name;
    instruction_t *program;
    unsigned int nuf_push;  /* Nuf pushes in program to estimate stack size */
    struct amacro *next;
} amacro_t;


/* from old gerb_aperture.h header file */
#define APERTURE_MIN 10
#define APERTURE_MAX 9999

/*
 * Maximum number of aperture parameters is set by the outline aperture
 * macro. There (p. 28) is defined up to 50 points in polygon.
 * So 50 points with x and y plus two for holding extra data gives...
 */
#define APERTURE_PARAMETERS_MAX 102

enum aperture_t {APERTURE_NONE, CIRCLE, RECTANGLE, OVAL, POLYGON, MACRO, 
		 MACRO_CIRCLE, MACRO_OUTLINE, MACRO_POLYGON, MACRO_MOIRE, 
		 MACRO_THERMAL, MACRO_LINE20, MACRO_LINE21, MACRO_LINE22};
enum aperture_state_t {OFF, ON, FLASH};
enum unit_t {INCH, MM, UNIT_UNSPECIFIED};

typedef struct gerb_simplified_amacro {
    enum aperture_t type;
    double parameter[APERTURE_PARAMETERS_MAX];
    struct gerb_simplified_amacro *next;
} gerb_simplified_amacro_t;

typedef struct gerb_aperture {
    enum aperture_t type;
    amacro_t *amacro;
    gerb_simplified_amacro_t *simplified;
    double parameter[APERTURE_PARAMETERS_MAX];
    int nuf_parameters;
    enum unit_t unit;
} gerb_aperture_t;

/* from old gerb_stats.h */
/* the gerb_aperture_list is used to keep track of 
 * apertures used in stats reporting */
typedef struct gerb_aperture_list_t {
    int number;
    int layer;
    int count;
    enum aperture_t type;
    double parameter[5];
    struct gerb_aperture_list_t *next;
} gerb_aperture_list_t;


typedef struct {
    struct error_list_t *error_list;
    struct gerb_aperture_list_t *aperture_list;
    struct gerb_aperture_list_t *D_code_list;

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

} gerb_stats_t;

/* from old drill_stats.h */
typedef struct drill_list {
    int drill_num;
    double drill_size;
    char *drill_unit;
    int drill_count;
    struct drill_list *next;
} drill_list_t;

typedef struct {
    int layer_count;
    
    error_list_t *error_list;
    drill_list_t *drill_list;
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

    int unknown;

    char *detect;

} drill_stats_t;

/* from gerb_image.h */
enum polarity_t {POSITIVE, NEGATIVE, DARK, CLEAR};
enum omit_zeros_t {LEADING, TRAILING, EXPLICIT, ZEROS_UNSPECIFIED};
enum coordinate_t {ABSOLUTE, INCREMENTAL};
enum interpolation_t {LINEARx1, LINEARx10, LINEARx01, LINEARx001, 
		      CW_CIRCULAR, CCW_CIRCULAR, PAREA_START, PAREA_END, DELETED};
enum encoding_t {NONE, ASCII, EBCDIC, BCD, ISO_ASCII, EIA };
enum layertype_t {GERBER, DRILL, PICK_AND_PLACE};
enum knockout_t {NOKNOCKOUT, FIXEDKNOCK, BORDER};
enum mirror_state_t {NOMIRROR, FLIPA, FLIPB, FLIPAB};
enum axis_select_t {NOSELECT, SWAPAB};
enum image_justify_type_t {NOJUSTIFY, LOWERLEFT, CENTERJUSTIFY};

enum selection_t {EMPTY, POINT_CLICK, DRAG_BOX};

typedef struct {
	gpointer image;
	gpointer net;
} gerb_selection_item_t;

typedef struct {
	enum selection_t type;
	gdouble lowerLeftX;
	gdouble lowerLeftY;
	gdouble upperRightX;
	gdouble upperRightY;
	GArray *selectedNodeArray;
} gerb_selection_info_t;

typedef struct {
    gdouble translateX;
    gdouble translateY;
    gdouble scaleX;
    gdouble scaleY;
    gboolean inverted;
} gerb_user_transformations_t;

typedef struct gerb_cirseg {
    double cp_x;
    double cp_y;
    double width;  /* of oval */
    double height; /* of oval */
    double angle1;
    double angle2;
} gerb_cirseg_t;

typedef struct gerb_step_and_repeat { /* SR parameters */
    int X;
    int Y;
    double dist_X;
    double dist_Y;
} gerb_step_and_repeat_t;
 
typedef struct {
    gboolean firstInstance;
    enum knockout_t type;
    enum polarity_t polarity;
    gdouble lowerLeftX;
    gdouble lowerLeftY;
    gdouble width;
    gdouble height;
    gdouble border;
} gerb_knockout_t;
 
typedef struct {
    gerb_step_and_repeat_t stepAndRepeat;
    gerb_knockout_t knockout;
    gdouble rotation;
    enum polarity_t polarity; 
    gchar *name;
    gpointer next;
} gerb_layer_t;

typedef struct {
    enum axis_select_t axisSelect;
    enum mirror_state_t mirrorState;
    enum unit_t unit;
    gdouble offsetA;
    gdouble offsetB;
    gdouble scaleA;
    gdouble scaleB;
    gpointer next;
} gerb_netstate_t;

typedef struct gerb_net {
    double start_x;
    double start_y;
    double stop_x;
    double stop_y;
    int aperture;
    enum aperture_state_t aperture_state;
    enum interpolation_t interpolation;
    struct gerb_cirseg *cirseg;
    struct gerb_net *next;
    GString *label;
    gerb_layer_t *layer;
    gerb_netstate_t *state;
} gerb_net_t;

typedef struct gerb_format {
    enum omit_zeros_t omit_zeros;
    enum coordinate_t coordinate;
    int x_int;
    int x_dec;
    int y_int;
    int y_dec;
    int lim_seqno; /* Length limit for codes of sequence number */
    int lim_gf;    /* Length limit for codes of general function */
    int lim_pf;    /* Length limit for codes of plot function */
    int lim_mf;    /* Length limit for codes of miscellaneous function */
} gerb_format_t;
	
	
typedef struct gerb_image_info {
    char *name;
    enum polarity_t polarity;
    double min_x; /* Always in inches */
    double min_y;
    double max_x;
    double max_y;
    double offsetA;
    double offsetB;
    enum encoding_t encoding;
    double imageRotation;
    enum image_justify_type_t imageJustifyTypeA;
    enum image_justify_type_t imageJustifyTypeB;
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
    HID_Attribute *attr_list;
    int n_attr;
} gerb_image_info_t;

typedef struct {
    enum layertype_t layertype;
    gerb_aperture_t *aperture[APERTURE_MAX];
    gerb_layer_t *layers;
    gerb_netstate_t *states;
    amacro_t *amacro;
    gerb_format_t *format;
    gerb_image_info_t *info;
    gerb_net_t *netlist;
    gerb_stats_t *gerb_stats;
    drill_stats_t *drill_stats;
} gerb_image_t;


/* from old gerbv_screen.h */
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

typedef enum {NORMAL, IN_MOVE, IN_ZOOM_OUTLINE, IN_MEASURE, ALT_PRESSED,
		IN_SELECTION_DRAG, SCROLLBAR} gerbv_state_t;
typedef enum {POINTER, PAN, ZOOM, MEASURE} gerbv_tool_t;
typedef enum {GERBV_MILS, GERBV_MMS, GERBV_INS} gerbv_unit_t;

typedef struct {
    gerb_image_t *image;
    GdkColor color;
    guint16 alpha;
    gboolean isVisible;
    gpointer privateRenderData;
    gchar *fullPathname; /* this should be the full pathname to the file */
    gchar *name;
    gerb_user_transformations_t transform;
} gerbv_fileinfo_t;

typedef struct {
	double x1, y1;
	double x2, y2;
} gerbv_bbox_t;

typedef struct {
    GdkColor  background;
    int max_files;
    gerbv_fileinfo_t **file;
    int curr_index;
    int last_loaded;

    gchar *path;
    gchar *execpath;    /* Path to executed version of gerbv */
    gchar *project;     /* Current project to simplify save next time */
} gerbv_project_t;

typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, ZOOM_SET } gerbv_zoom_dir_t;
typedef struct {
    gerbv_zoom_dir_t z_dir;
    GdkEventButton *z_event;
    int scale;
} gerbv_zoom_data_t;

#ifdef __cplusplus
}
#endif
void 
gerbv_open_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename);

void 
gerbv_open_layer_from_filename(gerbv_project_t *gerbvProject, gchar *filename);

gboolean 
gerbv_save_layer_from_index(gerbv_project_t *gerbvProject, gint index, gchar *filename);

void 
gerbv_save_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename);

void 
gerbv_save_as_project_from_filename(gerbv_project_t *gerbvProject, gchar *filename);

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
gerbv_add_parsed_image_to_project (gerbv_project_t *gerbvProject, gerb_image_t *parsed_image,
			gchar *filename, gchar *baseName, int idx, int reload);
int
gerbv_open_image(gerbv_project_t *gerbvProject, char *filename, int idx, int reload,
		HID_Attribute *fattr, int n_fattr, gboolean forceLoadFile);
		
/* Notice that the pixel field is used for alpha in this case */
typedef struct{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
}LayerColor;

/* old render.h stuff */
typedef struct {
	gdouble scaleFactorX;
	gdouble scaleFactorY;
	gdouble lowerLeftX;
	gdouble lowerLeftY;
	gint renderType; /* 0 is default */
	gint displayWidth;
	gint displayHeight;
} gerbv_render_info_t;

typedef struct {
    double left;
    double right;
    double top;
    double bottom;
} gerbv_render_size_t;

void
gerbv_render_get_boundingbox(gerbv_project_t *gerbvProject, gerbv_render_size_t *boundingbox);

void
gerbv_render_zoom_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo);

void
gerbv_render_translate_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo);

void
gerbv_render_to_pixmap_using_gdk (gerbv_project_t *gerbvProject, GdkPixmap *pixmap,
		gerbv_render_info_t *renderInfo, gerb_selection_info_t *selectionInfo,
		GdkColor *selectionColor);

void
gerbv_render_all_layers_to_cairo_target_for_vector_output (gerbv_project_t *gerbvProject,
		cairo_t *cr, gerbv_render_info_t *renderInfo);

void
gerbv_render_all_layers_to_cairo_target (gerbv_project_t *gerbvProject, cairo_t *cr,
			gerbv_render_info_t *renderInfo);

void
gerbv_render_layer_to_cairo_target (cairo_t *cr, gerbv_fileinfo_t *fileInfo,
						gerbv_render_info_t *renderInfo);
						
void
gerbv_render_cairo_set_scale_and_translation(cairo_t *cr, gerbv_render_info_t *renderInfo);

void
gerbv_render_layer_to_cairo_target_without_transforming(cairo_t *cr, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo );

/* export functions */
#ifdef EXPORT_PNG
void exportimage_export_to_png_file_autoscaled (gerbv_project_t *gerbvProject, int widthInPixels, int heightInPixels, gchar const* filename);
void exportimage_export_to_png_file (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo, gchar const* filename);
#endif

void exportimage_export_to_pdf_file_autoscaled (gerbv_project_t *gerbvProject, int widthInPoints, int heightInPoints, gchar const* filename);
void exportimage_export_to_pdf_file (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo, gchar const* filename);

void exportimage_export_to_postscript_file_autoscaled (gerbv_project_t *gerbvProject, int widthInPoints, int heightInPoints, gchar const* filename);
void exportimage_export_to_postscript_file (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo, gchar const* filename);

void exportimage_export_to_svg_file_autoscaled (gerbv_project_t *gerbvProject, int widthInPoints, int heightInPoints, gchar const* filename);
void exportimage_export_to_svg_file (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo, gchar const* filename);

gboolean
export_rs274x_file_from_image (gchar *filename, gerb_image_t *image);

gboolean
export_drill_file_from_image (gchar *filename, gerb_image_t *image);

#endif /* GERBV_H */

