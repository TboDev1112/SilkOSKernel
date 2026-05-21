#include "cmd.h"
#include "../kernel.h"

void cmd_neofetch(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  OS      : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("SilkOS\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Version : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("1.0.0 Codename 'Repina' (stable)\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Arch    : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("x86-64 long mode\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Boot    : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("GRUB Multiboot\n");
    vga_set_color(CLR_LGRAY,  CLR_BLACK);
}
