#include "../common/monarqui_common.h"
#include "monaction.h"
#include <dirent.h>
#include "monconf.h"
#include <string.h>
#include <fnmatch.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <lua.h>
#include <lauxlib.h>
#include <unistd.h>

monconf *monconf_create()
{
  monconf *conf;
  conf = malloc(sizeof(struct s_monconf));
  conf->entrylist = NULL;  
  conf->actionMap = g_hash_table_new(g_str_hash, g_str_equal);    
  conf->num_entries = 0;
  return conf;
}

monconf_entry *monconf_new_entry(monconf *conf) 
{  
  monconf_entry *entry;    
  entry = (monconf_entry *)malloc(sizeof(struct s_monconf_entry));
  entry->recursive = 1;
  entry->max_depth = 1000;
  entry->num_actions = 0;
  entry->actions = NULL;  
  entry->num_ignore_files = 0;
  entry->ignore_files = NULL;
  conf->entrylist = g_list_append(conf->entrylist,(gpointer)entry);
  conf->num_entries++;
  return entry;
}
  
monconf_entry *monconf_entry_duplicate(monconf *conf, monconf_entry *entry)
{
  GList *item;
  monconf_entry *dupl = monconf_new_entry(conf);
  dupl->file_name = g_strdup(entry->file_name);
  dupl->events = entry->events;
  dupl->recursive = entry->recursive;  
  char *csv = string_join(entry->ignore_files);
  monconf_entry_add_ignores_from_csv(dupl, csv);
  free(csv);
  item = g_list_first(entry->actions);
  while(item)
  {
    monconf_action_entry *dupl_action;
    monconf_action_entry *entry_action = (monconf_action_entry *)item->data;
    dupl_action = monconf_entry_new_action(dupl);
    dupl_action->action = entry_action->action;
    dupl_action->events = entry_action->events;
    dupl_action->num_globs = entry_action->num_globs;
    if(entry_action->filter_glob)
      dupl_action->filter_glob = strdup(entry_action->filter_glob);
    csv = string_join(entry_action->globs);
    monconf_action_entry_add_globs_from_csv(dupl_action,csv);
    free(csv);
    item = item->next;
  }
 
  return dupl;
}

monconf_action_entry *monconf_entry_new_action(monconf_entry *conf_entry)
{
  monconf_action_entry *action_entry;
  action_entry = (monconf_action_entry *)malloc(sizeof(struct s_monconf_action_entry));  
  action_entry->filter_glob= NULL;
  action_entry->globs = NULL;
  conf_entry->actions = g_list_append(conf_entry->actions, (gpointer)action_entry);
  conf_entry->num_actions++;
  return action_entry;
}

void monconf_remove_entry(monconf *conf, monconf_entry *entry)
{
  conf->entrylist = g_list_remove(g_list_first(conf->entrylist),entry);
  monconf_free_entry(entry);
}

void monconf_entry_remove_action_entry(monconf_entry *conf_entry, monconf_action_entry *action_entry)
{
  conf_entry->actions= g_list_remove(g_list_first(conf_entry->actions),action_entry);
  monconf_free_action_entry(action_entry);
}

