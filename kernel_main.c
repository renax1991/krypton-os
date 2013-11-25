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

elf_t kernel_elf;
extern struct sys_base_t *sys_base;

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
                This will be a historic point: at this time, the drivers' processes and the VFS manager
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

    kprintf("Physical Memory = %d kbytes\n", boot_info->mem_upper + boot_info->mem_lower);
    kprintf("************************************\n");
    kprintf("krypton.library ELF symbols @ 0x%X\n", kernel_elf);
    kprintf("Free Memory = %d kB\n", sys_base->free_pages * 4);
    for (i = 0; i < 100; i++){
        k[i] = (int*) kmalloc(1000);
    }
    kprintf("Free Memory after allocs = %d kB\n", sys_base->free_pages * 4);
    kprintf("First allocation address of k[0] = 0x%X\n", k[0]);
    kprintf("Last allocation address of k[99] = 0x%X\n", k[99]);
    for (i = 0; i < 100; i++){
        kfree(k[i]);
    }
    j = (int*) kmalloc(sizeof(int));
    kprintf("Last allocation address of j = 0x%X\n", j);
    kprintf("Free Memory after frees = %d kB\n", sys_base->free_pages * 4);
}
