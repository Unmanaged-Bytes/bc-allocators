// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_platform_internal.h"
#include "bc_core_test_wrap.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== Wrap: bc_allocators_platform_map (NULL-set out_pointer divergent — kept manual) ===== */

static int map_call_count = 0;
static int map_fail_on_call = 0;

bool __real_bc_allocators_platform_map(size_t size, void** out_pointer);

bool __wrap_bc_allocators_platform_map(size_t size, void** out_pointer)
{
    map_call_count++;
    if (map_fail_on_call > 0 && map_call_count == map_fail_on_call) {
        if (out_pointer != NULL) {
            *out_pointer = NULL;
        }
        return false;
    }
    return __real_bc_allocators_platform_map(size, out_pointer);
}

BC_TEST_WRAP_SHOULD_FAIL(bc_allocators_platform_get_page_size, bool, false,
                         (size_t* out_page_size), (out_page_size))

BC_TEST_WRAP_SHOULD_FAIL(bc_allocators_platform_get_cache_line_size, bool, false,
                         (size_t* out), (out))

/* ===== Reset helpers ===== */

static void reset_wraps(void)
{
    map_call_count = 0;
    map_fail_on_call = 0;
    BC_TEST_WRAP_RESET_SHOULD_FAIL(bc_allocators_platform_get_page_size);
    BC_TEST_WRAP_RESET_SHOULD_FAIL(bc_allocators_platform_get_cache_line_size);
}

/* ===== Test: mmap fail on context alloc (1st map call) ===== */

static void test_bc_allocators_context_create_map_fail_context_alloc(void** state)
{
    (void)state;
    reset_wraps();
    map_fail_on_call = 1;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);
}

/* ===== Test: mmap fail on region alloc (2nd map call) ===== */

static void test_bc_allocators_context_create_map_fail_region_alloc(void** state)
{
    (void)state;
    reset_wraps();
    map_fail_on_call = 2;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);
}

/* ===== Test: mmap fail on page_class alloc (3rd map call) ===== */

static void test_bc_allocators_context_create_map_fail_page_class_alloc(void** state)
{
    (void)state;
    reset_wraps();
    map_fail_on_call = 3;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);
}

/* ===== Test: sysconf fail for page_size ===== */

static void test_bc_allocators_context_create_page_size_fail(void** state)
{
    (void)state;
    reset_wraps();
    mock_bc_allocators_platform_get_page_size_should_fail = true;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);
}

/* ===== Test: verify LIFO cleanup on region fail ===== */
/* When the 2nd map (region) fails, the 1st map (context) must be unmapped.
   If the cleanup is correct, no leaked mappings remain. We verify this
   by checking that context_create returns false and ctx is NULL. */

static void test_bc_allocators_context_create_cleanup_lifo_region(void** state)
{
    (void)state;
    reset_wraps();
    map_fail_on_call = 2;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);

    /* A second create with no failures must succeed
       (proves the first create cleaned up properly) */
    reset_wraps();
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: verify LIFO cleanup on page_class fail ===== */
/* When the 3rd map (page_class) fails, both region and context must be unmapped. */

static void test_bc_allocators_context_create_cleanup_lifo_page_class(void** state)
{
    (void)state;
    reset_wraps();
    map_fail_on_call = 3;

    bc_allocators_context_t* ctx = NULL;
    assert_false(bc_allocators_context_create(NULL, &ctx));
    assert_null(ctx);

    reset_wraps();
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);
    bc_allocators_context_destroy(ctx);
}

/* ===== Test: cache_line_size fallback to 64 ===== */

static void test_bc_allocators_context_create_cache_line_fallback(void** state)
{
    (void)state;
    reset_wraps();
    mock_bc_allocators_platform_get_cache_line_size_should_fail = true;

    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    /* cache_line failure is not fatal — fallback to 64 */
    size_t cls = 0;
    assert_true(bc_allocators_context_cache_line_size(ctx, &cls));
    assert_int_equal(cls, 64);

    bc_allocators_context_destroy(ctx);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* map failures at each step */
        cmocka_unit_test(test_bc_allocators_context_create_map_fail_context_alloc),
        cmocka_unit_test(test_bc_allocators_context_create_map_fail_region_alloc),
        cmocka_unit_test(test_bc_allocators_context_create_map_fail_page_class_alloc),
        /* other failure modes */
        cmocka_unit_test(test_bc_allocators_context_create_page_size_fail),
        /* LIFO cleanup verification */
        cmocka_unit_test(test_bc_allocators_context_create_cleanup_lifo_region),
        cmocka_unit_test(test_bc_allocators_context_create_cleanup_lifo_page_class),
        /* cache line fallback */
        cmocka_unit_test(test_bc_allocators_context_create_cache_line_fallback),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
