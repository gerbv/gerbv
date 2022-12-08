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

/** \file ipcd356a.h
    \brief Header info for IPC-D-356A parsing and annotation
    \ingroup libgerbv
    
    For now, file_sniffer API is in here until someone decides to use
    this programming style for all the other file types.
    
    This library was tested using Altium 14.  To generate a suitable
    input file from Altium, use the following procedure (for example):
    
    * Open the PCB document
    * File -> Fabrication outputs -> Test Point Report
    * In the dialog that opens:
       ** check "IPC-D-356A"
       ** select "Reference to relative origin" or "Reference to
          absolute origin" to match the setting when generating
          Gerbers.  By default, this would be "relative".
       ** other options set as required.  Recommend selecting all
          test point layers.
       ** Press OK.
       
    "Reference to relative/absolute origin" is required to match 
    whatever the Gerbers are generated with, otherwise the images
    will not register and it will not be possible to match up
    the IPC information with the Gerber.  In Altium, the Gerber
    option setting is in the "Advanced" tab of the dialog.
*/

#ifndef IPCD356A_H
#define IPCD356A_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

#include "file_type.h"


/*
 * ipcd356a structs and enums
 */


/*
 * ipcd356a API
 */

//! Callback for file type sniffer.  See file_sniffer_likely_line_t.
int ipcd356a_likely_line(const char * line,         //!< Current line of file, trailing whitespace stripped, plus a nul.
                         file_sniffer_t * fsdata    //!< State data
                         ); 

/*! Parse IPC-D-356A and construct image(s) with attributes.  This image can be displayed like a gerber or component layer,
and may also be used to set RS274-X2 attributes on a set of conductor or drill layers read from actual Gerber files.

The returned image is displayable in the same way as normal RS274-X, however the details are limited by the
available information in this file format, which is designed for testing equipment.  For example, thru-hole pads
often have only their drill size and position defined, so will display as narrow annular rings.

THe 'layers' parameter is a bitmap indicating which layers of the board are of interest.  IPC-D-356A files number
layers in physical order, from layer 1 (top) to layer n (bottom).  For example, to process only the top layer
pass layers=0x02 (i.e. bit 1 set).  Bit 0, for "layer 0", means features which are unrelated to any copper layer,
such as board outline and scoring lines.  So passing layers=0x03 will return board outline and top pads.

IPC uses "layer 0" (really, access 00) to mean "access both sides".  Since it is not clear which layer number
is the bottom layer, such A00 features are included only if the top layer is wanted (layers bit 1 is set).  

Drill data is included if the drill hit passes through any of the specified layers.  Drill hits are annotated
via apertures with a hole size parameter.

Processing multiple layers is OK, but the image will only be useful for annotation (netnames, components etc.) since
all layers are generated in the one image, making the actual layers visually identical.  Source layer data is
still available via attribute IPCLayer which will have the layer number, or 0 for "both sides top and bottom".

\return a new gerbv_image_t *, or NULL on failure.  The caller owns the image.
*/
gerbv_image_t * ipcd356a_parse(gerb_file_t *fd,         //!< File to read (already opened and rewound)
                               unsigned long layers,    //!< Bitmap of layers to process.  See description.
                               gboolean include_tracks  //<! Include conductor (track) data if available.
                               );


#ifdef __cplusplus
}
#endif

#endif /* IPCD356A_H */

