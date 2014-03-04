#include "thread.h"
#include "sysbase.h"
#include "pmm.h"
#include "common.h"
#include "cpu.h"

extern void switch_context(thread_t *);


thread_t *create_thread(int (*fn)(void*), void *args, int (*at_exit)(void*),
        uint32_t *user_stack, uint32_t *kernel_stack, const char * name, int uid, int priority) {

    thread_t * new_thread = (thread_t *) kmalloc(sizeof (thread_t));
    memset (new_thread, 0, sizeof (thread_t));
    // Fill in the new thread name
    new_thread->node.name = (char *) kmalloc(strlen(name) + 1);
    strcpy(new_thread->node.name, name);
    new_thread->node.pri = priority; // Start with default priority
    new_thread->node.type = NT_THREAD;

    *--user_stack = (uint32_t)at_exit;  // Fake return address.
    *--user_stack = (uint32_t)args;

    new_thread->thread_flags = 0;
    new_thread->thread_flags = TS_READY | TB_LAUNCH;

    new_thread->user_esp = (uint32_t)user_stack;
    new_thread->init_kernel_esp = (uint32_t)kernel_stack;
    new_thread->page_directory = sys_base->act_page_directory;
    new_thread->initial_eip = (unsigned int) fn;

    add_head((list_head_t *) &sys_base->msgport_list, (list_node_t*) &new_thread->msg_port);
    new_list((list_head_t *) &new_thread->msg_port.message_list);

    new_thread->msg_port.num_msg = 0;

    enqueue((list_head_t *) &sys_base->thread_ready,
            (list_node_t *) new_thread);

    new_thread->uid = uid;

    return new_thread;
}

int _wait_for_flags(uint32_t flags) {
    disable();
    sys_base->running_thread->thread_flags &= (~TS_READY);
    sys_base->running_thread->thread_flags |= TS_WAIT;
    sys_base->running_thread->sig_wait |= flags;
    enqueue((list_head_t*) & sys_base->thread_wait,
             (list_node_t*) sys_base->running_thread);
    sys_base->sys_flags |= NEED_TASK_SWITCH;
}

/* system call wrapper to block a thread */
void wait(int sig_flags)
{
    asm volatile("mov $0, %%eax; \
                  mov %0, %%ebx; \
                  int $0xFF" :: "r" (sig_flags) : "%eax", "%ebx");
}

void forbid(){
    sys_base->forbid_counter++;
}

void permit(){
    sys_base->forbid_counter--;
}

void schedule() {
    /* This is the scheduler, called by one of the 2 interrupt handlers.
       We will check if the running thread has an event to be processed
       or there is a thread with an higher priority to be ran. */

    thread_t * top_thread;

    /* If multitasking is disabled, kernel was reentered or an immediate task switch is needed,
       return immediately */
    if(sys_base->forbid_counter > 0)
        return;

    /* If the running thread (if one is running) has a pending signal,
       reschedule immediately so it may be redispatched with a new time slice*/
    /*if((sys_base->running_thread->thread_flags & TB_SIGNAL) != 0){
        enqueue((list_head_t*) & sys_base->thread_ready,
                (list_node_t*) sys_base->running_thread);
        sys_base->sys_flags |= NEED_TASK_SWITCH;
        return;
    }*/

    /* Let's handle the normal case of thread preemption */
    /* Get a pointer to the highest priority ready-to-run thread */
    top_thread = (thread_t *) get_head((list_head_t *) & sys_base->thread_ready);
    /* If no more threads are ready, we must be the only running thread, so return */
    if(!top_thread)
        return;
    /* If we get this far, we need to check if the topmost thread has higher
       priority than we do, or if the time slice ended. If so, preempt. */
    if ( (sys_base->running_thread != NULL) && 
         /*(sys_base->running_thread->thread_flags & TS_READY) != 0) && */
         ( (sys_base->running_thread->node.pri < top_thread->node.pri) ||
         (sys_base->sys_flags & TIME_SLICE_EXPIRED) ) ) {

        enqueue((list_head_t*) & sys_base->thread_ready,
                (list_node_t*) sys_base->running_thread);
        sys_base->sys_flags |= NEED_TASK_SWITCH;
    }
}



/* This function gets called from the interrupt handler if a thread switch
 * is needed by testing NEED_TASK_SWITCH */
void switch_threads() {
    thread_t * running_thread = sys_base->running_thread;
    thread_t * next_thread;
    
    /* Unset the scheduling attention flags */
    sys_base->sys_flags &= (~NEED_TASK_SWITCH);
    sys_base->sys_flags &= (~TIME_SLICE_EXPIRED);
    /* Unset the thread's running flag, for the dispatcher to know
       the thread is not running, so it will not corrupt it's stack */
    running_thread->thread_flags &= (~TS_RUN);
    /* Try to get a thread structure from the ready queue */
    while(! (next_thread = (thread_t*) get_head((list_head_t *)&sys_base->thread_ready))) {
        /* If we get inside this loop, no thread is ready to run,
           and the timeslice counter has expired, so idle the processor
           until an interrupt comes and readies a thread */
        sys_base->sys_flags |= NEED_SCHEDULE; // Set the rescheduling flag
        enable();  // Enable interrupts
        asm volatile("hlt"); // Halt the processor

        /* At this time, the processor is halted and k_reenter == 0.
           Whenever an interrupt fires, k_reenter is incremented when
           the kernel is entered, and decremented when it exits.
           The scheduler can only be called when k_reenter == 0,
           because it means it is the last level before the return to
           ring 3 (k_reenter == -1) */
    }
    /* There is a thread to be run - unlink it from the list */
    remove((list_node_t *) next_thread);
    /* Set the running thread to be the new thread */
    running_thread = sys_base->running_thread = next_thread;
    /* Prepare the page directory for the dispatcher to change them */
    sys_base->act_page_directory = running_thread->page_directory;
    /* Restart the timeslice counter and set the thread as running */
    sys_base->ts_curr_count = sys_base->std_ts_quantum;
    running_thread->thread_flags |= TS_RUN;
    /* Set the thread's kernel stack for the interrupt handlers */
    set_kernel_stack(running_thread->init_kernel_esp);
    //k_reenter--;
}

/* Function to find a thread by name*/
thread_t * find_thread(char * name)  {
    /* If the thread is running, return it.
       This only makes sense if to avoid wreaking havoc in a int handler */
    if(strcmp(sys_base->running_thread->node.name, name) == 0)
        return sys_base->running_thread;

    forbid();
    thread_t * thread = (thread_t*) get_head((list_head_t *) &sys_base->thread_wait);

    while(thread){
        if(strcmp(thread->node.name, name) == 0)
        {
            permit();
            return thread;
        }
        thread = (thread_t*) get_next((list_node_t *) thread);
    }

    thread = (thread_t*) get_head((list_head_t *) &sys_base->thread_ready);

    while(thread){
        if(strcmp(thread->node.name, name) == 0)
        {
            permit();
            return thread;
        }
        thread = (thread_t*) get_next((list_node_t *) thread);
    }

    permit();
    return NULL;
}