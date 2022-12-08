/*
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
 *
 *   Copyright (C) 2022 Stephen J. Hardy
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

/** \file ipcd356a.c
    \brief IPC-D-356A parsing and annotation implementation
    \ingroup libgerbv
*/

#include "gerbv.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>

#include "common.h"
#include "gerber.h"

#include "ipcd356a.h"
#include "x2attr.h"


//#define dprintf if(DEBUG) printf
#define dprintf if(1) printf


#define MAXL 83     // 80 columns plus CRLF>null>


/*************************************************************
**
**
** IPC-D-356A API.
**
**
**************************************************************/


// Instance of a file sniffer likelihood callback, for detecting IPC-D-356A format.
//
// 1. If binary (except whitespace), or line longer than 80 (not counting trailing CR/LF), then definitely not.
// 1a. If length < 3, return -10.  Probably not a good file, but let's give it a chance.
// 2. If see a line 'P  VER IPC-D-356A[<whitespace>]' then definitely is.
// 3. If see a line 'P  VER IPC-D-356' then probably is but may be different standard version, so return 100.
//    - in this case, flag that we saw a P VER line.
// 4. Return 20 for each line starting with 'P  ', 'C  ', '317' or '327'
// 5. Return 5 for each line starting with 3 decimal digits. 
// 6. In all other cases, return -50 because we're getting sad.
//
// Special: on line 1, if file extension is .ipc or .IPC or a few others, add 250 to return value.
// If EOF (line==NULL) return 'definitely not' if we never saw a P VER line, else 0 (no change).

int
ipcd356a_likely_line(const char * line, file_sniffer_t * fsdata)
{
    int bonus = 0;
    
    if (!line)
        return fsdata->counters[0] ? 0 : FILE_DEFINITELY_NOT;
    if (fsdata->line_num==1 && !strncasecmp(fsdata->ext, "ipc", 3))
        bonus = 250;
    if (fsdata->line_len > 80 || file_type_line_contains_binary(line))
        return FILE_DEFINITELY_NOT;
    if (fsdata->line_len < 3)
        return -10 + bonus;
    if (!strncasecmp(line, "P  VER IPC-D-356A", 17))
        return FILE_DEFINITELY_IS;
    if (!strncasecmp(line, "P  VER IPC-D-356", 16)) {
        ++fsdata->counters[0];
        return 100 + bonus;
    }
    if (!strncasecmp(line, "P  ", 3) || !strncasecmp(line, "C  ", 3) || !strncmp(line, "317", 3) || !strncmp(line, "327", 3))
        return 20 + bonus;
    if (isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]))
        return 5 + bonus;
    return -50 + bonus;
}


