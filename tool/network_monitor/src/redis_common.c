// -------------------- Redis --------------------

#include <hiredis.h>
#include <pthread.h>

typedef struct{
    void *p;
    pthread_mutex_t lock;
}RedisCom;

RedisCom* redis_connect(char *ip, int port)
{
    RedisCom *rc = (RedisCom*)calloc(1, sizeof(RedisCom));
//    rc->p = redisConnect(ip, port);
    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0
    };
    rc->p = redisConnectWithTimeout(ip, port, tv);
    if (!rc->p || ((redisContext*)rc->p)->err)
    {
        if(rc->p)
            printf("[gnss]: redisConnect Error: %s\n", ((redisContext*)rc->p)->err);
        else
            printf("[gnss]: redisConnect Error: NULL\n");
        free(rc);
        return NULL;
    }
    pthread_mutex_init(&rc->lock, NULL);
    return rc;
}

void redis_disconnect(RedisCom *rc)
{
    if(!rc)
        return;
    redisFree((redisContext*)rc->p);
    pthread_mutex_destroy(&rc->lock);
}

void redis_getStr(RedisCom *rc, char *key, char *defaultRet)
{
    char cmd[128] = {0};
    //
    if(!rc || !rc->p)
        return;
    //
    pthread_mutex_lock(&rc->lock);
    //
    sprintf (cmd, "GET %s", key);
    redisReply *reply = (redisReply*)redisCommand((redisContext*)rc->p, cmd);
    if(reply->type == REDIS_REPLY_STRING)
        strcpy(defaultRet, reply->str);
    else if(reply->type == REDIS_REPLY_INTEGER)
        sprintf(defaultRet, "%ld", reply->integer);
    // else
    //     qDebug() << "redis_getStr: type err " << reply->type << " (" << reply->str << ") cmd: " << cmd;
    freeReplyObject(reply);
    //
    pthread_mutex_unlock(&rc->lock);
    //
    defaultRet[strlen(defaultRet)] = '\0';
}

int redis_getInt(RedisCom *rc, char *key, int defaultRet)
{
    char result[64] = {0};
    int resultInt = defaultRet;
    sprintf(result, "%d", defaultRet);
    redis_getStr(rc, key, result);
    sscanf(result, "%d", &resultInt);
    return resultInt;
}

void redis_setStr(RedisCom *rc, char *key, char *value)
{
    char cmd[128] = {0};
    //
    if(!rc || !rc->p)
        return;
    //
    pthread_mutex_lock(&rc->lock);
    //
    sprintf (cmd, "SET %s %s", key, value);
    redisReply *reply = (redisReply*)redisCommand((redisContext*)rc->p, cmd);
    freeReplyObject(reply);
    //
    pthread_mutex_unlock(&rc->lock);
}

void redis_setInt(RedisCom *rc, char *key, int value)
{
    char cmd[128] = {0};
    //
    if(!rc || !rc->p)
        return;
    //
    pthread_mutex_lock(&rc->lock);
    //
    sprintf (cmd, "SET %s %d", key, value);
    redisReply *reply = (redisReply*)redisCommand((redisContext*)rc->p, cmd);
    freeReplyObject(reply);
    //
    pthread_mutex_unlock(&rc->lock);
}