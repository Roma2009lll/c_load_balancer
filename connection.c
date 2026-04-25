#include "connection.h"

Connection* connectionCreate(ConnectionManager* mgr,int clienFd, Backend* backend){
    Connection* newCon=(Connection*)malloc(sizeof(Connection));
    if(newCon==NULL){
        return NULL;
    }
    newCon->clientFd=clienFd;
    setNonBlock(newCon->clientFd);
    newCon->backend=backend;
    int sockBeack=socket(AF_INET,SOCK_STREAM,0);
    if(sockBeack<0){
        free(newCon);
        return NULL;
    }
    setNonBlock(sockBeack);
    struct sockaddr_in backAddr={
        .sin_port=htons(backend->port),
        .sin_family=AF_INET
    };
    inet_pton(AF_INET,backend->host,&backAddr.sin_addr.s_addr);
    int res=connect(sockBeack,(struct sockaddr*)&backAddr,sizeof(backAddr));
    if (res < 0 && errno != EINPROGRESS) {
        close(sockBeack);
        free(newCon);
        return NULL;
    }
    newCon->backendFd=sockBeack;
    newCon->backendBufferLen=0;
    newCon->clientBufferLen=0;
    newCon->created=time(NULL);
    newCon->closing=0;
    conOpend(backend);
    if (clienFd < mgr->maxConnections && sockBeack < mgr->maxConnections) {
        mgr->connections[clienFd] = newCon;
        mgr->connections[sockBeack] = newCon;
    }
    return newCon;
}

Connection*findFd(ConnectionManager* mgr, int fd){
    if (fd >= 0 && fd < mgr->maxConnections) {
        return mgr->connections[fd];
    }
    return NULL;
}

ConnectionManager* managerCreate(BackendPool* pool, int maxCon){
    ConnectionManager* mgr=(ConnectionManager*)malloc(sizeof(ConnectionManager));
    if(mgr==NULL){
        return NULL;
    }
    mgr->backendPool=pool;
    mgr->maxConnections=maxCon;
    mgr->connections=(Connection**)calloc(mgr->maxConnections,sizeof(Connection*));
    if(mgr->connections==NULL){
        free(mgr);
        return NULL;
    }
    mgr->epollFd=epoll_create1(0);
    if(mgr->epollFd<0){
        free(mgr->connections);
        free(mgr);
        return NULL;
    }
    return mgr;
}

void freeManager(ConnectionManager* mgr){
    for(int i=0;i<mgr->maxConnections;i++){
        if(mgr->connections[i]!=NULL){
            connectionDestroy(mgr,mgr->connections[i]);
        }
    }
    close(mgr->epollFd);
    free(mgr->connections);
    free(mgr);
}

void connectionDestroy(ConnectionManager* mgr, Connection* con){
    if(con==NULL||con->clientFd<0||con->backendFd<0){
        return;
    }
    epoll_ctl(mgr->epollFd,EPOLL_CTL_DEL,con->clientFd,NULL);
    mgr->connections[con->clientFd] = NULL;
    close(con->clientFd);
    epoll_ctl(mgr->epollFd,EPOLL_CTL_DEL,con->backendFd,NULL);
    mgr->connections[con->backendFd] = NULL;
    close(con->backendFd);
    conClosed(con->backend);
    free(con);
}

void setNonBlock(int fd){
    int flags=fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flags|O_NONBLOCK);
}