#ifndef _MONARQUI_REACTOR_H
#define _MONARQUI_REACTOR_H
#include "monconf.h"
#include "monwatch.h"
#include <zmq.h>

typedef struct s_reactstart
{  
  void *zmq_context;   
  monconf *conf;  
  int usr_interrupt;
  int active;
  int socket_opened;
} reactstart; 

void *run_reactor(void *startarg);
#endif