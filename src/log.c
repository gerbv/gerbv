/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
 *
 * $Id$
 *
 * Contributed by Dino Ghilardi <dino.ghilardi@ieee.org> 
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
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "log.h"
#include "color.h"
#include "gerbv_screen.h"
#include "setup.h"

/*  #define SAVE_LOGFILE */

static GtkWidget *create_LogWindow(void);
#ifdef SAVE_LOGFILE
static GtkWidget *create_LogFileSelection(void);
#endif

/* XXX PHONY PROTOTYPE MUST FIX */
GtkWidget *lookup_widget (GtkWidget * widget, const gchar * widget_name);

/* CallBack Functions prototypes 
 * (cut-n-pasted from Glade-generated file callbacks.h)
 */
static void clear_log_button_clicked(GtkButton *button, gpointer user_data);
static void LogWindow_destroy(GtkObject *object, gpointer user_data);
static void close_log_button_clicked(GtkButton *button, gpointer user_data);
#ifdef SAVE_LOGFILE
static void logfile_select_ok_button_clicked(GtkButton *button, gpointer user_data);
static void logfile_cancel_button_clicked(GtkButton *button, gpointer user_data);
static void save_log_button_clicked(GtkButton *button, gpointer user_data);
#endif

static GtkWidget*
create_LogWindow (void)
{
    GtkWidget *LogWindow;
    GtkWidget *vbox1;
    GtkWidget *scrolledwindow1;
    GtkWidget *logtxt;
    GtkWidget *hbox1;
    GtkWidget *clear_log_button;
#ifdef SAVE_LOGFILE
    GtkWidget *save_log_button;
#endif
    GtkWidget *close_log_button;
    
    LogWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_object_set_data (GTK_OBJECT (LogWindow), "LogWindow", LogWindow);
    gtk_window_set_title (GTK_WINDOW (LogWindow), "Log Messages");
    gtk_window_set_default_size(GTK_WINDOW(LogWindow), 600, 100);
    
    vbox1 = gtk_vbox_new (FALSE, 0);
    gtk_widget_ref (vbox1);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "vbox1", vbox1,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (vbox1);
    gtk_container_add (GTK_CONTAINER (LogWindow), vbox1);
    
    scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (scrolledwindow1);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "scrolledwindow1", 
			      scrolledwindow1,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (scrolledwindow1);
    gtk_box_pack_start (GTK_BOX (vbox1), scrolledwindow1, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), 
				    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    
    logtxt = gtk_text_new (NULL, NULL);
    gtk_widget_ref (logtxt);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "logtxt", logtxt,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (logtxt);
    gtk_container_add (GTK_CONTAINER (scrolledwindow1), logtxt);
    
    hbox1 = gtk_hbox_new (FALSE, 0);
    gtk_widget_ref (hbox1);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (hbox1);
    gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);
    gtk_widget_set_usize (hbox1, -2, 24);

    clear_log_button = gtk_button_new_with_label ("Clear log");
    gtk_widget_ref (clear_log_button);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "clear_log_button", 
			      clear_log_button,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (clear_log_button);
    gtk_box_pack_start (GTK_BOX (hbox1), clear_log_button, TRUE, TRUE, 0);
    gtk_widget_set_usize (clear_log_button, -2, 24);
#ifdef SAVE_LOGFILE
    save_log_button = gtk_button_new_with_label ("Save Log to File");
    gtk_widget_ref (save_log_button);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "save_log_button", 
			      save_log_button,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (save_log_button);
    gtk_box_pack_start (GTK_BOX (hbox1), save_log_button, TRUE, TRUE, 0);
#endif
    close_log_button = gtk_button_new_with_label ("Close");
    gtk_widget_ref (close_log_button);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "close_log_button", 
			      close_log_button,
			      (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (close_log_button);
    gtk_box_pack_start (GTK_BOX (hbox1), close_log_button, TRUE, TRUE, 0);
    gtk_widget_set_usize (close_log_button, -2, 24);
    
    gtk_signal_connect (GTK_OBJECT (LogWindow), "destroy",
			GTK_SIGNAL_FUNC (LogWindow_destroy),
			NULL);
    gtk_signal_connect (GTK_OBJECT (clear_log_button), "clicked",
			GTK_SIGNAL_FUNC (clear_log_button_clicked),
			NULL);
#ifdef SAVE_LOGFILE
    gtk_signal_connect (GTK_OBJECT (save_log_button), "clicked",
			GTK_SIGNAL_FUNC (save_log_button_clicked),
			NULL);
#endif
    gtk_signal_connect (GTK_OBJECT (close_log_button), "clicked",
			GTK_SIGNAL_FUNC (close_log_button_clicked),
			NULL);
    
    return LogWindow;
} /* create_LogWindow */


