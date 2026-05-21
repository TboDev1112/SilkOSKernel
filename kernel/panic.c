#include "panic.h"
#include "../kernel.h"

void kernel_panic(const char *msg) {
    __asm__ volatile ("cli");

    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("\n\n");
    vga_puts("Kernel Panic!\n");

    vga_set_color(CLR_WHITE, CLR_BLACK);
    vga_puts("Reason : ");
    vga_set_color(CLR_YELLOW, CLR_BLACK);
    vga_puts(msg);

    vga_set_color(CLR_LGRAY, CLR_BLACK);
    vga_puts("\n\n  The system has been halted.\n");
    vga_puts("  Restart your machine to continue.\n\n");
    vga_set_color(CLR_LRED, CLR_BLACK);

    for (;;) __asm__ volatile ("hlt");
}
