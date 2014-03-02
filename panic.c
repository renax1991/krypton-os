//
// panic.c -- Defines the interface for bringing the system to an abnormal halt.
//            Written for JamesM's kernel development tutorials.
//      Rewritten for Krypton kernel

#include "panic.h"
#include "common.h"
#include "elf.h"
#include "kprintf.h"

static void print_stack_trace ();

extern elf_t kernel_elf;

void panic (const char *msg)
{
  kprintf ("*** System panic: %s\n", msg);
  print_stack_trace ();
  kprintf ("***\n");
  asm volatile("cli");
  for (;;) ;
}

void print_stack_trace ()
{
  uint32_t *ebp, *eip;
  asm volatile ("mov %%ebp, %0" : "=r" (ebp));
  while (ebp)
  {
    eip = ebp+1;
    kprintf ("   [0x%x] %s\n", *eip, elf_lookup_symbol (*eip, &kernel_elf));
    ebp = (uint32_t*) *ebp;
  }
}
