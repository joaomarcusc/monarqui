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

#define MAIN_PAGE_ENTRIES 0
#define MAIN_PAGE_ACTIONS 0
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
  GtkWidget *windowAction;
  
  GtkAction *action_mainExit;
  GtkAction *action_configOpen;
  GtkAction *action_configClose;
  GtkAction *action_startPause;  
  GtkAction *action_entryAdd;  
  GtkAction *action_entryModify;  
  GtkAction *action_entryDelete;  
  GtkAction *action_actionAdd;  
  GtkAction *action_actionModify;  
  GtkAction *action_actionDelete;  
  GtkAction *action_saveConfig;  
  GtkAction* action_toggleApplication;
  GtkAction* action_systrayPopup;
  
  GtkImage *image_startStop;
  
  GtkStatusIcon *systrayIcon;
  
  GtkListStore *listStoreActions;
  GtkListStore *listStoreEntries;
  GtkListStore *listStoreEntryActions;
  
  GtkTreeView *treeviewActions;
  GtkTreeView *treeviewEntries;
  GtkTreeView *treeviewEntryActions;
  
  GtkBuilder *builder;
  
  GtkMenu *systrayMenu;
  GtkMenuItem *systrayMenuItem_startStop;
  GtkMenuItem *systrayMenuItem_quit;
  
  monconf_entry *curr_entry;
  monaction_entry *curr_action;
  
  GtkImage *image_started;
  GtkImage *image_started_event;
  GtkImage *image_stopped;
  
  void *sub_event_log;
  
  GtkTextBuffer *textbufferLog;  
  
  gboolean logged_event;
  guint log_timer_id;
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

enum
{
  COL_ACTION_NAME=0,
  COL_ACTION_TYPE,
  COL_ACTION_SCRIPT
};

void populate_entries(struct s_gui_data *gui_data);
void populate_entry_actions(struct s_gui_data *gui_data, monconf_entry *conf_entry);
void populate_actions(struct s_gui_data *gui_data);

void on_action_toggleApplication_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  if(gtk_widget_get_visible(gui_data->windowMain))
  {
    gtk_widget_hide(gui_data->windowMain);
    gtk_widget_hide(gui_data->windowEntry);
  }
  else
  {
    gtk_widget_show(gui_data->windowMain);
  }
}

void on_action_systrayPopup_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;    
}

void on_action_mainExit_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  void *rstop_status, *lstop_status;
  if(gui_data->lstart.active)
  {
    stop_reactor_and_listener(&(gui_data->rthread), &(gui_data->rstart), &rstop_status, 
            &(gui_data->lthread), &(gui_data->lstart), &lstop_status);
  }
  monconf_free(gui_data->conf);  
  gtk_main_quit();
}

