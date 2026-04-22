// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_core.h"
#include "bc_allocators_arena.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ===== Helpers ===== */

static bc_allocators_context_t* create_default_ctx(void)
{
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    return ctx;
}

/* ===== create arena, verify non-NULL ===== */

static void test_bc_allocators_arena_create_basic(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 1 ===== */

static void test_bc_allocators_arena_allocate_align_1(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 10, 1, &ptr));
    assert_non_null(ptr);

    /* Write and read back to verify the block is usable */
    bc_core_fill(ptr, 10, (unsigned char)0xAA);
    assert_int_equal(((unsigned char*)ptr)[0], 0xAA);
    assert_int_equal(((unsigned char*)ptr)[9], 0xAA);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 8 ===== */

static void test_bc_allocators_arena_allocate_align_8(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 32, 8, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 8, 0);

    bc_core_fill(ptr, 32, (unsigned char)0xBB);
    assert_int_equal(((unsigned char*)ptr)[0], 0xBB);
    assert_int_equal(((unsigned char*)ptr)[31], 0xBB);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 16 ===== */

static void test_bc_allocators_arena_allocate_align_16(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 16, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 16, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 64 ===== */

static void test_bc_allocators_arena_allocate_align_64(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 8192, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 128, 64, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 64, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 128 (larger than header_aligned) ===== */

static void test_bc_allocators_arena_allocate_align_128(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 8192, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 128, 128, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 128, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 256 ===== */

static void test_bc_allocators_arena_allocate_align_256(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 8192, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 256, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 256, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 4096 (page) ===== */

static void test_bc_allocators_arena_allocate_align_4096(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 32 * 1024, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 4096, &ptr));
    assert_non_null(ptr);
    assert_int_equal((uintptr_t)ptr % 4096, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== multiple allocs in same chunk ===== */

static void test_bc_allocators_arena_allocate_multiple_same_chunk(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr1 = NULL;
    void* ptr2 = NULL;
    void* ptr3 = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 100, 8, &ptr1));
    assert_true(bc_allocators_arena_allocate(arena, 200, 8, &ptr2));
    assert_true(bc_allocators_arena_allocate(arena, 300, 8, &ptr3));

    assert_non_null(ptr1);
    assert_non_null(ptr2);
    assert_non_null(ptr3);

    /* Pointers must be different */
    assert_true(ptr1 != ptr2);
    assert_true(ptr2 != ptr3);
    assert_true(ptr1 != ptr3);

    /* Write to each to verify no overlap */
    bc_core_fill(ptr1, 100, (unsigned char)0x11);
    bc_core_fill(ptr2, 200, (unsigned char)0x22);
    bc_core_fill(ptr3, 300, (unsigned char)0x33);

    assert_int_equal(((unsigned char*)ptr1)[0], 0x11);
    assert_int_equal(((unsigned char*)ptr1)[99], 0x11);
    assert_int_equal(((unsigned char*)ptr2)[0], 0x22);
    assert_int_equal(((unsigned char*)ptr2)[199], 0x22);
    assert_int_equal(((unsigned char*)ptr3)[0], 0x33);
    assert_int_equal(((unsigned char*)ptr3)[299], 0x33);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== copy_string happy path ===== */

static void test_bc_allocators_arena_copy_string_basic(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    const char* source = "hello world";
    const char* copy = NULL;
    assert_true(bc_allocators_arena_copy_string(arena, source, &copy));
    assert_non_null(copy);
    assert_string_equal(copy, source);

    /* Must be a distinct copy, not the same pointer */
    assert_true(copy != source);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== copy_string empty string ===== */

static void test_bc_allocators_arena_copy_string_empty(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    const char* source = "";
    const char* copy = NULL;
    assert_true(bc_allocators_arena_copy_string(arena, source, &copy));
    assert_non_null(copy);
    assert_string_equal(copy, "");
    assert_int_equal(strlen(copy), 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== get_stats reflects allocations ===== */

static void test_bc_allocators_arena_get_stats_reflects_allocs(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    /* Stats before any alloc */
    bc_allocators_arena_stats_t stats = {0};
    assert_true(bc_allocators_arena_get_stats(arena, &stats));
    assert_true(stats.capacity > 0);
    assert_int_equal(stats.used, 0);
    assert_int_equal(stats.allocation_count, 0);

    /* Alloc some data */
    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 100, 1, &ptr));
    assert_non_null(ptr);

    assert_true(bc_allocators_arena_get_stats(arena, &stats));
    assert_true(stats.used > 0);
    assert_int_equal(stats.allocation_count, 1);

    /* Alloc more data */
    assert_true(bc_allocators_arena_allocate(arena, 200, 1, &ptr));
    assert_true(bc_allocators_arena_get_stats(arena, &stats));
    assert_int_equal(stats.allocation_count, 2);
    assert_true(stats.used >= 300);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== reset sets used/count to 0 ===== */

static void test_bc_allocators_arena_reset_clears_state(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 100, 1, &ptr));
    assert_true(bc_allocators_arena_allocate(arena, 200, 1, &ptr));

    assert_true(bc_allocators_arena_reset(arena));

    bc_allocators_arena_stats_t stats = {0};
    assert_true(bc_allocators_arena_get_stats(arena, &stats));
    assert_int_equal(stats.used, 0);
    assert_int_equal(stats.allocation_count, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== reset + re-alloc works ===== */

static void test_bc_allocators_arena_reset_then_realloc(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 100, 1, &ptr));
    assert_non_null(ptr);
    bc_core_fill(ptr, 100, (unsigned char)0xDD);

    assert_true(bc_allocators_arena_reset(arena));

    /* Re-alloc after reset must work */
    void* ptr2 = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 200, 1, &ptr2));
    assert_non_null(ptr2);
    bc_core_fill(ptr2, 200, (unsigned char)0xEE);
    assert_int_equal(((unsigned char*)ptr2)[0], 0xEE);
    assert_int_equal(((unsigned char*)ptr2)[199], 0xEE);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== create with capacity 0 returns false ===== */

static void test_bc_allocators_arena_create_capacity_zero(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_false(bc_allocators_arena_create(ctx, 0, &arena));
    assert_null(arena);

    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with size 0 returns false ===== */

static void test_bc_allocators_arena_allocate_size_zero(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_false(bc_allocators_arena_allocate(arena, 0, 8, &ptr));
    assert_null(ptr);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment 0 returns false ===== */

static void test_bc_allocators_arena_allocate_alignment_zero(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_false(bc_allocators_arena_allocate(arena, 64, 0, &ptr));
    assert_null(ptr);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc with alignment not power of 2 returns false ===== */

static void test_bc_allocators_arena_allocate_alignment_not_power_of_2(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* ptr = NULL;
    assert_false(bc_allocators_arena_allocate(arena, 64, 3, &ptr));
    assert_null(ptr);

    assert_false(bc_allocators_arena_allocate(arena, 64, 5, &ptr));
    assert_null(ptr);

    assert_false(bc_allocators_arena_allocate(arena, 64, 7, &ptr));
    assert_null(ptr);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== destroy with tracking disabled (branch bc_allocators_arena.c:127) ===== */

static void test_bc_allocators_arena_destroy_tracking_disabled(void** state)
{
    (void)state;

    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = false,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    /* Create and destroy arena with tracking disabled:
       bc_allocators_arena_destroy condition (ctx != NULL && ctx->tracking_enabled) is false */
    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    /* Must not crash: tracking branch is skipped */
    bc_allocators_arena_destroy(arena);

    bc_allocators_context_destroy(ctx);
}

/* ===== reset with tracking disabled and no chunks (branch bc_allocators_arena.c:330) ===== */

static void test_bc_allocators_arena_reset_tracking_disabled_no_chunks(void** state)
{
    (void)state;

    bc_allocators_context_config_t config = {
        .max_pool_memory = 0,
        .tracking_enabled = false,
    };
    bc_allocators_context_t* ctx = NULL;
    assert_true(bc_allocators_context_create(&config, &ctx));
    assert_non_null(ctx);

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));
    assert_non_null(arena);

    /* Allocate some data in primary */
    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 100, 1, &ptr));
    assert_non_null(ptr);

    /* Reset with tracking disabled and no chained chunks:
       free_chunks loop doesn't execute, chunks_freed == 0, condition false */
    assert_true(bc_allocators_arena_reset(arena));

    bc_allocators_arena_stats_t stats = {0};
    assert_true(bc_allocators_arena_get_stats(arena, &stats));
    assert_int_equal(stats.used, 0);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_arena_create_large_hugepage(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4 * 1024 * 1024, &arena));
    assert_non_null(arena);

    void* ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 64, 1, &ptr));
    assert_non_null(ptr);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== growable ===== */