#ifdef SAVE_LOGFILE
static GtkWidget *
create_LogFileSelection(void)
{
    GtkWidget *LogFileSelection;
    GtkWidget *logfile_select_ok_button1;
    GtkWidget *logfile_cancel_button;
    
    LogFileSelection = gtk_file_selection_new("Select Log file to write");
    gtk_object_set_data(GTK_OBJECT(LogFileSelection), "LogFileSelection",
			LogFileSelection);
    gtk_container_set_border_width(GTK_CONTAINER(LogFileSelection), 10);
    
    logfile_select_ok_button1 =
	GTK_FILE_SELECTION(LogFileSelection)->ok_button;
    gtk_object_set_data(GTK_OBJECT(LogFileSelection),
			"logfile_select_ok_button1",
			logfile_select_ok_button1);
    gtk_widget_show(logfile_select_ok_button1);
    GTK_WIDGET_SET_FLAGS(logfile_select_ok_button1, GTK_CAN_DEFAULT);
    
    logfile_cancel_button =
	GTK_FILE_SELECTION(LogFileSelection)->cancel_button;
    gtk_object_set_data(GTK_OBJECT(LogFileSelection),
			"logfile_cancel_button", logfile_cancel_button);
    gtk_widget_show(logfile_cancel_button);
    GTK_WIDGET_SET_FLAGS(logfile_cancel_button, GTK_CAN_DEFAULT);
    
    gtk_signal_connect(GTK_OBJECT(logfile_select_ok_button1), "clicked",
		       GTK_SIGNAL_FUNC(logfile_select_ok_button_clicked),
		       NULL);
    gtk_signal_connect(GTK_OBJECT(logfile_cancel_button), "clicked",
		       GTK_SIGNAL_FUNC(logfile_cancel_button_clicked),
		       NULL);
    
    return LogFileSelection;
} /* create_LogFileSelection */
#endif


void
gerbv_console_log_handler(const gchar *log_domain,
			  GLogLevelFlags log_level,
			  const gchar *message, 
			  gpointer user_data)
{

    fprintf(stderr, "%s", message);
} /* gerbv_console_log_handler */


