/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
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

/** \file pick-and-place.c
    \brief PNP (pick-and-place) parsing functions
    \ingroup libgerbv
*/

#include "gerbv.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "gerber.h"
#include "common.h"
#include "csv.h"
#include "pick-and-place.h"

static gerbv_net_t *pnp_new_net(gerbv_net_t *net); 
static void pnp_reset_bbox (gerbv_net_t *net);
static void pnp_init_net(gerbv_net_t *net, gerbv_image_t *image,
		const char *label,
		gerbv_aperture_state_t apert_state,
		gerbv_interpolation_t interpol);

void gerb_transf_free(gerbv_transf_t *transf)
{
    g_free(transf);
}


void gerb_transf_reset(gerbv_transf_t* transf)
{
    memset(transf,0,sizeof(gerbv_transf_t));
    
    transf->r_mat[0][0] = transf->r_mat[1][1] = 1.0; /*off-diagonals 0 diagonals 1 */
    //transf->r_mat[1][0] = transf->r_mat[0][1] = 0.0;
    transf->scale = 1.0;
    //transf->offset[0] = transf->offset[1] = 0.0;
    
} /* gerb_transf_reset */


gerbv_transf_t* gerb_transf_new(void)
{
    gerbv_transf_t *transf;

    transf = g_new(gerbv_transf_t, 1);
    gerb_transf_reset(transf);

    return transf;
} /* gerb_transf_new */


//!Rotation
/*! append rotation to transformation.
@param transf transformation to be modified
@param angle in rad (counterclockwise rotation) */

void gerb_transf_rotate(gerbv_transf_t* transf, double angle)
{
    double m[2][2]; 
    double s = sin(angle), c = cos(angle);
      
    memcpy(m, transf->r_mat, sizeof(m));    
    transf->r_mat[0][0] = c * m[0][0] - s * m[1][0];
    transf->r_mat[0][1] = c * m[0][1] - s * m[1][1];
    transf->r_mat[1][0] = s * m[0][0] + c * m[1][0];
    transf->r_mat[1][1] = s * m[0][1] + c * m[1][1];
//    transf->offset[0] = transf->offset[1] = 0.0; CHECK ME
    
} /* gerb_transf_rotate */

//!Translation
/*! append translation to transformation.
@param transf transformation to be modified
@param shift_x translation in x direction
@param shift_y translation in y direction */

void gerb_transf_shift(gerbv_transf_t* transf, double shift_x, double shift_y)
{
      
    transf->offset[0] += shift_x;
    transf->offset[1] += shift_y;
            
} /* gerb_transf_shift */

void gerb_transf_apply(double x, double y, gerbv_transf_t* transf, double *out_x, double *out_y)
{

//    x += transf->offset[0];
//    y += transf->offset[1];
    *out_x = (x * transf->r_mat[0][0] + y * transf->r_mat[0][1]) * transf->scale;
    *out_y = (x * transf->r_mat[1][0] + y * transf->r_mat[1][1]) * transf->scale;
    *out_x += transf->offset[0];
    *out_y += transf->offset[1];
    
    
} /* gerb_transf_apply */

void
pick_and_place_reset_bounding_box (gerbv_net_t *net) {
	net->boundingBox.left = -HUGE_VAL;
	net->boundingBox.right = HUGE_VAL;
	net->boundingBox.bottom = -HUGE_VAL;
	net->boundingBox.top = HUGE_VAL;
}

/* Parses a string representing float number with a unit.
 * Default unit can be specified with def_unit. */
