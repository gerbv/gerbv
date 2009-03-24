/*------------------------------------------------------------------------------
	Filename: example6.c
	
	Description: Demonstrate how to embed a libgerbv render window into a new
		application to create a custom viewer

	Instructions: Make sure you are in the example-code directory, and compile
		this program with the following command (assumes you are using a
		newer version of gtk which uses cairo):

	gcc -Wall -g `pkg-config --cflags libgerbv` `pkg-config --libs libgerbv` example6.c -o example6

	Run with the following command:
	
	./example6
	
	Common compiling problems:
		1. If you are compiling gerbv from source, make sure you run
			"make install" before trying to compile this example. This
			ensures libgerbv is correctly installed and can be found.
		2. If you installed gerbv to "/usr/local" (the default), many
			distributions don't correctly point pkgconfig to this target.
			To fix this, add the following to your ~/.bashrc file:
			
			export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig/:/usr/lib/pkgconfig/ 
------------------------------------------------------------------------------*/

/* gerbv.h pulls in all glib and gtk headers for you */
#include "gerbv.h"
#include <cairo.h>

/* this holds our rendering info like window size, scale factor, and translation */
gerbv_render_info_t screenRenderInfo;
/* this holds all our layers */
gerbv_project_t *mainProject;
/* store the drawing area widget globally to simplify the key event handling, to eliminate
   the need for an event box */
GtkWidget *drawingarea;

void
example_render_project_to_screen (GdkDrawable *drawable) {
	cairo_t *cr = gdk_cairo_create (drawable);
	
	/* this is by far the simplest method of rendering everything */
	gerbv_render_all_layers_to_cairo_target (mainProject, cr, &screenRenderInfo);

	/* if you know cairo well, feel free to incorporate your own method here,
	   but this method shows you one possible idea.  With it, you have more flexibilty over
	   the rendering
	int i;	
	// paint the background white before we draw anything
	cairo_set_source_rgba (cr, 1,1,1, 1);
	cairo_paint (cr);
	
	// step through all the files
	for(i = mainProject->max_files-1; i >= 0; i--) {
		if (mainProject->file[i]) {
			cairo_push_group (cr);
			gerbv_render_layer_to_cairo_target (cr, mainProject->file[i], &screenRenderInfo);
			cairo_pop_group_to_source (cr);
			cairo_paint_with_alpha (cr, 0.70);
		}
	} */
	cairo_destroy (cr);
}
	
/* this is called when the window size changes, and also during startup */    
gboolean
example_callbacks_drawingarea_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
	GdkDrawable *drawable = widget->window;
	
	/* figure out how large the window is, and then fit the rendered images inside
	   the specified window */
	gdk_drawable_get_size (drawable, &screenRenderInfo.displayWidth, &screenRenderInfo.displayHeight);
	gerbv_render_zoom_to_fit_display (mainProject, &screenRenderInfo);

	/* GTK should now automatically expose the window, so no need to do it manually */
	return TRUE;
}

/* this is called any time the window needs to redraw (another window moved in front of
    it, the window was un-minimized, etc) */
gboolean
example_callbacks_drawingarea_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	/* render all the layers */
	example_render_project_to_screen(widget->window);
	return TRUE;
}

/* do some simple translation based on the arrow keys and "Z" keys */
gboolean
example_callbacks_drawingarea_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	switch(event->keyval) {
		case GDK_Up:
			/* cairo renders positive Y as down, so keep the sign in mind */
			screenRenderInfo.lowerLeftY -= 0.1;
			break;
		case GDK_Down:
			screenRenderInfo.lowerLeftY += 0.1;
			break;
		case GDK_Left:
			screenRenderInfo.lowerLeftX += 0.1;
			break;
		case GDK_Right:
			screenRenderInfo.lowerLeftX -= 0.1;
			break;
		case GDK_z:
			/* notice the lower left corner doesn't move with this method...
			   to do a "true" zoom in, refer to render.c and see how Gerber Viewer
			   does it */
			screenRenderInfo.scaleFactorX += screenRenderInfo.scaleFactorX/3;
			screenRenderInfo.scaleFactorY += screenRenderInfo.scaleFactorY/3;
			break;
		case GDK_Z:
			screenRenderInfo.scaleFactorX -= screenRenderInfo.scaleFactorX/3;
			screenRenderInfo.scaleFactorY -= screenRenderInfo.scaleFactorY/3;
			break;
		default:
			break;
	}
	/* render everything again by forcing an expose event */
	GdkRectangle update_rect;
	
	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = screenRenderInfo.displayWidth;
	update_rect.height = screenRenderInfo.displayHeight;

	/* force the drawing area to have an expose_event, thus redrawing the window */
	gdk_window_invalidate_rect (drawingarea->window, &update_rect, FALSE);
	return TRUE;
}

void
example_create_GUI (void){
	GtkWidget *mainWindow;
	
	mainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size((GtkWindow *)mainWindow, 400, 400);
	
	gtk_window_set_title (GTK_WINDOW (mainWindow), "Example 6");

	/* a drawing area is the easiest way to make a custom cairo renderer */
	drawingarea = gtk_drawing_area_new();
	gtk_container_add (GTK_CONTAINER (mainWindow), drawingarea);

	/* hook up the signals we need to connect to */
	gtk_signal_connect(GTK_OBJECT(drawingarea), "expose_event",
		       GTK_SIGNAL_FUNC(example_callbacks_drawingarea_expose_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawingarea),"configure_event",
		       GTK_SIGNAL_FUNC(example_callbacks_drawingarea_configure_event), NULL);
	gtk_signal_connect(GTK_OBJECT(mainWindow), "key_press_event",
		       GTK_SIGNAL_FUNC(example_callbacks_drawingarea_key_press_event), NULL);
	gtk_signal_connect_after(GTK_OBJECT(mainWindow), "delete_event",
		       GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_widget_show_all (mainWindow);
}

int
main(int argc, char *argv[]) {
	/* create the top level libgerbv structure */
	mainProject = gerbv_create_project();
	
	/* make sure we change the render type to "cairo" instead of the GDK alternative */
	screenRenderInfo.renderType = GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY;
	
	/* parse 2 Gerber files */
	gerbv_open_layer_from_filename (mainProject, "example1-input.gbx");
	gerbv_open_layer_from_filename (mainProject, "example2-input.gbx");
	
	/* make sure we parsed the files */
	if ((mainProject->file[0] == NULL) || (mainProject->file[1] == NULL))
		g_error ("There was an error parsing the files.");
	
	/* start up the gtk engine and create our GUI */
	gtk_init (&argc, &argv);
	example_create_GUI ();
	
	/* start the main GUI loop...it will stay in this function call until you exit */
	gtk_main();
	
	/* destroy the project, which will in turn destroy all child images */
	gerbv_destroy_project (mainProject);
	return 0;
}