void on_action_entrySave_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data; 
  GList *list_item;
  monconf *conf = gui_data->conf;
  monconf_action_entry *action_entry;
  GtkTreeModel *model;
  GtkTreeIter iter;
  if(gui_data->curr_entry != NULL)
  {
    g_free(gui_data->curr_entry->file_name);
  }
  else
  {
    gui_data->curr_entry = monconf_new_entry(gui_data->conf);
  }
  monconf_entry_add_ignores_from_csv(gui_data->curr_entry, (char *)gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui_data->builder,"entryIgnores"))));
  gui_data->curr_entry->file_name = g_strdup(gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(gtk_builder_get_object(gui_data->builder,"filechooserPath"))));
  gui_data->curr_entry->recursive = (gtk_toggle_button_get_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxRecursive"))->toggle_button)) ? TRUE : FALSE);
  gui_data->curr_entry->events = 
    (gtk_toggle_button_get_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnCreate"))->toggle_button)) ? MON_CREATE : 0)
    | (gtk_toggle_button_get_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnModify"))->toggle_button)) ? MON_MODIFY: 0)
    | (gtk_toggle_button_get_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnDelete"))->toggle_button)) ? MON_DELETE: 0)
    | (gtk_toggle_button_get_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnAttrib"))->toggle_button)) ? MON_ATTRIB: 0)
    | (gtk_toggle_button_get_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnMovedFrom"))->toggle_button)) ? MON_MOVED_FROM : 0)
    | (gtk_toggle_button_get_active(&(GTK_CHECK_BUTTON(gtk_builder_get_object(gui_data->builder,"checkboxOnMovedTo"))->toggle_button)) ? MON_MOVED_TO: 0);  
  
  short int valid;
  gboolean enabled, event_create, event_modify, event_delete, event_attribs, event_move_from, event_move_to;
  gchar *action_name, *globs;
  model = gtk_tree_view_get_model(gui_data->treeviewEntryActions);  
  valid = gtk_tree_model_get_iter_first(model, &iter);
  while(valid)
  {    
    gboolean move_next = TRUE;
    gtk_tree_model_get(model, &iter,
		       COL_ACTION_ENTRY_ENABLED, &enabled, 
		       COL_ACTION_ENTRY_NAME, &action_name,
		       COL_ACTION_ENTRY_GLOBS, &globs,
		       COL_ACTION_ENTRY_CREATE, &event_create,
		       COL_ACTION_ENTRY_MODIFY, &event_modify,
		       COL_ACTION_ENTRY_DELETE, &event_delete,
		       COL_ACTION_ENTRY_ATTRIBS, &event_attribs,
		       COL_ACTION_ENTRY_MOVED_FROM, &event_move_from,
		       COL_ACTION_ENTRY_MOVED_TO, &event_move_to,
		       -1);
    action_entry = monconf_action_entry_get_by_name(gui_data->curr_entry, action_name);
    if(!enabled)
    {
      if(action_entry)
      {	
	monconf_entry_remove_action_entry(gui_data->curr_entry, action_entry);	
	move_next=0;
      }
    }	
    else
    {
      if(!action_entry) 
      {
	action_entry = monconf_entry_new_action(gui_data->curr_entry);
	action_entry->action = monconf_action_get_by_name(conf, action_name);	
      }
      monconf_action_entry_add_globs_from_csv(action_entry, globs);
      action_entry->events =
	(event_create ? MON_CREATE : 0)
	| (event_modify ? MON_MODIFY: 0)
	| (event_delete ? MON_DELETE : 0)
	| (event_attribs ? MON_ATTRIB : 0)
	| (event_move_from ? MON_MOVED_FROM : 0)
	| (event_move_to ? MON_MOVED_TO : 0);		
    }    
    g_free(action_name);
    g_free(globs); 
    valid = gtk_tree_model_iter_next(model, &iter);
  }
  
  populate_entries(gui_data);  
}

gboolean check_event_log(gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;    
  zmq_msg_t message;
  int msgsize;
  char *strmsg;
  char has_msg ;
  zmq_msg_init(&message);
  has_msg = !zmq_recv(gui_data->sub_event_log, &message, ZMQ_NOBLOCK);  
  gboolean found_msg = FALSE;
  while(has_msg) 
  {   
    found_msg = TRUE;
    msgsize = zmq_msg_size(&message);                      
    strmsg = (char *)malloc(msgsize);
    memcpy(strmsg, zmq_msg_data(&message), msgsize);
    GtkTextIter logIter;
    GtkTextMark *logMark = gtk_text_buffer_get_insert (gui_data->textbufferLog);
    gtk_text_buffer_get_iter_at_mark (gui_data->textbufferLog, &logIter, logMark);
    
    if (gtk_text_buffer_get_char_count(gui_data->textbufferLog))
	gtk_text_buffer_insert (gui_data->textbufferLog, &logIter, "\n", 1);

    gtk_text_buffer_insert (gui_data->textbufferLog, &logIter, strmsg, msgsize);      
    zmq_msg_close(&message);
    zmq_msg_init(&message);
    has_msg = !zmq_recv(gui_data->sub_event_log, &message, ZMQ_NOBLOCK);    
  }
  if(found_msg != gui_data->logged_event) 
  {
    gui_data->logged_event = found_msg;
    if(found_msg)
    {
      gtk_status_icon_set_from_pixbuf(gui_data->systrayIcon, gtk_image_get_pixbuf(gui_data->image_started_event));    
    }
    else
    {
      gtk_status_icon_set_from_pixbuf(gui_data->systrayIcon, gtk_image_get_pixbuf(gui_data->image_started));    
    }
  }
  if(gui_data->lstart.active)
    return TRUE;
  return FALSE;
}

void start_reading_event_log(struct s_gui_data *gui_data)
{
  gui_data->zmq_context = gui_data->lstart.zmq_context;
  gui_data->sub_event_log = zmq_socket(gui_data->zmq_context, ZMQ_SUB);    
  if(zmq_connect(gui_data->sub_event_log, "inproc://event_log") || zmq_setsockopt (gui_data->sub_event_log, ZMQ_SUBSCRIBE, "", 0))
  {
    fprintf(stderr, "Error %d opening the event log queue\n", errno);
    exit(-2);
    return;
  }
  g_timeout_add(500, check_event_log, (gpointer)gui_data);
}

void stop_reading_event_log(struct s_gui_data *gui_data)
{
  zmq_close(gui_data->sub_event_log);  
  gui_data->logged_event = FALSE;  
}

void on_action_entryClose_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  gtk_widget_hide(gui_data->windowEntry);  
}

void on_reactlist_start(struct s_gui_data *gui_data) 
{  
  gtk_action_set_label(gui_data->action_startPause, "Stop");    
  gtk_status_icon_set_from_pixbuf(gui_data->systrayIcon, gtk_image_get_pixbuf(gui_data->image_started));
  gtk_image_set_from_icon_name(gui_data->image_startStop, ICON_NAME_STOP, GTK_ICON_SIZE_BUTTON); 
  start_reading_event_log(gui_data);
}

void on_reactlist_stop(struct s_gui_data *gui_data) 
{
  gui_data->logged_event = FALSE;
  gtk_action_set_label(gui_data->action_startPause, "Start");
  gtk_status_icon_set_from_pixbuf(gui_data->systrayIcon, gtk_image_get_pixbuf(gui_data->image_stopped));
  gtk_image_set_from_icon_name(gui_data->image_startStop, ICON_NAME_START, GTK_ICON_SIZE_BUTTON); 
  stop_reading_event_log(gui_data);
}

void on_windowMain_window_state_event(GtkWidget *widget, GdkEventWindowState *event, gpointer *user_data) 
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
/*  if(event->new_window_state == GDK_WINDOW_STATE_ICONIFIED)
    gtk_widget_hide(gui_data->windowMain);  
  else
    gtk_widget_show_all(gui_data->windowMain);  
  */
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
void populate_entry_config(struct s_gui_data *gui_data)
{
  monconf_entry *entry = gui_data->curr_entry;
  if(gui_data->curr_entry)
  {
    char *ignores = string_join(entry->ignore_files);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui_data->builder,"entryIgnores")), ignores);
    free(ignores);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(gtk_builder_get_object(gui_data->builder,"filechooserPath")), gui_data->curr_entry->file_name);
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
    populate_entry_actions(gui_data, entry);
  } 
  else
  {
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui_data->builder,"entryIgnores")), "");
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
    populate_entry_actions(gui_data, NULL);
  }  
}

