// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_slab.h"
#include "bc_allocators_pool.h"

#include <stdbool.h>

/* ===== Wrap: bc_allocators_pool_allocate ===== */

static int pool_allocate_call_count = 0;
static int pool_allocate_fail_on_call = 0;

bool __real_bc_allocators_pool_allocate(bc_allocators_context_t* ctx, size_t size, void** out_ptr);

bool __wrap_bc_allocators_pool_allocate(bc_allocators_context_t* ctx, size_t size, void** out_ptr)
{
    pool_allocate_call_count++;
    if (pool_allocate_fail_on_call > 0 && pool_allocate_call_count == pool_allocate_fail_on_call) {
        *out_ptr = NULL;
        return false;
    }
    return __real_bc_allocators_pool_allocate(ctx, size, out_ptr);
}

/* ===== Reset helpers ===== */

static void reset_wraps(void)
{
    pool_allocate_call_count = 0;
    pool_allocate_fail_on_call = 0;
}

static bc_allocators_context_t* create_ctx_and_reset(void)
{
    reset_wraps();
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    reset_wraps();
    return ctx;
}

/* ===== create fails on slab struct alloc (call 1) ===== */

static void test_bc_allocators_slab_create_slab_alloc_fail(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx_and_reset();

    pool_allocate_fail_on_call = 1;

    bc_allocators_slab_t* slab = NULL;
    assert_false(bc_allocators_slab_create(ctx, sizeof(size_t), 4, &slab));
    assert_null(slab);

    pool_allocate_fail_on_call = 0;
    bc_allocators_context_destroy(ctx);
}

/* ===== create fails on page struct alloc (call 2) ===== */

static void test_bc_allocators_slab_create_page_alloc_fail(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx_and_reset();

    pool_allocate_fail_on_call = 2;

    bc_allocators_slab_t* slab = NULL;
    assert_false(bc_allocators_slab_create(ctx, sizeof(size_t), 4, &slab));
    assert_null(slab);

    pool_allocate_fail_on_call = 0;
    bc_allocators_context_destroy(ctx);
}

/* ===== create fails on objects alloc (call 3) ===== */

static void test_bc_allocators_slab_create_objects_alloc_fail(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx_and_reset();

    pool_allocate_fail_on_call = 3;

    bc_allocators_slab_t* slab = NULL;
    assert_false(bc_allocators_slab_create(ctx, sizeof(size_t), 4, &slab));
    assert_null(slab);

    pool_allocate_fail_on_call = 0;
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc fails when new page: page struct alloc fails ===== */

static void test_bc_allocators_slab_allocate_new_page_struct_fail(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx_and_reset();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, sizeof(size_t), 2, &slab));

    void* ptrs[2];
    assert_true(bc_allocators_slab_allocate(slab, &ptrs[0]));
    assert_true(bc_allocators_slab_allocate(slab, &ptrs[1]));

    pool_allocate_fail_on_call = pool_allocate_call_count + 1;

    void* ptr = NULL;
    assert_false(bc_allocators_slab_allocate(slab, &ptr));
    assert_null(ptr);

    pool_allocate_fail_on_call = 0;

    bc_allocators_slab_free(slab, ptrs[0]);
    bc_allocators_slab_free(slab, ptrs[1]);
    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc fails when new page: objects alloc fails ===== */

static void test_bc_allocators_slab_allocate_new_page_objects_fail(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx_and_reset();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, sizeof(size_t), 2, &slab));

    void* ptrs[2];
    assert_true(bc_allocators_slab_allocate(slab, &ptrs[0]));
    assert_true(bc_allocators_slab_allocate(slab, &ptrs[1]));

    pool_allocate_fail_on_call = pool_allocate_call_count + 2;

    void* ptr = NULL;
    assert_false(bc_allocators_slab_allocate(slab, &ptr));
    assert_null(ptr);

    pool_allocate_fail_on_call = 0;

    bc_allocators_slab_free(slab, ptrs[0]);
    bc_allocators_slab_free(slab, ptrs[1]);
    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bc_allocators_slab_create_slab_alloc_fail),
        cmocka_unit_test(test_bc_allocators_slab_create_page_alloc_fail),
        cmocka_unit_test(test_bc_allocators_slab_create_objects_alloc_fail),
        cmocka_unit_test(test_bc_allocators_slab_allocate_new_page_struct_fail),
        cmocka_unit_test(test_bc_allocators_slab_allocate_new_page_objects_fail),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
