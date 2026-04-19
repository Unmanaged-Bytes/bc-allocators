// SPDX-License-Identifier: MIT

/*
 * Tests for BC_TYPED_ARRAY_DEFINE.
 *
 * Verifies that the macro generates a typed array with:
 * - Direct element assignment in push (no bc_core_copy dispatch)
 * - Correct grow-on-overflow behavior
 * - Correct clear / reserve / destroy semantics
 * - Memory correctness (no leak, valid data after grow)
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include "bc_allocators_typed_array.h"
#include "bc_allocators.h"
#include "bc_allocators_pool.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== Test element type : 16-byte struct (matches bc_parallel_task_entry_t) ===== */

typedef struct {
    size_t key;
    size_t value;
} test_pair_t;

BC_TYPED_ARRAY_DEFINE(test_pair_t, test_pair_array)

/* ===== Test element type : 8-byte struct ===== */

typedef struct {
    uint32_t a;
    uint32_t b;
} test_word_t;

BC_TYPED_ARRAY_DEFINE(test_word_t, test_word_array)

/* ===== Helpers ===== */

static bc_allocators_context_t* make_mem(void)
{
    bc_allocators_context_t* mem = NULL;
    bc_allocators_context_create(NULL, &mem);
    return mem;
}

/* ===== Tests ===== */

static void test_zero_init_is_empty(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    test_pair_array_t arr = {0};
    assert_int_equal(test_pair_array_length(&arr), 0);
    assert_int_equal(test_pair_array_capacity(&arr), 0);
    assert_null(test_pair_array_data(&arr));

    bc_allocators_context_destroy(mem);
}

static void test_push_single_element(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    test_pair_array_t arr = {0};
    test_pair_t elem = {.key = 42, .value = 99};

    assert_true(test_pair_array_push(mem, &arr, elem));
    assert_int_equal(test_pair_array_length(&arr), 1);
    assert_true(test_pair_array_capacity(&arr) >= 1);

    test_pair_t* data = test_pair_array_data(&arr);
    assert_non_null(data);
    assert_int_equal(data[0].key, 42);
    assert_int_equal(data[0].value, 99);

    test_pair_array_destroy(mem, &arr);
    bc_allocators_context_destroy(mem);
}

static void test_push_multiple_elements_preserves_order(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    test_pair_array_t arr = {0};

    for (size_t i = 0; i < 20; i++) {
        test_pair_t elem = {.key = i, .value = i * 2};
        assert_true(test_pair_array_push(mem, &arr, elem));
    }

    assert_int_equal(test_pair_array_length(&arr), 20);

    const test_pair_t* data = test_pair_array_data(&arr);
    for (size_t i = 0; i < 20; i++) {
        assert_int_equal(data[i].key, i);
        assert_int_equal(data[i].value, i * 2);
    }

    test_pair_array_destroy(mem, &arr);
    bc_allocators_context_destroy(mem);
}

static void test_push_triggers_grow_and_preserves_data(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    /* Push 100 elements — forces multiple reallocations. */
    test_pair_array_t arr = {0};
    for (size_t i = 0; i < 100; i++) {
        test_pair_t elem = {.key = i, .value = i + 1};
        assert_true(test_pair_array_push(mem, &arr, elem));
    }

    assert_int_equal(test_pair_array_length(&arr), 100);
    assert_true(test_pair_array_capacity(&arr) >= 100);

    const test_pair_t* data = test_pair_array_data(&arr);
    for (size_t i = 0; i < 100; i++) {
        assert_int_equal(data[i].key, i);
        assert_int_equal(data[i].value, i + 1);
    }

    test_pair_array_destroy(mem, &arr);
    bc_allocators_context_destroy(mem);
}

static void test_clear_resets_length_keeps_capacity(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    test_pair_array_t arr = {0};
    for (size_t i = 0; i < 10; i++) {
        test_pair_t elem = {.key = i, .value = i};
        test_pair_array_push(mem, &arr, elem);
    }

    size_t cap_before = test_pair_array_capacity(&arr);
    test_pair_array_clear(&arr);

    assert_int_equal(test_pair_array_length(&arr), 0);
    assert_int_equal(test_pair_array_capacity(&arr), cap_before);
    assert_non_null(test_pair_array_data(&arr));

    /* Can push again after clear. */
    test_pair_t new_elem = {.key = 77, .value = 88};
    assert_true(test_pair_array_push(mem, &arr, new_elem));
    assert_int_equal(test_pair_array_length(&arr), 1);
    assert_int_equal(test_pair_array_data(&arr)[0].key, 77);

    test_pair_array_destroy(mem, &arr);
    bc_allocators_context_destroy(mem);
}

static void test_reserve_preallocates_capacity(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    test_pair_array_t arr = {0};
    assert_true(test_pair_array_reserve(mem, &arr, 256));
    assert_int_equal(test_pair_array_length(&arr), 0);
    assert_true(test_pair_array_capacity(&arr) >= 256);

    /* Push 256 elements — no reallocation should be needed. */
    size_t cap_after_reserve = test_pair_array_capacity(&arr);
    for (size_t i = 0; i < 256; i++) {
        test_pair_t elem = {.key = i, .value = i};
        assert_true(test_pair_array_push(mem, &arr, elem));
    }
    assert_int_equal(test_pair_array_capacity(&arr), cap_after_reserve);

    test_pair_array_destroy(mem, &arr);
    bc_allocators_context_destroy(mem);
}

static void test_reserve_noop_if_capacity_sufficient(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    test_pair_array_t arr = {0};
    assert_true(test_pair_array_reserve(mem, &arr, 64));
    size_t cap = test_pair_array_capacity(&arr);

    /* Requesting less than current capacity is a no-op. */
    assert_true(test_pair_array_reserve(mem, &arr, 32));
    assert_int_equal(test_pair_array_capacity(&arr), cap);

    test_pair_array_destroy(mem, &arr);
    bc_allocators_context_destroy(mem);
}

static void test_destroy_on_zero_init_is_safe(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    /* Destroy on a zero-initialized array must not crash. */
    test_pair_array_t arr = {0};
    test_pair_array_destroy(mem, &arr);

    bc_allocators_context_destroy(mem);
}

static void test_8byte_element_type(void** state)
{
    (void)state;
    bc_allocators_context_t* mem = make_mem();

    test_word_array_t arr = {0};
    for (size_t i = 0; i < 50; i++) {
        test_word_t w = {.a = (uint32_t)i, .b = (uint32_t)(i * 3)};
        assert_true(test_word_array_push(mem, &arr, w));
    }

    assert_int_equal(test_word_array_length(&arr), 50);
    const test_word_t* data = test_word_array_data(&arr);
    for (size_t i = 0; i < 50; i++) {
        assert_int_equal(data[i].a, (uint32_t)i);
        assert_int_equal(data[i].b, (uint32_t)(i * 3));
    }

    test_word_array_destroy(mem, &arr);
    bc_allocators_context_destroy(mem);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_zero_init_is_empty),
        cmocka_unit_test(test_push_single_element),
        cmocka_unit_test(test_push_multiple_elements_preserves_order),
        cmocka_unit_test(test_push_triggers_grow_and_preserves_data),
        cmocka_unit_test(test_clear_resets_length_keeps_capacity),
        cmocka_unit_test(test_reserve_preallocates_capacity),
        cmocka_unit_test(test_reserve_noop_if_capacity_sufficient),
        cmocka_unit_test(test_destroy_on_zero_init_is_safe),
        cmocka_unit_test(test_8byte_element_type),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
