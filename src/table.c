#include <glib-object.h>
#include <gtk/gtk.h>
#include <string.h>

#include "table.h"

/* ... is (char *)title / (GType)type pairs */
struct table *table_new_with_columns(gint col_nums, ...)
{
	struct table *table;
	GtkTreeViewColumn *column;
	const char *titles[col_nums];
	va_list args;
	gint i;

	if (col_nums == 0)
		return NULL;

	va_start(args, col_nums);
	table = g_new(struct table, 1);
	table->types = g_new(GType, col_nums);
	for (i = 0; i < col_nums; i++) {
		titles[i] = va_arg(args, const char *);
		table->types[i] = va_arg(args, GType);
	}
	va_end(args);

	table->column_nums = col_nums;
	table->list_store = gtk_list_store_newv(col_nums, table->types);
	table->widget = gtk_tree_view_new_with_model(
			GTK_TREE_MODEL(table->list_store));
	/* Auto free tree_model at tree_view destruction */
	g_object_unref(GTK_TREE_MODEL(table->list_store));

	table->renderers = g_new(GtkCellRenderer *, col_nums);
	for (i = 0; i < col_nums; i++) {
		table->renderers[i] = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new();
		gtk_tree_view_column_set_title(column, titles[i]);
		gtk_tree_view_column_pack_start(column,
				table->renderers[i], FALSE);
		/* TODO: get "text" attribute by types[i] function? */
		gtk_tree_view_column_add_attribute(column,
				table->renderers[i], "text", i);
		gtk_tree_view_append_column(GTK_TREE_VIEW(table->widget), column);
	}

	return table;
}

void table_destroy(struct table *table)
{
	gtk_widget_destroy(table->widget);
	g_free(table->types);
	g_free(table);
}

/* Set column alignment:
   0.0 -- left, 0.5 -- center, 1.0 -- right */
void table_set_column_align(struct table *table, gint column_num, gfloat align)
{
	g_object_set(G_OBJECT(table->renderers[column_num]),
			"xalign", align, NULL);
}

/* ... is values with types as in table_new_with_columns() */
int table_add_row(struct table *table, ...)
{
	GtkTreeIter iter;
	GValue val;
	va_list args;
	gint i;

	g_type_init();

	va_start(args, table);
	gtk_list_store_append(table->list_store, &iter);
	for (i = 0; i < table->column_nums; i++) {
		memset(&val, 0, sizeof(GValue));
		g_value_init(&val, table->types[i]);
		switch (table->types[i]) {
		case G_TYPE_STRING:
			g_value_set_static_string(&val,
					va_arg(args, const char *));
			break;
		case G_TYPE_INT:
			g_value_set_int(&val, va_arg(args, int));
			break;
		case G_TYPE_UINT:
			g_value_set_uint(&val, va_arg(args, unsigned int));
			break;
		case G_TYPE_DOUBLE:
			g_value_set_double(&val, va_arg(args, double));
			break;
		default:
			g_assert_not_reached();
		}
		gtk_list_store_set_value(table->list_store, &iter, i, &val);
	}
	va_end(args);

	return 0;
}
