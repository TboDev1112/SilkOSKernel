
ARCH          ?= x86_64
CROSS_COMPILE ?=

CC      ?= $(CROSS_COMPILE)gcc
LD      := $(CC)
AS      := nasm
OBJCOPY := objcopy

KERNEL  := silkos.bin
ISO     := silkos.iso
ISODIR  := isodir

# ── Extra files to embed ────────────────────────────────────────────────────
# Pass on the command line:
#   make EXTRA_FILES="myapp.elf data.bin"
#   ./build --files myapp.elf data.bin
#
# Each file is converted to a linkable .o via objcopy, and a C table
# (files_table.c) is generated so the kernel can find them by name.
EXTRA_FILES ?=

# ── Compiler / linker flags ─────────────────────────────────────────────────
CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -Wno-unused-parameter
LDFLAGS := -T linker.ld -ffreestanding -O2 -nostdlib -lgcc

ifeq ($(ARCH),x86_64)
  CFLAGS  += -m64 -mno-red-zone
  LDFLAGS += -m64
  CFLAGS  += -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
endif

CFLAGS  += -fno-stack-protector -fno-pie -fno-pic
LDFLAGS += -no-pie

# ── Object lists ────────────────────────────────────────────────────────────
BASE_OBJS := boot.o kernel.o files_table.o

# One .embed.o per extra file (target name includes any directory prefix)
FILE_OBJS := $(addsuffix .embed.o,$(EXTRA_FILES))

OBJS := $(BASE_OBJS) $(FILE_OBJS)

# ── Default target ──────────────────────────────────────────────────────────
all: $(KERNEL)

# ── Compile rules ───────────────────────────────────────────────────────────
boot.o: boot.asm
	$(AS) -f elf64 $< -o $@

kernel.o: kernel.c silk_fs.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── Embedded-file rules ─────────────────────────────────────────────────────

# Stamp file: records the current EXTRA_FILES list so files_table.c is
# regenerated whenever the list changes between make invocations.
FORCE:
.PHONY: FORCE

.files_list: FORCE
	@echo "$(EXTRA_FILES)" | cmp -s - $@ 2>/dev/null || \
	 echo "$(EXTRA_FILES)" > $@

# Convert each raw file into a linkable ELF64 object.
# objcopy creates three symbols per file:
#   _binary_<mangled>_start   — pointer to first byte
#   _binary_<mangled>_end     — pointer past last byte
#   _binary_<mangled>_size    — byte count (as an absolute symbol)
# where <mangled> is the filename with every non-alphanumeric char → '_'.
#
# The rule below handles files in subdirectories:
#   apps/myapp.elf  →  apps/myapp.elf.embed.o
define EMBED_RULE
$(1).embed.o: $(1)
	@echo "  EMBED   $$<"
	$(OBJCOPY) -I binary \
		-O elf64-x86-64 \
		-B i386:x86-64 \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		$$< $$@
endef

$(foreach f,$(EXTRA_FILES),$(eval $(call EMBED_RULE,$(f))))

# Generate the C file that exposes all embedded files to the kernel via
# the silk_files[] table declared in silk_fs.h.
files_table.c: .files_list $(EXTRA_FILES)
	@echo "  GEN     $@"
	@{ \
	  printf '#include "silk_fs.h"\n\n'; \
	  for f in $(EXTRA_FILES); do \
	    sym=$$(printf '%s' "$$f" | tr -c 'a-zA-Z0-9' '_'); \
	    printf 'extern const uint8_t _binary_%s_start[];\n' "$$sym"; \
	    printf 'extern const uint8_t _binary_%s_end[];\n'   "$$sym"; \
	  done; \
	  n=0; for f in $(EXTRA_FILES); do n=$$((n+1)); done; \
	  if [ "$$n" -eq 0 ]; then \
	    printf '\n/* No files embedded — rebuild with EXTRA_FILES="..." */\n'; \
	    printf 'const SilkFile silk_files[1] = { { 0, 0, 0 } };\n'; \
	    printf 'const size_t   silk_file_count = 0;\n'; \
	  else \
	    printf '\nconst SilkFile silk_files[] = {\n'; \
	    for f in $(EXTRA_FILES); do \
	      sym=$$(printf '%s' "$$f" | tr -c 'a-zA-Z0-9' '_'); \
	      name=$$(basename "$$f"); \
	      printf '    { "%s", _binary_%s_start, _binary_%s_end },\n' \
	        "$$name" "$$sym" "$$sym"; \
	    done; \
	    printf '};\n\n'; \
	    printf 'const size_t silk_file_count =\n'; \
	    printf '    sizeof(silk_files) / sizeof(silk_files[0]);\n'; \
	  fi; \
	} > $@

files_table.o: files_table.c silk_fs.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── Link ────────────────────────────────────────────────────────────────────
$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@.elf64
	$(OBJCOPY) -I elf64-x86-64 -O elf32-i386 $@.elf64 $@
	@rm -f $@.elf64
	@echo "  Built:  $(KERNEL)  (embedded files: $(if $(EXTRA_FILES),$(EXTRA_FILES),none))"

# ── ISO ─────────────────────────────────────────────────────────────────────
iso: $(KERNEL)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/$(KERNEL)
	@printf 'set timeout=0\nset default=0\n\nmenuentry "SilkOS" {\n    multiboot /boot/$(KERNEL)\n}\n' \
	    > $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR)
	@echo "  Built:  $(ISO)"

# ── Run / debug ─────────────────────────────────────────────────────────────
run: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) \
	    -audiodev pa,id=snd -machine pcspk-audiodev=snd

run-iso: iso
	qemu-system-x86_64 -cdrom $(ISO) \
	    -audiodev pa,id=snd -machine pcspk-audiodev=snd

debug: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) \
	    -serial stdio \
	    -d int,cpu_reset \
	    --no-reboot

# ── Clean ───────────────────────────────────────────────────────────────────
clean:
	rm -f $(BASE_OBJS) $(KERNEL) $(KERNEL).elf64 $(ISO)
	rm -f files_table.c .files_list
	rm -f *.embed.o
	rm -rf $(ISODIR)
	@echo "  Cleaned."

.PHONY: all iso run run-iso debug clean