/* Parse IPC-D-356A file from the top.

Keeping it simple for now; just parse the header and 317/327 records.

Here's a sample.  Column numbers and spacing are rigorously enforced.

C  Comment...
C  
P  JOB
P  UNITS CUST 1
P  TITLE Pro81FE.tmp
P  NUM 1.0
P  REV A
P  VER IPC-D-356A
P  IMAGE PRIMARY
P  NNAME1     A LIM
P  NNAME2     B LIM
327                 U4    -1         PA01X 074295Y 034925X0850Y0400R000 S0
327                 U4    -6         PA01X 076145Y 034925X0850Y0400R000 S0
317                 M3    -1    D3175PA00X 045400Y 030400X0000          S0
317                 U3    -6    D1300PA00X 060960Y 039370X0000          S0
3273V3              C1    -2         PA01X 095430Y 058420X1800Y1300R090 S0
3273V3              U5    -8         PA01X 078445Y 063140X0350Y1650R000 S0
3173V3              P2    -5    D0900PA00X 067310Y 030480X0000          S0
3173V3              VIA   -     D0711PA00X 082804Y 060071X0000          S0
3173V3              VIA   -     D0711PA00X 073279Y 059055X0000          S0
3175V               U7    -2    D0900PA00X 100965Y 054610X0000          S0
3175V               U6    -15   D0900PA00X 029845Y 041910X0000          S0
3175V               U6    -2    D0900PA00X 042545Y 054610X0000          S0
3175V               P2    -6    D0900PA00X 067310Y 027940X0000          S0
32724V              U2    -1         PA01X 098050Y 061690X0950Y1900R180 S0
31724V              P7    -2    D0900PA00X 079288Y 037211X0000          S0
31724V              VIA   -     D0711PA00X 097282Y 044323X0000          S0
327NNAME1           Q1    -3         PA01X 075885Y 056515X0600Y1500R270 S0
317NNAME1           P4    -5    D1200PA00X 075640Y 075380X0000          S0
317NNAME1           VIA   -     D0711PA00X 074422Y 067818X0000          S0
327NNAME2           Q2    -3         PA01X 075885Y 048895X0600Y1500R270 S0
317NNAME2           P4    -6    D1200PA00X 079450Y 075380X0000          S0
317NNAME2           VIA   -     D0711PA00X 074168Y 065278X0000          S0

Notes:
 UNITS specified as SI (=mm), CUST or CUST 0 (=inch, degree), CUST 1 (mm, degree), CUST 2 (inch, radian)
 mm units then assumes 0.001 resolution, inch is 0.0001.  Fixed point, no decimals.
 
While parsing, we don't know how many conductor layers the PCB has, but we can deduce from the L data.

Try to be forgiving as per the Postel rule.  Just ignore any lines which start with a code we don't understand.

Although the spec doesn't state it, it seems that everything is uppercase.  Just to be sure, we
uppercase each line.

317 records are for through-hole.  Data is:
  [netname|N/C] [refdes|VIA] [pin] [drill] [access] [location] [size] [soldermask]

327 are for SMD:
  Same info as for 317 except no drill and can't be a via.
  
We create a single image struct for the IPC data.  Flash apertures for each unique shape/via/drill/SMD pad/access/soldermask.
Aperture will have attribute for whether it is thru hole or SMD:
    .AperFunction,
        SMDPad
        ComponentPad,{CuDef|SMDef}
        ViaPad
If the aperture function cannot be determined, the attribute is not created. 
     IPCAccess,{n}   - access layer in IPC numbering (1,2...n)
     IPCDrill,diam   - drill diam inch

Unique aperture key is derived from record:
3273V3              U5    -8         PA01X 078445Y 063140X0350Y1650R000 S0
gives
 2                                    A01                X0350Y1650R000 S0
condensed to
 2A01X0350Y1650R000S0

317NNAME1           P4    -5    D1200PA00X 075640Y 075380X0000          S0
gives
 1                              D1200PA00                X0000          S0
condensed to
 1{C|V}D1200PA00X0000S0
where C for component, V for via.

Values stored with aperture key will be aperture index (D number 10, 11, ...).
 
Flashed net objects will have object attributes:
  .N,netname or .N,N/C
  .P,refdes[,pin]
  .C,refdes
Drill diameter, access etc. is obtained from the aperture, since a unique aperture is
generated for each distinct set of such attributes.
*/

typedef struct ipcd356a_state
{
    // State for parsing etc.
    unsigned long layers;       // Layer bitmap (bit 0 for non-copper, else 1,2,...).
    gboolean include_tracks;    // Whether to parse 378 records for conductors.
    gerbv_image_t * image;      // Image being constructed
    char * line;    // Current line, stripped and null term.
    char * p;       // Pointer (into same buffer as above) of next field to process.
    char * errmsg;  // Pointer to 1k buffer.  If not empty, will break from parsing.
                    // If set, should be translated _() and have \n ending.
    int linenum;    // 1's based line number
    int linelen;    // ascii strlen of line.  No UTF-8 smarts!
    
    // Following char * need g_free at end.
    char * job;
    char * title;
    char * num;
    char * rev;
    
    char   ver;     // 'A' for this version of IPC-D-356.  Other versions *may* work.
    gfloat toinch;  // Multiplicative factor to convert (integer) linear dimensions to inch.
    gfloat todeg;   // Multiplicative factor to convert (integer) angular dimensions to degrees.
    gboolean imageskip; // Whether skipping non-primary image
    
    GHashTable * netnames;  // Translate short netnames to real ones.  Key+data are interned strings.
                            // Also store netnames that require no alias, with key==value.

    // Extracted fields from test records (317, 017, 327, 027).  Used temporarily.  Last char of string arrays
    // is always nul.
    char refdes[7];         // Cols 21-26.  May be 'VIA'.
    char pin[5];            // Cols 28-31.
    char midnet;            // Col 32: either 'M' for mid-net point, or blank.
    gfloat holediam;        // Cols 34-37: Drill diameter if 317 record, else 0, in um or tenths, stored here in inch.
    char plating;           // Col 38: either 'P' for plated or 'U' for unplated hole.  ' ' if not a hole.
    int access;             // Cols 40-41: '00' for PTH access both sides, '01' primary, '02' etc. copper layer.
    gfloat locx, locy;      // Cols 43 (sign), 44-49 (x), 51 (sign), 52-57 (y), stored here in inch.
    gfloat sizex, sizey;    // Cols 69-62 (feature X size), 64-67 (feature Y size)
    int rot;                // Cols 69-71 (rotation) stored in integer degrees.
    int soldermask;         // Col 74: soldermask 0=none, 1=primary, 2=secondary, 3=both sides.
    gboolean givenx, giveny, givenrot;  // Whether optional sizey, rot given.  Having a ysize means the feature is rectangular,
                            // else it is round.  IPC groups oblong, octagonal etc. pads as "rectangles".
    
    // Aperture key.  See above block comment.
    char aperkey[33];
    // Aperture table.  Map interned aperkey to ptr to ptr to aperture we have created in the image list.
    // This indirection allows us to quickly work out the aperture index as <value> - image->aperture.
    // Since this is for temp use, it is sufficient to hash on the aperture pointer values.
    GHashTable * aperture_map;
    int ano;    // Next number to create.  Starts at 10 for Gerber compat.

    gerbv_net_t * net;      // Last graphical object created
    gerbv_layer_t * layer;  // Current state data for net.  Only one (the default) used.
    gerbv_netstate_t * netstate;    // Only one of these too.
    
    char outline_type[15];  // Outline type for 389 records
    int outline_aper_index;
    enum {
        DS_SIZE,            // Expecting X<size>Y<size>
        DS_MOVE,            // Expecting X<loc>Y<loc> for start of segment
        DS_LINE             // Expecting X and/or Y locations for 'lineto'.  X,Y are modal from drawx,drawy.
    } drawstate;            // Drawing state for when parsing outline or conductor records.
    gfloat drawx, drawy;    // Current segment drawing position
    const char * netname;   // Conductor netname
    
} ipcd356a_state_t;

