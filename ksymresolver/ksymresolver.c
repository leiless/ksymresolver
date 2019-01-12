/*
 * Created 180909 lynnl
 */
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <libkern/libkern.h>
#include <vm/vm_kern.h>

/* Exported: xnu/osfmk/mach/i386/vm_param.h */
#define VM_MIN_KERNEL_ADDRESS           ((vm_offset_t) 0xffffff8000000000UL)
#define VM_MIN_KERNEL_AND_KEXT_ADDRESS  (VM_MIN_KERNEL_ADDRESS - 0x80000000ULL)

#define KERN_HIB_BASE   ((vm_offset_t) 0xffffff8000100000ULL)
#define KERN_TEXT_BASE  ((vm_offset_t) 0xffffff8000200000ULL)

#define LOG(fmt, ...) printf(KEXTNAME_S ": " fmt "\n", ##__VA_ARGS__)

/* Exported: xnu/bsd/sys/systm.h#bsd_hostname */
static int (*bsd_hostname)(char *, int, int *);
static int *hz;

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

/**
 * Get value of global variable vm_kernel_slide(since 10.11)
 * @return      0 if failed to get
 * see: xnu/osfmk/vm/vm_kern.c#vm_kernel_unslide_or_perm_external
 */
static vm_offset_t get_vm_kernel_slide(void)
{
    static uint16_t i = 4096;
    static vm_offset_t fake = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
    static vm_offset_t slide = 0L;

    if (get_vm_kernel_addrperm_ext() == 0L) goto out_exit;
    if (i == 0 || slide != 0L) goto out_exit;

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

    if (mh->magic != MH_MAGIC_64 || mh->filetype != MH_EXECUTE) {
        LOG("mach header illegal - mh: %p mag: %#010x type: %#010x",
                mh, mh->magic, mh->filetype);
        goto out_done;
    }

    linkedit = find_seg64(mh, SEG_LINKEDIT);
    if (!linkedit) {
        LOG("cannot find LINKEDIT seg  mh: %p", mh);
        goto out_done;
    }
    linkedit_base = linkedit->vmaddr - linkedit->fileoff;

    symtab = (struct symtab_command *) find_lc(mh, LC_SYMTAB);
    if (!symtab) {
        LOG("cannot find SYMTAB cmd  mh: %p", mh);
        goto out_done;
    }

    if (symtab->nsyms == 0 || symtab->strsize == 0) {
        LOG("SYMTAB symbol size invalid  nsyms: %u strsize: %u",
                symtab->nsyms, symtab->strsize);
        goto out_done;
    }

    if (linkedit->fileoff > symtab->stroff ||
            linkedit->fileoff > symtab->symoff) {
        LOG("LINKEDIT fileoff(%#llx) out of range  stroff: %u symoff: %u",
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
    vm_offset_t vm_kernel_addrperm_ext1 = get_vm_kernel_addrperm_ext();
    LOG("vm_kernel_addrperm_ext: %#018lx\n", vm_kernel_addrperm_ext1);

    vm_offset_t vm_kern_slide = get_vm_kernel_slide();
    LOG("vm_kernel_slide:        %#018lx\n", vm_kern_slide);

    vm_address_t hib_base = KERN_HIB_BASE + vm_kern_slide;
    vm_address_t kern_base = KERN_TEXT_BASE + vm_kern_slide;

    LOG("HIB text base:          %#018lx\n", hib_base);
    LOG("kernel text base:       %#018lx\n", kern_base);

    struct mach_header_64 *mh = (struct mach_header_64 *) kern_base;
    LOG("magic:                  %#010x\n", mh->magic);
    LOG("cputype:                %#010x\n", mh->cputype);
    LOG("cpusubtype:             %#010x\n", mh->cpusubtype);
    LOG("filetype:               %#010x\n", mh->filetype);
    LOG("ncmds:                  %#010x\n", mh->ncmds);
    LOG("sizeofcmds:             %#010x\n", mh->sizeofcmds);
    LOG("flags:                  %#010x\n", mh->flags);
    LOG("reserved:               %#010x\n", mh->reserved);

    bsd_hostname = resolve_ksymbol2(mh, "_bsd_hostname");
    LOG("bsd_hostname(): %#018lx\n", (vm_address_t) bsd_hostname);
    if (bsd_hostname) {
        char buf[64];
        int outlen;
        int e = bsd_hostname(buf, sizeof(buf), &outlen);
        if (e == 0) {
            LOG("hostname: %s len: %d\n", buf, outlen);
        } else {
            LOG("bsd_hostname() failed  errno: %d\n", e);
        }
    }

    hz = resolve_ksymbol2(mh, "_hz");
    LOG("hz: %#018lx\n", (vm_address_t) hz);
    if (hz) LOG("hz: %d", *hz);

    LOG("");    /* Add a separator  we fail so you don't need to kextunload */
    return KERN_FAILURE;
}

kern_return_t ksymresolver_stop(kmod_info_t *ki __unused, void *d __unused)
{
    LOG("unloaded");
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

