#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include <string.h>
#include <linux/inotify.h>
#include "monwatch.h"

void destroy_int_pointer(gpointer *data)
{
}

monwatch *monwatch_create() 
{  
  monwatch *watch = malloc(sizeof(struct s_monwatch));
  watch->entrylist = NULL;
  watch->num_entries = 0;
  watch->inotify_fd = inotify_init();
  int flags = fcntl(watch->inotify_fd, F_GETFL, 0);
  fcntl(watch->inotify_fd, F_SETFL, flags | O_NONBLOCK);
  fcntl(watch->inotify_fd, flags);
  watch->wdescr_map = g_hash_table_new(g_direct_hash, g_direct_equal);
  return watch;
}

void monwatch_entry_free_gfunc(gpointer data, gpointer user_data) 
{
  monwatch_entry_free((monwatch *)user_data, (monwatch_entry *)data);
}

void monwatch_entry_free(monwatch *watch, monwatch_entry *entry) 
{   
  inotify_rm_watch(watch->inotify_fd, entry->wdescr);
  g_free(entry->file_name);
  g_free(entry);
}


void monwatch_free(monwatch *watch) 
{
  int i;  
  monwatch_entry *entry;  
  g_list_foreach(g_list_first(watch->entrylist), monwatch_entry_free_gfunc, (gpointer)watch);
  g_list_free(watch->entrylist); 
  g_hash_table_destroy(watch->wdescr_map);  
  close(watch->inotify_fd);  
  free(watch);
}

void monwatch_process_config(monwatch *watch, monconf *conf)
{    
  monconf_entry *entry;
  GList *item = g_list_first(conf->entrylist);
  while(item) 
  {
    entry = (monconf_entry *)item->data;
    _monwatch_config_iterator_aux(entry->file_name, entry, watch, 0);
    item = item->next;
  }
}

void _monwatch_config_iterator_aux(char *file_name, monconf_entry *entry, monwatch *watch, int depth)
{  
  struct dirent * dir_entry;
  //TODO: avoid buffer overflow here
  char *subdir_name;  
  DIR *dir;    
  if(depth > entry->max_depth)
    return;
  if(access(file_name, F_OK))
    return;
  dir = opendir(file_name);  
  if(!dir) 
    return;
  if(!monwatch_should_watch(entry, file_name))
    return;
  _monwatch_add_item(watch, entry, file_name);    
  if(entry->recursive) 
  {
    dir_entry = readdir(dir);    
    while(dir_entry)
    {        
      if(dir_entry->d_type & DT_DIR && 
	dir_entry->d_name[0] != '.')
      {      
	if(!monconf_entry_match_ignores(entry, dir_entry->d_name))
	{
	  subdir_name = g_strdup_printf("%s/%s",file_name,dir_entry->d_name);      	
	  _monwatch_config_iterator_aux(subdir_name, entry, watch, depth+1);      
	  g_free(subdir_name);
	}
      }
      dir_entry = readdir(dir);
    }    
    free(dir_entry);
  }  
  free(dir);
}
  
void _monwatch_add_item(monwatch *watch, monconf_entry *conf_entry, char *dir_path)
{
  int inomask;
  monwatch_entry *watch_entry = monwatch_new_entry(watch);
  inomask = mask_mon_to_inotify(conf_entry->events);
  watch_entry->file_name = g_strdup(dir_path);  
  if(conf_entry->recursive)
    inomask = inomask | IN_CREATE | IN_DELETE;
  watch_entry->inomask = inomask;
  watch_entry->wdescr = inotify_add_watch(watch->inotify_fd, dir_path, inomask);    
  watch_entry->conf_entry = conf_entry;
  g_hash_table_insert(watch->wdescr_map, (gpointer)(watch_entry->wdescr), watch_entry);
  printf("Watching directory %s\n", dir_path);  
}

monwatch_entry *monwatch_new_entry(monwatch *watch)
{  
  monwatch_entry *entry;    
  entry = (monwatch_entry *)malloc(sizeof(struct s_monwatch_entry));
  watch->entrylist = g_list_append(watch->entrylist,(gpointer)entry);
  watch->num_entries++;
  return entry;
}

