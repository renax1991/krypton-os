#include "thread.h"
#include "sysbase.h"
#include "pmm.h"
#include "common.h"

extern void switch_context(thread_t *);

thread_t *create_thread(int (*fn)(void*), void *arg,
        uint32_t *stack, const char * name, int pri) {
    forbid();
    // First check user permissions
    if (sys_base->running_thread->uid != 0)
        return NULL;

    thread_t * new_thread = (thread_t *) kmalloc(sizeof (thread_t));
    memset (new_thread, 0, sizeof (thread_t));
    // Fill in the new thread name
    new_thread->node.name = (char *) kmalloc(strlen(name) + 1);
    strcpy(new_thread->node.name, name);
    new_thread->node.pri = pri;
    new_thread->node.type = NT_THREAD;

    *--stack = (uint32_t)arg;
    *--stack = (uint32_t)0; // Fake return address.
    *--stack = (uint32_t)fn;

    new_thread->esp = (uint32_t)stack;
    //new_thread->eflags = 0x200; // Interrupts enabled.
    new_thread->page_directory = sys_base->act_page_directory;

    enqueue((list_head_t *) &sys_base->thread_ready,
            (list_node_t *) new_thread);

    permit();
    return new_thread;
}

void forbid(){
    asm volatile("cli");
    sys_base->forbid_counter++;
    asm volatile("sti");
}

void permit(){
    asm volatile("cli");
    sys_base->forbid_counter--;
    asm volatile("sti");
}

void schedule() {
    /* This is the scheduler, called by one of the 2 interrupt handlers.
     * We will check if the running thread has an event to be processed
     * or there is a thread with an higher priority to be ran.
     * If yes, we will enqueue the current thread so it may be redispatched
     * with the right entry point */

    /* If multitasking is disabled, return immediately */
    if(sys_base->forbid_counter > 0)
        return;
    /* Get a pointer to the highest priority ready-to-run thread */
    thread_t * top_thread =
            (thread_t *) get_head((list_head_t *) & sys_base->thread_ready);

    if (((sys_base->running_thread->thread_flags & TB_EXCEPTION_WAITING) != 0)
            || (top_thread != NULL &&
            (sys_base->running_thread->node.pri < top_thread->node.pri))
            || (sys_base->sys_flags & TIME_SLICE_EXPIRED != 0)) {
        enqueue((list_head_t*) & sys_base->thread_ready,
                (list_node_t*) sys_base->running_thread);
        sys_base->sys_flags != NEED_TASK_SWITCH;
    }

    /* If the task ready queue is empty, we will return right away */
}


/* This function gets called from the interrupt handler if a thread switch
 * is needed by testing NEED_TASK_SWITCH */
void switch_threads() {
    thread_t * running_thread = sys_base->running_thread;
    thread_t * next_thread =
            (thread_t *)remove_head((list_head_t *)&sys_base->thread_ready);
    running_thread->thread_flags &= (~TS_RUN);
    running_thread->thread_flags |= TS_READY;
    sys_base->act_page_directory = running_thread->page_directory;
    sys_base->ts_curr_count = sys_base->std_ts_quantum;
    sys_base->sys_flags &= (~TIME_SLICE_EXPIRED);
    running_thread->thread_flags &= (~TS_READY);
    running_thread->thread_flags |= TS_RUN;
}