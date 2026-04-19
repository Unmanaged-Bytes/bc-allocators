// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_arena.h"
#include "bc_allocators_pool.h"
#include "bc_allocators_slab.h"

#include "bc_core.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== Helpers ===== */

static bc_allocators_context_t* create_default_ctx(void)
{
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    return ctx;
}

/* ===== Pool alignment: 16B allocation is aligned to 32 bytes ===== */

static void test_pool_alloc_16B_is_aligned_32(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 32, 0);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Pool alignment: 32B allocation is aligned to 32 bytes ===== */

static void test_pool_alloc_32B_is_aligned_32(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 32, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 32, 0);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Pool alignment: 64B allocation is aligned to 64 bytes ===== */

static void test_pool_alloc_64B_is_aligned_64(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Pool alignment: 128B allocation is aligned to 64 bytes ===== */

static void test_pool_alloc_128B_is_aligned_64(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 128, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Pool alignment: 4096B allocation is aligned to 4096 bytes ===== */

static void test_pool_alloc_4096B_is_aligned_4096(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 4096, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 4096, 0);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Arena alignment: alignment=1 ===== */

static void test_arena_alloc_alignment_1(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, BC_ALLOCATORS_ARENA_SMALL_CAPACITY, &arena));
    assert_non_null(arena);

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 1, &ptr));
    assert_non_null(ptr);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Arena alignment: alignment=16 ===== */

static void test_arena_alloc_alignment_16(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, BC_ALLOCATORS_ARENA_SMALL_CAPACITY, &arena));
    assert_non_null(arena);

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 16, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 16, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Arena alignment: alignment=32 ===== */

static void test_arena_alloc_alignment_32(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, BC_ALLOCATORS_ARENA_SMALL_CAPACITY, &arena));
    assert_non_null(arena);

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 32, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 32, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Arena alignment: alignment=64 ===== */

static void test_arena_alloc_alignment_64(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, BC_ALLOCATORS_ARENA_SMALL_CAPACITY, &arena));
    assert_non_null(arena);

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 64, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Slab alignment: object_size=32 is aligned to 32 bytes ===== */

static void test_slab_alloc_32B_is_aligned_32(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, 32, 64, &slab));
    assert_non_null(slab);

    void* ptr = NULL;
    assert_true(bc_allocators_slab_allocate(slab, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 32, 0);

    bc_allocators_slab_free(slab, ptr);
    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== Slab alignment: object_size=64 is aligned to 64 bytes ===== */

static void test_slab_alloc_64B_is_aligned_64(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, 64, 64, &slab));
    assert_non_null(slab);

    void* ptr = NULL;
    assert_true(bc_allocators_slab_allocate(slab, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);

    bc_allocators_slab_free(slab, ptr);
    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== Pool 4KB + bc_core_fill: no ASan fault (alignment validation) ===== */

static void test_pool_alloc_then_bc_core_fill_no_asan_fault(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 4096, &ptr));
    assert_non_null(ptr);

    bc_core_fill(ptr, 4096, (unsigned char)0xAB);
    assert_int_equal(((unsigned char*)ptr)[0], 0xAB);
    assert_int_equal(((unsigned char*)ptr)[4095], 0xAB);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* pool alignment */
        cmocka_unit_test(test_pool_alloc_16B_is_aligned_32),
        cmocka_unit_test(test_pool_alloc_32B_is_aligned_32),
        cmocka_unit_test(test_pool_alloc_64B_is_aligned_64),
        cmocka_unit_test(test_pool_alloc_128B_is_aligned_64),
        cmocka_unit_test(test_pool_alloc_4096B_is_aligned_4096),
        /* arena alignment */
        cmocka_unit_test(test_arena_alloc_alignment_1),
        cmocka_unit_test(test_arena_alloc_alignment_16),
        cmocka_unit_test(test_arena_alloc_alignment_32),
        cmocka_unit_test(test_arena_alloc_alignment_64),
        /* slab alignment */
        cmocka_unit_test(test_slab_alloc_32B_is_aligned_32),
        cmocka_unit_test(test_slab_alloc_64B_is_aligned_64),
        /* integration: pool alloc + bc_core_fill */
        cmocka_unit_test(test_pool_alloc_then_bc_core_fill_no_asan_fault),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
