// ============================================================
//  kernel.c — SilkOS Kernel v0.0.1
//
//  Sections:
//    1.  I/O Port Access
//    2.  VGA Text Mode Driver
//    3.  String Utilities
//    4.  Keyboard Driver
//    5.  Shell Command Table  ← ADD YOUR COMMANDS HERE
//    6.  Built-in Command Implementations
//    7.  Shell Engine
//    8.  Kernel Entry Point
// ============================================================

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Forward declarations
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_puts(const char *s);
void kernel_panic(const char *msg);

// ============================================================
// 1. I/O Port Access
// ============================================================

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ============================================================
// 2. VGA Text Mode Driver
// ============================================================

#define VGA_COLS   80
#define VGA_ROWS   25
#define VGA_BUF    ((uint16_t *)0xB8000)

// VGA color codes (foreground | background << 4)
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
    vga_color = (CLR_BLACK << 4) | CLR_LGRAY;  // grey on black
    vga_buf   = VGA_BUF;
    vga_row   = 0;
    vga_col   = 0;
    for (size_t i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga_buf[i] = vga_entry(' ', vga_color);
    vga_update_cursor();
}

// ============================================================
// Kernel Panic
// ============================================================

void kernel_panic(const char *msg) {
    // Disable interrupts immediately — no going back
    __asm__ volatile ("cli");

    // Red on black — hard to miss
    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("\n\n");
    vga_puts("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    vga_puts("  !!          *** KERNEL PANIC ***        !!\n");
    vga_puts("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");

    vga_set_color(CLR_WHITE, CLR_BLACK);
    vga_puts("  Reason : ");
    vga_set_color(CLR_YELLOW, CLR_BLACK);
    vga_puts(msg);

    vga_set_color(CLR_LGRAY, CLR_BLACK);
    vga_puts("\n\n  The system has been halted.\n");
    vga_puts("  Restart your machine to continue.\n\n");
    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    // Halt forever — no recovery
    for (;;) __asm__ volatile ("hlt");
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_putchar(char c) {
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

// ============================================================
// 3. String Utilities
// ============================================================

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

// ============================================================
// 4. Keyboard Driver  (polling, no interrupts required)
// ============================================================

// US QWERTY scan code set 1 — unshifted
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

// US QWERTY scan code set 1 — shifted
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

// Special key return values (above ASCII range)
#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103

static bool kb_shift = false;
static bool kb_caps  = false;

// Block until the keyboard output buffer is ready, then read a byte
static uint8_t kb_read_raw(void) {
    while (!(inb(0x64) & 0x01));   // wait for OBF (output buffer full)
    return inb(0x60);
}

// Returns an int: printable ASCII char, KEY_* constant, or 0 (ignore)
int kb_getkey(void) {
    uint8_t sc = kb_read_raw();

    // Extended key prefix (arrows, etc.)
    if (sc == 0xE0) {
        uint8_t ext = kb_read_raw();
        if (ext & 0x80) return 0;  // release — ignore
        switch (ext) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            default:   return 0;
        }
    }

    // Key release (high bit set)
    if (sc & 0x80) {
        uint8_t rel = sc & 0x7F;
        if (rel == 0x2A || rel == 0x36) kb_shift = false;
        return 0;
    }

    // Modifier tracking
    if (sc == 0x2A || sc == 0x36) { kb_shift = true;           return 0; }
    if (sc == 0x3A)                { kb_caps  = !kb_caps;       return 0; }
    if (sc == 0x1D)                { /* Ctrl — ignored for now */ return 0; }

    if (sc >= 128) return 0;

    char c = kb_shift ? sc_shift[sc] : sc_normal[sc];

    // Apply Caps Lock to letters only
    if (kb_caps && c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (kb_caps && c >= 'A' && c <= 'Z' && !kb_shift) c = (char)(c + 32);

    return c;
}

// ============================================================
// 5. Shell Command Table
//
//  HOW TO ADD A COMMAND:
//  ──────────────────────
//  a) Write your handler function (must match signature below):
//       static void cmd_mycommand(void) { ... }
//
//  b) Add an entry to the `commands[]` array below:
//       { "mycommand", "What it does", cmd_mycommand },
//
//  That's it — it will automatically appear in `help`.
// ============================================================

typedef void (*cmd_fn)(void);

typedef struct {
    const char *name;   // what the user types
    const char *desc;   // shown by `help`
    cmd_fn      fn;     // handler function
} Command;

// Forward-declare built-ins
static void cmd_help(void);
static void cmd_halt(void);
static void cmd_mkpan(void);
static void cmd_kpc(void);
static void reboot(void);

static void cmd_mkpan(void) {
    kernel_panic("The SilkOS Kernel has had a fatal error and cannot recover: 0x001");
}

static void cmd_kpc(void) {
    vga_puts("Kernel Panic Codes:\n0x001: Manually Triggered Panic\n");
}

static void cmd_reboot(void) {
    vga_set_color(CLR_YELLOW, CLR_BLACK);
    vga_puts("\n  Rebooting SilkOS...\n");

    // Flush the keyboard controller — required before triggering reset
    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);

    // Pulse the CPU reset line via the keyboard controller
    outb(0x64, 0xFE);

    // If the above didn't work, triple-fault as a fallback
    kernel_panic("Reboot failed — could not reset via keyboard controller.");
}
// ┌─────────────────────────────────────────────────────────┐
// │              COMMAND TABLE — edit below                  │
// └─────────────────────────────────────────────────────────┘
static Command commands[] = {
    { "help", "Show this help message",     cmd_help },
    { "halt", "Shut down the kernel / shell",   cmd_halt },
    { "mkpan", "Manually trigger a Kernel panic.",  cmd_mkpan },
    { "kpc", "List of kernel panic codes for debugging.",   cmd_kpc},
    { "reboot", "Reboot the system", cmd_reboot },
    // { "mycommand", "Description here", cmd_mycommand },
};
// ┌─────────────────────────────────────────────────────────┐
// │                    end of table                          │
// └─────────────────────────────────────────────────────────┘

#define CMD_COUNT ((size_t)(sizeof(commands) / sizeof(commands[0])))

// ============================================================
// 6. Built-in Command Implementations
// ============================================================

static void cmd_help(void) {
    vga_puts("\n");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("  Available commands:\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
    vga_puts("  ──────────────────────────────────────\n");

    for (size_t i = 0; i < CMD_COUNT; i++) {
        vga_puts("  ");
        vga_set_color(CLR_LGREEN, CLR_BLACK);
        vga_puts(commands[i].name);
        vga_set_color(CLR_LGRAY, CLR_BLACK);
        // Pad to align descriptions
        size_t pad = 12 - kstrlen(commands[i].name);
        for (size_t p = 0; p < pad; p++) vga_putchar(' ');
        vga_puts(commands[i].desc);
        vga_puts("\n");
    }
    vga_puts("  ──────────────────────────────────────\n");
}

static bool kernel_running = true;

static void cmd_halt(void) {
    vga_puts("\n");
    vga_set_color(CLR_LRED, CLR_BLACK);
    vga_puts("  Halting SilkOS... Goodbye!\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);
    kernel_running = false;
}

// Paste this handler alongside your other cmd_* functions:

// ============================================================
// 7. Shell Engine
// ============================================================

#define INPUT_MAX    256
#define HISTORY_CAP   16   // number of history entries to keep

static char history[HISTORY_CAP][INPUT_MAX];
static int  history_count = 0;   // how many entries are stored
static int  history_nav   = -1;  // -1 = not browsing history

static void history_push(const char *cmd) {
    if (history_count < HISTORY_CAP) {
        kmemcpy(history[history_count], cmd, kstrlen(cmd) + 1);
        history_count++;
    } else {
        // Discard oldest, shift everything down
        for (int i = 0; i < HISTORY_CAP - 1; i++)
            kmemcpy(history[i], history[i + 1], INPUT_MAX);
        kmemcpy(history[HISTORY_CAP - 1], cmd, kstrlen(cmd) + 1);
    }
}

// Erase `n` characters visually: backspace, space, backspace
static void shell_erase_chars(int n) {
    for (int i = 0; i < n; i++) {
        vga_putchar('\b');
        vga_putchar(' ');
        vga_putchar('\b');
    }
}

static void shell_print_prompt(void) {
    vga_set_color(CLR_LGREEN, CLR_BLACK);
    vga_puts("\nsilk");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("> ");
    vga_set_color(CLR_WHITE, CLR_BLACK);
}

static void shell_execute(const char *line) {
    // Trim leading spaces
    while (*line == ' ') line++;
    if (*line == '\0') return;

    for (size_t i = 0; i < CMD_COUNT; i++) {
        if (kstrcmp(line, commands[i].name) == 0) {
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
    vga_puts("  Type 'help' for a list of commands.\n");

    shell_print_prompt();

    while (kernel_running) {
        int key = kb_getkey();
        if (!key) continue;

        // ── Enter ────────────────────────────────────────────
        if (key == '\n' || key == '\r') {
            input[input_len] = '\0';
            if (input_len > 0) {
                history_push(input);
                history_nav = -1;
            }
            shell_execute(input);
            input_len = 0;
            if (kernel_running) shell_print_prompt();

        // ── Backspace ────────────────────────────────────────
        } else if (key == '\b') {
            if (input_len > 0) {
                input_len--;
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            }

        // ── Up arrow: go back in history ─────────────────────
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

        // ── Down arrow: go forward in history ────────────────
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

        // ── Printable character ───────────────────────────────
        } else if (key >= 0x20 && key < 0x7F) {
            if (input_len < INPUT_MAX - 1) {
                input[input_len++] = (char)key;
                vga_putchar((char)key);
            }
        }
    }
}

// ============================================================
// 8. Kernel Entry Point
// ============================================================

void kernel_main(void) {
    vga_init();

    // ── Boot banner ───────────────────────────────────────────
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("  ╔══════════════════════════════════════════╗\n");
    vga_puts("  ║                                          ║\n");
    vga_set_color(CLR_WHITE, CLR_BLACK);
    vga_puts("  ║     Booted SilkOS Kernel v0.0.1          ║\n");
    vga_set_color(CLR_LCYAN, CLR_BLACK);
    vga_puts("  ║                                          ║\n");
    vga_puts("  ╚══════════════════════════════════════════╝\n\n");
    vga_set_color(CLR_LGRAY, CLR_BLACK);

    shell_run();

    // Shell has exited (halt command) — power down or spin
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