void populate_action_config(struct s_gui_data *gui_data)
{
  if(gui_data->curr_action)
  {
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui_data->builder,"entryActionName")), gui_data->curr_action->name);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(gtk_builder_get_object(gui_data->builder,"filechooserActionPath")), monconf_resolve_path(gui_data->curr_action->script));
  } 
  else
  {
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui_data->builder,"entryActionName")), NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(gtk_builder_get_object(gui_data->builder,"filechooserActionPath")),  NULL);
  }
  
}

void populate_entry_actions(struct s_gui_data *gui_data, monconf_entry *conf_entry)
{
  GtkTreeModel *modelEntries;  
  GtkTreeIter iter;
  monaction_entry *action_entry = NULL;
  monconf_action_entry *conf_action_entry = NULL;
  gtk_list_store_clear(gui_data->listStoreEntryActions);
  GList *action_keys = g_hash_table_get_values(gui_data->conf->actionMap);
  GList *item = g_list_first(action_keys);
  char *str_globs;
  
  while(item)
  {
    action_entry = (monaction_entry *)item->data;
    str_globs = NULL;
    if(conf_entry != NULL) 
    {
      conf_action_entry = monconf_action_entry_get_by_name(conf_entry, action_entry->name);    
      if(conf_action_entry != NULL)
	str_globs = string_join(conf_action_entry->globs);
    }    
    gtk_list_store_append(gui_data->listStoreEntryActions, &iter);
    gtk_list_store_set(gui_data->listStoreEntryActions,&iter,
           COL_ACTION_ENTRY_ENABLED, (gboolean)(conf_action_entry != NULL),       
	   COL_ACTION_ENTRY_NAME, (gchar *)action_entry->name,		       
           COL_ACTION_ENTRY_GLOBS, (conf_action_entry != NULL ? str_globs : ""),           
           COL_ACTION_ENTRY_CREATE, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_CREATE) : (gboolean)0),
           COL_ACTION_ENTRY_MODIFY, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_MODIFY) : (gboolean)0),
           COL_ACTION_ENTRY_DELETE, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_DELETE) : (gboolean)0),
           COL_ACTION_ENTRY_ATTRIBS, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_ATTRIB) : (gboolean)0),
           COL_ACTION_ENTRY_MOVED_FROM, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_MOVED_FROM) : (gboolean)0),
           COL_ACTION_ENTRY_MOVED_TO, (conf_action_entry != NULL ? (gboolean)(conf_action_entry->events & MON_MOVED_TO) : (gboolean)0),           		      
           -1
          );
    if(str_globs)
      free(str_globs);
    item = item->next;
  }  
  g_list_free(action_keys);
}

