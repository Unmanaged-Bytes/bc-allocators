// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_arena.h"
#include "bc_allocators_pool.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool pool_leak_seen;
    bool arena_leak_seen;
} leak_signals_t;

static void capture_leaks(const char* message, void* user_argument)
{
    leak_signals_t* signals = user_argument;
    if (strstr(message, "pool leak") != NULL) {
        signals->pool_leak_seen = true;
    }
    if (strstr(message, "arena leak") != NULL) {
        signals->arena_leak_seen = true;
    }
}

static bc_allocators_context_t* create_tracking_ctx(leak_signals_t* signals)
{
    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = true,
        .leak_callback = capture_leaks,
        .leak_callback_argument = signals,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);
    return ctx;
}

static void test_bc_allocators_leak_detection_arena_leak(void** state)
{
    (void)state;
    leak_signals_t signals = {0};
    bc_allocators_context_t* ctx = create_tracking_ctx(&signals);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    bc_allocators_context_destroy(ctx);

    assert_true(signals.arena_leak_seen);
    assert_false(signals.pool_leak_seen);
}

static void test_bc_allocators_leak_detection_pool_leak(void** state)
{
    (void)state;
    leak_signals_t signals = {0};
    bc_allocators_context_t* ctx = create_tracking_ctx(&signals);

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);

    bc_allocators_context_destroy(ctx);

    assert_true(signals.pool_leak_seen);
    assert_false(signals.arena_leak_seen);
}

static void test_bc_allocators_leak_detection_no_leak(void** state)
{
    (void)state;
    leak_signals_t signals = {0};
    bc_allocators_context_t* ctx = create_tracking_ctx(&signals);

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    bc_allocators_pool_free(ctx, ptr);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);
    bc_allocators_arena_destroy(arena);

    bc_allocators_context_destroy(ctx);

    assert_false(signals.pool_leak_seen);
    assert_false(signals.arena_leak_seen);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bc_allocators_leak_detection_arena_leak),
        cmocka_unit_test(test_bc_allocators_leak_detection_pool_leak),
        cmocka_unit_test(test_bc_allocators_leak_detection_no_leak),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
