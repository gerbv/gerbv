/*------------------------------------------------------------------------------
	Filename: example2.c
	
	Description: Loads example2-input.gbx, duplicates it and offsets it to the
		right by the width of the layer, merges the two images, and exports
		the merged image back to another RS274X file. Note: this example
		code uses the gerbv_image_t interface as opposed to the
		gerb_project_t interface.

	Instructions: Make sure you are in the example-code directory, and compile
		this program with the following command:

	gcc -Wall -g `pkg-config --cflags libgerbv` `pkg-config --libs libgerbv` example2.c -o example2

	Run with the following command:
	
	./example2
	
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
	gerbv_image_t *originalImage, *duplicatedImage;
	
	/* parse and create the first image (by default, the image will not be
	   offset and will be in its true position */
	originalImage = gerbv_create_rs274x_image_from_filename ("example2-input.gbx");
	
	/* make sure we parsed the file */
	if (originalImage == NULL)
		g_error ("There was an error parsing the file.");
	
	/* duplicate the image and place it into a new gerbv_image_t */
	duplicatedImage = gerbv_image_duplicate_image (originalImage, NULL);
	
	/* create a transormation for the second image, and offset it the width
	   of the first image in the x direction */
	gerbv_user_transformation_t newTransformation = {originalImage->info->max_x -
				originalImage->info->min_x,
				0, 0, 0, FALSE};

	/* merge the duplicated image onto the original image, but use the new
	   offset to move the new copy to the right */
	gerbv_image_copy_image (duplicatedImage, &newTransformation, originalImage);
	
	/* export the merged image to a new rs274x file */
	gerbv_export_rs274x_file_from_image ("example2-output.gbx",	originalImage, NULL);

	/* destroy all created structures */
	gerbv_destroy_image (originalImage);
	gerbv_destroy_image (duplicatedImage);
	return 0;
}
