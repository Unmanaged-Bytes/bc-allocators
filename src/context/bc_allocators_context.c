// SPDX-License-Identifier: MIT

#include "bc_allocators.h"

#include "bc_allocators_context_internal.h"
#include "bc_allocators_platform_internal.h"

#include "bc_core.h"

#include <sys/mman.h>
#include <unistd.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool bc_allocators_compute_alloc_size(const bc_allocators_context_t* ctx, size_t element_size, size_t count, size_t* out_size)
{
    if (element_size == 0 || count == 0) {
        return false;
    }
    size_t product = 0;
    if (!bc_core_safe_multiply(element_size, count, &product)) {
        return false;
    }
    if (!bc_core_align_up(product, ctx->page_size, out_size)) {
        return false;
    }
    return true;
}

bool bc_allocators_context_create(const bc_allocators_context_config_t* config, bc_allocators_context_t** out_ctx)
{
    *out_ctx = NULL;

    size_t max_memory = BC_ALLOCATORS_DEFAULT_MAX;
    bool tracking = true;
    bc_allocators_leak_callback_t leak_callback = NULL;
    void* leak_callback_argument = NULL;
    if (config != NULL) {
        max_memory = config->max_pool_memory;
        tracking = config->tracking_enabled;
        leak_callback = config->leak_callback;
        leak_callback_argument = config->leak_callback_argument;
    }
    if (max_memory == 0) {
        max_memory = BC_ALLOCATORS_DEFAULT_MAX;
    }

    size_t page_size = 0;
    if (!bc_allocators_platform_get_page_size(&page_size)) {
        return false;
    }

    size_t cache_line_size = 0;
    if (!bc_allocators_platform_get_cache_line_size(&cache_line_size)) {
        cache_line_size = 64;
    }

    size_t aligned_max = 0;
    if (!bc_core_align_up(max_memory, page_size, &aligned_max)) {
        return false;
    }

    size_t num_pages = aligned_max / page_size;

    size_t page_class_size = 0;
    if (!bc_core_align_up(num_pages, page_size, &page_class_size)) {
        return false;
    }

    size_t context_alloc_size = 0;
    if (!bc_core_align_up(sizeof(struct bc_allocators_context), page_size, &context_alloc_size)) {
        return false;
    }

    bc_allocators_context_t* ctx = NULL;
    if (!bc_allocators_platform_map(context_alloc_size, (void**)&ctx)) {
        return false;
    }

    if (!bc_allocators_platform_map(aligned_max, &ctx->region.base)) {
        bc_allocators_platform_unmap(ctx, context_alloc_size);
        return false;
    }

    void* page_class_mem = NULL;
    if (!bc_allocators_platform_map(page_class_size, &page_class_mem)) {
        bc_allocators_platform_unmap(ctx->region.base, aligned_max);
        bc_allocators_platform_unmap(ctx, context_alloc_size);
        return false;
    }
    bc_core_fill(page_class_mem, page_class_size, 0xFF);

    ctx->page_size = page_size;
    ctx->page_shift = (size_t)bc_core_ctz_u64((uint64_t)page_size);
    ctx->cache_line_size = cache_line_size;
    ctx->context_mmap_size = context_alloc_size;

    ctx->region.total_size = aligned_max;
    ctx->region.page_class_size = page_class_size;
    ctx->region.bump = 0;
    ctx->region.page_class = (uint8_t*)page_class_mem;

    for (size_t i = 0; i < BC_ALLOCATORS_NUM_CLASSES; i++) {
        ctx->pools[i].head = NULL;
    }
    ctx->large_pool.head = NULL;

    ctx->max_pool_memory = aligned_max;
    ctx->tracking_enabled = tracking;

    ctx->pool_alloc_count = 0;
    ctx->pool_free_count = 0;
    ctx->pool_active_bytes = 0;
    ctx->arena_create_count = 0;
    ctx->arena_destroy_count = 0;
    ctx->arena_total_bytes = 0;
    ctx->total_mapped_bytes = 0;
    ctx->peak_mapped_bytes = 0;
    ctx->leak_callback = leak_callback;
    ctx->leak_callback_argument = leak_callback_argument;

    madvise(ctx->region.base, aligned_max, MADV_HUGEPAGE);

    *out_ctx = ctx;
    return true;
}

void bc_allocators_context_destroy(bc_allocators_context_t* ctx)
{
    if (ctx->tracking_enabled && ctx->leak_callback != NULL) {
        if (ctx->pool_alloc_count != ctx->pool_free_count) {
            ctx->leak_callback("bc-allocators: pool leak detected", ctx->leak_callback_argument);
        }

        if (ctx->arena_create_count != ctx->arena_destroy_count) {
            ctx->leak_callback("bc-allocators: arena leak detected", ctx->leak_callback_argument);
        }
    }

    for (size_t cache_index = 0; cache_index < ctx->arena_cache_count; cache_index++) {
        bc_allocators_platform_unmap(ctx->arena_cache[cache_index].block, ctx->arena_cache[cache_index].mmap_size);
    }

    bc_allocators_platform_unmap((void*)ctx->region.page_class, ctx->region.page_class_size);

    bc_allocators_platform_unmap(ctx->region.base, ctx->region.total_size);

    size_t ctx_size = ctx->context_mmap_size;
    bc_allocators_platform_unmap(ctx, ctx_size);
}

bool bc_allocators_context_page_size(const bc_allocators_context_t* ctx, size_t* out_page_size)
{
    *out_page_size = ctx->page_size;
    return true;
}

bool bc_allocators_context_cache_line_size(const bc_allocators_context_t* ctx, size_t* out_cache_line_size)
{
    *out_cache_line_size = ctx->cache_line_size;
    return true;
}

bool bc_allocators_context_get_stats(const bc_allocators_context_t* ctx, bc_allocators_stats_t* out_stats)
{
    bc_core_zero(out_stats, sizeof(*out_stats));

    if (!ctx->tracking_enabled) {
        return true;
    }

    out_stats->pool_alloc_count = ctx->pool_alloc_count;
    out_stats->pool_free_count = ctx->pool_free_count;
    out_stats->pool_active_bytes = ctx->pool_active_bytes;
    out_stats->arena_create_count = ctx->arena_create_count;
    out_stats->arena_destroy_count = ctx->arena_destroy_count;
    if (out_stats->arena_create_count >= out_stats->arena_destroy_count) {
        out_stats->arena_active_count = out_stats->arena_create_count - out_stats->arena_destroy_count;
    }
    out_stats->arena_total_bytes = ctx->arena_total_bytes;
    out_stats->total_mapped_bytes = ctx->total_mapped_bytes;
    out_stats->peak_mapped_bytes = ctx->peak_mapped_bytes;

    return true;
}
