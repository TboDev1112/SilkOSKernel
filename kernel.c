#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "silk_fs.h"

void vga_set_color(uint8_t fg, uint8_t bg);
void vga_puts(const char *s);
void kernel_panic(const char *msg);

static inline void cpu_relax(void) {
    __asm__ volatile ("pause");
}

void usleep_mil(uint32_t ms) {
    const uint32_t LOOPS_PER_MS = 50000;

    for (uint32_t t = 0; t < ms; t++) {
        for (volatile uint32_t i = 0; i < LOOPS_PER_MS; i++) {
            cpu_relax();
        }
    }
}


static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define PIT_HZ 1193182UL

static void pc_speaker_set_freq(uint32_t freq) {
    uint32_t div = (uint32_t)(PIT_HZ / freq);
    outb(0x43, 0xB6);                              
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
}

static void pc_speaker_on(void) {
    outb(0x61, inb(0x61) | 0x03);                 
}

static void pc_speaker_off(void) {
    outb(0x61, inb(0x61) & (uint8_t)~0x03);
}

static void beep_play(uint32_t vol, uint32_t freq, uint32_t ms) {
    if (!vol || !freq || !ms) return;
    if (vol > 100) vol = 100;

    pc_speaker_set_freq(freq);

    if (vol == 100) {
        pc_speaker_on();
        usleep_mil(ms);
        pc_speaker_off();
        return;
    }

    const uint32_t CYCLE = 50000;       
    uint32_t on_loops  = (CYCLE * vol) / 100;
    uint32_t off_loops = CYCLE - on_loops;

    for (uint32_t t = 0; t < ms; t++) {
        pc_speaker_on();
        for (volatile uint32_t i = 0; i < on_loops; i++) cpu_relax();
        pc_speaker_off();
        for (volatile uint32_t i = 0; i < off_loops; i++) cpu_relax();
    }
    pc_speaker_off();
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static void serial_write_char(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0) {}
    outb(0x3F8 + 0, (uint8_t)c);
}

static void serial_write(const char *s) {
    for (; *s; s++) serial_write_char(*s);
}





#define VGA_COLS   80
#define VGA_ROWS   25
#define VGA_BUF    ((uint16_t *)(uintptr_t)0xB8000)


#define CLR_BLACK    0x0
#define CLR_BLUE     0x1
#define CLR_GREEN    0x2
#define CLR_CYAN     0x3
#define CLR_RED      0x4
#define CLR_MAGENTA  0x5
#define CLR_BROWN    0x6
#define CLR_LGRAY    0x7
#define CLR_DGRAY    0x8
#define CLR_LBLUE    0x9
#define CLR_LGREEN   0xA
#define CLR_LCYAN    0xB
#define CLR_LRED     0xC
#define CLR_LMAGENTA 0xD
#define CLR_YELLOW   0xE
#define CLR_WHITE    0xF

static uint8_t  vga_color;
static uint16_t *vga_buf;
static size_t   vga_row, vga_col;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_COLS + vga_col);
    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

static void vga_scroll(void) {
    for (size_t y = 0; y < VGA_ROWS - 1; y++)
        for (size_t x = 0; x < VGA_COLS; x++)
            vga_buf[y * VGA_COLS + x] = vga_buf[(y + 1) * VGA_COLS + x];
    for (size_t x = 0; x < VGA_COLS; x++)
        vga_buf[(VGA_ROWS - 1) * VGA_COLS + x] = vga_entry(' ', vga_color);
    if (vga_row > 0) vga_row = VGA_ROWS - 1;
}