void monconf_read_config(monconf *conf,const char *cfg_file)
{  
  monconf_entry *entry;
  monaction_entry *action_entry, action_tmp;
  monconf_action_entry *conf_action;
  int num_entries, i;   
  xmlDocPtr doc;
  xmlNode *node, *currNode, *actionNode, *actionChildren;
  xmlXPathContextPtr xctx; 
  xmlXPathObjectPtr xobj;
  conf->file_path = g_strdup(cfg_file);
  xmlInitParser();
  
  doc = xmlReadFile(cfg_file, NULL, 0);  
  if(doc == NULL) 
  {
    //TODO: Tratar erro ao ler o arquivo
    fprintf(stderr,"Error reading config file\n");
    exit(0);
    return;
  }
  xctx = xmlXPathNewContext(doc);
  if(xctx == NULL)
  {
    //TODO: Tratar erro 
    fprintf(stderr,"Error parsing config file\n");    
    xmlFreeDoc(doc);
    exit(0);
    return;
  }  
  xobj = xmlXPathEvalExpression("/config/entries/entry", xctx);
  for(i=0;i<xobj->nodesetval->nodeNr;i++)
  {
    node = xobj->nodesetval->nodeTab[i]->children;    
    currNode = node;
    entry = monconf_new_entry(conf);           
    while(currNode) 
    {      
      if(currNode->type == XML_ELEMENT_NODE)
      {	
	if(!strcmp(currNode->name,"path"))
	  entry->file_name= g_strdup(currNode->children->content);
	else if(!strcmp(currNode->name,"recursive"))
	  entry->recursive = (!strcmp(currNode->children->content,"true") ? 1 : 0);
	else if(!strcmp(currNode->name,"events"))
	  entry->events = str_events_to_int(currNode->children->content);
	else if(!strcmp(currNode->name,"ignores")) {	  
	  monconf_entry_add_ignores_from_csv(entry, currNode->children->content);
	}
	else if(!strcmp(currNode->name,"actions")) 
	{
	  actionNode = currNode->children;
	  while(actionNode)
	  {
	    if(actionNode-> type == XML_ELEMENT_NODE)
	    {
	      conf_action = monconf_entry_new_action(entry);
	      actionChildren = actionNode->children;
	      while(actionChildren)
	      {
		if(actionNode-> type == XML_ELEMENT_NODE)
		{
		  if(!strcmp(actionChildren->name,"name"))
		  {
		    conf_action->action = monconf_action_get_by_name(conf, actionChildren->children->content);
		    if(conf_action->action == NULL) 
		    {
		      conf_action->action = monconf_new_action(conf, actionChildren->children->content);
		    }
		  }
		  if(!strcmp(actionChildren->name,"events") && actionChildren->children != NULL && actionChildren->children->content != NULL)
		    conf_action->events = str_events_to_int(actionChildren->children->content);
		  else if(!strcmp(actionChildren->name,"globs")) 
		  {
		    if(actionChildren->children) 
		    {
		      conf_action->filter_glob = g_strdup(actionChildren->children->content);
		      monconf_action_entry_add_globs_from_csv(conf_action,conf_action->filter_glob);
		    }
		  }		  
		}
		actionChildren = actionChildren->next;
	      }	      
	    }
	    actionNode = actionNode->next;
	  }
	}
      }	      
      currNode = currNode->next;
    }
  }
  xmlXPathFreeContext(xctx);
  xmlXPathFreeObject(xobj);
  xmlFreeDoc(doc);
  xmlCleanupParser();
}

void string_free_gfunc(gpointer data, gpointer user_data)
{
  free((char *)data);
}

void monconf_free(monconf *conf) 
{
  int i;
  monconf_entry *entry;  
  GList *item;
  int num_entries = monconf_num_entries(conf);
  monaction_entry *action_entry;  
  g_list_foreach(g_list_first(conf->entrylist), &monconf_free_entry_gfunc, NULL);
  GList *keys = g_hash_table_get_values(conf->actionMap);  
  item = g_list_first(keys);
  while(item)
  {
    action_entry = (monaction_entry *)item->data;
    monaction_free_entry(action_entry);
    item = item->next;
  }
  g_free(conf->file_path);
  g_list_free(keys);
  g_hash_table_destroy(conf->actionMap);
  g_list_free(conf->entrylist); 
  free(conf);
} 

void monaction_free_entry(monaction_entry *action)
{  
  monaction_close_state(action);
  g_free(action->name);
  g_free(action->script);
  g_free(action);
}

void monaction_free_entry_gfunc(gpointer data, gpointer user_data)
{
  monaction_free_entry((monaction_entry *)data);
}
void monconf_free_entry_gfunc(gpointer data, gpointer user_data)
{
  monconf_free_entry((monconf_entry *)data);
}

void monconf_free_action_entry(monconf_action_entry *action)
{  
  g_list_foreach(g_list_first(action->globs),string_free_gfunc,NULL);
  g_list_free(action->globs);
  g_free(action->filter_glob);     
  g_free(action);  
}

