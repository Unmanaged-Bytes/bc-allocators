// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_pool.h"

#include "bc_allocators_context_internal.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== Helpers ===== */

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
/* ===== alloc_count increments on each allocate ===== */

static void test_bc_allocators_pool_stats_alloc_count(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 64, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_alloc_count, 10);

    for (int i = 0; i < 10; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== free_count increments on each free ===== */

static void test_bc_allocators_pool_stats_free_count(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 64, &ptrs[i]));
    }

    for (int i = 0; i < 10; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_free_count, 10);

    bc_allocators_context_destroy(ctx);
}

/* ===== active_bytes increases on alloc, decreases on free ===== */

static void test_bc_allocators_pool_stats_active_bytes(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    /* active_bytes should be > 0 after allocation */
    assert_true(stats.pool_active_bytes > 0);

    bc_allocators_pool_free(ctx, ptr);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    /* active_bytes should be back to 0 after free */
    assert_int_equal(stats.pool_active_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== active_bytes tracks multiple allocations ===== */

static void test_bc_allocators_pool_stats_active_bytes_multiple(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    void* ptr1 = NULL;
    void* ptr2 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 128, &ptr1));
    assert_true(bc_allocators_pool_allocate(ctx, 256, &ptr2));

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    /* active_bytes should account for both allocations */
    assert_true(stats.pool_active_bytes > 0);

    size_t bytes_after_two = stats.pool_active_bytes;

    bc_allocators_pool_free(ctx, ptr1);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    /* active_bytes should have decreased */
    assert_true(stats.pool_active_bytes < bytes_after_two);
    assert_true(stats.pool_active_bytes > 0);

    bc_allocators_pool_free(ctx, ptr2);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_active_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== tracking disabled: all stats stay 0 ===== */

static void test_bc_allocators_pool_stats_tracking_disabled(void** state)
{
    (void)state;

    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = false,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_alloc_count, 0);
    assert_int_equal(stats.pool_free_count, 0);
    assert_int_equal(stats.pool_active_bytes, 0);

    bc_allocators_pool_free(ctx, ptr);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_alloc_count, 0);
    assert_int_equal(stats.pool_free_count, 0);
    assert_int_equal(stats.pool_active_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== alloc + free balanced: counts match ===== */

static void test_bc_allocators_pool_stats_alloc_free_balanced(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    for (int i = 0; i < 50; i++) {
        void* ptr = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 32, &ptr));
        assert_non_null(ptr);
        bc_allocators_pool_free(ctx, ptr);
    }

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_alloc_count, 50);
    assert_int_equal(stats.pool_free_count, 50);
    assert_int_equal(stats.pool_active_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== arena_active_count: destroy_count > create_count (else path at bc_allocators.c:330) ===== */

static void test_bc_allocators_pool_stats_arena_destroy_gt_create(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    /* Artificially set destroy_count > create_count to trigger else path */
    ctx->arena_create_count = (size_t)2;
    ctx->arena_destroy_count = (size_t)5;

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));

    /* When destroy > create, the else branch is taken: arena_active_count stays 0 */
    assert_int_equal(stats.arena_active_count, 0);
    assert_int_equal(stats.arena_create_count, 2);
    assert_int_equal(stats.arena_destroy_count, 5);

    /* Reset before destroy to avoid leak warning side effects */
    ctx->arena_create_count = (size_t)0;
    ctx->arena_destroy_count = (size_t)0;

    bc_allocators_context_destroy(ctx);
}

/* ===== arena counters are plain size_t (not _Atomic) after refactor ===== */
/* RED: fails to compile against current _Atomic size_t arena_create_count because
   &ctx->arena_create_count has type _Atomic(size_t) * incompatible with size_t *.
   GREEN: compiles after refactor removes _Atomic from counters. */

static void test_bc_allocators_pool_stats_counters_plain_type(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    /* Take addresses of arena counters as plain size_t * */
    size_t* create_ptr = &ctx->arena_create_count;
    size_t* destroy_ptr = &ctx->arena_destroy_count;
    assert_non_null(create_ptr);
    assert_non_null(destroy_ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== size not on class boundary: alloc+free must balance to zero ===== */

static void test_bc_allocators_pool_stats_active_bytes_non_class_size(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_tracking_ctx();

    for (int iteration = 0; iteration < 50; iteration++) {
        void* ptr = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 20, &ptr));
        assert_non_null(ptr);
        bc_allocators_pool_free(ctx, ptr);
    }

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.pool_active_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bc_allocators_pool_stats_alloc_count),
        cmocka_unit_test(test_bc_allocators_pool_stats_free_count),
        cmocka_unit_test(test_bc_allocators_pool_stats_active_bytes),
        cmocka_unit_test(test_bc_allocators_pool_stats_active_bytes_multiple),
        cmocka_unit_test(test_bc_allocators_pool_stats_active_bytes_non_class_size),
        cmocka_unit_test(test_bc_allocators_pool_stats_alloc_free_balanced),
        cmocka_unit_test(test_bc_allocators_pool_stats_arena_destroy_gt_create),
        cmocka_unit_test(test_bc_allocators_pool_stats_counters_plain_type),
        cmocka_unit_test(test_bc_allocators_pool_stats_tracking_disabled),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
