#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include<pthread.h>
#include "connection.h"
#include"backend.h"
#include"config.h"

#define PORT 9000
#define MAX_CON 10000
#define BACKEND_COUNT 3
#define MAX_EVENTS 100

void acceptNewClient(int serverFd,BackendPool* mainPool,ConnectionManager* mainManager);
int LoadBalancerInit(BackendPool** mainPool,ConnectionManager** mainManager,int* listenFd,struct sockaddr_in* ptr,Config* conf);
void handleSendingDataCL(int curFd,BackendPool* mainPool,ConnectionManager* mainManager,Connection* conCheack);
void handleSendingDataBk(int curFd,BackendPool* mainPool,ConnectionManager* mainManager,Connection* conCheack);

int main(void){
    //gcc main.c backend.c connection.c -o load_balancer -Wall -pthread
    //./load_balancer
    Config conf;
    BackendPool* mainPool=NULL;
    ConnectionManager* mainManager=NULL;
    int listenFd=-1;
    struct sockaddr_in serverAddr={
        .sin_family=AF_INET,
        .sin_port=htons(PORT),
        .sin_addr.s_addr=INADDR_ANY
    };
    memset(&conf, 0, sizeof(Config));
    parseFile("config.txt",&conf);
    if((LoadBalancerInit(&mainPool,&mainManager,&listenFd,&serverAddr,&conf))==1){
        return 1;
    }
    pthread_t thread;
    if(pthread_create(&thread,NULL,healthCheacks,mainPool)!=0){
        return 1;
    }
    pthread_detach(thread);
    printf("Initialization finished\n");
    acceptNewClient(listenFd,mainPool,mainManager);
    return 0;
}

void acceptNewClient(int serverFd,BackendPool* mainPool,ConnectionManager* mainManager){
    struct epoll_event events[MAX_EVENTS];
    while(1){
        int n=epoll_wait(mainManager->epollFd,&events,MAX_EVENTS,-1);
        if(n<0){
            printf("Epoll_wait failed\n");
            return;
        }
        for(int i=0;i<n;i++){
            int curFd=events[i].data.fd;
            if(curFd==serverFd){
                while(1){
                    struct sockaddr_in clientNewAddr;
                    socklen_t len=sizeof(clientNewAddr);
                    int curFd=accept(serverFd,(struct sockaddr*)&clientNewAddr,&len);
                    if(curFd<0){
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; 
                        } else {
                            perror("accept failed");
                            break;
                        }
                    }
                    Backend* b=poolSelect(mainPool);
                    if(b==NULL){
                        printf("No backends avaliable\n");
                        continue;
                    }
                    Connection* newCon=connectionCreate(mainManager,curFd,b);
                    if(newCon==NULL){
                        continue;
                    }
                    struct epoll_event clientEv={
                        .data.fd=newCon->clientFd,
                        .events=EPOLLIN | EPOLLET
                    };
                    epoll_ctl(mainManager->epollFd,EPOLL_CTL_ADD,newCon->clientFd,&clientEv);
                    struct epoll_event backEv={
                        .data.fd=newCon->backendFd,
                        .events=EPOLLIN | EPOLLET
                    };
                    epoll_ctl(mainManager->epollFd,EPOLL_CTL_ADD,newCon->backendFd,&backEv);
                    printf("Client logged on port%d\n",b->port);
                }
            }else{
                Connection* conCheack=findFd(mainManager,curFd);
                if(conCheack==NULL){
                    continue;
                }
                uint32_t curEv=events[i].events;
                if (curEv & (EPOLLERR | EPOLLHUP)) {
                    connectionDestroy(mainManager, conCheack);
                    continue;
                }
                if(conCheack->backendFd==curFd&&(curEv&EPOLLOUT)){
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(curFd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                        printf("Backend  failed\n", curFd);
                        connectionDestroy(mainManager,conCheack);
                        continue;
                    }
                    struct epoll_event backEv = {
                        .data.fd = conCheack->backendFd,
                        .events = EPOLLIN | EPOLLET 
                    };
                    if (epoll_ctl(mainManager->epollFd, EPOLL_CTL_MOD, conCheack->backendFd, &backEv) < 0) {
                        connectionDestroy(mainManager, conCheack);
                        continue;
                    }
                }
                if(conCheack->clientFd == curFd && (curEv & EPOLLIN)){
                    handleSendingDataCL(curFd,mainPool,mainManager,conCheack);
                }
                else if(conCheack->backendFd == curFd && (curEv & EPOLLIN)){
                    handleSendingDataBk(curFd,mainPool,mainManager,conCheack);
                }
            }
        }
    }
}

