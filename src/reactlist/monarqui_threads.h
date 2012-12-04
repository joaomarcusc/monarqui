#ifndef _MONARQUI_THREADS_H
#define MONARQUI_THREADS_H
#include "monarqui_reactor.h"
#include "monarqui_listener.h"
#include <pthread.h>

void start_reactor_and_listener(pthread_t *rthread, reactstart *rstart, int *rstatus,
				pthread_t *lthread, liststart *lstart, int *lstatus);
void stop_reactor_and_listener(pthread_t *rthread, reactstart *rstart, void *rstatus,
				pthread_t *lthread, liststart *lstart, void *lstatus);
#endif