// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "bc_allocators.h"
#include "bc_allocators_slab.h"

#include "bc_allocators_slab_internal.h"

#include <stdbool.h>

/* ===== Helpers ===== */

static bc_allocators_context_t* create_default_ctx(void)
{
    bc_allocators_context_t* ctx = NULL;
    bool ok = bc_allocators_context_create(NULL, &ctx);
    assert_true(ok);
    assert_non_null(ctx);
    return ctx;
}

/* ===== create and destroy ===== */

static void test_bc_allocators_slab_create_and_destroy(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, sizeof(size_t), 4, &slab));
    assert_non_null(slab);

    bc_allocators_slab_stats_t stats = {0};
    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.object_size, sizeof(size_t));
    assert_int_equal(stats.objects_per_slab, 4);
    assert_int_equal(stats.slab_count, 1);
    assert_int_equal(stats.total_objects, 4);
    assert_int_equal(stats.used_objects, 0);
    assert_int_equal(stats.free_objects, 4);

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== create: object_size too small ===== */

static void test_bc_allocators_slab_create_invalid_object_size(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_false(bc_allocators_slab_create(ctx, sizeof(size_t) - 1, 4, &slab));
    assert_null(slab);

    bc_allocators_context_destroy(ctx);
}

/* ===== create: objects_per_slab zero ===== */

static void test_bc_allocators_slab_create_invalid_objects_per_slab(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_false(bc_allocators_slab_create(ctx, sizeof(size_t), 0, &slab));
    assert_null(slab);

    bc_allocators_context_destroy(ctx);
}

/* ===== alloc single and free ===== */

static void test_bc_allocators_slab_allocate_single_and_free(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, sizeof(size_t), 4, &slab));

    void* ptr = NULL;
    assert_true(bc_allocators_slab_allocate(slab, &ptr));
    assert_non_null(ptr);

    bc_allocators_slab_stats_t stats = {0};
    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.used_objects, 1);
    assert_int_equal(stats.free_objects, 3);

    bc_allocators_slab_free(slab, ptr);

    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.used_objects, 0);
    assert_int_equal(stats.free_objects, 4);

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc exhausts page, triggers new page ===== */

static void test_bc_allocators_slab_allocate_fills_page_and_spills(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, sizeof(size_t), 2, &slab));

    void* ptrs[5];

    assert_true(bc_allocators_slab_allocate(slab, &ptrs[0]));
    assert_non_null(ptrs[0]);
    assert_true(bc_allocators_slab_allocate(slab, &ptrs[1]));
    assert_non_null(ptrs[1]);

    bc_allocators_slab_stats_t stats = {0};
    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.slab_count, 1);
    assert_int_equal(stats.used_objects, 2);
    assert_int_equal(stats.free_objects, 0);

    assert_true(bc_allocators_slab_allocate(slab, &ptrs[2]));
    assert_non_null(ptrs[2]);

    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.slab_count, 2);
    assert_int_equal(stats.total_objects, 4);
    assert_int_equal(stats.used_objects, 3);
    assert_int_equal(stats.free_objects, 1);

    assert_true(bc_allocators_slab_allocate(slab, &ptrs[3]));
    assert_true(bc_allocators_slab_allocate(slab, &ptrs[4]));

    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.slab_count, 3);
    assert_int_equal(stats.used_objects, 5);

    for (int i = 0; i < 5; i++) {
        bc_allocators_slab_free(slab, ptrs[i]);
    }

    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.used_objects, 0);
    assert_int_equal(stats.free_objects, 6);

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== freed slot is reused (LIFO) ===== */

static void test_bc_allocators_slab_reuse_freed_slot(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, sizeof(size_t), 4, &slab));

    void* ptr1 = NULL;
    assert_true(bc_allocators_slab_allocate(slab, &ptr1));
    assert_non_null(ptr1);

    bc_allocators_slab_free(slab, ptr1);

    void* ptr2 = NULL;
    assert_true(bc_allocators_slab_allocate(slab, &ptr2));
    assert_ptr_equal(ptr1, ptr2);

    bc_allocators_slab_free(slab, ptr2);

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== destroy with multiple pages (allocated objects not freed) ===== */

static void test_bc_allocators_slab_destroy_with_multi_pages(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, sizeof(size_t), 2, &slab));

    void* ptrs[3];
    for (int i = 0; i < 3; i++) {
        assert_true(bc_allocators_slab_allocate(slab, &ptrs[i]));
    }

    assert_int_equal(slab->slab_count, 2);
    assert_non_null(slab->pages);
    assert_non_null(slab->pages->next);

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== internal struct access ===== */

static void test_bc_allocators_slab_internal_struct_access(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, 24, 8, &slab));

    assert_ptr_equal(slab->memory, ctx);
    assert_int_equal(slab->object_size, 24);
    assert_int_equal(slab->objects_per_slab, 8);
    assert_int_equal(slab->slab_count, 1);
    assert_int_equal(slab->used_objects, 0);
    assert_non_null(slab->free_list_head);
    assert_non_null(slab->pages);
    assert_null(slab->pages->next);
    assert_non_null(slab->pages->objects);

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== alloc/free cycle: free-list reuse without new page (item 2.3) ===== */

static void test_slab_alloc_free_cycle(void** state)
{
    (void)state;
    bc_allocators_context_t* ctx = create_default_ctx();

    bc_allocators_slab_t* slab = NULL;
    assert_true(bc_allocators_slab_create(ctx, 64, 128, &slab));
    assert_non_null(slab);

    void* ptrs[128];
    for (int i = 0; i < 128; i++) {
        ptrs[i] = NULL;
        assert_true(bc_allocators_slab_allocate(slab, &ptrs[i]));
        assert_non_null(ptrs[i]);
    }

    bc_allocators_slab_stats_t stats = {0};
    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.slab_count, 1);
    assert_int_equal(stats.used_objects, 128);
    assert_int_equal(stats.free_objects, 0);

    for (int i = 0; i < 128; i++) {
        bc_allocators_slab_free(slab, ptrs[i]);
    }

    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.slab_count, 1);
    assert_int_equal(stats.used_objects, 0);
    assert_int_equal(stats.free_objects, 128);

    void* realloc_ptrs[128];
    for (int i = 0; i < 128; i++) {
        realloc_ptrs[i] = NULL;
        assert_true(bc_allocators_slab_allocate(slab, &realloc_ptrs[i]));
        assert_non_null(realloc_ptrs[i]);
    }

    assert_true(bc_allocators_slab_get_stats(slab, &stats));
    assert_int_equal(stats.slab_count, 1);
    assert_int_equal(stats.used_objects, 128);

    for (int i = 0; i < 128; i++) {
        bc_allocators_slab_free(slab, realloc_ptrs[i]);
    }

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bc_allocators_slab_create_and_destroy),
        cmocka_unit_test(test_bc_allocators_slab_create_invalid_object_size),
        cmocka_unit_test(test_bc_allocators_slab_create_invalid_objects_per_slab),
        cmocka_unit_test(test_bc_allocators_slab_allocate_single_and_free),
        cmocka_unit_test(test_bc_allocators_slab_allocate_fills_page_and_spills),
        cmocka_unit_test(test_bc_allocators_slab_reuse_freed_slot),
        cmocka_unit_test(test_bc_allocators_slab_destroy_with_multi_pages),
        cmocka_unit_test(test_bc_allocators_slab_internal_struct_access),
        /* item 2.3: alloc/free cycle LIFO reuse */
        cmocka_unit_test(test_slab_alloc_free_cycle),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