void vga_init(void) {
    vga_color = (CLR_BLACK << 4) | CLR_LGRAY;  
    vga_buf   = VGA_BUF;
    vga_row   = 0;
    vga_col   = 0;
    for (size_t i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga_buf[i] = vga_entry(' ', vga_color);
    vga_update_cursor();
}





void kernel_panic(const char *msg) {
    
    __asm__ volatile ("cli");

    
    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("\n\n");
    vga_puts("Kernel Panic!\n");

    vga_set_color(CLR_WHITE, CLR_BLACK);
    vga_puts("Reason : ");
    vga_set_color(CLR_YELLOW, CLR_BLACK);
    vga_puts(msg);

    vga_set_color(CLR_LGRAY, CLR_BLACK);
    vga_puts("\n\n  The system has been halted.\n");
    vga_puts("  Restart your machine to continue.\n\n");
    vga_set_color(CLR_LRED, CLR_BLACK);
    
    for (;;) __asm__ volatile ("hlt");
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

static void vga_recolor_screen(uint8_t fg, uint8_t bg) {
    uint8_t color = (uint8_t)((bg << 4) | (fg & 0x0F));
    for (size_t i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        uint16_t cell = vga_buf[i];
        uint8_t ch = (uint8_t)(cell & 0xFF);
        vga_buf[i] = vga_entry((char)ch, color);
    }
    vga_color = color;
}

typedef struct {
    bool active;
    size_t x0, y0, w, h;
    size_t cx, cy;
    uint8_t fg, bg;
} VgaRedirect;

static VgaRedirect vga_redir = {0};
static size_t vga_saved_row, vga_saved_col;

static void vga_redir_begin(size_t x0, size_t y0, size_t w, size_t h, uint8_t fg, uint8_t bg) {
    vga_saved_row = vga_row;
    vga_saved_col = vga_col;
    vga_redir.active = true;
    vga_redir.x0 = x0;
    vga_redir.y0 = y0;
    vga_redir.w = w;
    vga_redir.h = h;
    vga_redir.cx = 0;
    vga_redir.cy = 0;
    vga_redir.fg = fg;
    vga_redir.bg = bg;
}

static void vga_redir_end(void) {
    vga_redir.active = false;
    vga_row = vga_saved_row;
    vga_col = vga_saved_col;
    vga_update_cursor();
}

static void vga_redir_clear(void) {
    if (!vga_redir.active) return;
    uint8_t color = (uint8_t)((vga_redir.bg << 4) | (vga_redir.fg & 0x0F));
    for (size_t y = 0; y < vga_redir.h; y++) {
        for (size_t x = 0; x < vga_redir.w; x++) {
            vga_buf[(vga_redir.y0 + y) * VGA_COLS + (vga_redir.x0 + x)] = vga_entry(' ', color);
        }
    }
    vga_redir.cx = 0;
    vga_redir.cy = 0;
}

static void vga_redir_scroll(void) {
    if (!vga_redir.active) return;
    uint8_t color = (uint8_t)((vga_redir.bg << 4) | (vga_redir.fg & 0x0F));
    for (size_t y = 0; y + 1 < vga_redir.h; y++) {
        for (size_t x = 0; x < vga_redir.w; x++) {
            vga_buf[(vga_redir.y0 + y) * VGA_COLS + (vga_redir.x0 + x)] =
                vga_buf[(vga_redir.y0 + y + 1) * VGA_COLS + (vga_redir.x0 + x)];
        }
    }
    size_t last = vga_redir.h - 1;
    for (size_t x = 0; x < vga_redir.w; x++) {
        vga_buf[(vga_redir.y0 + last) * VGA_COLS + (vga_redir.x0 + x)] = vga_entry(' ', color);
    }
    if (vga_redir.cy > 0) vga_redir.cy = vga_redir.h - 1;
}

void vga_putchar(char c) {
    if (c == '\n') serial_write_char('\n');
    else if (c == '\r') serial_write_char('\r');
    else if (c == '\b') serial_write_char('\b');
    else serial_write_char(c);

    if (vga_redir.active) {
        uint8_t color = (uint8_t)((vga_redir.bg << 4) | (vga_redir.fg & 0x0F));
        if (c == '\n') {
            vga_redir.cx = 0;
            if (++vga_redir.cy == vga_redir.h) vga_redir_scroll();
        } else if (c == '\r') {
            vga_redir.cx = 0;
        } else if (c == '\b') {
            if (vga_redir.cx > 0) {
                vga_redir.cx--;
                vga_buf[(vga_redir.y0 + vga_redir.cy) * VGA_COLS + (vga_redir.x0 + vga_redir.cx)] =
                    vga_entry(' ', color);
            }
        } else {
            vga_buf[(vga_redir.y0 + vga_redir.cy) * VGA_COLS + (vga_redir.x0 + vga_redir.cx)] =
                vga_entry(c, color);
            if (++vga_redir.cx == vga_redir.w) {
                vga_redir.cx = 0;
                if (++vga_redir.cy == vga_redir.h) vga_redir_scroll();
            }
        }
        vga_row = vga_redir.y0 + vga_redir.cy;
        vga_col = vga_redir.x0 + vga_redir.cx;
        vga_update_cursor();
        return;
    }

    if (c == '\n') {
        vga_col = 0;
        if (++vga_row == VGA_ROWS) vga_scroll();
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buf[vga_row * VGA_COLS + vga_col] = vga_entry(' ', vga_color);
        }
    } else {
        vga_buf[vga_row * VGA_COLS + vga_col] = vga_entry(c, vga_color);
        if (++vga_col == VGA_COLS) {
            vga_col = 0;
            if (++vga_row == VGA_ROWS) vga_scroll();
        }
    }
    vga_update_cursor();
}

void vga_puts(const char *s) {
    for (; *s; s++) vga_putchar(*s);
}





static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void kmemcpy(void *dst, const void *src, size_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

static void kmemset(void *dst, uint8_t val, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) d[i] = val;
}

#define EI_NIDENT   16
#define ELFMAG0     0x7Fu
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define ELFCLASS64  2u
#define ET_EXEC     2u
#define ET_DYN      3u
#define EM_X86_64   62u
#define PT_LOAD     1u
#define PT_DYNAMIC  2u

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

typedef struct {
    uint64_t r_offset;   
    uint64_t r_info;     
    int64_t  r_addend;
} Elf64_Rela;

typedef struct {
    int64_t  d_tag;
    uint64_t d_val;
} Elf64_Dyn;

#define ELF64_R_TYPE(i)     ((i) & 0xFFFFFFFFULL)
#define R_X86_64_RELATIVE   8u   

#define DT_NULL    0
#define DT_RELA    7    
#define DT_RELASZ  8    
#define DT_RELAENT 9    

static void kputs_sz(size_t v) {
    char buf[24];
    int  i = 0;
    if (v == 0) { vga_putchar('0'); return; }
    while (v > 0 && i < (int)sizeof(buf) - 1) {
        buf[i++] = (char)('0' + (int)(v % 10));
        v /= 10;
    }
    for (int j = i - 1; j >= 0; j--) vga_putchar(buf[j]);
}

static bool elf64_check(const uint8_t *data, size_t size) {
    if (size < sizeof(Elf64_Ehdr)) {
        vga_puts("  exec: file too small to be an ELF\n");
        return false;
    }
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 ||
        eh->e_ident[2] != ELFMAG2 || eh->e_ident[3] != ELFMAG3) {
        vga_puts("  exec: not an ELF file (bad magic)\n");
        return false;
    }
    if (eh->e_ident[4] != ELFCLASS64) {
        vga_puts("  exec: not a 64-bit ELF (need ELFCLASS64)\n");
        return false;
    }
    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) {
        vga_puts("  exec: ELF type is not executable\n");
        vga_puts("        (expected ET_EXEC or ET_DYN)\n");
        return false;
    }
    if (eh->e_machine != EM_X86_64) {
        vga_puts("  exec: not an x86-64 ELF (wrong e_machine)\n");
        return false;
    }
    return true;
}

