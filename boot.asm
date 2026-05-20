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
align 4096
pml4_table: resb 4096
pdp_table:  resb 4096
pd_table:   resb 4096

align 16
stack_bottom: resb 16384       
stack_top:


section .data
gdt64:
    dq 0                                        
.code: equ $ - gdt64                               
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)      
.ds:   equ $ - gdt64                               
    dq (1<<41) | (1<<44) | (1<<47)                 
gdt64_ptr:
    dw $ - gdt64 - 1                               
    dd gdt64                                        


section .text
bits 32
global _start
_start:
    mov esp, stack_top

    mov edi, pml4_table
    xor eax, eax
    mov ecx, (3 * 4096) / 4
    rep stosd

    mov eax, pdp_table
    or  eax, 3
    mov [pml4_table], eax

    mov eax, pd_table
    or  eax, 3
    mov [pdp_table], eax

    xor ecx, ecx
.fill_pd:
    mov eax, 0x200000
    mul ecx                                        
    or  eax, 0x83                                  
    mov dword [pd_table + ecx*8],     eax
    mov dword [pd_table + ecx*8 + 4], 0
    inc ecx
    cmp ecx, 512
    jl  .fill_pd

    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    mov eax, cr0
    or  eax, (1 << 31) | 1
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp  gdt64.code:long_mode_start


bits 64
long_mode_start:
    mov ax, gdt64.ds
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top

    extern kernel_main
    call kernel_main

    cli
.halt:
    hlt
    jmp .halt
