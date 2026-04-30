// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_CONTEXT_H
#define BC_ALLOCATORS_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef struct bc_allocators_context bc_allocators_context_t;

typedef void (*bc_allocators_leak_callback_t)(const char* message, void* user_argument);

typedef struct bc_allocators_context_config {
    size_t max_pool_memory;
    bool tracking_enabled;
    bc_allocators_leak_callback_t leak_callback;
    void* leak_callback_argument;
} bc_allocators_context_config_t;

bool bc_allocators_context_create(const bc_allocators_context_config_t* config, bc_allocators_context_t** out_ctx);
void bc_allocators_context_destroy(bc_allocators_context_t* ctx);

/* ===== Hardware query ===== */

bool bc_allocators_context_page_size(const bc_allocators_context_t* ctx, size_t* out_page_size);
bool bc_allocators_context_cache_line_size(const bc_allocators_context_t* ctx, size_t* out_cache_line_size);

/* ===== Aggregated statistics ===== */

typedef struct bc_allocators_stats {
    size_t pool_alloc_count;
    size_t pool_free_count;
    size_t pool_active_bytes;
    size_t arena_create_count;
    size_t arena_destroy_count;
    size_t arena_active_count;
    size_t arena_total_bytes;
    size_t total_mapped_bytes;
    size_t peak_mapped_bytes;
} bc_allocators_stats_t;

bool bc_allocators_context_get_stats(const bc_allocators_context_t* ctx, bc_allocators_stats_t* out_stats);

/* ===== Safe arithmetic ===== */

bool bc_allocators_compute_alloc_size(const bc_allocators_context_t* ctx, size_t element_size, size_t count, size_t* out_size);

/* ===== Aligned allocation ===== */

/* Aligned allocation. alignment must be power-of-two and >= sizeof(void*).
   Allocated memory must be released via bc_allocators_aligned_free.
   Backed by posix_memalign; ctx is currently unused but reserved for future
   tracking via the context. */
static inline bool bc_allocators_aligned_allocate(bc_allocators_context_t* ctx, size_t size, size_t alignment, void** out_ptr)
{
    (void)ctx;
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

static inline void bc_allocators_aligned_free(bc_allocators_context_t* ctx, void* ptr)
{
    (void)ctx;
    free(ptr);
}

/* Huge page allocation (2 MiB Linux x86-64). Tries MAP_HUGETLB first
   (requires sysctl vm.nr_hugepages pre-configured), falls back to mmap +
   madvise(MADV_HUGEPAGE) if MAP_HUGETLB returns ENOMEM. Returns false on
   complete failure.
   Caller must release via bc_allocators_huge_page_free with the same size.
   Recommended threshold: BC_BUFFER_HUGE_PAGE_THRESHOLD (2 MiB). Below this,
   prefer bc_allocators_aligned_allocate. */
static inline bool bc_allocators_huge_page_allocate(size_t size, void** out_ptr)
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

static inline void bc_allocators_huge_page_free(void* ptr, size_t size)
{
    (void)munmap(ptr, size);
}

#endif /* BC_ALLOCATORS_CONTEXT_H */
