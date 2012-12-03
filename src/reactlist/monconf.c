#include "monarqui_common.h"
#include "monaction.h"
#include "monconf.h"
#include <string.h>
#include <fnmatch.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <lua.h>
#include <lauxlib.h>

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
  xobj = xmlXPathEvalExpression("/config/actions/action", xctx);
  for(i=0;i<xobj->nodesetval->nodeNr;i++)
  {
    node = xobj->nodesetval->nodeTab[i]->children;
    currNode = node;
    action_tmp.script = NULL;
    while(currNode) 
    {      
      if(currNode->type == XML_ELEMENT_NODE)
      {	
	if(!strcmp(currNode->name,"name"))
	  strncpy(action_tmp.name, currNode->children->content, 64); 	  
	else if(!strcmp(currNode->name,"script"))
	  action_tmp.script = currNode->children->content;
	else if(!strcmp(currNode->name,"type")) 
	{
	  if(!strcmp(currNode->children->content,STR_ACT_SHELL))	  
	    action_tmp.type = MON_ACT_SHELL;
	  else if(!strcmp(currNode->children->content,STR_ACT_LUA))	  
	    action_tmp.type = MON_ACT_LUA;
	  else if(!strcmp(currNode->children->content,STR_ACT_LOG))	  
	    action_tmp.type = MON_ACT_LOG;
	}
      }	
      currNode = currNode->next;
    }
    action_entry = monconf_new_action(conf, action_tmp.name);
    action_entry->type = action_tmp.type;
    action_entry->script = g_strdup(action_tmp.script);  
    monaction_init_state(action_entry);
  }
  xmlXPathFreeObject(xobj);  
  xmlXPathFreeContext(xctx);
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
	else if(!strcmp(currNode->name,"ignore_files")) {	  
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
		    conf_action->action = (monaction_entry*)g_hash_table_lookup(conf->actionMap, actionChildren->children->content);
		  if(!strcmp(actionChildren->name,"events"))
		    conf_action->events = str_events_to_int(actionNode->children->content);
		  else if(!strcmp(actionChildren->name,"filter_glob")) 
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
      printf("%s\n",currNode->name);
      monconf_dump(conf);
      currNode = currNode->next;
    }
  }
  monconf_dump(conf);
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
  GList *keys = g_hash_table_get_keys(conf->actionMap);  
  item = g_list_first(keys);
  while(item)
  {
    action_entry = (monaction_entry *)item->data;
    monaction_free_entry(action_entry);
    item = item->next;
  }
  g_list_free(keys);
  g_hash_table_destroy(conf->actionMap);
  g_list_free(conf->entrylist); 
  free(conf);
} 

void monaction_free_entry(monaction_entry *action)
{
  lua_close(action->luaState);
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
  strncpy(entry->name, str_name, 64);  
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

void monconf_execute_preload_actions(monconf *conf)
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
    L = action_ptr->luaState;
    lua_getglobal(L, "initialize");    
    lua_pushnil(L);
    if (lua_pcall(L, 1, 0, 0))         
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
      if (lua_pcall(L, 1, 0, 0))         
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