static void
_advance_p(ipcd356a_state_t * ist)
{
    // Increment p past current non-space, then past space, to point to EOL or next non-blank char.
    while (!isspace(*ist->p)) ++ist->p;
    while (isspace(*ist->p)) ++ist->p;
}

static char *
_copy_field(ipcd356a_state_t * ist)
{
    // Copy non-blank chars at p and return as strdup.
    char * q = ist->p;
    char * k;
    while (!isspace(*q)) ++q;
    k = g_strndup(ist->p, q-ist->p);
    ist->p = q;
    return k;
}


static void
_parse_header(ipcd356a_state_t * ist)
{
    char * d, * e;
    const char * k, * v;
    
    if (!strncmp(ist->p, "JOB ", 4)) {
        _advance_p(ist);
        ist->job = _copy_field(ist);
    }
    else if (!strncmp(ist->p, "TITLE ", 6)) {
        _advance_p(ist);
        ist->title = _copy_field(ist);
    }
    else if (!strncmp(ist->p, "NUM ", 4)) {
        _advance_p(ist);
        ist->num = _copy_field(ist);
    }
    else if (!strncmp(ist->p, "REV ", 4)) {
        _advance_p(ist);
        ist->rev = _copy_field(ist);
    }
    else if (!strncmp(ist->p, "VER ", 4)) {
        _advance_p(ist);
        d = _copy_field(ist);
        if (!strncmp(d, "IPC-D-356", 9))
            ist->ver = isalpha(d[9]) ? d[9] : 0;    // Will be null, or 'A'
        g_free(d);
    }
    else if (!strncmp(ist->p, "UNITS ", 6)) {
        _advance_p(ist);
        d = _copy_field(ist);   // SI or CUST
        _advance_p(ist);
        e = _copy_field(ist);   // empty, or digit 0..2.
        // For now, raise error if radians used.  Don't know how to process.  Should be rare.
        if (!strcmp(d, "SI")) {
            ist->toinch = 1./25400.;
            sprintf(ist->errmsg, _("IPC-D-356A: do not support SI units\n"));
        }
        else if (!strcmp(d, "CUST")) {
            if (!strcmp(e, "1")) {
                ist->toinch = 1./25400.;
                ist->todeg = 1.; 
            }
            else if (!strcmp(e, "2")) {
                ist->toinch = 1./10000.;
                sprintf(ist->errmsg, _("IPC-D-356A: do not support CUST 2 units\n"));
            }
            else {
                ist->toinch = 1./10000.;
                ist->todeg = 1.;
            }
        }
        g_free(e);
        g_free(d);
    }
    else if (!strncmp(ist->p, "IMAGE ", 6)) {
        _advance_p(ist);
        d = _copy_field(ist);
        // Only parse for IMAGE PRIMARY.  Others are for PANEL and 4-digit step-and-repeat or multi-up
        // number.
        ist->imageskip = strcmp(d, "PRIMARY") != 0;
        g_free(d);
    }
    else if (!strncmp(ist->p, "NNAME", 5)) {
        // This is a netname alias to get around the 14 char no space limitation of the netname field.
        // Cols 9-13 are the alias (usually a number), cols 15-72 are the real CAD/EDA netname.
        // When we create Gerber attribs etc., we'll use the real names.  The alias is only used while
        // parsing IPC.
        d = _copy_field(ist);
        k = g_intern_string(d); // key will be interned "NNAME1", "NNAME2" etc.
        g_free(d);
        _advance_p(ist);
        v = g_intern_string(ist->p); // value will be interned real net name e.g. "FOO BAR"
        g_hash_table_insert(ist->netnames, (void *)k, (void *)v);
        //printf("IPC: inserted net name alias %s -> %s\n", k, v);
    }
}

