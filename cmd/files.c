#include "cmd.h"
#include "../kernel.h"
#include "../silk_fs.h"

void cmd_files(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("  Embedded files:\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);

    if (silk_file_count == 0) {
        vga_puts("  (none)\n");
        vga_puts("  Embed at build:  ./build --files a.elf image.ppm\n");
        vga_puts("  QEMU passthrough: ./build run --files image.ppm\n");
        return;
    }

    for (size_t i = 0; i < silk_file_count; i++) {
        vga_puts("  ");
        vga_set_color(CLR_LGREEN, CLR_BLACK);
        vga_puts(silk_files[i].name);
        vga_set_color(CLR_LGRAY, CLR_BLACK);

        size_t nl = kstrlen(silk_files[i].name);
        for (size_t p = nl; p < 22; p++) vga_putchar(' ');

        kputs_sz(silk_file_size(&silk_files[i]));
        vga_puts(" bytes\n");
    }
}
