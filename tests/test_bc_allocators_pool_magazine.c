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

/* ===== Helpers ===== */

static bc_allocators_context_t* create_default_ctx(void)
{
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    return ctx;
}

/* Drain the magazine pre-populated by the first bump_allocate_class call.
 * After the first alloc (from bump), BC_ALLOCATORS_MAG_SIZE blocks land in the
 * magazine and the rest go to the intrusive list.  Allocating MAG_SIZE more
 * empties the magazine; the next alloc comes from the intrusive list. */
static void drain_magazine(bc_allocators_context_t* ctx, void** drain, int n)
{
    for (int i = 0; i < n; i++) {
        drain[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 16, &drain[i]));
        assert_non_null(drain[i]);
    }
}

/* ===== Test 1: fill magazine, drain, fill again ===== */

static void test_magazine_fill(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Alloc BC_ALLOCATORS_MAG_SIZE+1 blocks: 1 from bump, MAG_SIZE from magazine */
    void* ptrs[BC_ALLOCATORS_MAG_SIZE + 1];
    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 16, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    /* Free all — magazine fills, remainder spills to intrusive */
    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    /* Re-allocate: must succeed (from magazine + intrusive) */
    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 16, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }
    bc_allocators_context_destroy(ctx);
}

/* ===== Test 2: overflow — MAG_SIZE+1 frees, alloc still works ===== */

static void test_magazine_overflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Drain the bump-populated magazine so it starts empty */
    void* drain[BC_ALLOCATORS_MAG_SIZE + 1];
    drain_magazine(ctx, drain, BC_ALLOCATORS_MAG_SIZE + 1);

    /* Now free MAG_SIZE+1 blocks: first MAG_SIZE fill the magazine,
     * the last one overflows to the intrusive list */
    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        bc_allocators_pool_free(ctx, drain[i]);
    }

    /* All blocks must be re-allocatable (magazine + intrusive paths) */
    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        drain[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 16, &drain[i]));
        assert_non_null(drain[i]);
        bc_allocators_pool_free(ctx, drain[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== Test 3: underflow — magazine empty, intrusive path used ===== */

static void test_magazine_underflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Alloc MAG_SIZE+2: 1 from bump, MAG_SIZE from magazine, 1 from intrusive */
    void* ptrs[BC_ALLOCATORS_MAG_SIZE + 2];
    for (int i = 0; i < BC_ALLOCATORS_MAG_SIZE + 2; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 16, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    /* All blocks must be distinct */
    for (int i = 0; i < BC_ALLOCATORS_MAG_SIZE + 2; i++) {
        for (int j = i + 1; j < BC_ALLOCATORS_MAG_SIZE + 2; j++) {
            assert_ptr_not_equal(ptrs[i], ptrs[j]);
        }
    }

    for (int i = 0; i < BC_ALLOCATORS_MAG_SIZE + 2; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }
    bc_allocators_context_destroy(ctx);
}

/* ===== Test 4: LIFO order preserved ===== */

static void test_magazine_lifo_order(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Drain initial magazine so the next allocs come from the intrusive list */
    void* drain[BC_ALLOCATORS_MAG_SIZE + 1];
    drain_magazine(ctx, drain, BC_ALLOCATORS_MAG_SIZE + 1);

    /* Allocate 3 distinct blocks from the intrusive list */
    void *a = NULL, *b = NULL, *c = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &a));
    assert_true(bc_allocators_pool_allocate(ctx, 16, &b));
    assert_true(bc_allocators_pool_allocate(ctx, 16, &c));
    assert_ptr_not_equal(a, b);
    assert_ptr_not_equal(b, c);
    assert_ptr_not_equal(a, c);

    /* Free A, B, C — must come back as C, B, A (LIFO) */
    bc_allocators_pool_free(ctx, a);
    bc_allocators_pool_free(ctx, b);
    bc_allocators_pool_free(ctx, c);

    void *r1 = NULL, *r2 = NULL, *r3 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &r1));
    assert_true(bc_allocators_pool_allocate(ctx, 16, &r2));
    assert_true(bc_allocators_pool_allocate(ctx, 16, &r3));

    assert_ptr_equal(r1, c);
    assert_ptr_equal(r2, b);
    assert_ptr_equal(r3, a);

    bc_allocators_pool_free(ctx, r1);
    bc_allocators_pool_free(ctx, r2);
    bc_allocators_pool_free(ctx, r3);
    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        bc_allocators_pool_free(ctx, drain[i]);
    }
    bc_allocators_context_destroy(ctx);
}

