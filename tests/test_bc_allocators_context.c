// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_context_internal.h"

#include "bc_core.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ===== context_create with NULL config (defaults) ===== */

static void test_bc_allocators_context_create_null_config(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);
    bc_allocators_context_destroy(ctx);
}

/* ===== context_create with explicit config ===== */

static void test_bc_allocators_context_create_explicit_config(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {
        .max_pool_memory = 32 * 1024 * 1024,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);
    bc_allocators_context_destroy(ctx);
}

/* ===== context_create with tracking disabled ===== */

static void test_bc_allocators_context_create_tracking_disabled(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = false,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);
    bc_allocators_context_destroy(ctx);
}

/* ===== context_create with max_pool_memory 0 (default 1 GB) ===== */

static void test_bc_allocators_context_create_zero_max_memory(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);
    bc_allocators_context_destroy(ctx);
}

/* ===== context_create SIZE_MAX overflow ===== */

static void test_bc_allocators_context_create_size_max_overflow(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {
        .max_pool_memory = SIZE_MAX,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(&config, &ctx));
    assert_null(ctx);
}

/* ===== Hardware getters ===== */

static void test_bc_allocators_context_page_size_positive(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t page_size = 0;
    assert_true(bc_allocators_context_page_size(ctx, &page_size));
    assert_true(page_size > 0);
    /* page_size must be a power of 2 */
    assert_int_equal(page_size & (page_size - 1), 0);

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_context_cache_line_size_positive(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t cls = 0;
    assert_true(bc_allocators_context_cache_line_size(ctx, &cls));
    assert_true(cls > 0);
    /* Must be a reasonable value (typically 32, 64, or 128) */
    assert_true(cls >= 16 && cls <= 256);

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_context_page_size_stable(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t ps1 = 0;
    size_t ps2 = 0;
    assert_true(bc_allocators_context_page_size(ctx, &ps1));
    assert_true(bc_allocators_context_page_size(ctx, &ps2));
    assert_int_equal(ps1, ps2);

    bc_allocators_context_destroy(ctx);
}

/* ===== Get stats with tracking enabled (debug-only) ===== */

static void test_bc_allocators_context_get_stats_tracking_enabled(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    bc_allocators_stats_t stats = {0};
    assert_true(bc_allocators_context_get_stats(ctx, &stats));

    /* Fresh context: all counters at zero */
    assert_int_equal(stats.pool_alloc_count, 0);
    assert_int_equal(stats.pool_free_count, 0);
    assert_int_equal(stats.pool_active_bytes, 0);
    assert_int_equal(stats.arena_create_count, 0);
    assert_int_equal(stats.arena_destroy_count, 0);
    assert_int_equal(stats.arena_active_count, 0);
    assert_int_equal(stats.arena_total_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== Get stats with tracking disabled ===== */

static void test_bc_allocators_context_get_stats_tracking_disabled(void** state)
{
    (void)state;
    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = false,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    bc_allocators_stats_t stats;
    bc_core_fill(&stats, sizeof(stats), (unsigned char)0xFF);
    assert_true(bc_allocators_context_get_stats(ctx, &stats));

    /* All fields must be zero when tracking is disabled */
    assert_int_equal(stats.pool_alloc_count, 0);
    assert_int_equal(stats.pool_free_count, 0);
    assert_int_equal(stats.pool_active_bytes, 0);
    assert_int_equal(stats.arena_create_count, 0);
    assert_int_equal(stats.arena_destroy_count, 0);
    assert_int_equal(stats.arena_active_count, 0);
    assert_int_equal(stats.arena_total_bytes, 0);
    assert_int_equal(stats.total_mapped_bytes, 0);
    assert_int_equal(stats.peak_mapped_bytes, 0);

    bc_allocators_context_destroy(ctx);
}

/* ===== Leak detection on destroy ===== */

/* To test leak detection, we access the internal context struct
   and manipulate the tracking counters directly. This simulates
   a scenario where allocations were made but not freed. */

typedef struct {
    char captured_message[256];
    size_t call_count;
} leak_capture_t;

static void leak_capture_callback(const char* message, void* user_argument)
{
    leak_capture_t* capture = user_argument;
    size_t max_length = sizeof(capture->captured_message) - 1;
    size_t i = 0;
    while (i < max_length && message[i] != '\0') {
        capture->captured_message[i] = message[i];
        i++;
    }
    capture->captured_message[i] = '\0';
    capture->call_count++;
}

static void test_bc_allocators_context_destroy_detects_pool_leak(void** state)
{
    (void)state;
    leak_capture_t capture = {0};
    bc_allocators_context_config_t config = {
        .tracking_enabled = true,
        .leak_callback = leak_capture_callback,
        .leak_callback_argument = &capture,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    ctx->pool_alloc_count = 5;
    ctx->pool_free_count = 3;

    bc_allocators_context_destroy(ctx);

    assert_int_equal(capture.call_count, 1);
    assert_non_null(strstr(capture.captured_message, "pool leak detected"));
}

static void test_bc_allocators_context_destroy_detects_arena_leak(void** state)
{
    (void)state;
    leak_capture_t capture = {0};
    bc_allocators_context_config_t config = {
        .tracking_enabled = true,
        .leak_callback = leak_capture_callback,
        .leak_callback_argument = &capture,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    ctx->arena_create_count = 3;
    ctx->arena_destroy_count = 1;

    bc_allocators_context_destroy(ctx);

    assert_int_equal(capture.call_count, 1);
    assert_non_null(strstr(capture.captured_message, "arena leak detected"));
}

static void test_bc_allocators_context_destroy_no_leak_no_output(void** state)
{
    (void)state;
    leak_capture_t capture = {0};
    bc_allocators_context_config_t config = {
        .tracking_enabled = true,
        .leak_callback = leak_capture_callback,
        .leak_callback_argument = &capture,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    bc_allocators_context_destroy(ctx);

    assert_int_equal(capture.call_count, 0);
}

static void test_bc_allocators_context_destroy_null_callback_silent(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    ctx->pool_alloc_count = 5;
    ctx->pool_free_count = 3;

    bc_allocators_context_destroy(ctx);
}

/* ===== Tracking counters are plain size_t (not _Atomic) after refactor ===== */
/* RED: fails to compile against current _Atomic size_t counters because
   &ctx->pool_alloc_count has type _Atomic(size_t) * which is incompatible with size_t *.
   GREEN: compiles after refactor removes _Atomic from tracking counters. */

static void test_bc_allocators_context_counters_plain_type(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    /* Take addresses of counters as plain size_t * — valid only when fields are plain size_t */
    size_t* alloc_ptr = &ctx->pool_alloc_count;
    size_t* free_ptr = &ctx->pool_free_count;
    assert_non_null(alloc_ptr);
    assert_non_null(free_ptr);

    bc_allocators_context_destroy(ctx);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* context create */
        cmocka_unit_test(test_bc_allocators_context_create_null_config),
        cmocka_unit_test(test_bc_allocators_context_create_explicit_config),
        cmocka_unit_test(test_bc_allocators_context_create_tracking_disabled),
        cmocka_unit_test(test_bc_allocators_context_create_zero_max_memory),
        cmocka_unit_test(test_bc_allocators_context_create_size_max_overflow),
        /* hardware getters */
        cmocka_unit_test(test_bc_allocators_context_page_size_positive),
        cmocka_unit_test(test_bc_allocators_context_cache_line_size_positive),
        cmocka_unit_test(test_bc_allocators_context_page_size_stable),
        cmocka_unit_test(test_bc_allocators_context_get_stats_tracking_enabled),
        cmocka_unit_test(test_bc_allocators_context_get_stats_tracking_disabled),
        /* leak detection */
        cmocka_unit_test(test_bc_allocators_context_destroy_detects_pool_leak),
        cmocka_unit_test(test_bc_allocators_context_destroy_detects_arena_leak),
        cmocka_unit_test(test_bc_allocators_context_destroy_no_leak_no_output),
        cmocka_unit_test(test_bc_allocators_context_destroy_null_callback_silent),
        /* counters plain type */
        cmocka_unit_test(test_bc_allocators_context_counters_plain_type),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
