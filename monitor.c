// monitor.c -- Defines functions for writing to the monitor.
//              heavily based on Bran's kernel development tutorials,
//              but rewritten for JamesM's kernel tutorials.

#include "monitor.h"
#include "font.h"
#include "pmm.h"
#include "common.h"

uint16_t *video_memory = (uint16_t*) 0xB8000;
uint16_t video_pitch;
// Stores the cursor position.
uint32_t cursor_x = 0;
uint32_t cursor_y = 0;

extern const struct bitmap_font font;

uint32_t font_data_lookup_table[16] = {
    0x00000000,
    0x000000FF,
    0x0000FF00,
    0x0000FFFF,
    0x00FF0000,
    0x00FF00FF,
    0x00FFFF00,
    0x00FFFFFF,
    0xFF000000,
    0xFF0000FF,
    0xFF00FF00,
    0xFF00FFFF,
    0xFFFF0000,
    0xFFFF00FF,
    0xFFFFFF00,
    0xFFFFFFFF
};

void video_init(struct vbe_mode_info_t * vmode_info) {
    uint8_t * framebuffer_phys = (uint8_t *) vmode_info->physbase;
    uint8_t * framebuffer_virtual = (uint8_t *) FRAMEBUFFER_VIRTUAL;
    int xmax = vmode_info->Xres, ymax = vmode_info->Yres;
    uint8_t * pixel = framebuffer_virtual;

    uint32_t i, j, fb_pages = (ymax * vmode_info->pitch + 1) / 4096;

    for (i = 0; i <= fb_pages; i++) {
        mm_map((void*) ((unsigned long) framebuffer_phys + i * 4096),
                (void*) ((unsigned long) FRAMEBUFFER_VIRTUAL + i * 4096),
                PAGE_USER | PAGE_WRITE);
    }

    for (i = 0; i < ymax; i++) {
        for (j = 0; j < xmax; j++) {
            pixel[j * 3] = 0x40;
            pixel[j * 3 + 1] = 0x40;
            pixel[j * 3 + 2] = 0x40;
        }
        pixel += vmode_info->pitch;
    }

    video_pitch = vmode_info->pitch;
}

static void draw_char(uint8_t *where, uint32_t character, uint8_t foreground, uint8_t background) {
    int row, pixel;
    uint8_t row_data, mask;
    uint32_t mask1, mask2;
    uint8_t *font_data_for_char = &font.Bitmap[(character - 31) * 16];

    for (row = 0; row < 16; row++) {
        row_data = font_data_for_char[row];
        mask = 0x80;
        /*mask1 = font_data_lookup_table[row_data >> 16];
        mask2 = font_data_lookup_table[row_data & 0x0F];
         *(uint32_t *)where = (packed_foreground & mask1) | (packed_background & ~mask1);
         *(uint32_t *)(&where[4]) = (packed_foreground & mask2) | (packed_background & ~mask2); */
        for (pixel = 0; pixel < 8; pixel++) {
            if ((row_data & mask) != 0) {
                where[pixel * 3] = foreground;
                where[pixel * 3 + 1] = foreground;
                where[pixel * 3 + 2] = foreground;
            } else {
                where[pixel * 3] = background;
                where[pixel * 3 + 1] = background;
                where[pixel * 3 + 2] = background;
            }
            mask = mask >> 1;
        }
        where += video_pitch;
    }
}

static void draw_char_mask(uint8_t *where, uint32_t character, uint8_t foreground) {
    int row, pixel;
    uint8_t row_data, mask;
    uint32_t mask1, mask2;
    uint8_t *font_data_for_char = &font.Bitmap[(character - 31) * 16];

    for (row = 0; row < 16; row++) {
        row_data = font_data_for_char[row];
        mask = 0x80;
        /*mask1 = font_data_lookup_table[row_data >> 16];
        mask2 = font_data_lookup_table[row_data & 0x0F];
         *(uint32_t *)where = (packed_foreground & mask1) | (packed_background & ~mask1);
         *(uint32_t *)(&where[4]) = (packed_foreground & mask2) | (packed_background & ~mask2); */
        for (pixel = 0; pixel < 8; pixel++) {
            if ((row_data & mask) != 0) {
                where[pixel * 3] = foreground;
                where[pixel * 3 + 1] = foreground;
                where[pixel * 3 + 2] = foreground;
            }
            mask = mask >> 1;
        }
        where += video_pitch;
    }
}

// Updates the hardware cursor.

static void move_cursor() {
    // The screen is 80 characters wide...
    uint16_t cursorLocation = cursor_y * 80 + cursor_x;
    outb(0x3D4, 14); // Tell the VGA board we are setting the high cursor byte.
    outb(0x3D5, cursorLocation >> 8); // Send the high cursor byte.
    outb(0x3D4, 15); // Tell the VGA board we are setting the low cursor byte.
    outb(0x3D5, cursorLocation); // Send the low cursor byte.
}

// Scrolls the text on the screen up by one line.

static void scroll() {
    // Get a space character with the default colour attributes.
    uint8_t attributeByte = (0 /*black*/ << 4) | (15 /*white*/ & 0x0F);
    uint16_t blank = 0x20 /* space */ | (attributeByte << 8);
    uint8_t * fbuffer = FRAMEBUFFER_VIRTUAL;

    // Row 25 is the end, this means we need to scroll up
    if (cursor_y >= 30) {
        // Move the current text chunk that makes up the screen
        // back in the buffer by a line
        int i;
        for (i = 0; i < 464 * video_pitch; i++)
            fbuffer[i] = fbuffer[video_pitch * 16 + i];

        // The last line should now be blank. Do this by writing
        // 80 spaces to it.
        for (i = 464 * video_pitch; i < 480 * video_pitch; i++)
            fbuffer[i] = 0;

        // The cursor should now be on the last line.
        cursor_y = 29;
    }
}

// Writes a single character out to the screen.

void monitor_put(char c) {
    // The background colour is black (0), the foreground is white (15).
    uint8_t backColour = 0;
    uint8_t foreColour = 15;

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
        //video_memory[cursor_y*80 + cursor_x] = c | attribute;
        draw_char_mask((uint8_t*) (FRAMEBUFFER_VIRTUAL +
                cursor_y * video_pitch * 16 + cursor_x * 8 * 3),
                c, 255);
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
    // Move the hardware cursor.
    //move_cursor();
}

// Clears the screen, by copying lots of spaces to the framebuffer.

void monitor_clear() {
    uint8_t * framebuffer = FRAMEBUFFER_VIRTUAL;

    int i,j;
    for (i = 0; i < 480; i++) {
        for(j = 0; j < 640; j++) {
            /*framebuffer[j*3] = 255;
            framebuffer[j*3 + 1] = 48;
            framebuffer[j*3 + 2] = 64;*/
            framebuffer[j*3] = 127;
            framebuffer[j*3 + 1] = 127;
            framebuffer[j*3 + 2] = 127;
        }
        framebuffer += video_pitch;
    }

    // Move the hardware cursor back to the start.
    cursor_x = 0;
    cursor_y = 0;
    //move_cursor();
}

// Outputs a null-terminated ASCII string to the monitor.

void monitor_write(char *c) {
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
