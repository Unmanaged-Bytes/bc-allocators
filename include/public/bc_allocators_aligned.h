// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_ALIGNED_H
#define BC_ALLOCATORS_ALIGNED_H

#include "bc_allocators_context.h"

#include <stdbool.h>
#include <stddef.h>

/* Aligned allocation. alignment must be power-of-two and >= sizeof(void*).
   Allocated memory must be released via bc_allocators_aligned_free.
   Backed by posix_memalign; ctx is currently unused but reserved for future
   tracking via the context. */
bool bc_allocators_aligned_allocate(bc_allocators_context_t* ctx, size_t size, size_t alignment, void** out_ptr);
void bc_allocators_aligned_free(bc_allocators_context_t* ctx, void* ptr);

/* Huge page allocation (2 MiB Linux x86-64). Tries MAP_HUGETLB first
   (requires sysctl vm.nr_hugepages pre-configured), falls back to mmap +
   madvise(MADV_HUGEPAGE) if MAP_HUGETLB returns ENOMEM. Returns false on
   complete failure.
   Caller must release via bc_allocators_huge_page_free with the same size.
   Recommended threshold: BC_BUFFER_HUGE_PAGE_THRESHOLD (2 MiB). Below this,
   prefer bc_allocators_aligned_allocate. */
bool bc_allocators_huge_page_allocate(size_t size, void** out_ptr);
void bc_allocators_huge_page_free(void* ptr, size_t size);

#endif /* BC_ALLOCATORS_ALIGNED_H */