short monwatch_should_watch(monconf_entry *entry, char *dir_path)
{
  return 1;
}

int monwatch_num_entries(monwatch *watch) 
{
  return watch->num_entries;
}

void monwatch_iterate(monwatch *watch, GFunc func, gpointer user_data)
{
  g_list_foreach(watch->entrylist, func, user_data);
}

int mask_mon_to_inotify(int mask)
{
  int inomask = 0;
  if(mask & MON_CREATE)
    inomask = inomask | IN_CREATE;
  if(mask & MON_MODIFY)
    inomask = inomask | IN_MODIFY;
  if(mask & MON_DELETE)
    inomask = inomask | IN_DELETE;
  if(mask & MON_ACCESS)
    inomask = inomask | IN_ACCESS;
  if(mask & MON_ATTRIB)
    inomask = inomask | IN_ATTRIB;
  if(mask & MON_MOVED_FROM)
    inomask = inomask | IN_MOVED_FROM;
  if(mask & MON_MOVED_TO)
    inomask = inomask | IN_MOVED_TO;
  return inomask;
}

monwatch_entry *monwatch_new_entry_duplicated(monwatch *watch, monwatch_entry *source)
{
  monwatch_entry *entry = monwatch_new_entry(watch);
  entry->conf_entry = source->conf_entry;
  return entry;
}

void _monwatch_delete_item(monwatch *watch, monwatch_entry *entry, char *dir_path)
{
  char *full_path;    
  int len_full_path;
  monwatch_entry *curr_entry;
  full_path = g_strdup_printf("%s/%s",entry->file_name, dir_path);
  len_full_path = strlen(full_path);
  GList *item = g_list_first(watch->entrylist);    
  while(item) 
  {  
    curr_entry = (monwatch_entry *)item->data;
    if(!strncmp(curr_entry->file_name, full_path, len_full_path))
    {
      printf("No longer watching directory %s\n",curr_entry->file_name);
      inotify_rm_watch(watch->inotify_fd, curr_entry->wdescr);
      item = g_list_remove(item, curr_entry);
    }
    if(item)
      item = item->next;
  }
  g_free(full_path);    
}

void monevent_serialize(monevent *event, char **buffer, int *buffer_size)
{    
  FILE *out = open_memstream(buffer, buffer_size);
  size_t len;
  len = strlen(event->base_path);
  fwrite(&len, sizeof(size_t), 1, out);
  fwrite(event->base_path, sizeof(char), len, out);
  len = strlen(event->file_path);
  fwrite(&len, sizeof(size_t), 1, out);
  fwrite(event->file_path, sizeof(char), len, out);
  len = strlen(event->action_name);
  fwrite(&len, sizeof(size_t), 1, out);
  fwrite(event->action_name, sizeof(char), len, out);
  fwrite(&(event->event), sizeof(int), 1, out);
  fwrite(&(event->is_dir), sizeof(int), 1, out);
  fwrite(&(event->timestamp), sizeof(int), 1, out);  
  fflush(out);
  fclose(out);
  
}
void monevent_deserialize(char *buffer, int buffer_size, monevent *event)
{
  FILE *in = fmemopen(buffer, buffer_size, "rb");  
  size_t len;
  fread(&len, sizeof(size_t), 1, in);  
  len++;
  event->base_path= (char *)calloc(sizeof(char),len);  
  fread(event->base_path, sizeof(char), len-1, in);
  event->base_path[len] = 0;

  fread(&len, sizeof(size_t), 1, in);
  len++;
  event->file_path= (char *)calloc(sizeof(char),len);
  fread(event->file_path, sizeof(char), len-1, in);
  event->file_path[len] = 0;
  
  fread(&len, sizeof(size_t), 1, in);
  len++;
  event->action_name= (char *)calloc(sizeof(char),len);
  fread(event->action_name, sizeof(char), len-1, in);  
  event->action_name[len] = 0;
  
  fread(&(event->event), sizeof(int), 1, in);
  fread(&(event->is_dir), sizeof(int), 1, in);
  fread(&(event->timestamp), sizeof(int), 1, in);
  fclose(in);
}
