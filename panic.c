//
// panic.c -- Defines the interface for bringing the system to an abnormal halt.
//            Written for JamesM's kernel development tutorials.
//      Rewritten for Krypton kernel

#include "panic.h"
#include "common.h"
#include "elf.h"
#include "kprintf.h"
#include "idt.h"

static void print_stack_trace ();
static void keyboard_reset_on_panic(registers_t * regs);

extern elf_t kernel_elf;

void panic (const char *msg)
{
  kprintf ("*** System panic: %s\n", msg);
  asm volatile("cli");
  kprintf("Press any key to reboot!");
  register_interrupt_handler(IRQ1, &keyboard_reset_on_panic);
  outb(0x21,0xfd);
  outb(0xa1,0xff);
  inb(0x60);
  asm volatile ("sti");
  for (;;) ;
}

static void keyboard_reset_on_panic(registers_t * regs)
{
  asm volatile ("mov $0, %eax; \
                 mov %eax, %cr3;");
  /* Hopefully this will not be reached */
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
