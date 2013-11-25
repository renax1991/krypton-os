/*
 *  cpu.cpp
 *  
 *
 *  Created by Renato Encarnação on 13/11/08.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

#include "cpu.h"

// We'll need at least 3 entries in our GDT...

struct gdt_entry gdt[5];
struct gdt_ptr gp;

struct idt_entry idt[256];
struct idt_ptr iptr;


// Extern assembler function
extern void gdt_flush();
extern void idt_load();

void enable(){
    asm volatile ("sti");
}

void disable(){
    asm volatile ("cli");
}

// Very simple: fills a GDT entry using the parameters
static void gdt_set_gate(int num, unsigned long base, unsigned long limit,
        unsigned char access, unsigned char gran)
{
	gdt[num].base_low = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].base_high = (base >> 24) & 0xFF;
	
	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].granularity = ((limit >> 16) & 0x0F);
	
	gdt[num].granularity |= (gran & 0xF0);
	gdt[num].access = access;
}

// Sets our 3 gates and installs the real GDT through the assembler function
void gdt_install()
{
	gp.limit = (sizeof(struct gdt_entry) * 5) - 1;
	gp.base = (unsigned int)&gdt;
	
	gdt_set_gate(0, 0, 0, 0, 0);
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
	
	gdt_flush();
}

void idt_install()
{
	int i;
	
	iptr.limit = (sizeof(struct idt_entry) * 256) - 1;
	iptr.base = (unsigned int)&idt;
	
	for (i=0; i < 256; i++) {
		idt[i].offset_1 = 0;
		idt[i].selector = 0;
		idt[i].zero = 0;
		idt[i].type_attr = 0;
		idt[i].offset_2 = 0;
	}
}
