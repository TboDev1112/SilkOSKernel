
ARCH          ?= i386
CROSS_COMPILE ?=

CC ?= $(CROSS_COMPILE)gcc
LD := $(CC)

AS      := nasm
KERNEL  := silkos.bin
ISO     := silkos.iso
ISODIR  := isodir

CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -Wno-unused-parameter
LDFLAGS := -T linker.ld -ffreestanding -O2 -nostdlib -lgcc

OBJS    := boot.o kernel.o

ifeq ($(ARCH),i386)
  CFLAGS  += -m32
  LDFLAGS += -m32
  CFLAGS  += -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
endif

CFLAGS  += -fno-stack-protector -fno-pie -fno-pic
LDFLAGS += -no-pie

all: $(KERNEL)

boot.o: boot.asm
	$(AS) -f elf32 $< -o $@

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "  Built: $(KERNEL)"

iso: $(KERNEL)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/$(KERNEL)
	@printf 'set timeout=0\nset default=0\n\nmenuentry "SilkOS" {\n    multiboot /boot/$(KERNEL)\n}\n' \
	    > $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR)
	@echo "  Built: $(ISO)"

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) \
	    -audiodev pa,id=snd -machine pcspk-audiodev=snd

run-iso: iso
	qemu-system-i386 -cdrom $(ISO) \
	    -audiodev pa,id=snd -machine pcspk-audiodev=snd

debug: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) \
	    -serial stdio \
	    -d int,cpu_reset \
	    --no-reboot

clean:
	rm -f $(OBJS) $(KERNEL) $(ISO)
	rm -rf $(ISODIR)
	@echo "  Cleaned."

.PHONY: all iso run run-iso debug clean