static void test_bc_allocators_arena_growable_exceeds_initial(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();
    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create_growable(ctx, 4096, 0, &arena));

    void* first_ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 2048, 1, &first_ptr));
    void* large_ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 32 * 1024, 8, &large_ptr));
    assert_non_null(large_ptr);

    *(char*)first_ptr = 'A';
    assert_int_equal(*(char*)first_ptr, 'A');

    bc_allocators_arena_stats_t stats;
    assert_true(bc_allocators_arena_get_stats(arena, &stats));
    assert_true(stats.chunk_count >= 2u);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_arena_growable_pointers_stable(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();
    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create_growable(ctx, 4096, 0, &arena));

    enum { N = 4096 };
    char* pointers[N];
    for (int i = 0; i < N; i++) {
        char* p = NULL;
        assert_true(bc_allocators_arena_allocate(arena, 128, 8, (void**)&p));
        p[0] = (char)(i & 0x7F);
        pointers[i] = p;
    }
    for (int i = 0; i < N; i++) {
        assert_int_equal(pointers[i][0], (char)(i & 0x7F));
    }

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_arena_growable_reset_drops_extra_chunks(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();
    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create_growable(ctx, 4096, 0, &arena));

    for (int i = 0; i < 1024; i++) {
        void* p = NULL;
        assert_true(bc_allocators_arena_allocate(arena, 128, 1, &p));
    }
    bc_allocators_arena_stats_t before;
    assert_true(bc_allocators_arena_get_stats(arena, &before));
    assert_true(before.chunk_count >= 2u);

    assert_true(bc_allocators_arena_reset(arena));
    bc_allocators_arena_stats_t after;
    assert_true(bc_allocators_arena_get_stats(arena, &after));
    assert_int_equal(after.chunk_count, 1u);
    assert_int_equal(after.used, 0u);

    void* p = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 128, 1, &p));

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

