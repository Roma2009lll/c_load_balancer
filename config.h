#ifndef CONFIG_H
#define CONFIG_H
#define MAX_BACKENDS 3

#include"backend.h"
#include "connection.h"
#include<ctype.h>
#include<string.h>

typedef struct {
    int port;
    Algorithm algorithm;
    char backends[MAX_BACKENDS][128]; 
    int backendCount;
    int healthInterval;
    int healthTimeout;
    int healthFails;
    int healthPasses;
} Config;

void parseFile(const char* fileName,Config* conf);

#endif