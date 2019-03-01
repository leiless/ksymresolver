/*
 * Created 180909 lynnl
 */
#include <sys/systm.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <libkern/version.h>
#include <vm/vm_kern.h>

#include "ksymresolver.h"
#include "utils.h"

#ifndef __TS__
#define __TS__      "????/??/?? ??:??:??+????"
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

    kassert_nonnull(mh);
    kassert_nonnull(name);

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
 * @param name          symbol name(should begin with _)
 * @return              NULL if not found
 */
void * __nullable resolve_ksymbol(const char * __nonnull name)
{
    static volatile struct mach_header_64 *mh = NULL;

    if (mh == NULL) {
        mh = (struct mach_header_64 *) (KERN_TEXT_BASE + get_vm_kernel_slide());
    }

    return resolve_ksymbol2((struct mach_header_64 *) mh, name);
}

kern_return_t ksymresolver_start(kmod_info_t *ki, void *d __unused)
{
    vm_offset_t vm_kern_ap_ext;
    vm_offset_t vm_kern_slide;
    vm_address_t hib_base;
    vm_address_t kern_base;
    struct mach_header_64 *mh;
    uuid_string_t uuid;
    int e;

    LOG("%s", version);     /* Print darwin kernel version */

    e = util_vma_uuid(ki->address, uuid);
    if (e) {
        LOG_ERR("util_vma_uuid() failed  errno: %d", e);
        goto out_fail;
    }

    vm_kern_ap_ext = get_vm_kernel_addrperm_ext();
    if (vm_kern_ap_ext == 0) {
        LOG_ERR("get_vm_kernel_addrperm_ext() failed");
        goto out_fail;
    }
    LOG_DBG("vm_kernel_addrperm_ext: %#018lx", vm_kern_ap_ext);

    vm_kern_slide = get_vm_kernel_slide();
    if (vm_kern_slide == 0) {
        LOG_ERR("get_vm_kernel_slide() failed");
        goto out_fail;
    }
    LOG_DBG("vm_kernel_slide:        %#018lx", vm_kern_slide);

    hib_base = KERN_HIB_BASE + vm_kern_slide;
    kern_base = KERN_TEXT_BASE + vm_kern_slide;

    LOG_DBG("HIB text base:          %#018lx", hib_base);
    LOG_DBG("kernel text base:       %#018lx", kern_base);

    mh = (struct mach_header_64 *) kern_base;
    LOG_DBG("magic:                  %#010x", mh->magic);
    LOG_DBG("cputype:                %#010x", mh->cputype);
    LOG_DBG("cpusubtype:             %#010x", mh->cpusubtype);
    LOG_DBG("filetype:               %#010x", mh->filetype);
    LOG_DBG("ncmds:                  %#010x", mh->ncmds);
    LOG_DBG("sizeofcmds:             %#010x", mh->sizeofcmds);
    LOG_DBG("flags:                  %#010x", mh->flags);
    LOG_DBG("reserved:               %#010x", mh->reserved);

    LOG("loaded  (version: %s build: %s ts: %s uuid: %s)",
        KEXTVERSION_S, KEXTBUILD_S, __TS__, uuid);

    return KERN_SUCCESS;
out_fail:
    return KERN_FAILURE;
}

kern_return_t ksymresolver_stop(kmod_info_t *ki, void *d __unused)
{
    int e;
    uuid_string_t uuid;

    e = util_vma_uuid(ki->address, uuid);
    if (e) LOG_ERR("util_vma_uuid() failed  errno: %d", e);

    LOG("unloaded  (version: %s build: %s ts: %s uuid: %s)",
        KEXTVERSION_S, KEXTBUILD_S, __TS__, uuid);

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

