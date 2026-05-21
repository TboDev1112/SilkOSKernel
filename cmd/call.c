#include "cmd.h"
#include "util.h"
#include "../kernel.h"

void cmd_call(void) {
    const char *p = cmd_args;
    p = cmd_skip_ws(p);
    if (*p == '\0') {
        vga_puts("\n  Usage: call <command> [; command2 ...]\n");
        return;
    }

    char buf[256];
    size_t bi = 0;
    while (*p) {
        if (*p == ';') {
            buf[bi] = '\0';
            const char *cmd = cmd_skip_ws(buf);
            if (*cmd) shell_execute(cmd);
            bi = 0;
            p++;
            continue;
        }
        if (bi + 1 < sizeof(buf)) buf[bi++] = *p;
        p++;
    }
    buf[bi] = '\0';
    const char *cmd = cmd_skip_ws(buf);
    if (*cmd) shell_execute(cmd);
}
