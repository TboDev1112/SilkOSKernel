#include "cmd.h"
#include "util.h"
#include "../kernel.h"
#include "../silk_fs.h"

typedef struct {
    uint32_t        width;
    uint32_t        height;
    uint8_t         maxval;
    const uint8_t  *pixels;
} PpmImage;

static inline uint16_t vga_cell(char c, uint8_t fg, uint8_t bg) {
    return (uint16_t)(uint8_t)c | ((uint16_t)((bg << 4) | (fg & 0x0F)) << 8);
}

static const uint8_t vga_rgb[16][3] = {
    {  0,   0,   0}, {  0,   0, 170}, {  0, 170,   0}, {  0, 170, 170},
    {170,   0,   0}, {170,   0, 170}, {170,  85,   0}, {170, 170, 170},
    { 85,  85,  85}, { 85,  85, 255}, { 85, 255,  85}, { 85, 255, 255},
    {255,  85,  85}, {255,  85, 255}, {255, 255,  85}, {255, 255, 255},
};

static uint8_t rgb_to_vga(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t best = 0xFFFFFFFFu;
    uint8_t  idx  = 0;
    for (uint8_t i = 0; i < 16; i++) {
        int dr = (int)r - (int)vga_rgb[i][0];
        int dg = (int)g - (int)vga_rgb[i][1];
        int db = (int)b - (int)vga_rgb[i][2];
        uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db);
        if (d < best) { best = d; idx = i; }
    }
    return idx;
}

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool read_u32(const uint8_t **p, const uint8_t *end, uint32_t *out) {
    const uint8_t *s = *p;
    while (s < end && is_space((char)*s)) s++;
    if (s >= end || *s < '0' || *s > '9') return false;
    uint32_t v = 0;
    while (s < end && *s >= '0' && *s <= '9') {
        v = v * 10u + (uint32_t)(*s - '0');
        s++;
    }
    *out = v;
    *p = s;
    return true;
}

static bool skip_ppm_comments(const uint8_t **p, const uint8_t *end) {
    const uint8_t *s = *p;
    while (s < end) {
        while (s < end && is_space((char)*s)) s++;
        if (s >= end || *s != '#') break;
        while (s < end && *s != '\n') s++;
        if (s < end) s++;
    }
    *p = s;
    return true;
}

static bool ppm_parse(const uint8_t *data, size_t size, PpmImage *img) {
    const uint8_t *p = data;
    const uint8_t *end = data + size;

    if (size < 4) return false;
    if (p[0] != 'P') return false;
    char kind = (char)p[1];
    p += 2;
    if (kind != '6' && kind != '5') return false;

    if (!skip_ppm_comments(&p, end)) return false;
    if (!read_u32(&p, end, &img->width) || !read_u32(&p, end, &img->height)) return false;

    uint32_t maxval = 0;
    if (!read_u32(&p, end, &maxval) || maxval == 0 || maxval > 255) return false;
    img->maxval = (uint8_t)maxval;

    while (p < end && is_space((char)*p)) p++;
    if (p >= end) return false;

    img->pixels = p;
    size_t need = (size_t)img->width * (size_t)img->height * (kind == '6' ? 3u : 1u);
    if ((size_t)(end - p) < need) return false;

    return img->width > 0 && img->height > 0;
}

static void ppm_sample(const PpmImage *img, bool color, uint32_t x, uint32_t y,
                       uint8_t *r, uint8_t *g, uint8_t *b) {
    if (x >= img->width)  x = img->width  ? img->width  - 1 : 0;
    if (y >= img->height) y = img->height ? img->height - 1 : 0;

    if (color) {
        const uint8_t *px = img->pixels + ((size_t)y * img->width + x) * 3u;
        *r = px[0]; *g = px[1]; *b = px[2];
        if (img->maxval != 255) {
            *r = (uint8_t)((*r * 255u) / img->maxval);
            *g = (uint8_t)((*g * 255u) / img->maxval);
            *b = (uint8_t)((*b * 255u) / img->maxval);
        }
    } else {
        uint8_t g8 = img->pixels[(size_t)y * img->width + x];
        if (img->maxval != 255) g8 = (uint8_t)((g8 * 255u) / img->maxval);
        *r = *g = *b = g8;
    }
}

static void render_ppm_to_vga(const PpmImage *img, bool color) {
    const size_t out_w = VGA_COLS;
    const size_t out_h = VGA_ROWS * 2;

    for (size_t row = 0; row < VGA_ROWS; row++) {
        for (size_t col = 0; col < VGA_COLS; col++) {
            uint32_t x0 = (uint32_t)((col * img->width) / out_w);
            uint32_t y_top = (uint32_t)((row * 2) * img->height / out_h);
            uint32_t y_bot = (uint32_t)((row * 2 + 1) * img->height / out_h);
            if (y_bot == y_top && y_top + 1 < img->height) y_bot = y_top + 1;

            uint8_t r0, g0, b0, r1, g1, b1;
            ppm_sample(img, color, x0, y_top, &r0, &g0, &b0);
            ppm_sample(img, color, x0, y_bot, &r1, &g1, &b1);

            uint8_t fg = rgb_to_vga(r0, g0, b0);
            uint8_t bg = rgb_to_vga(r1, g1, b1);
            vga_buf[row * VGA_COLS + col] = vga_cell('\xDF', fg, bg);
        }
    }

    vga_row = VGA_ROWS - 1;
    vga_col = 0;
    vga_update_cursor();
}

void cmd_render(void) {
    const char *name = cmd_args;
    while (*name == ' ') name++;

    if (*name == '\0') {
        vga_puts("\n  Usage: render <image.ppm>\n");
        vga_puts("  Supports binary PPM (P5/P6). Convert with:\n");
        vga_puts("    convert photo.png photo.ppm\n");
        vga_puts("  Pass files into QEMU: ./build run --files photo.ppm\n");
        return;
    }

    const SilkFile *file = silk_file_find(name);
    if (!file) {
        vga_puts("\n  render: '");
        vga_puts(name);
        vga_puts("' not found  (run 'files' for a list)\n");
        return;
    }

    PpmImage img;
    const uint8_t *data = file->data;
    size_t size = silk_file_size(file);
    if (!ppm_parse(data, size, &img)) {
        vga_puts("\n  render: unsupported or corrupt image (need P5/P6 PPM)\n");
        return;
    }

    bool color = (size >= 2 && data[1] == '6');
    vga_puts("\n  Rendering ");
    vga_puts(file->name);
    vga_puts(" ...\n");
    render_ppm_to_vga(&img, color);
    vga_puts("  Done.\n");
}
