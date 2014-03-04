;
; idt.asm -- contains interrupt descriptor table setup code.
;          Based on code from Bran's kernel development tutorials.
;          Rewritten for JamesM's kernel development tutorials.

[extern sys_base]

global idt_flush:function idt_flush.end-idt_flush ; Allows the C code to call idt_flush().
idt_flush:
    mov eax, [esp+4]  ; Get the pointer to the IDT, passed as a parameter. 
    lidt [eax]        ; Load the IDT pointer.
    ret
.end:
        
; This macro creates a stub for an ISR which does NOT pass it's own
; error code (adds a dummy errcode byte).
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli
    push 0                      ; Push a dummy error code.
    push %1                     ; Push the interrupt number.
    jmp isr_common_stub         ; Go to our common handler code.
%endmacro

; This macro creates a stub for an ISR which passes it's own
; error code.
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli
    push %1                     ; Push the interrupt number
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31
ISR_NOERRCODE 255

; C function in idt.c
extern idt_handler

global isr_common_stub:function isr_common_stub.end-isr_common_stub

; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
isr_common_stub:
    pusha                    ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov ax, ds               ; Lower 16-bits of eax = ds.
    push eax                 ; Save the data segment descriptor

    mov ax, 0x10             ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp    	     ; Push a pointer to the current top of stack - this becomes the registers_t* parameter.
    call idt_handler         ; Call into our C code.
    add esp, 4		     ; Remove the registers_t* parameter.

    jmp dispatch       ; Drop into the dispatcher
.end:


; This macro creates a stub for an IRQ - the first parameter is
; the IRQ number, the second is the ISR number it is remapped to.
%macro IRQ 2
  global irq%1
  irq%1:
    cli
    push byte 0
    push byte %2
    jmp irq_common_stub
%endmacro

IRQ   0,    32
IRQ   1,    33
IRQ   2,    34
IRQ   3,    35
IRQ   4,    36
IRQ   5,    37
IRQ   6,    38
IRQ   7,    39
IRQ   8,    40
IRQ   9,    41
IRQ  10,    42
IRQ  11,    43
IRQ  12,    44
IRQ  13,    45
IRQ  14,    46
IRQ  15,    47
        
; C function in idt.c
extern irq_handler
extern switch_threads

global irq_common_stub:function irq_common_stub.end-irq_common_stub

; This is our common IRQ stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
irq_common_stub:
    pusha                    ; Save the context

    mov ax, ds               ; Lower 16-bits of eax = ds.
    push eax                 ; Save the data segment descriptor

    mov ax, 0x10             ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp    	         ; Push a pointer to the current top of stack
                             ; this becomes the registers_t* parameter.
    call irq_handler         ; Call into our C code.
    add esp, 4		         ; Remove the registers_t* parameter

    jmp dispatch           ; Drop into the dispatcher
.end:

GLOBAL tss_flush   ; Allows our C code to call tss_flush().
tss_flush:
   mov ax, 0x2B      ; Load the index of our TSS structure - The index is
                     ; 0x28, as it is the 5th selector and each is 8 bytes
                     ; long, but we set the bottom two bits (making 0x2B)
                     ; so that it has an RPL of 3, not zero.
   ltr ax            ; Load 0x2B into the task state register.
   ret

GLOBAL enter_user_mode
enter_user_mode:
     ;JamesM tutorial says you need to disable interrupts. But really I see no need for it
     mov ax,0x23
     mov ds,ax
     mov es,ax
     mov fs,ax
     mov gs,ax ;we don't need to worry about SS. it's handled by iret

     mov eax,esp
     push 0x23 ;user data segment with bottom 2 bits set for ring 3
     push eax ;push our current stack just for the heck of it
     pushf
     pop ebx                  ; Get the stored cpu flags
     or ebx, 0x200            ; Set the IF flag to enable ints
     push ebx                 ; Store the cpu flags again
     push 0x1B; ;user data segment with bottom 2 bits set for ring 3
     push user_mode ;may need to remove the _ for this to work right
     iret
user_mode:
     ret

;***************************************************
; Here comes the code to dispatch a thread!
;***************************************************
dispatch:
    mov ebp, [sys_base]      ; Dereference sys_base pointer
    mov eax, [ebp+8]         ; Read k_reenter
    cmp eax, 0               ; If k_reenter > 0
    jne no_switch            ; Must NOT switch tasks!
    mov eax, [ebp]           ; Read sys_flags into eax
    bt eax, 2                ; Test if NEED_TASK_SWITCH is set in sys_flags
    jnc no_switch            ; Don't switch if not set
    mov eax, [ebp+4]         ; Fetch the running_thread node
    mov ebx, [eax+28]        ; Get the thread flags
    bt ebx, 0                ; Test if the thread was running
    jnc was_halt             ; If not, the CPU was halted before the interrupt
    mov [eax+40], esp        ; Save running_thread kernel-mode stack pointer
was_halt:
    call switch_threads      ; Call our C function to fetch the new thread
    mov eax, [ebp+4]         ; Fetch the NEW running_thread node
    mov esp, [eax+40]        ; Restore new kernel-mode stack pointer
    mov ebx, [eax+24]        ; Retrieve the new thread's page directory
    mov cr3, ebx             ; and load it into CR3
    mov ebx, [eax+28]        ; Get NEW the thread flags
    bt ebx, 4                ; Test if the flag TB_LAUNCH is set
    jnc no_launch            ; Skip thread launching code if not set

;***************************************************
; To launch a new thread, we must fake a stack
; frame for the iret instruction to do the job,
; as if we were returning from an exception.
;***************************************************
    push 0x23                ; Store the user stack segment selector
    mov ebx, [eax+20]        ; Get the starting user esp of the thread
    push ebx                 ; Store it on the stack for iret to fetch
    pushf                    ; Push cpu flags on the stack
    pop ebx                  ; Get the stored cpu flags
    or ebx, 0x200            ; Set the IF flag to enable ints
    push ebx                 ; Store the cpu flags again
    push 0x1B                ; Push the user code segment selector
    mov ebx, [eax+32]        ; Get the starting eip of the thread
    push ebx                 ; Store it on the stack for iret to fetch
    sub dword [ebp+8], 1     ; Decrement k_reenter
    mov ebp, 0               ; Clear the base pointer
    mov bx, 0x23             ; Load the remaining user segment selectors
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    and dword [eax+28], 0xFFFFFFEF ; Clear the TB_LAUNCH flag
    jmp launch              ; Launch the new thread!
no_launch:
    and dword [eax+28], 0xFFFFFFE7 ; Clear the TB_LAUNCH and TB_EXCEPT flags
no_switch:                  ; Jumps here if no thread switch needed
    sub dword [ebp+8], 1     ; Decrement k_reenter
    pop ebx                  ; Reload the original data segment descriptor
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    popa                     ; Restore the thread's context
    add esp, 8               ; Cleans up the pushed error code and pushed ISR number
launch:
    iret                     ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP