#include "gui_main.h"
#include "../reactlist/monconf.h"
#include "../reactlist/monwatch.h"
#include "../reactlist/monarqui_threads.h"
#include "../reactlist/monarqui_listener.h"
#include "../reactlist/monarqui_reactor.h"
#include "../common/monarqui_common.h"
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

#define ICON_NAME_START "media-playback-start"
#define ICON_NAME_STOP "gtk-stop"
#define ICON_SYSTRAY "folder"

struct s_gui_data 
{  
  pthread_t rthread; 
  pthread_t lthread;
  reactstart rstart;
  liststart lstart;  
  void *zmq_context;
  config_args *args;
  monconf *conf;
  GtkWidget *windowMain;
  GtkWidget *windowEntry;
  GtkWidget *windowConfig;
  
  GtkAction *action_mainExit;
  GtkAction *action_configOpen;
  GtkAction *action_configClose;
  GtkAction *action_startPause;  
  GtkAction *action_entryAdd;  
  GtkAction *action_entryModify;  
  GtkAction *action_entryDelete;  
  GtkAction *action_saveConfig;  
  
  GtkImage *image_startStop;
  
  GtkListStore *listStoreActions;
  GtkListStore *listStoreEntries;
  
  GtkTreeView *treeviewEntries;
};

void populate_config(struct s_gui_data *gui_data);

void on_action_mainExit_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  void *rstop_status, *lstop_status;
  if(gui_data->lstart.active)
  {
    stop_reactor_and_listener(&(gui_data->rthread), &(gui_data->rstart), &rstop_status, 
            &(gui_data->lthread), &(gui_data->lstart), &lstop_status);
  }
  gtk_main_quit();
}

void on_action_configOpen_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_widget_show(gui_data->windowConfig);
}

void on_action_configClose_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_widget_destroy(gui_data->windowConfig);
}

void on_reactlist_start(struct s_gui_data *gui_data) 
{
  gtk_image_set_from_icon_name(gui_data->image_startStop, ICON_NAME_STOP, GTK_ICON_SIZE_BUTTON); 
}

void on_reactlist_stop(struct s_gui_data *gui_data) 
{
  gtk_image_set_from_icon_name(gui_data->image_startStop, ICON_NAME_START, GTK_ICON_SIZE_BUTTON); 
  
}

void on_action_startPause_activate(GtkAction *action, gpointer user_data)
{   
  pthread_t *rthread, *lthread;
  reactstart *rstart;
  liststart *lstart; 
  void *zmq_context;
    
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  int rstatus, lstatus;
  void *rstop_status, *lstop_status;  
  if(gui_data->lstart.active)
  {
    stop_reactor_and_listener(&(gui_data->rthread), &(gui_data->rstart), &rstop_status, 
            &(gui_data->lthread), &(gui_data->lstart), &lstop_status);            
    on_reactlist_stop(gui_data);
  }
  else 
  {   
    start_reactor_and_listener(gui_data->conf,&(gui_data->rthread), &(gui_data->rstart), &rstatus, 
            &(gui_data->lthread), &(gui_data->lstart), &lstatus);  
    on_reactlist_start(gui_data);
  }
}

void show_config_window(struct s_gui_data *gui_data, int action_type)
{
  gtk_widget_show(gui_data->windowEntry);
}


void on_action_entryAdd_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  show_config_window(gui_data, EDIT_ACTION_ADD);
}

void on_action_entryModify_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  show_config_window(gui_data, EDIT_ACTION_MODIFY);
}

void on_action_entryDelete_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
}

void on_action_saveConfig_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
}

void on_windowMain_destroy (GtkObject *object, gpointer user_data)
{   
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_action_activate(gui_data->action_mainExit);
}

void on_windowConfig_destroy (GtkObject *object, gpointer user_data)
{   
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_action_activate(gui_data->action_configClose);
}

void on_btnStartPause_clicked(GtkObject *object, gpointer user_data)
{
}


