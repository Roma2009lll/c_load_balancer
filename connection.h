#ifndef CONNECTION_H
#define CONNECTION_H

#include "backend.h"
#include<fcntl.h>
#include <time.h>
#include<errno.h>
#include<sys/epoll.h>
#include <stdlib.h>
#define BUFFER_SIZE 4096
typedef struct {
    int clientFd;
    int backendFd;
    Backend* backend;
    char clientBuffer[BUFFER_SIZE];
    int clientBufferLen;
    char backendBuffer[BUFFER_SIZE];
    int backendBufferLen;
    time_t created;
    int closing;
} Connection;

typedef struct {
    Connection** connections;
    int maxConnections;
    BackendPool* backendPool;
    int epollFd;
} ConnectionManager;

ConnectionManager* managerCreate(BackendPool* pool, int maxCon);
Connection* connectionCreate(ConnectionManager* mgr,int client_fd, Backend* backend);
Connection*findFd(ConnectionManager* mgr, int fd);
void connectionDestroy(ConnectionManager* mgr, Connection* con);
void freeManager(ConnectionManager* mgr);
void setNonBlock(int fd);

#endif