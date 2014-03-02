// monitor.h -- Defines the interface for monitor code
//              From JamesM's kernel development tutorials.

#ifndef MONITOR_H
#define MONITOR_H

#include "common.h"

struct vbe_controller_info_t {
	char signature[4];             // == "VESA"
	short version;                 // == 0x0300 for VBE 3.0
	short oem_string[2];            // isa vbeFarPtr
	unsigned char capabilities[4];
	short videomodes[2];           // isa vbeFarPtr
	short total_memory;             // as # of 64KB blocks
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

// Write a single character out to the screen.
void monitor_put(char c);

// Clear the screen to all black.
void monitor_clear();

// Output a null-terminated ASCII string to the monitor.
void monitor_write(char *c);
void _monitor_write(char *c);

// Output a hex value to the monitor.
void monitor_write_hex(uint32_t n);

// Output a decimal value to the monitor.
void monitor_write_dec(uint32_t n);

void monitor_init();

#endif // MONITOR_H
