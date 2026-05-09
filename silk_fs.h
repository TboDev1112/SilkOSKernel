#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * silk_fs.h — embedded file table for SilkOS
 *
 * Files are bundled into the kernel binary at build time with:
 *
 *   make EXTRA_FILES="myapp.elf foo.bin"
 *     — or —
 *   ./build --files myapp.elf foo.bin
 *
 * Inside the kernel use the 'files' command to list them and
 * 'exec <name>' to load and run an ELF64 binary.
 */

typedef struct {
    const char    *name;     /* basename of the original file          */
    const uint8_t *data;     /* pointer to first byte in kernel image  */
    const uint8_t *data_end; /* pointer one past the last byte         */
} SilkFile;

/* Byte count of an embedded file — computed from the two pointers because
 * pointer arithmetic between extern symbols is not a C constant expression
 * and therefore cannot appear in a static initializer. */
static inline size_t silk_file_size(const SilkFile *f) {
    return (size_t)(f->data_end - f->data);
}

extern const SilkFile silk_files[];
extern const size_t   silk_file_count;
