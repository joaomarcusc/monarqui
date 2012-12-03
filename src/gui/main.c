#include "../reactlist/monconf.h"
#include "../reactlist/monwatch.h"
#include "../reactlist/monarqui_threads.h"
#include "../reactlist/monarqui_listener.h"
#include "../reactlist/monarqui_reactor.h"
#include "../reactlist/monarqui_common.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/inotify.h>
#include <glib.h>
#include <gtk/gtk.h>

void on_startpause_clicked (GtkObject *object, gpointer user_data)
{
  pthread_t *rthread, *lthread;
  reactstart *rstart;
  liststart *lstart; 
  void *zmq_context;
  
  GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(object));
  zmq_context = (void *)g_object_get_data(G_OBJECT(window), "zmq_context");
  rthread = (pthread_t *)g_object_get_data(G_OBJECT(window), "rthread");
  lthread = (pthread_t *)g_object_get_data(G_OBJECT(window), "lthread");
  rstart = (reactstart *)g_object_get_data(G_OBJECT(window), "rstart");
  lstart = (liststart *)g_object_get_data(G_OBJECT(window), "lstart");
  int rstatus, lstatus;
  void *rstop_status, *lstop_status;
  if(lstart->active)
  {
    stop_reactor_and_listener(zmq_context, rthread, rstart, &rstop_status, lthread, lstart, &lstop_status);
    gtk_button_set_label((GtkButton *)object, "Start");
  }
  else 
  {
    start_reactor_and_listener(zmq_context, rthread, rstart, &rstatus, lthread, lstart, &lstatus);
    gtk_button_set_label((GtkButton *)object, "Stop");
  }
}

void on_window_destroy (GtkObject *object, gpointer user_data)
{
  void *zmq_context = (void *)g_object_get_data(G_OBJECT(object), "zmq_context");
  pthread_t *rthread = (pthread_t *)g_object_get_data(G_OBJECT(object), "rthread");
  pthread_t *lthread = (pthread_t *)g_object_get_data(G_OBJECT(object), "lthread");
  reactstart *rstart = (reactstart *)g_object_get_data(G_OBJECT(object), "rstart");
  liststart *lstart= (liststart *)g_object_get_data(G_OBJECT(object), "lstart");
  void *rstatus, *lstatus;
  if(lstart->active)
  {
    stop_reactor_and_listener(zmq_context, rthread, rstart, &rstatus, lthread, lstart, &lstatus);
  }
  gtk_main_quit();
}

int
main (int argc, char *argv[])
{
    void *zmq_context;
    
    pthread_t rthread, lthread;
    reactstart rstart;
    liststart lstart;
    
    rstart.active = 0;
    lstart.active = 0;
    
    GtkBuilder      *builder; 
    GtkWidget       *window;

    zmq_context = CREATE_ZMQ_CONTEXT();
    gtk_init (&argc, &argv);

    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, "ui-glade/monarqui.glade", NULL);
    window = GTK_WIDGET (gtk_builder_get_object (builder, "windowMain"));

    g_object_set_data(G_OBJECT(window), "zmq_context", (gpointer)zmq_context);
    g_object_set_data(G_OBJECT(window), "rthread", (gpointer)&rthread);
    g_object_set_data(G_OBJECT(window), "lthread", (gpointer)&lthread);
    g_object_set_data(G_OBJECT(window), "rstart", (gpointer)&rstart);
    g_object_set_data(G_OBJECT(window), "lstart", (gpointer)&lstart);
    gtk_builder_connect_signals (builder, NULL);

    g_object_unref (G_OBJECT (builder));
    
    gtk_widget_show (window);                
    gtk_main ();

    return 0;
}
