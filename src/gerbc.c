/*
 * gEDA - GNU Electronic Design Automation
 * gerbc.c
 * Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libguile.h>
#include <guile/gh.h> /* To be deprecated */

#include "gerber.h"
#include "scm_gerber.h"



void
internal_main(int argc, char *argv[])
{
	int read_opt;
	char *backend = NULL;
	FILE *fd;
	struct gerb_image *image;
	struct gerb_net *net;
	int i,j;
	SCM cur_out = scm_current_output_port();
	SCM scm_image;

	/*
	 * -b <backend> : execute gerb-<backend>.scm
	 * gerb-<backend>.scm shall have a function accepting one argument;
	 * the argument is a list with the parsed gerber file.
	 */

	while ((read_opt = getopt(argc, argv, "b:")) != -1) {
		switch (read_opt) {
		case 'b' :
			backend = (char *)malloc(strlen(optarg) + strlen("gerb-.scm"));
			strcpy(backend, "gerb-");
			strcat(backend, optarg);
			strcat(backend, ".scm");
			break;
		case '?' :
			err(1, "Illegal option : %c\n", optopt);
		}
	}
	
	if (argc - optind != 1)
		err(1, "Wrong number of parameters: %s [-b backend] <infile name>\n", argv[0]);

	fd = fopen(argv[optind], "r");
	if (fd == NULL) {
		perror("fopen");
		exit(1);
	}
	image = parse_gerb(fd);
	fclose(fd);

	scm_image = scm_image2scm(image);

	if (backend == NULL) {
		scm_display(scm_image, cur_out);
		printf("\n");
	} else {
		scm_primitive_load(scm_makfrom0str(backend));
		scm_apply(scm_eval(gh_symbol2scm("main")), scm_image, SCM_EOL);
	}
		
	free_gerb_image(image);

	exit(0);
}

int
main(int argc, char *argv[])
{

	gh_enter(argc, argv, internal_main);
	
	return 0;
}
