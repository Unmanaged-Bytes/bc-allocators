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

static bc_allocators_context_t* create_small_ctx(size_t max_pool_memory)
{
    bc_allocators_context_config_t config = {
        .max_pool_memory = max_pool_memory,
        .tracking_enabled = true,
    };
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(&config, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    return ctx;
}

/* ===== region exhaustion: allocate until bump is full ===== */

static void test_bc_allocators_pool_allocate_region_exhausted(void** state)
{
    (void)state;

    /* Create a context with very small max_pool_memory (1 MB) */
    bc_allocators_context_t* ctx = create_small_ctx(1024 * 1024);

    /* Allocate 262144 (largest class) repeatedly until the region is exhausted */
    void* ptrs[64];
    int allocated = 0;
    bool got_failure = false;

    for (int i = 0; i < 64; i++) {
        ptrs[i] = NULL;
        if (!bc_allocators_pool_allocate(ctx, 262144, &ptrs[i])) {
            got_failure = true;
            break;
        }
        allocated++;
    }

    /* We expect the region to be exhausted before 64 allocations of 262144
       within a 1 MB region */
    assert_true(got_failure);

    /* Free what we allocated */
    for (int i = 0; i < allocated; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== region exhaustion with small blocks ===== */

static void test_bc_allocators_pool_allocate_region_exhausted_small(void** state)
{
    (void)state;

    /* 64 KB context -- very tight */
    bc_allocators_context_t* ctx = create_small_ctx(64 * 1024);

    void* ptrs[8192];
    int allocated = 0;
    bool got_failure = false;

    for (int i = 0; i < 8192; i++) {
        ptrs[i] = NULL;
        if (!bc_allocators_pool_allocate(ctx, 16, &ptrs[i])) {
            got_failure = true;
            break;
        }
        allocated++;
    }

    /* 64KB / 16 (class 0 = 16 bytes) ~ 4096 blocks max; exhaust well before 8192. */
    assert_true(got_failure);

    for (int i = 0; i < allocated; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== large allocation exceeding max_pool_memory ===== */

static void test_bc_allocators_pool_allocate_large_exceeds_region(void** state)
{
    (void)state;

    /* 1 MB context */
    bc_allocators_context_t* ctx = create_small_ctx(1024 * 1024);

    /* 6 MB > BC_ALLOCATORS_MAX_CLASS_SIZE (4 MB): size_class_for_size returns -1,
       routes directly through allocate_large; bump_raw fails in 1 MB region */
    void* ptr = NULL;
    assert_false(bc_allocators_pool_allocate(ctx, 6 * 1024 * 1024, &ptr));
    assert_null(ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== near-max class size allocation exceeding max_pool_memory ===== */

static void test_bc_allocators_pool_allocate_near_max_class_exceeds_region(void** state)
{
    (void)state;

    /* 1 MB context */
    bc_allocators_context_t* ctx = create_small_ctx(1024 * 1024);

    /* 3 MB: size_class_for_size returns 18 (class 18 = 4194304 bytes).
     * bump_allocate_class(18) fails: 4194304 bytes does not fit in the 1 MB region. */
    void* ptr = NULL;
    assert_false(bc_allocators_pool_allocate(ctx, 3 * 1024 * 1024, &ptr));
    assert_null(ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== reallocate fails: original ptr not freed ===== */

static void test_bc_allocators_pool_reallocate_fail_preserves_ptr(void** state)
{
    (void)state;

    /* 256 KB context */
    bc_allocators_context_t* ctx = create_small_ctx(256 * 1024);

    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 64, (unsigned char)0xDD);

    /* Try to reallocate to a size that exceeds the region */
    void* new_ptr = NULL;
    assert_false(bc_allocators_pool_reallocate(ctx, ptr, 512 * 1024, &new_ptr));
    assert_null(new_ptr);

    /* Original ptr must still be valid (spec: ptr is NOT freed on failure) */
    assert_int_equal(((unsigned char*)ptr)[0], 0xDD);
    assert_int_equal(((unsigned char*)ptr)[63], 0xDD);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== allocate after region exhausted then free and retry ===== */

static void test_bc_allocators_pool_allocate_after_free_reuse(void** state)
{
    (void)state;

    /* 128 KB context */
    bc_allocators_context_t* ctx = create_small_ctx(128 * 1024);

    /* Fill the region with 64-byte allocations */
    void* ptrs[4096];
    int allocated = 0;

    for (int i = 0; i < 4096; i++) {
        ptrs[i] = NULL;
        if (!bc_allocators_pool_allocate(ctx, 64, &ptrs[i])) {
            break;
        }
        allocated++;
    }

    assert_true(allocated > 0);

    /* Free all */
    for (int i = 0; i < allocated; i++) {
        bc_allocators_pool_free(ctx, ptrs[i]);
    }

    /* Now allocate again -- should succeed by reusing from free-list */
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &ptr));
    assert_non_null(ptr);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== free ptr before region base: p < base (bc_allocators_pool.c:280) ===== */

static void test_bc_allocators_pool_free_ptr_before_base(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_small_ctx(1024 * 1024);

    /* Use address (void*)1 -- always < any valid mmap base address.
       ptr_in_region: p >= base is false (p < base branch) */
    bc_allocators_pool_free(ctx, (void*)1);

    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bc_allocators_pool_allocate_region_exhausted),
        cmocka_unit_test(test_bc_allocators_pool_allocate_region_exhausted_small),
        cmocka_unit_test(test_bc_allocators_pool_allocate_large_exceeds_region),
        cmocka_unit_test(test_bc_allocators_pool_allocate_near_max_class_exceeds_region),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_fail_preserves_ptr),
        cmocka_unit_test(test_bc_allocators_pool_allocate_after_free_reuse),
        cmocka_unit_test(test_bc_allocators_pool_free_ptr_before_base),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