static void elf64_apply_relocs(const Elf64_Phdr *dyn_ph, uint64_t load_base) {
    const Elf64_Dyn *dyn = (const Elf64_Dyn *)(uintptr_t)dyn_ph->p_vaddr;

    uint64_t rela_addr = 0, rela_sz = 0, rela_ent = sizeof(Elf64_Rela);

    for (const Elf64_Dyn *d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
            case DT_RELA:    rela_addr = d->d_val; break;
            case DT_RELASZ:  rela_sz   = d->d_val; break;
            case DT_RELAENT: rela_ent  = d->d_val; break;
        }
    }

    if (!rela_addr || !rela_sz || !rela_ent) return;

    const uint8_t *base = (const uint8_t *)(uintptr_t)rela_addr;
    for (uint64_t off = 0; off + rela_ent <= rela_sz; off += rela_ent) {
        const Elf64_Rela *rel = (const Elf64_Rela *)(base + off);
        if (ELF64_R_TYPE(rel->r_info) == R_X86_64_RELATIVE) {
            uint64_t *target = (uint64_t *)(uintptr_t)(load_base + rel->r_offset);
            *target = load_base + (uint64_t)rel->r_addend;
        }
    }
}

static int elf64_exec(const uint8_t *data, size_t size) {
    if (!elf64_check(data, size)) return -1;

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;

    uint64_t ph_end = eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(Elf64_Phdr);
    if (ph_end > (uint64_t)size) {
        vga_puts("  exec: program header table out of bounds\n");
        return -1;
    }

    const Elf64_Phdr *dyn_ph = (const Elf64_Phdr *)0;
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr *ph =
            (const Elf64_Phdr *)(data + eh->e_phoff + (uint64_t)i * sizeof(Elf64_Phdr));
        if (ph->p_type == PT_DYNAMIC) {
            if (eh->e_type == ET_DYN) {
                dyn_ph = ph;
            } else {
                vga_puts("  exec: binary needs a dynamic linker — not supported\n");
                vga_puts("        recompile with: -static -no-pie\n");
                return -1;
            }
        }
    }

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr *ph =
            (const Elf64_Phdr *)(data + eh->e_phoff + (uint64_t)i * sizeof(Elf64_Phdr));

        if (ph->p_type != PT_LOAD) continue;

        if (ph->p_offset + ph->p_filesz > (uint64_t)size) {
            vga_puts("  exec: segment data exceeds file size\n");
            return -1;
        }
        if (ph->p_vaddr + ph->p_memsz < ph->p_vaddr) {
            vga_puts("  exec: segment address overflow\n");
            return -1;
        }
        if (ph->p_vaddr < 0x200000ULL ||
            ph->p_vaddr + ph->p_memsz > 0x40000000ULL) {
            vga_puts("  exec: segment vaddr out of safe range\n");
            vga_puts("        (must be 0x200000 – 0x40000000)\n");
            vga_puts("        recompile with: -Ttext 0x400000\n");
            return -1;
        }

        uint8_t       *dst = (uint8_t *)(uintptr_t)ph->p_vaddr;
        const uint8_t *src = data + ph->p_offset;

        kmemcpy(dst, src, (size_t)ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            kmemset(dst + ph->p_filesz, 0, (size_t)(ph->p_memsz - ph->p_filesz));
    }

    if (dyn_ph) elf64_apply_relocs(dyn_ph, 0);

    typedef int (*entry_t)(void);
    entry_t entry = (entry_t)(uintptr_t)eh->e_entry;
    return entry();
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


