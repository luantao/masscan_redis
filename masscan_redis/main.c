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
#include <curl/curl.h>
#include <time.h>
#include "cJSON.h"
#include "main.h"

#define redis_host "127.0.0.1"
#define redis_port 6379
#define redis_queue_key "masscan:queue:list"
//#define redis_remote_host "10.138.77.203"
#define redis_remote_host "127.0.0.1"
#define redis_remote_port 6379
#define redis_remote_key "logcenter:adlab_ip_port_banner"
#define BUFES PIPE_BUF
#define MAX_BUF_SIZE PIPE_BUF
#define FIFONAME "devPort.pid"

//===========redis===========
redisContext* cache_client, *cache_client_remote;
#define REDIS_EXE(A,B,C) redisReply* cache_reply = (redisReply*) redisCommand(cache_client, A,B,C);
#define REDIS_FREE freeReplyObject(cache_reply);
#define REDIS_CHECK(X)  if (cache_reply->type != X) {printf("Failed to execute command[%s]\n", command);REDIS_FREE exit(EXIT_FAILURE);}

#define REDIS_EXE_R(A,B,C) redisReply* cache_reply_r = (redisReply*) redisCommand(cache_client_remote, A,B,C);
#define REDIS_FREE_R freeReplyObject(cache_reply_r);
#define REDIS_CHECK_R(X)  if (cache_reply_r->type != X) {printf("Failed to execute command[%s]\n", command);REDIS_FREE_R exit(EXIT_FAILURE);}

typedef struct _half_str {
    char string[MAX_BUF_SIZE * 20];
    int num;
} half_str;
half_str half_json;

void get_cache_client() {
    cache_client = redisConnect(redis_host, redis_port);
    if (cache_client->err) {
        redisFree(cache_client);
        cache_client=NULL;
        perror("can not client redis\n");
        exit(EXIT_FAILURE);
    } else {
        printf("redis client success!\n");
    }
}

void get_cache_client_r() {
    cache_client_remote = redisConnect(redis_remote_host, redis_remote_port);
    if (cache_client_remote->err) {
        redisFree(cache_client_remote);
        cache_client_remote=NULL;
        perror("can not client remote redis\n");
        exit(EXIT_FAILURE);
    } else {
        printf("redis client remote success!\n");
    }
}

void pack_str(msgpack_packer *pk, char *string) {
    int len = strlen(string);
    msgpack_pack_str(pk, len);
    msgpack_pack_str_body(pk, string, len);
}

size_t write_data(char* buffer, size_t size, size_t nmemb, char *response) {
    strncpy(response, buffer, 1024);
    return size*nmemb;
}

cJSON* get_addr(char* ip) {
    CURL *curl;
    CURLcode res;
    char *url = "http://tempt151.ops.zwt.qihoo.net:8362/geoip/index.php?ips=";
    char *request;
    char response[1024];
    cJSON *response_json;
    request = (char *) malloc(strlen(url) + strlen(ip));
    strcat(request, url);
    strcat(request, ip);
    curl = curl_easy_init();
    if (curl != NULL) {
        curl_easy_setopt(curl, CURLOPT_URL, request);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_POST, 0);
        res = curl_easy_perform(curl);
        if (CURLE_OK == res) {
            response_json = cJSON_Parse(response);
        }
        curl_easy_cleanup(curl);
    }
    free(request);
    request=NULL;
    if (response_json) {
        return response_json;
    } else {
        return NULL;
    }
}

