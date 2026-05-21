#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VGA_COLS   80
#define VGA_ROWS   25

#define CLR_BLACK    0x0
#define CLR_BLUE     0x1
#define CLR_GREEN    0x2
#define CLR_CYAN     0x3
#define CLR_RED      0x4
#define CLR_MAGENTA  0x5
#define CLR_BROWN    0x6
#define CLR_LGRAY    0x7
#define CLR_DGRAY    0x8
#define CLR_LBLUE    0x9
#define CLR_LGREEN   0xA
#define CLR_LCYAN    0xB
#define CLR_LRED     0xC
#define CLR_LMAGENTA 0xD
#define CLR_YELLOW   0xE
#define CLR_WHITE    0xF

#define KEY_UP       0x100
#define KEY_DOWN     0x101
#define KEY_LEFT     0x102
#define KEY_RIGHT    0x103
#define KEY_CTRL_J   0x104
#define KEY_CTRL_T   0x105

extern uint16_t *vga_buf;
extern size_t    vga_row;
extern size_t    vga_col;

extern bool kb_shift;
extern bool kb_caps;
extern bool kb_ctrl;

void vga_init(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_recolor_screen(uint8_t fg, uint8_t bg);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_redir_begin(size_t x0, size_t y0, size_t w, size_t h, uint8_t fg, uint8_t bg);
void vga_redir_end(void);
void vga_redir_clear(void);
void vga_update_cursor(void);

void kernel_panic(const char *msg);
void kernel_request_halt(void);
bool kernel_is_running(void);

void beep_play(uint32_t vol, uint32_t freq, uint32_t ms);
int  elf64_exec(const uint8_t *data, size_t size);

uint8_t inb(uint16_t port);
void    outb(uint16_t port, uint8_t val);

size_t kstrlen(const char *s);
int    kstrcmp(const char *a, const char *b);
void   kmemcpy(void *dst, const void *src, size_t n);
void   kputs_sz(size_t v);

int kb_getkey(void);

void shell_execute(const char *line);
