#include "cmd.h"
#include "util.h"
#include "../kernel.h"

void cmd_time(void) {
    while (cmos_read(0x0A) & 0x80);

    uint8_t sec  = cmos_read(0x00);
    uint8_t min  = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day  = cmos_read(0x07);
    uint8_t mon  = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);

    uint8_t regB = cmos_read(0x0B);
    if (!(regB & 0x04)) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        year = bcd_to_bin(year);
    }

    uint16_t full_year = (uint16_t)(2000 + year);

    vga_puts("\n  ");
    vga_set_color(CLR_WHITE, CLR_BLACK);
    print_d2(hour); vga_putchar(':');
    print_d2(min);  vga_putchar(':');
    print_d2(sec);
    vga_puts("  ");
    print_d2(day);  vga_putchar('/');
    print_d2(mon);  vga_putchar('/');
    vga_putchar((char)('0' + full_year / 1000));
    vga_putchar((char)('0' + (full_year % 1000) / 100));
    vga_putchar((char)('0' + (full_year % 100) / 10));
    vga_putchar((char)('0' + full_year % 10));
    vga_puts("  (from RTC)\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
}