void handleSendingDataCL(int curFd,BackendPool* mainPool,ConnectionManager* mainManager,Connection* conCheack){
    while(1){
        int spaceLeft=BUFFER_SIZE-conCheack->clientBufferLen;
        if(spaceLeft<=0){
            printf("Buffer on %d  ovverride\n",conCheack->clientFd);
            memset(conCheack->clientBuffer,0,BUFFER_SIZE);
            conCheack->clientBufferLen = 0; 
            spaceLeft = BUFFER_SIZE;
        }
        int bytes=recv(conCheack->clientFd,conCheack->clientBuffer+conCheack->clientBufferLen,spaceLeft,0);
        if(bytes>0){
            conCheack->clientBufferLen+=bytes;
            int bytesSend=0;
            while(bytesSend < conCheack->clientBufferLen){
                int sendCur=send(conCheack->backendFd,conCheack->clientBuffer+bytesSend,conCheack->clientBufferLen-bytesSend,0);
                if (sendCur < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK){
                        break;
                    }else{
                        connectionDestroy(mainManager, conCheack);
                        return;
                    }
                }
                bytesSend+=sendCur;
            }
            int remaining=conCheack->clientBufferLen-bytesSend;
            memmove(conCheack->clientBuffer, conCheack->clientBuffer + bytesSend, remaining);
            conCheack->clientBufferLen = remaining;
        }else if (bytes == 0) {
            printf("Client disconnected\n");
            connectionDestroy(mainManager, conCheack);
            break;
        }else{
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }else{
                connectionDestroy(mainManager, conCheack);
                break;
            }
        }
    }
}

void handleSendingDataBk(int curFd,BackendPool* mainPool,ConnectionManager* mainManager,Connection* conCheack){
    while(1){
        int spaceLeft=BUFFER_SIZE-conCheack->backendBufferLen;
        if(spaceLeft<=0){
            printf("Buffer on %d  ovverride\n",conCheack->backendFd);
            memset(conCheack->backendBuffer,0,BUFFER_SIZE);
            conCheack->backendBufferLen = 0; 
            spaceLeft = BUFFER_SIZE;
        }
        int bytes=recv(conCheack->backendFd,conCheack->backendBuffer+conCheack->backendBufferLen,spaceLeft,0);
        if(bytes>0){
            conCheack->backendBufferLen+=bytes;
            int bytesSend=0;
            while(bytesSend < conCheack->backendBufferLen){
                int sendCur=send(conCheack->clientFd,conCheack->backendBuffer+bytesSend,conCheack->backendBufferLen - bytesSend,0);
                if (sendCur < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK){
                        break;
                    }else{
                        connectionDestroy(mainManager, conCheack);
                        return;
                    }
                }
                bytesSend+=sendCur;
            }
            int remaining=conCheack->backendBufferLen-bytesSend;
            memmove(conCheack->backendBuffer, conCheack->backendBuffer + bytesSend, remaining);
            conCheack->backendBufferLen = remaining;
        }else if (bytes == 0) {
            connectionDestroy(mainManager, conCheack);
            break;
        }else{
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }else{
                connectionDestroy(mainManager, conCheack);
                break;
            }
        }
    }
}

int LoadBalancerInit(BackendPool** mainPool,ConnectionManager** mainManager,int* listenFd,struct sockaddr_in* ptr,Config* conf){
    *mainPool=createPoll(conf->algorithm);
    if(*mainPool==NULL){
        return 1;
    }
    *mainManager=managerCreate(mainPool,MAX_CON);
    if(*mainManager==NULL){
        freeePoll(mainPool);
        return 1;
    }
    for(int i = 0; i < conf->backendCount; i++){
        char host[64];
        int port;
        if (sscanf(conf->backends[i], "%63[^:]:%d", host, &port) == 2) {
            poolAdd(*mainPool, host, port);
        }
    }
    *listenFd=socket(AF_INET,SOCK_STREAM,0);
    if(*listenFd<0){
        return 1;
    }
    int opt = 1;
    if (setsockopt(*listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return 1;
    }
    setNonBlock(*listenFd);
    socklen_t len=sizeof(*ptr);
    ptr->sin_port = htons(conf->port);
    if((bind(*listenFd,(struct sockaddr*)ptr,len))<0){
        return 1;
    }
    if((listen(*listenFd,SOMAXCONN))<0){
        return 1;
    }
    struct epoll_event ev={
        .data.fd=*listenFd,
        .events=EPOLLIN
    };
    if((epoll_ctl((*mainManager)->epollFd,EPOLL_CTL_ADD,*listenFd,&ev))<0){
        return 1;
    }
    return 0;
}
