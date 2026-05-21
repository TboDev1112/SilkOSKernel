#include "cmd.h"
#include "util.h"
#include "../kernel.h"
#include "../silk_fs.h"

void cmd_exec(void) {
    const char *name = cmd_args;
    while (*name == ' ') name++;

    if (*name == '\0') {
        vga_puts("\n  Usage: exec <filename>\n");
        vga_puts("  Use 'files' to list available binaries.\n");
        return;
    }

    const SilkFile *file = silk_file_find(name);
    if (file) {
        vga_puts("\n");
        int rc = elf64_exec(file->data, silk_file_size(file));
        if (rc >= 0) {
            vga_puts("\n  [process exited: ");
            cmd_kputs_i32(rc);
            vga_puts("]\n");
        }
        return;
    }

    vga_puts("\n  exec: '");
    vga_puts(name);
    vga_puts("' not found  (run 'files' for a list)\n");
}
