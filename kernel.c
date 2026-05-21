#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "kernel.h"
#include "cmd/cmd.h"
#include "silk_fs.h"

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


void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
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

void beep_play(uint32_t vol, uint32_t freq, uint32_t ms) {
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





#define VGA_BUF    ((uint16_t *)(uintptr_t)0xB8000)

static uint8_t  vga_color;
uint16_t       *vga_buf;
size_t          vga_row, vga_col;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

void vga_update_cursor(void) {
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

void vga_recolor_screen(uint8_t fg, uint8_t bg) {
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

void vga_redir_begin(size_t x0, size_t y0, size_t w, size_t h, uint8_t fg, uint8_t bg) {
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

void vga_redir_end(void) {
    vga_redir.active = false;
    vga_row = vga_saved_row;
    vga_col = vga_saved_col;
    vga_update_cursor();
}

void vga_redir_clear(void) {
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





size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

void kmemcpy(void *dst, const void *src, size_t n) {
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

void kputs_sz(size_t v) {
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

int elf64_exec(const uint8_t *data, size_t size) {
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


bool kb_shift = false;
bool kb_caps  = false;
bool kb_ctrl  = false;


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












static bool kernel_running = true;

void kernel_request_halt(void) {
    kernel_running = false;
}

bool kernel_is_running(void) {
    return kernel_running;
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



void shell_execute(const char *line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    for (size_t i = 0; i < cmd_count; i++) {
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

    while (kernel_is_running()) {
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
            if (kernel_is_running()) shell_print_prompt();

        
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





void kernel_main(uint64_t mbi_phys) {
    serial_init();
    serial_write("SilkOS: kernel_main entered\n");
    silk_fs_init(mbi_phys);
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
