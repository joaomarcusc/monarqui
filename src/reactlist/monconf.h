#ifndef _MONCONF_H
#define _MONCONF_H
#include <glib.h>
#include <stdlib.h>
#include "../common/monarqui_common.h"
#include "monaction.h"
#include <glob.h>
#include <lua.h>
#include <lauxlib.h>

typedef struct s_monconf_action_entry 
{
  monaction_entry *action;
  char *filter_glob;
  int num_globs;
  GList *globs;
  int events;
} monconf_action_entry;

typedef struct s_monconf_entry
{
  char *file_name;    
  int events;
  short recursive;
  short max_depth;    
  int num_actions;    
  GList *actions;    
  int num_ignore_files;
  GList *ignore_files;
  
} monconf_entry;

typedef struct s_monconf
{
  int num_entries;
  GList *entrylist;      
  GHashTable *actionMap;
} monconf;

typedef struct s_config_args 
{
  char *config_path;  
  monconf *conf;
} config_args;

monconf *monconf_create();
void monconf_read_config(monconf *conf,const char *cfg_file);
void monconf_free(monconf *conf);
void monconf_free_entry(monconf_entry *entry);
void monconf_free_entry_gfunc(gpointer data, gpointer user_data);
void monconf_free_action_entry(monconf_action_entry *action);
void monconf_free_action_entry_gfunc(gpointer data, gpointer user_data); 
int monconf_num_entries(monconf *conf);
monconf_action_entry *monconf_entry_new_action(monconf_entry *conf_entry);
monconf_entry *monconf_new_entry(monconf *conf);
monconf_entry *monconf_nth_entry(monconf *conf, int n);
monaction_entry *monconf_new_action(monconf *conf, const char *str_name);
void add_str_to_g_list(GList *list, const char *data);
int str_events_to_int(char *str);
void monconf_foreach(monconf *conf, GFunc func, gpointer user_data);
void monconf_entry_add_ignores_from_csv(monconf_entry *entry, char *csv_data);
void monconf_action_entry_add_glob(monconf_action_entry *action_entry, char *data);
void monconf_action_entry_add_globs_from_csv(monconf_action_entry *entry, char *csv_data);
void monaction_free_entry(monaction_entry *action);
void monaction_free_entry_gfunc(gpointer data, gpointer user_data);
void monconf_dump(monconf *conf);
void monconf_execute_preload_actions(monconf *conf);
int monconf_action_match_entry_globs(monconf_action_entry *action_entry, const char *full_path);
int monconf_entry_match_ignores(monconf_entry *conf_entry, const char *file_name);
void monconf_prepare_config_directory();
void monconf_parse_cli_args(config_args *args,int argc, char **argv);
void monconf_free_cli_args(config_args *args);
void monconf_find_config(config_args *args);

#endif
