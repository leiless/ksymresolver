/*
 * Created 190114 lynnl
 */

#ifndef UTILS_H
#define UTILS_H

#include <sys/systm.h>

/**
 * os_log() is only available on macOS 10.12 or newer
 *  thus os_log do have compatibility issue  use printf instead
 *
 * XNU kernel version of printf() don't recognize some rarely used specifiers
 *  like h, i, j, t  use unrecognized spcifier may raise kernel panic
 *
 * Feel free to print NULL as %s  it checked explicitly by kernel-printf
 *
 * see: xnu/osfmk/kern/printf.c#printf
 */
#define LOG(fmt, ...)        printf(KEXTNAME_S ": " fmt "\n", ##__VA_ARGS__)

#define LOG_ERR(fmt, ...)    LOG("[ERR] " fmt, ##__VA_ARGS__)
#define LOG_BUG(fmt, ...)    LOG("[BUG] " fmt, ##__VA_ARGS__)
#define LOG_OFF(fmt, ...)    (void) (0, ##__VA_ARGS__)
#ifdef DEBUG
#define LOG_DBG(fmt, ...)    LOG("[DBG] " fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)    LOG_OFF(fmt, ##__VA_ARGS__)
#endif

#define panicf(fmt, ...)                \
    panic("\n" fmt "\n%s@%s#L%d\n\n",   \
            ##__VA_ARGS__, __BASE_FILE__, __FUNCTION__, __LINE__)

#ifdef DEBUG
/*
 * NOTE: Do NOT use any multi-nary conditional/logical operator inside assertion
 *       like operators && || ?:  it's extremely EVIL
 *       Separate them  each statement per line
 */
#define kassert(ex) (ex) ? (void) 0 : panicf("Assert `%s' failed", #ex)

/**
 * @ex      the expression
 * @fmt     panic message format
 *
 * Example: kassertf(sz > 0, "Why size %zd nonpositive?", sz);
 */
#define kassertf(ex, fmt, ...) \
    (ex) ? (void) 0 : panicf("Assert `%s' failed: " fmt, #ex, ##__VA_ARGS__)
#else
#define kassert(ex) (ex) ? (void) 0 : LOG_BUG("Assert `%s' failed", #ex)

#define kassertf(ex, fmt, ...) \
    (ex) ? (void) 0 : LOG_BUG("Assert `%s' failed: " fmt, #ex, ##__VA_ARGS__)
#endif

#define kassert_nonnull(ptr) kassert(((void *) ptr) != NULL)

int util_vma_uuid(vm_address_t, uuid_string_t);

#endif /* UTILS_H */

