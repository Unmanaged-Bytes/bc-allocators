// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_CONTEXT_INTERNAL_H
#define BC_ALLOCATORS_CONTEXT_INTERNAL_H

#include "bc_allocators.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ===== Constants ===== */

#define BC_ALLOCATORS_NUM_CLASSES 19
#define BC_ALLOCATORS_MAX_CLASS_SIZE (4 * 1024 * 1024)
#define BC_ALLOCATORS_DEFAULT_MAX ((size_t)1024 * 1024 * 1024)

#define BC_ALLOCATORS_PAGE_CLASS_UNASSIGNED ((uint8_t)0xFF)
#define BC_ALLOCATORS_PAGE_CLASS_LARGE ((uint8_t)0xFE)
#define BC_ALLOCATORS_LARGE_META_SIZE ((size_t)16)

/* Magazine pointer cache: 14 slots × 8B + mag_count(8B) + head(8B) = 128B = 2 CL */
#define BC_ALLOCATORS_MAG_SIZE 14

#define BC_ALLOCATORS_ARENA_CACHE_SIZE 4

typedef struct {
    void* block;
    size_t mmap_size;
} bc_allocators_arena_cache_entry_t;

/* ===== Size classes (global, shared between all contexts) ===== */

extern const size_t bc_allocators_class_sizes[BC_ALLOCATORS_NUM_CLASSES];

/* ===== Pool free-list (magazine + intrusive overflow) ===== */

/* Layout: 14×8B (mag) + 8B (mag_count) + 8B (head) = 128B = exactly 2 cache lines.
 * Hot path: push/pop only touch mag[], never the freed block.
 * Overflow/underflow fall back to the intrusive singly-linked list (head). */
typedef struct bc_allocators_free_list {
    void* mag[BC_ALLOCATORS_MAG_SIZE]; /* hot pointer cache (112 bytes) */
    size_t mag_count;                  /* entries currently in mag      */
    void* head;                        /* intrusive overflow list        */
} bc_allocators_free_list_t;

/* ===== Region (bump allocator) ===== */

typedef struct bc_allocators_region {
    void* base;
    size_t total_size;
    size_t page_class_size;
    size_t bump;
    uint8_t* page_class;
} bc_allocators_region_t;

/* ===== Context ===== */

struct bc_allocators_context {
    /* --- CL0: hot read-every-call fields --- */
    size_t page_size;
    size_t page_shift;
    size_t cache_line_size;
    bool tracking_enabled;

    /* --- Pool region (CL0-CL1 boundary) --- */
    bc_allocators_region_t region;

    /* --- Context mmap size (for destroy) --- */
    size_t context_mmap_size;

    /* --- Per-class free-lists (each pool = 1 CL, aligned to CL boundary) --- */
    bc_allocators_free_list_t pools[BC_ALLOCATORS_NUM_CLASSES] __attribute__((aligned(64)));
    bc_allocators_free_list_t large_pool;

    /* --- Cold: configuration --- */
    size_t max_pool_memory;

    size_t pool_alloc_count;
    size_t pool_free_count;
    size_t pool_active_bytes;
    size_t arena_create_count;
    size_t arena_destroy_count;
    size_t arena_total_bytes;
    size_t total_mapped_bytes;
    size_t peak_mapped_bytes;

    bc_allocators_leak_callback_t leak_callback;
    void* leak_callback_argument;

    bc_allocators_arena_cache_entry_t arena_cache[BC_ALLOCATORS_ARENA_CACHE_SIZE];
    size_t arena_cache_count;
};

#endif /* BC_ALLOCATORS_CONTEXT_INTERNAL_H */
