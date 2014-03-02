// monitor.c -- Defines functions for writing to the monitor.
//              heavily based on Bran's kernel development tutorials,
//              but rewritten for JamesM's kernel tutorials.

#include "monitor.h"
#include "pmm.h"
#include "common.h"

uint16_t *video_memory = (uint16_t*) 0xB8000;

// Stores the cursor position.
uint32_t cursor_x = 0;
uint32_t cursor_y = 0;

 /* void update_cursor(int row, int col)
  * by Dark Fiber
  */
void update_cursor(int row, int col)
{
   unsigned short position=(row*80) + col;

   // cursor LOW port to vga INDEX register
   outb(0x3D4, 0x0F);
   outb(0x3D5, (unsigned char)(position&0xFF));
   // cursor HIGH port to vga INDEX register
   outb(0x3D4, 0x0E);
   outb(0x3D5, (unsigned char )((position>>8)&0xFF));
}

// Scrolls the text on the screen up by one line.

static void scroll() {
    // Get a space character with the default colour attributes.
    uint8_t attributeByte = (0 /*black*/ << 4) | (15 /*white*/ & 0x0F);
    uint16_t blank = 0x20 /* space */ | (attributeByte << 8);

    if (cursor_y >= 25) {
        // Move the current text chunk that makes up the screen
        // back in the buffer by a line
        int i;
        for (i = 0; i < 80*24; i++)
            video_memory[i] = video_memory[80 + i];

        // The last line should now be blank. Do this by writing
        // 80 spaces to it.
        for (i = 80*24;
             i < 80*25; i++)
            video_memory[i] = blank;

        // The cursor should now be on the last line.
        cursor_y = 24;
    }
    update_cursor(cursor_y, cursor_x);
}

// Writes a single character out to the screen.

void monitor_write(char *c)
{
    asm volatile("mov $1, %%eax; \
                  mov %0, %%ebx; \
                  int $0xFF;" :: "r" (c) : "%eax", "%ebx");
}

void monitor_put(char c) {
    // The background colour is black (0), the foreground is white (7).
    uint8_t backColour = 0;
    uint8_t foreColour = 7;

    // The attribute byte is made up of two nibbles - the lower being the
    // foreground colour, and the upper the background colour.
    uint8_t attributeByte = (backColour << 4) | (foreColour & 0x0F);
    // The attribute byte is the top 8 bits of the word we have to send to the
    // VGA board.
    uint16_t attribute = attributeByte << 8;

    // Handle a backspace, by moving the cursor back one space
    if (c == 0x08 && cursor_x)
        cursor_x--;

        // Handle a tab by increasing the cursor's X, but only to a point
        // where it is divisible by 8.
    else if (c == 0x09)
        cursor_x = (cursor_x + 8) & ~(8 - 1);

        // Handle carriage return
    else if (c == '\r')
        cursor_x = 0;

        // Handle newline by moving cursor back to left and increasing the row
    else if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    }
        // Handle any other printable character.
    else if (c >= ' ') {
        video_memory[cursor_x + 80*cursor_y] = (uint16_t) c | attribute;
        cursor_x++;
    }

    // Check if we need to insert a new line because we have reached the end
    // of the screen.
    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }

    // Scroll the screen if needed.
    scroll();
    update_cursor(cursor_y, cursor_x);
}

// Clears the screen, by copying lots of spaces to the framebuffer.

void monitor_clear() {

    int i;
    for (i = 0; i < 80*25; i++) {
         video_memory[i] = 0;
    }

    // Move the hardware cursor back to the start.
    cursor_x = 0;
    cursor_y = 0;
    update_cursor(cursor_y, cursor_x);
}

void monitor_init() {
   unsigned short position = 0;
   // cursor LOW port to vga INDEX register
   outb(0x3D4, 0x0F);
   position |= ((unsigned short ) inb(0x3D5) & 0xFF);
   position = position << 8;
   // cursor HIGH port to vga INDEX register
   outb(0x3D4, 0x0E);
   position |= ((unsigned short ) inb(0x3D5) & 0xFF);
   cursor_y = (position / 80) + 1;
   cursor_x = 0;
   scroll();
}

// Outputs a null-terminated ASCII string to the monitor.

void _monitor_write(char *c) {
    while (*c)
        monitor_put(*c++);
}

void monitor_write_hex(uint32_t n) {
    int tmp;
    char noZeroes = 1;

    monitor_write("0x");

    int i;
    for (i = 28; i >= 0; i -= 4) {
        tmp = (n >> i) & 0xF;
        if (tmp == 0 && noZeroes != 0)
            continue;

        noZeroes = 0;
        if (tmp >= 0xA)
            monitor_put(tmp - 0xA + 'a');
        else
            monitor_put(tmp + '0');
    }
}

void monitor_write_dec(uint32_t n) {
    if (n == 0) {
        monitor_put('0');
        return;
    }

    uint32_t acc = n;
    char c[32];
    int i = 0;
    while (acc > 0) {
        c[i] = '0' + acc % 10;
        acc /= 10;
        i++;
    }
    c[i] = 0;

    char c2[32];
    c2[i--] = 0;
    int j = 0;
    while (i >= 0)
        c2[i--] = c[j++];
    monitor_write(c2);
}
