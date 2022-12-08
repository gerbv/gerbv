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

//! Parse IPC-D-356A and construct an image with attributes.  This image can be displayed like a gerber or component layer,
//! and may also be used to set RS274-X2 attributes on a set of conductor layers.
gerbv_image_t * ipcd356a_parse(gerb_file_t *fd);


#ifdef __cplusplus
}
#endif

#endif /* IPCD356A_H */

