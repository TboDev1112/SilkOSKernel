#include "cmd.h"
#include "util.h"
#include "../kernel.h"

void cmd_beep(void) {
    const char *p = cmd_args;
    int32_t vol, freq, dur;

    if (!cmd_parse_i32(&p, &vol) || !cmd_parse_i32(&p, &freq) || !cmd_parse_i32(&p, &dur)) {
        vga_puts("\n  Usage: beep <volume 1-100> <frequency Hz> <duration ms>\n");
        vga_puts("  Example: beep 16 400 200\n");
        return;
    }
    if (vol < 1 || vol > 100) {
        vga_puts("\n  beep: volume must be 1-100\n");
        return;
    }
    if (freq < 20 || freq > 20000) {
        vga_puts("\n  beep: frequency must be 20-20000 Hz\n");
        return;
    }
    if (dur < 1 || dur > 30000) {
        vga_puts("\n  beep: duration must be 1-30000 ms\n");
        return;
    }

    beep_play((uint32_t)vol, (uint32_t)freq, (uint32_t)dur);
}
