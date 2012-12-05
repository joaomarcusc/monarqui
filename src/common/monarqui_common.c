#include "monarqui_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include <lua.h>
#include <zmq.h>

int str_events_to_int(char *str) 
{  
  char *tmpstr = strdup(str);
  char *token, *rest;
  int events = 0;      
  token = strtok_r(tmpstr, STR_EVENT_SEPARATOR, &rest);
  while(token) 
  {    
    if(strcmp(token,STR_CREATE)==0)
      events = events | MON_CREATE;
    else if(strcmp(token,STR_MODIFY)==0)
      events = events | MON_MODIFY;
    else if(strcmp(token,STR_DELETE)==0)
      events = events | MON_DELETE;
    else if(strcmp(token,STR_ATTRIB)==0)
      events = events | MON_ATTRIB;
    else if(strcmp(token,STR_MOVED_FROM)==0)
      events = events | MON_MOVED_FROM;
    else if(strcmp(token,STR_MOVED_TO)==0)
      events = events | MON_MOVED_TO;
    else if(strcmp(token,STR_ACCESS)==0)
      events = events | MON_ACCESS;
    token = strtok_r(NULL, STR_EVENT_SEPARATOR, &rest);
  }  
  free(tmpstr);
  tmpstr = NULL;
  return events;
}

void show_lua_error(lua_State *L, char *msg){
	fprintf(stderr, "\nFATAL ERROR:\n  %s: %s\n\n",
		msg, lua_tostring(L, -1));
}
void bail(lua_State *L, char *msg){
	show_lua_error(L,msg);
	exit(1);
}

void monarqui_prepare_config_directory()
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
  }
  g_free(config_dir);
}
