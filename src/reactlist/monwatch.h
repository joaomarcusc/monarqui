#ifndef _MONWATCH_H_
#define _MONWATCH_H_
#include <glib.h>
#include <dirent.h>
#include "monconf.h"

typedef struct s_monwatch_entry
{
  char *file_name;  
  int wdescr;
  int inomask;
  monconf_entry *conf_entry;    
} monwatch_entry;

typedef struct s_monwatch
{    
  int num_entries;
  GList *entrylist;
  GHashTable *wdescr_map;  
  int inotify_fd;
} monwatch;

typedef struct s_monwatch_event 
{
  char *file_name;
  int type;
  int timestamp;  
} monwatch_event;

typedef struct s_monevent
{
  char *base_path;  
  char *file_path;  
  int event;
  int timestamp;
  int is_dir; 
  char *action_name; 
} monevent;

monwatch *monwatch_create();
void monwatch_process_config(monwatch *watch, monconf *conf);
void monwatch_free(monwatch *watch);
void monwatch_entry_free(monwatch *watch, monwatch_entry *entry);
void _monwatch_config_iterator_aux(char *file_name, monconf_entry *entry, monwatch *watch, int depth);
void _monwatch_add_item(monwatch *watch, monconf_entry *conf_entry, char *dir_path);
void _monwatch_delete_item(monwatch *watch, monwatch_entry *entry, char *dir_path);
monwatch_entry *monwatch_new_entry(monwatch *watch);
monwatch_entry *monwatch_new_entry_duplicated(monwatch *watch, monwatch_entry *source);
short monwatch_should_watch(monconf_entry *entry, char *dir_path);
int monwatch_num_entries(monwatch *watch);
void monwatch_iterate(monwatch *watch, GFunc func, gpointer user_data);
int mask_mon_to_inotify(int mask);

void monevent_serialize(monevent *event, char **buffer, int *buffer_size);
void monevent_deserialize(char *buffer, int buffer_size, monevent *event);
#endif