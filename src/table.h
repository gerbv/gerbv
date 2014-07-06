#ifndef TABLE_H
#define TABLE_H

struct table {
	GtkWidget	*widget;	/* All table */
	GtkListStore	*list_store;
	GType		*types;		/* Column types array */
	GtkCellRenderer **renderers;	/* Column renderers pointers array */
	gint		column_nums;	/* Column number */
};

struct table *table_new_with_columns(gint col_nums, ...);
void table_destroy(struct table *table);
void table_set_sortable(struct table *table);
void table_set_column_align(struct table *table, gint column_num, gfloat align);
int table_add_row(struct table *table, ...);

#endif	/* TABLE_H */