static const char *
_register_netname(ipcd356a_state_t * ist)
{
    char alias[15];
    char * q;
    const char * k;
    const char * n;
    int i;
    for (i = 0, q=ist->p; i < 14 && !isspace(*q); ++i, ++q);
    strncpy(alias, ist->p, q-ist->p);
    alias[q-ist->p] = 0;
    // For now, treat "N/C" and blank the same, by returning an empty string.
    if (!strcmp(alias, "N/C") || !alias[0])
        return "";
    k = g_intern_string(alias);
    n = (const char *)g_hash_table_lookup(ist->netnames, k);
    if (n)
        return n;
    // First time seen.  Create new with key==value.
    g_hash_table_insert(ist->netnames, (void *)k, (void *)k);
    //printf("IPC: inserted net name %s\n", k);
    return k;
}


static int
_fixed_int(ipcd356a_state_t * ist, int col, int len)
{
    int v, i;
    const char * p;
    
    for (p = ist->line + col, i = 0, v = 0; i < len; ++i, ++p) 
        if (isdigit(*p))
            v = v*10 + (*p - '0');
    return v;
}

static int
_signed_fixed_int(ipcd356a_state_t * ist, int col, int len)
{
    return _fixed_int(ist, col+1, len-1) * (ist->line[col] == '-' ? -1 : 1);
}

static void 
_create_aperture_key(ipcd356a_state_t * ist, int rectype)
{
    /*
        Unique aperture key is derived from record e.g.:
        3273V3              U5    -8         PA01X 078445Y 063140X0350Y1650R090 S0
        gives
         2                                    A01                X0350Y1650R090 S0
        condensed to
         2A1S0X350Y1650R90

        317NNAME1           P4    -5    D1200PA00X 075640Y 075380X0000          S0
        gives
         1                              D1200PA00                X0000          S0
        condensed to
         1{C|V}D1200PA0S0X0
        where C for component, V for via.
        
        Dimensions are stored in 1/10,000ths inch, rounded to int using rint(3).
        
        We encode the rotation as integer degrees [0..180).  IPC supports 0..359,
        but for rendering 180 multiple rotations are insignificant.  Caller is
        responsible for the mod 180 in ist->rot.
        
        For rectype==389, this is a board outline etc. Create aperture key as
        8BX<d>Y<d> where <d> is the aperture size(s) in "tenths".  Possible types are
           B   BOARD_EDGE
           P   PANEL_EDGE
           S   SCORE_LINE
           O   OTHER_FAB
    */
    if (rectype == 327) {
        sprintf(ist->aperkey,
                "2A%dS%dX%d",
                ist->access,
                ist->soldermask,
                (int)rint(ist->sizex*10000.));
                
    }
    else if (rectype == 317) {
        sprintf(ist->aperkey,
                "1%cD%d%cA%dS%dX%d",
                !strcmp(ist->refdes, "VIA") ? 'V' : 'C',
                (int)rint(ist->holediam*10000.),
                ist->plating,
                ist->access,
                ist->soldermask,
                (int)rint(ist->sizex*10000.));
    }
    else if (rectype == 389) {
        sprintf(ist->aperkey,
                "8%cX%d",
                ist->outline_type[0],
                (int)rint(ist->sizex*10000.));
    }
    else if (rectype == 378) {
        sprintf(ist->aperkey,
                "7X%d",
                (int)rint(ist->sizex*10000.));
    }
    
    if (ist->giveny)
        sprintf(ist->aperkey + strlen(ist->aperkey),
            "Y%d",
            (int)rint(ist->sizey*10000.));
    if (ist->rot)
        sprintf(ist->aperkey + strlen(ist->aperkey),
            "R%d",
            ist->rot);
}


