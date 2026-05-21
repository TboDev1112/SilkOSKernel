#include "cmd.h"
#include "util.h"
#include "../kernel.h"

static inline uint16_t vga_entry_local(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void vga_put_at(size_t x, size_t y, char c, uint8_t fg, uint8_t bg) {
    if (x >= VGA_COLS || y >= VGA_ROWS) return;
    uint8_t color = (uint8_t)((bg << 4) | (fg & 0x0F));
    vga_buf[y * VGA_COLS + x] = vga_entry_local(c, color);
}

static void vga_put_cell(size_t x, size_t y, uint16_t cell) {
    if (x >= VGA_COLS || y >= VGA_ROWS) return;
    vga_buf[y * VGA_COLS + x] = cell;
}

static void vga_write_at(size_t x, size_t y, const char *s, uint8_t fg, uint8_t bg) {
    size_t cx = x;
    while (*s && cx < VGA_COLS) {
        vga_put_at(cx, y, *s, fg, bg);
        cx++;
        s++;
    }
}

static void print_d2_at(size_t x, size_t y, uint8_t v, uint8_t fg, uint8_t bg) {
    vga_put_at(x + 0, y, (char)('0' + v / 10), fg, bg);
    vga_put_at(x + 1, y, (char)('0' + v % 10), fg, bg);
}

static void sxgui_draw_status_bar(void) {
    size_t y = VGA_ROWS - 1;
    for (size_t x = 0; x < VGA_COLS; x++) vga_put_at(x, y, ' ', CLR_BLACK, CLR_LGRAY);

    uint8_t sec  = cmos_read(0x00);
    uint8_t min  = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t regB = cmos_read(0x0B);
    if (!(regB & 0x04)) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
    }

    vga_write_at(2, y, "Sx", CLR_BLACK, CLR_LGRAY);
    size_t tx = VGA_COLS - 10;
    if (tx < 10) tx = 10;
    print_d2_at(tx + 0, y, hour, CLR_BLACK, CLR_LGRAY);
    vga_put_at(tx + 2, y, ':', CLR_BLACK, CLR_LGRAY);
    print_d2_at(tx + 3, y, min, CLR_BLACK, CLR_LGRAY);
    vga_put_at(tx + 5, y, ':', CLR_BLACK, CLR_LGRAY);
    print_d2_at(tx + 6, y, sec, CLR_BLACK, CLR_LGRAY);
}

static void sxterm_draw_window(size_t x0, size_t y0, size_t w, size_t h) {
    if (w < 4 || h < 3) return;
    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            char ch = ' ';
            if (y == 0 && (x == 0 || x + 1 == w)) ch = '+';
            else if (y + 1 == h && (x == 0 || x + 1 == w)) ch = '+';
            else if (y == 0 || y + 1 == h) ch = '-';
            else if (x == 0 || x + 1 == w) ch = '|';
            vga_put_at(x0 + x, y0 + y, ch, CLR_BLACK, CLR_LGRAY);
        }
    }
    vga_write_at(x0 + 2, y0, "SxTerm  (Ctrl+J closes)", CLR_BLACK, CLR_LGRAY);
}

static void sxterm_run(void) {
    const size_t win_w = 70;
    const size_t win_h = 9;
    const size_t x0 = (VGA_COLS - win_w) / 2;
    const size_t y0 = VGA_ROWS - 1 - win_h;

    sxterm_draw_window(x0, y0, win_w, win_h);

    const size_t ix0 = x0 + 1;
    const size_t iy0 = y0 + 1;
    const size_t iw = win_w - 2;
    const size_t ih = win_h - 2;

    vga_redir_begin(ix0, iy0, iw, ih, CLR_LGRAY, CLR_BLACK);
    vga_redir_clear();

    char input[128];
    int len = 0;

    vga_puts("$ ");

    for (;;) {
        int key = kb_getkey();
        if (!key) continue;

        if (key == KEY_CTRL_J) break;

        if (key == '\n' || key == '\r') {
            input[len] = '\0';
            vga_putchar('\n');
            if (len > 0) shell_execute(input);
            vga_putchar('\n');
            vga_puts("$ ");
            len = 0;
            continue;
        }

        if (key == '\b') {
            if (len > 0) { len--; vga_putchar('\b'); }
            continue;
        }

        if (key >= 0x20 && key < 0x7F) {
            if (len < (int)sizeof(input) - 1) {
                input[len++] = (char)key;
                vga_putchar((char)key);
            }
        }
    }

    vga_redir_end();
}

