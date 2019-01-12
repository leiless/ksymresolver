/*
 * Created 180909 lynnl
 */
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <libkern/libkern.h>
#include <vm/vm_kern.h>
#include "ksymresolver.h"

/* Exported: xnu/osfmk/mach/i386/vm_param.h */
#define VM_MIN_KERNEL_ADDRESS           ((vm_offset_t) 0xffffff8000000000UL)
#define VM_MIN_KERNEL_AND_KEXT_ADDRESS  (VM_MIN_KERNEL_ADDRESS - 0x80000000ULL)

#define KERN_HIB_BASE   ((vm_offset_t) 0xffffff8000100000ULL)
#define KERN_TEXT_BASE  ((vm_offset_t) 0xffffff8000200000ULL)

#ifndef __kext_makefile__
#define KEXTNAME_S      "ksymresolver"
#define KEXTVERSION_S   "0000.00.01"
#define KEXTBUILD_S     "0"
#define BUNDLEID_S      "cn.junkman.kext." KEXTNAME_S
#define __TZ__          "+0800"
#endif

#define LOG(fmt, ...)       printf(KEXTNAME_S ": " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   LOG("[ERR] " fmt, ##__VA_ARGS__)
#ifdef DEBUG
#define LOG_DBG(fmt, ...)   LOG("[DBG] " fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)   (void) (0, ##__VA_ARGS__)
#endif

/**
 * Get value of global variable vm_kernel_addrperm_ext(since 10.11)
 * @return      0 if failed to get
 * see: xnu/osfmk/vm/vm_kern.c#vm_kernel_addrperm_external
 */
static vm_offset_t get_vm_kernel_addrperm_ext(void)
{
    static vm_offset_t addrperm_ext = 0L;
    if (addrperm_ext != 0L) goto out_exit;
    vm_kernel_addrperm_external(VM_MIN_KERNEL_AND_KEXT_ADDRESS, &addrperm_ext);
    addrperm_ext -= VM_MIN_KERNEL_AND_KEXT_ADDRESS;
out_exit:
    return addrperm_ext;
}

#define MAX_SLIDE_STEP      4096

/**
 * Get value of global variable vm_kernel_slide(since 10.11)
 * @return      0 if failed to get
 * see: xnu/osfmk/vm/vm_kern.c#vm_kernel_unslide_or_perm_external
 */
static vm_offset_t get_vm_kernel_slide(void)
{
    static uint16_t i = MAX_SLIDE_STEP;
    static vm_offset_t fake = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
    static vm_offset_t slide = 0L;

    if (get_vm_kernel_addrperm_ext() == 0L) goto out_exit;
    if (slide != 0L || i == 0) goto out_exit;

    while (--i) {
        vm_kernel_unslide_or_perm_external(fake, &slide);
        /* We assume unslide address is unique to those two */
        if (slide != fake && slide != fake + get_vm_kernel_addrperm_ext()) {
            slide = fake - slide;
            break;
        }
        fake += 0x100000L;
        slide = 0L;
    }

out_exit:
    return slide;
}

static struct segment_command_64 *find_seg64(
                struct mach_header_64 *mh,
                const char *name)
{
    uint32_t i;
    struct load_command *lc;
    struct segment_command_64 *seg;

    lc = (struct load_command *) ((uint8_t *) mh + sizeof(*mh));
    for (i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_64) {
            seg = (struct segment_command_64 *) lc;
            if (!strcmp(seg->segname, name)) return seg;
        }

        lc = (struct load_command *) ((uint8_t *) lc + lc->cmdsize);
    }

    return NULL;
}

static struct load_command *find_lc(
        struct mach_header_64 *mh,
        uint32_t cmd)
{
    uint32_t i;
    struct load_command *lc;

    /* First LC begins straight after the mach header */
    lc = (struct load_command *) ((uint8_t *) mh + sizeof(*mh));
    for (i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == cmd) return lc;
        lc = (struct load_command *) ((uint8_t *) lc + lc->cmdsize);
    }

    return NULL;
}

/**
 * Find a symbol from symtab
 * keywords: kernel_nlist_t  struct nlist_64
 * see: xnu/libkern/c++/OSKext.cpp#slidePrelinkedExecutable
 */
