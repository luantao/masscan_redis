/* 
 * File:   main.h
 * Author: luantao
 *
 * Created on 2015年8月1日, 下午4:14
 */

#ifndef MAIN_H
#define	MAIN_H

#ifdef	__cplusplus
extern "C" {
#endif

    typedef struct _dev_port{
        char *ip;
        char *port;
        char *country;
        char *city;
        char *banner;
        char *service;
        char *device;
        char *company;
        char *grab_time;
    }dev_port;

    typedef struct _masscan_dev_port{
        char *ip;
        char *port;
        char *proto;
        char *name;
        char *banner;
    }masscan_dev_port;
#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

