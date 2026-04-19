// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_pool.h"

#include "bc_allocators_context_internal.h"

#include "bc_core.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== Helper ===== */

static bc_allocators_context_t* create_tracking_ctx(void)
{
    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);
    return ctx;
}

/* ===== Test: pool_free fallthrough — cls 0-18, page_class < NUM_CLASSES ===== */
/* After header removal: pool_free dispatches via page_class[offset>>page_shift].
   If cls < BC_ALLOCATORS_NUM_CLASSES (0-18), push to pools[cls].
   The guard (cls >= NUM_CLASSES) evaluates to false for valid pool class values 0-18. */

static void test_bc_allocators_pool_free_small_class_1(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    /* size=16 -> class=0 -> page tagged with class 0
       pool_free: page_class returns 0, cls < NUM_CLASSES -> push to pools[0] */
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 16, (unsigned char)0x11);

    bc_allocators_pool_free(ctx, ptr);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_alloc_count, 1);
    assert_int_equal(stats.pool_free_count, 1);

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_pool_free_small_class_2(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    /* size=32 -> class=1 -> page tagged with class 1
       pool_free: page_class returns 1, cls < NUM_CLASSES -> push to pools[1] */
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 32, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 32, (unsigned char)0x22);

    bc_allocators_pool_free(ctx, ptr);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_alloc_count, 1);
    assert_int_equal(stats.pool_free_count, 1);

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_pool_free_all_small_classes(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    /* Allocate and free from each of the 19 size classes (0-18).
     * size=4194304 maps to class 18, allocated directly from pool (page tagged with 18). */
    static const size_t sizes[19] = {
        16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304,
    };
    void* ptrs[19];

    for (int i = 0; i < 19; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, sizes[i], &ptrs[i]));
        assert_non_null(ptrs[i]);
        bc_core_fill(ptrs[i], sizes[i], (unsigned char)(int)(0x10 + i));
    }

    /* Verify class 18 (4194304) was not routed to large path:
     * page_class must be 18, not BC_ALLOCATORS_PAGE_CLASS_LARGE (0xFE). */
    {
        void* ptr18 = ptrs[18];
        size_t offset = (size_t)((unsigned char*)ptr18 - (unsigned char*)ctx->region.base);
        size_t page_idx = offset >> ctx->page_shift;
        uint8_t cls = ctx->region.page_class[page_idx];
        assert_int_equal((int)cls, 18);
    }

    for (int i = 0; i < 19; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_alloc_count, 19);
    assert_int_equal(stats.pool_free_count, 19);
    assert_int_equal(stats.pool_active_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: reallocate small blocks also exercises pool_free fallthrough ===== */

static void test_bc_allocators_pool_free_via_realloc(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    /* Reallocate to a bigger class: the old small block is freed via pool_free,
       exercising the fallthrough at line 488 with a valid cls (1-14). */
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 16, (unsigned char)0xAA);

    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, 64, &new_ptr));
    assert_non_null(new_ptr);

    /* First 16 bytes preserved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0xAA);

    bc_allocators_pool_free(ctx, new_ptr);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    /* alloc: 2 (initial + realloc), free: 2 (via realloc + final free) */
    assert_int_equal(stats.pool_alloc_count, stats.pool_free_count);

    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bc_allocators_pool_free_small_class_1),
        cmocka_unit_test(test_bc_allocators_pool_free_small_class_2),
        cmocka_unit_test(test_bc_allocators_pool_free_all_small_classes),
        cmocka_unit_test(test_bc_allocators_pool_free_via_realloc),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
