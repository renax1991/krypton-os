/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader. */
	. = 1M;
	
	.setup :
	{
		*(.multiboot)
		*(.setup)
	}
	
	. += 0xC0000000;  /* Now we put the sections to be loaded at the higher half */

	.text : AT(ADDR(.text) - 0xC0000000)
	{
		*(.text)
	}

	.data ALIGN (4096) : AT(ADDR(.data) - 0xC0000000)
	{
		*(.data)
		*(.rodata*)
	}

	.bss ALIGN (4096) : AT(ADDR(.bss) - 0xC0000000)
	{
		*(COMMON*)
		*(.bss*)
	}

        end = .; _end = .; __end = .;

	/* The compiler may produce other sections, by default it will put them in
	   a segment with the same name. Simply add stuff here as needed. */
}