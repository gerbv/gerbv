/* csv - read write comma separated value format
 * Copyright (c) 2003 Michael B. Allen <mba2000 ioplex.com>
 *
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/* We (Juergen Haas and Tomasz Motylewski) execute our rights given above
 * to distribute and sublicence this file (csv.h) and csv.c, csv_defines.h
 * under General Pulic Licence version 2 or any later version.
 *
 * This file is derived from libmba : A library of generic C modules
 * http://www.ioplex.com/~miallen/libmba/dl/libmba-0.8.9.tar.gz
 */

/** \file csv.h
    \brief Header info for the parsing support functions for the pick and place parser
    \ingroup libgerbv
*/

#ifndef CSV_H
#define CSV_H

/* csv - read write comma separated value format
 */

#ifdef __cplusplus
extern "C" {
#endif

# define LIBMBA_API extern

#include <stdio.h>
/*#include "text.h"*/

#if defined(USE_WCHAR)
#define csv_row_parse csv_row_parse_wcs
#else
#define csv_row_parse csv_row_parse_str
#endif

#define CSV_TRIM   0x01
#define CSV_QUOTES 0x02

LIBMBA_API int csv_row_parse_str(const char *src, size_t sn, char *buf, size_t bn, char *row[], int rn, int sep, int flags);
LIBMBA_API int csv_row_parse_wcs(const wchar_t *src, size_t sn, wchar_t *buf, size_t bn, wchar_t *row[], int rn, int sep, int flags);
LIBMBA_API int csv_row_fread(FILE *in, char *buf, size_t bn, char *row[], int numcols, int sep, int flags);

#ifdef __cplusplus
}
#endif

#endif /* CSV_H */
