// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_CONTEXT_H
#define BC_ALLOCATORS_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

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

#endif /* BC_ALLOCATORS_CONTEXT_H */
