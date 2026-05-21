#include "cmd.h"
#include "../kernel.h"

void cmd_halt(void) {
    vga_puts("\n");
    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("  Halting SilkOS... Goodbye!\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
    kernel_request_halt();
}