#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103
#define KEY_CTRL_J 0x104
#define KEY_CTRL_T 0x105

static bool kb_shift = false;
static bool kb_caps  = false;
static bool kb_ctrl  = false;


static uint8_t kb_read_raw(void) {
    for (;;) {
        uint8_t st = inb(0x64);
        if (!(st & 0x01)) continue;
        uint8_t data = inb(0x60);
        if (!(st & 0x20)) return data;  
    }
}


int kb_getkey(void) {
    uint8_t sc = kb_read_raw();

    
    if (sc == 0xE0) {
        uint8_t ext = kb_read_raw();
        if (ext & 0x80) return 0;  
        switch (ext) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            default:   return 0;
        }
    }

    
    if (sc & 0x80) {
        uint8_t rel = sc & 0x7F;
        if (rel == 0x2A || rel == 0x36) kb_shift = false;
        if (rel == 0x1D) kb_ctrl = false;
        return 0;
    }

    
    if (sc == 0x2A || sc == 0x36) { kb_shift = true;           return 0; }
    if (sc == 0x3A)                { kb_caps  = !kb_caps;       return 0; }
    if (sc == 0x1D)                { kb_ctrl = true;            return 0; }

    if (sc >= 128) return 0;

    char c = kb_shift ? sc_shift[sc] : sc_normal[sc];

    if (kb_ctrl && (c == 'j' || c == 'J')) return KEY_CTRL_J;
    if (kb_ctrl && (c == 't' || c == 'T')) return KEY_CTRL_T;
    if (kb_ctrl) return 0;

    
    if (kb_caps && c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (kb_caps && c >= 'A' && c <= 'Z' && !kb_shift) c = (char)(c + 32);

    return c;
}















