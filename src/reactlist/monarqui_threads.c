#include "monarqui_threads.h"
#include "../reactlist/monconf.h"
#include "../reactlist/monwatch.h"
#include "../reactlist/monarqui_listener.h"
#include "../reactlist/monarqui_reactor.h"
#include "../common/monarqui_common.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/inotify.h>
#include <glib.h>

void start_reactor_and_listener(struct config_args *args, pthread_t* rthread, reactstart* rstart, int *rstatus, pthread_t* lthread, liststart* lstart, int *lstatus)
{
  pthread_attr_t attr;
  int ltr, rtr;    
  monconf *conf;  
  int signum;      
  
  conf = monconf_create();
  monconf_read_config(conf,args->config_path);
  monconf_execute_preload_actions(conf);
  rstart->conf = conf;
  rstart->zmq_context = zmq_init(0);
  rstart->usr_interrupt = 0;
  rstart->active = 1;
  rstart->socket_connected = 0;
  lstart->conf = conf;
  lstart->zmq_context = rstart->zmq_context;
  lstart->usr_interrupt = 0;
  lstart->active = 1;
  lstart->socket_bound = 0;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  *lstatus = pthread_create(lthread, &attr, run_listener, (void *)lstart);   
  while(!lstart->socket_bound);
  *rstatus = pthread_create(rthread, &attr, run_reactor, (void *)rstart);

}

void stop_reactor_and_listener(pthread_t* rthread, reactstart* rstart, void *rstatus, pthread_t* lthread, liststart* lstart, void *lstatus)
{
  rstart->usr_interrupt = 1;
  lstart->usr_interrupt = 1;
  while(rstart->active || lstart->active);
  monconf_free(lstart->conf); 
  monwatch_free(lstart->watch);
  zmq_close(lstart->zmq_context);      
  rstart->usr_interrupt = 0;
  rstart->active = 0;
  rstart->socket_connected = 0;
  lstart->usr_interrupt = 0;
  lstart->active = 0;
  lstart->socket_bound = 0;
}