void
gerbv_gtk_log_handler(const gchar *log_domain,
		      GLogLevelFlags log_level,
		      const gchar *message, 
		      gpointer user_data)
{
    GtkWidget *LogText = NULL;
    GdkColor textfgcolor;
    
    
    /* If the log window does not exist, create it  */
    if(screen.win.log == NULL){
	screen.win.log = create_LogWindow();
    }
    gtk_widget_show(screen.win.log);
    LogText =(GtkWidget *)lookup_widget((GtkWidget *)screen.win.log, "logtxt");

    /*
     * Print text to logfile if we have demanded it.
     */
    if(setup.log.to_file) {	/* Append log message to file if required */
	FILE *fd = NULL;
	fd = fopen(setup.log.filename, "a");
	if(fd) {
	    switch(log_level & G_LOG_LEVEL_MASK) {
	    case G_LOG_LEVEL_ERROR:
		fprintf(fd, "ERROR    :");
		break;
	    case G_LOG_LEVEL_CRITICAL:
		fprintf(fd, "CRITICAL :");
		break;
	    case G_LOG_LEVEL_WARNING:
		fprintf(fd, "WARNING  :");
		break;
	    case G_LOG_LEVEL_MESSAGE:
		fprintf(fd, "MESSAGE  :");
		break;
	    case G_LOG_LEVEL_INFO:
		fprintf(fd, "INFO     :");
		break;
	    case G_LOG_LEVEL_DEBUG:
		fprintf(fd, "DEBUG    :");
		break;
	    default:
		fprintf(fd, "Unhandled[%d]:", (int)log_level);
		break;
	    }
	    fprintf(fd, "%s\n", message);
	    fclose(fd);
	} else {
	    /*could not open file... do something else??? 
	      P.S: do not send a warning using g_warning or whatever 
	      from here, or a recursion condition
	      will happen and the program will crash. */
	    
	    gdk_color_parse("red", &textfgcolor);
	    /* allocate text foreground color */
	    gdk_colormap_alloc_color(gdk_colormap_get_system(), &textfgcolor,
				     FALSE, TRUE);
	    gtk_text_insert(GTK_TEXT(LogText), NULL, &textfgcolor, NULL,
			    "log.c: Could not open log file for writing\n",
			    -1);
	    
	}
    }
    
    /* now select a different color for a different message. 
     * See rgb.txt for the color names definition 
     * (on my PC it is on /usr/X11R6/lib/X11/rgb.txt) */
    switch(log_level & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_ERROR:
	/* a message of this kind aborts the application calling abort() */
	gdk_color_parse("red", &textfgcolor);
	break;
    case G_LOG_LEVEL_CRITICAL:
	gdk_color_parse("red", &textfgcolor);
	break;
    case G_LOG_LEVEL_WARNING:
	gdk_color_parse("darkred", &textfgcolor);
	break;
    case G_LOG_LEVEL_MESSAGE:
	gdk_color_parse("darkblue", &textfgcolor);
	break;
    case G_LOG_LEVEL_INFO:
	gdk_color_parse("darkgreen", &textfgcolor);
	break;
    case G_LOG_LEVEL_DEBUG:
	gdk_color_parse("SaddleBrown", &textfgcolor);
	break;
    default:
	gdk_color_parse("black", &textfgcolor);
	break;
    }

    /*
     * Fatal aborts application. We will try to get the message out anyhow.
     */
    if (log_level & G_LOG_FLAG_FATAL)
	fprintf(stderr, "Fatal error : %s\n", message);

    /* allocate text foreground color */
    gdk_colormap_alloc_color(gdk_colormap_get_system(), &textfgcolor,
			     FALSE, TRUE);

    /* if needed can be used also a different background color for the text
     * (see gtk_text_insert documentation)
     * gtk_text_insert(GtkText *text, GdkFont *font, GdkColor *fore, 
     * GdkColor *back, const char *chars, gint length);
     */
    gtk_text_insert(GTK_TEXT(LogText), NULL, &textfgcolor, NULL, message, -1);
    
    gdk_colormap_free_colors(gdk_colormap_get_system(), &textfgcolor, 1);
}


static void
clear_log_button_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *LogText = NULL;
    LogText =(GtkWidget *)lookup_widget((GtkWidget *)screen.win.log, "logtxt");
    gtk_text_backward_delete(GTK_TEXT(LogText),
			     gtk_text_get_length(GTK_TEXT(LogText)));
} /* clear_log_button_clicked */


static void
LogWindow_destroy(GtkObject *object, gpointer user_data)
{
    gtk_widget_destroy(screen.win.log);
    screen.win.log = NULL;
} /* LogWindow_destroy */


static void
close_log_button_clicked(GtkButton *button, gpointer user_data)
{
    gtk_widget_hide(GTK_WIDGET(screen.win.log));
} /* close_log_button_clicked */


#ifdef SAVE_LOGFILE
static void
logfile_select_ok_button_clicked(GtkButton *button, gpointer user_data)
{
    GtkFileSelection *FileSelection = NULL;
    char *filename;

    FileSelection =
	(GtkFileSelection *)lookup_widget(GTK_WIDGET(button),
					  "LogFileSelection");

    filename = gtk_file_selection_get_filename(FileSelection);
    printf("Log filename:%s\n", filename);

    gtk_widget_destroy(GTK_WIDGET(FileSelection));
} /* logfile_select_ok_button_clicked */


static void
logfile_cancel_button_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *FileSelection = NULL;
    FileSelection = lookup_widget(GTK_WIDGET(button), "LogFileSelection");
    gtk_widget_destroy(FileSelection);
} /* logfile_cancel_button_clicked */


static void
save_log_button_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *FileSelection = NULL;
    FileSelection = create_LogFileSelection();
    gtk_widget_show(FileSelection);
} /* save_log_button_clicked */
#endif
