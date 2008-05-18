/*------------------------------------------------------------------------------
	Filename: example1.c
	
	Description: Loads example1-input.gbx into a project, and then exports
		the layer back to another RS274X file.

	Instructions: Make sure you are in the example-code directory, and compile
		this program with the following command:

	gcc -Wall -g `pkg-config --cflags gtk+-2.0 glib-2.0 libgerbv` `pkg-config \
--libs gtk+-2.0 glib-2.0 libgerbv` example1.c -o example1

	Run with the following command:
	
	./example1

------------------------------------------------------------------------------*/

#include "gerbv.h"

int
main(int argc, char *argv[]) {
	/* create a top level libgerbv structure */
	gerbv_project_t *mainProject = gerbv_create_project();

	/* parse a Gerber file and store it in the gerbv_project_t struct */
	gerbv_open_layer_from_filename (mainProject, "example1-input.gbx");
	
	/* export the first (and only) image in the project, which will be the
	   one we just loaded */
	gerbv_export_rs274x_file_from_image ("example1-output.gbx",
			mainProject->file[0]->image);

	/* destroy the top-level structure and free all memory */
	gerbv_destroy_project (mainProject);
	return 0;
}
