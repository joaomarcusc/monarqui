#include "monarqui_reactor.h"
#include "monconf.h"
#include "../common/monarqui_common.h"
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
  void *sub_socket;
  zmq_msg_t message;  
  int msgsize;
  int recvreply;  
  reactstart *start;  
  monevent evt;
  start = (reactstart *)startarg;
  printf("Waiting for events to react...\n");
    
  sub_socket = zmq_socket(start->zmq_context, ZMQ_SUB);    
  if(zmq_connect(sub_socket, "inproc://file_events") || zmq_setsockopt (sub_socket, ZMQ_SUBSCRIBE, NULL, 0))
  {
    start->active = 0;
    fprintf(stderr,"Error opening the reactor: %d\n",zmq_errno());
    pthread_exit(NULL);
    return;
  }               
  start->socket_connected = 1;
  int errornumber;
  while(!start->usr_interrupt) 
  {      
    zmq_msg_init(&message);
    if(!zmq_recv(sub_socket, &message, ZMQ_NOBLOCK)) {            
      msgsize = zmq_msg_size(&message);                    
      monevent_deserialize(zmq_msg_data(&message),msgsize,&evt);
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
        if(retval)
        {
          printf("Execution action %s for event %d on %s/%s:\n",evt.action_name, evt.event, evt.base_path, evt.file_path);
        }
        else 
        {
          fprintf(stderr, "FAILED execution action %s for event %d on %s/%s:\n",evt.action_name, evt.event, evt.base_path, evt.file_path);
        }
      } 
      free(evt.file_path);
      free(evt.action_name);
      free(evt.base_path);
      zmq_msg_close(&message);
    } else {      
      zmq_msg_close(&message);
      usleep(500);
    }    
  }
  zmq_close(sub_socket);
  printf("Closing reactor...\n");
  start->active = 0;
  start->socket_connected = 0;
  pthread_exit(NULL);
}
