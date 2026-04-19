// SPDX-License-Identifier: MIT

#include "bc_allocators_arena.h"

#include "bc_allocators_arena_internal.h"
#include "bc_allocators_context_internal.h"
#include "bc_allocators_platform_internal.h"

#include "bc_core.h"

#include <sys/mman.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BC_ALLOCATORS_HUGEPAGE_THRESHOLD (2 * 1024 * 1024)

bool bc_allocators_arena_create(bc_allocators_context_t* ctx, size_t capacity, bc_allocators_arena_t** out_arena)
{
    *out_arena = NULL;

    if (capacity == 0) {
        return false;
    }

    size_t page_size = ctx->page_size;

    size_t header_aligned = (sizeof(struct bc_allocators_arena) + 63) & ~(size_t)63;

    size_t total_raw = 0;
    if (!bc_core_safe_add(header_aligned, capacity, &total_raw)) {
        return false;
    }

    size_t mmap_size = 0;
    if (!bc_core_align_up(total_raw, page_size, &mmap_size)) {
        return false;
    }

    void* block = NULL;
    for (size_t cache_index = 0; cache_index < ctx->arena_cache_count; cache_index++) {
        if (ctx->arena_cache[cache_index].mmap_size == mmap_size) {
            block = ctx->arena_cache[cache_index].block;
            ctx->arena_cache[cache_index] = ctx->arena_cache[ctx->arena_cache_count - 1];
            ctx->arena_cache_count--;
            break;
        }
    }

    if (block == NULL) {
        if (!bc_allocators_platform_map(mmap_size, &block)) {
            return false;
        }
        if (mmap_size >= BC_ALLOCATORS_HUGEPAGE_THRESHOLD) {
            madvise(block, mmap_size, MADV_HUGEPAGE);
        }
    }

    bc_allocators_arena_t* arena = (bc_allocators_arena_t*)block;
    arena->ctx = ctx;
    arena->total_mmap_size = mmap_size;
    arena->capacity = capacity;
    arena->used = 0;
    arena->allocation_count = 0;
    arena->memory_start = (unsigned char*)block + header_aligned;

    if (ctx->tracking_enabled) {
        ctx->arena_create_count++;
        ctx->arena_total_bytes += mmap_size;
        ctx->total_mapped_bytes += mmap_size;

        if (ctx->total_mapped_bytes > ctx->peak_mapped_bytes) {
            ctx->peak_mapped_bytes = ctx->total_mapped_bytes;
        }
    }

    *out_arena = arena;
    return true;
}

void bc_allocators_arena_destroy(bc_allocators_arena_t* arena)
{
    bc_allocators_context_t* ctx = arena->ctx;
    if (ctx->tracking_enabled) {
        ctx->arena_destroy_count++;
        ctx->arena_total_bytes -= arena->total_mmap_size;
        ctx->total_mapped_bytes -= arena->total_mmap_size;
    }

    if (ctx->arena_cache_count < BC_ALLOCATORS_ARENA_CACHE_SIZE) {
        ctx->arena_cache[ctx->arena_cache_count].block = (void*)arena;
        ctx->arena_cache[ctx->arena_cache_count].mmap_size = arena->total_mmap_size;
        ctx->arena_cache_count++;
    } else {
        bc_allocators_platform_unmap(arena, arena->total_mmap_size);
    }
}

static bool try_alloc_in_primary(bc_allocators_arena_t* arena, size_t size, size_t alignment, void** out_ptr)
{
    uintptr_t base_address = (uintptr_t)arena->memory_start;
    uintptr_t current_address = base_address + arena->used;

    size_t aligned_address_raw = 0;
    if (!bc_core_align_up((size_t)current_address, alignment, &aligned_address_raw)) {
        return false;
    }

    size_t aligned_offset = (size_t)((uintptr_t)aligned_address_raw - base_address);

    size_t end = 0;
    if (!bc_core_safe_add(aligned_offset, size, &end)) {
        return false;
    }

    if (end > arena->capacity) {
        return false;
    }

    *out_ptr = arena->memory_start + aligned_offset;
    arena->used = end;
    arena->allocation_count++;
    return true;
}

bool bc_allocators_arena_allocate(bc_allocators_arena_t* arena, size_t size, size_t alignment, void** out_ptr)
{
    *out_ptr = NULL;

    if (size == 0 || alignment == 0) {
        return false;
    }

    if ((alignment & (alignment - 1)) != 0) {
        return false;
    }

    if (try_alloc_in_primary(arena, size, alignment, out_ptr)) {
        return true;
    }

    return false;
}

bool bc_allocators_arena_copy_string(bc_allocators_arena_t* arena, const char* source, const char** out_copy)
{
    size_t len = 0;
    bc_core_length(source, '\0', &len);
    size_t alloc_size = len + 1;

    void* ptr = NULL;
    if (!bc_allocators_arena_allocate(arena, alloc_size, 1, &ptr)) {
        return false;
    }

    bc_core_copy(ptr, source, alloc_size);
    *out_copy = (const char*)ptr;
    return true;
}

bool bc_allocators_arena_reset(bc_allocators_arena_t* arena)
{
    arena->used = 0;
    arena->allocation_count = 0;

    return true;
}

bool bc_allocators_arena_reset_secure(bc_allocators_arena_t* arena)
{
    bc_core_zero_secure(arena->memory_start, arena->used);
    madvise(arena->memory_start, arena->capacity, MADV_DONTNEED);

    arena->used = 0;
    arena->allocation_count = 0;

    return true;
}

bool bc_allocators_arena_release_pages(bc_allocators_arena_t* arena)
{
    bc_allocators_platform_advise(arena->memory_start, arena->capacity, MADV_DONTNEED);
    return true;
}

bool bc_allocators_arena_get_stats(const bc_allocators_arena_t* arena, bc_allocators_arena_stats_t* out_stats)
{
    out_stats->capacity = arena->capacity;
    out_stats->used = arena->used;
    out_stats->allocation_count = arena->allocation_count;

    return true;
}
