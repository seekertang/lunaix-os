#include <klibc/string.h>
#include <lunaix/boot_generic.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <sys/mm/mempart.h>

/**
 * @brief Reserve memory for kernel bootstrapping initialization
 *
 * @param bhctx
 */
void
boot_begin(struct boot_handoff* bhctx)
{
    bhctx->prepare(bhctx);

    struct boot_mmapent *mmap = bhctx->mem.mmap, *mmapent;
    for (size_t i = 0; i < bhctx->mem.mmap_len; i++) {
        mmapent = &mmap[i];
        size_t size_pg = PN(ROUNDUP(mmapent->size, PG_SIZE));

        if (mmapent->type == BOOT_MMAP_FREE) {
            pmm_mark_chunk_free(PN(mmapent->start), size_pg);
            continue;
        }

        ptr_t pa = PG_ALIGN(mmapent->start);
        for (size_t j = 0; j < size_pg && pa < KERNEL_EXEC;
             j++, pa += PM_PAGE_SIZE) {
            vmm_set_mapping(VMS_SELF, pa, pa, PG_PREM_RW, VMAP_IGNORE);
        }
    }
}

extern u8_t __kexec_boot_end; /* link/linker.ld */

/**
 * @brief Release memory for kernel bootstrapping initialization
 *
 * @param bhctx
 */
void
boot_end(struct boot_handoff* bhctx)
{
    struct boot_mmapent *mmap = bhctx->mem.mmap, *mmapent;
    for (size_t i = 0; i < bhctx->mem.mmap_len; i++) {
        mmapent = &mmap[i];
        size_t size_pg = PN(ROUNDUP(mmapent->size, PG_SIZE));

        if (mmapent->start >= KERNEL_EXEC || mmapent->type == BOOT_MMAP_FREE) {
            continue;
        }

        if (mmapent->type == BOOT_MMAP_RCLM) {
            pmm_mark_chunk_free(PN(mmapent->start), size_pg);
        }

        ptr_t pa = PG_ALIGN(mmapent->start);
        for (size_t j = 0; j < size_pg && pa < KERNEL_EXEC;
             j++, pa += PM_PAGE_SIZE) {
            vmm_del_mapping(VMS_SELF, pa);
        }
    }

    bhctx->release(bhctx);
}

/**
 * @brief Clean up the boot stage code and data
 *
 */
void
boot_cleanup()
{
    // clean up
    for (size_t i = 0; i < (ptr_t)(&__kexec_boot_end); i += PG_SIZE) {
        vmm_del_mapping(VMS_SELF, (ptr_t)i);
        pmm_free_page(KERNEL_PID, (ptr_t)i);
    }
}