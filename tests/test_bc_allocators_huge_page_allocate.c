// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_core.h"

#include <stdbool.h>
#include <stdint.h>

#define HUGE_PAGE_2MIB ((size_t)2 * 1024 * 1024)
#define HUGE_PAGE_256MIB ((size_t)256 * 1024 * 1024)

/* ===== huge_page_allocate: 2 MiB ===== */

static void test_huge_page_allocate_2mib(void** state)
{
    (void)state;
    void* ptr = NULL;
    bool ok = bc_allocators_huge_page_allocate(HUGE_PAGE_2MIB, &ptr);
    if (!ok) {
        skip();
        return;
    }
    assert_non_null(ptr);

    unsigned char* bytes = (unsigned char*)ptr;
    bytes[0] = 0xAB;
    bytes[HUGE_PAGE_2MIB - 1] = 0xCD;
    assert_int_equal(bytes[0], 0xAB);
    assert_int_equal(bytes[HUGE_PAGE_2MIB - 1], 0xCD);

    bc_allocators_huge_page_free(ptr, HUGE_PAGE_2MIB);
}

/* ===== huge_page_allocate: 256 MiB ===== */

static void test_huge_page_allocate_256mib(void** state)
{
    (void)state;
    void* ptr = NULL;
    bool ok = bc_allocators_huge_page_allocate(HUGE_PAGE_256MIB, &ptr);
    if (!ok) {
        skip();
        return;
    }
    assert_non_null(ptr);

    unsigned char* bytes = (unsigned char*)ptr;
    bytes[0] = 0x12;
    bytes[HUGE_PAGE_256MIB - 1] = 0x34;
    assert_int_equal(bytes[0], 0x12);
    assert_int_equal(bytes[HUGE_PAGE_256MIB - 1], 0x34);

    bc_allocators_huge_page_free(ptr, HUGE_PAGE_256MIB);
}

/* ===== huge_page_allocate: matches BC_BUFFER_HUGE_PAGE_THRESHOLD ===== */

static void test_huge_page_allocate_threshold(void** state)
{
    (void)state;
    void* ptr = NULL;
    bool ok = bc_allocators_huge_page_allocate(BC_BUFFER_HUGE_PAGE_THRESHOLD, &ptr);
    if (!ok) {
        skip();
        return;
    }
    assert_non_null(ptr);
    bc_allocators_huge_page_free(ptr, BC_BUFFER_HUGE_PAGE_THRESHOLD);
}

/* ===== huge_page_allocate: size 0 rejected ===== */

static void test_huge_page_allocate_rejects_size_zero(void** state)
{
    (void)state;
    void* ptr = (void*)(uintptr_t)0xDEADBEEF;
    assert_false(bc_allocators_huge_page_allocate(0, &ptr));
    assert_null(ptr);
}

/* ===== huge_page_allocate: alloc + free + alloc again (no leak/state issue) ===== */

static void test_huge_page_allocate_repeated(void** state)
{
    (void)state;
    void* ptr1 = NULL;
    bool ok1 = bc_allocators_huge_page_allocate(HUGE_PAGE_2MIB, &ptr1);
    if (!ok1) {
        skip();
        return;
    }
    assert_non_null(ptr1);
    bc_allocators_huge_page_free(ptr1, HUGE_PAGE_2MIB);

    void* ptr2 = NULL;
    bool ok2 = bc_allocators_huge_page_allocate(HUGE_PAGE_2MIB, &ptr2);
    assert_true(ok2);
    assert_non_null(ptr2);
    bc_allocators_huge_page_free(ptr2, HUGE_PAGE_2MIB);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_huge_page_allocate_2mib),      cmocka_unit_test(test_huge_page_allocate_256mib),
        cmocka_unit_test(test_huge_page_allocate_threshold), cmocka_unit_test(test_huge_page_allocate_rejects_size_zero),
        cmocka_unit_test(test_huge_page_allocate_repeated),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
