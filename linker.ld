/* linker.ld — SilkOS Kernel Linker Script
 *
 * Loads the kernel at the 1 MiB mark (0x100000), which is
 * conventional for x86 kernels booted by GRUB / multiboot.
 */

ENTRY(_start)

SECTIONS
{
    /* Start placing sections at the 1 MiB physical address */
    . = 1M;

    /* Multiboot header must be within the first 8 KiB of the binary */
    .multiboot : {
        *(.multiboot)
    }

    /* Executable code */
    .text ALIGN(4K) : {
        *(.text)
    }

    /* Read-only data (string literals, const globals, etc.) */
    .rodata ALIGN(4K) : {
        *(.rodata)
    }

    /* Initialized read/write data */
    .data ALIGN(4K) : {
        *(.data)
    }

    /* Zero-initialized data (BSS) */
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }
}
