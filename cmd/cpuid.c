#include "cmd.h"
#include "../kernel.h"

void cmd_cpuid(void) {
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    for (int i = 0; i < 4; i++) vendor[i]     = (char)((ebx >> (i * 8)) & 0xFF);
    for (int i = 0; i < 4; i++) vendor[i + 4] = (char)((edx >> (i * 8)) & 0xFF);
    for (int i = 0; i < 4; i++) vendor[i + 8] = (char)((ecx >> (i * 8)) & 0xFF);
    vendor[12] = '\0';

    char brand[49];
    uint32_t *b = (uint32_t *)brand;
    for (int leaf = 0; leaf < 3; leaf++) {
        uint32_t r[4];
        __asm__ volatile (
            "cpuid"
            : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
            : "a"(0x80000002 + (uint32_t)leaf)
        );
        for (int j = 0; j < 4; j++) b[leaf * 4 + j] = r[j];
    }
    brand[48] = '\0';

    const char *brand_trim = brand;
    while (*brand_trim == ' ') brand_trim++;

    vga_puts("\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Vendor  : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts(vendor);     vga_puts("\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Brand   : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts(brand_trim); vga_puts("\n");
    vga_set_color(CLR_LGRAY,  CLR_BLACK);
}
