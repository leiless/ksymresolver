/*
 * Created 190112 lynnl
 */
#ifndef KSYMRESOLVER_H
#define KSYMRESOLVER_H

#ifndef __kext_makefile__
#define KEXTNAME_S              "ksymresolver"
#define KEXTVERSION_S           "0000.00.01"
#define KEXTBUILD_S             "0"
#define BUNDLEID_S              "cn.junkman.kext." KEXTNAME_S
#define __TZ__                  "+0800"
#endif

/* Exported: xnu/osfmk/mach/i386/vm_param.h */
#ifndef VM_MIN_KERNEL_ADDRESS
#define VM_MIN_KERNEL_ADDRESS           ((vm_offset_t) 0xffffff8000000000UL)
#endif
#ifndef VM_MIN_KERNEL_AND_KEXT_ADDRESS
#define VM_MIN_KERNEL_AND_KEXT_ADDRESS  (VM_MIN_KERNEL_ADDRESS - 0x80000000ULL)
#endif

#define KERN_HIB_BASE                   ((vm_offset_t) 0xffffff8000100000ULL)
#define KERN_TEXT_BASE                  ((vm_offset_t) 0xffffff8000200000ULL)

/**
 * Resolve a kernel symbol address
 * @param name          symbol name(must begin with _)
 * @return              NULL if not found
 */
void *resolve_ksymbol(const char *);

#endif  /* KSYMRESOLVER_H */

