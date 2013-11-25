// common.h -- Defines typedefs and some global functions.
//             From JamesM's kernel development tutorials.

#ifndef COMMON_H
#define COMMON_H

#if !defined(__cplusplus)
#include <stdbool.h> /* C doesn't have booleans by default. */
#endif
#include <stddef.h>

#define FRAMEBUFFER_VIRTUAL 0xA0000000

/* Amiga-style node definitions for the doubly linked lists*/
#define NT_MEMORY       0
#define NT_RESOURCE     1
#define NT_DEVICE       2
#define NT_LIBRARY      3
#define NT_MSGPORT      4
#define NT_THREAD       5
#define NT_INTERRUPT    6
#define NT_SEMAPOHORE   7

// Some standard typedefs, to standardise sizes across platforms.
// These typedefs are written for 32-bit X86.
typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef unsigned short uint16_t;
typedef          short int16_t;
typedef unsigned char  uint8_t;
typedef          char  int8_t;

/* Standard Amiga-style list definitions */

struct list_node_s {
	struct list_node_s  *next;
	struct list_node_s  *prev;
	uint32_t            type;
	uint32_t            pri;
	char *              name;
} __attribute__((packed));

typedef struct list_node_s  list_node_t;

struct list_head_s {
	struct list_node_s  *head;
	struct list_node_s  *tail;
	struct list_node_s  *tailPrev;
} __attribute__((packed));

typedef struct list_head_s  list_head_t;

//Macros para pilhas

#define PUSH 	add_head
#define POP 	remove_head

void add_head(list_head_t *, list_node_t *);
void add_tail(list_head_t *, list_node_t *);
void enqueue(list_head_t *, list_node_t *);

list_node_t *   remove_head(list_head_t *);
list_node_t *   remove_tail(list_head_t *);
void            new_list(list_head_t *);

list_node_t *   get_next(list_node_t *);
list_node_t *   get_head(list_head_t *);
list_node_t *   get_tail(list_head_t *);
/*
#define PANIC(msg) panic(msg, __FILE__, __LINE__);
#define ASSERT(b) ((b) ? (void)0 : panic_assert(__FILE__, __LINE__, #b))

extern void panic(const char *message, const char *file, u32int line);
extern void panic_assert(const char *file, u32int line, const char *desc);
*/

void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len);
void memset(uint8_t *dest, uint8_t val, uint32_t len);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
int strlen(char *src);
int strcmp(char *str1, char *str2);

#endif // COMMON_H
