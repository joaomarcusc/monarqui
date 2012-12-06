#include "monarqui_cli.h"
#include "../reactlist/monconf.h"
#include "../reactlist/monwatch.h"
#include "../reactlist/monarqui_threads.h"
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

volatile sig_atomic_t usr_interrupt = 0;

void interrupt_main(int sig) 
{
  usr_interrupt = 1;
  printf("halding signal\n");  
}

int main(int argc, char **argv) 
{  
  config_args args;
  pthread_t rthread, lthread;
  reactstart rstart;
  liststart lstart; 
  int rinitstatus, linitstatus;
  void *rexitstatus, *lexitstatus;
  int ltr, rtr;    
  monconf *conf;  
  int signum;
  sigset_t oldmask, mask, pendingMask;
  monconf_prepare_config_directory();
  monconf_parse_cli_args(&args, argc, argv);
  conf = monconf_create();
  monconf_read_config(conf, args.config_path);
  sigemptyset(&mask);
  sigemptyset(&oldmask);
  sigaddset(&mask,SIGHUP);
  sigaddset(&mask,SIGSTOP);
  sigaddset(&mask,SIGQUIT);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);
  start_reactor_and_listener(conf, &rthread, &rstart, &rinitstatus, &lthread, &lstart, &linitstatus);
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
  stop_reactor_and_listener(&rthread, &rstart, &rexitstatus, &lthread, &lstart, &lexitstatus);
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
  printf("Exiting...\n");  
  fflush(stdout);
  fflush(stderr);
  
  monconf_free_cli_args(&args);
  return EXIT_SUCCESS;
}




