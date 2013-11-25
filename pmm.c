/* mm.c - Krypton memory management functions
 *
 * Based on code from JamesM tutorials
 */

#include "pmm.h"
#include "cpu.h"
#include "sysbase.h"
#include "monitor.h"
#include "common.h"
#include "panic.h"
#include "kprintf.h"
#include "multiboot.h"

#define PM_STACK_ADDR   (unsigned long *) 0xFF000000
#define SH_HEAP_START   (unsigned long) 0x80000000

static inline void split_chunk(heap_header_t *chunk, uint32_t len);

static inline void alloc_chunk(uint32_t start, uint32_t len);

static inline void glue_chunk (heap_header_t *chunk);

static inline void free_chunk (heap_header_t *chunk);

extern unsigned int end;

unsigned long kernelpagedir[1024] __attribute__((aligned(4096)));
unsigned long lowpagetable[1024] __attribute__((aligned(4096)));

void *kernelpagedirPtr = 0;

unsigned int vm_online;
struct sys_base_t * sys_base;  // best to use this pointer;
struct sys_base_t SystemBaseStruct;

// This function fills the page directory and the page table,
// then enables paging by putting the address of the page directory
// into the CR3 register and setting the 31st bit into the CR0 one

void init_paging() {
    // Pointers to the page directory and the page table

    void *lowpagetablePtr = 0;
    int k = 0;

    // Translate the page directory from
    // virtual address to physical address
    kernelpagedirPtr = (char *) kernelpagedir + 0x40000000;
    // Same for the page table
    lowpagetablePtr = (char *) lowpagetable + 0x40000000;

    // Counts from 0 to 1023 to...
    for (k = 0; k < 1024; k++) {
        // ...map the first 4MB of memory into the page table...
        lowpagetable[k] = (k * 4096) | 0x3;
        // ...and clear the page directory entries
        kernelpagedir[k] = 0;
    }

    // Fills the addresses 0...4MB and 3072MB...3076MB
    // of the page directory with the same page table

    kernelpagedir[0] = ((unsigned long) lowpagetablePtr) | 0x3;
    kernelpagedir[768] = ((unsigned long) lowpagetablePtr) | 0x3;

    // Self-reference the page directory for later easy access
    kernelpagedir[1023] = ((unsigned long) kernelpagedirPtr) | 0x3;

    // Copies the address of the page directory into the CR3 register and,
    // finally, enables paging!

    asm volatile ( "mov %0, %%eax\n"
            "mov %%eax, %%cr3\n"
            "mov %%cr0, %%eax\n"
            "orl $0x80000000, %%eax\n"
            "mov %%eax, %%cr0\n" ::"m" (kernelpagedirPtr));
}

/* mm_init() - memory manager initialization function
 *
 * This calls init_paging to rudely map some space into the PD,
 * places sys_base into place and bootstraps the page allocator.
 * Then it sets up the public memory heap and maps sys_base to it's
 * beginning.
 *
 */
void mm_init(multiboot_t *mboot_ptr) {
    // Unwind our starting placement address from the very end of the kernel
    unsigned int start = ((unsigned int) & end + 0x40000000);
    unsigned long *mm_stack;
    unsigned long fake_mboot_address = (unsigned long) mboot_ptr;
    multiboot_t * fake_mboot;

    // fake the multiboot info address (we still are segmented)
    fake_mboot_address += 0x40000000;
    fake_mboot = (multiboot_t *) fake_mboot_address;

    // initialize paging, get rid of segmentation and initialize interrupts
    init_paging();
    gdt_install();
    init_idt();

    start = (mboot_ptr->mem_upper) * 1024;
    /* Now we will place our system base structure
     * at the very beginning of the first page */
    //sys_base = (struct sys_base_t *) ((start + 0x10000) & PAGE_MASK);
    // Set the first page to be allocated
    sys_base->pm_last_page = (start + 0x1000) & PAGE_MASK;
    sys_base->vm_online = 0;
    // Setup the physical memory stack
    mm_stack = (unsigned long *) mm_map((void*) pm_alloc(),
            PM_STACK_ADDR, PAGE_WRITE);

    //sys_base = (struct sys_base_t *) mm_map(sys_base, SYS_BASE, PAGE_WRITE);
    sys_base->mm_free_page_stack_ptr = mm_stack;
    sys_base->mm_free_page_stack_max = (uint32_t) mm_stack;

    // Now let's fill the page allocator stack
    // For each entry in the memory info passed by GRUB, "free" those pages

    sys_base->free_pages = 0;

    uint32_t i = mboot_ptr->mmap_addr;
    while (i < mboot_ptr->mmap_addr + mboot_ptr->mmap_length) {
        mmap_entry_t *me = (mmap_entry_t*) i;

        // Does this entry specify usable RAM?
        if (me->type == 1) {
            uint32_t j;
            // For every page in this entry, add to the free page stack.
            for (j = me->base_addr_low; j < me->base_addr_low + me->length_low; j += 0x1000) {
                pm_free(j);
            }
        }

        // The multiboot specification is strange in this respect - the size member does not include "size" itself in its calculations,
        // so we must add sizeof (uint32_t).
        i += me->size + sizeof (uint32_t);
    }

    // Tell the physical allocator that virtual memory is ONLINE!
    sys_base->vm_online = 1;

    // Now we will initialize the shared memory heap
    // Fill in the list header fields with an empty list
    sys_base->sh_heap = NULL;
    // Make the maximum heap address to be the start i.e. the heap is empty
    sys_base->sh_heap_max = SH_HEAP_START;
}