monconf_action_entry *monconf_action_entry_get_by_name(monconf_entry *entry, const char *action_name)
{
  GList *item = g_list_first(entry->actions);
  while(item)
  {
    if(!strcmp(((monconf_action_entry *)item->data)->action->name,action_name))
      return (monconf_action_entry *)item->data;
    item = item->next;
  }
  return NULL;
}

void monconf_free_action_entry_gfunc(gpointer data, gpointer user_data) 
{
  monconf_free_action_entry((monconf_action_entry *) data);
}

void monconf_free_entry(monconf_entry *entry)
{   
  monconf_action_entry *action;  
  g_list_foreach(g_list_first(entry->actions), &monconf_free_action_entry_gfunc, NULL);
  g_list_foreach(g_list_first(entry->ignore_files), &string_free_gfunc, NULL);
  g_list_free(g_list_first(entry->actions));
  g_list_free(g_list_first(entry->ignore_files));
  g_free(entry->file_name);
  g_free(entry);
}

int monconf_num_entries(monconf *conf)
{
  return conf->num_entries;
}

monconf_entry *monconf_nth_entry(monconf *conf, int n) 
{
  return (monconf_entry *)g_list_nth_data(conf->entrylist,n);
}

void monconf_foreach(monconf *conf, GFunc func, gpointer user_data)
{
  g_list_foreach(g_list_first(conf->entrylist), func, user_data);
}
  
monaction_entry *monconf_new_action(monconf *conf, const char *str_name) 
{
  monaction_entry *entry = malloc(sizeof(monaction_entry));
  entry->name = g_strdup(str_name);  
  entry->state_initialized = 0;
  g_hash_table_insert(conf->actionMap,entry->name,entry);
  return entry;
}

void monconf_entry_add_ignore(monconf_entry *entry, char *data)
{
  entry->ignore_files = g_list_append(entry->ignore_files, g_strdup(data));
  entry->num_ignore_files++;
}

void monconf_action_entry_add_glob(monconf_action_entry *action_entry, char *data)
{
  action_entry->globs = g_list_append(action_entry->globs, g_strdup(data));
  action_entry->num_globs++;
}

void monconf_entry_add_ignores_from_csv(monconf_entry *entry, char *csv_data)
{
  GList *list_item;
  list_item = g_list_first(entry->ignore_files);
  while(list_item)
  {
    free((char *)list_item->data);
    list_item = list_item->next;
  }
  g_list_free(g_list_first(entry->ignore_files));
  entry->ignore_files = NULL;
  char *tmpdata = strdup(csv_data);
  char *token, *rest;
  int size = 0;
  token = strtok_r(tmpdata, STR_EVENT_SEPARATOR, &rest);  
  while(token) 
  {     
    monconf_entry_add_ignore(entry,token);
    token = strtok_r(NULL, STR_EVENT_SEPARATOR, &rest);
  }  	  
  free(tmpdata);
}

void monconf_action_entry_add_globs_from_csv(monconf_action_entry *entry, char *csv_data)
{
  GList *list_item;
  list_item = g_list_first(entry->globs);
  while(list_item)
  {
    free((char *)list_item->data);
    list_item = list_item->next;
  }
  g_list_free(g_list_first(entry->globs));
  entry->globs = NULL;
  char *tmpdata = strdup(csv_data);
  char *token, *rest;
  int size = 0;
  token = strtok_r(tmpdata, STR_EVENT_SEPARATOR, &rest);  
  while(token) 
  {     
    monconf_action_entry_add_glob(entry,token);
    token = strtok_r(NULL, STR_EVENT_SEPARATOR, &rest);
  }  	  
  free(tmpdata);
}