static gerbv_aperture_t *
_register_aperture(ipcd356a_state_t * ist, int * aindex)
{
    const char * k = g_intern_string(ist->aperkey);
    gerbv_aperture_t ** app = (gerbv_aperture_t **)g_hash_table_lookup(ist->aperture_map, k);
    gerbv_aperture_t * a;
    char buf[10];
    
    if (app) {
        *aindex = app - ist->image->aperture;
        return *app;
    }
	a = (gerbv_aperture_t *) g_new0 (gerbv_aperture_t, 1);
    
    // Set this aperture according to the test record info we have parsed.
    a->unit = GERBV_UNIT_INCH;
    if (ist->ano == APERTURE_MAX) {
        --ist->ano;
        sprintf(ist->errmsg, _("IPC-D-356A: more than %d apertures required\n"), APERTURE_MAX);
    }
    *aindex = ist->ano;
    g_hash_table_insert(ist->aperture_map, (void *)k, (void *)(ist->image->aperture + ist->ano));
    ist->image->aperture[ist->ano++] = a;
    a->type = ist->giveny ? GERBV_APTYPE_RECTANGLE : GERBV_APTYPE_CIRCLE;
    a->nuf_parameters = 1;
    a->parameter[0] = ist->sizex;
    if (ist->givenrot && ist->rot != 90 && ist->rot != 0) {
        // Non-ortho.  Any rotation other than 0 or 90 requires
        // an aperture macro to draw the rotated pad.  For now, just replace these with 
        // a round aperture of the minimum dimension size, until someone has the energy to
        // do better.
        a->type = GERBV_APTYPE_CIRCLE;
        if (ist->giveny && ist->sizey < ist->sizex)
            a->parameter[0] = ist->sizey;
    }
    else if (a->type == GERBV_APTYPE_RECTANGLE) {
        // IPC treats all non-round features as rectangular.  In the case of oblong pads,
        // at least for Altium output, the semicircular ends are not included in the
        // rectangle.
        a->parameter[a->nuf_parameters++] = ist->sizey;
        if (ist->rot == 90) {
            // For 90 degrees rotation, swap x,y
            a->parameter[0] = ist->sizey;
            a->parameter[1] = ist->sizex;
        }
    }

    if (ist->holediam > 0.) {
        // Round holes only with IPC.  In the case that the feature size is less
        // than the holsize+25%, then make the feature size equal to the hole size+25%.
        // This will create a visible display when fast rendering options are used.
        // Extra 25% may not be correct, but provides at least a thin visual ring.
        gfloat min_annular = ist->holediam*1.25;
        a->parameter[a->nuf_parameters++] = ist->holediam;
        if (a->parameter[0] < min_annular)
            a->parameter[0] = min_annular;
        if (a->type == GERBV_APTYPE_RECTANGLE && a->parameter[1] < min_annular)
            a->parameter[1] = min_annular;
    }

    /*
        .AperFunction,
            SMDPad
            ComponentPad,{CuDef|SMDef}
            ViaPad
    If the aperture function cannot be determined, the attribute is not created. 
         IPCAccess,{n}   - access layer in IPC numbering (0,1,2...n)
         IPCPlating,{P|U}   - hole plating
         
    TODO: currently assume all copper-defined pads.  Shouldn't matter too much
    since don't really care about that detail.
    */	
    x2attr_set_aperture_attr(a, ".AperFunction",
                        ist->aperkey[0]=='2' ? "SMDPad" :
                        ist->aperkey[1]=='V' ? "ViaPad" :
                        ist->aperkey[0]=='8' ? 
                            (ist->aperkey[1]=='B' ? "Profile" :
                             ist->aperkey[1]=='P' ? "Other,PanelEdge" :
                             ist->aperkey[1]=='S' ? "Other,ScoringLine" :
                                                    "Other,OtherFab"
                            ) :
                        ist->aperkey[0]=='7' ? "Conductor" :
                        "ComponentPad,CuDef");
    if (ist->aperkey[0]=='1' || ist->aperkey[0]=='2' || ist->aperkey[0]=='7') {
        sprintf(buf, "%d", ist->access);
        x2attr_set_aperture_attr(a, "IPCAccess", buf);
        if (ist->plating != ' ' && ist->aperkey[0]!='7') {
            sprintf(buf, "%c", ist->plating);
            x2attr_set_aperture_attr(a, "IPCPlating", buf);
        }
    }
	return a;
}

