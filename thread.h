/*! \file thread.h
 * 
 * Definitions related to multithreading and message ports.
 *
 * CHANGELOG
 *
 * - 2/Mar/2014 : File fully commented (Renato Encarnação)
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

/*! \brief Message Port structure.
 *
 * This structure is a message port, where threads can post and receive
 * messages to and from each other.
 */
struct msg_port_s {
    list_node_t node;          //!< Node to link this port to others in a list
    list_head_t message_list;  //!< Messages posted on this port
    uint8_t num_msg;           //!< Number of messages posted
};

/*! \brief Thread Descriptor structure.
 *
 * This structure defines the basic control flow inside Krypton: a thread.
 * It will be embedded by DOS inside other structures to form what is called
 * a Process. The kernel only deals with the preemption and message
 * passing between threads.
 */
struct thread_s{
    list_node_t node;            //!< Node to link this thread to others in a list.
    uint32_t user_esp;           //!< User stack pointer, stored by an interrupt.
    pagedir_t * page_directory;  //!< Current page directory (this will be messed up by DOS).
    uint32_t thread_flags;       //!< Thread active flags (running, signalled, etc.).
    uint32_t initial_eip;        //!< Entry Instruction Pointer.
    uint32_t except_eip;         //!< Exception handler entry point.
    uint32_t kernel_esp;         //!< Kernel stack pointer, stored by the interrupt.
    uint32_t uid;                //!< Thread owner, used to implement user-level protection.
    uint32_t init_kernel_esp;    //!< Initial kernel stack pointer, stored in the TSS.
    uint32_t sig_wait;           //!< Signals being waited by the thread.
    uint32_t sig_recvd;          //!< Signals received by the thread.
    struct msg_port_s msg_port;  //!< Thread's message port.
    list_head_t msg_wait_proc;   //!< List of processes waiting to post a message, if the port is full.
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
