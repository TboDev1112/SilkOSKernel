#include "silk_fs.h"
#include "kernel.h"

#define SILK_FS_MAX 64

extern const SilkFile silk_embedded[];
extern const size_t   silk_embedded_count;

static SilkFile  file_table[SILK_FS_MAX];
static size_t    file_count;

const SilkFile *silk_files = file_table;
size_t          silk_file_count;

#define MB_MAGIC      0x2BADB002u
#define MB_INFO_MODS  (1u << 3)

typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t reserved;
} multiboot_module_t;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} multiboot_info_t;

static const char *path_basename(const char *path) {
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return base;
}

static void add_file(const char *name, const uint8_t *start, const uint8_t *end) {
    if (file_count >= SILK_FS_MAX || !name || !start || end <= start) return;
    file_table[file_count].name     = name;
    file_table[file_count].data     = start;
    file_table[file_count].data_end = end;
    file_count++;
}

void silk_fs_init(uint64_t mbi_phys) {
    file_count = 0;

    for (size_t i = 0; i < silk_embedded_count && file_count < SILK_FS_MAX; i++)
        add_file(silk_embedded[i].name, silk_embedded[i].data, silk_embedded[i].data_end);

    if (!mbi_phys) {
        silk_file_count = file_count;
        return;
    }

    const multiboot_info_t *info = (const multiboot_info_t *)(uintptr_t)mbi_phys;
    if (!(info->flags & MB_INFO_MODS) || !info->mods_count || !info->mods_addr) {
        silk_file_count = file_count;
        return;
    }

    const multiboot_module_t *mods =
        (const multiboot_module_t *)(uintptr_t)info->mods_addr;

    for (uint32_t i = 0; i < info->mods_count && file_count < SILK_FS_MAX; i++) {
        const char *cmdline = (const char *)(uintptr_t)mods[i].cmdline;
        const char *name = (cmdline && *cmdline) ? path_basename(cmdline) : "module";
        add_file(name,
                 (const uint8_t *)(uintptr_t)mods[i].mod_start,
                 (const uint8_t *)(uintptr_t)mods[i].mod_end);
    }

    silk_file_count = file_count;
}

const SilkFile *silk_file_find(const char *name) {
    for (size_t i = silk_file_count; i > 0; i--) {
        if (kstrcmp(silk_files[i - 1].name, name) == 0)
            return &silk_files[i - 1];
    }
    return (const SilkFile *)0;
}
