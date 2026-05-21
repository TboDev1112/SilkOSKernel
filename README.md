# SilkOS Kernel

A simple bootable kernel for QEMU with a built-in shell.

Run `help` in the shell to see available commands.

## Build and run

```bash
make run
# or
./build run
```

## Passing files into the VM

Files can be supplied in two ways:

**Runtime (QEMU passthrough)** — no kernel rebuild when you change files:

```bash
./build run --files image.ppm app.elf
# or
make run RUN_FILES="image.ppm app.elf"
```

QEMU loads these as Multiboot modules. Use `files` in the shell to list them.

**Build-time embed** — files are baked into `silkos.bin`:

```bash
./build --embed app.elf demo.ppm
# or
make EXTRA_FILES="app.elf demo.ppm"
```

You can combine both: embed some files and pass others at runtime.

## Image rendering

Convert an image to binary PPM, pass it in, then render:

```bash
convert photo.png photo.ppm
./build run --files photo.ppm
```

In the shell:

```
files
render photo.ppm
```

Supports P5 (grayscale) and P6 (RGB) PPM. Images are scaled to the 80×25 text framebuffer using half-block characters.

## ISO

```bash
make iso
make run-iso RUN_FILES="image.ppm"
```
