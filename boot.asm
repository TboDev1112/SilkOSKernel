; ============================================================
;  boot.asm — SilkOS Multiboot Bootloader
;  Assembled with: nasm -f elf32 boot.asm -o boot.o
; ============================================================

; --- Multiboot Header Constants ---
MBALIGN  equ 1 << 0            ; Align modules on page boundaries
MEMINFO  equ 1 << 1            ; Provide memory map
MB_FLAGS equ MBALIGN | MEMINFO
MB_MAGIC equ 0x1BADB002        ; Magic number GRUB looks for
MB_CHECK equ -(MB_MAGIC + MB_FLAGS)

; --- Multiboot Header (must be within first 8KB of binary) ---
section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECK

; --- Stack (16 KiB, 16-byte aligned for SSE compliance) ---
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; --- Entry Point ---
section .text
global _start
extern kernel_main

_start:
    mov  esp, stack_top     ; Set up stack pointer
    call kernel_main        ; Jump into the C kernel
    ; kernel_main should never return, but if it does:
    cli                     ; Disable interrupts
.hang:
    hlt                     ; Halt the CPU
    jmp .hang               ; Loop forever just in case