static double
pick_and_place_get_float_unit(const char *str, const char *def_unit)
{
    double x = 0.0;
    char unit_str[41] = {0,};
    const char *unit = unit_str;

    /* float, optional space, optional unit mm,cm,in,mil */
    sscanf(str, "%lf %40s", &x, unit_str);

    if (unit_str[0] == '\0')
	unit = def_unit;

    /* NOTE: in order of comparability,
     * i.e. "mm" before "m", as "m" will match "mm" */
    if (strstr(unit, "mm")) {
	x /= 25.4;
    } else if (strstr(unit, "in")) {
	/* NOTE: "in" is without scaling. */
    } else if (strstr(unit, "cmil")) {
	x /= 1e5;
    } else if (strstr(unit, "dmil")) {
	x /= 1e4;
    } else if (strstr(unit, "mil")) {
	x /= 1e3;
    } else if (strstr(unit, "km")) {
	x /= 25.4/1e6;
    } else if (strstr(unit, "dm")) {
	x /= 25.4/100;
    } else if (strstr(unit, "cm")) {
	x /= 25.4/10;
    } else if (strstr(unit, "um")) {
	x /= 25.4*1e3;
    } else if (strstr(unit, "nm")) {
	x /= 25.4*1e6;
    } else if (strstr(unit,  "m")) {
	x /= 25.4/1e3;
    } else { /* default to "mil" */
	x /= 1e3;
    }

    return x;
} /* pick_and_place_get_float_unit */


/** search a string for a delimiter.
 Must occur at least n times. */
int 
pick_and_place_screen_for_delimiter(char *str, int n)
{
    char *ptr;
    char delimiter[4] = "|,;:";
    int counter[4];
    int idx, idx_max = 0;

    memset(counter, 0, sizeof(counter));
    for(ptr = str; *ptr; ptr++) {
	switch(*ptr) {
	case '|':
	    idx = 0;
	    break;
	case ',':
	    idx = 1;
	    break;
	case ';':
	    idx = 2;
	    break;
	case ':':
	    idx = 3;
	    break;
	default:
	    continue;
	    break;
	}
	counter[idx]++;
	if(counter[idx] > counter[idx_max]) {
	    idx_max = idx;
	}
    }
    
    if (counter[idx_max] > n) {
	return (unsigned char) delimiter[idx_max];
    } else {
	return -1;
    }
} /* pick_and_place_screen_for_delimiter */


/**Parses the PNP data.
   two lists are filled with the row data.\n One for the scrollable list in the search and select parts interface, the other one a mere two columned list, which drives the autocompletion when entering a search.\n
   It also tries to determine the shape of a part and sets  pnp_state->shape accordingly which will be used when drawing the selections as an overlay on screen.
   @return the initial node of the pnp_state netlist
 */

