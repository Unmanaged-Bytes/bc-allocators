// SPDX-License-Identifier: MIT

#include "bc_allocators.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern void* __libc_malloc(size_t size);
extern void __libc_free(void* ptr);

extern void* mallocx(size_t size, int flags);
extern void sdallocx(void* ptr, size_t size, int flags);

extern void* mi_malloc(size_t size);
extern void mi_free(void* ptr);

typedef void* (*alloc_fn_t)(void* state, size_t size);
typedef void (*free_fn_t)(void* state, void* ptr, size_t size);

typedef struct {
    const char* name;
    alloc_fn_t alloc_op;
    free_fn_t free_op;
    void* state;
} allocator_t;

static void* glibc_alloc(void* state, size_t size)
{
    (void)state;
    return __libc_malloc(size);
}

static void glibc_free(void* state, void* ptr, size_t size)
{
    (void)state;
    (void)size;
    __libc_free(ptr);
}

static void* jemalloc_alloc(void* state, size_t size)
{
    (void)state;
    return mallocx(size, 0);
}

static void jemalloc_free(void* state, void* ptr, size_t size)
{
    (void)state;
    sdallocx(ptr, size, 0);
}

static void* mimalloc_alloc(void* state, size_t size)
{
    (void)state;
    return mi_malloc(size);
}

static void mimalloc_free(void* state, void* ptr, size_t size)
{
    (void)state;
    (void)size;
    mi_free(ptr);
}

static void* bc_pool_alloc(void* state, size_t size)
{
    bc_allocators_context_t* ctx = (bc_allocators_context_t*)state;
    void* p = NULL;
    bc_allocators_pool_allocate(ctx, size, &p);
    return p;
}

static void bc_pool_free(void* state, void* ptr, size_t size)
{
    (void)size;
    bc_allocators_pool_free((bc_allocators_context_t*)state, ptr);
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void use_ptr(void* p)
{
    __asm__ volatile("" : "+r"(p));
}

static double pattern_uniform(const allocator_t* a, size_t size, size_t iters)
{
    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        void* p = a->alloc_op(a->state, size);
        use_ptr(p);
        a->free_op(a->state, p, size);
    }
    uint64_t e = now_ns() - t0;
    return (double)e / (double)iters;
}

static double pattern_lifo(const allocator_t* a, size_t size, size_t depth, size_t iters, void** scratch)
{
    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        for (size_t j = 0; j < depth; j++) {
            scratch[j] = a->alloc_op(a->state, size);
            use_ptr(scratch[j]);
        }
        for (size_t j = depth; j-- > 0;) {
            a->free_op(a->state, scratch[j], size);
        }
    }
    uint64_t e = now_ns() - t0;
    size_t total_ops = iters * depth * 2;
    return (double)e / (double)total_ops;
}

static double pattern_random(const allocator_t* a, size_t size, size_t live_max, size_t total_ops, void** scratch, uint64_t seed)
{
    uint64_t s = seed ? seed : 0x123456789ULL;
    size_t live = 0;
    uint64_t t0 = now_ns();
    for (size_t i = 0; i < total_ops; i++) {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        bool do_alloc = (live == 0) || (live < live_max && (s & 1));
        if (do_alloc) {
            scratch[live] = a->alloc_op(a->state, size);
            use_ptr(scratch[live]);
            live++;
        } else {
            size_t idx = (size_t)(s >> 1) % live;
            a->free_op(a->state, scratch[idx], size);
            scratch[idx] = scratch[--live];
        }
    }
    while (live > 0) {
        a->free_op(a->state, scratch[--live], size);
    }
    uint64_t e = now_ns() - t0;
    return (double)e / (double)total_ops;
}

int main(void)
{
    printf("bench_bc_allocators_vs_external\n\n");

    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    allocator_t glibc_a = {"glibc", glibc_alloc, glibc_free, NULL};
    allocator_t jemalloc_a = {"jemalloc", jemalloc_alloc, jemalloc_free, NULL};
    allocator_t mimalloc_a = {"mimalloc", mimalloc_alloc, mimalloc_free, NULL};
    allocator_t bcpool_a = {"bc-pool", bc_pool_alloc, bc_pool_free, ctx};

    const allocator_t* all[4] = {&glibc_a, &jemalloc_a, &mimalloc_a, &bcpool_a};
    int avail[4] = {1, 1, 1, 1};

    static const size_t sizes[] = {16, 64, 256, 4096};
    static const size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    void** scratch = (void**)__libc_malloc(sizeof(void*) * 4096);

    printf("\n--- pattern uniform: alloc + immediate free ---\n");
    for (size_t s = 0; s < num_sizes; s++) {
        size_t iters = sizes[s] <= 256 ? 5000000 : 1000000;
        double base = 0.0;
        printf("  size=%-5zu", sizes[s]);
        for (int i = 0; i < 4; i++) {
            if (!avail[i]) {
                printf("  %-9s    n/a       ", all[i]->name);
                continue;
            }
            double ns = pattern_uniform(all[i], sizes[s], iters);
            if (i == 0) {
                base = ns;
            }
            printf("  %-9s=%6.1f ns (%.2fx)", all[i]->name, ns, base / ns);
        }
        printf("\n");
    }

    printf("\n--- pattern LIFO: alloc N, free reverse ---\n");
    static const size_t depths[] = {64, 1024};
    for (size_t d = 0; d < 2; d++) {
        for (size_t s = 0; s < num_sizes; s++) {
            size_t iters = depths[d] >= 1024 ? 1000 : 50000;
            double base = 0.0;
            printf("  depth=%-4zu size=%-5zu", depths[d], sizes[s]);
            for (int i = 0; i < 4; i++) {
                if (!avail[i]) {
                    printf("  %-9s    n/a       ", all[i]->name);
                    continue;
                }
                double ns = pattern_lifo(all[i], sizes[s], depths[d], iters, scratch);
                if (i == 0) {
                    base = ns;
                }
                printf("  %-9s=%6.1f ns (%.2fx)", all[i]->name, ns, base / ns);
            }
            printf("\n");
        }
    }

    printf("\n--- pattern random: alloc/free mixed (max live=512) ---\n");
    for (size_t s = 0; s < num_sizes; s++) {
        size_t total = 5000000;
        double base = 0.0;
        printf("  size=%-5zu", sizes[s]);
        for (int i = 0; i < 4; i++) {
            if (!avail[i]) {
                printf("  %-9s    n/a       ", all[i]->name);
                continue;
            }
            double ns = pattern_random(all[i], sizes[s], 512, total, scratch, (uint64_t)0xCAFE0001 + (uint64_t)i);
            if (i == 0) {
                base = ns;
            }
            printf("  %-9s=%6.1f ns (%.2fx)", all[i]->name, ns, base / ns);
        }
        printf("\n");
    }

    __libc_free(scratch);
    bc_allocators_context_destroy(ctx);
    return 0;
}
