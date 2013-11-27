/* 
 * File:   pmm.h
 * Author: renato
 *
 * Created on November 15, 2013, 4:10 PM
 */

#ifndef _PMM_H
#define	_PMM_H

#include "common.h"
#include "idt.h"
#include "multiboot.h"

#define PAGE_PRESENT   0x1        // Page is mapped in.
#define PAGE_WRITE     0x2        // Page is writable. Not set means read-only.
#define PAGE_USER      0x4        // Page is writable from user space. Unset means kernel-only.
#define PAGE_MASK      0xFFFFF000 // Mask constant to page-align an address.

typedef unsigned long pagedir_t;
typedef unsigned long pagetable_t;

typedef struct heap_header
{
    struct heap_header *next, *prev;   // node linkage fields
    uint32_t allocated : 1;
    uint32_t length : 31;
} __attribute__((packed)) heap_header_t;

void mm_init(multiboot_t * mboot);

unsigned int pm_alloc();

void pm_free();

void * get_physaddr(void * virtualaddr);

void * mm_map(void * physaddr, void * virtualaddr, unsigned int flags);

void mm_unmap(void * virtualaddr);

volatile void *kmalloc(uint32_t l);

volatile void kfree (void *p);

inline volatile void page_fault (registers_t *regs);

inline void switch_page_directory(void *pagetabledir_ptr);

#endif	/* _PMM_H */

