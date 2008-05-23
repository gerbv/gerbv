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
 * to distribute and sublicence this file (csv.c) and csv.h, csv_defines.h
 * under General Pulic Licence version 2 or any later version.
 *
 * This file is derived from libmba : A library of generic C modules
 * http://www.ioplex.com/~miallen/libmba/dl/libmba-0.8.9.tar.gz
 */

/** \file csv.c
    \brief Parsing support functions for the pick and place parser
    \ingroup libgerbv
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <wchar.h>
#include <wctype.h>

#include "gerbv.h"
#include "csv.h"
#include "csv_defines.h"
#define ST_START     1
#define ST_COLLECT   2
#define ST_TAILSPACE 3
#define ST_END_QUOTE 4
#define istspace iswspace


struct sinput {
	FILE *in;
	const char *src;
	size_t sn;
	size_t count;
};


struct winput {
	const wchar_t *src;
	size_t sn;
	size_t count;
};


static int
snextch(struct sinput *in)
{
	int ch;

	if (in->in) {
		if ((ch = fgetc(in->in)) == EOF) {
			if (ferror(in->in)) {
				GERB_MESSAGE("errno:%d", errno);
				return -1;
			}
			return 0;
		}
	} else {
		if (in->sn == 0) {
			return 0;
		}
		ch = (unsigned char) *(in->src)++;
		in->sn--;
	}
	in->count++;

	return ch;
}/* snextch */


static int
wnextch(struct winput *in)
{
	int ch;

	if (in->sn == 0) {
		return 0;
	}
	ch = *(in->src)++;
	in->sn--;
	in->count++;

	return ch;
}/* wnextch */

static int
csv_parse_str(struct sinput *in, char *buf, size_t bn, char *row[], int rn, int sep, int flags)
{
	int trim, quotes, ch, state, r, j, t, inquotes;

	trim = flags & CSV_TRIM;
	quotes = flags & CSV_QUOTES;
	state = ST_START;
	inquotes = 0;
	ch = r = j = t = 0;

	memset(row, 0, sizeof(char *) * rn);

	while (rn && bn && (ch = snextch(in)) > 0) {
		switch (state) {
			case ST_START:
				if (ch != '\n' && ch != sep && isspace(ch)) {
					if (!trim) {
						buf[j++] = ch; bn--;
						t = j;
					}
					break;
				} else if (quotes && ch == '"') {
					j = t = 0;
					state = ST_COLLECT;
					inquotes = 1;
					break;
				}
				state = ST_COLLECT;
			case ST_COLLECT:
				if (inquotes) {
					if (ch == '"') {
						state = ST_END_QUOTE;
						break;
					}
				} else if (ch == sep || ch == '\n') {
					row[r++] = buf; rn--;
					buf[t] = '\0'; bn--;
					buf += t + 1;
					j = t = 0;

					state = ST_START;
					inquotes = 0;
					if (ch == '\n') {
						rn = 0;
					}
					break;
				} else if (quotes && ch == '"') {
					errno = EILSEQ;
					GERB_MESSAGE("%d: unexpected quote in element",errno);
					return -1;
				}
				buf[j++] = ch; bn--;
				if (!trim || isspace(ch) == 0) {
					t = j;
				}
				break;
			case ST_TAILSPACE:
			case ST_END_QUOTE:
				if (ch == sep || ch == '\n') {
					row[r++] = buf; rn--;
					buf[j] = '\0'; bn--;
					buf += j + 1;
					j = t =  0;
					state = ST_START;
					inquotes = 0;
					if (ch == '\n') {
						rn = 0;
					}
					break;
				} else if (quotes && ch == '"' && state != ST_TAILSPACE) {
					buf[j++] = '"';	bn--;		 /* nope, just an escaped quote */
					t = j;
					state = ST_COLLECT;
					break;
				} else if (isspace(ch)) {
					state = ST_TAILSPACE;
					break;
				}
				errno = EILSEQ;
				GERB_MESSAGE("%d: bad end quote in element", errno);
				return -1;
		}
	}
	if (ch <= 0) {
		/* treat EOF as EOL, so the last record is accepted even when
		   \n is not present. Some users parse strings, not lines */
		if(state == ST_TAILSPACE || state == ST_END_QUOTE
			|| (state == ST_COLLECT && ! inquotes)) {
			row[r++] = buf; rn--;
			buf[j] = '\0'; bn--;
			buf += j + 1;
			inquotes = 0;
			rn = 0;
		} else {
	//		AMSG("");
			return -1;
		}
	}
	if (bn == 0) {
		errno = E2BIG;
		GERB_MESSAGE("E2BIG %d ", errno);
		return -1;
	}
	if (rn) {
		if (inquotes) {
			errno = EILSEQ;
			GERB_MESSAGE("EILSEQ %d ", errno);
			return -1;
		}
		row[r] = buf;
		buf[t] = '\0';
	}
	// return error if we can't read the minimum number of fields
	if (r < 4) {
		return -1;
	}
	return in->count;
}/* csv_parse_str */


