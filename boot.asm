
; SilkOS x86-64 bootloader
; GRUB Multiboot1 enters in 32-bit protected mode.  We set up Long Mode
; page tables, switch to 64-bit, then hand off to kernel_main.

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


; Three 4 KB page tables for identity-mapping the first 1 GB.
section .bss
align 4096
pml4_table: resb 4096
pdp_table:  resb 4096
pd_table:   resb 4096

align 16
stack_bottom: resb 16384       ; 16 KB stack
stack_top:


; 64-bit GDT: null | code (ring 0, 64-bit) | data (ring 0)
section .data
gdt64:
    dq 0                                           ; null descriptor
.code: equ $ - gdt64                               ; selector = 0x08
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)      ; X, S=1, P, L=1
.ds:   equ $ - gdt64                               ; selector = 0x10
    dq (1<<41) | (1<<44) | (1<<47)                 ; W, S=1, P
gdt64_ptr:
    dw $ - gdt64 - 1                               ; limit = 23
    dd gdt64                                        ; 32-bit base (kernel < 4 GB)


section .text
bits 32
global _start
_start:
    ; Temporary stack for the 32-bit setup phase.
    mov esp, stack_top

    ; Zero all three page-table pages.
    mov edi, pml4_table
    xor eax, eax
    mov ecx, (3 * 4096) / 4
    rep stosd

    ; PML4[0] -> PDP table (present | writable)
    mov eax, pdp_table
    or  eax, 3
    mov [pml4_table], eax

    ; PDP[0] -> PD table (present | writable)
    mov eax, pd_table
    or  eax, 3
    mov [pdp_table], eax

    ; Fill 512 PD entries: 2 MB identity pages covering the first 1 GB.
    xor ecx, ecx
.fill_pd:
    mov eax, 0x200000
    mul ecx                                        ; EDX:EAX = 2 MB * ecx
    or  eax, 0x83                                  ; present | writable | PS (2 MB)
    mov dword [pd_table + ecx*8],     eax
    mov dword [pd_table + ecx*8 + 4], 0
    inc ecx
    cmp ecx, 512
    jl  .fill_pd

    ; Load PML4 address into CR3.
    mov eax, pml4_table
    mov cr3, eax

    ; Enable PAE (CR4 bit 5).
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; Set Long Mode Enable in EFER MSR (0xC0000080, bit 8).
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    ; Enable paging + protected mode to activate Long Mode.
    mov eax, cr0
    or  eax, (1 << 31) | 1
    mov cr0, eax

    ; Load the 64-bit GDT and jump to the code segment.
    lgdt [gdt64_ptr]
    jmp  gdt64.code:long_mode_start


bits 64
long_mode_start:
    ; Load data segment selector into all segment registers.
    mov ax, gdt64.ds
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Establish the 64-bit stack.
    mov rsp, stack_top

    extern kernel_main
    call kernel_main

    ; kernel_main should never return.
    cli
.halt:
    hlt
    jmp .halt
