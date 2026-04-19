// SPDX-License-Identifier: MIT

#include "bc_allocators_platform_internal.h"

#include "bc_core.h"

#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool bc_allocators_platform_map(size_t size, void** out_pointer)
{
    void* mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mapped_memory == MAP_FAILED) { /* GCOVR_EXCL_BR_LINE -- OS-level mmap failure */
        return false;                  /* GCOVR_EXCL_LINE -- OS-level mmap failure */
    }

    *out_pointer = mapped_memory;
    return true;
}

bool bc_allocators_platform_unmap(void* pointer, size_t size)
{
    if (munmap(pointer, size) != 0) { /* GCOVR_EXCL_BR_LINE -- OS-level munmap failure */
        return false;                 /* GCOVR_EXCL_LINE -- OS-level munmap failure */
    }

    return true;
}

bool bc_allocators_platform_get_page_size(size_t* out_page_size)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) { /* GCOVR_EXCL_BR_LINE -- sysconf never fails on Linux */
        return false;     /* GCOVR_EXCL_LINE -- sysconf never fails on Linux */
    }
    *out_page_size = (size_t)page_size;
    return true;
}

bool bc_allocators_platform_get_cache_line_size(size_t* out_cache_line_size)
{
    if (bc_core_cache_line_size(out_cache_line_size)) {
        return true;
    }

    /* GCOVR_EXCL_START -- sysconf cache line query never fails on Linux */
    *out_cache_line_size = 64;
    return true;
    /* GCOVR_EXCL_STOP */
}

int bc_allocators_platform_advise(void* addr, size_t len, int advice)
{
    return madvise(addr, len, advice);
}
