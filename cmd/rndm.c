#include "cmd.h"
#include "util.h"
#include "../kernel.h"

static uint64_t xorshift64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static uint64_t rndm_seed_init(void) {
    uint64_t s = 0x243f6a8885a308d3ULL;

    while (cmos_read(0x0A) & 0x80) {}
    s ^= (uint64_t)cmos_read(0x00) << 0;
    s ^= (uint64_t)cmos_read(0x02) << 8;
    s ^= (uint64_t)cmos_read(0x04) << 16;
    s ^= (uint64_t)cmos_read(0x07) << 24;
    s ^= (uint64_t)cmos_read(0x08) << 32;
    s ^= (uint64_t)cmos_read(0x09) << 40;

    for (int i = 0; i < 8; i++)
        xorshift64(&s);
    if (s == 0) s = 1;
    return s;
}

static uint32_t rndm_codepoint(uint64_t *state) {
    static const struct { uint32_t lo; uint32_t hi; } blocks[] = {
        { 0x0020, 0x007E }, /* Basic Latin */
        { 0x00A1, 0x00FF }, /* Latin-1 supplement */
        { 0x0100, 0x024F }, /* Latin extended */
        { 0x0370, 0x03FF }, /* Greek */
        { 0x0400, 0x04FF }, /* Cyrillic */
        { 0x0600, 0x06FF }, /* Arabic */
        { 0x3040, 0x30FF }, /* Hiragana/Katakana */
        { 0x4E00, 0x4FFF }, /* CJK sample slice */
        { 0x2000, 0x206F }, /* General punctuation */
        { 0x2190, 0x21FF }, /* Arrows */
        { 0x2200, 0x22FF }, /* Math operators */
        { 0x2500, 0x257F }, /* Box drawing */
        { 0x2580, 0x259F }, /* Block elements */
        { 0x25A0, 0x25FF }, /* Geometric shapes */
        { 0x2600, 0x26FF }, /* Misc symbols */
        { 0x2700, 0x27BF }, /* Dingbats */
    };

    size_t pick = (size_t)(xorshift64(state) % (sizeof(blocks) / sizeof(blocks[0])));
    uint32_t lo = blocks[pick].lo;
    uint32_t hi = blocks[pick].hi;
    uint32_t span = hi - lo + 1;
    return lo + (uint32_t)(xorshift64(state) % span);
}

static void utf8_put(uint32_t cp) {
    char seq[4];
    int n = 0;

    if (cp < 0x80) {
        seq[n++] = (char)cp;
    } else if (cp < 0x800) {
        seq[0] = (char)(0xC0 | (cp >> 6));
        seq[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        seq[0] = (char)(0xE0 | (cp >> 12));
        seq[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        seq[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else if (cp < 0x110000) {
        seq[0] = (char)(0xF0 | (cp >> 18));
        seq[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        seq[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        seq[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    } else {
        return;
    }

    for (int i = 0; i < n; i++)
        vga_putchar(seq[i]);
}

void cmd_rndm(void) {
    uint64_t rng = rndm_seed_init();

    vga_puts("\n  rndm: streaming random Unicode (Ctrl+C to stop)\n\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);

    for (;;) {
        int key = kb_poll();
        if (key == KEY_CTRL_C) break;

        utf8_put(rndm_codepoint(&rng));
    }

    vga_puts("\n\n  Stopped.\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
}