GArray *
pick_and_place_parse_file(gerb_file_t *fd)
{
    PnpPartData   pnpPartData;
    int           lineCounter = 0, parsedLines = 0;
    int           ret;
    char          *row[12];
    char          buf[MAXL+2], buf0[MAXL+2];
    char          def_unit[41] = {0,};
    double        tmp_x, tmp_y;
    gerbv_transf_t *tr_rot = gerb_transf_new();
    GArray 	*pnpParseDataArray = g_array_new (FALSE, FALSE, sizeof(PnpPartData));
    gboolean foundValidDataRow = FALSE;
    /* Unit declaration for "PcbXY Version 1.0" files as exported by pcb */
    const char *def_unit_prefix = "# X,Y in ";
    
    /*
     * many locales redefine "." as "," and so on, so sscanf has problems when
     * reading Pick and Place files using %f format 
     */
    setlocale(LC_NUMERIC, "C" );

    while ( fgets(buf, MAXL, fd->fd) != NULL ) {
	int len = strlen(buf)-1;
	int i_length = 0, i_width = 0;
	
	lineCounter += 1; /*next line*/
	if(lineCounter < 2) {
	    /* 
	     * TODO in principle column names could be read and interpreted
	     * but we skip the first line with names of columns for this time
	     */
	    continue;
	}
	if(len >= 0 && buf[len] == '\n') {
	    buf[len--] = 0;
	}
	if(len >= 0 && buf[len] == '\r') {
	    buf[len--] = 0;
	}
	if (0 == strncmp(buf, def_unit_prefix, strlen(def_unit_prefix))) {
	    sscanf(&buf[strlen(def_unit_prefix)], "%40s.", def_unit);
	}
	if (len <= 11)  {  //lets check a minimum length of 11
	    continue;
	}

	if ((len > 0) && (buf[0] == '%')) {
	    continue;
	}

	/* Abort if we see a G54 */
	if ((len > 4) && (strncmp(buf,"G54 ", 4) == 0)) {
	    g_array_free (pnpParseDataArray, TRUE);
	    return NULL;  
	}

	/* abort if we see a G04 code */
	if ((len > 4) && (strncmp(buf,"G04 ", 4) == 0)) {
	    g_array_free (pnpParseDataArray, TRUE);
	    return NULL;  
	}

	/* this accepts file both with and without quotes */
/* 	if (!pnp_state) { /\* we are in first line *\/ */
/* 	   if ((delimiter = pnp_screen_for_delimiter(buf, 8)) < 0) { */
/* 	   continue; */
/* 	   } */
/* 	} */

	ret = csv_row_parse(buf, MAXL,  buf0, MAXL, row, 11, ',',   CSV_QUOTES);

	if (ret > 0) {
	    foundValidDataRow = TRUE;
	} else {
	    continue;
	}
/* 	printf("direct:%s, %s, %s, %s, %s, %s, %s, %s, %s, %s,  %s, ret %d\n", row[0], row[1], row[2],row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10], ret);        */
/* 	g_warning ("FFF %s %s\n",row[8],row[6]); */

	if (row[0] && row[8]) { // here could be some better check for the syntax
	    snprintf (pnpPartData.designator, sizeof(pnpPartData.designator)-1, "%s", row[0]);
	    snprintf (pnpPartData.footprint, sizeof(pnpPartData.footprint)-1, "%s", row[1]);
	    snprintf (pnpPartData.layer, sizeof(pnpPartData.layer)-1, "%s", row[8]);
	    if (row[10] != NULL) {
		if ( ! g_utf8_validate(row[10], -1, NULL)) {
		    gchar * str = g_convert(row[10], strlen(row[10]), "UTF-8", "ISO-8859-1",
					    NULL, NULL, NULL);
		    // I have not decided yet whether it is better to use always
		    // "ISO-8859-1" or current locale.
		    // str = g_locale_to_utf8(row[10], -1, NULL, NULL, NULL);
		    snprintf (pnpPartData.comment, sizeof(pnpPartData.comment)-1, "%s", str);
		    g_free(str);
		} else {
		    snprintf (pnpPartData.comment, sizeof(pnpPartData.comment)-1, "%s", row[10]);
		}
	    }
	    /*
	      gchar* g_convert(const gchar *str, gssize len, const gchar *to_codeset, const gchar *from_codeset, gsize *bytes_read, gsize *bytes_written, GError **error);
	    */
	    pnpPartData.mid_x = pick_and_place_get_float_unit(row[2], def_unit);
	    pnpPartData.mid_y = pick_and_place_get_float_unit(row[3], def_unit);
	    pnpPartData.ref_x = pick_and_place_get_float_unit(row[4], def_unit);
	    pnpPartData.ref_y = pick_and_place_get_float_unit(row[5], def_unit);
	    pnpPartData.pad_x = pick_and_place_get_float_unit(row[6], def_unit);
	    pnpPartData.pad_y = pick_and_place_get_float_unit(row[7], def_unit);
	    /* This line causes segfault if we accidently starts parsing 
	     * a gerber file. It is crap crap crap */
	    if (row[9])
		sscanf(row[9], "%lf", &pnpPartData.rotation); // no units, always deg
	}
	/* for now, default back to PCB program format
	 * TODO: implement better checking for format
	 */
	else if (row[0] && row[1] && row[2] && row[3] && row[4] && row[5] && row[6]) {
	    snprintf (pnpPartData.designator, sizeof(pnpPartData.designator)-1, "%s", row[0]);
	    snprintf (pnpPartData.footprint, sizeof(pnpPartData.footprint)-1, "%s", row[1]);		
	    snprintf (pnpPartData.layer, sizeof(pnpPartData.layer)-1, "%s", row[6]);	
	    pnpPartData.mid_x = pick_and_place_get_float_unit(row[3], def_unit);
	    pnpPartData.mid_y = pick_and_place_get_float_unit(row[4], def_unit);
	    pnpPartData.pad_x = pnpPartData.mid_x + 0.03;
	    pnpPartData.pad_y = pnpPartData.mid_y + 0.03;
	    sscanf(row[5], "%lf", &pnpPartData.rotation); // no units, always deg
	    /* check for coordinate sanity, and abort if it fails
	     * Note: this is mainly to catch comment lines that get parsed
	     */
	    if ((fabs(pnpPartData.mid_x) < 0.001)&&(fabs(pnpPartData.mid_y) < 0.001)) {
		continue;			
	    }
	} else {
	    continue;
	}


	/* 
	 * now, try and figure out the actual footprint shape to draw, or just
	 * guess something reasonable
	 */
	if(sscanf(pnpPartData.footprint, "%02d%02d", &i_length, &i_width) == 2) {
	    // parse footprints like 0805 or 1206
	    pnpPartData.length = 0.01 * i_length;
	    pnpPartData.width = 0.01 * i_width;
	    pnpPartData.shape = PART_SHAPE_RECTANGLE;
	} else { 
	    gerb_transf_reset(tr_rot);
	    gerb_transf_rotate(tr_rot, -DEG2RAD(pnpPartData.rotation));/* rotate it back to get dimensions */
	    gerb_transf_apply( pnpPartData.pad_x -  pnpPartData.mid_x, 
			       pnpPartData.pad_y - pnpPartData.mid_y, tr_rot, &tmp_x, &tmp_y);
	    if ((fabs(tmp_y) > fabs(tmp_x/100)) && (fabs(tmp_x) > fabs(tmp_y/100))){
		pnpPartData.length = 2 * fabs(tmp_x);/* get dimensions*/
		pnpPartData.width = 2 * fabs(tmp_y);
		pnpPartData.shape = PART_SHAPE_STD;
	    } else {
		pnpPartData.length = 0.015;
		pnpPartData.width = 0.015;
		pnpPartData.shape = PART_SHAPE_UNKNOWN;
	    }
	}  
	g_array_append_val (pnpParseDataArray, pnpPartData);
	parsedLines += 1;
    }   
    gerb_transf_free(tr_rot);
    /* fd->ptr=0; */
    /* rewind(fd->fd); */
	
    /* so a sanity check and see if this is a valid pnp file */
    if ((((float) parsedLines / (float) lineCounter) < 0.3) ||
	(!foundValidDataRow)) {
	/* this doesn't look like a valid PNP file, so return error */
	g_array_free (pnpParseDataArray, TRUE);
	return NULL;
    }     
    return pnpParseDataArray;
} /* pick_and_place_parse_file */


