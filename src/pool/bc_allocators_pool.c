// SPDX-License-Identifier: MIT

#include "bc_allocators_pool.h"

#include "bc_allocators_context_internal.h"
#include "bc_allocators_platform_internal.h"

#include "bc_core.h"

#include <sys/mman.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const size_t bc_allocators_class_sizes[BC_ALLOCATORS_NUM_CLASSES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304,
};

typedef struct bc_allocators_large_meta {
    size_t user_size;
    size_t total_size;
} bc_allocators_large_meta_t;

static int size_class_for_size(size_t size)
{
    if (size > BC_ALLOCATORS_MAX_CLASS_SIZE) {
        return -1;
    }
    if (size <= 16) {
        return 0;
    }
    int bits = 64 - __builtin_clzl(size - 1);
    return bits - 4;
}

static inline void free_list_push(bc_allocators_free_list_t* fl, void* ptr)
{
    if (__builtin_expect(fl->mag_count < BC_ALLOCATORS_MAG_SIZE, 1)) {
        fl->mag[fl->mag_count++] = ptr;
        return;
    }

    *(void**)ptr = fl->head;
    fl->head = ptr;
}

static inline void* free_list_pop(bc_allocators_free_list_t* fl)
{
    if (__builtin_expect(fl->mag_count > 0, 1)) {
        return fl->mag[--fl->mag_count];
    }

    void* head = fl->head;
    if (__builtin_expect(head != NULL, 1)) {
        fl->head = *(void**)head;
    }
    return head;
}

static void tag_page_class(bc_allocators_context_t* ctx, size_t offset, size_t size_bytes, uint8_t class_val)
{
    size_t page_shift = ctx->page_shift;
    size_t start_page = offset >> page_shift;
    size_t end_page = (offset + size_bytes - 1) >> page_shift;
    for (size_t p = start_page; p <= end_page; p++) {
        ctx->region.page_class[p] = class_val;
    }
}

static void* bump_raw(bc_allocators_context_t* ctx, size_t bytes_needed)
{
    size_t old_bump = ctx->region.bump;
    if (old_bump + bytes_needed > ctx->region.total_size) {
        return NULL;
    }
    ctx->region.bump += bytes_needed;
    return (unsigned char*)ctx->region.base + old_bump;
}

static void* bump_allocate_class(bc_allocators_context_t* ctx, int cls)
{
    size_t block_size = bc_allocators_class_sizes[cls];
    size_t page_size = ctx->page_size;

    size_t bump_size = block_size;
    if (bump_size < page_size) {
        bump_size = page_size;
    } else {
        size_t aligned = 0;
        if (!bc_core_align_up(bump_size, page_size, &aligned)) {
            return NULL;
        }
        bump_size = aligned;
    }

    void* page_base = bump_raw(ctx, bump_size);
    if (page_base == NULL) {
        return NULL;
    }

    size_t offset = (size_t)((unsigned char*)page_base - (unsigned char*)ctx->region.base);
    tag_page_class(ctx, offset, bump_size, (uint8_t)cls);

    size_t num_blocks = bump_size / block_size;
    unsigned char* base = (unsigned char*)page_base;
    for (size_t i = 1; i < num_blocks; i++) {
        void* block = base + i * block_size;
        free_list_push(&ctx->pools[cls], block);
    }

    return page_base;
}

static void* allocate_large(bc_allocators_context_t* ctx, size_t user_size)
{
    size_t meta_size = BC_ALLOCATORS_LARGE_META_SIZE;
    size_t total_raw = 0;
    if (!bc_core_safe_add(meta_size, user_size, &total_raw)) {
        return NULL;
    }
    size_t total_aligned = 0;
    if (!bc_core_align_up(total_raw, ctx->page_size, &total_aligned)) {
        return NULL;
    }

    void* block = free_list_pop(&ctx->large_pool);
    if (block != NULL) {
        bc_allocators_large_meta_t* meta = (bc_allocators_large_meta_t*)block;
        if (meta->total_size >= total_aligned) {
            meta->user_size = user_size;
            return (unsigned char*)block + meta_size;
        }
        free_list_push(&ctx->large_pool, block);
    }

    block = bump_raw(ctx, total_aligned);
    if (block == NULL) {
        return NULL;
    }

    size_t offset = (size_t)((unsigned char*)block - (unsigned char*)ctx->region.base);
    tag_page_class(ctx, offset, total_aligned, BC_ALLOCATORS_PAGE_CLASS_LARGE);

    bc_allocators_large_meta_t* meta = (bc_allocators_large_meta_t*)block;
    meta->total_size = total_aligned;
    meta->user_size = user_size;
    return (unsigned char*)block + meta_size;
}

static void free_large(bc_allocators_context_t* ctx, void* user_ptr)
{
    size_t meta_size = BC_ALLOCATORS_LARGE_META_SIZE;
    void* block = (unsigned char*)user_ptr - meta_size;
    bc_allocators_large_meta_t* meta = (bc_allocators_large_meta_t*)block;

    meta->user_size = 0;

    free_list_push(&ctx->large_pool, block);
}

static inline void track_alloc(bc_allocators_context_t* ctx, size_t bytes)
{
    if (!ctx->tracking_enabled) {
        return;
    }
    ctx->pool_alloc_count++;
    ctx->pool_active_bytes += bytes;
}

