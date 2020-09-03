#ifndef __REDIS_COMMON_H
#define __REDIS_COMMON_H

//handle type
#define RedisCom void

RedisCom* redis_connect(char *ip, int port);

void redis_disconnect(RedisCom *rc);

void redis_getStr(RedisCom *rc, char *key, char *defaultRet);

int redis_getInt(RedisCom *rc, char *key, int defaultRet);

void redis_setStr(RedisCom *rc, char *key, char *value);

void redis_setInt(char *key, int value, RedisCom *rc);

#endif /* __REDIS_COMMON_H */