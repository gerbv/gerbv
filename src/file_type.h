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

/** \file file_type.h
    \brief Header info for automatic file type detection
    \ingroup libgerbv
*/

#ifndef FILE_TYPE_H
#define FILE_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

typedef enum file_type {
    FILE_TYPE_CANNOT_OPEN = -1,
    FILE_TYPE_UNKNOWN,
    FILE_TYPE_RS274D,
    FILE_TYPE_RS274X,
    FILE_TYPE_RS274X2,
    FILE_TYPE_EXCELLON,
    FILE_TYPE_GERBV_PROJECT,
    FILE_TYPE_CSV,
    FILE_TYPE_IPCD356A,
    
    _FILE_TYPE_LAST
} file_type_t;

#ifdef FILE_TYPE_C
// Strings of the above (in this header for easier maintenance).
const char * file_type_strings[] =
{
    "Unknown",
    "RS274-D",
    "RS274-X",
    "RS274-X2",
    "Excellon",
    "gerbv project",
    "CSV",
    "IPC-D-356A"
};
#endif

typedef struct file_sniffer
{
    const char * filename;  // Basename of the file being read (i.e. without directory)
    const char * ext;       // Points to just the extension (past the dot, if any, and not 1st char, else zero length string).
    file_type_t     type;   // Type of file we're looking for.
    int     running_score;  // Score so far
    #define FILE_THRESHOLD       1000
    #define FILE_DEFINITELY_NOT  (-FILE_THRESHOLD)
    #define FILE_DEFINITELY_IS   FILE_THRESHOLD
    int     line_num;       // 1-based line number
    int     line_len;       // Length excluding any whitespace at end.  -1 at EOF.
    #define NUM_FILE_COUNTERS   10
    int     counters[NUM_FILE_COUNTERS]; // For general use by the *_likely_line() function. 
    gpointer user_data;     // Starts NULL, g_malloc() by *_likely_line() if necessary, g_free by library.
} file_sniffer_t;

/*! Given a line from the input file, compute likelihood that this is a line
belonging to the expected file type.  This is a callback definition.  A function
should be coded according to this prototype that recognizes a file of a
particular type (file_type_t enum).  Callbacks are invoked when
file_sniffer_guess_file_type() is called.

Returned values are added to a running total.  If the total
reaches FILE_THRESHOLD or more, then the file is assumed to be of the expected type and
file reading is terminated.  If it reaches -FILE_THRESHOLD, it is assumed not
the be the expected type and reading is terminated.

Since FILE_THRESHOLD is 1000, returning a value of 1 for each line that could be
valid in that file will assure 1000 lines will be read.  Note that the *_likely_line()
API for different file types may be being called at the same time, in which case the
first file to reach FILE_THRESHOLD will "win" and reading will halt.

If the input file is short and EOF is reached, this function will be called once more,
but with a NULL line parameter.  It has one final opportunity to assign a "score".

For any one file, the function will be called at most FILE_THRESHOLD times, so lines
past this cannot be examined.

\return FILE_DEFINITELY_NOT if this is definitely not an IPC-D-356A file.
        0 if completely inconclusive
        FILE_DEFINITELY_IS if it definitely is an IPC-D-356A file (barring any corruption farther on).
        An intermediate value according to the line's evidence quality, and how quickly to
        recognize the file.
*/
typedef int (*file_sniffer_likely_line_t)(const char * line, file_sniffer_t * fsdata);

/*! Determine the type of a given file, via competing callbacks.

\return The type of file that was detected, or FILE_TYPE_UNKNOWN if none,
or FILE_TYPE_CANNOT_OPEN if error (and message will be posted).
If there was no clear winner (no file type reached the threshold of likeliness),
then if the 'best' parameter is not NULL, it will be set to the best positive match found,
with ties resolved by *first* in the array order given.
'best' may also be FILE_TYPE_UNKNOWN if all types definitely failed.
*/

file_type_t file_sniffer_guess_file_type(
                        const char * filename,  //!< File name (will be opened for reading, closed on return)
                        int num_possible_types, //!< Sizes of the following arrays
                        file_type_t * file_types,   //!< Array of types we are expecting
                        file_sniffer_likely_line_t * line_funcs, //!< Corresponding array of sniffer callback function pointers.
                        file_type_t * best  //!< output param to store best matching type if no clear winner.
                        );


/*! Determine whether the null-terminated given ascii line contains
binary data as defined as char ordinals in the range 1-31 or 127, excluding whitespace.
UTF-8, if present, will be ignored since the ordinals are all >= 128.
*/
gboolean file_type_line_contains_binary(const char * line);

//! Map file_type_t enum to string
const char * file_type_as_string(file_type_t type);




#ifdef __cplusplus
}
#endif

#endif /* FILE_TYPE_H */