volatile void *kmalloc(uint32_t l) {
    l += sizeof (heap_header_t);
    //kprintf("Asked %d bytes\n", l);
    heap_header_t *cur_header = sys_base->sh_heap, *prev_header = 0;

    while (cur_header) {
        //kprintf("Testing chunk 0x%X, length=%d, allocated = %d\n", cur_header,
                //cur_header->length, cur_header->allocated);
        if (cur_header->allocated == 0 && cur_header->length >= l) {
            //kprintf("Heap chunk 0x%X not allocated, allocating...\n");
            split_chunk(cur_header, l);
            cur_header->allocated = 1;
            return (void*) ((uint32_t) cur_header + sizeof (heap_header_t));
        }
        prev_header = cur_header;
        //kprintf("prev_header = 0x%X\n", prev_header);
        cur_header = cur_header->next;
    }

    uint32_t chunk_start;
    if (prev_header)
        chunk_start = (uint32_t) prev_header + prev_header->length;
    else {
        chunk_start = SH_HEAP_START;
        sys_base->sh_heap = (heap_header_t *)chunk_start;
    }
    //kprintf("allocating new chunk @ 0x%X, lenght %d\n", chunk_start, l);
    alloc_chunk(chunk_start, l);
    cur_header = (heap_header_t *) chunk_start;
    cur_header->prev = prev_header;
    cur_header->next = 0;
    cur_header->allocated = 1;
    cur_header->length = l;

    prev_header->next = cur_header;

    return (void*) (chunk_start + sizeof (heap_header_t));
}

volatile void kfree (void *p)
{
  heap_header_t *header = (heap_header_t*)((uint32_t)p - sizeof(heap_header_t));
  header->allocated = 0;

  glue_chunk (header);
}

static inline void glue_chunk (heap_header_t *chunk)
{
  if (chunk->next && chunk->next->allocated == 0)
  {
    chunk->length = chunk->length + chunk->next->length;
    chunk->next->next->prev = chunk;
    chunk->next = chunk->next->next;
  }

  if (chunk->prev && chunk->prev->allocated == 0)
  {
    chunk->prev->length = chunk->prev->length + chunk->length;
    chunk->prev->next = chunk->next;
    chunk->next->prev = chunk->prev;
    chunk = chunk->prev;
  }

  if (chunk->next == 0)
    free_chunk (chunk);
}

static inline void free_chunk (heap_header_t *chunk)
{
  chunk->prev->next = 0;

  if (chunk->prev == 0)
  sys_base->sh_heap = 0;

  // While the heap max can contract by a page and still be greater than the chunk address...
  while ( (sys_base->sh_heap_max-0x1000) >= (uint32_t)chunk )
  {
    sys_base->sh_heap_max -= 0x1000;
    uint32_t page = get_physaddr (sys_base->sh_heap_max);
    pm_free (page);
    mm_unmap (sys_base->sh_heap_max);
  }
}

static inline void split_chunk(heap_header_t *chunk, uint32_t len) {
    // In order to split a chunk, once we split we need to know that there will be enough
    // space in the new chunk to store the chunk header, otherwise it just isn't worthwhile.
    if (chunk->length - len > sizeof (heap_header_t)) {
        heap_header_t *newchunk = (heap_header_t *) ((uint32_t) chunk + chunk->length);
        newchunk->prev = chunk;
        newchunk->next = 0;
        newchunk->allocated = 0;
        newchunk->length = chunk->length - len;
        kprintf("Splitted chunk 0x%X from chunk 0x%X, new lenght %d\n",
                newchunk, chunk, newchunk->length);
        chunk->next = newchunk;
        chunk->length = len;
    }
}

