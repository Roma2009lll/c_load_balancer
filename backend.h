#ifndef BACKEND_H
#define BACKEND_H

#include <stdio.h>
#include <string.h> 
#include<stdbool.h>
#include<time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum {
    ALG_ROUND_ROBIN,
    ALG_LEAST_CONNECTIONS
} Algorithm;

typedef struct {
    char host[64];
    int port;
    bool healthy;              
    int activeConnections;    
    int failures;  
    int successes; 
    time_t lastCheck;  
    pthread_mutex_t lock;
} Backend;

typedef struct {
    Backend* backends;        
    int count;               
    int capacity;              
    int current_index;         
    pthread_mutex_t lock;     
    Algorithm alg;             
} BackendPool;

BackendPool* createPoll(Algorithm alg);
void poolAdd(BackendPool* pool, const char* host, int port);
Backend* poolSelect(BackendPool* pool);
Backend* roundRobin(BackendPool* pool);
Backend* leastCon(BackendPool* pool);
void healthChek(Backend* backEnd);
void conOpend(Backend* b);
void conClosed(Backend* b);
void freeePoll (BackendPool* pool);
void healthCheacks(void* argv);

#endif