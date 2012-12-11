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
  GtkListStore *listStoreEntryActions;
  
  GtkTreeView *treeviewEntries;
  
  GtkBuilder *builder;
};

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

enum
{
  COL_ACTION_ENTRY_ENABLED = 0,
  COL_ACTION_ENTRY_NAME,
  COL_ACTION_ENTRY_GLOBS,
  COL_ACTION_ENTRY_CREATE,
  COL_ACTION_ENTRY_MODIFY,
  COL_ACTION_ENTRY_DELETE,
  COL_ACTION_ENTRY_ATTRIBS,
  COL_ACTION_ENTRY_MOVED_FROM,
  COL_ACTION_ENTRY_MOVED_TO
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

void on_action_entrySave_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
}

void on_action_entryClose_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  gtk_widget_hide(gui_data->windowEntry);  
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
void populate_entry_config(struct s_gui_data *gui_data, const char *path,monconf_entry *entry)
{
  if(path && entry)
  {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(gtk_builder_get_object(gui_data->builder,"filechooserPath")), path);
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxRecursive"))->toggle_button), entry->recursive);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnCreate"))->toggle_button), 
				  entry->events & MON_CREATE);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnModify"))->toggle_button), 
				  entry->events & MON_MODIFY);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnDelete"))->toggle_button), 
				  entry->events & MON_DELETE);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnAttrib"))->toggle_button), 
				  entry->events & MON_ATTRIB);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnMovedFrom"))->toggle_button), 
				  entry->events & MON_MOVED_FROM);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnMovedTo"))->toggle_button), 
				  entry->events & MON_MOVED_TO);	
  } 
  else
  {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(gtk_builder_get_object(gui_data->builder,"filechooserPath")), NULL);
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxRecursive"))->toggle_button), 0);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnCreate"))->toggle_button), 
				  0);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnModify"))->toggle_button), 
				  0);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnDelete"))->toggle_button), 
				  0);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnAttrib"))->toggle_button), 
				  0);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnMovedFrom"))->toggle_button), 
				  0);	
    gtk_toggle_button_set_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnMovedTo"))->toggle_button), 
				  0);	
  }
}

void populate_entry_actions(struct s_gui_data *gui_data, monconf_entry *conf_entry)
{
  GtkTreeModel *modelEntries;  
  GtkTreeIter iter;
  monaction_entry *action_entry = NULL;
  monconf_action_entry *conf_action_entry = NULL;
  gtk_list_store_clear(gui_data->listStoreEntryActions);
  GList *action_keys = g_hash_table_get_keys(gui_data->conf->actionMap);
  GList *item = g_list_first(action_keys);
  while(item)
  {
    action_entry = (monaction_entry *)item->data;
    if(conf_entry != NULL) 
    {
      conf_action_entry = monconf_action_entry_get_by_name(conf_entry, action_entry->name);
    }
    gtk_list_store_append(gui_data->listStoreEntryActions, &iter);
    gtk_list_store_set(gui_data->listStoreEntryActions,&iter,
           COL_ACTION_ENTRY_ENABLED, conf_action_entry != NULL,
	   COL_ACTION_ENTRY_NAME, action_entry->name,
           COL_ACTION_ENTRY_GLOBS, (conf_action_entry != NULL ? string_join(conf_action_entry->globs) : ""),           
           COL_ACTION_ENTRY_CREATE, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_CREATE) : 0),
           COL_ACTION_ENTRY_MODIFY, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_MODIFY) : 0),
           COL_ACTION_ENTRY_DELETE, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_DELETE) : 0),
           COL_ACTION_ENTRY_ATTRIBS, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_ATTRIB) : 0),
           COL_ACTION_ENTRY_MOVED_FROM, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_MOVED_FROM) : 0),
           COL_ACTION_ENTRY_MOVED_TO, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_MOVED_TO) : 0),           
           -1
          );
  
    item = item->next;
  }  
  g_list_free(action_keys);
}

