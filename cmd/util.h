#pragma once

#include <stdint.h>
#include <stdbool.h>

const char *cmd_skip_ws(const char *p);
bool        cmd_parse_i32(const char **pp, int32_t *out);
bool        cmd_streq(const char *a, const char *b);
void        cmd_kputs_i32(int32_t v);

uint8_t cmos_read(uint8_t reg);
uint8_t bcd_to_bin(uint8_t bcd);
void    print_d2(uint8_t v);
