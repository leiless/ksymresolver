/*
 * Created 190114 lynnl
 */
#include <mach-o/loader.h>

#include "utils.h"

/**
 * Extract UUID load command from a Mach-O address
 *
 * @addr    Mach-O starting address
 * @output  UUID string output
 * @return  0 if success  errno o.w.
 */
int util_vma_uuid(vm_address_t addr, uuid_string_t output)
{
    int e = 0;
    uint8_t *p = (void *) addr;
    struct mach_header *h = (struct mach_header *) addr;
    struct load_command *lc;
    uint32_t i;
    uint8_t *u;

    kassert_nonnull(addr);
    kassert_nonnull(output);

    if (h->magic == MH_MAGIC || h->magic == MH_CIGAM) {
        p += sizeof(struct mach_header);
    } else if (h->magic == MH_MAGIC_64 || h->magic == MH_CIGAM_64) {
        p += sizeof(struct mach_header_64);
    } else {
        e = EBADMACHO;
        goto out_bad;
    }

    for (i = 0; i < h->ncmds; i++, p += lc->cmdsize) {
        lc = (struct load_command *) p;
        if (lc->cmd == LC_UUID) {
            u = p + sizeof(*lc);

            (void) snprintf(output, sizeof(uuid_string_t),
                    "%02x%02x%02x%02x-%02x%02x-%02x%02x-"
                    "%02x%02x-%02x%02x%02x%02x%02x%02x",
                    u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
                    u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
            goto out_bad;
        }
    }

    e = ENOENT;
out_bad:
    return e;
}

