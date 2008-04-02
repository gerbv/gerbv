/*
 *                            COPYRIGHT
 *
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
 *
 *  Copyright (C) 2008 Dan McMahill
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <gtk/gtk.h>

#include "attribute.h"
#include "gerbv_screen.h"

#define dprintf if(DEBUG) printf

/* Callback for toggling a boolean attribute */
static void
set_flag_cb (GtkToggleButton * button, gboolean * flag)
{
  *flag = gtk_toggle_button_get_active (button);
}

/* Callback for setting an integer value */
static void
intspinner_changed_cb (GtkWidget * spin_button, gpointer data)
{
  int *ival = data;

  *ival = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin_button));
}

/* Callback for setting a floating point value */
static void
dblspinner_changed_cb (GtkWidget * spin_button, gpointer data)
{
  double *dval = data;

  *dval = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin_button));
}

/* Callback for setting an string value */
static void
entry_changed_cb (GtkEntry * entry, char **str)
{
  const gchar *s;

  s = gtk_entry_get_text (entry);

  if (*str)
    free (*str);
  *str = strdup (s);
}

/* Callback for setting an enum value */
static void
enum_changed_cb (GtkWidget * combo_box, int *val)
{
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));
  *val = active;
}

/* Utility function for building a vbox with a text label */
/* Written by Bill Wilson for PCB */
static GtkWidget *
ghid_category_vbox (GtkWidget * box, const gchar * category_header,
		    gint header_pad,
		    gint box_pad, gboolean pack_start, gboolean bottom_pad)
{
  GtkWidget *vbox, *vbox1, *hbox, *label;
  gchar *s;

  vbox = gtk_vbox_new (FALSE, 0);
  if (pack_start)
    gtk_box_pack_start (GTK_BOX (box), vbox, FALSE, FALSE, 0);
  else
    gtk_box_pack_end (GTK_BOX (box), vbox, FALSE, FALSE, 0);

  if (category_header)
    {
      label = gtk_label_new (NULL);
      s = g_strconcat ("<span weight=\"bold\">", category_header,
		       "</span>", NULL);
      gtk_label_set_markup (GTK_LABEL (label), s);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, header_pad);
      g_free (s);
    }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  label = gtk_label_new ("     ");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  vbox1 = gtk_vbox_new (FALSE, box_pad);
  gtk_box_pack_start (GTK_BOX (hbox), vbox1, TRUE, TRUE, 0);

  if (bottom_pad)
    {
      label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
    }
  return vbox1;
}

/* Utility function for creating a spin button */
/* Written by Bill Wilson for PCB */
static void
ghid_spin_button (GtkWidget * box, GtkWidget ** spin_button, gfloat value,
		  gfloat low, gfloat high, gfloat step0, gfloat step1,
		  gint digits, gint width,
		  void (*cb_func) (), gpointer data, gboolean right_align,
		  gchar * string)
{
  GtkWidget *hbox = NULL, *label, *spin_but;
  GtkSpinButton *spin;
  GtkAdjustment *adj;

  if (string && box)
    {
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 2);
      box = hbox;
    }
  adj = (GtkAdjustment *) gtk_adjustment_new (value,
					      low, high, step0, step1, 0.0);
  spin_but = gtk_spin_button_new (adj, 0.5, digits);
  if (spin_button)
    *spin_button = spin_but;
  if (width > 0)
    gtk_widget_set_size_request (spin_but, width, -1);
  spin = GTK_SPIN_BUTTON (spin_but);
  gtk_spin_button_set_numeric (spin, TRUE);
  if (data == NULL)
    data = (gpointer) spin;
  if (cb_func)
    g_signal_connect (G_OBJECT (spin_but), "value_changed",
		      G_CALLBACK (cb_func), data);
  if (box)
    {
      if (right_align && string)
	{
	  label = gtk_label_new (string);
	  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
	  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 2);
	}
      gtk_box_pack_start (GTK_BOX (box), spin_but, FALSE, FALSE, 2);
      if (!right_align && string)
	{
	  label = gtk_label_new (string);
	  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 2);
	}
    }
}

