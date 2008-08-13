
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#endif
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "textbox.h"

typedef struct {
  GtkWidget *topbox;
  GtkWidget *textbox;
  const char *title;
} TextBoxCache;

static TextBoxCache *textbox_cache = 0;
static int textbox_cache_size = 0;
static int textbox_cache_max = 0;

static void
log_close_cb (GtkWidget * widget, gpointer data)
{
  TextBoxCache *tbc = textbox_cache + (int)data;
  gtk_widget_destroy (tbc->topbox);
  g_free ((char *)tbc->title);
  tbc->title = 0;
}

static void
log_clear_cb (GtkWidget * widget, gpointer data)
{
  TextBoxCache *tbc = textbox_cache + (int)data;
  clear_textbox (tbc->textbox);
}

static void
log_destroy_cb (GtkWidget * widget, gpointer data)
{
  TextBoxCache *tbc = textbox_cache + (int)data;
  g_free ((char *)tbc->title);
  tbc->title = 0;
}

GtkWidget *
get_textbox (const char *title)
{
  GtkWidget *scrolled, *view, *log_window, *vbox, *hbox, *button;
  GtkTextBuffer *buffer;
  int i;

  for (i=0; i<textbox_cache_size; i++)
    if (textbox_cache[i].title
	&& strcmp (title, textbox_cache[i].title) == 0)
      return textbox_cache[i].textbox;

  for (i=0; i<textbox_cache_size; i++)
    if (textbox_cache[i].title == 0)
      break;

  if (i == textbox_cache_size)
    {
      if (textbox_cache_size >= textbox_cache_max)
	{
	  textbox_cache_max += 10;
	  textbox_cache = (TextBoxCache *) realloc (textbox_cache, textbox_cache_max * sizeof (TextBoxCache));
	}
      textbox_cache_size ++;
    }

  textbox_cache[i].title = g_strdup (title);



  log_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (log_window), "destroy",
		    G_CALLBACK (log_destroy_cb), (void *)i);
  gtk_window_set_title (GTK_WINDOW (log_window), title);
  gtk_window_set_wmclass (GTK_WINDOW (log_window), "gerbv_log", "gerbv");
  gtk_window_set_default_size (GTK_WINDOW (log_window), 400, 300);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_container_add (GTK_CONTAINER (log_window), vbox);



  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);

  view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_create_tag (buffer, "heading",
			      "weight", PANGO_WEIGHT_BOLD,
			      "size", 14 * PANGO_SCALE, NULL);
  gtk_text_buffer_create_tag (buffer, "italic",
			      "style", PANGO_STYLE_ITALIC, NULL);
  gtk_text_buffer_create_tag (buffer, "bold",
			      "weight", PANGO_WEIGHT_BOLD, NULL);
  gtk_text_buffer_create_tag (buffer, "center",
			      "justification", GTK_JUSTIFY_CENTER, NULL);
  gtk_text_buffer_create_tag (buffer, "underline",
			      "underline", PANGO_UNDERLINE_SINGLE, NULL);

  gtk_container_add (GTK_CONTAINER (scrolled), view);


  hbox = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock (GTK_STOCK_CLEAR);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (log_clear_cb), (void *)i);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (log_close_cb), (void *)i);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  gtk_widget_realize (log_window);
  gtk_widget_set_uposition (GTK_WIDGET (log_window), 10, 10);
  gtk_widget_show_all (log_window);


  textbox_cache[i].topbox = log_window;
  textbox_cache[i].textbox = view;

  return view;
}

void
clear_textbox (GtkWidget *view)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_set_text (buffer, "", 0);
}

static char *buf = 0;
static int bufsz = 0;

void
tb_printf (GtkWidget *view, const char *fmt, ...)
{
  GtkTextIter iter;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  va_list args;
  int needed;

  va_start (args, fmt);
  needed = g_printf_string_upper_bound (fmt, args);

  if (bufsz < needed+1)
    {
      bufsz = needed + 10;
      buf = realloc (buf, bufsz);
    }

  g_vsnprintf (buf, bufsz, fmt, args);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_get_end_iter (buffer, &iter);

  gtk_text_buffer_insert (buffer, &iter, buf, -1);

  mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, FALSE);
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view), mark,
				0, TRUE, 0.0, 1.0);
  gtk_text_buffer_delete_mark (buffer, mark);
}
