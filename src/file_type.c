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

/** \file file_type.c
    \brief Automatic file type detection implementation
    \ingroup libgerbv
*/

#define FILE_TYPE_C     

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

#include "file_type.h"


#define dprintf if(DEBUG) printf
//#define dprintf if(1) printf


/*************************************************************
**
**
** File type sniffer API.
**
**TODO: this should be moved to its own source file(s).
**************************************************************/

#define MAXL 256

file_type_t 
file_sniffer_guess_file_type(
                        const char * filename,
                        int num_possible_types,
                        file_type_t * file_types,
                        file_sniffer_likely_line_t * line_funcs,
                        file_type_t * best
                        )
{
    file_sniffer_t * fsdata, * f;
    int i;
    char          buf[MAXL+2];
    char * line, * basename;
    gerb_file_t * fd;
    unsigned long running = 0uL;
    file_type_t winner = FILE_TYPE_UNKNOWN;
    int score, linelen, linenum;
        
        
    if (num_possible_types <= 0 || num_possible_types > sizeof(running)*8) {
        GERB_COMPILE_ERROR(_("Trying to open \"%s\":\n"
        "Internal error - too few/many possible file types (%d)."),
                filename, num_possible_types);
        return FILE_TYPE_CANNOT_OPEN;
    }

    fd = gerb_fopen(filename);
    if (!fd) {
        GERB_COMPILE_ERROR(_("Trying to open \"%s\": %s"),
                filename, strerror(errno));
        return FILE_TYPE_CANNOT_OPEN;
    }

    
    fsdata = g_new0(file_sniffer_t, num_possible_types);
    g_assert(fsdata);
    
    basename = g_path_get_basename(filename);
    
    dprintf("file_sniffer_guess_file_type: guessing for %s\n", basename);
    
    for (i = 0, f = fsdata; i < num_possible_types; ++i, ++f) {
        f->filename = basename;
        f->ext = strchr(f->filename, '.');
        if (!f->ext || f->ext == f->filename)
            f->ext = "";
        else
            ++f->ext;
        f->type = file_types[i];
        running |= 1uL<<i;
    }

    // Run the competition.
    linenum = 0;
    do {
        line = fgets(buf, MAXL, fd->fd);    // NULL at EOF.
        ++linenum;
        if (line) {
            g_strchomp(line);   // Strip trailing ws
            linelen = strlen(line);
        }
        else
            linelen = -1;
        for (i = 0, f = fsdata; i < num_possible_types; ++i, ++f) {
            if (!(1uL << i & running))
                continue;
            f->line_len = linelen;
            f->line_num = linenum;
            
            // Call the oracle(s)...
            score = line_funcs[i](line, f);
            
            if (score >= FILE_DEFINITELY_IS) {
                winner = file_types[i];
                dprintf("file_sniffer_guess_file_type: is %s (line %d)\n", file_type_as_string(winner), f->line_num);
                break;
            }
            else if (score <= FILE_DEFINITELY_NOT) {
                running &= ~(1uL<<i);
                dprintf("file_sniffer_guess_file_type: is not %s (line %d)\n", file_type_as_string(winner), f->line_num);
            }
            else {
                f->running_score += score;
                if (score >= FILE_DEFINITELY_IS) {
                    winner = file_types[i];
                    dprintf("file_sniffer_guess_file_type: is %s (line %d)\n", file_type_as_string(winner), f->line_num);
                    break;
                }
                else if (score <= FILE_DEFINITELY_NOT) {
                    running &= ~(1uL<<i);
                    dprintf("file_sniffer_guess_file_type: is not %s (line %d)\n", file_type_as_string(winner), f->line_num);
                }
            }
        }
        // Break out if nothing running or reached EOF or more than FILE_THRESHOLD lines.
    } while (running && line && linenum < FILE_THRESHOLD);

    // If no type reached the threshold, then the file type is FILE_TYPE_UNKNOWN.
    // If the 'best' param is not NULL, set it to the best match.
    if (best)
        *best = winner;
    if (best && winner == FILE_TYPE_UNKNOWN) {
        score = 0;
        for (i = 0, f = fsdata; i < num_possible_types; ++i, ++f) {
            if (!(1uL << i & running))
                continue;
            if (f->running_score <= 0) {
                dprintf("file_sniffer_guess_file_type: score non-positive (%d) for type %s\n", 
                        f->running_score, file_type_as_string(f->type));
                continue;
            }
            dprintf("file_sniffer_guess_file_type: score %d for type %s\n", 
                    f->running_score, file_type_as_string(f->type));
            if (f->running_score > score) {
                score = f->running_score;
                *best = f->type;
            }
        }
    }
    
    
    // Cleanup
    g_free(basename);
    for (i = 0, f = fsdata; i < num_possible_types; ++i, ++f) {
        if (f->user_data)
            g_free(f->user_data);
    }
    g_free(fsdata);
    gerb_fclose(fd);

    dprintf("file_sniffer_guess_file_type: is %s\n", file_type_as_string(winner));
    
    return winner;
}


gboolean 
file_type_line_contains_binary(const char * line)
{
    while (*line++)
        if ((*line >= 1 && *line < 32 && !isspace(*line)) || *line==0x7F)
            return TRUE;
    return FALSE;
}

const char * 
file_type_as_string(file_type_t type)
{
    if (type == FILE_TYPE_CANNOT_OPEN)
        return "<cannot open>";
    if (type >= FILE_TYPE_UNKNOWN && type < _FILE_TYPE_LAST)
        return file_type_strings[(int)type];
    return "<invalid type>";
}



