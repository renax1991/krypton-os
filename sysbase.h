/* 
 * File:   sysbase.h
 * Author: renato
 *
 *  This file contains the sys_base_t structure definition
 *
 *
 * Created on November 15, 2013, 6:27 PM
 */

#ifndef _SYSBASE_H
#define	_SYSBASE_H

#include "pmm.h"
#include "common.h"

struct sys_base_t {
    unsigned int vm_online;
    unsigned int pm_last_page; // Address of the last never-freed page given
    unsigned long * mm_free_page_stack_ptr; // Free page stack address
    unsigned long mm_free_page_stack_max;  // Maximum free page stack address
    unsigned long free_pages; // Number of free pages
    heap_header_t * sh_heap; // Public heap address
    unsigned long sh_heap_max;
    list_head_t resources_list;
    list_head_t device_list;
    list_head_t lib_list;
    list_head_t msgport_list;
    list_head_t thread_ready;
    list_head_t thread_wait;
    list_head_t intr_list;
    list_head_t semaphore_list;
};

#endif	/* _SYSBASE_H */