void on_action_entryDuplicate_activate(GtkAction *action, gpointer user_data)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;  
  monconf_entry    *dupl_entry;
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  
  selection = gtk_tree_view_get_selection(gui_data->treeviewEntries);
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gchar *path;    
    gtk_tree_model_get (model, &iter, COL_ENTRY_PATH, &path, -1);
    monconf_entry *entry = monconf_entry_get_by_path(gui_data->conf, path);
    dupl_entry = monconf_entry_duplicate(gui_data->conf, entry);    
    populate_entries(gui_data);
  }
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
      gui_data->curr_entry = NULL;
      populate_entry_config(gui_data);
      break;
    case EDIT_ACTION_MODIFY:    
      selection = gtk_tree_view_get_selection(gui_data->treeviewEntries);
      if (gtk_tree_selection_get_selected(selection, &model, &iter))
      {
	gchar *path;    
	gtk_tree_model_get (model, &iter, COL_ENTRY_PATH, &path, -1);
        monconf_entry *entry = monconf_entry_get_by_path(gui_data->conf, path);
	gui_data->curr_entry = entry;
	populate_entry_config(gui_data);
	g_free(path);
	show_window = 1;
      }
      break;
  }
  if(show_window) 
    gtk_widget_show(gui_data->windowEntry);
}

void show_action_window(struct s_gui_data *gui_data, int action_type)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;  
  char show_window = 0;  
  switch(action_type)
  {
    case EDIT_ACTION_ADD:            
      show_window = 1;
      gui_data->curr_action = NULL;
      populate_action_config(gui_data);
      break;
    case EDIT_ACTION_MODIFY:    
      selection = gtk_tree_view_get_selection(gui_data->treeviewActions);
      if (gtk_tree_selection_get_selected(selection, &model, &iter))
      {
	gchar *action_name;    
	gtk_tree_model_get (model, &iter, COL_ACTION_NAME, &action_name, -1);
        monaction_entry *entry = monconf_action_get_by_name(gui_data->conf, action_name);
	gui_data->curr_action = entry;
	populate_action_config(gui_data);
	g_free(action_name);
	show_window = 1;
      }
      break;
  }
  if(show_window) 
    gtk_widget_show(gui_data->windowAction);  
}

void on_action_mainAdd_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_action_activate(gui_data->action_entryAdd);
}

void on_action_mainModify_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_action_activate(gui_data->action_entryModify);
}

void on_action_mainDelete_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_action_activate(gui_data->action_entryDelete);
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
    dialog = gtk_message_dialog_new(GTK_WINDOW(gui_data->windowMain),
	      GTK_DIALOG_DESTROY_WITH_PARENT,
	      GTK_MESSAGE_QUESTION,
	      GTK_BUTTONS_YES_NO,
	      "Remove the entry '%s'?",path);
    gtk_window_set_title(GTK_WINDOW(dialog), "Remove Entry");
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
    {
      monconf_remove_entry(gui_data->conf, entry);
      populate_entries(gui_data);
    }
    gtk_widget_destroy(dialog);      
    g_free(path);
  }
}

void on_action_actionAdd_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  show_action_window(gui_data, EDIT_ACTION_ADD);
}

