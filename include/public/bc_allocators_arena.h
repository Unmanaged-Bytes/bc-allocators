// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_ARENA_H
#define BC_ALLOCATORS_ARENA_H

#include "bc_allocators_context.h"

#include <stdbool.h>
#include <stddef.h>

/* ===== Arena allocator ===== */

/* Recommended arena capacities for typical BC workloads. All sizes are
 * virtual address space reservations backed by demand-paged mmap, so
 * physical memory usage is driven by what the arena actually writes,
 * not by the chosen capacity. */
#define BC_ALLOCATORS_ARENA_SMALL_CAPACITY ((size_t)1 * 1024 * 1024)
#define BC_ALLOCATORS_ARENA_DEFAULT_CAPACITY ((size_t)16 * 1024 * 1024)
#define BC_ALLOCATORS_ARENA_LARGE_CAPACITY ((size_t)256 * 1024 * 1024)

typedef struct bc_allocators_arena bc_allocators_arena_t;

typedef struct bc_allocators_arena_stats {
    size_t capacity;
    size_t used;
    size_t allocation_count;
    size_t chunk_count;
    size_t total_reserved;
} bc_allocators_arena_stats_t;

/* Not thread-safe. Each arena must be used from a single thread
   or protected by external synchronization. */

bool bc_allocators_arena_create(bc_allocators_context_t* ctx, size_t capacity, bc_allocators_arena_t** out_arena);

/* Growable arena: starts with initial_chunk_size, doubles each time a new
   chunk is needed, up to max_chunk_size (0 = no cap). Pointers returned
   by allocate() remain valid for the lifetime of the arena; chunks are
   linked, never relocated. */
bool bc_allocators_arena_create_growable(bc_allocators_context_t* ctx, size_t initial_chunk_size, size_t max_chunk_size,
                                         bc_allocators_arena_t** out_arena);

void bc_allocators_arena_destroy(bc_allocators_arena_t* arena);
// Returns a pointer aligned to the requested alignment parameter.
bool bc_allocators_arena_allocate(bc_allocators_arena_t* arena, size_t size, size_t alignment, void** out_ptr);
bool bc_allocators_arena_copy_string(bc_allocators_arena_t* arena, const char* source, const char** out_copy);
bool bc_allocators_arena_reset(bc_allocators_arena_t* arena);
bool bc_allocators_arena_reset_secure(bc_allocators_arena_t* arena);
bool bc_allocators_arena_release_pages(bc_allocators_arena_t* arena);
bool bc_allocators_arena_get_stats(const bc_allocators_arena_t* arena, bc_allocators_arena_stats_t* out_stats);

#endif /* BC_ALLOCATORS_ARENA_H */