static void ps2_wait_write(void) { while (inb(0x64) & 0x02); }
static void ps2_wait_read_any(void) { while (!(inb(0x64) & 0x01)); }

static void mouse_cmd(uint8_t cmd) {
    ps2_wait_write(); outb(0x64, 0xD4);
    ps2_wait_write(); outb(0x60, cmd);
    ps2_wait_read_any(); inb(0x60);
}

static void mouse_init(void) {
    ps2_wait_write(); outb(0x64, 0xA8);
    ps2_wait_write(); outb(0x64, 0x20);
    ps2_wait_read_any();
    uint8_t cb = inb(0x60) | 0x02;
    ps2_wait_write(); outb(0x64, 0x60);
    ps2_wait_write(); outb(0x60, cb);
    mouse_cmd(0xF6);
    mouse_cmd(0xF4);
}

#define MSCALE 4
static int     mouse_fx = 0;
static int     mouse_fy = 0;
static uint8_t mouse_buf[3];
static int     mouse_buf_pos = 0;

static bool mouse_poll(int *out_dx, int *out_dy) {
    uint8_t st = inb(0x64);
    if ((st & 0x21) != 0x21) return false;
    uint8_t b = inb(0x60);
    if (mouse_buf_pos == 0 && !(b & 0x08)) return false;
    mouse_buf[mouse_buf_pos++] = b;
    if (mouse_buf_pos < 3) return false;
    mouse_buf_pos = 0;
    if (mouse_buf[0] & 0xC0) return false;
    *out_dx =  (int)mouse_buf[1] - ((mouse_buf[0] & 0x10) ? 256 : 0);
    *out_dy = -((int)mouse_buf[2] - ((mouse_buf[0] & 0x20) ? 256 : 0));
    return true;
}

