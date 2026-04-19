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

static bc_allocators_context_t* create_default_ctx(void)
{
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    return ctx;
}

/* ===== Test: page_class stores direct class ===== */
static void test_bc_allocators_pool_page_class_size16_is_class0(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    /* size=16 -> class=0 -> page_class must be 0 */
    assert_int_equal((int)cls, 0);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_pool_page_class_size64_is_class2(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    /* size=64 -> class=2 -> page_class must be 2 */
    assert_int_equal((int)cls, 2);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_pool_page_class_size32_is_class1(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 32, &ptr));
    assert_non_null(ptr);

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    /* size=32 -> class=1 -> page_class must be 1 */
    assert_int_equal((int)cls, 1);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_pool_page_class_size128_is_class3(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 128, &ptr));
    assert_non_null(ptr);

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    /* size=128 -> class=3 -> page_class must be 3 */
    assert_int_equal((int)cls, 3);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: class 18 (4194304 bytes) goes through pool, not large path ===== */
static void test_bc_allocators_pool_page_class_size4mb_is_class18_not_large(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 4194304, &ptr));
    assert_non_null(ptr);

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    /* size=4194304 -> class=18 -> page_class must be 18 (not BC_ALLOCATORS_PAGE_CLASS_LARGE) */
    assert_int_equal((int)cls, 18);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: ptr IS block start (no header offset) ===== */
static void test_bc_allocators_pool_free_pushes_to_class0_not_class1(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);

    bc_allocators_pool_free(ctx, ptr);

    /* size=16 -> class=0: free pushes block to pools[0] */
    assert_non_null(ctx->pools[0].head);

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: 16-byte blocks use class 0, not class 1 ===== */
static void test_bc_allocators_pool_free_size16_does_not_use_pools1(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);
    bc_allocators_pool_free(ctx, ptr);

    /* size=16 -> class=0: pools[1] must remain empty */
    assert_null(ctx->pools[1].head);

    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* page_class value checks */
        cmocka_unit_test(test_bc_allocators_pool_page_class_size16_is_class0),
        cmocka_unit_test(test_bc_allocators_pool_page_class_size32_is_class1),
        cmocka_unit_test(test_bc_allocators_pool_page_class_size64_is_class2),
        cmocka_unit_test(test_bc_allocators_pool_page_class_size128_is_class3),
        /* class 18 (4194304) must not route to large path */
        cmocka_unit_test(test_bc_allocators_pool_page_class_size4mb_is_class18_not_large),
        /* free-list slot checks */
        cmocka_unit_test(test_bc_allocators_pool_free_pushes_to_class0_not_class1),
        cmocka_unit_test(test_bc_allocators_pool_free_size16_does_not_use_pools1),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
