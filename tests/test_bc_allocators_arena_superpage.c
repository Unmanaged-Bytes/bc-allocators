// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_arena.h"

#include "bc_allocators_arena_internal.h"

#include "bc_core.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== alloc beyond capacity returns false ===== */

static void test_arena_alloc_beyond_capacity_returns_false(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {.max_pool_memory = 0, .tracking_enabled = true};
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    size_t alloc_size = 64;
    size_t success_count = 0;
    bool last_result = true;

    while (true) {
        void* ptr = NULL;
        last_result = bc_allocators_arena_allocate(arena, alloc_size, 1, &ptr);
        if (!last_result) {
            break;
        }
        success_count++;
    }

    assert_true(success_count >= 1);
    assert_false(last_result);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== capacity is constant after filling the arena ===== */

static void test_arena_capacity_constant_after_full(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {.max_pool_memory = 0, .tracking_enabled = true};
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    bc_allocators_arena_stats_t stats_before = {0};
    assert_true(bc_allocators_arena_get_stats(arena, &stats_before));
    size_t capacity_before = stats_before.capacity;

    size_t alloc_size = 64;
    while (true) {
        void* ptr = NULL;
        if (!bc_allocators_arena_allocate(arena, alloc_size, 1, &ptr)) {
            break;
        }
    }

    bc_allocators_arena_stats_t stats_after = {0};
    assert_true(bc_allocators_arena_get_stats(arena, &stats_after));
    size_t capacity_after = stats_after.capacity;

    assert_int_equal(capacity_before, capacity_after);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== reset allows full reuse ===== */

static void test_arena_reset_allows_full_reuse(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {.max_pool_memory = 0, .tracking_enabled = true};
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    size_t alloc_size = 64;
    size_t first_pass_count = 0;
    while (true) {
        void* ptr = NULL;
        if (!bc_allocators_arena_allocate(arena, alloc_size, 1, &ptr)) {
            break;
        }
        first_pass_count++;
    }

    assert_true(bc_allocators_arena_reset(arena));

    for (size_t i = 0; i < first_pass_count; i++) {
        void* ptr = NULL;
        assert_true(bc_allocators_arena_allocate(arena, alloc_size, 1, &ptr));
        assert_non_null(ptr);
    }

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== reset_secure zeros the used region ===== */

static void test_arena_reset_secure_zeros_used_region(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {.max_pool_memory = 0, .tracking_enabled = true};
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 1, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 64, (unsigned char)0xAA);

    assert_true(bc_allocators_arena_reset_secure(arena));

    void* ptr2 = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 1, &ptr2));
    assert_non_null(ptr2);

    unsigned char* bytes = (unsigned char*)ptr2;
    for (size_t i = 0; i < 64; i++) {
        assert_int_equal(bytes[i], 0x00);
    }

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== tracking bytes stable across reset ===== */

static void test_arena_tracking_bytes_stable_across_reset(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {.max_pool_memory = 0, .tracking_enabled = true};
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    bc_allocators_stats_t ctx_stats_before = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &ctx_stats_before));
    size_t total_mapped_before = ctx_stats_before.total_mapped_bytes;

    size_t alloc_size = 64;
    while (true) {
        void* ptr = NULL;
        if (!bc_allocators_arena_allocate(arena, alloc_size, 1, &ptr)) {
            break;
        }
    }

    assert_true(bc_allocators_arena_reset(arena));

    bc_allocators_stats_t ctx_stats_after = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &ctx_stats_after));
    size_t total_mapped_after = ctx_stats_after.total_mapped_bytes;

    assert_int_equal(total_mapped_before, total_mapped_after);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== create sets peak_mapped_bytes >= arena mmap size ===== */

static void test_arena_create_peak_tracking(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {.max_pool_memory = 0, .tracking_enabled = true};
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    bc_allocators_stats_t ctx_stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &ctx_stats));

    assert_true(ctx_stats.peak_mapped_bytes >= arena->total_mmap_size);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

static void test_arena_peak_not_updated_when_below(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {.max_pool_memory = 0, .tracking_enabled = true};
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));

    bc_allocators_arena_t* large_arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 1024 * 1024, &large_arena));

    bc_allocators_stats_t stats_after_large = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats_after_large));
    size_t peak_after_large = stats_after_large.peak_mapped_bytes;

    bc_allocators_arena_destroy(large_arena);

    bc_allocators_arena_t* small_arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &small_arena));

    bc_allocators_stats_t stats_after_small = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats_after_small));
    assert_int_equal(stats_after_small.peak_mapped_bytes, peak_after_large);

    bc_allocators_arena_destroy(small_arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_arena_alloc_beyond_capacity_returns_false),
        cmocka_unit_test(test_arena_capacity_constant_after_full),
        cmocka_unit_test(test_arena_reset_allows_full_reuse),
        cmocka_unit_test(test_arena_reset_secure_zeros_used_region),
        cmocka_unit_test(test_arena_tracking_bytes_stable_across_reset),
        cmocka_unit_test(test_arena_create_peak_tracking),
        cmocka_unit_test(test_arena_peak_not_updated_when_below),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