static inline void alloc_chunk(uint32_t start, uint32_t len) {
    while (start + len > sys_base->sh_heap_max) {
        //kprintf("Trying to map chunk 0x%x, start = 0x%X, len = %d",
        //        sys_base->sh_heap_max, start, len);
        uint32_t page = pm_alloc();
        //kprintf(" in page 0x%X\n", page);
        mm_map(page, sys_base->sh_heap_max, PAGE_WRITE);
        sys_base->sh_heap_max += 0x1000;
        //kprintf("Mapped new chunk @ 0x%X -> 0x%X\n",page, sys_base->sh_heap_max - 0x1000);
    }
}

unsigned int pm_alloc() {
    unsigned long page_to_be_returned;

    if (sys_base->vm_online == 0 || sys_base->mm_free_page_stack_ptr == PM_STACK_ADDR)
        page_to_be_returned = sys_base->pm_last_page += 0x1000;
    else {
        (sys_base->mm_free_page_stack_ptr)--;
        page_to_be_returned = *(sys_base->mm_free_page_stack_ptr);
    }
    (sys_base->free_pages)--;
    return page_to_be_returned;
}

void pm_free(unsigned int page) {
    // Sanity test to not overwrite kernel nor zeropage when freeing pages
    if( (page > (sys_base->pm_last_page - 0x10000)) ||
        (page > 0x100000 && page < 0x180000) ||
        (page < 0x20000)) return;

    if (sys_base->mm_free_page_stack_ptr >= sys_base->mm_free_page_stack_max){
        mm_map((void*) page, sys_base->mm_free_page_stack_ptr, PAGE_WRITE);
        sys_base->mm_free_page_stack_max += 4096;
    }
    else {
        *(sys_base->mm_free_page_stack_ptr) = page;
        (sys_base->mm_free_page_stack_ptr)++;
        (sys_base->free_pages)++;
    }
}

void * get_physaddr(void * virtualaddr) {
    unsigned long pdindex = (unsigned long) virtualaddr >> 22;
    unsigned long ptindex = (unsigned long) virtualaddr >> 12 & 0x03FF;

    unsigned long * pd = (unsigned long *) 0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // If the page table isn't present, return null
    if ((pd[pdindex] & 0x01) == 0)
        return NULL;

    unsigned long * pt = ((unsigned long *) 0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    // If the page isn't present, return null
    if ((pt[ptindex] & 0x01) == 0)
        return NULL;

    return (void *) ((pt[ptindex] & ~0xFFF) + ((unsigned long) virtualaddr & 0xFFF));
}

static inline void flush_tlb(unsigned long virtualaddr) {
    asm volatile("invlpg (%0)" ::"r" (virtualaddr) : "memory");
}

void * mm_map(void * physaddr, void * virtualaddr, unsigned int flags) {

    unsigned long pdindex = (unsigned long) virtualaddr >> 22;
    unsigned long ptindex = ((unsigned long) virtualaddr >> 12) & 0x03FF;
    unsigned long * pt;

    if (physaddr == 0)
        return 0;

    unsigned long * pd = (unsigned long *) 0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // When it is not present, you need to create a new empty PT and
    // adjust the PDE accordingly.

    if ((pd[pdindex] & 0x01) == 0){ // If the page table isn't present, add one
        pd[pdindex] = pm_alloc() | (flags & 0xFFF) | 0x01;
        // zero out the page table
        pt = ((unsigned long *) 0xFFC00000) + 0x400 * pdindex;
        memset(pt, 0, 4096);
    }

    pt = ((unsigned long *) 0xFFC00000) + 0x400 * pdindex; // 0x400 ??
    // Here you need to check whether the PT entry is present.
    // When it is, then there is already a mapping present. What do you do now?
    pt[ptindex] = ((unsigned long) physaddr) | (flags & 0xFFF) | 0x01; // Present

    // Now you need to flush the entry in the TLB
    // to validate the change.

    flush_tlb((unsigned long) virtualaddr);

    return virtualaddr;
}

void mm_unmap(void * virtualaddr) {

    unsigned long pdindex = (unsigned long) virtualaddr >> 22;
    unsigned long ptindex = ((unsigned long) virtualaddr >> 12) & 0x03FF;
    unsigned long * pt;
    
    pt = ((unsigned long *) 0xFFC00000) + 0x400 * pdindex; // 0x400 ??
    // Set the page table entry to 0
    pt[ptindex] = 0;

    // Now you need to flush the entry in the TLB
    // to validate the change.

    flush_tlb((unsigned long) virtualaddr);
}

inline volatile void page_fault(registers_t *regs) {
    uint32_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2));

    kprintf("Page fault at 0x%x, faulting address 0x%x\n", regs->eip, cr2);
    kprintf("Error code: %x\n", regs->err_code);
    panic("");
    for (;;);
}