/*	------------------------------------------------------------------
 *	pick_and_place_check_file_type
 *	------------------------------------------------------------------
 *	Description: Tries to parse the given file into a pick-and-place
 *		data set. If it fails to read any good rows, then returns
 *		FALSE, otherwise it returns TRUE.
 *	Notes: 
 *	------------------------------------------------------------------
 */
gboolean
pick_and_place_check_file_type(gerb_file_t *fd, gboolean *returnFoundBinary)
{
    char *buf;
    int len = 0;
    int i;
    char *letter;
    gboolean found_binary = FALSE;
    gboolean found_G54 = FALSE;
    gboolean found_M0 = FALSE;
    gboolean found_M2 = FALSE;
    gboolean found_G2 = FALSE;
    gboolean found_ADD = FALSE;
    gboolean found_comma = FALSE;
    gboolean found_R = FALSE;
    gboolean found_U = FALSE;
    gboolean found_C = FALSE;
    gboolean found_boardside = FALSE;

    buf = malloc(MAXL);
    if (buf == NULL)
	GERB_FATAL_ERROR("malloc buf failed in %s()", __FUNCTION__);

    while (fgets(buf, MAXL, fd->fd) != NULL) {
	len = strlen(buf);
     
	/* First look through the file for indications of its type */
	
	/* check for non-binary file */
	for (i = 0; i < len; i++) {
	    if (!isprint((int) buf[i]) && (buf[i] != '\r') && 
                (buf[i] != '\n') && (buf[i] != '\t')) {
		found_binary = TRUE;
	    }
	}
	
	if (g_strstr_len(buf, len, "G54")) {
	    found_G54 = TRUE;
	}
	if (g_strstr_len(buf, len, "M00")) {
	    found_M0 = TRUE;
	}
	if (g_strstr_len(buf, len, "M02")) {
	    found_M2 = TRUE;
	}
	if (g_strstr_len(buf, len, "G02")) {
	    found_G2 = TRUE;
	}
	if (g_strstr_len(buf, len, "ADD")) {
	    found_ADD = TRUE;
	}
	if (g_strstr_len(buf, len, ",")) {
	    found_comma = TRUE;
	}
	/* Semicolon can be separator too */
	if (g_strstr_len(buf, len, ";")) {
	    found_comma = TRUE;
	}
	
	/* Look for refdes -- This is dumb, but what else can we do? */
	if ((letter = g_strstr_len(buf, len, "R")) != NULL) {
	    if (isdigit((int) letter[1])) { /* grab char after R */
		found_R = TRUE;
	    }
	}
	if ((letter = g_strstr_len(buf, len, "C")) != NULL) {
	    if (isdigit((int) letter[1])) { /* grab char after C */
		found_C = TRUE;
	    }
	}
	if ((letter = g_strstr_len(buf, len, "U")) != NULL) {
	    if (isdigit((int) letter[1])) { /* grab char after U */
		found_U = TRUE;
	    }
	}
	
	/* Look for board side indicator since this is required
	 * by many vendors */
	if (g_strstr_len(buf, len, "top")) {
	    found_boardside = TRUE;
	}
	if (g_strstr_len(buf, len, "Top")) {
	    found_boardside = TRUE;
	}
	if (g_strstr_len(buf, len, "TOP")) {
	    found_boardside = TRUE;
	}
	/* Also look for evidence of "Layer" in header.... */
	if (g_strstr_len(buf, len, "ayer")) {
	    found_boardside = TRUE;
	}
	if (g_strstr_len(buf, len, "AYER")) {
	    found_boardside = TRUE;
	}
	
    }
    rewind(fd->fd);
    free(buf);

    /* Now form logical expression determining if this is a pick-place file */
    *returnFoundBinary = found_binary;
    if (found_G54) 
	return FALSE;
    if (found_M0) 
	return FALSE;
    if (found_M2) 
	return FALSE;
    if (found_G2) 
	return FALSE;
    if (found_ADD) 
	return FALSE;
    if (found_comma && (found_R || found_C || found_U) && 
	found_boardside) 
	return TRUE;
    
    return FALSE;
    
} /* pick_and_place_check_file_type */


