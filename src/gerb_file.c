/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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

/** \file gerb_file.c
    \brief File parsing support functions
    \ingroup libgerbv
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <errno.h>
#include <glib/gstdio.h>

#include "common.h"
#include "gerbv.h"
#include "gerb_file.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#undef DPRINTF
#define DPRINTF(...) do { if (DEBUG) printf(__VA_ARGS__); } while (0)

gerb_file_t *
gerb_fopen(char const * filename)
{
    gerb_file_t *fd;
    struct stat statinfo;
    
    DPRINTF("---> Entering gerb_fopen, filename = %s\n", filename);

    fd = g_new(gerb_file_t, 1);
    if (fd == NULL) {
	return NULL;
    }

    DPRINTF("     Doing fopen\n");
    /* fopen() can't open files with non ASCII filenames on windows */
    fd->fd = g_fopen(filename, "rb");
    if (fd->fd == NULL) {
	int saved_errno = errno;
	g_free(fd);
	errno = saved_errno;
	return NULL;
    }

    DPRINTF("     Doing fstat\n");
    fd->ptr = 0;
    fd->fileno = fileno(fd->fd);
    if (fstat(fd->fileno, &statinfo) < 0) {
	int saved_errno = errno;
	fclose(fd->fd);
	g_free(fd);
	errno = saved_errno;
	return NULL;
    }

    DPRINTF("     Checking S_ISREG\n");
    if (!S_ISREG(statinfo.st_mode)) {
	fclose(fd->fd);
	g_free(fd);
	errno = EISDIR;
	return NULL;
    }

    DPRINTF("     Checking statinfo.st_size\n");
    if ((int)statinfo.st_size == 0) {
	fclose(fd->fd);
	g_free(fd);
	errno = EIO; /* More compatible with the world outside Linux */
	return NULL;
    }

#ifdef HAVE_SYS_MMAN_H

    DPRINTF("     Doing mmap\n");
    fd->datalen = (int)statinfo.st_size;
    fd->data = (char *)mmap(0, statinfo.st_size, PROT_READ, MAP_PRIVATE,
			    fd->fileno, 0);
    if(fd->data == MAP_FAILED) {
	int saved_errno = errno;
	fclose(fd->fd);
	g_free(fd);
	errno = saved_errno;
	return NULL;
    } else {
	/* Copy into a heap buffer with null terminator so strtol/strtod
	 * have a safe stopping point — mmap does not guarantee '\0'
	 * after the file content. */
	char *buf = (char *)g_malloc(fd->datalen + 1);
	if (buf == NULL) {
	    int saved_errno = errno;
	    munmap(fd->data, fd->datalen);
	    fclose(fd->fd);
	    g_free(fd);
	    errno = saved_errno;
	    return NULL;
	}
	memcpy(buf, fd->data, fd->datalen);
	buf[fd->datalen] = '\0';
	munmap(fd->data, fd->datalen);
	fd->data = buf;
    }

#else
    /* all systems without mmap, not only MINGW32 */

    DPRINTF("     Doing calloc\n");
    fd->datalen = (int)statinfo.st_size;
    fd->data = calloc(1, statinfo.st_size + 1);
    if (fd->data == NULL) {
	int saved_errno = errno;
        fclose(fd->fd);
        g_free(fd);
	errno = saved_errno;
        return NULL;
    }
    if (fread((void*)fd->data, 1, statinfo.st_size, fd->fd) != statinfo.st_size) {
	int saved_errno = errno;
        fclose(fd->fd);
	g_free(fd->data);
        g_free(fd);
	errno = saved_errno;
	return NULL;
    }
    rewind (fd->fd);

#endif

    DPRINTF("     Setting filename\n");
    fd->filename = g_strdup(filename);

    DPRINTF("<--- Leaving gerb_fopen\n");
    return fd;
} /* gerb_fopen */


int
gerb_fgetc(gerb_file_t *fd)
{

    if (fd->ptr >= fd->datalen)
	return EOF;

    return (int) fd->data[fd->ptr++];
} /* gerb_fgetc */


int
gerb_fgetint(gerb_file_t *fd, int *len)
{
    long int result;
    char *end;

    if (fd->ptr >= fd->datalen) {
	if (len)
	    *len = 0;
	return 0;
    }

    errno = 0;
    result = strtol(fd->data + fd->ptr, &end, 10);
    if (errno) {
	GERB_COMPILE_ERROR(_("Failed to read integer"));
	return 0;
    }

    if (len) {
	*len = end - (fd->data + fd->ptr);
    }

    fd->ptr = end - fd->data;

    if (len && (result < 0))
	*len -= 1;

    return (int)result;
} /* gerb_fgetint */


