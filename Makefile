CC=~/opt/cross/bin/i586-elf-gcc
LDSCRIPT=linker.ld
LDFLAGS=-g -ffreestanding -O2 -nostdlib
CCFLAGS=-g -std=gnu99 -ffreestanding -O2 -Wall -Wextra
KERNEL=kry_kern
C_SRC=$(wildcard *.c)
ASM_SRC=$(wildcard *.asm)
OBJ=$(C_SRC:.c=.o) $(ASM_SRC:.asm=.o)
NASM_FLAGS=-felf

all: $(KERNEL)
	@echo Done.

$(KERNEL): $(OBJ)
	@echo Linking kernel image "$(KERNEL)"...
	@$(CC) -T $(LDSCRIPT) -o $@ $(LDFLAGS) $^ -lgcc
	
%.o: %.c
	@echo [CC] $<
	@$(CC) -c $< -o $@ $(CCFLAGS)

%.o: %.asm
	@echo [ASM] $<
	@nasm $(NASM_FLAGS) -o $@ $<
	
clean:
	@rm -rf *.o
	@echo Cleaned up.
