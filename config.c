#include"config.h"

void parseFile(const char* fileName,Config* conf){
    FILE* fp=fopen(fileName,"r");
    if(fp==NULL){
        printf("Error opening file\n");
        return;
    }

    char buffer[256];
    while((fgets(buffer,sizeof(buffer),fp))!=NULL){
        char key[64];
        char value[128];
        if((sscanf(buffer,"%63s %127s",key, value))==2){
            if((strcmp(key,"port"))==0){
                conf->port=atoi(value);
                continue;
            }
            if((strcmp(key,"algorithm"))==0){
                if (strcmp(value, "round_robin") == 0) {
                    conf->algorithm = ALG_ROUND_ROBIN;
                    continue;
                }
            }
            if((strcmp(key,"backend"))==0){
                if (conf->backendCount < MAX_BACKENDS) {
                    strcpy(conf->backends[conf->backendCount], value);
                    conf->backendCount++;
                    continue;
                }
            }
            if (strcmp(key, "health_check_interval") == 0) {
                conf->healthInterval = atoi(value);
                continue;
            } 
            if (strcmp(key, "health_check_timeout") == 0) {
                conf->healthTimeout = atoi(value);
                continue;
            } 
            if (strcmp(key, "health_check_fails") == 0) {
                conf->healthFails = atoi(value);
                continue;
            } 
            if (strcmp(key, "health_check_passes") == 0) {
                conf->healthPasses = atoi(value);
                continue;
            }
        }
    }
    fclose(fp);
}