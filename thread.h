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

/*! \brief Definition of the possible thread states (TS_*).
 *
 * These are the possible states of a given thread, stored
 * in the thread_flags bitset of the thread descriptor.
 */
#define TS_RUN       1    //!< Thread is running
#define TS_WAIT      2    //!< Thread is blocked, waiting
#define TS_READY     4    //!< Thread is ready to run
#define TB_SIGNAL    8    //!< Thread has been signaled
#define TB_LAUNCH    16   //!< Thread is going to be launched


 /*! \brief Signal types definition (ST).
 *
 * Here are the available signals to be sent and received
 * by threads to each other, with the wait() and signal() system calls.
 */
#define ST_MESG      1    //!< Thread received a message
#define ST_EXCEPT    2    //!< Thread received an exception


struct msg_port_s {
    list_node_t node;  // Node to link this port to others in a list
    list_head_t message_list; // Messages posted on this port
    uint8_t num_msg; // Number of messages posted
};

struct thread_s{
    list_node_t node;
    uint32_t user_esp;
    pagedir_t * page_directory;
    uint32_t thread_flags;
    uint32_t initial_eip;
    uint32_t except_eip;
    uint32_t kernel_esp;
    uint32_t uid;    // Task owner - 0 means 'core'
    uint32_t init_kernel_esp;
    uint32_t sig_wait;       // Signals being waited
    uint32_t sig_recvd;      // Signals received
    struct msg_port_s msg_port;
    list_head_t msg_wait_proc;
} __attribute__((packed));

typedef struct msg_port_s msg_port_t;
typedef struct thread_s thread_t;

thread_t *init_threading ();
thread_t *create_thread(int (*fn)(void*), void *args, int (*at_exit)(void*),
        uint32_t *user_stack, uint32_t *kernel_stack, const char * name, int uid);
void switch_thread (thread_t *next);
void schedule();

void forbid();
void permit();

int _wait_for_flags(uint32_t flags);
void wait(int);


#endif	/* _THREAD_H */