static int
csv_parse_wcs(struct winput *in, wchar_t *buf, size_t bn, wchar_t *row[], int rn, wint_t sep, int flags)
{
	int trim, quotes, state, r, j, t, inquotes;
	wint_t ch;

	trim = flags & CSV_TRIM;
	quotes = flags & CSV_QUOTES;
	state = ST_START;
	inquotes = 0;
	ch = r = j = t = 0;

	memset(row, 0, sizeof(wchar_t *) * rn);

	while (rn && bn && (ch = wnextch(in)) > 0) {
		switch (state) {
			case ST_START:
				if (ch != L'\n' && ch != sep && iswspace(ch)) {
					if (!trim) {
						buf[j++] = ch; bn--;
						t = j;
					}
					break;
				} else if (quotes && ch == L'"') {
					j = t = 0;
					state = ST_COLLECT;
					inquotes = 1;
					break;
				}
				state = ST_COLLECT;
			case ST_COLLECT:
				if (inquotes) {
					if (ch == L'"') {
						state = ST_END_QUOTE;
						break;
					}
				} else if (ch == sep || ch == L'\n') {
					row[r++] = buf; rn--;
					buf[t] = L'\0'; bn--;
					buf += t + 1;
					j = t = 0;
					state = ST_START;
					inquotes = 0;
					if (ch == L'\n') {
						rn = 0;
					}
					break;
				} else if (quotes && ch == L'"') {
					errno = EILSEQ;
					GERB_MESSAGE("%d: unexpected quote in element", errno);
					return -1;
				}
				buf[j++] = ch; bn--;
				if (!trim || iswspace(ch) == 0) {
					t = j;
				}
				break;
			case ST_TAILSPACE:
			case ST_END_QUOTE:
				if (ch == sep || ch == L'\n') {
					row[r++] = buf; rn--;
					buf[j] = L'\0'; bn--;
					buf += j + 1;
					j = t =  0;
					state = ST_START;
					inquotes = 0;
					if (ch == L'\n') {
						rn = 0;
					}
					break;
				} else if (quotes && ch == L'"' && state != ST_TAILSPACE) {
					buf[j++] = L'"'; bn--;		 /* nope, just an escaped quote */
					t = j;
					state = ST_COLLECT;
					break;
				} else if (iswspace(ch)) {
					state = ST_TAILSPACE;
					break;
				}
				errno = EILSEQ;
				GERB_MESSAGE("%d: bad end quote in element ", errno);
				return -1;
		}
	}
	if (ch <= 0) {
		/* treat EOF as EOL, so the last record is accepted even when
		   \n is not present. Some users parse strings, not lines */
		if(state == ST_TAILSPACE || state == ST_END_QUOTE
			|| (state == ST_COLLECT && ! inquotes)) {
			row[r++] = buf; rn--;
			buf[j] = L'\0'; bn--;
			buf += j + 1;
			inquotes = 0;
			rn = 0;
		} else {
	//		AMSG("");
			return -1;
		}
	}
	if (bn == 0) {
		errno = E2BIG;
	GERB_MESSAGE("%d", errno);
		return -1;
	}
	if (rn) {
		if (inquotes) {
			errno = EILSEQ;
		GERB_MESSAGE("%d", errno);
			return -1;
		}
		row[r] = buf;
		buf[t] = L'\0';
	}

	return in->count;
}/*csv_row_parse_wcs*/


int
csv_row_parse_wcs(const wchar_t *src, size_t sn, wchar_t *buf, size_t bn, wchar_t *row[], int rn, int sep, int trim)
{
	struct winput input;
	input.src = src;
	input.sn = sn;
	input.count = 0;
	return csv_parse_wcs(&input, buf, bn, row, rn, (wint_t)sep, trim);
}/*csv_row_parse_wcs*/


int
csv_row_parse_str(const char *src, size_t sn, char *buf, size_t bn, char *row[], int rn, int sep, int trim)
{
	struct sinput input;
	input.in = NULL;
	input.src = src;
	input.sn = sn;
	input.count = 0;
	return csv_parse_str(&input, buf, bn, row, rn, sep, trim);
}/*csv_row_parse_str*/


int
csv_row_fread(FILE *in, char *buf, size_t bn, char *row[], int numcols, int sep, int trim)
{
	struct sinput input;
	input.in = in;
	input.count = 0;
	return csv_parse_str(&input, buf, bn, row, numcols, sep, trim);
}/*csv_row_fread*/

