





MBALIGN  equ 1 << 0
MEMINFO  equ 1 << 1
MB_FLAGS equ MBALIGN | MEMINFO
MB_MAGIC equ 0x1BADB002
MB_CHECK equ -(MB_MAGIC + MB_FLAGS)


section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECK


section .bss
align 16
stack_bottom:
    resb 16384
stack_top:


section .text
global _start
extern kernel_main

_start:
    mov  esp, stack_top
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang
