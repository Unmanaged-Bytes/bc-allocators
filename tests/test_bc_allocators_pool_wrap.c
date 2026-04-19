// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_pool.h"
#include "bc_core.h"

#include "bc_allocators_context_internal.h"

#include <sys/mman.h>

#include <stdbool.h>
#include <stdint.h>

/* ===== Wrap: bc_core_align_up (count-based) ===== */

static int align_up_call_count = 0;
static int align_up_fail_on_call = 0;

bool __real_bc_core_align_up(size_t value, size_t alignment, size_t* out_result);

bool __wrap_bc_core_align_up(size_t value, size_t alignment, size_t* out_result)
{
    align_up_call_count++;
    if (align_up_fail_on_call > 0 && align_up_call_count == align_up_fail_on_call) {
        return false;
    }
    return __real_bc_core_align_up(value, alignment, out_result);
}

/* ===== Wrap: bc_core_safe_add (count-based) ===== */

static int safe_add_call_count = 0;
static int safe_add_fail_on_call = 0;

bool __real_bc_core_safe_add(size_t a, size_t b, size_t* out_result);

bool __wrap_bc_core_safe_add(size_t a, size_t b, size_t* out_result)
{
    safe_add_call_count++;
    if (safe_add_fail_on_call > 0 && safe_add_call_count == safe_add_fail_on_call) {
        return false;
    }
    return __real_bc_core_safe_add(a, b, out_result);
}

/* ===== Wrap: madvise (for MADV_FREE fallback coverage) ===== */

static bool madvise_should_fail_free = false;

int __real_madvise(void* addr, size_t length, int advice);

int __wrap_madvise(void* addr, size_t length, int advice)
{
    if (madvise_should_fail_free && advice == MADV_FREE) {
        return -1;
    }
    return __real_madvise(addr, length, advice);
}

/* ===== Reset helpers ===== */

static void reset_wraps(void)
{
    align_up_call_count = 0;
    align_up_fail_on_call = 0;
    safe_add_call_count = 0;
    safe_add_fail_on_call = 0;
    madvise_should_fail_free = false;
}

static bc_allocators_context_t* create_ctx(void)
{
    reset_wraps();
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    return ctx;
}

/* ===== Test: align_up failure for page_class_size in context_create ===== */

static void test_bc_allocators_context_create_page_class_align_up_failure(void** state)
{
    (void)state;
    reset_wraps();
    /* Call 1: align_up(max_memory) — must succeed.
       Call 2: align_up(num_pages) — force failure (line 70-71 in bc_allocators.c). */
    align_up_fail_on_call = 2;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);

    reset_wraps();
}

/* ===== Test: align_up failure for context_alloc_size in context_create ===== */

static void test_bc_allocators_context_create_context_align_up_failure(void** state)
{
    (void)state;
    reset_wraps();
    /* Call 1: align_up(max_memory) — must succeed.
       Call 2: align_up(num_pages) — must succeed.
       Call 3: align_up(sizeof context) — force failure (line 75-76 in bc_allocators.c). */
    align_up_fail_on_call = 3;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);

    reset_wraps();
}

/* ===== Test: safe_add overflow in allocate_large (line 202) ===== */

static void test_bc_allocators_pool_allocate_large_safe_add_overflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Warm up: allocate a small block to establish TLS context */
    void* warmup = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &warmup));
    assert_non_null(warmup);

    /* Record safe_add calls so far, then fail the next one (in allocate_large) */
    int count_after_warmup = safe_add_call_count;
    safe_add_fail_on_call = count_after_warmup + 1;

    /* Try large allocation (beyond max class size) */
    void* ptr = NULL;
    assert_false(bc_allocators_pool_allocate(ctx, BC_ALLOCATORS_MAX_CLASS_SIZE + 4096, &ptr));

    bc_allocators_pool_free(ctx, warmup);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: align_up overflow in allocate_large (line 206) ===== */

static void test_bc_allocators_pool_allocate_large_align_up_overflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Record align_up calls after context creation, then fail the next one */
    int count_after_ctx = align_up_call_count;
    align_up_fail_on_call = count_after_ctx + 1;

    /* Try large allocation */
    void* ptr = NULL;
    assert_false(bc_allocators_pool_allocate(ctx, BC_ALLOCATORS_MAX_CLASS_SIZE + 4096, &ptr));

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: align_up fail in bump_allocate_class for large block (line 170) ===== */

