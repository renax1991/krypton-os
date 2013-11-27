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

/* Check if the compiler thinks if we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

#define STD_TS_QUANTUM  20  // standard timeslicing quantum is 20ms

elf_t kernel_elf;
extern void *kernelpagedirPtr;
thread_t * kernel_thread;

int demo_thread(void* niente);

const char * kernel_thread_name = "krypton.library";

/* init - KryptonOS full initialization function 
                This function parses the Multiboot information,
                loads the system device drivers and exits.

                Things we will do:
                        - Paint the screen dark gray, for the world to know we're here.
                        - Initialize the interrupt system
                        - Initialize the virtual memory manager and reset the flat memory model.
                        - Initialize the multitasking system
                        _ Identify loaded modules and spawn a process for each one of them. Paint screen red
                if failed.
                        - Enumerate PCI devices and load their drivers, if needed.
                        - Paint the screen a lighter gray
                        - Load the VFS manager and turn on interrupts.
                At this time, the drivers' processes and the VFS manager
                will be waiting to be run in the waiting task queue. When we turn on the interrupts the
                timer interrupt will begin fireing and the multitasking system is online!
                        - Terminate this task by issuing exit().
 */

void init(multiboot_t * boot_info,
        struct vbe_controller_info_t * vbe_info,
        struct vbe_mode_info_t * vmode_info,
        int panic_info) {
    int i, *j;
    int *k[100];
    // Initialize the memory manager and interrupts
    mm_init(boot_info);

    // We need to know if we're given a valid VESA data structure
    // Let's check if we have the VESA signature
    if (strcmp(vbe_info->signature, "VESA") != 0)
        for (;;);

    // Initialize the framebuffer by painting it dark gray
    video_init(vmode_info);
    kernel_elf = elf_from_multiboot(boot_info);

    monitor_clear();
    sys_base->forbid_counter = 1;
    sys_base->sys_flags = 0;

    kprintf("KRYPTON operating system and libraries\n");
    kprintf("Release 1 (C) ERA Team\n");
    kprintf("Multiboot info found @ 0x%X\n", (unsigned long) boot_info);
    kprintf("Physical memory stack @ 0x%X\n", sys_base->mm_free_page_stack_ptr);
    
    uint32_t a = boot_info->mmap_addr;

    kprintf("******* Memory Map @ 0x%X *******\n", a);

    while (a < boot_info->mmap_addr + boot_info->mmap_length) {
        mmap_entry_t *me = (mmap_entry_t*) a;

        // Does this entry specify usable RAM?
        if (me->type == 1) {
            // For every page in this entry, add to the free page stack.
            kprintf("Free space @ 0x%X, size %d kbytes\n", me->base_addr_low,
                    (me->base_addr_low + me->length_low)/1024);
        }

        // The multiboot specification is strange in this respect - the size member does not include "size" itself in its calculations,
        // so we must add sizeof (uint32_t).
        a += me->size + sizeof (uint32_t);
    }

    kprintf("************************************\n");
    kprintf("krypton.library ELF symbols @ 0x%X\n", kernel_elf);
    kprintf("Free Memory = %d kB\n", sys_base->free_pages * 4);

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
    // Initialize the timer period to 1ms
    init_timer(1000);

    // Now we will prepare the first thread of the system - the kernel thread
    kernel_thread = (thread_t*) kmalloc(sizeof(thread_t));

    kernel_thread->node.name = kernel_thread_name; // Set as krypton.library
    kernel_thread->node.pri = -100; // Set low priority
    kernel_thread->node.type = NT_THREAD;   // Set node type
    new_list((list_head_t *)&kernel_thread->msg_port); // Set message port
    kernel_thread->thread_flags = TS_RUN; // Set status to running
    kernel_thread->uid = 0;
    enqueue((list_head_t *) &sys_base->thread_ready, (list_node_t *) kernel_thread);

    // Set the actual running thread as the kernel thread
    sys_base->running_thread = kernel_thread;
    // Set the timeslicing quantum
    sys_base->std_ts_quantum = STD_TS_QUANTUM;
    sys_base->ts_curr_count = STD_TS_QUANTUM;
    kprintf("Standard timeslicing quantum is %dms\n", sys_base->std_ts_quantum);

    sys_base->act_page_directory = kernel_thread->page_directory;
    
    // Set the forbid counter to 0 to allow multitasking to happen
    permit();
    kprintf("sys_base->forbid_counter = %d\n", sys_base->forbid_counter);
    // This will be an historic moment: we turn the interrupts on
    // After this line the multithreading system is effectively ONLINE!
    asm volatile("sti");

    /*if(!create_thread(demo_thread, NULL, (uint32_t *) kmalloc(1000),
                    "thread1.task", 100))
        panic("can't create new thread");*/
    kprintf("1");

    // This is the end of the road
}

int demo_thread(void* niente)
{
    for(;;) kprintf("2");
}
