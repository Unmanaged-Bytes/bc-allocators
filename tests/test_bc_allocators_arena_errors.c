// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_arena.h"
#include "bc_core.h"

#include "bc_allocators_arena_internal.h"
#include "bc_allocators_platform_internal.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== Wrap: bc_allocators_platform_map ===== */

static int map_call_count = 0;
static int map_fail_on_call = 0;

bool __real_bc_allocators_platform_map(size_t size, void** out_pointer);

bool __wrap_bc_allocators_platform_map(size_t size, void** out_pointer)
{
    map_call_count++;
    if (map_fail_on_call > 0 && map_call_count == map_fail_on_call) {
        if (out_pointer != NULL) {
            *out_pointer = NULL;
        }
        return false;
    }
    return __real_bc_allocators_platform_map(size, out_pointer);
}

/* ===== Wrap: bc_core_align_up ===== */

static int align_up_call_count = 0;
static int align_up_fail_on_call = 0;
static int align_up_fail_from_call = 0;

bool __real_bc_core_align_up(size_t value, size_t alignment, size_t* out_result);

bool __wrap_bc_core_align_up(size_t value, size_t alignment, size_t* out_result)
{
    align_up_call_count++;
    if (align_up_fail_on_call > 0 && align_up_call_count == align_up_fail_on_call) {
        return false;
    }
    if (align_up_fail_from_call > 0 && align_up_call_count >= align_up_fail_from_call) {
        return false;
    }
    return __real_bc_core_align_up(value, alignment, out_result);
}

/* ===== Wrap: bc_core_safe_add ===== */

static int safe_add_call_count = 0;
static int safe_add_fail_on_call = 0;
static int safe_add_fail_from_call = 0;

bool __real_bc_core_safe_add(size_t a, size_t b, size_t* out_result);

bool __wrap_bc_core_safe_add(size_t a, size_t b, size_t* out_result)
{
    safe_add_call_count++;
    if (safe_add_fail_on_call > 0 && safe_add_call_count == safe_add_fail_on_call) {
        return false;
    }
    if (safe_add_fail_from_call > 0 && safe_add_call_count >= safe_add_fail_from_call) {
        return false;
    }
    return __real_bc_core_safe_add(a, b, out_result);
}

/* ===== Reset helpers ===== */

static void reset_wraps(void)
{
    map_call_count = 0;
    map_fail_on_call = 0;
    align_up_call_count = 0;
    align_up_fail_on_call = 0;
    align_up_fail_from_call = 0;
    safe_add_call_count = 0;
    safe_add_fail_on_call = 0;
    safe_add_fail_from_call = 0;
}

/* ===== Helper: create a context (counts map calls during context_create) ===== */

static bc_allocators_context_t* create_ctx_and_record_map_calls(int* out_map_calls_for_ctx)
{
    reset_wraps();
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    *out_map_calls_for_ctx = map_call_count;
    return ctx;
}

/* ===== Test: mmap fail on arena create (first map call after ctx create) ===== */

static void test_bc_allocators_arena_create_map_fail(void** state)
{
    (void)state;
    int ctx_map_calls = 0;
    bc_allocators_context_t* ctx = create_ctx_and_record_map_calls(&ctx_map_calls);

    map_fail_on_call = ctx_map_calls + 1;

    bc_allocators_arena_t* arena = NULL;
    assert_false(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_null(arena);

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: align_up overflow on alignment ===== */

static void test_bc_allocators_arena_allocate_align_up_overflow(void** state)
{
    (void)state;
    int ctx_map_calls = 0;
    bc_allocators_context_t* ctx = create_ctx_and_record_map_calls(&ctx_map_calls);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    align_up_fail_from_call = align_up_call_count + 1;

    void* ptr = NULL;
    assert_false(bc_allocators_arena_allocate(arena, 64, 8, &ptr));

    align_up_fail_from_call = 0;
    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: tracking - arena_create_count increments ===== */

static void test_bc_allocators_arena_tracking_create_count(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_create_count, 0);

    bc_allocators_arena_t* arena1 = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena1));
    assert_non_null(arena1);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_create_count, 1);

    bc_allocators_arena_t* arena2 = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena2));
    assert_non_null(arena2);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_create_count, 2);

    bc_allocators_arena_destroy(arena2);
    bc_allocators_arena_destroy(arena1);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: tracking - arena_destroy_count increments ===== */

