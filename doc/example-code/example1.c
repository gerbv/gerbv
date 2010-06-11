/*------------------------------------------------------------------------------
	Filename: example1.c
	
	Description: Loads example1-input.gbx into a project, and then exports
		the layer back to another RS274X file.

	Instructions: Make sure you are in the example-code directory, and compile
		this program with the following command:

	gcc -Wall -g `pkg-config --cflags libgerbv` `pkg-config --libs libgerbv` example1.c -o example1

	Run with the following command:
	
	./example1
	
	Common compiling problems:
		1. If you are compiling gerbv from source, make sure you run
			"make install" before trying to compile this example. This
			ensures libgerbv is correctly installed and can be found.
		2. If you installed gerbv to "/usr/local" (the default), many
			distributions don't correctly point pkgconfig to this target.
			To fix this, add the following to your ~/.bashrc file:
			
			export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig/:/usr/lib/pkgconfig/ 
------------------------------------------------------------------------------*/

#include "gerbv.h"

int
main(int argc, char *argv[]) {
	/* create a top level libgerbv structure */
	gerbv_project_t *mainProject = gerbv_create_project();

	/* parse a Gerber file and store it in the gerbv_project_t struct */
	gerbv_open_layer_from_filename (mainProject, "example1-input.gbx");
	
	/* make sure we parsed the file */
	if (mainProject->file[0] == NULL)
		g_error ("There was an error parsing the file.");
		
	/* export the first (and only) image in the project, which will be the
	   one we just loaded */
	gerbv_export_rs274x_file_from_image ("example1-output.gbx",
			mainProject->file[0]->image, NULL);

	/* destroy the top-level structure and free all memory */
	gerbv_destroy_project (mainProject);
	return 0;
}
