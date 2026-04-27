// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== aligned_allocate: alignment 64 ===== */

static void test_aligned_allocate_64(void** state)
{
    (void)state;
    void* ptr = NULL;
    assert_true(bc_allocators_aligned_allocate(NULL, 1024, 64, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);
    bc_allocators_aligned_free(NULL, ptr);
}

/* ===== aligned_allocate: alignment 128 ===== */

static void test_aligned_allocate_128(void** state)
{
    (void)state;
    void* ptr = NULL;
    assert_true(bc_allocators_aligned_allocate(NULL, 4096, 128, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 128, 0);
    bc_allocators_aligned_free(NULL, ptr);
}

/* ===== aligned_allocate: alignment 4096 ===== */

static void test_aligned_allocate_4096(void** state)
{
    (void)state;
    void* ptr = NULL;
    assert_true(bc_allocators_aligned_allocate(NULL, 16384, 4096, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 4096, 0);
    bc_allocators_aligned_free(NULL, ptr);
}

/* ===== aligned_allocate: small size 8B aligned 8 ===== */

static void test_aligned_allocate_small_size(void** state)
{
    (void)state;
    void* ptr = NULL;
    assert_true(bc_allocators_aligned_allocate(NULL, 8, sizeof(void*), &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % sizeof(void*), 0);
    bc_allocators_aligned_free(NULL, ptr);
}

/* ===== aligned_allocate: large size 1 MiB aligned 64 ===== */

static void test_aligned_allocate_large_size(void** state)
{
    (void)state;
    size_t size = 1024 * 1024;
    void* ptr = NULL;
    assert_true(bc_allocators_aligned_allocate(NULL, size, 64, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);
    bc_allocators_aligned_free(NULL, ptr);
}

/* ===== aligned_allocate: write+read across full buffer (no fault) ===== */

static void test_aligned_allocate_writeable(void** state)
{
    (void)state;
    size_t size = 4096;
    void* ptr = NULL;
    assert_true(bc_allocators_aligned_allocate(NULL, size, 64, &ptr));
    assert_non_null(ptr);

    unsigned char* bytes = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        bytes[i] = (unsigned char)(i & 0xFF);
    }
    for (size_t i = 0; i < size; i++) {
        assert_int_equal(bytes[i], (unsigned char)(i & 0xFF));
    }

    bc_allocators_aligned_free(NULL, ptr);
}

/* ===== aligned_allocate: alignment non-power-of-two rejected ===== */

static void test_aligned_allocate_rejects_non_pow2(void** state)
{
    (void)state;
    void* ptr = (void*)(uintptr_t)0xDEADBEEF;
    assert_false(bc_allocators_aligned_allocate(NULL, 1024, 48, &ptr));
    assert_null(ptr);
}

/* ===== aligned_allocate: alignment < sizeof(void*) rejected ===== */

static void test_aligned_allocate_rejects_alignment_too_small(void** state)
{
    (void)state;
    void* ptr = (void*)(uintptr_t)0xDEADBEEF;
    assert_false(bc_allocators_aligned_allocate(NULL, 1024, 4, &ptr));
    assert_null(ptr);
}

/* ===== aligned_allocate: size 0 rejected ===== */

static void test_aligned_allocate_rejects_size_zero(void** state)
{
    (void)state;
    void* ptr = (void*)(uintptr_t)0xDEADBEEF;
    assert_false(bc_allocators_aligned_allocate(NULL, 0, 64, &ptr));
    assert_null(ptr);
}

/* ===== aligned_free: NULL pointer is safe (free(NULL) semantics) ===== */

static void test_aligned_free_null(void** state)
{
    (void)state;
    bc_allocators_aligned_free(NULL, NULL);
}

/* ===== aligned_allocate: with non-NULL ctx (currently untracked, but must work) ===== */

static void test_aligned_allocate_with_ctx(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(NULL, &ctx));
    assert_non_null(ctx);

    void* ptr = NULL;
    assert_true(bc_allocators_aligned_allocate(ctx, 256, 64, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);
    bc_allocators_aligned_free(ctx, ptr);

    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_aligned_allocate_64),
        cmocka_unit_test(test_aligned_allocate_128),
        cmocka_unit_test(test_aligned_allocate_4096),
        cmocka_unit_test(test_aligned_allocate_small_size),
        cmocka_unit_test(test_aligned_allocate_large_size),
        cmocka_unit_test(test_aligned_allocate_writeable),
        cmocka_unit_test(test_aligned_allocate_rejects_non_pow2),
        cmocka_unit_test(test_aligned_allocate_rejects_alignment_too_small),
        cmocka_unit_test(test_aligned_allocate_rejects_size_zero),
        cmocka_unit_test(test_aligned_free_null),
        cmocka_unit_test(test_aligned_allocate_with_ctx),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
