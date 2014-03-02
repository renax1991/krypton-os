/* message.h - Krypton IPC messaging include file */

#ifndef MESSAGE_H
#define MESSAGE_H

#include "common.h"
#include "thread.h"

/* Maximum number of messages in a port */
#define MAX_MESSAGES 16
#define MAX_MSG_SZ 64

#define MF_UNREAD 1

struct message_s {
	list_node_t node;
	uint8_t flags;
	uint8_t msg_buf[MAX_MSG_SZ];
};

typedef struct message_s message_t;

/* Post a message (non-blocking) on a port */
int _msg_post(thread_t *, uint8_t * src_msg_buf, uint16_t buf_sz);

/* Retrieve a message form the thread's local port */
message_t * _msg_retrieve();

/* Delete a retrieved (or not) message */
void _msg_cycle();

#endif /* MESSAGE_H */