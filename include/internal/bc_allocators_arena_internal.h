// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_ARENA_INTERNAL_H
#define BC_ALLOCATORS_ARENA_INTERNAL_H

#include "bc_allocators.h"

#include <stddef.h>

/* ===== Arena (internal) ===== */

struct bc_allocators_arena {
    bc_allocators_context_t* ctx;
    size_t total_mmap_size;
    size_t capacity;
    size_t used;
    size_t allocation_count;
    unsigned char* memory_start;
};

#endif /* BC_ALLOCATORS_ARENA_INTERNAL_H */