void child() {
    printf("----child----%d\n", getpid());
    get_cache_client();
    get_cache_client_r();
    time_t now;
    struct tm *tm_now;
    char datetime[200];
    while (1) {
        time(&now);
        tm_now = localtime(&now);
        strftime(datetime, 200, "%Y-%m-%d %H:%M:%S", tm_now);
        redisReply* cache_reply = (redisReply*) redisCommand(cache_client, "lpop %s", redis_queue_key);
        if (cache_reply->type == REDIS_REPLY_NIL) {
            sleep(1);
        } else {
            msgpack_sbuffer sbuf;
            msgpack_packer pk;
            msgpack_sbuffer_init(&sbuf);
            msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
            cJSON *remote = cJSON_CreateObject();
            cJSON *json = cJSON_Parse(cache_reply->str); //dev_port信息
            cJSON *ip = cJSON_GetObjectItem(json, "ip");
            cJSON *_ports = cJSON_GetObjectItem(json, "ports");
            cJSON *port = cJSON_GetObjectItem(_ports->child, "port");
            cJSON *proto = cJSON_GetObjectItem(_ports->child, "proto");
            cJSON *_service = cJSON_GetObjectItem(_ports->child, "service");
            cJSON *name = cJSON_GetObjectItem(_service, "name");
            cJSON *banner = cJSON_GetObjectItem(_service, "banner");
            cJSON *ipAddr = get_addr(ip->valuestring); //归属地及坐标
            if (ipAddr && strcmp(cJSON_Print(ipAddr),"false")!=0) {
                cJSON *data = cJSON_GetObjectItem(ipAddr, "data");
                if (data->type != 0 &&strcmp(cJSON_Print(data),"[]")!=0) {
                    cJSON *country = cJSON_GetObjectItem(data->child, "country_name");
                    if (country->type != 0) {
                        if (country->valuestring == NULL) {
                            cJSON_AddStringToObject(remote, "country", "-");
                        } else {
                            cJSON_AddStringToObject(remote, "country", country->valuestring);
                        }
                    } else {
                        cJSON_AddStringToObject(remote, "country", "-");
                    }
                    cJSON *city = cJSON_GetObjectItem(data->child, "city_name");
                    if (city->type != 0) {
                        if (city->valuestring == NULL) {
                            cJSON_AddStringToObject(remote, "city", "-");
                        } else {
                            cJSON_AddStringToObject(remote, "city", city->valuestring);
                        }
                    } else {
                        cJSON_AddStringToObject(remote, "city", "-");
                    }
                }else{
                    cJSON_AddStringToObject(remote, "country", "-");
                    cJSON_AddStringToObject(remote, "city", "-");
                }
            }else{
                cJSON_AddStringToObject(remote, "country", "-");
                cJSON_AddStringToObject(remote, "city", "-");
            }
            cJSON_Delete(ipAddr);
            
            cJSON_AddStringToObject(remote, "ip", ip->valuestring);
            cJSON_AddNumberToObject(remote, "port", 111);
            cJSON_AddStringToObject(remote, "banner", banner->valuestring);
            cJSON_AddStringToObject(remote, "service", name->valuestring);
            cJSON_AddStringToObject(remote, "device", "-");
            cJSON_AddStringToObject(remote, "company", "-");
            cJSON_AddStringToObject(remote, "grab_time", datetime);
            
            pack_str(&pk, cJSON_Print(remote));
            char *command;
            command = (void *) malloc(sbuf.size);
            memcpy(command, sbuf.data, sbuf.size);
            if(strlen(command)>sbuf.size){
            char *p;
            p = command + sbuf.size;
            memset(p, 0x00, strlen(command) - sbuf.size);}
            REDIS_EXE_R("rpush %s %s", redis_remote_key, command);
            REDIS_CHECK_R(REDIS_REPLY_INTEGER);
            REDIS_FREE_R
//                                msgpack_zone mempool;
//                                msgpack_object deserialized;
//                                msgpack_zone_init(&mempool, 1024);
//                                msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);
//                                msgpack_object_print(stdout, deserialized);

            free(command);
            command=NULL;
            msgpack_sbuffer_destroy(&sbuf);
            cJSON_Delete(remote);
            remote=NULL;
            cJSON_Delete(json);
            json=NULL;
        }
        REDIS_FREE
        usleep(10000);
    }

}

void _child() {
//    fork();
    child();
}

void getDevPort(cJSON * json) {
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
    //    msgpack_pack_array(&pk, 14);
    //    ip = cJSON_GetObjectItem(json, "ip");
    //    _ports = cJSON_GetObjectItem(json, "ports");
    //    port = cJSON_GetObjectItem(_ports->child, "port");
    //    proto = cJSON_GetObjectItem(_ports->child, "proto");
    //    _service = cJSON_GetObjectItem(_ports->child, "service");
    //    name = cJSON_GetObjectItem(_service, "name");
    //    banner = cJSON_GetObjectItem(_service, "banner");
    //    pack_str(&pk, "ip");
    //    pack_str(&pk, ip->valuestring);
    //    pack_str(&pk, "port");
    //        msgpack_pack_str(&pk, strlen(banner->valuestring));
    //        msgpack_pack_str_body(&pk, banner->valuestring, strlen(banner->valuestring));
    //    msgpack_pack_int(&pk, 111);
    //    //        pack_str(&pk,proto->valuestring);
    //    pack_str(&pk, name->valuestring);
    //    pack_str(&pk, "country");
    //    pack_str(&pk, "-");
    //    pack_str(&pk, "city");
    //    pack_str(&pk, "-");
    //    pack_str(&pk, "banner");
    //    pack_str(&pk, banner->valuestring);
    //    pack_str(&pk, "service");
    //    pack_str(&pk, "-");
    //    pack_str(&pk, "device");
    //    pack_str(&pk, "-");
    //    pack_str(&pk, "company");
    //    pack_str(&pk, "-");
    //    pack_str(&pk, "grab_time");
    //    pack_str(&pk, "-");
    //    printf("%s\n", sbuf.data);
    //    printf("%ld\n", sbuf.size);
    //        msgpack_zone mempool;
    //    msgpack_object deserialized;
    //        msgpack_zone_init(&mempool, 2048);
    //
    //    msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);
    //
    //    /* print the deserialized object. */
    //    msgpack_object_print(stdout, deserialized);

    //    char *command;
    //    command = (char *) malloc(sbuf.size);
    //    memset(command, 0x00, sbuf.size);
    //    strncat(command, sbuf.data, sbuf.size);
    //    printf("%s\n", command);
    //    REDIS_EXE("rpush %s %s", redis_queue_key, command);
    //    REDIS_CHECK(REDIS_REPLY_INTEGER);
    //    REDIS_FREE
    //    msgpack_sbuffer_destroy(&sbuf);

    char *command;
    command = cJSON_Print(json);
    REDIS_EXE("rpush %s %s", redis_queue_key, command);
    REDIS_CHECK(REDIS_REPLY_INTEGER);
    REDIS_FREE
    free(command);
    command=NULL;
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
            _json=NULL;
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
            json=NULL;
        } else {
            getDevPort(json);
            cJSON_Delete(json);
            json=NULL;
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
    if ((fd = open(fifo_name, O_RDWR)) < 0) {//O_NONBLOCK
        perror("open");
        exit(EXIT_FAILURE);
    }
    switch (fork()) {
        case 0:
            _child();
            _exit(0);
        case -1:
            perror("fork child error");
            exit(EXIT_FAILURE);
        default:
            break;
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