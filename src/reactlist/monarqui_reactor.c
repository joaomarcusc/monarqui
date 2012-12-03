#include "monarqui_reactor.h"
#include "monconf.h"
#include "monarqui_common.h"
#include <zmq.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <lua.h>
#include <lauxlib.h>

void *run_reactor(void *startarg)
{  
  lua_State *L;
  monaction_entry *action_entry;
  void *pull_socket;
  zmq_msg_t message;  
  int msgsize;
  int recvreply;  
  reactstart *start;  
  char msgcontent[2048];
  monevent evt;
  // TODO: message buffer size shouldn't be fixed
  start = (reactstart *)startarg;
  printf("Waiting for events to react...\n");
    
  pull_socket = zmq_socket(start->zmq_context, ZMQ_PULL);
  zmq_bind(pull_socket, ZMQ_QUEUE_NAME);
  start->socket_opened = 1;
  while(!start->usr_interrupt) 
  {    
    zmq_msg_init(&message);    
    recvreply = RECV_ZMQ_MESSAGE(&message, pull_socket, ZMQ_NOBLOCK);    
    if(!recvreply) {      
      msgsize = zmq_msg_size(&message);                  
      memcpy(&evt,zmq_msg_data(&message),msgsize);      
      action_entry = (monaction_entry *)g_hash_table_lookup(start->conf->actionMap,evt.action_name);
      if(action_entry)
      {
        L = action_entry->luaState;
        lua_getglobal(L, "event_action");    
        lua_pushinteger(L, evt.event);
        lua_pushstring(L, evt.base_path);
        lua_pushstring(L, evt.file_path);
        lua_pushinteger(L, evt.timestamp);
        if (lua_pcall(L, 4, 1, 0))         
	    show_lua_error(L, "lua_pcall() failed");     
        int retval = lua_toboolean(L,-1);
	lua_pop(L, 1);      
	if(!retval)
	{
	  printf("Execution action %s for event %d on %s/%s:\n",evt.action_name, evt.event, evt.base_path, evt.file_path);
	}
	else 
	{
	  fprintf(stderr, "FAILED execution action %s for event %d on %s/%s:\n",evt.action_name, evt.event, evt.base_path, evt.file_path);
	}
      } 
    } else {
      usleep(500);
    }
    zmq_msg_close(&message);
  }
  zmq_close(pull_socket);
  printf("Closing reactor...\n");
  start->active = 0;
  start->socket_opened = 0;
  pthread_exit(NULL);
}
