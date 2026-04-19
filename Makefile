# ============================================================
#  Makefile — SilkOS Build System
#
#  Requires:
#    - i686-elf-gcc  (cross-compiler)  https://wiki.osdev.org/GCC_Cross-Compiler
#    - nasm                             https://nasm.us
#    - grub-mkrescue + xorriso          (for ISO target)
#    - qemu-system-i386                 (for run/run-iso targets)
# ============================================================

CC      := i686-elf-gcc
AS      := nasm
KERNEL  := silkos.bin
ISO     := silkos.iso
ISODIR  := isodir

CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -Wno-unused-parameter
LDFLAGS := -T linker.ld -ffreestanding -O2 -nostdlib -lgcc

OBJS    := boot.o kernel.o

# ── Default target ─────────────────────────────────────────
all: $(KERNEL)

# ── Compile assembly bootloader ────────────────────────────
boot.o: boot.asm
	$(AS) -f elf32 $< -o $@

# ── Compile C kernel ───────────────────────────────────────
kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Link kernel binary ─────────────────────────────────────
$(KERNEL): $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	@echo "  Built: $(KERNEL)"

# ── Build bootable ISO (needs grub-mkrescue + xorriso) ─────
iso: $(KERNEL)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/$(KERNEL)
	@printf 'set timeout=0\nset default=0\n\nmenuentry "SilkOS" {\n    multiboot /boot/$(KERNEL)\n}\n' \
	    > $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR)
	@echo "  Built: $(ISO)"

# ── Run with QEMU (kernel binary, no ISO needed) ───────────
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

# ── Run ISO in QEMU ────────────────────────────────────────
run-iso: iso
	qemu-system-i386 -cdrom $(ISO)

# ── Run with serial debug output ───────────────────────────
debug: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) \
	    -serial stdio \
	    -d int,cpu_reset \
	    --no-reboot

# ── Clean build artifacts ───────────────────────────────────
clean:
	rm -f $(OBJS) $(KERNEL) $(ISO)
	rm -rf $(ISODIR)
	@echo "  Cleaned."

.PHONY: all iso run run-iso debug clean
