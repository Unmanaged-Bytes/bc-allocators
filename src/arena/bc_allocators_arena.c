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
#define BC_ALLOCATORS_ARENA_HEADER_ALIGNED ((sizeof(struct bc_allocators_arena) + 63) & ~(size_t)63)
#define BC_ALLOCATORS_CHUNK_HEADER_ALIGNED ((sizeof(bc_allocators_arena_chunk_t) + 63) & ~(size_t)63)

static bool bc_allocators_arena_map_block(bc_allocators_context_t* ctx, size_t mmap_size, void** out_block)
{
    for (size_t cache_index = 0; cache_index < ctx->arena_cache_count; cache_index++) {
        if (ctx->arena_cache[cache_index].mmap_size == mmap_size) {
            *out_block = ctx->arena_cache[cache_index].block;
            ctx->arena_cache[cache_index] = ctx->arena_cache[ctx->arena_cache_count - 1];
            ctx->arena_cache_count--;
            return true;
        }
    }
    if (!bc_allocators_platform_map(mmap_size, out_block)) {
        return false;
    }
    if (mmap_size >= BC_ALLOCATORS_HUGEPAGE_THRESHOLD) {
        madvise(*out_block, mmap_size, MADV_HUGEPAGE);
    }
    return true;
}

static void bc_allocators_arena_release_block(bc_allocators_context_t* ctx, void* block, size_t mmap_size)
{
    if (ctx->arena_cache_count < BC_ALLOCATORS_ARENA_CACHE_SIZE) {
        ctx->arena_cache[ctx->arena_cache_count].block = block;
        ctx->arena_cache[ctx->arena_cache_count].mmap_size = mmap_size;
        ctx->arena_cache_count++;
    } else {
        bc_allocators_platform_unmap(block, mmap_size);
    }
}