void monconf_dump(monconf *conf)
{
  monconf_entry *entry_ptr;
  monaction_entry *action_ptr;
  monconf_action_entry *action_entry_ptr;
  GList *action_names, *ignore_item, *action_item, *glob_item;
  char *str_item;
  printf("Number of entries: %d\n",conf->num_entries);
  GList *entry_item = g_list_first(conf->entrylist);
  while(entry_item)
  {
    entry_ptr = (monconf_entry *)entry_item->data;
    printf("============================\n");
    printf("Entry\n");
    printf("File/Dir name: %s\n",(char *)entry_ptr->file_name);
    printf("# of Ignores: %d\n",entry_ptr->num_ignore_files);
    printf("# of Actions: %d\n",entry_ptr->num_actions);    
    ignore_item = g_list_first(entry_ptr->ignore_files);
    while(ignore_item)
    {
      printf("Ignore: %s\n",(char *)ignore_item->data);      
      ignore_item = ignore_item->next;
    }
    action_item = g_list_first(entry_ptr->actions);
    while(action_item)
    {
      printf("---------------------------------\n");
      printf("Action\n");
      action_entry_ptr = (monconf_action_entry *)action_item->data;
      printf("Name: %s\n",action_entry_ptr->action->name);
      printf("Filter Glob: %s\n",action_entry_ptr->filter_glob);
      glob_item = g_list_first(action_entry_ptr->globs);
      while(glob_item) 
      {
	printf("Glob: %s\n",(char *)glob_item->data);
	glob_item = glob_item->next;
      }
      action_item = action_item->next;
    }        
    entry_item = entry_item->next;
  }
  printf("==============================\n");
  printf("Registered Actions\n");  
  action_names = g_hash_table_get_keys(conf->actionMap);
  action_item = g_list_first(action_names);
  while(action_item)
  {
    action_ptr = (monaction_entry *)g_hash_table_lookup(conf->actionMap,action_item->data);
    printf("------------------\n");
    printf("Registerd Action\n");
    printf("Name: %s\n",action_ptr->name);
    printf("Script: %s\n",action_ptr->script);
    printf("Script: %d\n",action_ptr->type);
    action_item = action_item->next;
  }
  g_list_free(action_names);
}

void monconf_initialize_scripts(monconf *conf)
{
  lua_State *L;
  monconf_action_entry *conf_action_entry_ptr;
  monaction_entry *action_ptr;
  monconf_entry *entry_ptr;  
  GList *action_names = g_hash_table_get_keys(conf->actionMap);
  GList *action_item = g_list_first(action_names);
  while(action_item)
  {
    action_ptr = (monaction_entry *)g_hash_table_lookup(conf->actionMap,action_item->data); 
    monaction_close_state(action_ptr);
    monaction_init_state(action_ptr);
    L = action_ptr->luaState;
    lua_getglobal(L, "initialize");    
    lua_pushnil(L);
    if (lua_pcall(L, 1, 1, 0))         
	bail(L, "lua_pcall() failed");     
    lua_pop(L, 1);
    action_item = action_item->next;
  }  
  GList *entry_item = g_list_first(conf->entrylist);
  GList *action_entry_item;
  while(entry_item)
  {
    entry_ptr = (monconf_entry *)entry_item->data;
    action_entry_item = g_list_first(entry_ptr->actions);
    while(action_entry_item)
    {
      conf_action_entry_ptr = (monconf_action_entry *)action_entry_item->data;
      action_ptr = conf_action_entry_ptr->action;
      L = action_ptr->luaState;
      lua_getglobal(L, "conf_action_preload");
      lua_pushstring(L,(char *)entry_ptr->file_name);
      if (lua_pcall(L, 1, 1, 0))         
	  bail(L, "lua_pcall() failed");           
      lua_pop(L, 1);
      action_entry_item = action_entry_item->next;
    }
    entry_item = entry_item->next;
  }

  g_list_free(action_names);
}

int monconf_action_match_entry_globs(monconf_action_entry *action_entry, const char *full_path)
{
  GList *glob_item = g_list_first(action_entry->globs);
  if(!glob_item) 
  {
    return 1;
  }
  else 
  {
    int matched_glob = 0;
    while(glob_item)
    {
      char *glob_data = (char *)glob_item->data;				
      if(!glob_data || !strlen(glob_data) || !fnmatch(glob_data,full_path, FNM_PERIOD))
      {
	matched_glob = 1;
	break;
      }	      		
      glob_item = glob_item->next;
    }
    return matched_glob;
  }	  
}

