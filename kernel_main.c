/*
 *  kernel_main.c
 *  
 *
 *  Created by Renato Encarnação on 13/11/03.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */


#if !defined(__cplusplus)
#include <stdbool.h> /* C doesn't have booleans by default. */
#endif
#include <stddef.h>
#include "cpu.h"
#include "pmm.h"
#include "common.h"
#include "monitor.h"

#define FRAMEBUFFER_VIRTUAL 0xA0000000

struct vbe_controller_info_t {
	char signature[4];             // == "VESA"
	short version;                 // == 0x0300 for VBE 3.0
	short oem_string[2];            // isa vbeFarPtr
	unsigned char capabilities[4];
	short videomodes[2];           // isa vbeFarPtr
	short total_memory;             // as # of 64KB blocks
} __attribute__((packed));

struct multiboot_info_t {
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmd_line;
	uint32_t mods_count;
	uint32_t mods_addr;
} __attribute__((packed));

struct vbe_mode_info_t {
	uint16_t attributes;
	uint8_t winA,winB;
	uint16_t granularity;
	uint16_t winsize;
	uint16_t segmentA, segmentB;
	void * realFctPtr;
	uint16_t pitch; // bytes per scanline
	
	uint16_t Xres, Yres;
	uint8_t Wchar, Ychar, planes, bpp, banks;
	uint8_t memory_model, bank_size, image_pages;
	uint8_t reserved0;
	
	uint8_t red_mask, red_position;
	uint8_t green_mask, green_position;
	uint8_t blue_mask, blue_position;
	uint8_t rsv_mask, rsv_position;
	uint8_t directcolor_attributes;
	
	uint32_t physbase;  // your LFB (Linear Framebuffer) address ;)
	uint32_t reserved1;
	short reserved2;
} __attribute__((packed));

/* Check if the compiler thinks if we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif


void video_init(struct vbe_mode_info_t * vmode_info)
{
	uint8_t * framebuffer_phys = (uint8_t *) vmode_info->physbase;
        uint8_t * framebuffer_virtual = (uint8_t *) FRAMEBUFFER_VIRTUAL;
	int xmax = vmode_info->Xres, ymax = vmode_info->Yres;
        uint8_t * pixel = framebuffer_virtual;

        int i, j, fb_pages = (ymax * vmode_info->pitch + 1)/4096;

        for( i = 0; i <= fb_pages; i++) {
            mm_map((void*)((unsigned long)framebuffer_phys + i * 4096),
                   (void*)((unsigned long)FRAMEBUFFER_VIRTUAL + i*4096),
                    PAGE_USER | PAGE_WRITE);
        }
	
	for(i = 0; i < ymax; i++) {
		for(j = 0; j < xmax; j++) {
			pixel[j*2] = 0x10;
			pixel[j*2+1] = 0x42;
		}
		pixel += vmode_info->pitch;
	}
}

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

void init(struct multiboot_info_t * boot_info,
          struct vbe_controller_info_t * vbe_info,
          struct vbe_mode_info_t * vmode_info,
          int panic_info)
{
        // Initialize the memory manager
        mm_init(boot_info->mem_upper);
        init_idt();
        
        // We need to know if we're given a valid VESA data structure
	// Let's check if we have the VESA signature
        if(strcmp(vbe_info->signature, "VESA") != 0)
            for(;;);

	// Initialize the framebuffer by painting it dark gray
	video_init(vmode_info);
}
