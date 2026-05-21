
OUTPUT_FORMAT("elf64-x86-64")

ENTRY(_start)

__MB_MAGIC = 0x1BADB002;
__MB_FLAGS = 0x00010003; /* ALIGN | MEMINFO | HAS_ADDR */

SECTIONS
{
    . = 0x00100000;

    __kernel_load_start = .;

    .multiboot : {
        __mb_hdr = .;
        LONG(__MB_MAGIC);
        LONG(__MB_FLAGS);
        /* QEMU only checks magic+flags+checksum before reading the addr fields. */
        LONG(-(__MB_MAGIC + __MB_FLAGS));
        LONG(__mb_hdr);
        LONG(__kernel_load_start);
        LONG(__kernel_load_end);
        LONG(__kernel_bss_end);
        LONG(_start);
    }

    .text ALIGN(4K) : {
        *(.text)
    }

    .rodata ALIGN(4K) : {
        *(.rodata)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    __kernel_load_end = .;

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }

    __kernel_bss_end = .;
}