void show_config_window(struct s_gui_data *gui_data, int action_type)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;  
  char show_window = 0;  
  switch(action_type)
  {
    case EDIT_ACTION_ADD:            
      show_window = 1;
      populate_entry_config(gui_data,NULL,NULL);
      break;
    case EDIT_ACTION_MODIFY:    
      selection = gtk_tree_view_get_selection(gui_data->treeviewEntries);
      if (gtk_tree_selection_get_selected(selection, &model, &iter))
      {
	gchar *path;    
	gtk_tree_model_get (model, &iter, COL_ENTRY_PATH, &path, -1);
        monconf_entry *entry = monconf_entry_get_by_path(gui_data->conf, path);
	populate_entry_config(gui_data, path, entry);
	g_free(path);
	show_window = 1;
      }
      break;
  }
  if(show_window) 
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
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;  
  selection = gtk_tree_view_get_selection(gui_data->treeviewEntries);
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gchar *path;    
    gtk_tree_model_get (model, &iter, COL_ENTRY_PATH, &path, -1);
    monconf_entry *entry = monconf_entry_get_by_path(gui_data->conf, path);
    
    GtkWidget *dialog;    
    dialog = gtk_message_dialog_new(GTK_WINDOW(gui_data->windowEntry),
	      GTK_DIALOG_DESTROY_WITH_PARENT,
	      GTK_MESSAGE_QUESTION,
	      GTK_BUTTONS_YES_NO,
	      "Remove the entry '%s'?",path);
    gtk_window_set_title(GTK_WINDOW(dialog), "Remove Entry");
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
    {
      monconf_remove_entry(gui_data->conf, entry);
      populate_config(gui_data);
    }
    gtk_widget_destroy(dialog);      
    g_free(path);
  }
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
  
  
  GtkStatusIcon *systray_icon;
  gtk_init (&argc, &argv);
  
  data.builder = gtk_builder_new ();
  gtk_builder_add_from_file (data.builder, "ui-glade/monarqui_gui.glade", NULL);  
  
  data.builder = GTK_BUILDER(data.builder);
  data.windowMain = GTK_WIDGET(gtk_builder_get_object (data.builder, "windowMain"));
  data.windowEntry = GTK_WIDGET(gtk_builder_get_object (data.builder, "windowEntry"));
  data.windowConfig = GTK_WIDGET(gtk_builder_get_object (data.builder, "windowConfig"));
  data.action_mainExit = GTK_ACTION(gtk_builder_get_object(data.builder, "action_mainExit"));
  data.action_configOpen = GTK_ACTION(gtk_builder_get_object(data.builder, "action_configOpen"));
  data.action_configClose = GTK_ACTION(gtk_builder_get_object(data.builder, "action_configClose"));
  data.action_startPause = GTK_ACTION(gtk_builder_get_object(data.builder, "action_startPause"));
  data.action_entryAdd = GTK_ACTION(gtk_builder_get_object(data.builder, "action_entryAdd"));
  data.action_entryModify = GTK_ACTION(gtk_builder_get_object(data.builder, "action_entryModify"));
  data.action_entryDelete = GTK_ACTION(gtk_builder_get_object(data.builder, "action_entryDelete"));  
  data.action_saveConfig = GTK_ACTION(gtk_builder_get_object(data.builder, "action_saveConfig"));  
  data.image_startStop = (GtkImage *)GTK_WIDGET(gtk_builder_get_object(data.builder,"image_startStop"));  
  data.args = &args;
  data.conf = conf;
  data.treeviewEntries = GTK_TREE_VIEW(gtk_builder_get_object(data.builder,"treeviewEntries"));
  data.listStoreActions = GTK_LIST_STORE(gtk_builder_get_object(data.builder,"listStoreActions"));  
  data.listStoreEntries = GTK_LIST_STORE(gtk_builder_get_object(data.builder,"listStoreEntries"));
  data.listStoreEntryActions = GTK_LIST_STORE(gtk_builder_get_object(data.builder,"listStoreEntryActions"));  
  
  gtk_builder_connect_signals (data.builder, (gpointer)&data);  
  
  gtk_image_set_from_icon_name(data.image_startStop, ICON_NAME_START, GTK_ICON_SIZE_BUTTON);          
  systray_icon = gtk_status_icon_new_from_icon_name(ICON_SYSTRAY);  
  gtk_status_icon_set_visible(systray_icon, TRUE);
  populate_config(&data);  
  
  gtk_widget_show (data.windowMain);                
  gtk_main ();
  g_object_unref (G_OBJECT (data.builder));    
  return 0;
}

void populate_config(struct s_gui_data *gui_data) 
{
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