int monconf_entry_match_ignores(monconf_entry *conf_entry, const char *file_name)
{
  GList *item = g_list_first(conf_entry->ignore_files);
  while(item)
  {
    if(!strcmp(file_name, (char *)item->data))
      return 1;
    item = item->next;
  }
  return 0;
}

void monconf_prepare_config_directory()
{
  struct stat st;
  char *home = getenv("HOME");
  char *config_dir;
  char *config_file_path;
  config_dir = g_strdup_printf("%s/.monarqui", home);
  if(stat(config_dir, &st) < 0)
  {
    printf("Config directory %s does't exist, creating...\n",config_dir);
    mkdir(config_dir, 0750);
  }
  config_file_path = g_strdup_printf("%s/config.xml",config_dir);
  if(stat(config_file_path, &st) < 0)
  {
    printf("Creating config file under %s\n...",config_file_path);
    FILE *fconfig = fopen(config_file_path,"w+");
    fprintf(fconfig,"<?xml version=\"1.0\"?>\n");
    fprintf(fconfig,"<config>\n");
    fprintf(fconfig,"    <actions>\n");
    fprintf(fconfig,"    </actions>\n");
    fprintf(fconfig,"    <entries>\n");
    fprintf(fconfig,"    </entries>\n");
    fprintf(fconfig,"</config>\n");
    fclose(fconfig);   
  }
  g_free(config_file_path);
  g_free(config_dir);
}

void monconf_parse_cli_args(config_args*args,int argc, char **argv)
{  
  args->config_path = NULL;
  while(1)
  {
    static struct option options[] = {
      {"config", 1, 0, 'c'}
    };
    int option_index = 0;
    int c;
    c = getopt_long (argc, argv, "c",
		      options, &option_index);
    if(c == -1)
      break;
    switch(c) 
    {
      case 'c':
	args->config_path = g_strdup_printf("%s",optarg);
	break;
    }  
  }
  if(!args->config_path)
    monconf_find_config(args);
  if(!args->config_path)
  {
    fprintf(stderr,"Couldn't find the config.xml file\n");
    exit(1);
  }
    
}

void monconf_free_cli_args(config_args*args)
{
  if(args->config_path)
    g_free(args->config_path);
}

void monconf_find_config(config_args*args)
{  
  struct stat st;
  char *home;
  char *config_file_path;
    
  if(stat("config.xml",&st) >= 0)
  {
    args->config_path = g_strdup("config.xml");    
  }
  else 
  {
    home = getenv("HOME");
    config_file_path = g_strdup_printf("%s/.monarqui/config.xml", home);  
    if(stat(config_file_path,&st) < 0)
    {
      FILE *fconfig = fopen(config_file_path,"w+");
      fprintf(fconfig,"<?xml version=\"1.0\"?>\n");
      fprintf(fconfig,"<config>\n");
      fprintf(fconfig,"    <actions>\n");
      fprintf(fconfig,"    </actions>\n");
      fprintf(fconfig,"    <entries>\n");
      fprintf(fconfig,"    </entries>\n");
      fprintf(fconfig,"</config>\n");
      fclose(fconfig);      
    }
    args->config_path = g_strdup(config_file_path);      
    free(config_file_path);    
  }    
}

monconf_entry *monconf_entry_get_by_path(monconf *conf, char *path)
{
  GList *item = g_list_first(conf->entrylist);
  while(item)
  {
    if(!strcmp(((monconf_entry *)item->data)->file_name,path))
      return (monconf_entry *)item->data;
    item = item->next;
  }
  return NULL;
}

monaction_entry *monconf_action_get_by_name(monconf *conf, const char *action_name)
{
  return (monaction_entry *)g_hash_table_lookup(conf->actionMap, action_name);
}