static void 
_parse_test_point(ipcd356a_state_t * ist, gboolean smd)
{
    gerbv_aperture_t * aper;
    const char * netname = _register_netname(ist);  // The real netname (interned)
    int aindex = 0;
	gerbv_net_t * net;
	char buf[100];
    //gerbv_render_size_t boundingBoxNew = {HUGE_VAL,-HUGE_VAL,HUGE_VAL,-HUGE_VAL};   // no size
    //gerbv_render_size_t boundingBoxNew = {-HUGE_VAL,HUGE_VAL,-HUGE_VAL,HUGE_VAL};   // infinite size
    
    // -1 is to adjust from column numbers in the doc, to 0-based C arrays.
    strncpy(ist->refdes, ist->line + 21-1, sizeof(ist->refdes)-1);
    strncpy(ist->pin, ist->line + 28-1, sizeof(ist->pin)-1);
    g_strchomp(ist->refdes);
    g_strchomp(ist->pin);
    ist->midnet = ist->line[32-1];
    ist->holediam = 0.;
    ist->plating = ' ';
    if (!smd && ist->line[33-1] == 'D') {
        ist->holediam = ist->toinch * _fixed_int(ist, 34-1, 4);
        ist->plating = ist->line[38-1];
    }
    ist->access = 0;
    if (ist->line[39-1] == 'A')
        ist->access = _fixed_int(ist, 40-1, 2);
    if (!(ist->layers & 1uL<<(ist->access ? ist->access : 1)))
        // Not looking for this layer
        return;
    ist->locx = ist->toinch * _signed_fixed_int(ist, 43-1, 7);
    ist->locy = ist->toinch * _signed_fixed_int(ist, 51-1, 7);
    // Altium 14 (at least) generates sizex=0 for thru hole, so only holediam shows up.
    ist->sizex = 0.;
    if (ist->line[58-1] == 'X')
        ist->sizex = ist->toinch * _fixed_int(ist, 59-1, 4);
    ist->sizey = ist->sizex;
    ist->giveny = FALSE;
    if (ist->line[63-1] == 'Y') {
        ist->sizey = ist->toinch * _fixed_int(ist, 64-1, 4);
        ist->giveny = TRUE;
    }
    ist->rot = 0;
    ist->givenrot = FALSE;
    if (ist->line[68-1] == 'R') {
        // Just in case radians used, do the conversion.  Also, modulo 180 because IPC is
        // very simple with shapes: features are either round (no angle is relevant) or
        // rectangular i.e. bilaterally symmetrical.  This slightly reduces the number of
        // apertures required for rendering.
        ist->rot = (int)rint(ist->todeg * _fixed_int(ist, 69-1, 3)) % 180;
        ist->givenrot = TRUE;
    }
    ist->soldermask = 0;
    if (ist->line[73-1] == 'S')
        ist->soldermask = _fixed_int(ist, 74-1, 1);
        
    // Now that we have the basic data, create annotated drawing elements.
    // We'll need an aperture, so create the aperture key and find existing,
    // or create new.
    _create_aperture_key(ist, smd ? 327 : 317);
    aper = _register_aperture(ist, &aindex);
    
    // Append a graphical object.
    net = g_new0 (gerbv_net_t, 1);
    ist->net->next = net;
    ist->net = net;
    
    //TODO: Currently we are basically ignoring the access (layer)
    // except we generate unique apertures.  Need to also define a drill layer
    // or more if blind/buried holes etc.
    net->layer = ist->layer;    // Nothing apart from the default layer state.
    net->state = ist->netstate;
    
    // Fill it in.  All flashes, so start==stop.
    net->start_x = ist->locx;
    net->start_y = ist->locy;
    net->stop_x = ist->locx;
    net->stop_y = ist->locy;
    net->aperture = aindex;
    net->aperture_state = GERBV_APERTURE_STATE_FLASH;

    net->boundingBox.left = ist->locx - aper->parameter[0]*0.5;
    net->boundingBox.right = ist->locx + aper->parameter[0]*0.5;
    net->boundingBox.bottom = ist->locy - aper->parameter[aper->type == GERBV_APTYPE_RECTANGLE ? 1 : 0]*0.5;
    net->boundingBox.top = ist->locy + aper->parameter[aper->type == GERBV_APTYPE_RECTANGLE ? 1 : 0]*0.5;
    gerber_update_image_min_max(&net->boundingBox, 0., 0., ist->image);
    /*
    Flashed objects will have object attributes:
      .N,netname
      .P,refdes,pin
      .C,refdes
    But attrib not created if not applicable e.g. no .C if refdes is 'VIA'.
    Drill diameter, access etc. is obtained from the aperture, since a unique aperture is
    generated for each distinct set of such attributes.
    */
    sprintf(buf, "%d", ist->access);
    x2attr_set_net_attr(net, "IPCLayer", buf);
    if (netname && *netname && strcmp(netname, "N/C"))
        x2attr_set_net_attr(net, ".N", netname);
    if (ist->refdes[0] && strcmp(ist->refdes, "NOREF") && strcmp(ist->refdes, "VIA")) {
        x2attr_set_net_attr(net, ".C", ist->refdes);
        if (ist->pin[0] && strcmp(ist->pin, "NPIN")) {
            sprintf(buf, "%s,%s", ist->refdes, ist->pin);
            x2attr_set_net_attr(net, ".P", buf);
        }
    }
}



static void
_parse_thru_hole(ipcd356a_state_t * ist)
{
    _parse_test_point(ist, FALSE);
    
}

static void
_parse_surface_mount(ipcd356a_state_t * ist)
{
    _parse_test_point(ist, TRUE);
}


static gboolean
_read_x(ipcd356a_state_t * ist, gfloat * xp)
{
    int v = 0;
    ist->givenx = FALSE;
    if (*ist->p != 'X')
        return FALSE;
    while (isdigit(*++ist->p))
        v = v*10 + (*ist->p - '0');
    *xp = ist->toinch * v;
    ist->givenx = TRUE;
    return TRUE;
}

