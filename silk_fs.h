#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char    *name;     
    const uint8_t *data;     
    const uint8_t *data_end; 
} SilkFile;

static inline size_t silk_file_size(const SilkFile *f) {
    return (size_t)(f->data_end - f->data);
}

extern const SilkFile silk_files[];
extern const size_t   silk_file_count;
