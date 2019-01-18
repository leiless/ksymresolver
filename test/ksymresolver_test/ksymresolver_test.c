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
static boolean_t *doprnt_hide_pointers;

kern_return_t ksymresolver_test_start(kmod_info_t *ki __unused, void *d __unused)
{
    doprnt_hide_pointers = resolve_ksymbol("_doprnt_hide_pointers");
    if (doprnt_hide_pointers != NULL) {
        LOG("doprnt_hide_pointers: %d", *doprnt_hide_pointers);
        /*
         * Set to FALSE causes __doprnt print real value of pointer instead of <ptr>
         * see: xnu/osfmk/kern/printf.c#__doprnt
         */
        *doprnt_hide_pointers = FALSE;
    }
    LOG("&doprnt_hide_pointers: %p", doprnt_hide_pointers);

    hz = resolve_ksymbol("_hz");
    LOG("&hz: %p", hz);
    if (hz) LOG("hz: %d", *hz);

    tick = resolve_ksymbol("_tick");
    LOG("&tick: %p", tick);
    if (tick) LOG("tick: %d", *tick);

    boottime_sec = resolve_ksymbol("_boottime_sec");
    LOG("boottime_sec: %p", boottime_sec);
    if (boottime_sec) LOG("boottime_sec: %ld", boottime_sec());

    bsd_hostname = resolve_ksymbol("_bsd_hostname");
    LOG("bsd_hostname: %p", bsd_hostname);
    if (bsd_hostname) {
        char buf[64];
        int outlen;
        int e = bsd_hostname(buf, sizeof(buf), &outlen);
        if (e == 0) {
            LOG("hostname: %s len: %d", buf, outlen);
        } else {
            LOG("bsd_hostname() failed  errno: %d", e);
        }
    }

    LOG("loaded");
    return KERN_SUCCESS;
}

kern_return_t ksymresolver_test_stop(kmod_info_t *ki __unused, void *d __unused)
{
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