static void *resolve_ksymbol2(struct mach_header_64 *mh, const char *name)
{
    struct segment_command_64 *linkedit;
    vm_address_t linkedit_base;
    struct symtab_command *symtab;
    char *strtab;
    struct nlist_64 *nl;
    uint32_t i;
    char *str;
    void *addr = NULL;

    if ((mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) || mh->filetype != MH_EXECUTE) {
        LOG_ERR("bad mach header  mh: %p mag: %#010x type: %#010x",
                    mh, mh->magic, mh->filetype);
        goto out_done;
    }

    linkedit = find_seg64(mh, SEG_LINKEDIT);
    if (linkedit == NULL) {
        LOG_ERR("cannot find SEG_LINKEDIT  mh: %p", mh);
        goto out_done;
    }
    linkedit_base = linkedit->vmaddr - linkedit->fileoff;

    symtab = (struct symtab_command *) find_lc(mh, LC_SYMTAB);
    if (symtab == NULL) {
        LOG_ERR("cannot find LC_SYMTAB  mh: %p", mh);
        goto out_done;
    }

    if (symtab->nsyms == 0 || symtab->strsize == 0) {
        LOG_ERR("SYMTAB symbol size invalid  nsyms: %u strsize: %u",
                symtab->nsyms, symtab->strsize);
        goto out_done;
    }

    if (linkedit->fileoff > symtab->stroff ||
            linkedit->fileoff > symtab->symoff) {
        LOG_ERR("LINKEDIT fileoff(%#llx) out of range  stroff: %u symoff: %u",
                linkedit->fileoff, symtab->stroff, symtab->symoff);
        goto out_done;
    }

    strtab = (char *) (linkedit_base + symtab->stroff);
    nl = (struct nlist_64 *) (linkedit_base + symtab->symoff);
    for (i = 0; i < symtab->nsyms; i++) {
        /* Skip debugging symbols */
        if (nl[i].n_type & N_STAB) continue;

        str = (char *) strtab + nl[i].n_un.n_strx;
        if (!strcmp(str, name)) {
            addr = (void *) nl[i].n_value;
            break;
        }
    }

out_done:
    return addr;
}

/**
 * Resolve a kernel symbol address
 * @param name          symbol name(must begin with _)
 * @return              NULL if not found
 */
void *resolve_ksymbol(const char * __nonnull name)
{
    static struct mach_header_64 *mh = NULL;

    if (mh == NULL) {
        mh = (struct mach_header_64 *) KERN_TEXT_BASE + get_vm_kernel_slide();
    }

    return resolve_ksymbol2(mh, name);
}

kern_return_t ksymresolver_start(kmod_info_t *ki __unused, void *d __unused)
{
    vm_offset_t vm_kern_ap_ext;
    vm_offset_t vm_kern_slide;
    vm_address_t hib_base;
    vm_address_t kern_base;
    struct mach_header_64 *mh;

    LOG("loaded  (version: %s build: %s ts: %s %s%s uuid: %s)",
        KEXTVERSION_S, KEXTBUILD_S, __DATE__, __TIME__, __TZ__, "");

    vm_kern_ap_ext = get_vm_kernel_addrperm_ext();
    LOG_DBG("vm_kernel_addrperm_ext: %#018lx\n", vm_kern_ap_ext);

    vm_kern_slide = get_vm_kernel_slide();
    LOG_DBG("vm_kernel_slide:        %#018lx\n", vm_kern_slide);

    hib_base = KERN_HIB_BASE + vm_kern_slide;
    kern_base = KERN_TEXT_BASE + vm_kern_slide;

    LOG_DBG("HIB text base:          %#018lx\n", hib_base);
    LOG_DBG("kernel text base:       %#018lx\n", kern_base);

    mh = (struct mach_header_64 *) kern_base;
    LOG_DBG("magic:                  %#010x\n", mh->magic);
    LOG_DBG("cputype:                %#010x\n", mh->cputype);
    LOG_DBG("cpusubtype:             %#010x\n", mh->cpusubtype);
    LOG_DBG("filetype:               %#010x\n", mh->filetype);
    LOG_DBG("ncmds:                  %#010x\n", mh->ncmds);
    LOG_DBG("sizeofcmds:             %#010x\n", mh->sizeofcmds);
    LOG_DBG("flags:                  %#010x\n", mh->flags);
    LOG_DBG("reserved:               %#010x\n", mh->reserved);

    return KERN_SUCCESS;
}

kern_return_t ksymresolver_stop(kmod_info_t *ki __unused, void *d __unused)
{
    LOG("unloaded  (version: %s build: %s ts: %s %s%s uuid: %s)",
        KEXTVERSION_S, KEXTBUILD_S, __DATE__, __TIME__, __TZ__, "");
    return KERN_SUCCESS;
}

#ifdef __kext_makefile__
extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);

/* Will expand name if it's a macro */
#define KMOD_EXPLICIT_DECL2(name, ver, start, stop) \
    __attribute__((visibility("default")))          \
        KMOD_EXPLICIT_DECL(name, ver, start, stop)

KMOD_EXPLICIT_DECL2(BUNDLEID, KEXTBUILD_S, _start, _stop)

__private_extern__ kmod_start_func_t *_realmain = ksymresolver_start;
__private_extern__ kmod_stop_func_t *_antimain = ksymresolver_stop;

__private_extern__ int _kext_apple_cc = __APPLE_CC__;
#endif

