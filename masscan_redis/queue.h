/* 
 * File:   queue.h
 * Author: luantao
 *
 * Created on 2015年8月2日, 上午11:33
 */

#ifndef QUEUE_H
#define	QUEUE_H

#ifdef	__cplusplus
extern "C" {
#endif
    typedef char * elemType;

    typedef struct nodet {
        elemType data;
        struct nodet * next;
    } node_t;

    typedef struct queuet {
        node_t * head;
        node_t * tail;
    } queue_t;



#ifdef	__cplusplus
}
#endif

#endif	/* QUEUE_H */

