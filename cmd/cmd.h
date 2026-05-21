#pragma once

#include <stddef.h>

typedef void (*cmd_fn)(void);

typedef struct {
    const char *name;
    const char *desc;
    cmd_fn      fn;
} Command;

extern const char *cmd_args;

extern const Command commands[];
extern const size_t cmd_count;

void cmd_help(void);
void cmd_halt(void);
void cmd_mkpan(void);
void cmd_kpc(void);
void cmd_reboot(void);
void cmd_echo(void);
void cmd_color(void);
void cmd_calculate(void);
void cmd_call(void);
void cmd_sxgui(void);
void cmd_clear(void);
void cmd_ver(void);
void cmd_cpuid(void);
void cmd_time(void);
void cmd_neofetch(void);
void cmd_beep(void);
void cmd_files(void);
void cmd_exec(void);
void cmd_render(void);
void cmd_rndm(void);
