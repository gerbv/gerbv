/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
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

/** \file search.h
    \brief Iterate through data structures, for searching and selection.
    \ingroup libgerbv

This API uses a similar algorithm to draw.c.  Instead of rendering the
image to a cairo surface etc., it invokes a callback with sufficient
data for the callback to identify features of interest, such as tracks and
pads, and the apertures from which they were generated.

This API was introduced to support annotation of image data with attributes
recovered from IPC-D-356A files and similar.  It should also be a much faster
way to perform intersections with the image, for handling mouse clicks and
hovers.

Basically, each gerbv_net_t type object is presented to the callback,
along with some API that can be called to find whether a world-coordinate
point lies within the object, or if not then its distance from the
object.

Objects which represent regions are first condensed into a single polygonal
object, which usually simplifies callback logic.

Unlike the cairo-based testing in draw.c (for selection), this API can
return more information, such as the distance from each object rather than
a simple boolean hit result.

There are some simplifications, however.  The following features may not
be implemented owing to very infrequent occurrence, deprecation in the
standard, or great implementation difficulty.

  - knockouts (deprecated)
  - drawing with non-round aperture (rare and not that important)
  - apertures with rectangular holes (deprecated) - uses X dimension.
  - apertures other than round, with holes.  Holes ignored.  Rare.
  - step and repeat (only look at the primary image).
  - polygonal apertures >8 sides treated like circles.
  - no boolean geometry i.e. subsequent drawing with reversed
    polarity does not cut holes in a tested shape, for the purpose
    of intersection testing.  (Properly defined regions with cutouts, 
    however, will work as expected since all data is available.)
    This also applies to aperture macros with exotic geometry.
    The elements in such macros are treated individually.
  - "Moire" macro treated as a circle with OD of largest ring,
    and ignores crosshairs.
  - Thermal macro treated as ring, ignoring cross traces.
  - Tracks painted with rectangular aperture - these are treated like
    a circular aperture with diameter = min(dx,dy).  Rarely used.


In general, callback "sees" the object shapes after they have been transformed to
canonical position.  This makes it easier to test for intersections.  The
scanner will automatically transform the search point or vector so that it
can be compared with the transformed objects.


*/

#ifndef SEARCH_H
#define SEARCH_H

#include "gerbv.h"

#define SEARCH_MAX_XFORM    5   // Max levels in search transform stack.

/** Context provided to search_callback_t.

This is used as a flag to tell the callback what sort of graphical object is
being processed.
*/
typedef enum {
    SEARCH_CIRCLE,          //!< Filled circle (disk) centered at (0,0) with given radius dx.
    SEARCH_RING,            //!< Ring (disk with hole) centered at (0,0) with given inner and outer radius dy,dx.
    SEARCH_RECTANGLE,       //!< Rectangle with lower left at 0,0, and given width and height in dx,dy.
    SEARCH_OBROUND,         //!< Oblong round flash - params as for track, since looks like short track.
    SEARCH_POLYGON,         //!< Arbitrary filled area assuming even/odd winding rule.
    SEARCH_TRACK,           /*!< Stroked straight track with rounded ends.  One end at (0,0),
                                 the other at (dx,0) with half width hlw. */
    SEARCH_POLY_TRACK,      //!< Stroked arbitrary polygonal path, with half width hlw and assumed round ends.  Not closed.
} search_context_t;

/** Polygon coordinate pair.
*/
typedef struct vertex
{
    gdouble x;
    gdouble y;
} vertex_t;

struct search_state;

/** Callback function signature for search callbacks.
*/
typedef void (*search_callback_t)(struct search_state * ss, search_context_t ctx);


/** State structure used by callbacks and iterators.
*/
typedef struct search_state
{
    search_callback_t   callback;   //!< Current callback
    gpointer            user_data;  //!< Current user data
    gerbv_image_t *     image;      //!< The image being searched
    gerbv_aperture_t *  aperture;   //!< The current aperture
    gerbv_net_t *       net;        //!< The current object.  For regions, is the first object in region.
    int                 stack;      //!< Current transform stack index.
    gboolean            clear[SEARCH_MAX_XFORM];      
                                    /*!< Whether drawing "clear", else normal drawing.
                                         This starts FALSE regardless of the overall file polarity,
                                         since usually soldermask openings etc. are the thing of interest. */
    cairo_matrix_t      transform[SEARCH_MAX_XFORM];
                                    //!< Yes, we're lazy and use the cairo library for transforms.  
    GArray *            polygon;    //!< An array of vertex_t.  Callback uses poly and len fields below rather than this.                         
    
    // Data that depends on callback context...
    gdouble             dx;         //!< Width or X dimension or outer radius of circle/ring
    gdouble             dy;         //!< Height or Y dimension or inner radius of ring
    gdouble             hlw;        //!< Half linewidth for stroke type context (track etc.)
    const vertex_t *    poly;       //!< Convenient pointer into above polygon data, for start of vertex data (track, polygon).
    int                 len;        //!< Number of vertex data (track, polygon).
    
} search_state_t;



/** Structure returned by standard searches.
*/
typedef struct search_result
{
    gerbv_net_t *       net;        //!< The object.  For regions, is the first object in region.
    gdouble             dist;       //!< Signed distance from point to object's border.  -ve if *inside* the object area.
} search_result_t;

/**
*/
gdouble search_distance_to_border(search_state_t * ss, search_context_t ctx, const vertex_t * vtx);
gdouble search_distance_to_border_no_transform(search_state_t * ss, search_context_t ctx, gdouble x, gdouble y);
search_state_t * search_create_search_state_for_image(gerbv_image_t * image);
void search_destroy_search_state(search_state_t * ss);
search_state_t * search_init_search_state_for_image(search_state_t * ss, gerbv_image_t * image);
search_state_t * search_image(search_state_t * ss, gerbv_image_t * image, search_callback_t cb, gpointer user_data);
GArray * search_image_for_closest_to_border(gerbv_image_t * image, gdouble boardx, gdouble boardy, int nclosest, gdouble not_over);

#endif /* SEARCH_H */

