// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_ARENA_INTERNAL_H
#define BC_ALLOCATORS_ARENA_INTERNAL_H

#include "bc_allocators.h"

#include <stdbool.h>
#include <stddef.h>

/* ===== Arena (internal) ===== */

typedef struct bc_allocators_arena_chunk {
    struct bc_allocators_arena_chunk* next;
    size_t mmap_size;
    size_t capacity;
    size_t used;
    unsigned char* memory_start;
} bc_allocators_arena_chunk_t;

struct bc_allocators_arena {
    bc_allocators_context_t* ctx;
    bc_allocators_arena_chunk_t* first_chunk;
    bc_allocators_arena_chunk_t* current_chunk;
    size_t allocation_count;
    size_t initial_chunk_size;
    size_t max_chunk_size;
    bool growable;
};

#endif /* BC_ALLOCATORS_ARENA_INTERNAL_H */
