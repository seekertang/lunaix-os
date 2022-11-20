#include <lunaix/mm/mmap.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#include <lunaix/syscall.h>

void*
mem_map(ptr_t mnt,
        struct llist_header* regions,
        void* addr,
        struct v_file* file,
        u32_t offset,
        size_t length,
        u32_t proct)
{
    if (!length || (length & (PG_SIZE - 1)) || (offset & (PG_SIZE - 1))) {
        __current->k_status = EINVAL;
        return (void*)-1;
    }

    // read_page is not supported
    if (!file->ops->read_page) {
        __current->k_status = ENODEV;
        return (void*)-1;
    }

    ptr_t last_end = USER_START;
    struct mm_region *pos, *n;
    llist_for_each(pos, n, regions, head)
    {
        if (pos->start - last_end >= length && last_end >= addr) {
            goto found;
        }
        last_end = pos->end;
    }

    __current->k_status = ENOMEM;
    return (void*)-1;

found:
    addr = last_end;
    ptr_t end = addr + length;

    struct mm_region* region = region_add(regions, addr, end, proct);
    region->mfile = file;
    region->offset = offset;

    u32_t attr = PG_ALLOW_USER;
    if ((proct & REGION_WRITE)) {
        attr |= PG_WRITE;
    }

    for (u32_t i = 0; i < length; i += PG_SIZE) {
        vmm_set_mapping(mnt, addr + i, 0, attr, 0);
    }

    return addr;
}

void*
mem_unmap(ptr_t mnt, struct llist_header* regions, void* addr, size_t length)
{
    length = ROUNDUP(length, PG_SIZE);
    ptr_t cur_addr = ROUNDDOWN((ptr_t)addr, PG_SIZE);
    struct mm_region *pos, *n;

    llist_for_each(pos, n, regions, head)
    {
        if (pos->start >= cur_addr) {
            break;
        }
    }

    while (&pos->head != regions && cur_addr > pos->start) {
        u32_t l = pos->end - cur_addr;
        pos->end = cur_addr;

        if (l > length) {
            // unmap cause discontinunity in a memory region -  do split
            struct mm_region* region = valloc(sizeof(struct mm_region));
            *region = *pos;
            region->start = cur_addr + length;
            llist_insert_after(&pos->head, &region->head);
            l = length;
        }

        for (size_t i = 0; i < l; i += PG_SIZE) {
            ptr_t pa = vmm_del_mapping(mnt, cur_addr + i);
            if (pa) {
                pmm_free_page(__current->pid, pa);
            }
        }

        n = container_of(pos->head.next, typeof(*pos), head);
        if (pos->end == pos->start) {
            llist_delete(&pos->head);
            vfree(pos);
        }

        pos = n;
        length -= l;
        cur_addr += length;
    }
}

__DEFINE_LXSYSCALL5(void*,
                    mmap,
                    void*,
                    addr,
                    size_t,
                    length,
                    int,
                    proct,
                    int,
                    fd,
                    size_t,
                    offset)
{
    int errno = 0;
    struct v_fd* vfd;
    if ((errno = vfs_getfd(fd, &vfd))) {
        __current->k_status = errno;
        return (void*)-1;
    }

    return mem_map(PD_REFERENCED,
                   &__current->mm.regions,
                   addr,
                   vfd->file,
                   offset,
                   length,
                   proct);
}

__DEFINE_LXSYSCALL2(void, munmap, void*, addr, size_t, length)
{
    return mem_unmap(PD_REFERENCED, &__current->mm.regions, addr, length);
}