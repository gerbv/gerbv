/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2006 Stefan Petersen (spe at stacken.kth.se)
 *
 * $Id$
 *
 * Contributed by Dino Ghilardi <dino.ghilardi at ieee dot org> 
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
#include <errno.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "log.h"
#include "color.h"
#include "gerbv_screen.h"
#include "setup.h"


/*  #define SAVE_LOGFILE */

static GtkWidget *create_LogWindow(void);
#ifdef SAVE_LOGFILE
static GtkWidget *create_LogFileSelection(void);
#endif


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

/*
 * Text buffer widget
 */
static GtkWidget *textviewLog;

static GtkWidget*
create_LogWindow (void)
{
    GtkWidget *LogWindow;
    GtkWidget *vbox1;
    GtkWidget *scrolledwindow1;
    GtkWidget *hbox1;
    GtkWidget *clear_log_button;
#ifdef SAVE_LOGFILE
    GtkWidget *save_log_button;
#endif
    GtkWidget *close_log_button;
    
    LogWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (LogWindow), "Log Messages");
    gtk_window_set_default_size(GTK_WINDOW(LogWindow), 600, 200);
    
    vbox1 = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox1);
    gtk_container_add (GTK_CONTAINER (LogWindow), vbox1);
    
    scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (scrolledwindow1);
    gtk_box_pack_start (GTK_BOX (vbox1), scrolledwindow1, TRUE, TRUE, 0);
    
    textviewLog = gtk_text_view_new();
    gtk_widget_show(textviewLog);
    gtk_container_add(GTK_CONTAINER(scrolledwindow1), textviewLog);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textviewLog), FALSE);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(textviewLog), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textviewLog), GTK_WRAP_WORD);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textviewLog), FALSE);

    hbox1 = gtk_hbox_new (TRUE, 7);
    gtk_widget_ref (hbox1);
    gtk_object_set_data_full (GTK_OBJECT (LogWindow), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (hbox1);
    gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);
    gtk_widget_set_size_request (hbox1, -1, 28);

    clear_log_button = gtk_button_new_with_mnemonic("Clear log");
    gtk_widget_show (clear_log_button);
    gtk_box_pack_start (GTK_BOX (hbox1), clear_log_button, FALSE, TRUE, 0);

#ifdef SAVE_LOGFILE
    save_log_button = gtk_button_new_with_mnemonic("Save to log file");
    gtk_widget_show(save_log_button);
    gtk_box_pack_start(GTK_BOX (hbox1), save_log_button, FALSE, TRUE, 0);
    gtk_widget_set_size_request(save_log_button, -1, 27);
#endif

    close_log_button = gtk_button_new_with_mnemonic("close");
    gtk_widget_show (close_log_button);
    gtk_box_pack_start (GTK_BOX (hbox1), close_log_button, FALSE, TRUE, 0);
    g_signal_connect ((gpointer) clear_log_button, "clicked",
		      G_CALLBACK (clear_log_button_clicked), NULL);

#ifdef SAVE_LOGFILE
    g_signal_connect ((gpointer) save_log_button, "clicked",
		      G_CALLBACK (save_log_button_clicked), NULL);
