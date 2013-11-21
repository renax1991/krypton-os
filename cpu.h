/*
 *  cpu.h
 *  
 *
 *  Created by Renato Encarnação on 13/11/08.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

// Defines the structures of a GDT entry and of a GDT pointer

struct gdt_entry
{
	unsigned short limit_low;
	unsigned short base_low;
	unsigned char base_middle;
	unsigned char access;
	unsigned char granularity;
	unsigned char base_high;
} __attribute__((packed));

struct idt_entry{
	unsigned short offset_1; // offset bits 0..15
	unsigned short selector; // a code segment selector in GDT or LDT
	unsigned char zero;      // unused, set to 0
	unsigned char type_attr; // type and attributes, see below
	unsigned short offset_2; // offset bits 16..31
} __attribute__((packed));

struct idt_ptr{
	unsigned short limit;
	unsigned long base;
} __attribute__((packed));

struct gdt_ptr
{
	unsigned short limit;
	unsigned int base;
} __attribute__((packed));


/*******************************************************************
 init_paging()
 This function fills the page directory and the page table,
 then enables paging by putting the address of the page directory
 into the CR3 register and setting the 31st bit into the CR0 one
*******************************************************************/ 
void init_paging();


/*******************************************************************
 get_install()
 Sets our 3 gates and installs the real GDT through the assembler function
 *******************************************************************/
void gdt_install();