typedef void (*cmd_fn)(void);

typedef struct {
    const char *name;   
    const char *desc;   
    cmd_fn      fn;     
} Command;


static void cmd_help(void);
static void cmd_halt(void);
static void cmd_mkpan(void);
static void cmd_kpc(void);
static void cmd_color(void);
static void cmd_calculate(void);
static void cmd_call(void);
static void cmd_sxgui(void);
static void cmd_beep(void);
static void cmd_files(void);
static void cmd_exec(void);
static void reboot(void);
static void echo(void);

static uint8_t cmos_read(uint8_t reg);
static uint8_t bcd_to_bin(uint8_t bcd);

static void shell_execute(const char *line);

static void cmd_mkpan(void) {
    kernel_panic("The SilkOS Kernel has had a fatal error and cannot recover: 0x001");
}

static void cmd_kpc(void) {
    vga_puts("Kernel Panic Codes:\n0x001: Manually Triggered Panic\n0x002: Failed Reboot\n");
}

static void cmd_reboot(void) {
    vga_set_color(CLR_YELLOW, CLR_BLACK);
    vga_puts("\n  Rebooting SilkOS...\n");

    
    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);

    
    outb(0x64, 0xFE);

    
    kernel_panic("The SilkOS Kernel has had a fatal error and cannot recover: 0x002");
}
static const char *cmd_args = "";

