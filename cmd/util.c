#include "util.h"
#include "../kernel.h"

const char *cmd_skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

bool cmd_parse_i32(const char **pp, int32_t *out) {
    const char *p = cmd_skip_ws(*pp);
    bool neg = false;
    if (*p == '+') p++;
    else if (*p == '-') { neg = true; p++; }
    p = cmd_skip_ws(p);
    if (*p < '0' || *p > '9') return false;
    int32_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = (int32_t)(v * 10 + (*p - '0'));
        p++;
    }
    *pp = p;
    *out = neg ? -v : v;
    return true;
}

bool cmd_streq(const char *a, const char *b) {
    return kstrcmp(a, b) == 0;
}

void cmd_kputs_i32(int32_t v) {
    char buf[16];
    int i = 0;
    if (v == 0) { vga_putchar('0'); return; }
    bool neg = v < 0;
    uint32_t n = neg ? (uint32_t)(-v) : (uint32_t)v;
    while (n && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    if (neg) vga_putchar('-');
    while (i--) vga_putchar(buf[i]);
}

uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)(((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F));
}

void print_d2(uint8_t v) {
    vga_putchar((char)('0' + v / 10));
    vga_putchar((char)('0' + v % 10));
}