static gboolean
_read_y(ipcd356a_state_t * ist, gfloat * yp)
{
    int v = 0;
    ist->giveny = FALSE;
    if (*ist->p != 'Y')
        return FALSE;
    while (isdigit(*++ist->p))
        v = v*10 + (*ist->p - '0');
    *yp = ist->toinch * v;
    ist->giveny = TRUE;
    return TRUE;
}


static gboolean
_parse_conductor_path(ipcd356a_state_t * ist, int rectype, const char * netname)
{
	gerbv_net_t * net;
	char buf[100];

    while (isspace(*ist->p)) ++ist->p;
        
    // ist->p points to current parsing position (not space).  Read coordinate data using state machine.
    do {
        switch (ist->drawstate) {
        case DS_SIZE:
            // X must be given, Y may be givenin that order.
            if (!_read_x(ist, &ist->sizex))
                return FALSE;
            if (!_read_y(ist, &ist->sizey))
                ist->sizey = ist->sizex;
            _create_aperture_key(ist, rectype);
            _register_aperture(ist, &ist->outline_aper_index);
            ist->drawstate = DS_MOVE;
            break;
        case DS_MOVE:
            // X,Y must be given, in that order.  Setting start location.
            if (!_read_x(ist, &ist->locx) ||
                !_read_y(ist, &ist->locy))
                return FALSE;
            if (*ist->p != '*')
                ist->drawstate = DS_LINE;
            break;
        default: // DS_LINE
            // X and/or Y may be given (but at least one).  Draw line from loc to here, then set loc.
            // Note use of bitwise and rather than &&, since do not want to use shortcut evaluation!
            if (!_read_x(ist, &ist->drawx) &
                !_read_y(ist, &ist->drawy))
                return FALSE;
            // Implement modality
            if (!ist->givenx)
                ist->drawx = ist->locx;
            if (!ist->giveny)
                ist->drawy = ist->locy;
            // Create line object
            net = g_new0 (gerbv_net_t, 1);
            ist->net->next = net;
            ist->net = net;
            
            // Outlines don't get a specific access (copper layer) since they're not really test points.
            net->layer = ist->layer;    // Nothing apart from the default layer state.
            net->state = ist->netstate;
            
            // Fill it in.  All flashes, so start==stop.
            net->start_x = ist->locx;
            net->start_y = ist->locy;
            net->stop_x = ist->drawx;
            net->stop_y = ist->drawy;
            net->aperture = ist->outline_aper_index;
            net->aperture_state = GERBV_APERTURE_STATE_ON;

            net->boundingBox.left = MIN(ist->locx, ist->drawx) - ist->sizex*0.5;
            net->boundingBox.right = MAX(ist->locx, ist->drawx) + ist->sizex*0.5;
            net->boundingBox.bottom = MIN(ist->locy, ist->drawy) - ist->sizey*0.5;
            net->boundingBox.top = MAX(ist->locy, ist->drawy) + ist->sizey*0.5;
            gerber_update_image_min_max(&net->boundingBox, 0., 0., ist->image);
            
            if (ist->access) {
                sprintf(buf, "%d", ist->access);
                x2attr_set_net_attr(net, "IPCLayer", buf);
            }
            if (netname && *netname && strcmp(netname, "N/C"))
                x2attr_set_net_attr(net, ".N", netname);

            ist->locx = ist->drawx;
            ist->locy = ist->drawy;
            
            if (*ist->p == '*') {
                ist->drawstate = DS_MOVE;
                ++ist->p;
                // else it should be a space or EOL, so continue in LINE mode.
            }
            break;
        }
        
        while (isspace(*ist->p)) ++ist->p;

    } while (*ist->p);
    
    return TRUE;
}

static gboolean
_parse_conductor(ipcd356a_state_t * ist, gboolean start)
{
    // Parse 378 record (if start) or 078 continuation record.
    if (start) {
        if (ist->linelen < 24)
            return FALSE;
        ist->netname = _register_netname(ist);
        // This is the layer number (use access field)
        ist->access = 0;
        if (ist->line[19-1] == 'L')
            ist->access = _fixed_int(ist, 20-1, 2);
            
        if (!(ist->layers & 1uL << ist->access))
            return FALSE;   // Not asking for this layer

        ist->drawstate = DS_SIZE;
        ist->rot = 0;   // no aperture rotations
        ist->holediam = 0;  // No "drills".
        ist->p = ist->line + 22;
    }

    return _parse_conductor_path(ist, 378, ist->netname);
}

static gboolean
_parse_outline(ipcd356a_state_t * ist, gboolean start)
{

    // Parse 389 record (if start) or 089 continuation record.
    if (start) {
        if (ist->linelen < 24)
            return FALSE;
        strncpy(ist->outline_type, ist->line + 4-1, sizeof(ist->outline_type)-1);
        g_strchomp(ist->outline_type);
        if (strcmp(ist->outline_type, "BOARD_EDGE")
            && strcmp(ist->outline_type, "PANEL_EDGE")
            && strcmp(ist->outline_type, "SCORE_LINE")
            && strcmp(ist->outline_type, "OTHER_FAB"))
            // Not recognized
            return FALSE;
        ist->drawstate = DS_SIZE;
        ist->rot = 0;   // no aperture rotations
        ist->holediam = 0;  // No "drills".
        _advance_p(ist);
    }

    return _parse_conductor_path(ist, 389, NULL);
}