double
gerb_fgetdouble(gerb_file_t *fd)
{
    char *start;
    int remaining;
    double result;
    char *end;

    if (fd->ptr >= fd->datalen)
	return 0.0;

    start = fd->data + fd->ptr;
    remaining = fd->datalen - fd->ptr;

    /* Prevent strtod from consuming hex float notation (0x.../0X...).
     * In Gerber aperture macros, x/X is the multiplication operator,
     * so "0X25.4" must parse as "0" followed by "X25.4", not as a
     * hexadecimal floating-point literal. */
    if (remaining >= 2
	    && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
	fd->ptr += 1;
	return 0.0;
    }

    errno = 0;
    result = strtod(start, &end);
    if (errno) {
	GERB_COMPILE_ERROR(_("Failed to read double"));
	return 0.0;
    }

    fd->ptr = end - fd->data;

    return result;
} /* gerb_fgetdouble */


char *
gerb_fgetstring(gerb_file_t *fd, char term)
{
    char *strend = NULL;
    char *newstr;
    char *i, *iend;
    int len;

    iend = fd->data + fd->datalen;
    for (i = fd->data + fd->ptr; i < iend; i++) {
	if (*i == term) {
	    strend = i;
	    break;
	}
    }

    if (strend == NULL)
	return NULL;

    len = strend - (fd->data + fd->ptr);

    newstr = (char *)g_malloc(len + 1);
    if (newstr == NULL)
	return NULL;
    strncpy(newstr, fd->data + fd->ptr, len);
    newstr[len] = '\0';
    fd->ptr += len;

    return newstr;
} /* gerb_fgetstring */


void 
gerb_ungetc(gerb_file_t *fd)
{
    if (fd->ptr)
	fd->ptr--;

    return;
} /* gerb_ungetc */


void
gerb_fclose(gerb_file_t *fd)
{
    if (fd) {
        g_free(fd->filename);

	/* fd->data is always heap-allocated: the mmap path now copies
	 * into a g_malloc'd buffer (for null termination) before
	 * munmap, and the non-mmap path uses calloc. */
	g_free(fd->data);
	if (fclose(fd->fd) == EOF)
	    GERB_FATAL_ERROR("fclose: %s", strerror(errno));
	g_free(fd);
    }

    return;
} /* gerb_fclose */


char *
gerb_find_file(char const * filename, char **paths)
{
    char *curr_path = NULL;
    char *complete_path = NULL;
    int	 i;

#ifdef DEBUG
    if( DEBUG > 0 ) {
        for (i = 0; paths[i] != NULL; i++) {
            printf("%s():  paths[%d] = \"%s\"\n", __FUNCTION__, i, paths[i]);
        }
    }
#endif

    for (i = 0; paths[i] != NULL; i++) {
        DPRINTF("%s():  Try paths[%d] = \"%s\"\n", __FUNCTION__, i, paths[i]);

	/*
	 * Environment variables start with a $ sign 
	 */
	if (paths[i][0] == '$') {
	    char *env_name, *env_value, *tmp;
	    int len;

	    /* Extract environment name. Remember we start with a $ */
        
   	    tmp = strchr(paths[i], G_DIR_SEPARATOR);
	    if (tmp == NULL) 
		len = strlen(paths[i]) - 1;
	    else
		len = tmp - paths[i] - 1;
	    env_name = (char *)g_malloc(len + 1);
	    if (env_name == NULL)
		return NULL;
	    strncpy(env_name, (char *)(paths[i] + 1), len);
	    env_name[len] = '\0';

	    env_value = getenv(env_name);
            DPRINTF("%s():  Trying \"%s\" = \"%s\" from the environment\n",
                __FUNCTION__, env_name,
                env_value == NULL ? "(null)" : env_value);

	    if (env_value == NULL) {
	      curr_path = NULL;
	    } else {
	      curr_path = (char *)g_malloc(strlen(env_value) + strlen(&paths[i][len + 1]) + 1);
	      if (curr_path == NULL)
		return NULL;
	      strcpy(curr_path, env_value);
	      strcat(curr_path, &paths[i][len + 1]);
	      g_free(env_name);
	    }
	} else {
	    curr_path = paths[i];
	}

	if (curr_path != NULL) {
	  /*
	   * Build complete path (inc. filename) and check if file exists.
	   */
	  complete_path = g_build_filename(curr_path, filename, NULL);
	  if (complete_path == NULL)
	    return NULL;
	  
	  if (paths[i][0] == '$') {
	    g_free(curr_path);
	    curr_path = NULL;
	  }
	  
	  DPRINTF("%s():  Tring to access \"%s\"\n", __FUNCTION__,
		  complete_path);
	  
	  if (access(complete_path, R_OK) != -1)
	    break;
	  
	  g_free(complete_path);
	  complete_path = NULL;
	}
    }
	
    if (complete_path == NULL)
      errno = ENOENT;
    
    DPRINTF("%s():  returning complete_path = \"%s\"\n", __FUNCTION__,
	    complete_path == NULL ? "(null)" : complete_path);
    
    return complete_path;
} /* gerb_find_file */