static bool bc_allocators_arena_make_first(bc_allocators_context_t* ctx, size_t capacity, size_t initial_chunk_size,
                                           size_t max_chunk_size, bool growable, bc_allocators_arena_t** out_arena)
{
    *out_arena = NULL;

    if (capacity == 0) {
        return false;
    }

    size_t page_size = ctx->page_size;
    size_t overhead = BC_ALLOCATORS_ARENA_HEADER_ALIGNED + BC_ALLOCATORS_CHUNK_HEADER_ALIGNED;

    size_t total_raw = 0;
    if (!bc_core_safe_add(overhead, capacity, &total_raw)) {
        return false;
    }
    size_t mmap_size = 0;
    if (!bc_core_align_up(total_raw, page_size, &mmap_size)) {
        return false;
    }

    void* block = NULL;
    if (!bc_allocators_arena_map_block(ctx, mmap_size, &block)) {
        return false;
    }

    bc_allocators_arena_t* arena = (bc_allocators_arena_t*)block;
    bc_allocators_arena_chunk_t* chunk = (bc_allocators_arena_chunk_t*)((unsigned char*)block + BC_ALLOCATORS_ARENA_HEADER_ALIGNED);

    chunk->next = NULL;
    chunk->mmap_size = mmap_size;
    chunk->capacity = mmap_size - overhead;
    chunk->used = 0;
    chunk->memory_start = (unsigned char*)block + overhead;

    arena->ctx = ctx;
    arena->first_chunk = chunk;
    arena->current_chunk = chunk;
    arena->allocation_count = 0;
    arena->initial_chunk_size = initial_chunk_size;
    arena->max_chunk_size = max_chunk_size;
    arena->growable = growable;

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

bool bc_allocators_arena_create(bc_allocators_context_t* ctx, size_t capacity, bc_allocators_arena_t** out_arena)
{
    return bc_allocators_arena_make_first(ctx, capacity, capacity, capacity, false, out_arena);
}

bool bc_allocators_arena_create_growable(bc_allocators_context_t* ctx, size_t initial_chunk_size, size_t max_chunk_size,
                                         bc_allocators_arena_t** out_arena)
{
    if (initial_chunk_size == 0) {
        return false;
    }
    if (max_chunk_size != 0 && max_chunk_size < initial_chunk_size) {
        return false;
    }
    return bc_allocators_arena_make_first(ctx, initial_chunk_size, initial_chunk_size, max_chunk_size, true, out_arena);
}

static bool bc_allocators_arena_grow(bc_allocators_arena_t* arena, size_t min_capacity)
{
    bc_allocators_context_t* ctx = arena->ctx;

    size_t last_capacity = arena->current_chunk->capacity;
    size_t target_capacity = last_capacity * 2u;
    if (target_capacity < min_capacity) {
        target_capacity = min_capacity;
    }
    if (arena->max_chunk_size != 0 && target_capacity > arena->max_chunk_size) {
        target_capacity = arena->max_chunk_size;
    }
    if (target_capacity < min_capacity) {
        target_capacity = min_capacity;
    }

    size_t overhead = BC_ALLOCATORS_CHUNK_HEADER_ALIGNED;
    size_t total_raw = 0;
    if (!bc_core_safe_add(overhead, target_capacity, &total_raw)) {
        return false;
    }
    size_t mmap_size = 0;
    if (!bc_core_align_up(total_raw, ctx->page_size, &mmap_size)) {
        return false;
    }

    void* block = NULL;
    if (!bc_allocators_arena_map_block(ctx, mmap_size, &block)) {
        return false;
    }

    bc_allocators_arena_chunk_t* chunk = (bc_allocators_arena_chunk_t*)block;
    chunk->next = NULL;
    chunk->mmap_size = mmap_size;
    chunk->capacity = mmap_size - overhead;
    chunk->used = 0;
    chunk->memory_start = (unsigned char*)block + overhead;

    arena->current_chunk->next = chunk;
    arena->current_chunk = chunk;

    if (ctx->tracking_enabled) {
        ctx->arena_total_bytes += mmap_size;
        ctx->total_mapped_bytes += mmap_size;
        if (ctx->total_mapped_bytes > ctx->peak_mapped_bytes) {
            ctx->peak_mapped_bytes = ctx->total_mapped_bytes;
        }
    }
    return true;
}

void bc_allocators_arena_destroy(bc_allocators_arena_t* arena)
{
    bc_allocators_context_t* ctx = arena->ctx;
    bc_allocators_arena_chunk_t* chunk = arena->first_chunk->next;
    while (chunk != NULL) {
        bc_allocators_arena_chunk_t* next = chunk->next;
        size_t chunk_mmap_size = chunk->mmap_size;
        if (ctx->tracking_enabled) {
            ctx->arena_total_bytes -= chunk_mmap_size;
            ctx->total_mapped_bytes -= chunk_mmap_size;
        }
        bc_allocators_arena_release_block(ctx, chunk, chunk_mmap_size);
        chunk = next;
    }

    size_t first_mmap_size = arena->first_chunk->mmap_size;
    if (ctx->tracking_enabled) {
        ctx->arena_destroy_count++;
        ctx->arena_total_bytes -= first_mmap_size;
        ctx->total_mapped_bytes -= first_mmap_size;
    }
    bc_allocators_arena_release_block(ctx, arena, first_mmap_size);
}

static bool bc_allocators_arena_try_alloc_in_chunk(bc_allocators_arena_chunk_t* chunk, size_t size, size_t alignment, void** out_ptr)
{
    uintptr_t base_address = (uintptr_t)chunk->memory_start;
    uintptr_t current_address = base_address + chunk->used;

    size_t aligned_address_raw = 0;
    if (!bc_core_align_up((size_t)current_address, alignment, &aligned_address_raw)) {
        return false;
    }
    size_t aligned_offset = (size_t)((uintptr_t)aligned_address_raw - base_address);

    size_t end = 0;
    if (!bc_core_safe_add(aligned_offset, size, &end)) {
        return false;
    }
    if (end > chunk->capacity) {
        return false;
    }

    *out_ptr = chunk->memory_start + aligned_offset;
    chunk->used = end;
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

    if (bc_allocators_arena_try_alloc_in_chunk(arena->current_chunk, size, alignment, out_ptr)) {
        arena->allocation_count++;
        return true;
    }

    if (!arena->growable) {
        return false;
    }

    size_t min_capacity = size + alignment;
    if (!bc_allocators_arena_grow(arena, min_capacity)) {
        return false;
    }
    if (!bc_allocators_arena_try_alloc_in_chunk(arena->current_chunk, size, alignment, out_ptr)) {
        return false;
    }
    arena->allocation_count++;
    return true;
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
    bc_allocators_context_t* ctx = arena->ctx;
    bc_allocators_arena_chunk_t* chunk = arena->first_chunk->next;
    while (chunk != NULL) {
        bc_allocators_arena_chunk_t* next = chunk->next;
        size_t chunk_mmap_size = chunk->mmap_size;
        if (ctx->tracking_enabled) {
            ctx->arena_total_bytes -= chunk_mmap_size;
            ctx->total_mapped_bytes -= chunk_mmap_size;
        }
        bc_allocators_arena_release_block(ctx, chunk, chunk_mmap_size);
        chunk = next;
    }
    arena->first_chunk->next = NULL;
    arena->first_chunk->used = 0;
    arena->current_chunk = arena->first_chunk;
    arena->allocation_count = 0;
    return true;
}

bool bc_allocators_arena_reset_secure(bc_allocators_arena_t* arena)
{
    bc_allocators_arena_chunk_t* chunk = arena->first_chunk;
    while (chunk != NULL) {
        bc_core_zero_secure(chunk->memory_start, chunk->used);
        madvise(chunk->memory_start, chunk->capacity, MADV_DONTNEED);
        chunk = chunk->next;
    }
    return bc_allocators_arena_reset(arena);
}

bool bc_allocators_arena_release_pages(bc_allocators_arena_t* arena)
{
    bc_allocators_arena_chunk_t* chunk = arena->first_chunk;
    while (chunk != NULL) {
        bc_allocators_platform_advise(chunk->memory_start, chunk->capacity, MADV_DONTNEED);
        chunk = chunk->next;
    }
    return true;
}

bool bc_allocators_arena_get_stats(const bc_allocators_arena_t* arena, bc_allocators_arena_stats_t* out_stats)
{
    size_t chunk_count = 0;
    size_t total_capacity = 0;
    size_t total_used = 0;
    size_t total_reserved = 0;
    const bc_allocators_arena_chunk_t* chunk = arena->first_chunk;
    while (chunk != NULL) {
        chunk_count += 1u;
        total_capacity += chunk->capacity;
        total_used += chunk->used;
        total_reserved += chunk->mmap_size;
        chunk = chunk->next;
    }

    out_stats->capacity = total_capacity;
    out_stats->used = total_used;
    out_stats->allocation_count = arena->allocation_count;
    out_stats->chunk_count = chunk_count;
    out_stats->total_reserved = total_reserved;
    return true;
}