static void cmd_echo(void) {
    vga_puts("\n  ");
    vga_set_color(CLR_WHITE, CLR_BLACK);
    vga_puts(cmd_args);
    vga_puts("\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
}

static bool streq(const char *a, const char *b) {
    return kstrcmp(a, b) == 0;
}

static void cmd_color(void) {
    const char *a = cmd_args;
    while (*a == ' ') a++;

    if (*a == '\0') {
        vga_puts("\n  Usage: color 0x0|0x1|0x2|0x3|0x4 | color reset\n");
        return;
    }

    if (streq(a, "reset")) {
        vga_recolor_screen(CLR_WHITE, CLR_BLACK);
        vga_puts("\n  Color reset.\n");
        return;
    }

    if (a[0] == '0' && (a[1] == 'x' || a[1] == 'X')) a += 2;

    char d = a[0];
    uint8_t fg;
    switch (d) {
        case '0': fg = CLR_RED;    break;
        case '1': fg = CLR_BLUE;   break;
        case '2': fg = CLR_YELLOW; break;
        case '3': fg = CLR_BROWN;  break;
        case '4': fg = CLR_BLACK;  break;
        default:
            vga_puts("\n  Unknown color. Use 0x0..0x4 or reset.\n");
            return;
    }

    vga_recolor_screen(fg, CLR_BLACK);
    vga_puts("\n  Color set.\n");
}

static void kputs_i32(int32_t v) {
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

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static bool parse_i32(const char **pp, int32_t *out) {
    const char *p = skip_ws(*pp);
    bool neg = false;
    if (*p == '+') p++;
    else if (*p == '-') { neg = true; p++; }
    p = skip_ws(p);
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

static int op_prec(char op) {
    if (op == '*' || op == '/') return 2;
    if (op == '+' || op == '-') return 1;
    return 0;
}

static bool apply_op(int32_t *vals, int *vtop, char op) {
    if (*vtop < 1) return false;
    int32_t b = vals[(*vtop)--];
    int32_t a = vals[(*vtop)--];
    int32_t r = 0;
    switch (op) {
        case '+': r = a + b; break;
        case '-': r = a - b; break;
        case '*': r = a * b; break;
        case '/': if (b == 0) return false; r = a / b; break;
        default: return false;
    }
    vals[++(*vtop)] = r;
    return true;
}

static void cmd_calculate(void) {
    const char *p = cmd_args;
    int32_t vals[64];
    char ops[64];
    int vtop = -1;
    int otop = -1;

    for (;;) {
        p = skip_ws(p);
        if (*p == '\0') break;

        int32_t num;
        if (!parse_i32(&p, &num)) {
            vga_puts("\n  calculate: parse error\n");
            return;
        }
        vals[++vtop] = num;

        p = skip_ws(p);
        char op = *p;
        if (op == '\0') break;
        if (op != '+' && op != '-' && op != '*' && op != '/') {
            vga_puts("\n  calculate: expected operator\n");
            return;
        }
        p++;

        while (otop >= 0 && op_prec(ops[otop]) >= op_prec(op)) {
            if (!apply_op(vals, &vtop, ops[otop--])) {
                vga_puts("\n  calculate: math error\n");
                return;
            }
        }
        ops[++otop] = op;
    }

    while (otop >= 0) {
        if (!apply_op(vals, &vtop, ops[otop--])) {
            vga_puts("\n  calculate: math error\n");
            return;
        }
    }

    if (vtop != 0) {
        vga_puts("\n  calculate: parse error\n");
        return;
    }

    vga_puts("\n  = ");
    kputs_i32(vals[0]);
    vga_puts("\n");
}

static void cmd_call(void) {
    const char *p = cmd_args;
    p = skip_ws(p);
    if (*p == '\0') {
        vga_puts("\n  Usage: call <command> [; command2 ...]\n");
        return;
    }

    char buf[256];
    size_t bi = 0;
    while (*p) {
        if (*p == ';') {
            buf[bi] = '\0';
            const char *cmd = skip_ws(buf);
            if (*cmd) shell_execute(cmd);
            bi = 0;
            p++;
            continue;
        }
        if (bi + 1 < sizeof(buf)) buf[bi++] = *p;
        p++;
    }
    buf[bi] = '\0';
    const char *cmd = skip_ws(buf);
    if (*cmd) shell_execute(cmd);
}

static void cmd_beep(void) {
    const char *p = cmd_args;
    int32_t vol, freq, dur;

    if (!parse_i32(&p, &vol) || !parse_i32(&p, &freq) || !parse_i32(&p, &dur)) {
        vga_puts("\n  Usage: beep <volume 1-100> <frequency Hz> <duration ms>\n");
        vga_puts("  Example: beep 16 400 200\n");
        return;
    }
    if (vol < 1 || vol > 100) { vga_puts("\n  beep: volume must be 1-100\n");         return; }
    if (freq < 20 || freq > 20000) { vga_puts("\n  beep: frequency must be 20-20000 Hz\n"); return; }
    if (dur  < 1 || dur  > 30000) { vga_puts("\n  beep: duration must be 1-30000 ms\n");   return; }

    beep_play((uint32_t)vol, (uint32_t)freq, (uint32_t)dur);
}

static void vga_put_at(size_t x, size_t y, char c, uint8_t fg, uint8_t bg) {
    if (x >= VGA_COLS || y >= VGA_ROWS) return;
    uint8_t color = (uint8_t)((bg << 4) | (fg & 0x0F));
    vga_buf[y * VGA_COLS + x] = vga_entry(c, color);
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

static void shell_execute(const char *line);

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
    return vga_entry('\x10', (uint8_t)((fg << 4) | bg));
}

static void cmd_sxgui(void) {
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



static void cmd_clear(void) {
    vga_init();
}



static void cmd_ver(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  OS      : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("SilkOS\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Version : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("1.0.0 Codename 'Repina' (stable)\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Arch    : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("x86-64 long mode\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Boot    : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("GRUB Multiboot\n");
    vga_set_color(CLR_LGRAY,  CLR_BLACK);
}

static void cmd_neofetch(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  OS      : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("SilkOS\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Version : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("1.0.0 Codename 'Repina' (stable)\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Arch    : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("x86-64 long mode\n");
    vga_set_color(CLR_LCYAN,  CLR_BLACK); vga_puts("  Boot    : "); vga_set_color(CLR_WHITE, CLR_BLACK); vga_puts("GRUB Multiboot\n");
    vga_set_color(CLR_LGRAY,  CLR_BLACK);
}



static void cmd_cpuid(void) {
    
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




static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)(((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F));
}


static void print_d2(uint8_t v) {
    vga_putchar((char)('0' + v / 10));
    vga_putchar((char)('0' + v % 10));
}

static void cmd_time(void) {   
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

static void cmd_files(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("  Embedded files:\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);

    if (silk_file_count == 0) {
        vga_puts("  (none)\n");
        vga_puts("  Bundle files with: make EXTRA_FILES=\"a.elf b.bin\"\n");
        vga_puts("                 or: ./build --files a.elf b.bin\n");
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

static void cmd_exec(void) {
    const char *name = cmd_args;
    while (*name == ' ') name++;

    if (*name == '\0') {
        vga_puts("\n  Usage: exec <filename>\n");
        vga_puts("  Use 'files' to list available binaries.\n");
        return;
    }

    for (size_t i = 0; i < silk_file_count; i++) {
        if (kstrcmp(silk_files[i].name, name) == 0) {
            vga_puts("\n");
            int rc = elf64_exec(silk_files[i].data, silk_file_size(&silk_files[i]));
            if (rc >= 0) {
                vga_puts("\n  [process exited: ");
                kputs_i32(rc);
                vga_puts("]\n");
            }
            return;
        }
    }

    vga_puts("\n  exec: '");
    vga_puts(name);
    vga_puts("' not found  (run 'files' for a list)\n");
}

static Command commands[] = {
    { "help", "Show this help message",     cmd_help },
    { "halt", "Shut down the kernel / shell",   cmd_halt },
    { "mkpan", "Manually trigger a Kernel panic.",  cmd_mkpan },
    { "kpc", "List of kernel panic codes for debugging.",   cmd_kpc},
    { "reboot", "Reboot the system", cmd_reboot },
    { "echo", "echo the given raw string", cmd_echo },
    { "color", "Set text color (0x0..0x4 or reset)", cmd_color },
    { "calculate", "Evaluate integer expression (ex: 2 + 2)", cmd_calculate },
    { "call", "Run another command (supports ';')", cmd_call },
    { "sxgui", "Launch Sx desktop (Ctrl+J to quit)", cmd_sxgui },
    { "clear",  "Clear the screen",                          cmd_clear  },  
    { "ver",    "Show kernel and system version info",       cmd_ver    },
    { "cpuid",  "Show CPU vendor and brand string",          cmd_cpuid  },
    { "time",   "Show current date and time from RTC",       cmd_time   },
    { "neofetch", "custom neofetch for SilkOS",    cmd_neofetch},
    { "beep",    "Play a tone: beep <vol 1-100> <freq Hz> <ms>", cmd_beep },
    { "files",   "List files bundled into this kernel image",    cmd_files },
    { "exec",    "Run an embedded ELF64 binary: exec <name>",    cmd_exec  },
};

#define CMD_COUNT ((size_t)(sizeof(commands) / sizeof(commands[0])))

static void cmd_help(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("  Available commands:\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
    vga_puts("\n");

    for (size_t i = 0; i < CMD_COUNT; i++) {
        vga_puts("  ");
        vga_set_color(CLR_LGREEN, CLR_BLACK);
        vga_puts(commands[i].name);
        vga_set_color(CLR_LGRAY, CLR_BLACK);       
        size_t pad = 12 - kstrlen(commands[i].name);
        for (size_t p = 0; p < pad; p++) vga_putchar(' ');
        vga_puts(commands[i].desc);
        vga_puts("\n");
    }
    vga_puts("\n");
}

static bool kernel_running = true;

static void cmd_halt(void) {
    vga_puts("\n");
    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("  Halting SilkOS... Goodbye!\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
    kernel_running = false;
}



#define INPUT_MAX    256
#define HISTORY_CAP   16   

static char history[HISTORY_CAP][INPUT_MAX];
static int  history_count = 0;   
static int  history_nav   = -1;  

static void history_push(const char *cmd) {
    if (history_count < HISTORY_CAP) {
        kmemcpy(history[history_count], cmd, kstrlen(cmd) + 1);
        history_count++;
    } else {
        
        for (int i = 0; i < HISTORY_CAP - 1; i++)
            kmemcpy(history[i], history[i + 1], INPUT_MAX);
        kmemcpy(history[HISTORY_CAP - 1], cmd, kstrlen(cmd) + 1);
    }
}


static void shell_erase_chars(int n) {
    for (int i = 0; i < n; i++) {
        vga_putchar('\b');
        vga_putchar(' ');
        vga_putchar('\b');
    }
}

static void shell_print_prompt(void) {
    vga_puts("\nsilk> ");
}



static void shell_execute(const char *line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    for (size_t i = 0; i < CMD_COUNT; i++) {
        size_t nlen = kstrlen(commands[i].name);
        if (kstrcmp(line, commands[i].name) == 0) {
            
            cmd_args = "";
            commands[i].fn();
            return;
        }
        
        bool prefix_match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (line[j] != commands[i].name[j]) { prefix_match = false; break; }
        }
        if (prefix_match && line[nlen] == ' ') {
            const char *args = line + nlen + 1;
            while (*args == ' ') args++;   
            cmd_args = args;
            commands[i].fn();
            return;
        }
    }

    vga_puts("\n  ");
    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("Unknown command: ");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
    vga_puts(line);
    vga_puts("  (try 'help')\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
}

void shell_run(void) {
    char input[INPUT_MAX];
    int  input_len = 0;

    vga_set_color(CLR_LGRAY, CLR_BLACK);

    shell_print_prompt();

    while (kernel_running) {
        int key = kb_getkey();
        if (!key) continue;

        
        if (key == '\n' || key == '\r') {
            input[input_len] = '\0';
            if (input_len > 0) {
                history_push(input);
                history_nav = -1;
            }
            shell_execute(input);
            input_len = 0;
            if (kernel_running) shell_print_prompt();

        
        } else if (key == '\b') {
            if (input_len > 0) {
                input_len--;
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            }

        
        } else if (key == KEY_UP) {
            if (history_count == 0) continue;

            if (history_nav == -1)
                history_nav = history_count - 1;
            else if (history_nav > 0)
                history_nav--;

            shell_erase_chars(input_len);
            size_t hlen = kstrlen(history[history_nav]);
            kmemcpy(input, history[history_nav], hlen + 1);
            input_len = (int)hlen;
            vga_puts(input);

        
        } else if (key == KEY_DOWN) {
            if (history_nav == -1) continue;
            history_nav++;

            shell_erase_chars(input_len);

            if (history_nav >= history_count) {
                history_nav = -1;
                input_len   = 0;
            } else {
                size_t hlen = kstrlen(history[history_nav]);
                kmemcpy(input, history[history_nav], hlen + 1);
                input_len = (int)hlen;
                vga_puts(input);
            }

        
        } else if (key >= 0x20 && key < 0x7F) {
            if (input_len < INPUT_MAX - 1) {
                input[input_len++] = (char)key;
                vga_putchar((char)key);
            }
        }
    }
}





void kernel_main(void) {
    serial_init();
    serial_write("SilkOS: kernel_main entered\n");
    vga_init();

    
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("Booting SilkOS kernel...\n");
    usleep_mil(200);
    vga_puts("Setting basic shell config...ok\n");
    usleep_mil(89);
    vga_set_color(CLR_LMAGENTA, CLR_BLACK);
    vga_puts("\n\nWelcome to SilkOS!\n\n");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    usleep_mil(400);
    vga_puts("Working.");
    usleep_mil(100);
    vga_puts(".");
    usleep_mil(100);
    vga_puts(".");
    usleep_mil(100);
    vga_puts(".");
    usleep_mil(100);
    vga_puts(".\n");
    usleep_mil(18);
    vga_puts("Finding commands\n\n");
    usleep_mil(12);
    vga_puts("Finished booting. Run help or visit repinafamily.us/silk/manual for help.");
    vga_set_color(CLR_LGRAY, CLR_BLACK);

    shell_run();

    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
