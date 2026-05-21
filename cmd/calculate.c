#include "cmd.h"
#include "util.h"
#include "../kernel.h"

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

void cmd_calculate(void) {
    const char *p = cmd_args;
    int32_t vals[64];
    char ops[64];
    int vtop = -1;
    int otop = -1;

    for (;;) {
        p = cmd_skip_ws(p);
        if (*p == '\0') break;

        int32_t num;
        if (!cmd_parse_i32(&p, &num)) {
            vga_puts("\n  calculate: parse error\n");
            return;
        }
        vals[++vtop] = num;

        p = cmd_skip_ws(p);
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
    cmd_kputs_i32(vals[0]);
    vga_puts("\n");
}
