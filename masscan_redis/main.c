/* 
 * File:   main.c
 * Author: luantao
 *
 * Created on 2015年7月31日, 上午10:09
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include <msgpack.h>
#include "cJSON.h"
#include "main.h"

#define redis_host "127.0.0.1"
#define redis_port 6379
#define redis_queue_key "masscan:queue:list"
#define redis_remote_key "logcenter:adlab_ip_port_banner"
#define BUFES PIPE_BUF
#define MAX_BUF_SIZE PIPE_BUF
#define FIFONAME "devPort.pid"

//===========redis===========
redisContext* cache_client;
#define REDIS_EXE(A,B,C) redisReply* cache_reply = (redisReply*) redisCommand(cache_client, A,B,C);
#define REDIS_FREE freeReplyObject(cache_reply);
#define REDIS_NIL if (cache_reply->type == REDIS_REPLY_NIL) {REDIS_FREE return NULL;}
#define REDIS_CHECK(X)  if (cache_reply->type != X) {printf("Failed to execute command[%s]\n", command);REDIS_FREE exit(EXIT_FAILURE);}REDIS_FREE

typedef struct _half_str {
    char string[MAX_BUF_SIZE * 20];
    int num;
} half_str;
half_str half_json;

void get_cache_client() {
    cache_client = redisConnect(redis_host, redis_port);
    if (cache_client->err) {
        redisFree(cache_client);
        perror("can not client redis\n");
        exit(EXIT_FAILURE);
    } else {
        printf("redis client success!\n");
    }
}

void getDevPort(cJSON *json) {
    //    cJSON *ip;
    //    cJSON *_ports;
    //    cJSON *port;
    //    cJSON *proto;
    //    cJSON *_service;
    //    cJSON *name;
    //    cJSON *banner;
    //    msgpack_sbuffer sbuf;
    //    msgpack_packer pk;
    //    msgpack_sbuffer_init(&sbuf);
    //    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    //    msgpack_pack_array(&pk, 5);
    //    ip = cJSON_GetObjectItem(json, "ip");
    //    _ports = cJSON_GetObjectItem(json, "ports");
    //    port = cJSON_GetObjectItem(_ports->child, "port");
    //    proto = cJSON_GetObjectItem(_ports->child, "proto");
    //    _service = cJSON_GetObjectItem(_ports->child, "service");
    //    name = cJSON_GetObjectItem(_service, "name");
    //    banner = cJSON_GetObjectItem(_service, "banner");
    //    msgpack_pack_str(&pk, strlen(ip->valuestring));
    //    msgpack_pack_str_body(&pk, ip->valuestring, strlen(ip->valuestring));
    //    msgpack_pack_int(&pk, port->valueint);
    //    msgpack_pack_str(&pk, strlen(proto->valuestring));
    //    msgpack_pack_str_body(&pk, proto->valuestring, strlen(proto->valuestring));
    //    msgpack_pack_str(&pk, strlen(name->valuestring));
    //    msgpack_pack_str_body(&pk, name->valuestring, strlen(name->valuestring));
    //    msgpack_pack_str(&pk, strlen(banner->valuestring));
    //    msgpack_pack_str_body(&pk, banner->valuestring, strlen(banner->valuestring));

    //    printf("%s\n",sbuf.data);
    //    printf("%ld\n",strlen(sbuf.data));
    //    printf("%ld\n",sbuf.size);
    //    char *command;
    //    command=(char *)malloc(sbuf.size);
    //    memset(command,0x00,sbuf.size);
    //    strncat(command,sbuf.data,sbuf.size);
    //    REDIS_EXE("rpush %s %s",redis_queue_key,command);
    //    REDIS_CHECK(REDIS_REPLY_INTEGER);
    //    msgpack_sbuffer_destroy(&sbuf);

    char *command;
    command = cJSON_Print(json);
    REDIS_EXE("rpush %s %s", redis_queue_key, command);
    REDIS_CHECK(REDIS_REPLY_INTEGER);
    free(command);
}

void getJSON(char * ptr) {
    if (strcmp(ptr, ",") == 0) {
        return;
    }
    cJSON *_json;
    if (strlen(half_json.string) > 0) {
        strcat(half_json.string, ptr);
        _json = cJSON_Parse(half_json.string);
        if (_json) {
            getDevPort(_json);
            cJSON_Delete(_json);
            memset(half_json.string, 0x00, MAX_BUF_SIZE * 20);
            half_json.num = 0;
        }
    } else {
        cJSON *json;
        json = cJSON_Parse(ptr);
        if (!json) {
            strcat(half_json.string, ptr);
            half_json.num++;
            cJSON_Delete(json);
        } else {
            getDevPort(json);
            cJSON_Delete(json);
        }
    }
}

void _init() {
    memset(half_json.string, 0x00, MAX_BUF_SIZE);
    half_json.num = 0;
    get_cache_client();
}

int main(void) {
    char *ptr;
    int fd;
    char buf[BUFES];
    mode_t mode = 0666;
    char *fifo_name = FIFONAME;
    _init();
    if ((access(fifo_name, 0)) != -1) {
        remove(fifo_name);
    }
    if ((mkfifo(fifo_name, mode)) < 0) {
        perror("failed to PID");
        exit(EXIT_FAILURE);
    }
    if ((fd = open(fifo_name, O_RDWR | O_NONBLOCK)) < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    while (1) {
        memset(buf, 0x00, BUFES);
        if (read(fd, buf, BUFES) > 0) {
            char *p = NULL;
            ptr = strtok_r(buf, "\n", &p);
            while (ptr != NULL) {
                getJSON(ptr);
                ptr = strtok_r(NULL, "\n", &p);
            }
        }
    }
    close(fd);
    exit(0);
}