#include "cmd.h"

const char *cmd_args = "";

const Command commands[] = {
    { "help",      "Show this help message",                        cmd_help },
    { "halt",      "Shut down the kernel / shell",                  cmd_halt },
    { "mkpan",     "Manually trigger a Kernel panic.",              cmd_mkpan },
    { "kpc",       "List of kernel panic codes for debugging.",     cmd_kpc },
    { "reboot",    "Reboot the system",                             cmd_reboot },
    { "echo",      "echo the given raw string",                     cmd_echo },
    { "color",     "Set text color (0x0..0x4 or reset)",            cmd_color },
    { "calculate", "Evaluate integer expression (ex: 2 + 2)",       cmd_calculate },
    { "call",      "Run another command (supports ';')",            cmd_call },
    { "sxgui",     "Launch Sx desktop (Ctrl+J to quit)",            cmd_sxgui },
    { "clear",     "Clear the screen",                              cmd_clear },
    { "ver",       "Show kernel and system version info",           cmd_ver },
    { "cpuid",     "Show CPU vendor and brand string",              cmd_cpuid },
    { "time",      "Show current date and time from RTC",           cmd_time },
    { "neofetch",  "custom neofetch for SilkOS",                    cmd_neofetch },
    { "beep",      "Play a tone: beep <vol 1-100> <freq Hz> <ms>",  cmd_beep },
    { "files",     "List files bundled into this kernel image",     cmd_files },
    { "exec",      "Run an embedded ELF64 binary: exec <name>",     cmd_exec },
    { "render",    "Draw a PPM image on screen: render <file.ppm>", cmd_render },
};

const size_t cmd_count = sizeof(commands) / sizeof(commands[0]);
