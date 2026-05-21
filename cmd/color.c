#include "cmd.h"
#include "util.h"
#include "../kernel.h"

void cmd_color(void) {
    const char *a = cmd_args;
    while (*a == ' ') a++;

    if (*a == '\0') {
        vga_puts("\n  Usage: color 0x0|0x1|0x2|0x3|0x4 | color reset\n");
        return;
    }

    if (cmd_streq(a, "reset")) {
        vga_recolor_screen(CLR_WHITE, CLR_BLACK);
        vga_puts("\n  Color reset.\n");
        return;
    }

    if (a[0] == '0' && (a[1] == 'x' || a[1] == 'X')) a += 2;

    char d = a[0];
    uint8_t fg;
    switch (d) {
        case '0': fg = CLR_RED;    break;
        case '1': fg = CLR_BLUE;   break;
        case '2': fg = CLR_YELLOW; break;
        case '3': fg = CLR_BROWN;  break;
        case '4': fg = CLR_BLACK;  break;
        default:
            vga_puts("\n  Unknown color. Use 0x0..0x4 or reset.\n");
            return;
    }

    vga_recolor_screen(fg, CLR_BLACK);
    vga_puts("\n  Color set.\n");
}
