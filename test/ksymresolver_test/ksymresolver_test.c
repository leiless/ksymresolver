/*
 * Created 190112 lynnl
 */
#include <mach/mach_types.h>
#include <libkern/libkern.h>
#include "../../ksymresolver/ksymresolver.h"

#ifndef KEXTNAME_S
#define KEXTNAME_S      "ksymresolver_test"
#endif

#define LOG(fmt, ...) printf(KEXTNAME_S ": " fmt "\n", ##__VA_ARGS__)

static int *hz;
static int *tick;
static time_t (*boottime_sec)(void);
/* Exported: xnu/bsd/sys/systm.h#bsd_hostname */
static int (*bsd_hostname)(char *, int, int *);

kern_return_t ksymresolver_test_start(kmod_info_t *ki __unused, void *d __unused)
{
    return KERN_SUCCESS;
}

kern_return_t ksymresolver_test_stop(kmod_info_t *ki __unused, void *d __unused)
{
    hz = resolve_ksymbol("_hz");
    LOG("hz addr: %#018lx\n", (vm_address_t) hz);
    if (hz) LOG("hz: %d", *hz);

    tick = resolve_ksymbol("_tick");
    LOG("tick addr: %#018lx\n", (vm_address_t) tick);
    if (tick) LOG("tick: %d", *tick);

    boottime_sec = resolve_ksymbol("_boottime_sec");
    LOG("boottime_sec addr: %#018lx", (vm_address_t) boottime_sec);
    if (boottime_sec) {
        LOG("boottime_sec: %ld", boottime_sec());
    }

    bsd_hostname = resolve_ksymbol("_bsd_hostname");
    LOG("bsd_hostname addr: %#018lx\n", (vm_address_t) bsd_hostname);
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

__private_extern__ kmod_start_func_t *_realmain = ksymresolver_test_start;
__private_extern__ kmod_stop_func_t *_antimain = ksymresolver_test_stop;

__private_extern__ int _kext_apple_cc = __APPLE_CC__;
#endif

