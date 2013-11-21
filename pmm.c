#include "pmm.h"
#include "cpu.h"
#include "sysbase.h"
#include "monitor.h"

#define PM_STACK_ADDR   (unsigned long *) 0xFF000000
#define SYS_BASE        (struct sys_base_t *) 0x80000000

extern unsigned int end;

unsigned long kernelpagedir[1024] __attribute__ ((aligned (4096)));
unsigned long lowpagetable[1024] __attribute__ ((aligned (4096)));

void *kernelpagedirPtr = 0;

unsigned int vm_online;
struct sys_base_t * sys_base;

// This function fills the page directory and the page table,
// then enables paging by putting the address of the page directory
// into the CR3 register and setting the 31st bit into the CR0 one
void init_paging()
{
	// Pointers to the page directory and the page table
	
	void *lowpagetablePtr = 0;
	int k = 0;

	// Translate the page directory from
	// virtual address to physical address
	kernelpagedirPtr = (char *)kernelpagedir + 0x40000000;
        // Same for the page table
	lowpagetablePtr = (char *)lowpagetable + 0x40000000;

	// Counts from 0 to 1023 to...
	for (k = 0; k < 1024; k++)
	{
                // ...map the first 4MB of memory into the page table...
		lowpagetable[k] = (k * 4096) | 0x3;
                // ...and clear the page directory entries
		kernelpagedir[k] = 0;
	}

	// Fills the addresses 0...4MB and 3072MB...3076MB
	// of the page directory with the same page table

	kernelpagedir[0] = ((unsigned long)lowpagetablePtr) | 0x3;
	kernelpagedir[768] = ((unsigned long)lowpagetablePtr) | 0x3;

        // Self-reference the page directory for later easy access
	kernelpagedir[1023] = ((unsigned long)kernelpagedirPtr) | 0x3;

	// Copies the address of the page directory into the CR3 register and,
        // finally, enables paging!

	asm volatile (	"mov %0, %%eax\n"
                        "mov %%eax, %%cr3\n"
                	"mov %%cr0, %%eax\n"
                	"orl $0x80000000, %%eax\n"
			"mov %%eax, %%cr0\n" :: "m" (kernelpagedirPtr));
}


/* mm_init() - memory manager initialization function
 *
 * This calls init_paging to rudely map some space into the PD,
 * places sys_base into place and bootstraps the page allocator.
 * Then it sets up the public memory heap and maps sys_base to it's
 * beginning.
 *
 */
void mm_init(int megs)
{
    // Unwind our starting placement address from the very end of the kernel
    unsigned int start = ((unsigned int)&end + 0x40000000);
    unsigned long *mm_stack;
    
    // initialize paging and get rid of segmentation
    init_paging();
    gdt_install();

    /* Now we will place our system base structure
     * at the very beginning of the first page */
    sys_base = (struct sys_base_t *) ((start + 0x1000) & PAGE_MASK);
    // Set the first page to be allocated
    sys_base->pm_last_page = (start + 0x2000) & PAGE_MASK;
    sys_base->vm_online = 0;
    // Setup the physical memory stack and map sys_base to it's virtual address
    mm_stack = (unsigned long *) mm_map((void*)pm_alloc(), 
                    PM_STACK_ADDR, PAGE_WRITE);

    sys_base =  (struct sys_base_t *) mm_map(sys_base, SYS_BASE, PAGE_WRITE);
    sys_base->mm_free_page_stack_ptr = mm_stack;
    
    // Initialize free page counter (memory in MB * 1024 / 4 kB per page)
    // Grub will pass the high memory area, so we will discard 1MB of RAM
    sys_base->free_pages = megs*256;
    // Tell the physical allocator that virtual memory is ONLINE!
    vm_online = 1;
}

unsigned int pm_alloc()
{
    unsigned long page_to_be_returned;

    if(sys_base->vm_online == 0 || sys_base->mm_free_page_stack_ptr == PM_STACK_ADDR)
        page_to_be_returned = sys_base->pm_last_page += 0x1000;
    else {
        page_to_be_returned = *(sys_base->mm_free_page_stack_ptr);
        (sys_base->mm_free_page_stack_ptr)--;
    }

    return page_to_be_returned;
}

void pm_free(unsigned int page)
{
    (sys_base->mm_free_page_stack_ptr)++;

    if( !get_physaddr(sys_base->mm_free_page_stack_ptr) )
        mm_map((void*)page, sys_base->mm_free_page_stack_ptr, PAGE_WRITE);

    *(sys_base->mm_free_page_stack_ptr) = page;
}

void * get_physaddr(void * virtualaddr)
{
    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = (unsigned long)virtualaddr >> 12 & 0x03FF;

    unsigned long * pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // If the page table isn't present, return null
    if( (pd[pdindex] & 0x01) == 0 )
        return (void *) 0;

    unsigned long * pt = ((unsigned long *)0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    // If the page isn't present, return null
    if( (pt[ptindex] & 0x01) == 0 )
        return (void *) 0;

    return (void *)((pt[ptindex] & ~0xFFF) + ((unsigned long)virtualaddr & 0xFFF));
}

static inline void flush_tlb(unsigned long virtualaddr)
{
   asm volatile("invlpg (%0)" ::"r" (virtualaddr) : "memory");
}

void * mm_map(void * physaddr, void * virtualaddr, unsigned int flags)
{
    if( (((unsigned long) physaddr & 0xFFF) != 0) ||
            (((unsigned long)virtualaddr & 0xFFF) != 0) )
        return 0;

    unsigned long pdindex = (unsigned long)virtualaddr >> 22;
    unsigned long ptindex = ((unsigned long)virtualaddr >> 12) & 0x03FF;
    
    if( physaddr == 0 )
        return 0;

    unsigned long * pd = (unsigned long *)0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // When it is not present, you need to create a new empty PT and
    // adjust the PDE accordingly.

    if( (pd[pdindex] & 0x01) == 0 ) // If the page table isn't present, add one
        pd[pdindex] = pm_alloc() | (flags & 0xFFF) | 0x01;
    
    unsigned long * pt = ((unsigned long *)0xFFC00000) + 0x400 * pdindex; // 0x400 ??
    // Here you need to check whether the PT entry is present.
    // When it is, then there is already a mapping present. What do you do now?
    pt[ptindex] = ((unsigned long)physaddr) | (flags & 0xFFF) | 0x01; // Present

    // Now you need to flush the entry in the TLB
    // to validate the change.

    flush_tlb((unsigned long)virtualaddr);

    return virtualaddr;
}
