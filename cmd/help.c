#include "cmd.h"
#include "../kernel.h"

void cmd_help(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("  Available commands:\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
    vga_puts("\n");

    for (size_t i = 0; i < cmd_count; i++) {
        vga_puts("  ");
        vga_set_color(CLR_LGREEN, CLR_BLACK);
        vga_puts(commands[i].name);
        vga_set_color(CLR_LGRAY, CLR_BLACK);
        size_t pad = 12 - kstrlen(commands[i].name);
        for (size_t p = 0; p < pad; p++) vga_putchar(' ');
        vga_puts(commands[i].desc);
        vga_puts("\n");
    }
    vga_puts("\n");
}
