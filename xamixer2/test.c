#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *scale;
	GtkObject *adj;


	gtk_init(&argc, &argv);

	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	adj = gtk_adjustment_new(1.0, 
				 0.0, 
				 110.0,
				 1.0,
				 4.0,
				 0.0);
	scale = gtk_hscale_new(GTK_ADJUSTMENT(adj));
	gtk_widget_show(scale);
	gtk_container_add(GTK_CONTAINER(window), scale);


	gtk_widget_show(window);
	gtk_main();


	return 0;
}
