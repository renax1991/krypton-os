/* 
 * File:   pmm.h
 * Author: renato
 *
 * Created on November 15, 2013, 4:10 PM
 */

#ifndef _PMM_H
#define	_PMM_H

#define PAGE_PRESENT   0x1        // Page is mapped in.
#define PAGE_WRITE     0x2        // Page is writable. Not set means read-only.
#define PAGE_USER      0x4        // Page is writable from user space. Unset means kernel-only.
#define PAGE_MASK      0xFFFFF000 // Mask constant to page-align an address.

typedef unsigned long pagedir_t;
typedef unsigned long pagetable_t;

void mm_init(int megs);

unsigned int pm_alloc();

void pm_free();
void * get_physaddr(void * virtualaddr);
void * mm_map(void * physaddr, void * virtualaddr, unsigned int flags);


#endif	/* _PMM_H */