char *monconf_resolve_path(const char *path)
{
  struct stat st;
  char *home;
  char *cwd;
  char *temp_file_path;
  char *real_path = NULL;
  home = getenv("HOME");
  if(stat(path,&st) >= 0)
  {
    real_path = g_strdup(path);
  }
  if(!real_path)
  {

    temp_file_path = g_strdup_printf("%s/.monarqui/%s", home, path);  
    if(stat(temp_file_path,&st) >= 0)
    {
      real_path = temp_file_path;
    }
    else
    {
      g_free(temp_file_path);     
    }
  }
  if(!real_path)
  {
    temp_file_path = g_strdup_printf("/usr/share/monarqui/%s", home, path);  
    if(stat(temp_file_path,&st) >= 0)
    {
      real_path = temp_file_path;
    }
    else
    {
      g_free(temp_file_path);     
    }    
  }  
  if(!real_path)
  {
    temp_file_path = g_strdup_printf("/usr/local/share/monarqui/%s", home, path);  
    if(stat(temp_file_path,&st) >= 0)
    {
      real_path = temp_file_path;
    }
    else
    {
      g_free(temp_file_path);     
    }   
  }  
  if(!real_path)
  {
    temp_file_path = g_strdup_printf("/opt/monarqui/%s", home, path);  
    if(stat(temp_file_path,&st) >= 0)
    {
      real_path = temp_file_path;
    }
    else
    {
      g_free(temp_file_path);     
    }
  }    
  if(!real_path)
  {
    cwd = getcwd(NULL,0);
    temp_file_path = g_strdup_printf("%s/%s", cwd, path);
    if(stat(temp_file_path,&st) >= 0)
    {
      real_path = temp_file_path;
    }
    else
    {
      g_free(temp_file_path);     
    }
    free(cwd);
  }    
  return real_path;
}

char *get_file_name(const char *full_file_name) {
  char *tmp;    
  const char *start_pos = strrchr(full_file_name, '/');    
  const char *end_pos= strrchr(full_file_name, '.');  
    
  if(start_pos == NULL)
    start_pos = full_file_name;
  else
    start_pos++;
  int size = 0;  
  if(end_pos != NULL)
  {
    for(tmp=start_pos;tmp!=end_pos;tmp++)
      size++;
  }
  char *file_name = (char *)calloc(size+1,sizeof(char));
  strncpy(file_name, start_pos, size);
  return file_name;
}

const char *get_file_extension(const char *full_file_name) {
    const char *dot_pos= strrchr(full_file_name, '.');
    if(!dot_pos|| dot_pos == full_file_name) return "";
    return dot_pos + 1;
}

gboolean monconf_file_is_script(const char *path)
{
  const char *ext = get_file_extension(path);
  return !strcmp(ext,"lua");
}

void monconf_load_actions_from_dir(monconf *conf, const char *path)
{
  monaction_entry *action;
  struct dirent * dir_entry;
  char *subdir_name;  
  DIR *dir;    
  if(access(path, F_OK))
    return;
  dir = opendir(path);  
  if(!dir) 
    return;
  dir_entry = readdir(dir);    
  while(dir_entry)
  {        
    if(dir_entry->d_type & DT_DIR && 
      dir_entry->d_name[0] != '.')
    {      
      subdir_name = g_strdup_printf("%s/%s",path,dir_entry->d_name);      	
      monconf_load_actions_from_dir(conf, subdir_name);      
      g_free(subdir_name);     
    } 
    else if(dir_entry->d_type & DT_REG)
    {      
      char *full_path= g_strdup_printf("%s/%s",path,dir_entry->d_name);
      char *action_name;
      if(monconf_file_is_script(full_path))
      {
		
	action_name = get_file_name(dir_entry->d_name);
	action = monconf_action_get_by_name(conf, action_name);
	if(action == NULL)	
	  action = monconf_new_action(conf, action_name);
	
	action->type = MON_ACT_LUA;
	action->script = g_strdup(full_path);	
	free(action_name);
      }
      g_free(full_path);
      
    }
//    free(dir_entry);
    dir_entry = readdir(dir);
  }  
  free(dir);  
}

