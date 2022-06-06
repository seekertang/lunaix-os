#include <arch/x86/interrupts.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/mm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/common.h>
#include <lunaix/syslog.h>
#include <lunaix/status.h>
#include <lunaix/sched.h>

static void kprintf(const char* fmt, ...) { va_list args; va_start(args, fmt); __kprintf("PFAULT", fmt, args); va_end(args); }

extern void __print_panic_msg(const char* msg, const isr_param* param);

void
intr_routine_page_fault (const isr_param* param) 
{
    uintptr_t ptr = cpu_rcr2();
    if (!ptr) {
        goto segv_term;
    }

    struct mm_region* hit_region = region_get(__current, ptr);

    if (!hit_region) {
        // Into the void...
        goto segv_term;
    }

    // if (param->eip == ptr && !(hit_region->attr & REGION_EXEC)) {
    //     goto segv_term;
    // }

    x86_pte_t* pte = CURPROC_PTE(ptr >> 12);
    if (*pte & PG_PRESENT) {
        if ((hit_region->attr & REGION_PERM_MASK) == (REGION_RSHARED | REGION_READ)) {
            // normal page fault, do COW
                uintptr_t pa = (uintptr_t)vmm_dup_page(__current->pid, PG_ENTRY_ADDR(*pte));
                pmm_free_page(__current->pid, *pte & ~0xFFF);
                *pte = (*pte & 0xFFF) | pa | PG_WRITE;
                return;
        }
        // impossible cases or accessing privileged page
        goto segv_term;
    }

    if (!(*pte)) {
        // Invalid location
        goto segv_term;
    }
    uintptr_t loc = *pte & ~0xfff;
    // a writable page, not present, pte attr is not null and no indication of cached page -> a new page need to be alloc
    if ((hit_region->attr & REGION_WRITE) && (*pte & 0xfff) && !loc) {
        uintptr_t pa = pmm_alloc_page(__current->pid, 0);
        *pte = *pte | pa | PG_PRESENT;
        return;
    }
    // page not present, bring it from disk or somewhere else
    __print_panic_msg("WIP page fault route", param);
    while (1);

segv_term:
    kprintf(KERROR "(pid: %d) Segmentation fault on %p (%p:%p)\n", __current->pid, ptr, param->cs, param->eip);
    terminate_proc(LXSEGFAULT);
    // should not reach
}