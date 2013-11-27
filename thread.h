/* 
 * File:   thread.h
 * Author: renato
 *
 * Created on November 25, 2013, 7:18 PM
 */

#ifndef _THREAD_H
#define	_THREAD_H
#include "common.h"
#include "pmm.h"

// Possible thread states
#define TS_RUN                  1
#define TS_WAIT                 2
#define TS_READY                4
#define TB_EXCEPTION_WAITING    8

typedef struct {
    list_node_t node;
    uint32_t esp;
    pagedir_t * page_directory;
    uint32_t uid;    // Task owner - 0 means 'core'
    list_head_t msg_port;
    uint32_t thread_flags;
} __attribute__((packed)) thread_t;

thread_t *init_threading ();
thread_t *create_thread (int (*fn)(void*), void *arg,
        uint32_t *stack, const char * name, int pri);
void switch_thread (thread_t *next);
void schedule();


#endif	/* _THREAD_H */

