#include "cmd.h"
#include "../kernel.h"

void cmd_reboot(void) {
    vga_set_color(CLR_YELLOW, CLR_BLACK);
    vga_puts("\n  Rebooting SilkOS...\n");

    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);

    outb(0x64, 0xFE);

    kernel_panic("The SilkOS Kernel has had a fatal error and cannot recover: 0x002");
}
