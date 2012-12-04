#include "monarqui_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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