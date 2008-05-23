/*------------------------------------------------------------------------------
	Filename: example4.c
	
	Description: Loads example4-input.gbx, searches through the file and
		removes any entities with a width less than 60mils, and re-exports
		the modified image to a new RS274X file.

	Instructions: Make sure you are in the example-code directory, and compile
		this program with the following command:

	gcc -Wall -g `pkg-config --cflags gtk+-2.0 glib-2.0 libgerbv` `pkg-config \
--libs gtk+-2.0 glib-2.0 libgerbv` example4.c -o example4

	Run with the following command:
	
	./example4

------------------------------------------------------------------------------*/

#include "gerbv.h"

int
main(int argc, char *argv[]) {
	gerbv_image_t *workingImage;
	gerbv_net_t *currentNet;

	/* parse and create the image */
	workingImage = gerbv_create_rs274x_image_from_filename ("example4-input.gbx");
	
	/* make sure we parsed the file */
	if (workingImage == NULL)
		g_error ("There was an error parsing the file.");
		
	/* run through all the nets in the layer */
	for (currentNet = workingImage->netlist; currentNet; currentNet = currentNet->next){	
		/* check if the net aperture is a circle and has diameter < 0.060 inches */
		if ((currentNet->aperture_state != GERBV_APERTURE_STATE_OFF) &&
				(workingImage->aperture[currentNet->aperture] != NULL) &&
				(workingImage->aperture[currentNet->aperture]->type == GERBV_APTYPE_CIRCLE) &&
				(workingImage->aperture[currentNet->aperture]->parameter[0] < 0.060)){
			/* we found a path which meets the criteria, so delete the net for
			   demostration purposes */
			gerbv_image_delete_net (currentNet);
		}
	}

	/* export the modified image to a new rs274x file */
	gerbv_export_rs274x_file_from_image ("example4-output.gbx", workingImage);

	/* destroy all created structures */
	gerbv_destroy_image (workingImage);
	return 0;
}
