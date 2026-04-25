#include"backend.h"

Backend* roundRobin(BackendPool* pool){
    pthread_mutex_lock(&pool->lock);
    int attemps=0;
    while(attemps<pool->count){
        Backend* curBack=&(pool->backends[pool->current_index]);
        pool->current_index=(pool->current_index+1)%pool->count;
        if(curBack->healthy){
            pthread_mutex_unlock(&pool->lock);
            return curBack;
        }
        attemps++;
    }
    pthread_mutex_unlock(&pool->lock);
    return NULL;
}

Backend* leastCon(BackendPool* pool){
    pthread_mutex_lock(&pool->lock);
    long long minCon=__INT_MAX__;
    Backend* bestCon=NULL;
    for(int i=0;i<pool->count;i++){
        if(pool->backends[i].healthy&&pool->backends[i].activeConnections<minCon){
            bestCon= &(pool->backends[i]);
            minCon=pool->backends[i].activeConnections;
        }
    }
    pthread_mutex_unlock(&pool->lock);
    return bestCon;
}

void conOpend(Backend* b) {
    pthread_mutex_lock(&b->lock);
    b->activeConnections++;
    pthread_mutex_unlock(&b->lock);
}

void conClosed(Backend* b) {
    pthread_mutex_lock(&b->lock);
    b->activeConnections--;
    if (b->activeConnections < 0) {
        b->activeConnections = 0;
    }
    pthread_mutex_unlock(&b->lock);
}

void healthChek(Backend* backEnd){
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0){
        return;
    }
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    struct sockaddr_in clientCon={
        .sin_family=AF_INET,
        .sin_port=htons(backEnd->port)
    };
    inet_pton(AF_INET, backEnd->host, &clientCon.sin_addr);
    int resCon=connect(sock,(struct sockaddr*)&clientCon,sizeof(clientCon));
    close(sock);
    pthread_mutex_lock(&backEnd->lock);
    if(resCon==-1){
        backEnd->failures++;
        backEnd->successes=0;
        if(backEnd!=NULL&&backEnd->failures>3){
            backEnd->healthy=false;
            printf("BackEnd marked unhelthy\n");
        }
    }else{
        backEnd->failures=0;
        backEnd->successes++;
        if(backEnd!=NULL&&backEnd->successes>=2){
            backEnd->healthy=true;
            printf("BackEnd marked helthy\n");
        }
    }
    backEnd->lastCheck=time(NULL);
    pthread_mutex_unlock(&backEnd->lock);
}

BackendPool* createPoll(Algorithm alg){
    BackendPool* mainPool =(BackendPool*)malloc(sizeof(BackendPool));
    if(mainPool==NULL){
        return NULL;
    }
    mainPool->capacity=10;
    mainPool->count=0;
    mainPool->current_index=0;
    mainPool->alg=alg;
    mainPool->backends=(Backend*)malloc(sizeof(Backend)*mainPool->capacity);
    if(mainPool->backends==NULL){
        free(mainPool);
        return NULL;
    }
    pthread_mutex_init(&mainPool->lock, NULL);
    return mainPool;
}

void poolAdd(BackendPool* pool, const char* host, int port) {
    if (pool->count >= pool->capacity) return; 
    Backend* b = &pool->backends[pool->count];
    b->port = port;
    strncpy(b->host, host, sizeof(b->host) - 1);
    b->host[sizeof(b->host) - 1] = '\0';
    b->failures = 0;
    b->successes = 0;
    b->activeConnections = 0; 
    b->healthy = true;
    pthread_mutex_init(&b->lock, NULL);
    pool->count++;
}

Backend* poolSelect(BackendPool* pool){
    if(pool->alg==ALG_ROUND_ROBIN){
        return roundRobin(pool);
    }else{
        return leastCon(pool);
    }
}

void freeePoll (BackendPool* pool){
    if(pool!=NULL){
        if(pool->backends!=NULL){
            for(int i=0;i<pool->count;i++){
                pthread_mutex_destroy(&pool->backends[i].lock);
            }
            free(pool->backends);
        }
        pthread_mutex_destroy(&pool->lock);
        free(pool);
    }
}

void healthCheacks(void* argv){
    BackendPool* pool=(BackendPool*)argv;
    while(1){
        for(int i=0;i<pool->count;i++){
            healthChek(((pool->backends)+i));
        }
        sleep(5);
    }
    return NULL;
}