/*	------------------------------------------------------------------
 *	pick_and_place_convert_pnp_data_to_image
 *	------------------------------------------------------------------
 *	Description: Render a parsedPickAndPlaceData array into a gerb_image.
 *	Notes:
 *	------------------------------------------------------------------
 */
gerbv_image_t *
pick_and_place_convert_pnp_data_to_image(GArray *parsedPickAndPlaceData, gint boardSide) 
{
    gerbv_image_t *image = NULL;
    gerbv_net_t *curr_net = NULL;
    int i;
    gerbv_transf_t *tr_rot = gerb_transf_new();
    gerbv_drill_stats_t *stats;  /* Eventually replace with pick_place_stats */
    gboolean foundElement = FALSE;
    const double draw_width = 0.01;

    /* step through and make sure we have an element on the layer before
       we actually create a new image for it and fill it */
    for (i = 0; i < parsedPickAndPlaceData->len; i++) {
	PnpPartData partData = g_array_index(parsedPickAndPlaceData, PnpPartData, i);
	
	if ((boardSide == 0) && !((partData.layer[0]=='b') || (partData.layer[0]=='B')))
		continue;
	if ((boardSide == 1) && !((partData.layer[0]=='t') || (partData.layer[0]=='T')))
		continue;
		
	foundElement = TRUE;
    }
    if (!foundElement)
    	return NULL;

    image = gerbv_create_image(image, "Pick and Place (X-Y) File");
    if (image == NULL) {
	GERB_FATAL_ERROR("malloc image failed in %s()", __FUNCTION__);
    }

    image->format = g_new0(gerbv_format_t, 1);
    if (image->format == NULL) {
	GERB_FATAL_ERROR("malloc format failed in %s()", __FUNCTION__);
    }

    /* Separate top/bot layer type is needed for reload purpose */
    if (boardSide == 1)
	    image->layertype = GERBV_LAYERTYPE_PICKANDPLACE_TOP;
    else
	    image->layertype = GERBV_LAYERTYPE_PICKANDPLACE_BOT;

    stats = gerbv_drill_stats_new();
    if (stats == NULL)
        GERB_FATAL_ERROR("malloc pick_place_stats failed in %s()",
			__FUNCTION__);
    image->drill_stats = stats;


    curr_net = image->netlist;
    curr_net->layer = image->layers;
    curr_net->state = image->states;
    pnp_reset_bbox (curr_net);	
    image->info->min_x = HUGE_VAL;
    image->info->min_y = HUGE_VAL;
    image->info->max_x = -HUGE_VAL;
    image->info->max_y = -HUGE_VAL;

    image->aperture[0] = g_new0(gerbv_aperture_t, 1);
    assert(image->aperture[0] != NULL);
    image->aperture[0]->type = GERBV_APTYPE_CIRCLE;
    image->aperture[0]->amacro = NULL;
    image->aperture[0]->parameter[0] = draw_width;
    image->aperture[0]->nuf_parameters = 1;

    for (i = 0; i < parsedPickAndPlaceData->len; i++) {
	PnpPartData partData = g_array_index(parsedPickAndPlaceData, PnpPartData, i);
	float radius,labelOffset;  

	curr_net = pnp_new_net(curr_net);
	curr_net->layer = image->layers;
	curr_net->state = image->states;

	if ((partData.rotation > 89) && (partData.rotation < 91))
		labelOffset = fabs(partData.length/2);
	else if ((partData.rotation > 179) && (partData.rotation < 181))
		labelOffset = fabs(partData.width/2);
	else if ((partData.rotation > 269) && (partData.rotation < 271))
		labelOffset = fabs(partData.length/2);
	else if ((partData.rotation > -91) && (partData.rotation < -89))
		labelOffset = fabs(partData.length/2);
	else if ((partData.rotation > -181) && (partData.rotation < -179))
		labelOffset = fabs(partData.width/2);
	else if ((partData.rotation > -271) && (partData.rotation < -269))
		labelOffset = fabs(partData.length/2);
	else labelOffset = fabs(partData.width/2);

	partData.rotation = DEG2RAD(partData.rotation);

	/* check if the entry is on the specified layer */
	if ((boardSide == 0) && !((partData.layer[0]=='b') || (partData.layer[0]=='B')))
		continue;
	if ((boardSide == 1) && !((partData.layer[0]=='t') || (partData.layer[0]=='T')))
		continue;

	curr_net = pnp_new_net(curr_net);
	pnp_init_net(curr_net, image, partData.designator,
			GERBV_APERTURE_STATE_OFF,
			GERBV_INTERPOLATION_LINEARx1);

	/* First net of PNP is just a label holder, so calculate the lower left
	 * location to line up above the element */
	curr_net->start_x = curr_net->stop_x = partData.mid_x;
	curr_net->start_y = curr_net->stop_y =
		partData.mid_y + labelOffset + draw_width;   

	gerb_transf_reset(tr_rot);
	gerb_transf_shift(tr_rot, partData.mid_x, partData.mid_y);
	gerb_transf_rotate(tr_rot, -partData.rotation);
	    
	if ((partData.shape == PART_SHAPE_RECTANGLE) ||
	    (partData.shape == PART_SHAPE_STD)) {
	    // TODO: draw rectangle length x width taking into account rotation or pad x,y

	    curr_net = pnp_new_net(curr_net);
	    pnp_init_net(curr_net, image, partData.designator,
			    GERBV_APERTURE_STATE_ON,
			    GERBV_INTERPOLATION_LINEARx1);

	    gerb_transf_apply(partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
	    gerb_transf_apply(-partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);
	    
/* TODO: write unifying function */

	    curr_net = pnp_new_net(curr_net);
	    pnp_init_net(curr_net, image, partData.designator,
			    GERBV_APERTURE_STATE_ON,
			    GERBV_INTERPOLATION_LINEARx1);

	    gerb_transf_apply(-partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
	    gerb_transf_apply(-partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);

	    curr_net = pnp_new_net(curr_net);
	    pnp_init_net(curr_net, image, partData.designator,
			    GERBV_APERTURE_STATE_ON,
			    GERBV_INTERPOLATION_LINEARx1);

	    gerb_transf_apply(-partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
	    gerb_transf_apply(partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);

	    curr_net = pnp_new_net(curr_net);
	    pnp_init_net(curr_net, image, partData.designator,
			    GERBV_APERTURE_STATE_ON,
			    GERBV_INTERPOLATION_LINEARx1);

	    gerb_transf_apply(partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
	    gerb_transf_apply(partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);

	    curr_net = pnp_new_net(curr_net);
	    pnp_init_net(curr_net, image, partData.designator,
			    GERBV_APERTURE_STATE_ON,
			    GERBV_INTERPOLATION_LINEARx1);

	    if (partData.shape == PART_SHAPE_RECTANGLE) {
		gerb_transf_apply(partData.length/4, -partData.width/2, tr_rot, 
				  &curr_net->start_x, &curr_net->start_y);
		gerb_transf_apply(partData.length/4, partData.width/2, tr_rot, 
				  &curr_net->stop_x, &curr_net->stop_y);
	    } else {
		gerb_transf_apply(partData.length/4, partData.width/2, tr_rot, 
				  &curr_net->start_x, &curr_net->start_y);
		gerb_transf_apply(partData.length/4, partData.width/4, tr_rot, 
				  &curr_net->stop_x, &curr_net->stop_y);

		curr_net = pnp_new_net(curr_net);
		pnp_init_net(curr_net, image, partData.designator,
				GERBV_APERTURE_STATE_ON,
				GERBV_INTERPOLATION_LINEARx1);

		gerb_transf_apply(partData.length/2, partData.width/4, tr_rot, 
				  &curr_net->start_x, &curr_net->start_y);
		gerb_transf_apply(partData.length/4, partData.width/4, tr_rot, 
				  &curr_net->stop_x, &curr_net->stop_y);     
	    }

	    /* calculate a rough radius for the min/max screen calcs later */
	    radius = MAX(partData.length/2, partData.width/2);
	} else {
	    gdouble tmp_x,tmp_y;

	    pnp_init_net(curr_net, image, partData.designator,
			    GERBV_APERTURE_STATE_ON,
			    GERBV_INTERPOLATION_LINEARx1);

	    curr_net->start_x = partData.mid_x;
	    curr_net->start_y = partData.mid_y;
	    gerb_transf_apply( partData.pad_x -  partData.mid_x, 
			       partData.pad_y - partData.mid_y, tr_rot, &tmp_x, &tmp_y);
			       
	    curr_net->stop_x = tmp_x;
	    curr_net->stop_y = tmp_y;


	    curr_net = pnp_new_net(curr_net);
	    pnp_init_net(curr_net, image, partData.designator,
			    GERBV_APERTURE_STATE_ON,
			    GERBV_INTERPOLATION_CW_CIRCULAR);

	    curr_net->start_x = partData.mid_x;
	    curr_net->start_y = partData.mid_y;
	    curr_net->stop_x = partData.pad_x;
	    curr_net->stop_y = partData.pad_y;

	    curr_net->cirseg = g_new0 (gerbv_cirseg_t, 1);
	    curr_net->cirseg->angle1 = 0.0;
	    curr_net->cirseg->angle2 = 360.0;
	    curr_net->cirseg->cp_x = partData.mid_x;
	    curr_net->cirseg->cp_y = partData.mid_y;
	    radius = hypot(partData.pad_x - partData.mid_x,
			    partData.pad_y-partData.mid_y);
	    if (radius < 0.001)
	    	radius = 0.1;
	    curr_net->cirseg->width = 2*radius; /* fabs(pad_x-mid_x) */
	    curr_net->cirseg->height = 2*radius;
	}

	/* 
	 * update min and max numbers so the screen zoom-to-fit 
	 *function will work
	 */
	image->info->min_x = MIN(image->info->min_x, (partData.mid_x - radius - 0.02));
	image->info->min_y = MIN(image->info->min_y, (partData.mid_y - radius - 0.02));
	image->info->max_x = MAX(image->info->max_x, (partData.mid_x + radius + 0.02));
	image->info->max_y = MAX(image->info->max_y, (partData.mid_y + radius + 0.02));
    }
    curr_net->next = NULL;
    
    gerb_transf_free(tr_rot);
    return image;
} /* pick_and_place_convert_pnp_data_to_image */


/*	------------------------------------------------------------------
 *	pick_and_place_parse_file_to_images
 *	------------------------------------------------------------------
 *	Description: Renders a pick and place file to a gerb_image.
 *	If image pointer is not NULL, then corresponding image will not be
 *	populated.
 *	Notes: The file format should already be verified before calling
 *       this function, since it does very little sanity checking itself.
 *	------------------------------------------------------------------
 */
void
pick_and_place_parse_file_to_images(gerb_file_t *fd, gerbv_image_t **topImage,
			gerbv_image_t **bottomImage) 
{ 
	GArray *parsedPickAndPlaceData = pick_and_place_parse_file (fd);

	if (parsedPickAndPlaceData != NULL) {
		/* Non NULL pointer is used as "not to reload" mark */
		if (*bottomImage == NULL)
			*bottomImage = pick_and_place_convert_pnp_data_to_image(parsedPickAndPlaceData, 0);

		if (*topImage == NULL)
			*topImage = pick_and_place_convert_pnp_data_to_image(parsedPickAndPlaceData, 1);

		g_array_free (parsedPickAndPlaceData, TRUE);
	}
} /* pick_and_place_parse_file_to_images */

static gerbv_net_t *
pnp_new_net(gerbv_net_t *net)
{
	gerbv_net_t *n;
	net->next = g_new0(gerbv_net_t, 1);
	n = net->next;
	assert(n != NULL);

	pnp_reset_bbox (n);

	return n;
}

static void
pnp_reset_bbox (gerbv_net_t *net)
{
	net->boundingBox.left = -HUGE_VAL;
	net->boundingBox.right = HUGE_VAL;
	net->boundingBox.bottom = -HUGE_VAL;
	net->boundingBox.top = HUGE_VAL;
}

static void
pnp_init_net(gerbv_net_t *net, gerbv_image_t *image, const char *label,
		gerbv_aperture_state_t apert_state,
		gerbv_interpolation_t interpol)
{
	net->aperture = 0;
	net->aperture_state = apert_state;
	net->interpolation = interpol;
	net->layer = image->layers;
	net->state = image->states;

	if (strlen(label) > 0) {
		net->label = g_string_new (label);
	}
}