int main (int argc, char *argv[])
{
  struct s_gui_data data;  
  config_args args;
  args.config_path = NULL;
  monconf_parse_cli_args(&args, argc, argv);  
    
  monconf_prepare_config_directory();
  monconf *conf = monconf_create();
  monconf_read_config(conf, args.config_path);
  data.rstart.active = 0;
  data.lstart.active = 0;
  
  GtkBuilder      *builder; 
  GtkStatusIcon *systray_icon;
  gtk_init (&argc, &argv);
  
  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, "ui-glade/monarqui_gui.glade", NULL);  
  
  data.windowMain = GTK_WIDGET(gtk_builder_get_object (builder, "windowMain"));
  data.windowEntry = GTK_WIDGET(gtk_builder_get_object (builder, "windowEntry"));
  data.windowConfig = GTK_WIDGET(gtk_builder_get_object (builder, "windowConfig"));
  data.action_mainExit = GTK_ACTION(gtk_builder_get_object(builder, "action_mainExit"));
  data.action_configOpen = GTK_ACTION(gtk_builder_get_object(builder, "action_configOpen"));
  data.action_configClose = GTK_ACTION(gtk_builder_get_object(builder, "action_configClose"));
  data.action_startPause = GTK_ACTION(gtk_builder_get_object(builder, "action_startPause"));
  data.action_entryAdd = GTK_ACTION(gtk_builder_get_object(builder, "action_entryAdd"));
  data.action_entryModify = GTK_ACTION(gtk_builder_get_object(builder, "action_entryModify"));
  data.action_entryDelete = GTK_ACTION(gtk_builder_get_object(builder, "action_entryDelete"));  
  data.action_saveConfig = GTK_ACTION(gtk_builder_get_object(builder, "action_saveConfig"));  
  data.image_startStop = (GtkImage *)GTK_WIDGET(gtk_builder_get_object(builder,"image_startStop"));  
  data.args = &args;
  data.conf = conf;
  data.treeviewEntries = (GtkTreeView *) GTK_TREE_VIEW(gtk_builder_get_object(builder,"treeviewEntries"));
  data.listStoreActions = (GtkListStore *) GTK_LIST_STORE(gtk_builder_get_object(builder,"listStoreActions"));  
  data.listStoreEntries = (GtkListStore *) GTK_LIST_STORE(gtk_builder_get_object(builder,"listStoreEntries"));  
  
  gtk_builder_connect_signals (builder, (gpointer)&data);  
  
  gtk_image_set_from_icon_name(data.image_startStop, ICON_NAME_START, GTK_ICON_SIZE_BUTTON);          
  systray_icon = gtk_status_icon_new_from_icon_name(ICON_SYSTRAY);  
  gtk_status_icon_set_visible(systray_icon, TRUE);
  populate_config(&data);
  g_object_unref (G_OBJECT (builder));    
  
  gtk_widget_show (data.windowMain);                
  gtk_main ();
  
  return 0;
}

void populate_config(struct s_gui_data *gui_data) 
{
  enum
  {
    COL_ENTRY_PATH = 0,
    COL_ENTRY_RECURSIVE,
    COL_ENTRY_EVENT_CREATE,
    COL_ENTRY_EVENT_MODIFY,
    COL_ENTRY_EVENT_DELETE,
    COL_ENTRY_EVENT_ATTRIBS,
    COL_ENTRY_EVENT_MOVED_FROM,
    COL_ENTRY_EVENT_MOVED_TO,    
    COL_ENTRY_IGNORE
  };
  GtkTreeModel *modelEntries;
  GtkTreeIter iter;

  gtk_list_store_clear(gui_data->listStoreEntries);
  GList *item = g_list_first(gui_data->conf->entrylist);
  while(item)
  {
    monconf_entry *entry = (monconf_entry *)item->data;
    gtk_list_store_append(gui_data->listStoreEntries, &iter);
    gtk_list_store_set(gui_data->listStoreEntries,&iter,
           COL_ENTRY_PATH, entry->file_name,
           COL_ENTRY_RECURSIVE, (gboolean)entry->recursive,
           COL_ENTRY_EVENT_CREATE, (gboolean)(entry->events & MON_CREATE),
           COL_ENTRY_EVENT_MODIFY, (gboolean)(entry->events & MON_MODIFY),
           COL_ENTRY_EVENT_DELETE, (gboolean)(entry->events & MON_DELETE),
           COL_ENTRY_EVENT_ATTRIBS, (gboolean)(entry->events & MON_ATTRIB),
           COL_ENTRY_EVENT_MOVED_FROM, (gboolean)(entry->events & MON_MOVED_FROM),
           COL_ENTRY_EVENT_MOVED_TO, (gboolean)(entry->events & MON_MOVED_TO),
           COL_ENTRY_IGNORE, string_join(entry->ignore_files),           
           -1
          );
  
    item = item->next;
  }
}
