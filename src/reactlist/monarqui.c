#include "monarqui.h"
#include "monconf.h"
#include "monwatch.h"
#include "monarqui_listener.h"
#include "monarqui_reactor.h"
#include "monarqui_common.h"
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

volatile sig_atomic_t usr_interrupt = 0;

void interrupt_main(int sig) 
{
  usr_interrupt = 1;
  printf("halding signal\n");  
}

int main(int argc, char **argv) 
{  
  pthread_t rthread, lthread;
  pthread_attr_t attr;
  reactstart rstart;
  liststart lstart; 
  void *rstatus, *lstatus;
  int ltr, rtr;    
  void *zmq_context;
  monconf *conf;  
  int signum;
  sigset_t oldmask, mask, pendingMask;
   
  sigemptyset(&mask);
  sigemptyset(&oldmask);
  sigaddset(&mask,SIGHUP);
  sigaddset(&mask,SIGSTOP);
  sigaddset(&mask,SIGQUIT);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);
    
  zmq_context = CREATE_ZMQ_CONTEXT();
  conf = monconf_create();
  monconf_read_config(conf,"config.xml");
  monconf_execute_preload_actions(conf);
  rstart.conf = conf;
  rstart.zmq_context = zmq_context;
  rstart.usr_interrupt = 0;
  rstart.active = 1;
  rstart.socket_opened = 0;
  lstart.conf = conf;
  lstart.zmq_context = zmq_context;
  lstart.usr_interrupt = 0;
  lstart.active = 1;
  
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  rtr = pthread_create(&rthread, &attr, run_reactor, (void *)&rstart);
  while(!rstart.socket_opened);
  ltr = pthread_create(&lthread, &attr, run_listener, (void *)&lstart);   
  pthread_detach(rthread);
  pthread_detach(lthread);
  while(1) 
  {
    sigpending(&pendingMask);
    if (sigismember(&pendingMask, SIGHUP) ||
	sigismember(&pendingMask, SIGQUIT) ||
	sigismember(&pendingMask, SIGSTOP)) {      
      break;
    }
    else
      usleep(10000);
  }
  rstart.usr_interrupt = 1;
  lstart.usr_interrupt = 1;
  pthread_join(rthread, &rstatus);
  pthread_join(lthread, &lstatus);
  monconf_free_config(conf);  
  DESTROY_ZMQ_CONTEXT(zmq_context);    
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
  printf("Exiting...\n");  
  fflush(stdout);
  fflush(stderr);
  return EXIT_SUCCESS;
}


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


