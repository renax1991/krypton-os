// common.h -- Defines typedefs and some global functions.
//             From JamesM's kernel development tutorials.

#ifndef COMMON_H
#define COMMON_H

// Some standard typedefs, to standardise sizes across platforms.
// These typedefs are written for 32-bit X86.
typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef unsigned short uint16_t;
typedef          short int16_t;
typedef unsigned char  uint8_t;
typedef          char  int8_t;



void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len);
void memset(uint8_t *dest, uint8_t val, uint32_t len);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
int strlen(char *src);
int strcmp(char *str1, char *str2);

#endif // COMMON_H