/* ===== Test 5: RED — free must NOT overwrite block content ===== */

/* With the intrusive free-list, free_list_push writes the next-pointer at
 * offset 0 of the freed block, destroying whatever the caller stored there.
 * The magazine cache stores pointers in a separate hot array so the block is
 * never touched on free.  This test is RED with the intrusive implementation
 * and GREEN after the magazine is in place. */

static void test_magazine_block_content_preserved(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Drain the bump-populated magazine so it is empty before the test */
    void* drain[BC_ALLOCATORS_MAG_SIZE + 1];
    drain_magazine(ctx, drain, BC_ALLOCATORS_MAG_SIZE + 1);

    /* Allocate a block that comes from the intrusive list (magazine is empty) */
    void* target = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &target));
    assert_non_null(target);

    /* Write a known pattern into every byte of the block */
    bc_core_fill(target, 16, (unsigned char)0xAB);

    /* Free the block — magazine has room: must NOT overwrite block content */
    bc_allocators_pool_free(ctx, target);

    /* LIFO: immediate alloc of same class must return the same block */
    void* returned = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &returned));
    assert_ptr_equal(returned, target);

    /* Verify all bytes are intact — fails with intrusive (bytes[0..7] overwritten) */
    unsigned char* bytes = (unsigned char*)returned;
    assert_int_equal(bytes[0], 0xAB);
    assert_int_equal(bytes[7], 0xAB);
    assert_int_equal(bytes[15], 0xAB);

    bc_allocators_pool_free(ctx, returned);
    for (int i = 0; i <= BC_ALLOCATORS_MAG_SIZE; i++) {
        bc_allocators_pool_free(ctx, drain[i]);
    }
    bc_allocators_context_destroy(ctx);
}

/* ===== Test 6: class isolation — class 0 ops don't affect class 1 ===== */

static void test_magazine_class_isolation(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Alloc from class 0 (16B) and class 1 (32B) */
    void *p16 = NULL, *p32 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &p16));
    assert_true(bc_allocators_pool_allocate(ctx, 32, &p32));
    assert_non_null(p16);
    assert_non_null(p32);

    /* Cycle BC_ALLOCATORS_MAG_SIZE allocs/frees on class 0 */
    void* cls0[BC_ALLOCATORS_MAG_SIZE];
    for (int i = 0; i < BC_ALLOCATORS_MAG_SIZE; i++) {
        cls0[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 16, &cls0[i]));
        assert_non_null(cls0[i]);
    }
    for (int i = 0; i < BC_ALLOCATORS_MAG_SIZE; i++) {
        bc_allocators_pool_free(ctx, cls0[i]);
    }

    /* Class 1 must be unaffected: alloc still works and returns a valid pointer */
    void* p32b = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 32, &p32b));
    assert_non_null(p32b);
    assert_ptr_not_equal(p32b, p16); /* cross-class blocks are always distinct */

    bc_allocators_pool_free(ctx, p16);
    bc_allocators_pool_free(ctx, p32);
    bc_allocators_pool_free(ctx, p32b);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_magazine_fill),
        cmocka_unit_test(test_magazine_overflow),
        cmocka_unit_test(test_magazine_underflow),
        cmocka_unit_test(test_magazine_lifo_order),
        cmocka_unit_test(test_magazine_block_content_preserved),
        cmocka_unit_test(test_magazine_class_isolation),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
