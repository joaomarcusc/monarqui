#ifndef _MONARQUI_LISTENER_H
#define _MONARQUI_LISTENER_H
#include "monconf.h"
#include "monwatch.h"
typedef struct s_liststart
{  
  void *zmq_context;   
  monconf *conf;
  int usr_interrupt;
  int active;  
  monwatch *watch;
  short socket_bound; 
} liststart;


void *run_listener(void *startarg);
int get_monmask(int inotify_mask, int event_mask);
#endif