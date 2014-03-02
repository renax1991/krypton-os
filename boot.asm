; Declare constants used for creating a multiboot header.
MBALIGN     equ  1<<0                   ; align loaded modules on page boundaries
MEMINFO     equ  1<<1                   ; provide memory map
VIDEOMODE   equ  1<<2                   ; use GRUB 2 to initialize video card
FLAGS       equ  MBALIGN | MEMINFO      ; this is the Multiboot 'flag' field
MAGIC       equ  0x1BADB002             ; 'magic number' lets bootloader find the header
CHECKSUM    equ -(MAGIC + FLAGS)        ; checksum of above, to prove we are multiboot


global mboot                    ; Make 'mboot' accessible from C.

; Declare a header as in the Multiboot Standard. We put this into a special
; section so we can force the header to be in the start of the final program.
; You don't need to understand all these details as it is just magic values that
; is documented in the multiboot standard. The bootloader will search for this
; magic sequence and recognize us as a multiboot kernel.
section .multiboot
mboot:
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

; The linker script specifies _start as the entry point to the kernel and the
; bootloader will jump to this position once the kernel has been loaded. It
; doesn't make sense to return from this function as the bootloader is gone.
section .text
bits 32
global _start
_start:
	; Our kernel will run in the higher half of the virtual address space (0xCOOOOOOO),
	; so we need to provide a fake GDT to make the processor think we're loaded there,
	; before we start running C code
	
	; Here's the trick: we load a GDT with a base address
	; of 0x40000000 for the code (0x08) and data (0x10) segments
	
	lgdt [trickgdt]			; Load the fake GDT and
	mov ax, 0x10			; load all data segment registers
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
 
	; jump to the higher half kernel
	jmp 0x08:higherhalf
 
higherhalf:
	; To set up a stack, we simply set the esp register to point to the top of
	; our stack (as it grows downwards).
	mov esp, stack_top
	

	push ebx		; Push GRUB2 multiboot structure pointer
    mov ebp, 0      ; Zero ebp to initialize the stack unwinder
	
	; We are now ready to actually execute C code. We cannot embed that in an
	; assembly file, so we'll create a init.c file in a moment. In that file,
	; we'll create a C entry point called init and call it here.
	extern init
	call init
 
	; In case the function returns, we'll want to put the computer into an
	; infinite loop. To do that, the halt instruction ('hlt') to stop the CPU until
	; the next interrupt arrives, and jumping to the halt instruction if it ever
	; continues execution, just to be safe.
.hang:
	hlt
	jmp .hang
	
[global gdt_flush] ; make 'gdt_flush' accessible from C code
[global idt_load] ; make 'idt_load' accessible from C code
[extern gp]        ; tells the assembler to look at C code for 'gp'
[extern iptr]        ; tells the assembler to look at C code for 'iptr'
 
; this function does the same thing of the 'start' one, this time with
; the real GDT
gdt_flush:
	lgdt [gp]			; load GDT pointer into register
	mov ax, 0x10		; load data selector onto all
	mov ds, ax			; segment selectors
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	jmp 0x08:flush2		; do a far jump to flush the cache
 
flush2:
	ret
	
idt_load:
	lidt [iptr]
	ret
	
[section .setup] ; tells the assembler to include this data in the '.setup' section
 
trickgdt:
	dw gdt_end - gdt - 1 ; size of the GDT
	dd gdt ; linear address of GDT
 
gdt:
	dd 0, 0							; null gate
	db 0xFF, 0xFF, 0, 0, 0, 10011010b, 11001111b, 0x40	; code selector 0x08: base 0x40000000, limit 0xFFFFFFFF, type 0x9A, granularity 0xCF
	db 0xFF, 0xFF, 0, 0, 0, 10010010b, 11001111b, 0x40	; data selector 0x10: base 0x40000000, limit 0xFFFFFFFF, type 0x92, granularity 0xCF
 
gdt_end:
	
	; Currently the stack pointer register (esp) points at anything and using it may
	; cause massive harm. Instead, we'll provide our own stack. We will allocate
	; room for a small temporary stack by creating a symbol at the bottom of it,
	; then allocating 4 kbytes for it, and finally creating a symbol at the top.
section .bss
align 4
stack_bottom:
resb 0x1000
stack_top:
