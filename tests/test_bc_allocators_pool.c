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

/* ===== allocate size 1 maps to class 16 ===== */

static void test_bc_allocators_pool_allocate_size_one(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 1, &ptr));
    assert_non_null(ptr);

    /* Write and read back to verify the block is usable */
    bc_core_fill(ptr, 1, (unsigned char)0xAB);
    assert_int_equal(((unsigned char*)ptr)[0], 0xAB);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== allocate each of the 19 size classes ===== */

static void test_bc_allocators_pool_allocate_all_size_classes(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    static const size_t class_sizes[BC_ALLOCATORS_NUM_CLASSES] = {
        16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304,
    };

    void* ptrs[BC_ALLOCATORS_NUM_CLASSES];

    for (size_t i = 0; i < BC_ALLOCATORS_NUM_CLASSES; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, class_sizes[i], &ptrs[i]));
        assert_non_null(ptrs[i]);

        /* Write first and last byte to verify the block is usable */
        bc_core_fill(ptrs[i], class_sizes[i], (unsigned char)(int)(i & 0xFF));
        assert_int_equal(((unsigned char*)ptrs[i])[0], (int)(i & 0xFF));
        assert_int_equal(((unsigned char*)ptrs[i])[class_sizes[i] - 1], (int)(i & 0xFF));
    }

    for (size_t i = 0; i < BC_ALLOCATORS_NUM_CLASSES; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== allocate + free cycle ===== */

static void test_bc_allocators_pool_allocate_free_cycle(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    for (int i = 0; i < 100; i++) {
        void* ptr = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
        assert_non_null(ptr);
        bc_core_fill(ptr, 64, (unsigned char)0x42);
        bc_allocators_pool_free(ctx, ptr);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== allocate many small to fill free-list and trigger batch bump ===== */

static void test_bc_allocators_pool_allocate_many_trigger_batch_flush(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Allocate more than CACHE_SIZE (8) blocks to trigger slow path */
    void* ptrs[32];
    for (int i = 0; i < 32; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 32, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    for (int i = 0; i < 32; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== free many then reallocate from free-list ===== */

static void test_bc_allocators_pool_free_many_trigger_batch_fill(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Allocate, free all (fills free-list), then allocate again (reuse) */
    void* ptrs[32];
    for (int i = 0; i < 32; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 64, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    /* Free all -- pushes to free-list */
    for (int i = 0; i < 32; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    /* Allocate again -- should reuse from free-list */
    for (int i = 0; i < 32; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, 64, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    for (int i = 0; i < 32; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate same class may keep same ptr ===== */

static void test_bc_allocators_pool_reallocate_same_class(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 16, (unsigned char)0xCC);

    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, 16, &new_ptr));
    assert_non_null(new_ptr);

    /* Data must be preserved regardless of whether ptr moved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0xCC);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate to bigger class moves data ===== */

static void test_bc_allocators_pool_reallocate_bigger_class(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 16, (unsigned char)0xDD);

    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, 256, &new_ptr));
    assert_non_null(new_ptr);

    /* First 16 bytes of old data must be preserved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0xDD);
    assert_int_equal(((unsigned char*)new_ptr)[15], 0xDD);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate with NULL ptr is equivalent to allocate ===== */

static void test_bc_allocators_pool_reallocate_null_ptr(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, NULL, 64, &ptr));
    assert_non_null(ptr);

    bc_core_fill(ptr, 64, (unsigned char)0xEE);
    assert_int_equal(((unsigned char*)ptr)[0], 0xEE);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== free ptr outside region is a no-op ===== */

static void test_bc_allocators_pool_free_outside_region(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Stack variable -- definitely outside region */
    int stack_var = 42;

    /* Must not crash */
    bc_allocators_pool_free(ctx, &stack_var);

    bc_allocators_context_destroy(ctx);
}

/* ===== allocate with size 0 returns false ===== */

static void test_bc_allocators_pool_allocate_size_zero(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_false(bc_allocators_pool_allocate(ctx, 0, &ptr));
    assert_null(ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate with size 0 returns false ===== */

static void test_bc_allocators_pool_reallocate_size_zero(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);

    void* new_ptr = NULL;
    assert_false(bc_allocators_pool_reallocate(ctx, ptr, 0, &new_ptr));
    assert_null(new_ptr);

    /* ptr must still be valid (not freed on failure) */
    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== context switch flushes TLS of previous ctx ===== */

static void test_bc_allocators_pool_context_switch(void** state)
{
    (void)state;

    /* Create ctx1, allocate from it (active_ctx = ctx1) */
    bc_allocators_context_t* ctx1 = create_default_ctx();
    void* ptr1 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx1, 64, &ptr1));
    assert_non_null(ptr1);

    /* Create ctx2, allocate from it (triggers context switch: flush ctx1 TLS) */
    bc_allocators_context_t* ctx2 = create_default_ctx();
    void* ptr2 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx2, 64, &ptr2));
    assert_non_null(ptr2);

    /* Free from ctx2, then free from ctx1 (switch back to ctx1) */
    bc_allocators_pool_free(ctx2, ptr2);
    bc_allocators_pool_free(ctx1, ptr1);

    bc_allocators_context_destroy(ctx2);
    bc_allocators_context_destroy(ctx1);
}

/* ===== free small block: cls valid (1-14), fallthrough free_list_push path ===== */

static void test_bc_allocators_pool_free_small_block_valid_cls(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Allocate from each size class and free -- ensures cls 1-14 path is hit.
       The guard (cls <= 0 || cls >= NUM_CLASSES) is false => free_list_push executes. */
    static const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024};
    void* ptrs[7];
    for (int i = 0; i < 7; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_pool_allocate(ctx, sizes[i], &ptrs[i]));
        assert_non_null(ptrs[i]);
        bc_core_fill(ptrs[i], sizes[i], (unsigned char)(int)(0x10 + i));
    }

    /* Free all: each goes through the valid-cls path */
    for (int i = 0; i < 7; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== multiple contexts: destroy one while other is active ===== */

static void test_bc_allocators_pool_multiple_contexts(void** state)
{
    (void)state;

    bc_allocators_context_t* ctx1 = create_default_ctx();
    void* ptr1 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx1, 64, &ptr1));
    assert_non_null(ptr1);

    bc_allocators_context_t* ctx2 = create_default_ctx();
    void* ptr2 = NULL;
    assert_true(bc_allocators_pool_allocate(ctx2, 64, &ptr2));
    assert_non_null(ptr2);

    /* Destroy ctx1 first, ctx2 still active */
    bc_allocators_pool_free(ctx1, ptr1);
    bc_allocators_context_destroy(ctx1);

    /* Free ptr2 and destroy ctx2 */
    bc_allocators_pool_free(ctx2, ptr2);
    bc_allocators_context_destroy(ctx2);
}

/* ===== free pointer outside region is a silent no-op ===== */

static void test_bc_allocators_pool_free_out_of_region(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    /* Stack variable is definitely outside the mmap region */
    int stack_var = 42;

    /* Must not crash -- ptr_in_region returns false, free returns early */
    bc_allocators_pool_free(ctx, &stack_var);

    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* happy path */
        cmocka_unit_test(test_bc_allocators_pool_allocate_size_one),
        cmocka_unit_test(test_bc_allocators_pool_allocate_all_size_classes),
        cmocka_unit_test(test_bc_allocators_pool_allocate_free_cycle),
        cmocka_unit_test(test_bc_allocators_pool_allocate_many_trigger_batch_flush),
        cmocka_unit_test(test_bc_allocators_pool_free_many_trigger_batch_fill),
        /* reallocate */
        cmocka_unit_test(test_bc_allocators_pool_reallocate_same_class),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_bigger_class),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_null_ptr),
        /* free edge cases */
        cmocka_unit_test(test_bc_allocators_pool_free_outside_region),
        /* boundary */
        cmocka_unit_test(test_bc_allocators_pool_allocate_size_zero),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_size_zero),
        /* context switch and cleanup */
        cmocka_unit_test(test_bc_allocators_pool_context_switch),
        cmocka_unit_test(test_bc_allocators_pool_multiple_contexts),
        cmocka_unit_test(test_bc_allocators_pool_free_out_of_region),
        /* small block free valid cls */
        cmocka_unit_test(test_bc_allocators_pool_free_small_block_valid_cls),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
