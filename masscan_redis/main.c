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
char half_str[MAX_BUF_SIZE * 2];

void sendData(int pipe) {
    char *buf;
    buf = (char *) malloc(MAX_BUF_SIZE);
    while (read(pipe, buf, MAX_BUF_SIZE) > 0) {
        cJSON *json;
        if (strlen(half_str) > 0) {
            strcat(half_str, buf);
            json = cJSON_Parse(half_str);
            printf("%s\n", half_str);
            memset(half_str, 0x00, MAX_BUF_SIZE);
        } else {
            json = cJSON_Parse(buf);
        }
        if (!json) {
            strcpy(half_str, buf);
            cJSON_Delete(json);
        } else {
            char *out;
            out = cJSON_Print(json);
            cJSON_Delete(json);
//            printf("%s\n", out);
            free(out);
        }
        memset(buf, 0x00, MAX_BUF_SIZE);
    }
    free(buf);
}

int main(void) {
    char *ptr;
    int fd, timer = 0;
    char buf[BUFES], *contaner;
    mode_t mode = 0666;
    char *fifo_name = FIFONAME;
    contaner = (char *) malloc(BUFES * 10);
    memset(contaner, 0x00, BUFES * 10);
    memset(half_str, 0x00, MAX_BUF_SIZE);
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
    int fds[2];
    if (pipe(fds) < 0) {
        perror("create pipe error");
        exit(EXIT_FAILURE);
    }
    switch (fork()) {
        case 0:
            close(fds[1]);
            sendData(fds[0]);
            close(fds[0]);
            _exit(0);
        case -1:
            perror("fork child error");
            exit(EXIT_FAILURE);
        default:
            close(fds[0]);
            pipe_send = fds[1];
            break;
    }
    while (1) {
        memset(buf, 0x00, BUFES);
        if (read(fd, buf, BUFES) > 0) {
            strcat(contaner, buf);
            timer++;
            if (timer > 8) {
                char *p = NULL;
                ptr = strtok_r(buf, "\n", &p);
                while (ptr != NULL) {
                    write(pipe_send, ptr, BUFES);
                    ptr = strtok_r(NULL, "\n", &p);
                }
                memset(contaner, 0x00, BUFES * 10);
            }
        }
    }
    close(fd);
    exit(0);
}