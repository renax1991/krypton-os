/* message.c - Krypton IPC messaging implementation */

#include "message.h"
#include "sysbase.h"

void _msg_post(thread_t * thread, uint8_t * src_msg_buf, uint16_t buf_sz) {
	message_t * msg;

	// Check bounds
	if(buf_sz > MAX_MSG_SZ)
		return 2;

	// Check for a full port
	if(thread->msg_port.num_msg >= MAX_MESSAGES)
	{
		// block_thread(...);
	}

	msg = (message_t *) kmalloc(sizeof(message_t));
	if(!msg)
		panic("Out of memory posting a message.\n");

	memcpy(msg->msg_buf, src_msg_buf, buf_sz);
	add_tail((list_head_t *) &thread->msg_port.message_list, (list_node_t *) msg);
	thread->msg_port.num_msg++;
	_signal(thread, ST_MESG);
}

/* System call wrapper with struct-based return. */
message_t * msg_retrieve()
{
	message_t * volatile return_message = NULL;

    do {
    	asm volatile("mov $2, %%eax; \
                  mov %0, %%ebx; \
                  int $0xFF;" :: "r" (&return_message) : "%eax", "%ebx");
    	if(!return_message)
    		wait(ST_MESG);
    	else
    		break;
    } while(1);

    return return_message;
}

/* System call wrapper with no argument. */
void msg_cycle()
{
    asm volatile("mov $3, %%eax; \
                  int $0xFF;" ::: "%eax");

}

void _msg_retrieve(message_t ** msg_ptr) {
	*msg_ptr = (message_t *) get_head((list_head_t*)&sys_base->running_thread->msg_port.message_list);
}

void _msg_cycle()
{
	message_t * msg = remove_head((list_head_t*)&sys_base->running_thread->msg_port.message_list);

	if(!msg)
		return;
	kfree(msg);
}

int _signal(thread_t * thread, int sig_flags) {
	if(!thread || !(thread->sig_wait) || (thread->sig_wait & sig_flags) == 0)
		return -1;

	thread->sig_recvd |= sig_flags;
	thread->sig_wait &= (~sig_flags);

	remove((list_node_t*) thread);
    enqueue((list_head_t*) &sys_base->thread_ready,
            (list_node_t*) thread);
    thread->thread_flags &= (~TS_WAIT);
    thread->thread_flags |= TS_READY;
    sys_base->sys_flags |= NEED_SCHEDULE;

    return 0;
}