void monconf_load_available_actions(monconf *conf)
{
  struct stat st;
  char *home;
  char *cwd;
  char *temp_file_path;  
  home = getenv("HOME");

  temp_file_path = g_strdup_printf("%s/.monarqui/", home);  
  if(stat(temp_file_path,&st) >= 0)
  {    
    monconf_load_actions_from_dir(conf, temp_file_path);
  }
  g_free(temp_file_path);     
  temp_file_path = NULL;  
  
  temp_file_path = g_strdup("/usr/share/monarqui/actions");  
  if(stat(temp_file_path,&st) >= 0)
  {    
    monconf_load_actions_from_dir(conf, temp_file_path);
  }
  g_free(temp_file_path);     
  temp_file_path = NULL;  
  
  temp_file_path = g_strdup("/usr/local/share/monarqui/actions");  
  if(stat(temp_file_path,&st) >= 0)
  {    
    monconf_load_actions_from_dir(conf, temp_file_path);
  }
  g_free(temp_file_path);     \
  temp_file_path = NULL; 
  
  temp_file_path = g_strdup_printf("/opt/monarqui/actions", home);  
  if(stat(temp_file_path,&st) >= 0)
  {    
    monconf_load_actions_from_dir(conf, temp_file_path);
  }
  g_free(temp_file_path);     
  temp_file_path = NULL; 
      
  cwd = getcwd(NULL,0);
  temp_file_path = g_strdup_printf("%s/actions", cwd);
    if(stat(temp_file_path,&st) >= 0)
  {    
    monconf_load_actions_from_dir(conf, temp_file_path);
  }
  g_free(temp_file_path);     
  temp_file_path = NULL; 
  free(cwd);
}

void monconf_save_config(monconf *conf, const char *file_path)
{
  monconf_entry *entry;  
  monconf_action_entry *conf_action;
  GList *entry_item, *action_entry_item;
  int num_entries, i;   
  xmlDocPtr doc;
  xmlNode *config_node, *entries_node, *entry_node, *entry_actions_node, *entry_action_node;
  xmlXPathContextPtr xctx; 
  xmlXPathObjectPtr xobj;
  
  doc = xmlNewDoc(BAD_CAST "1.0");
  config_node = xmlNewNode(NULL, BAD_CAST "config");
  xmlDocSetRootElement(doc, config_node);
  entries_node = xmlNewChild(config_node, NULL, BAD_CAST "entries", NULL);
  
  entry_item = g_list_first(conf->entrylist);
  while(entry_item)
  {
    entry = (monconf_entry *)entry_item->data;
    char *ignores = string_join(entry->ignore_files);
    char *events = int_events_to_str(entry->events);    
    entry_node = xmlNewChild(entries_node, NULL, BAD_CAST "entry", NULL);
    xmlNewChild(entry_node, NULL, BAD_CAST "path", BAD_CAST (entry->file_name));
    xmlNewChild(entry_node, NULL, BAD_CAST "recursive", BAD_CAST (entry->recursive ? "true" : "false"));
    xmlNewChild(entry_node, NULL, BAD_CAST "events", BAD_CAST events);
    xmlNewChild(entry_node, NULL, BAD_CAST "ignores", BAD_CAST ignores);  
    entry_actions_node = xmlNewChild(entry_node, NULL, BAD_CAST "actions", NULL);  
    action_entry_item = g_list_first(entry->actions);
    while(action_entry_item)
    {      
      conf_action = (monconf_action_entry *)action_entry_item->data;
      char *action_events = int_events_to_str(conf_action->events);
      char *globs = string_join(conf_action->globs);
      entry_action_node = xmlNewChild(entry_actions_node, NULL, BAD_CAST "action", NULL);
      xmlNewChild(entry_action_node, NULL, BAD_CAST "name", conf_action->action->name);
      xmlNewChild(entry_action_node, NULL, BAD_CAST "events", action_events);
      xmlNewChild(entry_action_node, NULL, BAD_CAST "globs", globs);      
      free(action_events);
      free(globs);
      action_entry_item = action_entry_item->next;      
    }
    entry_item = entry_item->next;
    free(ignores);
    free(events);
  }
  FILE *f = fopen(file_path == NULL ? conf->file_path : file_path,"w+");
  xmlDocDump(f,doc);
  fclose(f);
  xmlFreeDoc(doc);  
}