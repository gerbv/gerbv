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
#include <sys/mman.h>
#include <errno.h>

#include "gerb_file.h"

gerb_file_t *
gerb_fopen(char *filename)
{
    gerb_file_t *fd;
    struct stat statinfo;

    fd = (gerb_file_t *)malloc(sizeof(gerb_file_t));
    if (fd == NULL) {
	return NULL;
    }

    fd->fd = fopen(filename, "r");
    if (fd->fd == NULL) {
	return NULL;
    }

    fd->ptr = 0;
    fd->fileno = fileno(fd->fd);
    fstat(fd->fileno, &statinfo);
    if (!S_ISREG(statinfo.st_mode)) {
	errno = EISDIR;
	return NULL;
    }
    if ((int)statinfo.st_size == 0) {
	errno = EIO; /* More compatible with the world outside Linux */
	return NULL;
    }
    fd->datalen = (int)statinfo.st_size;
    fd->data = (char *)mmap(0, statinfo.st_size, PROT_READ, MAP_PRIVATE, 
			    fd->fileno, 0);
    if(fd->data == MAP_FAILED) {
	fclose(fd->fd);
	free(fd);
	fd = NULL;
    }

    return fd;
} /* gerb_fopen */


int
gerb_fgetc(gerb_file_t *fd)
{

    if (fd->ptr > fd->datalen || fd->datalen == 0)
	return EOF;

    return (int) fd->data[fd->ptr++];
} /* gerb_fgetc */


int
gerb_fgetint(gerb_file_t *fd)
{
    long int result;
    char *end;
    
    result = strtol(fd->data + fd->ptr, &end, 10);
    fd->ptr = end - fd->data;

    return (int)result;
} /* gerb_fgetint */


double
gerb_fgetdouble(gerb_file_t *fd)
{
    double result;
    char *end;
    
    result = strtod(fd->data + fd->ptr, &end);
    fd->ptr = end - fd->data;

    return result;
} /* gerb_fgetdouble */


char *
gerb_fgetstring(gerb_file_t *fd, char term)
{
    char *strend;
    char *newstr;
    int len;
    
    strend = strchr(fd->data + fd->ptr, term);
    if (strend == NULL)
	return NULL;

    len = strend - (fd->data + fd->ptr);

    newstr = (char *)malloc(len + 1);
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
    munmap(fd->data, fd->datalen);
    fclose(fd->fd);
    free(fd);
    
    return;
} /* gerb_fclose */


char *
gerb_find_file(char *filename, char **paths)
{
    char *curr_path = NULL;
    char *complete_path = NULL;
    int	 i;

    for (i = 0; paths[i] != NULL; i++) {

	/*
	 * Environment variables start with a $ sign 
	 */
	if (paths[i][0] == '$') {
	    char *env_name, *env_value, *tmp;
	    int len;

	    /* Extract environment name. Remember we start with a $ */
	    tmp = strchr(paths[i], '/');
	    if (tmp == NULL) 
		len = strlen(paths[i]) - 1;
	    else
		len = tmp - paths[i] - 1;
	    env_name = (char *)malloc(len);
	    strncpy(env_name, (char *)(paths[i] + 1), len);
	    env_name[len] = '\0';

	    env_value = getenv(env_name);
	    if (env_value == NULL) break;
	    curr_path = (char *)malloc(strlen(env_value) + strlen(&paths[i][len + 1]) + 1);
	    strcpy(curr_path, env_value);
	    strcat(curr_path, &paths[i][len + 1]);
	    free(env_name);
	} else {
	    curr_path = paths[i];
	}

	/*
	 * Build complete path (inc. filename) and check if file exists.
	 */
	complete_path = (char *)malloc(strlen(curr_path) + strlen(filename) + 2);
	strcpy(complete_path, curr_path);
	complete_path[strlen(curr_path)] = '/';
	complete_path[strlen(curr_path) + 1] = '\0';
	strncat(complete_path, filename, strlen(filename));

	if (access(complete_path, R_OK) != -1)
	    break;

	free(complete_path);
	complete_path = NULL;
    }

    return complete_path;
} /* gerb_find_file */
