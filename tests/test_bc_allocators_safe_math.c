// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== compute_alloc_size happy path ===== */

static void test_bc_allocators_compute_alloc_size_normal(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t page_size = 0;
    assert_true(bc_allocators_context_page_size(ctx, &page_size));
    assert_true(page_size > 0);

    size_t result = 0;
    assert_true(bc_allocators_compute_alloc_size(ctx, 100, 10, &result));
    /* 100 * 10 = 1000, aligned up to page_size */
    assert_true(result >= 1000);
    assert_true(page_size > 0 && result % page_size == 0);

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_compute_alloc_size_exact_page(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t page_size = 0;
    assert_true(bc_allocators_context_page_size(ctx, &page_size));
    size_t result = 0;
    assert_true(bc_allocators_compute_alloc_size(ctx, page_size, 1, &result));
    assert_int_equal(result, page_size);

    bc_allocators_context_destroy(ctx);
}

/* ===== compute_alloc_size edge cases ===== */

static void test_bc_allocators_compute_alloc_size_zero_element(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t result = 0;
    assert_false(bc_allocators_compute_alloc_size(ctx, 0, 10, &result));

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_compute_alloc_size_zero_count(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t result = 0;
    assert_false(bc_allocators_compute_alloc_size(ctx, 100, 0, &result));

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_compute_alloc_size_overflow_mul(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    size_t result = 0;
    assert_false(bc_allocators_compute_alloc_size(ctx, SIZE_MAX, 2, &result));

    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_compute_alloc_size_overflow_align(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    /* Product = SIZE_MAX - 1 (no mul overflow), but align_up to page overflows */
    size_t result = 0;
    assert_false(bc_allocators_compute_alloc_size(ctx, SIZE_MAX - 1, 1, &result));

    bc_allocators_context_destroy(ctx);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* compute_alloc_size happy path */
        cmocka_unit_test(test_bc_allocators_compute_alloc_size_normal),
        cmocka_unit_test(test_bc_allocators_compute_alloc_size_exact_page),
        /* compute_alloc_size edge cases */
        cmocka_unit_test(test_bc_allocators_compute_alloc_size_zero_element),
        cmocka_unit_test(test_bc_allocators_compute_alloc_size_zero_count),
        cmocka_unit_test(test_bc_allocators_compute_alloc_size_overflow_mul),
        cmocka_unit_test(test_bc_allocators_compute_alloc_size_overflow_align),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