/* Utility function for creating a check button */
/* Written by Bill Wilson for PCB */
static void
ghid_check_button_connected (GtkWidget * box,
			     GtkWidget ** button,
			     gboolean active,
			     gboolean pack_start,
			     gboolean expand,
			     gboolean fill,
			     gint pad,
			     void (*cb_func) (),
			     gpointer data, gchar * string)
{
  GtkWidget *b;

  if (!string)
    return;
  b = gtk_check_button_new_with_label (string);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), active);
  if (box && pack_start)
    gtk_box_pack_start (GTK_BOX (box), b, expand, fill, pad);
  else if (box && !pack_start)
    gtk_box_pack_end (GTK_BOX (box), b, expand, fill, pad);

  if (cb_func)
    gtk_signal_connect (GTK_OBJECT (b), "clicked",
			GTK_SIGNAL_FUNC (cb_func), data);
  if (button)
    *button = b;
}

/* 
 * The following function is taken almost directly from
 * ghid_attribte_dialog() from pcb.  It is a generic attribute editor
 * gui where the dialog is built on the fly based on a passed in
 * attribute list.
 * 
 * Written by Dan McMahill
 */
int
attribute_interface_dialog (HID_Attribute * attrs,
		       int n_attrs, HID_Attr_Val * results,
		       const char * title,
		       const char * descr)
{
  GtkWidget *dialog, *main_vbox, *vbox, *vbox1, *hbox, *entry;
  GtkWidget *combo;
  GtkWidget *widget;
  int i, j;
  GtkTooltips *tips;
  int rc = 0;

  dprintf ("%s(%p, %d, %p, \"%s\", \"%s\")\n", __FUNCTION__, attrs, n_attrs, results, title, descr);

  tips = gtk_tooltips_new ();

  dialog = gtk_dialog_new_with_buttons (title,
					GTK_WINDOW (screen.win.topLevelWindow),
					GTK_DIALOG_MODAL
					| GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_CANCEL, GTK_RESPONSE_NONE,
					GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  gtk_window_set_wmclass (GTK_WINDOW (dialog), "gerbv_attribute_editor", "gerbv");

  main_vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);

  vbox = ghid_category_vbox (main_vbox, descr != NULL ? descr : "",
			     4, 2, TRUE, TRUE);

  /* 
   * Iterate over all the attributes and build up a dialog box
   * that lets us control all of the options.  By doing things this
   * way, any changes to the attributes or if there is a new list of
   * attributes, everything will automatically be reflected in this
   * dialog box. 
   */
  for (j = 0; j < n_attrs; j++)
      {
	  dprintf ("%s():  Adding attribute #%d\n", __FUNCTION__, j);
	  switch (attrs[j].type)
	      {
	      case HID_Label:
		  widget = gtk_label_new (attrs[j].name);
		  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
		  gtk_tooltips_set_tip (tips, widget, attrs[j].help_text, NULL);
		  break;
		  
	      case HID_Integer:
		  hbox = gtk_hbox_new (FALSE, 4);
		  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
		  
		  /* 
		   * FIXME 
		   * need to pick the "digits" argument based on min/max
		   * values
		   */
		  ghid_spin_button (hbox, &widget, attrs[j].default_val.int_value,
				    attrs[j].min_val, attrs[j].max_val, 1.0, 1.0, 0, 0,
				    intspinner_changed_cb,
				    &(attrs[j].default_val.int_value), FALSE, NULL);
		  
		  gtk_tooltips_set_tip (tips, widget, attrs[j].help_text, NULL);
		  
		  widget = gtk_label_new (attrs[j].name);
		  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
		  break;
		  
	      case HID_Real:
		  hbox = gtk_hbox_new (FALSE, 4);
		  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
		  
		  /* 
		   * FIXME 
		   * need to pick the "digits" and step size argument more
		   * intelligently
		   */
		  ghid_spin_button (hbox, &widget, attrs[j].default_val.real_value,
				    attrs[j].min_val, attrs[j].max_val, 0.01, 0.01, 3,
				    0, 
				    dblspinner_changed_cb,
				    &(attrs[j].default_val.real_value), FALSE, NULL);
		  
		  gtk_tooltips_set_tip (tips, widget, attrs[j].help_text, NULL);
		  
		  widget = gtk_label_new (attrs[j].name);
		  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
		  break;
		  
	      case HID_String:
		  hbox = gtk_hbox_new (FALSE, 4);
		  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
		  
		  entry = gtk_entry_new ();
		  gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
		  gtk_entry_set_text (GTK_ENTRY (entry),
				      attrs[j].default_val.str_value);
		  gtk_tooltips_set_tip (tips, entry, attrs[j].help_text, NULL);
		  g_signal_connect (G_OBJECT (entry), "changed",
				    G_CALLBACK (entry_changed_cb),
				    &(attrs[j].default_val.str_value));
		  
		  widget = gtk_label_new (attrs[j].name);
		  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
		  break;
		  
	      case HID_Boolean:
		  /* put this in a check button */
		  ghid_check_button_connected (vbox, &widget,
					       attrs[j].default_val.int_value,
					       TRUE, FALSE, FALSE, 0, set_flag_cb,
					       &(attrs[j].default_val.int_value),
					       attrs[j].name);
		  gtk_tooltips_set_tip (tips, widget, attrs[j].help_text, NULL);
		  break;
		  
	      case HID_Enum:
		  hbox = gtk_hbox_new (FALSE, 4);
		  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
		  
		  /* 
		   * We have to put the combo_box inside of an event_box in
		   * order for tooltips to work.
		   */
		  widget = gtk_event_box_new ();
		  gtk_tooltips_set_tip (tips, widget, attrs[j].help_text, NULL);
		  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
		  
		  combo = gtk_combo_box_new_text ();
		  gtk_container_add (GTK_CONTAINER (widget), combo);
		  g_signal_connect (G_OBJECT (combo), "changed",
				    G_CALLBACK (enum_changed_cb),
				    &(attrs[j].default_val.int_value));
		  

		  /* 
		   * Iterate through each value and add them to the
		   * combo box
		   */
		  i = 0;
		  while (attrs[j].enumerations[i])
		      {
			  gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
						     attrs[j].enumerations[i]);
			  i++;
		      }
		  gtk_combo_box_set_active (GTK_COMBO_BOX (combo),
					    attrs[j].default_val.int_value);
		  widget = gtk_label_new (attrs[j].name);
		  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
		  break;

	      case HID_Mixed:
		  dprintf ("HID_Mixed\n");
		  break;

	      case HID_Path:
		  vbox1 = ghid_category_vbox (vbox, attrs[j].name, 4, 2, TRUE, TRUE);
		  entry = gtk_entry_new ();
		  gtk_box_pack_start (GTK_BOX (vbox1), entry, FALSE, FALSE, 0);
		  gtk_entry_set_text (GTK_ENTRY (entry),
				      attrs[j].default_val.str_value);
		  g_signal_connect (G_OBJECT (entry), "changed",
				    G_CALLBACK (entry_changed_cb),
				    &(attrs[j].default_val.str_value));

		  gtk_tooltips_set_tip (tips, entry, attrs[j].help_text, NULL);
		  break;

	      default:
		  fprintf (stderr, "%s: unknown type of HID attribute\n", __FUNCTION__);
		  break;
	      }
      }


  gtk_widget_show_all (dialog);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
      {
	  /* copy over the results */
	  for (i = 0; i < n_attrs; i++)
	      {
		  results[i] = attrs[i].default_val;
		  if (results[i].str_value)
		      results[i].str_value = strdup (results[i].str_value);
	      }
	  rc = 0;
      }
  else
      rc = 1;

  gtk_widget_destroy (dialog);

  return rc;
}

void
attribute_merge (HID_Attribute *dest, int ndest, HID_Attribute *src, int nsrc)
{
    int i, j;

    /* Here is a brain dead merge algorithm which shold make anyone cringe.
     * Still, it is simple and we won't merge many attributes and not
     * many times either.
     */

    for (i = 0 ; i < nsrc ; i++) {
	/* see if our destination wants this attribute */
	j = 0;
	while (j < ndest && strcmp (src[i].name, dest[j].name) != 0)
	    j++;

	/* if we wanted it and it is the same type, copy it over */
	if (j < ndest && src[i].type == dest[j].type) {
	    dest[j].default_val = src[i].default_val;
	}
    }

}


