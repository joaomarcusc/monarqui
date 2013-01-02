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
  void *pub_log_socket;
  zmq_msg_t message, evt_message;
  int msgsize;
  int recvreply;  
  reactstart *start;  
  monevent evt;
  start = (reactstart *)startarg;  
  pub_log_socket = zmq_socket(start->zmq_context, ZMQ_PUB);  
  printf("Binding the event log...\n");
  if(zmq_bind(pub_log_socket, "inproc://event_log"))
  {
    start->active = 0;
    fprintf(stderr,"Error %d binding the event log queue\n",zmq_errno());
    pthread_exit(NULL);
    exit(-2);
    return;    
  }  
  sub_socket = zmq_socket(start->zmq_context, ZMQ_SUB);    
  if(zmq_connect(sub_socket, "inproc://file_events") || zmq_setsockopt (sub_socket, ZMQ_SUBSCRIBE, "", 0))
  {
    start->active = 0;
    fprintf(stderr,"Error %d connecting to the file events queue\n",zmq_errno());
    pthread_exit(NULL);
    exit(-2);
    return;    
  }               
  start->socket_connected = 1;
  printf("Waiting for events to react...\n");
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
	  char *event_str;
	  char *evt_type_str = int_events_to_str(evt.event);
	  event_str = g_strdup_printf("Action: %s, Event: %s, Path: %s/%s",evt.action_name, evt_type_str, evt.base_path, evt.file_path);	  
	  free(evt_type_str);
	  zmq_msg_init(&evt_message);
	  int event_str_len = strlen(event_str);
	  zmq_msg_init_size(&evt_message, event_str_len);
	  memcpy(zmq_msg_data(&evt_message), event_str, event_str_len);
	  if(zmq_send(pub_log_socket, &evt_message, 0))
	  {
	    fprintf(stderr,"Error %d sending event log message\n",zmq_errno());
	  }
	  zmq_msg_close(&evt_message);
	  g_free(event_str);	
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
  zmq_close(pub_log_socket);
  printf("Closing reactor...\n");
  start->active = 0;
  start->socket_connected = 0;
  pthread_exit(NULL);
}