static void test_bc_allocators_arena_tracking_destroy_count(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_destroy_count, 0);

    bc_allocators_arena_destroy(arena);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_destroy_count, 1);

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: tracking - arena_active_count reflects create - destroy ===== */

static void test_bc_allocators_arena_tracking_active_count(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    bc_allocators_arena_t* arena1 = NULL;
    bc_allocators_arena_t* arena2 = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena1));
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena2));

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_active_count, 2);

    bc_allocators_arena_destroy(arena1);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_active_count, 1);

    bc_allocators_arena_destroy(arena2);

    assert_true(bc_allocators_context_get_stats(ctx, &stats));
    assert_int_equal(stats.arena_active_count, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: safe_add overflow in arena_create ===== */

static void test_bc_allocators_arena_create_safe_add_overflow(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    safe_add_fail_on_call = safe_add_call_count + 1;

    bc_allocators_arena_t* arena = NULL;
    assert_false(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_null(arena);

    safe_add_fail_on_call = 0;
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: align_up fail on page alignment in arena_create ===== */

static void test_bc_allocators_arena_create_align_up_page(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    align_up_fail_on_call = align_up_call_count + 1;

    bc_allocators_arena_t* arena = NULL;
    assert_false(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_null(arena);

    align_up_fail_on_call = 0;
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: safe_add overflow in try_alloc_in_primary ===== */

static void test_bc_allocators_arena_allocate_safe_add_overflow(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    safe_add_fail_from_call = safe_add_call_count + 1;

    void* ptr = NULL;
    assert_false(bc_allocators_arena_allocate(arena, 64, 8, &ptr));

    safe_add_fail_from_call = 0;
    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: copy_string fails when arena_alloc fails ===== */

static void test_bc_allocators_arena_copy_string_alloc_fail(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 64, &arena));
    assert_non_null(arena);

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 1, &ptr));
    assert_non_null(ptr);

    safe_add_fail_from_call = safe_add_call_count + 1;

    const char* copy = NULL;
    assert_false(bc_allocators_arena_copy_string(arena, "hello", &copy));

    safe_add_fail_from_call = 0;
    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: internal struct field access (proves header resolves) ===== */

static void test_bc_allocators_arena_internal_struct_access(void** state)
{
    (void)state;
    reset_wraps();

    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    assert_ptr_equal(arena->ctx, ctx);
    bc_allocators_arena_stats_t stats;
    assert_true(bc_allocators_arena_get_stats(arena, &stats));
    assert_true(stats.capacity > 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* map failures */
        cmocka_unit_test(test_bc_allocators_arena_create_map_fail),
        /* alignment overflow */
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_up_overflow),
        /* tracking counters */
        cmocka_unit_test(test_bc_allocators_arena_tracking_create_count),
        cmocka_unit_test(test_bc_allocators_arena_tracking_destroy_count),
        cmocka_unit_test(test_bc_allocators_arena_tracking_active_count),
        /* arena_create overflow paths */
        cmocka_unit_test(test_bc_allocators_arena_create_safe_add_overflow),
        cmocka_unit_test(test_bc_allocators_arena_create_align_up_page),
        /* arena_alloc overflow paths */
        cmocka_unit_test(test_bc_allocators_arena_allocate_safe_add_overflow),
        /* copy_string overflow paths */
        cmocka_unit_test(test_bc_allocators_arena_copy_string_alloc_fail),
        /* internal struct access */
        cmocka_unit_test(test_bc_allocators_arena_internal_struct_access),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