static void test_bc_allocators_pool_bump_allocate_align_up_overflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Size 65537 maps to class 14 (262144) directly (no promotion after header removal).
       262144 >= page_size so align_up is called in bump_allocate_class.
       Record current align_up count, then fail the next one. */
    int count_after_ctx = align_up_call_count;
    align_up_fail_on_call = count_after_ctx + 1;

    /* After header removal: size 65537 -> class=14 (262144). No actual_class promotion.
       262144 >= page_size so align_up is called in bump_allocate_class.
       Current code: original_class=13, actual_class=14 (262144) — same block size,
       different class index, but align_up is still triggered. */
    void* ptr = NULL;
    assert_false(bc_allocators_pool_allocate(ctx, 65537, &ptr));

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: safe_add overflow in large-to-large reallocate (line 437) ===== */

static void test_bc_allocators_pool_reallocate_large_safe_add_overflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Allocate a large block and write data */
    size_t large_size = BC_ALLOCATORS_MAX_CLASS_SIZE + 4096;
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, large_size, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 64, (unsigned char)0xAB);

    /* Record safe_add calls, then fail the next one in reallocate_large */
    int count_now = safe_add_call_count;
    safe_add_fail_on_call = count_now + 1;

    /* Try to reallocate to a bigger large block */
    void* new_ptr = NULL;
    assert_false(bc_allocators_pool_reallocate(ctx, ptr, large_size + 4096, &new_ptr));

    /* Original data must be preserved */
    unsigned char expected[64];
    bc_core_fill(expected, 64, (unsigned char)0xAB);
    assert_memory_equal(ptr, expected, 64);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: align_up overflow in large-to-large reallocate (line 441) ===== */

static void test_bc_allocators_pool_reallocate_large_align_up_overflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Allocate a large block */
    size_t large_size = BC_ALLOCATORS_MAX_CLASS_SIZE + 4096;
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, large_size, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 64, (unsigned char)0xCD);

    /* Record align_up calls, then fail the next one in reallocate path */
    int count_now = align_up_call_count;
    align_up_fail_on_call = count_now + 1;

    /* Try to reallocate to a bigger large block */
    void* new_ptr = NULL;
    assert_false(bc_allocators_pool_reallocate(ctx, ptr, large_size + 4096, &new_ptr));

    /* Original data must be preserved */
    unsigned char expected[64];
    bc_core_fill(expected, 64, (unsigned char)0xCD);
    assert_memory_equal(ptr, expected, 64);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: MADV_FREE fail triggers fallback to MADV_DONTNEED (line 244) ===== */

static void test_bc_allocators_pool_free_large_madvise_fallback(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Allocate a large block */
    size_t large_size = BC_ALLOCATORS_MAX_CLASS_SIZE + 4096;
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, large_size, &ptr));
    assert_non_null(ptr);

    /* Make MADV_FREE fail so free_large falls back to MADV_DONTNEED */
    madvise_should_fail_free = true;

    /* Free should not crash — fallback works */
    bc_allocators_pool_free(ctx, ptr);

    madvise_should_fail_free = false;
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: free on UNASSIGNED page is a silent no-op (lines 294, 498) ===== */

static void test_bc_allocators_pool_free_unassigned_page(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Allocate a small block to ensure the region has some data */
    void* warmup = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 64, &warmup));
    assert_non_null(warmup);

    /* The bump pointer tells us how much of the region is used.
       Any page beyond the bump has UNASSIGNED class.
       Pick a pointer in the region but beyond the bump. */
    size_t bump = ctx->region.bump;
    size_t page_size = ctx->page_size;

    /* Compute the next page-aligned offset beyond the bump */
    size_t unassigned_offset = ((bump / page_size) + 2) * page_size;

    /* Make sure this offset is within the region */
    if (unassigned_offset < ctx->region.total_size) {
        void* unassigned_ptr = (unsigned char*)ctx->region.base + unassigned_offset;

        /* Free on an UNASSIGNED page should silently return */
        bc_allocators_pool_free(ctx, unassigned_ptr);
    }

    bc_allocators_pool_free(ctx, warmup);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: pool_free uses page_class lookup, not ptr[-1] tag ===== */
/*
 * After header removal, pool_free dispatches via page_class[offset>>page_shift],
 * not via ptr[-1].
 *
 * RED contract: after alloc(16), page_class for the pointer's page must be 0
 * (direct class for 16-byte blocks). Current code tags with actual_class=1, so
 * this assertion FAILS against the current header-separation implementation.
 *
 * Replaces: test_bc_allocators_pool_free_tag_out_of_range_non_large
 * (ptr[-1] forging is meaningless when free path uses page_class, not tag)
 */