void on_action_actionModify_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  show_action_window(gui_data, EDIT_ACTION_MODIFY);
}

void on_action_actionDelete_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data; 
}

void on_action_actionSave_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;   
}

void on_action_actionClose_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data; 
  gtk_widget_hide(gui_data->windowAction);
}

void on_action_saveConfig_activate(GtkAction *action, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  monconf_save_config(gui_data->conf, NULL);
  GtkWidget *dialog;    
  dialog = gtk_message_dialog_new(GTK_WINDOW(gui_data->windowMain),
	    GTK_DIALOG_DESTROY_WITH_PARENT,
	    GTK_MESSAGE_INFO,
	    GTK_BUTTONS_OK,
	    "Config saved at '%s'",gui_data->conf->file_path);
  gtk_window_set_title(GTK_WINDOW(dialog), "Config Saved");
  gtk_dialog_run(GTK_DIALOG(dialog));  
  gtk_widget_destroy(dialog);     
}

void on_windowMain_destroy (GtkObject *object, gpointer user_data)
{   
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_action_activate(gui_data->action_mainExit);
}

void on_btnStartPause_clicked(GtkObject *object, gpointer user_data)
{
}

void on_systrayIcon_activate(GtkObject *object, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;
  gtk_action_activate(gui_data->action_toggleApplication);
}

void on_systrayIcon_popup_menu(GtkStatusIcon *statusIcon, guint button, guint activate_time, gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;     
}

int main (int argc, char *argv[])
{ 
  char *cwd;
  char *glade_path;
  struct s_gui_data data;  
  config_args args;
  args.config_path = NULL;
  monconf_parse_cli_args(&args, argc, argv);  
    
  monconf_prepare_config_directory();
  monconf *conf = monconf_create();
  monconf_read_config(conf, args.config_path);
  monconf_load_available_actions(conf);
  data.rstart.active = 0;
  data.lstart.active = 0;
  
  gtk_init (&argc, &argv);
  
  data.builder = gtk_builder_new ();
  glade_path = monconf_resolve_path("ui-glade/monarqui_gui.glade");
  gtk_builder_add_from_file (data.builder, glade_path, NULL);  
  g_free(glade_path);
  data.builder = GTK_BUILDER(data.builder);
  data.windowMain = GTK_WIDGET(gtk_builder_get_object (data.builder, "windowMain"));
  data.windowEntry = GTK_WIDGET(gtk_builder_get_object (data.builder, "windowEntry"));
  data.windowConfig = GTK_WIDGET(gtk_builder_get_object (data.builder, "windowConfig"));
  data.windowAction = GTK_WIDGET(gtk_builder_get_object (data.builder, "windowAction"));
  data.action_mainExit = GTK_ACTION(gtk_builder_get_object(data.builder, "action_mainExit"));
  data.action_configOpen = GTK_ACTION(gtk_builder_get_object(data.builder, "action_configOpen"));
  data.action_configClose = GTK_ACTION(gtk_builder_get_object(data.builder, "action_configClose"));
  data.action_startPause = GTK_ACTION(gtk_builder_get_object(data.builder, "action_startPause"));
  data.action_entryAdd = GTK_ACTION(gtk_builder_get_object(data.builder, "action_entryAdd"));
  data.action_entryModify = GTK_ACTION(gtk_builder_get_object(data.builder, "action_entryModify"));
  data.action_entryDelete = GTK_ACTION(gtk_builder_get_object(data.builder, "action_entryDelete"));
  data.action_actionAdd = GTK_ACTION(gtk_builder_get_object(data.builder, "action_actionAdd"));
  data.action_actionModify = GTK_ACTION(gtk_builder_get_object(data.builder, "action_actionModify"));
  data.action_actionDelete = GTK_ACTION(gtk_builder_get_object(data.builder, "action_actionDelete"));
  data.action_saveConfig = GTK_ACTION(gtk_builder_get_object(data.builder, "action_saveConfig"));  
  data.action_toggleApplication = GTK_ACTION(gtk_builder_get_object(data.builder, "action_toggleApplication"));
  data.action_systrayPopup = GTK_ACTION(gtk_builder_get_object(data.builder, "action_systrayPopup"));
  data.image_startStop = GTK_IMAGE(gtk_builder_get_object(data.builder,"image_startStop"));  
  data.systrayIcon = GTK_STATUS_ICON(gtk_builder_get_object(data.builder,"systrayIcon"));  
  data.args = &args;
  data.conf = conf;
  data.treeviewActions = GTK_TREE_VIEW(gtk_builder_get_object(data.builder,"treeviewActions"));
  data.treeviewEntries = GTK_TREE_VIEW(gtk_builder_get_object(data.builder,"treeviewEntries"));
  data.treeviewEntryActions = GTK_TREE_VIEW(gtk_builder_get_object(data.builder,"treeviewEntryActions"));
  data.listStoreActions = GTK_LIST_STORE(gtk_builder_get_object(data.builder,"listStoreActions"));  
  data.listStoreEntries = GTK_LIST_STORE(gtk_builder_get_object(data.builder,"listStoreEntries"));
  data.listStoreEntryActions = GTK_LIST_STORE(gtk_builder_get_object(data.builder,"listStoreEntryActions"));  
  
  data.image_started = GTK_IMAGE(gtk_builder_get_object(data.builder,"image_started"));
  data.image_started_event = GTK_IMAGE(gtk_builder_get_object(data.builder,"image_started_event"));
  data.image_stopped = GTK_IMAGE(gtk_builder_get_object(data.builder,"image_stopped"));
  data.logged_event = FALSE;
  data.textbufferLog = GTK_TEXT_BUFFER(gtk_builder_get_object(data.builder, "textbufferLog"));
  
  data.systrayMenu = GTK_MENU(gtk_menu_new());
  data.systrayMenuItem_startStop = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Start"));
  data.systrayMenuItem_quit = GTK_MENU_ITEM(gtk_menu_item_new_with_label("Quit"));  
  
  
  gtk_menu_shell_append(GTK_MENU_SHELL(data.systrayMenu), GTK_WIDGET(data.systrayMenuItem_startStop));
  gtk_menu_shell_append(GTK_MENU_SHELL(data.systrayMenu), GTK_WIDGET(data.systrayMenuItem_quit));
    
  gtk_action_set_label(data.action_startPause, "Start");
  gtk_builder_connect_signals (data.builder, (gpointer)&data);    
     
  gtk_image_set_from_icon_name(data.image_startStop, ICON_NAME_START, GTK_ICON_SIZE_BUTTON);          
  gtk_status_icon_set_from_pixbuf(data.systrayIcon, gtk_image_get_pixbuf(data.image_stopped));
  populate_entries(&data);  
  populate_actions(&data);
  gtk_widget_show (data.windowMain);                
  gtk_main ();
  g_object_unref (G_OBJECT (data.builder));
  free(args.config_path);
  return 0;
}

