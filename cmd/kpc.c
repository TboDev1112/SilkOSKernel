#include "cmd.h"
#include "../kernel.h"

void cmd_kpc(void) {
    vga_puts("Kernel Panic Codes:\n0x001: Manually Triggered Panic\n0x002: Failed Reboot\n");
}