static const char sc_normal[128] = {
     0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
     0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
     0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*',  0, ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static const char sc_shift[128] = {
     0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
     0,  'A','S','D','F','G','H','J','K','L',':','"','~',
     0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*',  0, ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static uint8_t kb_ext_pending = 0;

static int kb_poll(void) {
    uint8_t st = inb(0x64);
    if (!(st & 0x01) || (st & 0x20)) return 0;
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { kb_ext_pending = 1; return 0; }
    if (kb_ext_pending) {
        kb_ext_pending = 0;
        if (sc & 0x80) return 0;
        switch (sc) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            default:   return 0;
        }
    }
    if (sc & 0x80) {
        uint8_t r = sc & 0x7F;
        if (r == 0x2A || r == 0x36) kb_shift = false;
        if (r == 0x1D) kb_ctrl = false;
        return 0;
    }
    if (sc == 0x2A || sc == 0x36) { kb_shift = true;    return 0; }
    if (sc == 0x3A)                { kb_caps = !kb_caps; return 0; }
    if (sc == 0x1D)                { kb_ctrl = true;     return 0; }
    if (sc >= 128) return 0;
    char c = kb_shift ? sc_shift[sc] : sc_normal[sc];
    if (kb_ctrl && (c == 'j' || c == 'J')) return KEY_CTRL_J;
    if (kb_ctrl && (c == 't' || c == 'T')) return KEY_CTRL_T;
    if (kb_ctrl) return 0;
    if (kb_caps && c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (kb_caps && c >= 'A' && c <= 'Z' && !kb_shift) c = (char)(c + 32);
    return c;
}

static uint16_t make_ptr_cell(uint16_t under) {
    uint8_t color = (uint8_t)(under >> 8);
    uint8_t fg = color & 0x0F;
    uint8_t bg = (color >> 4) & 0x0F;
    return vga_entry_local('\x10', (uint8_t)((fg << 4) | bg));
}

void cmd_sxgui(void) {
    const uint8_t desk_fg = CLR_BLACK;
    const uint8_t desk_bg = CLR_GREEN;

    for (size_t ry = 0; ry < VGA_ROWS - 1; ry++)
        for (size_t rx = 0; rx < VGA_COLS; rx++)
            vga_put_at(rx, ry, ' ', desk_fg, desk_bg);
    sxgui_draw_status_bar();

    mouse_init();
    mouse_fx      = (VGA_COLS / 2) * MSCALE;
    mouse_fy      = ((VGA_ROWS - 1) / 2) * MSCALE;
    mouse_buf_pos = 0;
    kb_ext_pending = 0;

    size_t cx = (size_t)(mouse_fx / MSCALE);
    size_t cy = (size_t)(mouse_fy / MSCALE);
    uint16_t under = vga_buf[cy * VGA_COLS + cx];
    vga_put_cell(cx, cy, make_ptr_cell(under));

    for (;;) {
        vga_row = cy;
        vga_col = cx;
        vga_update_cursor();

        bool moved = false;

        int mdx = 0, mdy = 0;
        if (mouse_poll(&mdx, &mdy)) {
            mouse_fx += mdx;
            mouse_fy += mdy;
            if (mouse_fx < 0)                                mouse_fx = 0;
            if (mouse_fx >= (int)(VGA_COLS * MSCALE))        mouse_fx = (int)(VGA_COLS * MSCALE) - 1;
            if (mouse_fy < 0)                                mouse_fy = 0;
            if (mouse_fy >= (int)((VGA_ROWS - 1) * MSCALE)) mouse_fy = (int)((VGA_ROWS - 1) * MSCALE) - 1;
            moved = true;
        }

        int key = kb_poll();

        if (key == KEY_CTRL_J) {
            vga_put_cell(cx, cy, under);
            break;
        }

        if (key == KEY_CTRL_T) {
            vga_put_cell(cx, cy, under);
            sxterm_run();
            mouse_buf_pos = 0;
            for (size_t ry = 0; ry < VGA_ROWS - 1; ry++)
                for (size_t rx = 0; rx < VGA_COLS; rx++)
                    vga_put_at(rx, ry, ' ', desk_fg, desk_bg);
            sxgui_draw_status_bar();
            under = vga_buf[cy * VGA_COLS + cx];
            vga_put_cell(cx, cy, make_ptr_cell(under));
        } else if (key == KEY_UP    && mouse_fy >= MSCALE)                         { mouse_fy -= MSCALE; moved = true; }
          else if (key == KEY_DOWN  && mouse_fy < (int)((VGA_ROWS - 2) * MSCALE)) { mouse_fy += MSCALE; moved = true; }
          else if (key == KEY_LEFT  && mouse_fx >= MSCALE)                         { mouse_fx -= MSCALE; moved = true; }
          else if (key == KEY_RIGHT && mouse_fx < (int)((VGA_COLS - 1) * MSCALE)) { mouse_fx += MSCALE; moved = true; }

        if (moved) {
            size_t ncx = (size_t)(mouse_fx / MSCALE);
            size_t ncy = (size_t)(mouse_fy / MSCALE);
            if (ncx != cx || ncy != cy) {
                vga_put_cell(cx, cy, under);
                cx = ncx;
                cy = ncy;
                under = vga_buf[cy * VGA_COLS + cx];
                vga_put_cell(cx, cy, make_ptr_cell(under));
            }
        }

        sxgui_draw_status_bar();
    }

    vga_init();
    vga_set_color(CLR_WHITE, CLR_BLACK);
}