void populate_entries(struct s_gui_data *gui_data) 
{
  GtkTreeModel *modelEntries;
  GtkTreeIter iter;

  gtk_list_store_clear(gui_data->listStoreEntries);
  GList *item = g_list_first(gui_data->conf->entrylist);
  while(item)
  {
    monconf_entry *entry = (monconf_entry *)item->data;
    char *str_ignores = string_join(entry->ignore_files);
    gtk_list_store_append(gui_data->listStoreEntries, &iter);
    gtk_list_store_set(gui_data->listStoreEntries,&iter,
           COL_ENTRY_PATH, entry->file_name,
           COL_ENTRY_RECURSIVE, (gboolean)(entry->recursive),
           COL_ENTRY_EVENT_CREATE, (gboolean)(entry->events & MON_CREATE),
           COL_ENTRY_EVENT_MODIFY, (gboolean)(entry->events & MON_MODIFY),
           COL_ENTRY_EVENT_DELETE, (gboolean)(entry->events & MON_DELETE),
           COL_ENTRY_EVENT_ATTRIBS, (gboolean)(entry->events & MON_ATTRIB),
           COL_ENTRY_EVENT_MOVED_FROM, (gboolean)(entry->events & MON_MOVED_FROM),
           COL_ENTRY_EVENT_MOVED_TO, (gboolean)(entry->events & MON_MOVED_TO),
           COL_ENTRY_IGNORE, str_ignores,           
           -1
          );
    free(str_ignores);
    item = item->next;
  }
}

