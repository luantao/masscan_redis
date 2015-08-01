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
#include "cJSON.h"
#include "main.h"

#define BUFES PIPE_BUF
#define MIN_LIN 100
#define MAX_BUF_SIZE PIPE_BUF
#define FIFONAME "devPort.pid"

int pipe_send;
char half_str[MAX_BUF_SIZE];

void sendData(int pipe) {
    char *buf;
    int len;
    buf = (char *) malloc(MAX_BUF_SIZE);
    while (read(pipe, buf, MAX_BUF_SIZE) > 0) {
        len = strlen(buf) - 2;
        char *tmp;
        tmp = (char *) malloc(10);
        strncpy(tmp, buf + len, 2);
        if (strcmp(tmp, "},") != 0) {
            strncpy(half_str, buf, strlen(buf));
            //            printf("%s\n",half_str);
            continue;
        } else {
            cJSON *json;
            if (strlen(half_str) != 0 && strcmp(half_str, ",") != 0) {
                strcat(half_str, buf);
                json = cJSON_Parse(half_str);
            } else {
                json = cJSON_Parse(buf);
            }
            if (!json) {
                printf("Error before: [%s]\n -[%s]\n ==[%s]==\n", cJSON_GetErrorPtr(), buf, half_str);
            } else {
                char *out;
                out = cJSON_Print(json);
                cJSON_Delete(json);
                //                printf("%s\n", out);
                free(out);
            }
            memset(half_str, 0x00, MAX_BUF_SIZE);
        }
        free(tmp);
    }
    free(buf);
}

int main(void) {
    char *ptr;
    int fd, timer = 0;
    char buf[BUFES], *contaner;
    mode_t mode = 0666;
    char *fifo_name = FIFONAME;
    memset(half_str, 0x00, MAX_BUF_SIZE);
    contaner = (char *) malloc(MAX_BUF_SIZE * 10);
    memset(contaner, 0x00, MAX_BUF_SIZE * 10);
    if ((access(fifo_name, 0)) != -1) {
        remove(fifo_name);
    }
    if ((mkfifo(fifo_name, mode)) < 0) {
        perror("failed to PID");
        exit(1);
    }
    if ((fd = open(fifo_name, O_RDWR | O_NONBLOCK)) < 0) {
        perror("open");
        exit(1);
    }
    int fds[2];
    if (pipe(fds) < 0) {
        perror("create pipe error");
        return 0;
    }
    switch (fork()) {
        case 0:
            close(fds[1]);
            sendData(fds[0]);
            close(fds[0]);
            _exit(0);
        case -1:
            perror("fork child error");
            return 0;
        default:
            close(fds[0]);
            pipe_send = fds[1];
            break;
    }
    while (1) {
        memset(buf, '\n', BUFES);
        if (read(fd, buf, BUFES) > 0) {
            timer++;
            strcat(contaner, buf);
            if (timer > 8) {
                timer=0;
                char *p = NULL;
                ptr = strtok_r(contaner, "\n", &p);
                while (ptr != NULL) {
                    write(pipe_send, ptr, MAX_BUF_SIZE);
                    ptr = strtok_r(NULL, "\n", &p);
                }
                memset(contaner,0x00,MAX_BUF_SIZE * 10);
            }
        }
    }
    close(fd);
    exit(0);
}