// SPDX-License-Identifier: MIT

#include "bc_allocators_aligned.h"

#include "bc_core.h"

#include <sys/mman.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

bool bc_allocators_aligned_allocate(bc_allocators_context_t* ctx, size_t size, size_t alignment, void** out_ptr)
{
    BC_UNUSED(ctx);

    *out_ptr = NULL;

    if (size == 0) {
        return false;
    }
    if (alignment < sizeof(void*)) {
        return false;
    }
    if ((alignment & (alignment - 1)) != 0) {
        return false;
    }

    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return false;
    }

    *out_ptr = ptr;
    return true;
}

void bc_allocators_aligned_free(bc_allocators_context_t* ctx, void* ptr)
{
    BC_UNUSED(ctx);
    free(ptr);
}

bool bc_allocators_huge_page_allocate(size_t size, void** out_ptr)
{
    *out_ptr = NULL;

    if (size == 0) {
        return false;
    }

#ifdef MAP_HUGETLB
    void* mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (mapped != MAP_FAILED) {
        *out_ptr = mapped;
        return true;
    }
#endif

    void* fallback = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fallback == MAP_FAILED) {
        return false;
    }

#ifdef MADV_HUGEPAGE
    (void)madvise(fallback, size, MADV_HUGEPAGE);
#endif

    *out_ptr = fallback;
    return true;
}

void bc_allocators_huge_page_free(void* ptr, size_t size)
{
    (void)munmap(ptr, size);
}
