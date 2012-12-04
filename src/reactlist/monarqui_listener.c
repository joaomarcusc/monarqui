#include "monarqui_listener.h"
#include <zmq.h>
#include <stdio.h>
#include <linux/inotify.h>
#include <string.h>
#include <time.h>
#include <fnmatch.h>
#include <unistd.h>
#include <pthread.h>
#include "monarqui_common.h"
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 32 * ( EVENT_SIZE + 16 ) )
int check_filters_for_event(struct inotify_event *event, monwatch_entry *watch_entry);

void *run_listener(void *startarg) 
{
  GList *action_item, *glob_item;
  monwatch *watch;
  monwatch_entry *entry, *new_entry;
  monconf_action_entry *action_entry;
  int newstr_len;
  char buffer[EVENT_BUF_LEN];
  int elength, i, num_entries;
  int idx, msgsize;
  short matched_glob;
  monevent evt;  
  lua_State *L;  
  char *msgbuf;
  size_t msgbufsize;

  liststart *start = (liststart *)startarg;  
  watch = monwatch_create();
  start->watch = watch;
  monwatch_process_config(watch,start->conf);
  printf("Listening for events...\n");
  num_entries = monwatch_num_entries(watch);
 
  void *pub_socket = zmq_socket(start->zmq_context, ZMQ_PUB);
  if(zmq_bind(pub_socket, "inproc://file_events"))
  {
    start->active = 0;
    fprintf(stderr,"Error opening the inotify listener: %d\n",zmq_errno());
    pthread_exit(NULL);
    return;
  }    
  start->socket_bound = 1;
  zmq_msg_t message;
  
  while(!start->usr_interrupt)
  {
    idx++;
    elength = read(watch->inotify_fd, buffer, EVENT_BUF_LEN);
    i = 0;
    while(i < elength) 
    {
      struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
      if(event->len)
      {			
	entry = (monwatch_entry *)g_hash_table_lookup(watch->wdescr_map,(gpointer)event->wd);		
	char *full_path = g_strdup_printf("%s/%s",entry->file_name,event->name);
	evt.event = check_event(event->mask,entry->conf_entry->events);	
	if(evt.event && check_filters_for_event(event, entry)) 
	{
	  if(event->mask & IN_ISDIR && ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO))
	      && (entry->conf_entry->recursive)) 
	  {
	    _monwatch_add_item(watch, entry->conf_entry, full_path);
	  }	  	  
  	  action_item = g_list_first(entry->conf_entry->actions);
	  while(action_item)
	  {
	    action_entry = (monconf_action_entry *)action_item->data;	    
	    if(evt.event & entry->conf_entry->events)
	    {      
	      if(!monconf_entry_match_ignores(entry->conf_entry,event->name)
		&& monconf_action_match_entry_globs(action_entry, full_path))
	      {
		evt.action_name = action_entry->action->name;
		evt.timestamp = time(NULL);
		evt.base_path = entry->file_name;
		evt.file_path = event->name;		
		evt.is_dir = ((event->mask & IN_ISDIR) ? 1 : 0);
		zmq_msg_init(&message);				
		monevent_serialize(&evt, &msgbuf, &msgbufsize);
		zmq_msg_init_size(&message,msgbufsize);		
		memcpy(zmq_msg_data(&message), msgbuf, msgbufsize);			
		if(zmq_send(pub_socket, &message, 0))
		{
		  fprintf(stderr,"Error %d sending message\n",zmq_errno());
		}		
		free(msgbuf);		
		zmq_msg_close(&message);	      
	      }
	    }
	    action_item = action_item->next;
	  }
	  g_free(full_path);
	}		
	if(event->mask & IN_ISDIR && ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)))
	{
	  _monwatch_delete_item(watch, entry, event->name);
	}			
      }      
      i += EVENT_SIZE + event->len;
    }
    usleep(500);
  }
  printf("Closing listener...\n");
  zmq_close(pub_socket);
  start->socket_bound = 0;
  start->active = 0;
  pthread_exit(NULL);
}

int check_event(int inotify_mask, int event_mask)
{
  int retmask = 0;
  if(event_mask & MON_CREATE && (inotify_mask & IN_CREATE))
    retmask = retmask | MON_CREATE;
  if(event_mask & MON_MODIFY && (inotify_mask & IN_MODIFY))
    retmask = retmask | MON_MODIFY;
  if(event_mask & MON_DELETE && (inotify_mask & IN_DELETE))
    retmask = retmask | MON_DELETE;  
  if(event_mask & MON_ATTRIB && (inotify_mask & IN_ATTRIB))
    retmask = retmask | MON_ATTRIB;
  if(event_mask & MON_ACCESS && (inotify_mask & IN_ACCESS))
    retmask = retmask | MON_ACCESS;  
  if(event_mask & MON_MOVED_FROM && (inotify_mask & IN_MOVED_FROM))
    retmask = retmask | MON_MOVED_FROM;  
  if(event_mask & MON_MOVED_TO && (inotify_mask & IN_MOVED_TO))
    retmask = retmask | MON_MOVED_TO;  
  return retmask;
}

int check_filters_for_event(struct inotify_event *event, monwatch_entry *watch_entry)
{
  int num_ignores, i;
  GList *item = g_list_first(watch_entry->conf_entry->ignore_files);
  while(item)
  {
    if(!strcmp(event->name,(char *)(item->data)))
      return 0;
    item = item->next;
  }
  return 1;
}