#endif

    gtk_signal_connect (GTK_OBJECT (close_log_button), "clicked",
			GTK_SIGNAL_FUNC (close_log_button_clicked), NULL);

    gtk_signal_connect (GTK_OBJECT (LogWindow), "destroy",
			GTK_SIGNAL_FUNC (LogWindow_destroy),
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
    GtkTextBuffer *textbuffer = NULL;
    GtkTextIter iter;
    GtkTextTag *tag;
    GtkTextMark *StartMark = NULL, *StopMark = NULL;
    GtkTextIter StartIter, StopIter;
    
    /* If the log window does not exist, create it  */
    if(screen.win.log == NULL){
	screen.win.log = create_LogWindow();
    }
    gtk_widget_show(screen.win.log);
    textbuffer = gtk_text_view_get_buffer((GtkTextView*)textviewLog);

    /* create a mark for the end of the text. */
    gtk_text_buffer_get_end_iter(textbuffer, &iter);

    gtk_widget_show(screen.win.log);

    /*
     * Print text to logfile if we have demanded it.
     */
    if(setup.log.to_file) {	/* Append log message to file if required */
	FILE *fd = NULL;
	fd = fopen(setup.log.filename, "a");
	if(fd == NULL) {
	    fprintf(stderr, "Could not log to file %s:%s\n", 
		    setup.log.filename, strerror(errno));
	    exit(0);
	}
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
    }

    /* Change default color throughout the widget, to set a
       color different from the default. */
#if 0
    gdk_color_parse("green", &color);
    gtk_widget_modify_text(GTK_WIDGET(textviewLog), GTK_STATE_NORMAL, &color);
#endif

    /* get the current end position of the text (it will be the
       start of the new text. */
    StartMark = gtk_text_buffer_create_mark(textbuffer,
					    "NewTextStart", &iter, TRUE);

    tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
                                    "blue_foreground");
    /* the tag does not exist: create it and let them exist in the tag table.*/
    if (tag == NULL)    {
	tag = gtk_text_buffer_create_tag(textbuffer, "black_foreground",
                                         "foreground", "black", NULL);
        tag = gtk_text_buffer_create_tag(textbuffer, "blue_foreground",
                                         "foreground", "blue", NULL);
        tag = gtk_text_buffer_create_tag(textbuffer, "red_foreground",
                                         "foreground", "red", NULL);
        tag = gtk_text_buffer_create_tag(textbuffer, "darkred_foreground",
                                         "foreground", "darkred", NULL);
        tag = gtk_text_buffer_create_tag(textbuffer, "darkblue_foreground",
                                         "foreground", "darkblue", NULL);
        tag = gtk_text_buffer_create_tag (textbuffer, "darkgreen_foreground",
                                          "foreground", "darkgreen", NULL);
        tag = gtk_text_buffer_create_tag (textbuffer,
                                          "saddlebrown_foreground",
                                          "foreground", "saddlebrown", NULL);
    }

    /* 
     * See rgb.txt for the color names definition 
     * (on my PC it is on /usr/X11R6/lib/X11/rgb.txt)
     */
    switch (log_level & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_ERROR:
	/* a message of this kind aborts the application calling abort() */
        tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
                                        "red_foreground");

	break;
    case G_LOG_LEVEL_CRITICAL:
        tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
                                        "red_foreground");
	break;
    case G_LOG_LEVEL_WARNING:
        tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
                                        "darkred_foreground");
	break;
    case G_LOG_LEVEL_MESSAGE:
        tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
                                        "darkblue_foreground");
	break;
    case G_LOG_LEVEL_INFO:
        tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
                                        "drakgreen_foreground");
	break;
    case G_LOG_LEVEL_DEBUG:
	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textbuffer),
					"saddlebrown_foreground");
	break;
    default:
        tag = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table(textbuffer),
                                         "black_foreground");
	break;
    }

    /*
     * Fatal aborts application. We will try to get the message out anyhow.
     */
    if (log_level & G_LOG_FLAG_FATAL)
	fprintf(stderr, "Fatal error : %s\n", message);
    
    gtk_text_buffer_insert(textbuffer, &iter, message, -1);
    
    gtk_text_buffer_get_end_iter(textbuffer, &iter);
    
    StopMark = gtk_text_buffer_create_mark(textbuffer,
					   "NewTextStop", &iter, TRUE);
    
    gtk_text_buffer_get_iter_at_mark(textbuffer, &StartIter, StartMark);
    gtk_text_buffer_get_iter_at_mark(textbuffer, &StopIter, StopMark);

    gtk_text_buffer_apply_tag(textbuffer, tag, &StartIter, &StopIter);
    
} /* gerbv_gtk_log_handler */


static void
clear_log_button_clicked(GtkButton *button, gpointer user_data)
{
    GtkTextBuffer *textbuffer = NULL;
    GtkTextIter iterstart, iterend;
    textbuffer = gtk_text_view_get_buffer((GtkTextView *)textviewLog);
    
    gtk_text_buffer_get_start_iter(textbuffer, &iterend);
    gtk_text_buffer_get_end_iter(textbuffer, &iterstart);
    
    gtk_text_buffer_delete(textbuffer, &iterstart, &iterend);

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