gerbv_image_t *
ipcd356a_parse(gerb_file_t *fd, unsigned long layers, gboolean include_tracks)
{
    gerbv_image_t *image = NULL;
    char buf[MAXL];
    ipcd356a_state_t * ist;
    int i;
    char errmsg[1025];
    gboolean accum_389 = FALSE;
    gboolean accum_378 = FALSE;
    
    ist = g_new0(ipcd356a_state_t, 1);
    if (ist == NULL)
        GERB_FATAL_ERROR("malloc ipcd356a_state_t failed in %s()", __FUNCTION__);

    /* 
     * Create new image.  This will be returned.
     */
    image = gerbv_create_image(image, "IPC-D-356A File");
    if (image == NULL)
        GERB_FATAL_ERROR("malloc image failed in %s()", __FUNCTION__);
    image->layertype = GERBV_LAYERTYPE_IPCD356A;
    image->format = g_new0 (gerbv_format_t, 1); // just to avoid complaints
    ist->layers = layers;
    ist->include_tracks = include_tracks;
    ist->image = image;
    ist->net = image->netlist;

    ist->linenum = 0;
    ist->toinch = 1./25400.;    // Assume um default resolution.
    ist->todeg = 1.;            // Assume degree default resolution.
    ist->errmsg = errmsg;
    errmsg[0] = 0;              // Initially, no error.
    ist->imageskip = FALSE;
    ist->netnames = g_hash_table_new(g_direct_hash, g_direct_equal);
    ist->aperture_map = g_hash_table_new(g_direct_hash, g_direct_equal);
    ist->ano = 10;

    ist->layer = image->layers;
    ist->netstate = image->states;
    ist->net->layer = ist->layer;
    ist->net->state = ist->netstate;
    
    while ((ist->line = fgets(buf, MAXL, fd->fd))) {
        if (ist->errmsg[0])
            break;
        ++ist->linenum;
        g_strchomp(ist->line);   // Strip trailing ws
        ist->linelen = strlen(ist->line);
        if (ist->linelen < 3 || ist->line[0] == 'C' || ist->line[0] == 'c')
            // Short line or comment
            continue;
        if (ist->linelen > 80)
            // Someone is trying to hack us by buffer overflow.  Defeat them.
            continue;
        // Uppercase it...
        for (i = 0; i < ist->linelen; ++i)
            ist->line[i] = toupper(ist->line[i]);
        
        ist->p = ist->line + 3;
        if (!strncmp(ist->line, "P  ", 3)) {
            _parse_header(ist);
            continue;
        }
        if (ist->imageskip)
            continue;

        if (accum_389) {
            if (!strncmp(ist->line, "089", 3))
                accum_389 = _parse_outline(ist, FALSE);
            else
                accum_389 = FALSE;
        }
        else if (accum_378) {
            if (!strncmp(ist->line, "078", 3))
                accum_378 = _parse_conductor(ist, FALSE);
            else
                accum_378 = FALSE;
        }

        // For test records, min. line length should be 74, since we expect
        // up to the soldermask field.  Note that 017 and 027 records are
        // continuations of 317 and 327 resp.  They should have the same XY location,
        // but we don't check that and just process them all the same.
        //TODO: the continuation record grouping is really adding some connectivity
        // info, so we should make use of it.
        if ((!strncmp(ist->line, "317", 3) || !strncmp(ist->line, "017", 3)) && ist->linelen >= 74)
            _parse_thru_hole(ist);
        else if ((!strncmp(ist->line, "327", 3) || !strncmp(ist->line, "027", 3)) && ist->linelen >= 74)
            _parse_surface_mount(ist);
        else if (ist->layers & 1uL && !strncmp(ist->line, "389", 3))
            accum_389 = _parse_outline(ist, TRUE);
        else if (ist->include_tracks && !strncmp(ist->line, "378", 3))
            accum_378 = _parse_conductor(ist, TRUE);
    }
    
    g_free(ist->job);
    g_free(ist->title);
    g_free(ist->num);
    g_free(ist->rev);
    
    g_hash_table_destroy(ist->aperture_map);
    g_hash_table_destroy(ist->netnames);
    
    if (ist->errmsg)
        GERB_COMPILE_ERROR("%s", ist->errmsg);
    
    
    g_free(ist);
    
    x2attr_set_image_attr(image, ".FileFunction", "Other,IPC-D-356A");
    
    return image;
}