static void test_bc_allocators_pool_free_page_class_dispatch(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Allocate size=16 */
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 16, &ptr));
    assert_non_null(ptr);

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    /* After header removal: page_class must be 0 (direct class 0 = 16-byte blocks).
     * Current code stores actual_class=1 — this assertion FAILS (RED). */
    assert_int_equal((int)cls, 0);

    /* Free must succeed (no crash) regardless of the class tag value */
    bc_allocators_pool_free(ctx, ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== Test: get_usable_size uses page_class lookup, not ptr[-1] tag ===== */
/*
 * After header removal, get_usable_size returns class_sizes[cls] where cls comes
 * from page_class[], not from ptr[-1].
 *
 * RED contract: after alloc(32), page_class for the pointer's page must be 1
 * (direct class 1 = 32-byte blocks). Current code tags with actual_class=2, so
 * this assertion FAILS against the current header-separation implementation.
 *
 * Replaces: test_bc_allocators_pool_get_usable_size_tag_out_of_range
 * (ptr[-1] forging is meaningless when get_usable_size uses page_class, not tag)
 */
static void test_bc_allocators_pool_get_usable_size_page_class_dispatch(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Allocate size=32 */
    void* ptr = NULL;
    assert_true(bc_allocators_pool_allocate(ctx, 32, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 32, (unsigned char)0xAB);

    size_t offset = (size_t)((unsigned char*)ptr - (unsigned char*)ctx->region.base);
    size_t page_idx = offset >> ctx->page_shift;
    uint8_t cls = ctx->region.page_class[page_idx];

    /* After header removal: page_class must be 1 (direct class 1 = 32-byte blocks).
     * Current code stores actual_class=2 — this assertion FAILS (RED). */
    assert_int_equal((int)cls, 1);

    /* Reallocate same class — in-place via page_class comparison.
     * new_class = size_class_for_size(32) = 1, old_class = page_class[idx] = 1 -> in-place.
     * This succeeds after the fix. With current code the page_class assert above already fails. */
    void* new_ptr = NULL;
    assert_true(bc_allocators_pool_reallocate(ctx, ptr, 32, &new_ptr));
    assert_non_null(new_ptr);

    /* Data preserved */
    assert_int_equal(((unsigned char*)new_ptr)[0], 0xAB);

    bc_allocators_pool_free(ctx, new_ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: region.bump is plain size_t (not _Atomic) after refactor ===== */
/* RED: fails to compile against current _Atomic(size_t) bump because
   &ctx->region.bump has type _Atomic(size_t) * which is incompatible with size_t *.
   GREEN: compiles after refactor removes _Atomic from region.bump. */

static void test_bc_allocators_pool_region_bump_plain_type(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_ctx();

    /* Take address of bump as plain size_t * — valid only when bump is size_t, not _Atomic */
    size_t* bump_ptr = &ctx->region.bump;
    assert_non_null(bump_ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* align_up guards during context creation */
        cmocka_unit_test(test_bc_allocators_context_create_page_class_align_up_failure),
        cmocka_unit_test(test_bc_allocators_context_create_context_align_up_failure),
        /* overflow guards in allocate_large */
        cmocka_unit_test(test_bc_allocators_pool_allocate_large_safe_add_overflow),
        cmocka_unit_test(test_bc_allocators_pool_allocate_large_align_up_overflow),
        /* overflow guard in bump_allocate_class */
        cmocka_unit_test(test_bc_allocators_pool_bump_allocate_align_up_overflow),
        /* overflow guards in large-to-large reallocate */
        cmocka_unit_test(test_bc_allocators_pool_reallocate_large_safe_add_overflow),
        cmocka_unit_test(test_bc_allocators_pool_reallocate_large_align_up_overflow),
        /* madvise fallback */
        cmocka_unit_test(test_bc_allocators_pool_free_large_madvise_fallback),
        /* UNASSIGNED page free */
        cmocka_unit_test(test_bc_allocators_pool_free_unassigned_page),
        /* page_class dispatch in pool_free — RED: current code tags with actual_class, not direct class */
        cmocka_unit_test(test_bc_allocators_pool_free_page_class_dispatch),
        /* page_class dispatch in get_usable_size — RED: same */
        cmocka_unit_test(test_bc_allocators_pool_get_usable_size_page_class_dispatch),
        /* region.bump plain type (RED: fails against _Atomic bump) */
        cmocka_unit_test(test_bc_allocators_pool_region_bump_plain_type),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
