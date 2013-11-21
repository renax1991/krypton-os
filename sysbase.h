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

struct sys_base_t {
    unsigned int vm_online;     // Flag to tell whether paging is online
    unsigned int pm_last_page;  // Address of the last never-freed page given
    unsigned long * mm_free_page_stack_ptr;
    unsigned long free_pages;
};

#endif	/* _SYSBASE_H */

