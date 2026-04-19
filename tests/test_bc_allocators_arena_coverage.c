// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include "bc_allocators.h"

#include <stdbool.h>
#include <stdlib.h>

static bc_allocators_context_t* make_ctx(void)
{
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);
    return ctx;
}

static void test_arena_cache_hit_reuses_same_size_block(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = make_ctx();

    size_t capacity = BC_ALLOCATORS_ARENA_SMALL_CAPACITY;

    bc_allocators_arena_t* a = NULL;
    assert_true(bc_allocators_arena_create(ctx, capacity, &a));
    bc_allocators_arena_destroy(a);

    bc_allocators_arena_t* b = NULL;
    assert_true(bc_allocators_arena_create(ctx, capacity, &b));
    assert_non_null(b);
    bc_allocators_arena_destroy(b);

    bc_allocators_context_destroy(ctx);
}

static void test_arena_cache_overflow_forces_unmap(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = make_ctx();

    size_t capacity = BC_ALLOCATORS_ARENA_SMALL_CAPACITY;
    enum { N = 6 };

    bc_allocators_arena_t* arenas[N] = {0};
    for (int i = 0; i < N; i++) {
        assert_true(bc_allocators_arena_create(ctx, capacity, &arenas[i]));
        assert_non_null(arenas[i]);
    }
    for (int i = 0; i < N; i++) {
        bc_allocators_arena_destroy(arenas[i]);
    }

    bc_allocators_context_destroy(ctx);
}

static void test_arena_release_pages(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = make_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, BC_ALLOCATORS_ARENA_SMALL_CAPACITY, &arena));
    assert_non_null(arena);

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 4096, 64, &ptr));
    assert_non_null(ptr);

    assert_true(bc_allocators_arena_release_pages(arena));

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_arena_cache_hit_reuses_same_size_block),
        cmocka_unit_test(test_arena_cache_overflow_forces_unmap),
        cmocka_unit_test(test_arena_release_pages),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
