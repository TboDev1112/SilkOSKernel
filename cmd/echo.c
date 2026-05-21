#include "cmd.h"
#include "../kernel.h"

void cmd_echo(void) {
    vga_puts("\n  ");
    vga_set_color(CLR_WHITE, CLR_BLACK);
    vga_puts(cmd_args);
    vga_puts("\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
}
