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

/* Size larger than the max class size (262144) to trigger large path */
#define LARGE_SIZE (BC_ALLOCATORS_MAX_CLASS_SIZE + 4096)

/* ===== allocate large block ===== */

static void test_bc_allocators_pool_allocate_large(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptr));
    assert_non_null(ptr);

    /* Write and read back pattern to verify usability */
    bc_core_fill(ptr, LARGE_SIZE, (unsigned char)0xBE);
    assert_int_equal(((unsigned char*)ptr)[0], 0xBE);
    assert_int_equal(((unsigned char*)ptr)[LARGE_SIZE - 1], 0xBE);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== free large triggers MADV_FREE/DONTNEED ===== */

static void test_bc_allocators_pool_free_large(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptr));
    assert_non_null(ptr);

    /* Free should not crash and should release via madvise */
    bc_allocators_pool_free(ctx, ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== allocate large again reuses from free-list ===== */

static void test_bc_allocators_pool_large_reuse_from_free_list(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr1 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptr1));
    assert_non_null(ptr1);

    bc_allocators_pool_free(ctx, ptr1);

    /* Second allocation should reuse from large_pool free-list */
    void* ptr2 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptr2));
    assert_non_null(ptr2);

    bc_allocators_pool_free(ctx, ptr2);
    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate large via mremap preserves data ===== */

static void test_bc_allocators_pool_reallocate_large(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptr));
    assert_non_null(ptr);

    /* Write a known pattern */
    bc_core_fill(ptr, LARGE_SIZE, (unsigned char)0xAA);

    /* Reallocate to a bigger large size */
    size_t bigger_size = LARGE_SIZE * 2;
    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, bigger_size, &new_ptr));
    assert_non_null(new_ptr);

    /* Old data must be preserved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0xAA);
    assert_int_equal(((unsigned char*)new_ptr)[LARGE_SIZE - 1], 0xAA);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== multiple large allocations ===== */

static void test_bc_allocators_pool_multiple_large_allocations(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Allocate several large blocks */
    void* ptrs[4];
    for (int i = 0; i < 4; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptrs[i]));
        assert_non_null(ptrs[i]);
        bc_core_fill(ptrs[i], LARGE_SIZE, (unsigned char)(int)(0xC0 + i));
    }

    /* Verify each block has its own pattern */
    for (int i = 0; i < 4; i++) {
        assert_int_equal(((unsigned char*)ptrs[i])[0], 0xC0 + i);
        assert_int_equal(((unsigned char*)ptrs[i])[LARGE_SIZE - 1], 0xC0 + i);
    }

    for (int i = 0; i < 4; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate from small class to large ===== */

static void test_bc_allocators_pool_reallocate_small_to_large(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 64, (unsigned char)0xBB);

    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, LARGE_SIZE, &new_ptr));
    assert_non_null(new_ptr);

    /* First 64 bytes of old data must be preserved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0xBB);
    assert_int_equal(((unsigned char*)new_ptr)[63], 0xBB);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate from large to small class ===== */

static void test_bc_allocators_pool_reallocate_large_to_small(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, LARGE_SIZE, (unsigned char)0x77);

    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, 64, &new_ptr));
    assert_non_null(new_ptr);

    /* First 64 bytes of old data must be preserved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0x77);
    assert_int_equal(((unsigned char*)new_ptr)[63], 0x77);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== large in-place shrink realloc ===== */

static void test_bc_allocators_pool_large_shrink_realloc(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Allocate a large block of LARGE_SIZE * 2 */
    size_t big_size = LARGE_SIZE * 2;
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, big_size, &ptr));
    assert_non_null(ptr);

    /* Write a known pattern */
    bc_core_fill(ptr, big_size, (unsigned char)0xAF);

    /* Reallocate to LARGE_SIZE (still large, but smaller -- in-place shrink) */
    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, LARGE_SIZE, &new_ptr));
    assert_non_null(new_ptr);

    /* Data must be preserved in the shrunk region */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0xAF);
    assert_int_equal(((unsigned char*)new_ptr)[LARGE_SIZE - 1], 0xAF);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== large reuse too small pushes back and bump-allocates ===== */

static void test_bc_allocators_pool_large_reuse_too_small(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Allocate a large block of LARGE_SIZE, then free it */
    void* ptr1 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, LARGE_SIZE, &ptr1));
    assert_non_null(ptr1);
    bc_allocators_pool_free(ctx, ptr1);

    /* Allocate a much larger block (LARGE_SIZE * 4).
       The freed block is too small, so it gets pushed back
       and a new block is bump-allocated. */
    size_t much_larger = LARGE_SIZE * 4;
    void* ptr2 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, much_larger, &ptr2));
    assert_non_null(ptr2);

    /* Write pattern to verify usability */
    bc_core_fill(ptr2, much_larger, (unsigned char)0xDA);
    assert_int_equal(((unsigned char*)ptr2)[0], 0xDA);
    assert_int_equal(((unsigned char*)ptr2)[much_larger - 1], 0xDA);

    bc_allocators_pool_free(ctx, ptr2);
    /* The original small block may still be in large_pool; destroy cleans up */
    bc_allocators_context_destroy(ctx);
}

/* ===== large in-place shrink with tracking disabled (bc_allocators_pool.c:440) ===== */

static void test_bc_allocators_pool_large_shrink_realloc_tracking_disabled(void** state)
{
    (void)state;

    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = false,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    /* Allocate a large block of big_size */
    size_t big_size = LARGE_SIZE * 2;
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, big_size, &ptr));
    assert_non_null(ptr);

    bc_core_fill(ptr, big_size, (unsigned char)0x5A);

    /* Reallocate to LARGE_SIZE (still large, in-place shrink).
       With tracking disabled: if(ctx->tracking_enabled) branch at :440 is false */
    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, LARGE_SIZE, &new_ptr));
    assert_non_null(new_ptr);

    /* Data preserved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0x5A);
    assert_int_equal(((unsigned char*)new_ptr)[LARGE_SIZE - 1], 0x5A);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bc_allocators_pool_allocate_large),
        cmocka_unit_test(test_bc_allocators_pool_free_large),
        cmocka_unit_test(test_bc_allocators_pool_large_reuse_from_free_list),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_large),
        cmocka_unit_test(test_bc_allocators_pool_multiple_large_allocations),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_small_to_large),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_large_to_small),
        cmocka_unit_test(test_bc_allocators_pool_large_shrink_realloc),
        cmocka_unit_test(test_bc_allocators_pool_large_reuse_too_small),
        cmocka_unit_test(test_bc_allocators_pool_large_shrink_realloc_tracking_disabled),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