void populate_actions(struct s_gui_data *gui_data)
{
  GtkTreeIter iter;
  monaction_entry *action_entry;
  gtk_list_store_clear(gui_data->listStoreActions);
  GList *action_list = g_hash_table_get_values(gui_data->conf->actionMap);
  GList *item = g_list_first(action_list);
  while(item)
  {
    char *type_str;
    switch(action_entry->type)
    {
      case MON_ACT_SHELL:
	type_str = g_strdup(STR_ACT_SHELL);
	break;
      case MON_ACT_LOG:
	type_str = g_strdup(STR_ACT_LOG);
	break;
      default:
	type_str = g_strdup(STR_ACT_LUA);
	break;
    }
    action_entry = (monaction_entry *)item->data;
    gtk_list_store_append(gui_data->listStoreActions, &iter);
    gtk_list_store_set(gui_data->listStoreActions, &iter,
		       COL_ACTION_NAME, action_entry->name,
		       COL_ACTION_TYPE, type_str,
		       COL_ACTION_SCRIPT, action_entry->script,
		       -1);
    g_free(type_str);
    item = item->next;
  }
  g_list_free(action_list);
}

void toggle_cell_checkbox(GtkTreeView *treeview, GtkListStore *listStore, GtkCellRendererToggle *cell_renderer, gchar *path, int row_index)
{
  GtkTreeModel     *model;
  GtkTreeIter       iter;  
  gboolean active = (gtk_cell_renderer_toggle_get_active(cell_renderer) ? FALSE : TRUE);
  model = gtk_tree_view_get_model(treeview);
  gtk_tree_model_get_iter_from_string(model, &iter, path);  
  gtk_list_store_set(listStore,&iter,
           row_index, active,		    
	   -1);  
}

void on_entryAction_enabled_toggled(GtkCellRendererToggle *cell_renderer, gchar *path,gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  toggle_cell_checkbox(gui_data->treeviewEntryActions, gui_data->listStoreEntryActions, cell_renderer, path, COL_ACTION_ENTRY_ENABLED);
}

void on_entryAction_globs_edited(GtkCellRendererText *renderer, gchar *path,
                                                        gchar *new_text,
                                                        gpointer user_data)
{
  GtkTreeModel     *model;
  GtkTreeIter       iter; 
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  model = gtk_tree_view_get_model(gui_data->treeviewEntryActions);
  gtk_tree_model_get_iter_from_string(model, &iter, path);  
  gtk_list_store_set(gui_data->listStoreEntryActions,&iter,
           COL_ACTION_ENTRY_GLOBS, new_text,		    
	   -1);    
}

void on_entryAction_create_toggled(GtkCellRendererToggle *cell_renderer, gchar *path,gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  toggle_cell_checkbox(gui_data->treeviewEntryActions, gui_data->listStoreEntryActions, cell_renderer, path, COL_ACTION_ENTRY_CREATE);
}

void on_entryAction_modify_toggled(GtkCellRendererToggle *cell_renderer, gchar *path,gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  toggle_cell_checkbox(gui_data->treeviewEntryActions, gui_data->listStoreEntryActions, cell_renderer, path, COL_ACTION_ENTRY_MODIFY);
}

void on_entryAction_delete_toggled(GtkCellRendererToggle *cell_renderer, gchar *path,gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  toggle_cell_checkbox(gui_data->treeviewEntryActions, gui_data->listStoreEntryActions, cell_renderer, path, COL_ACTION_ENTRY_DELETE);
}

void on_entryAction_attribs_toggled(GtkCellRendererToggle *cell_renderer, gchar *path,gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  toggle_cell_checkbox(gui_data->treeviewEntryActions, gui_data->listStoreEntryActions, cell_renderer, path, COL_ACTION_ENTRY_ATTRIBS);
}

void on_entryAction_movefrom_toggled(GtkCellRendererToggle *cell_renderer, gchar *path,gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  toggle_cell_checkbox(gui_data->treeviewEntryActions, gui_data->listStoreEntryActions, cell_renderer, path, COL_ACTION_ENTRY_MOVED_FROM);
}

void on_entryAction_moveto_toggled(GtkCellRendererToggle *cell_renderer, gchar *path,gpointer user_data)
{
  struct s_gui_data *gui_data = (struct s_gui_data *)user_data;  
  toggle_cell_checkbox(gui_data->treeviewEntryActions, gui_data->listStoreEntryActions, cell_renderer, path, COL_ACTION_ENTRY_MOVED_TO);
}