static inline void track_free(bc_allocators_context_t* ctx, size_t bytes)
{
    if (!ctx->tracking_enabled) {
        return;
    }
    ctx->pool_free_count++;
    ctx->pool_active_bytes -= bytes;
}

static bool ptr_in_region(const bc_allocators_context_t* ctx, const void* ptr)
{
    const unsigned char* base = (const unsigned char*)ctx->region.base;
    const unsigned char* p = (const unsigned char*)ptr;
    return p >= base && p < base + ctx->region.total_size;
}

static size_t get_usable_size(const bc_allocators_context_t* ctx, const void* ptr, bool* is_large)
{
    size_t offset = (size_t)((const unsigned char*)ptr - (const unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    if (cls < BC_ALLOCATORS_NUM_CLASSES) {
        *is_large = false;
        return bc_allocators_class_sizes[cls];
    }
    *is_large = true;
    size_t meta_size = BC_ALLOCATORS_LARGE_META_SIZE;
    const bc_allocators_large_meta_t* meta = (const bc_allocators_large_meta_t*)((const unsigned char*)ptr - meta_size);
    return meta->user_size;
}

__attribute__((flatten, hot)) bool bc_allocators_pool_allocate(bc_allocators_context_t* ctx, size_t size, void** out_ptr)
{
    *out_ptr = NULL;

    if (size == 0) {
        return false;
    }

    int original_class = size_class_for_size(size);
    if (original_class == -1) {
        void* user_ptr = allocate_large(ctx, size);
        if (user_ptr == NULL) {
            return false;
        }
        *out_ptr = user_ptr;
        track_alloc(ctx, size);
        return true;
    }

    int cls = original_class;
    size_t class_size = bc_allocators_class_sizes[cls];

    void* block = free_list_pop(&ctx->pools[cls]);
    if (__builtin_expect(block != NULL, 1)) {
        *out_ptr = block;
        track_alloc(ctx, class_size);
        return true;
    }

    block = bump_allocate_class(ctx, cls);
    if (block == NULL) {
        return false;
    }

    *out_ptr = block;
    track_alloc(ctx, class_size);
    return true;
}

bool bc_allocators_pool_reallocate(bc_allocators_context_t* ctx, void* ptr, size_t new_size, void** out_ptr)
{
    *out_ptr = NULL;

    if (new_size == 0) {
        return false;
    }

    if (ptr == NULL) {
        return bc_allocators_pool_allocate(ctx, new_size, out_ptr);
    }

    bool old_is_large = false;
    size_t old_usable = get_usable_size(ctx, ptr, &old_is_large);

    if (!old_is_large) {
        int new_cls = size_class_for_size(new_size);
        if (new_cls >= 0) {
            size_t offset = (size_t)((const unsigned char*)ptr - (const unsigned char*)ctx->region.base);
            size_t page_idx = offset >> ctx->page_shift;
            uint8_t old_cls = ctx->region.page_class[page_idx];
            if (old_cls == (uint8_t)new_cls) {
                *out_ptr = ptr;
                return true;
            }
        }
    }

    if (old_is_large) {
        int new_class = size_class_for_size(new_size);
        if (new_class == -1) {
            size_t meta_size = BC_ALLOCATORS_LARGE_META_SIZE;
            void* old_block = (unsigned char*)ptr - meta_size;
            bc_allocators_large_meta_t* old_meta = (bc_allocators_large_meta_t*)old_block;
            size_t old_total = old_meta->total_size;

            size_t new_total_raw = 0;
            if (!bc_core_safe_add(meta_size, new_size, &new_total_raw)) {
                return false;
            }
            size_t new_total = 0;
            if (!bc_core_align_up(new_total_raw, ctx->page_size, &new_total)) {
                return false;
            }

            if (new_total <= old_total) {
                size_t old_user_size = old_meta->user_size;
                old_meta->user_size = new_size;
                *out_ptr = ptr;
                if (ctx->tracking_enabled) {
                    ctx->pool_active_bytes -= old_user_size;
                    ctx->pool_active_bytes += new_size;
                }
                return true;
            }
        }
    }

    void* new_ptr = NULL;
    if (!bc_allocators_pool_allocate(ctx, new_size, &new_ptr)) {
        return false;
    }

    size_t copy_size = old_usable < new_size ? old_usable : new_size;
    bc_core_copy(new_ptr, ptr, copy_size);
    bc_allocators_pool_free(ctx, ptr);

    *out_ptr = new_ptr;
    return true;
}

__attribute__((flatten, hot)) void bc_allocators_pool_free(bc_allocators_context_t* ctx, void* ptr)
{
    if (!ptr_in_region(ctx, ptr)) {
        return;
    }

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    if (cls < BC_ALLOCATORS_NUM_CLASSES) {
        free_list_push(&ctx->pools[cls], ptr);
        track_free(ctx, bc_allocators_class_sizes[cls]);
        return;
    }

    if (cls == BC_ALLOCATORS_PAGE_CLASS_LARGE) {
        size_t meta_size = BC_ALLOCATORS_LARGE_META_SIZE;
        const bc_allocators_large_meta_t* meta = (const bc_allocators_large_meta_t*)((unsigned char*)ptr - meta_size);
        size_t user_size = meta->user_size;
        free_large(ctx, ptr);
        track_free(ctx, user_size);
        return;
    }
}
