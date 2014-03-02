/*
 *  kernel_main.c
 *  
 *
 *  Created by Renato Encarnação on 13/11/03.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */


#include "cpu.h"
#include "pmm.h"
#include "common.h"
#include "monitor.h"
#include "multiboot.h"
#include "elf.h"
#include "kprintf.h"
#include "sysbase.h"
#include "idt.h"
#include "thread.h"
#include "timer.h"
#include "panic.h"
#include "message.h"

/* Check if the compiler thinks if we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

#define STD_TS_QUANTUM  10  // standard timeslicing quantum is 10 timer ticks
#define TIMER_FREQUENCY 100

extern void enter_user_mode();
elf_t kernel_elf;
extern void *kernelpagedirPtr;
thread_t * kernel_thread;

/* KBDUS means US Keyboard Layout. This is a scancode table
*  used to layout a standard US keyboard. I have left some
*  comments in to give you an idea of what key is what, even
*  though I set it's array index to 0. You can change that to
*  whatever you want using a macro, if you wish! */
unsigned char kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
  '9', '0', '-', '=', '\b', /* Backspace */
  '\t',         /* Tab */
  'q', 'w', 'e', 'r',   /* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
    0,          /* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
 '\'', '`',   0,        /* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',            /* 49 */
  'm', ',', '.', '/',   0,              /* Right shift */
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

int demo_thread(void* niente);
void protection_fault(registers_t *regs);

void keypress(registers_t *regs);
int keypress_excp_handler(void * data);
void syscall(registers_t *regs);

char * kernel_thread_name = "krypton.library";

/* init - KryptonOS full initialization function 
                This function parses the Multiboot information,
                loads the system device drivers and starts the system

                Things we will do:
                        - Initialize the interrupt system
                        - Initialize the virtual memory manager and reset the flat memory model.
                        - Initialize the multitasking system
                        _ Identify loaded modules and spawn a process for each one of them.
                        - Enumerate PCI devices and load their drivers, if needed.
                        - Paint the screen a lighter gray
                        - Load the VFS manager and turn on interrupts.
                At this time, the drivers' processes and the VFS manager
                will be waiting to be run in the waiting task queue. When we turn on the interrupts the
                timer interrupt will begin fireing and the multitasking system is online!
                        - Terminate this task by issuing thread_exit().
 */

void init(multiboot_t * boot_info) {
    except_t * test;
    thread_t * keyboard_thread;
    // Initialize the memory manager and interrupts
    mm_init(boot_info);
    kernel_elf = elf_from_multiboot(boot_info);

    monitor_init();
    sys_base->forbid_counter = 1;
    sys_base->sys_flags = 0;

    // Begin initializing the multithreading system
    // First initialize all the system's lists
    new_list((list_head_t*)&sys_base->device_list);
    new_list((list_head_t*)&sys_base->intr_list);
    new_list((list_head_t*)&sys_base->lib_list);
    new_list((list_head_t*)&sys_base->msgport_list);
    new_list((list_head_t*)&sys_base->resources_list);
    new_list((list_head_t*)&sys_base->semaphore_list);
    new_list((list_head_t*)&sys_base->thread_ready);
    new_list((list_head_t*)&sys_base->thread_wait);

    // No flags must be set
    sys_base->sys_flags = 0;

    // Now we will prepare the first thread of the system - the kernel thread
    kernel_thread = (thread_t*) kmalloc(sizeof(thread_t));

    kernel_thread->node.name = kernel_thread_name; // Set as krypton.library
    kernel_thread->node.pri = 0; // Set priority
    kernel_thread->node.type = NT_THREAD;   // Set node type
    new_list((list_head_t *)&kernel_thread->msg_port); // Set message port
    kernel_thread->thread_flags = TS_RUN; // Set status to running
    kernel_thread->uid = 0;
    kernel_thread->init_kernel_esp = (uint32_t) kmalloc(4000) + 4000;
    set_kernel_stack(kernel_thread->init_kernel_esp);

    // Set the actual running thread as the kernel thread
    sys_base->running_thread = kernel_thread;
    // Set the timeslicing quantum
    sys_base->std_ts_quantum = STD_TS_QUANTUM;
    sys_base->ts_curr_count = STD_TS_QUANTUM;


    sys_base->act_page_directory = kernel_thread->page_directory = kernelpagedirPtr;
    
    register_interrupt_handler(13, &protection_fault);
    register_interrupt_handler(IRQ1, &keypress);
    register_interrupt_handler(255, &syscall);
    
    // This will be an historic moment: we turn the interrupts on and enter
    // ring 3.
    // After this line the multithreading system is effectively ONLINE!
    // Initialize the PIT timer
    init_timer(TIMER_FREQUENCY);
    sys_base->k_reenter=-1;
    test = (except_t *) kmalloc(sizeof(except_t));
    if(!(keyboard_thread = create_thread(demo_thread, NULL, NULL,
                        ((uint32_t *) kmalloc(4000)) + 1000,
                        ((uint32_t *) kmalloc(4000)) + 1000,
                    "keyboard.device", 0)))
        panic("can't create new thread");
    /*test->node.type = IRQ1;
    test->thread_ptr = keyboard_thread;
    enqueue((list_head_t *) &sys_base->intr_list,
            (list_node_t *) test);*/
    enter_user_mode();
    permit();
    kprintf("KRYPTON Operating System and Libraries\nRevision 1, built %s\n (C) The ERA Software Team\n\n", __DATE__);
    // This is the end of the road
    wait(0);
    kprintf("*** krypton.library woken up. Panicking...\n");
    asm volatile ("hlt");
}

int demo_thread(void* niente)
{
    kprintf("Hello World from %s!\n", sys_base->running_thread->node.name);
    message_t * msg;
    int * scancode;

    while(1) {
        wait(ST_MESG);
        msg = _msg_retrieve();
        scancode = (int*)&msg->msg_buf[0];
        kprintf("%c", kbdus[*scancode]);
        _msg_cycle();
    }
}

void protection_fault(registers_t *regs) {
    uint32_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2));

    kprintf("OOPS: %s crashed at 0x%x!!\n", sys_base->running_thread->node.name, regs->eip);
    panic("not syncing - unhandled protection fault");
}

void keypress(registers_t *regs) {
    thread_t * keyboard_driver_thread = find_thread("keyboard.device");
    int scancode;

    if(!keyboard_driver_thread)
        panic("not syncing - no user-mode keyboard driver installed");

    scancode = inb(0x60);
    if (!(scancode&0x80))
        _msg_post(keyboard_driver_thread, &scancode, sizeof(int));
}

void syscall(registers_t *regs) {
    if(sys_base->running_thread->uid != 0)
        return;

    switch(regs->eax) {
        case 0x00: _wait_for_flags((uint32_t) regs->ebx); break;
        case 0x01: _monitor_write((char*) regs->ebx); break;
    }
}