static void test_bc_allocators_arena_fixed_allocate_rejects_overflow(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();
    bc_allocators_arena_t* arena = NULL;
    assert_true(bc_allocators_arena_create(ctx, 4096, &arena));

    void* small_ptr = NULL;
    assert_true(bc_allocators_arena_allocate(arena, 3000, 1, &small_ptr));
    void* over = NULL;
    assert_false(bc_allocators_arena_allocate(arena, 10 * 1024, 1, &over));
    assert_null(over);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        /* happy path */
        cmocka_unit_test(test_bc_allocators_arena_create_basic),
        cmocka_unit_test(test_bc_allocators_arena_growable_exceeds_initial),
        cmocka_unit_test(test_bc_allocators_arena_growable_pointers_stable),
        cmocka_unit_test(test_bc_allocators_arena_growable_reset_drops_extra_chunks),
        cmocka_unit_test(test_bc_allocators_arena_fixed_allocate_rejects_overflow),
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_1),
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_8),
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_16),
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_64),
        cmocka_unit_test(test_bc_allocators_arena_allocate_multiple_same_chunk),
        cmocka_unit_test(test_bc_allocators_arena_copy_string_basic),
        cmocka_unit_test(test_bc_allocators_arena_copy_string_empty),
        cmocka_unit_test(test_bc_allocators_arena_get_stats_reflects_allocs),
        /* reset */
        cmocka_unit_test(test_bc_allocators_arena_reset_clears_state),
        cmocka_unit_test(test_bc_allocators_arena_reset_then_realloc),
        /* boundary */
        cmocka_unit_test(test_bc_allocators_arena_create_capacity_zero),
        cmocka_unit_test(test_bc_allocators_arena_allocate_size_zero),
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_128),
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_256),
        cmocka_unit_test(test_bc_allocators_arena_allocate_align_4096),
        cmocka_unit_test(test_bc_allocators_arena_allocate_alignment_zero),
        cmocka_unit_test(test_bc_allocators_arena_allocate_alignment_not_power_of_2),
        cmocka_unit_test(test_bc_allocators_arena_create_large_hugepage),
        cmocka_unit_test(test_bc_allocators_arena_destroy_tracking_disabled),
        cmocka_unit_test(test_bc_allocators_arena_reset_tracking_disabled_no_chunks),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
