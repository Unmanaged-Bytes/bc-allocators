// SPDX-License-Identifier: MIT

#include "bc_allocators.h"
#include "bc_allocators_arena.h"
#include "bc_allocators_pool.h"
#include "bc_allocators_slab.h"

#include "bc_core.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ===== Existing: pool alloc/free latency ===== */

static void bench_pool_alloc_free(const char* label, size_t alloc_size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        void* ptr = NULL;
        bc_allocators_pool_allocate(ctx, alloc_size, &ptr);
        bc_allocators_pool_free(ctx, ptr);
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  pool alloc+free %-6s  %5.1f ns/cycle  %.0f Mops/s\n", label, (double)elapsed / (double)iters,
           (double)iters / ((double)elapsed / 1e9) / 1e6);

    bc_allocators_context_destroy(ctx);
}

/* ===== Existing: pool batch alloc/free ===== */

static void bench_pool_batch(size_t alloc_size, size_t batch, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    void** ptrs = NULL;
    bc_allocators_pool_allocate(ctx, batch * sizeof(void*), (void**)&ptrs);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        for (size_t j = 0; j < batch; j++) {
            bc_allocators_pool_allocate(ctx, alloc_size, &ptrs[j]);
        }
        for (size_t j = 0; j < batch; j++) {
            bc_allocators_pool_free(ctx, ptrs[j]);
        }
    }
    uint64_t elapsed = now_ns() - t0;

    size_t total_ops = iters * batch * 2;
    printf("  pool batch %zu×%-4zu      %5.1f ns/op    %.0f Mops/s\n", batch, alloc_size, (double)elapsed / (double)total_ops,
           (double)total_ops / ((double)elapsed / 1e9) / 1e6);

    bc_allocators_pool_free(ctx, ptrs);
    bc_allocators_context_destroy(ctx);
}

/* ===== Existing: arena alloc latency ===== */

static void bench_arena_alloc(size_t alloc_size, size_t count)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_arena_t* arena = NULL;
    bc_allocators_arena_create(ctx, 1024 * 1024, &arena);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < count; i++) {
        void* ptr = NULL;
        bc_allocators_arena_allocate(arena, alloc_size, 8, &ptr);
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  arena alloc %-4zu       %5.1f ns/op    %.0f Mops/s\n", alloc_size, (double)elapsed / (double)count,
           (double)count / ((double)elapsed / 1e9) / 1e6);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Existing: arena fill+reset ===== */

static void bench_arena_reset(size_t fill_count, size_t reset_iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_arena_t* arena = NULL;
    bc_allocators_arena_create(ctx, 1024 * 1024, &arena);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < reset_iters; i++) {
        for (size_t j = 0; j < fill_count; j++) {
            void* ptr = NULL;
            bc_allocators_arena_allocate(arena, 64, 8, &ptr);
        }
        bc_allocators_arena_reset(arena);
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  arena fill(%zu)+reset  %5.0f ns/cycle  %.0f Kcycles/s\n", fill_count, (double)elapsed / (double)reset_iters,
           (double)reset_iters / ((double)elapsed / 1e9) / 1e3);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Existing: arena create+destroy ===== */

static void bench_arena_create_destroy(size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        bc_allocators_arena_t* arena = NULL;
        bc_allocators_arena_create(ctx, 64 * 1024, &arena);
        bc_allocators_arena_destroy(arena);
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  arena create+destroy   %5.0f ns/cycle  %.0f Kcycles/s\n", (double)elapsed / (double)iters,
           (double)iters / ((double)elapsed / 1e9) / 1e3);

    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.2: pool copy throughput ===== */

static void bench_pool_copy_throughput(const char* label, size_t size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    void* src = NULL;
    void* dst = NULL;
    bc_allocators_pool_allocate(ctx, size, &src);
    bc_allocators_pool_allocate(ctx, size, &dst);
    bc_core_fill(src, size, (unsigned char)0xAB);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        bc_core_copy(dst, src, size);
    }
    uint64_t elapsed = now_ns() - t0;

    double gb_per_s = ((double)size * (double)iters) / (double)elapsed;
    printf("  pool copy %-6s      %5.2f GB/s\n", label, gb_per_s);

    bc_allocators_pool_free(ctx, dst);
    bc_allocators_pool_free(ctx, src);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.2: arena copy throughput ===== */

static void bench_arena_copy_throughput(const char* label, size_t size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_arena_t* arena = NULL;
    bc_allocators_arena_create(ctx, size * 4, &arena);

    void* src = NULL;
    void* dst = NULL;
    bc_allocators_arena_allocate(arena, size, 32, &src);
    bc_allocators_arena_allocate(arena, size, 32, &dst);
    bc_core_fill(src, size, (unsigned char)0xCD);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        bc_core_copy(dst, src, size);
    }
    uint64_t elapsed = now_ns() - t0;

    double gb_per_s = ((double)size * (double)iters) / (double)elapsed;
    printf("  arena copy %-6s     %5.2f GB/s\n", label, gb_per_s);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.2: slab copy throughput ===== */

static void bench_slab_copy_throughput(size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_slab_t* slab = NULL;
    bc_allocators_slab_create(ctx, 64, 128, &slab);

    void* src = NULL;
    bc_allocators_pool_allocate(ctx, 64, &src);
    bc_core_fill(src, 64, (unsigned char)0xEF);

    void* objs[128];
    for (size_t i = 0; i < 128; i++) {
        bc_allocators_slab_allocate(slab, &objs[i]);
    }

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        for (size_t j = 0; j < 128; j++) {
            bc_core_copy(objs[j], src, 64);
        }
    }
    uint64_t elapsed = now_ns() - t0;

    size_t total_bytes = 64 * 128 * iters;
    double gb_per_s = (double)total_bytes / (double)elapsed;
    printf("  slab copy 64B×128     %5.2f GB/s\n", gb_per_s);

    for (size_t i = 0; i < 128; i++) {
        bc_allocators_slab_free(slab, objs[i]);
    }
    bc_allocators_pool_free(ctx, src);
    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.2: pool fill sweep ===== */

static void bench_pool_fill_sweep(const char* label, size_t size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    void* ptr = NULL;
    bc_allocators_pool_allocate(ctx, size, &ptr);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        bc_core_fill(ptr, size, (unsigned char)0x42);
    }
    uint64_t elapsed = now_ns() - t0;

    double gb_per_s = ((double)size * (double)iters) / (double)elapsed;
    printf("  pool fill %-6s      %5.2f GB/s\n", label, gb_per_s);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.2: pool zero sweep ===== */

static void bench_pool_zero_sweep(const char* label, size_t size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    void* ptr = NULL;
    bc_allocators_pool_allocate(ctx, size, &ptr);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        bc_core_zero(ptr, size);
    }
    uint64_t elapsed = now_ns() - t0;

    double gb_per_s = ((double)size * (double)iters) / (double)elapsed;
    printf("  pool zero %-6s      %5.2f GB/s\n", label, gb_per_s);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.2: arena reset secure ===== */

static void bench_arena_reset_secure(size_t arena_size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_arena_t* arena = NULL;
    bc_allocators_arena_create(ctx, arena_size, &arena);

    void* ptr = NULL;
    bc_allocators_arena_allocate(arena, arena_size, 32, &ptr);
    bc_core_fill(ptr, arena_size, (unsigned char)0xFF);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        bc_allocators_arena_reset_secure(arena);
        bc_allocators_arena_allocate(arena, arena_size, 32, &ptr);
    }
    uint64_t elapsed = now_ns() - t0;

    double gb_per_s = ((double)arena_size * (double)iters) / (double)elapsed;
    printf("  arena reset_secure 4MB %5.2f GB/s\n", gb_per_s);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.3: slab alloc+free latency ===== */

static void bench_slab_alloc_free(const char* label, size_t object_size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_slab_t* slab = NULL;
    bc_allocators_slab_create(ctx, object_size, 128, &slab);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        void* ptr = NULL;
        bc_allocators_slab_allocate(slab, &ptr);
        bc_allocators_slab_free(slab, ptr);
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  slab alloc+free %-5s %5.1f ns/op    %.0f Mops/s\n", label, (double)elapsed / (double)iters,
           (double)iters / ((double)elapsed / 1e9) / 1e6);

    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.3: slab batch alloc+free ===== */

static void bench_slab_batch(size_t object_size, size_t batch, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_slab_t* slab = NULL;
    bc_allocators_slab_create(ctx, object_size, batch, &slab);

    void** ptrs = NULL;
    bc_allocators_pool_allocate(ctx, batch * sizeof(void*), (void**)&ptrs);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        for (size_t j = 0; j < batch; j++) {
            bc_allocators_slab_allocate(slab, &ptrs[j]);
        }
        for (size_t j = 0; j < batch; j++) {
            bc_allocators_slab_free(slab, ptrs[j]);
        }
    }
    uint64_t elapsed = now_ns() - t0;

    size_t total_ops = iters * batch * 2;
    printf("  slab batch %zu×%-4zu      %5.1f ns/op    %.0f Mops/s\n", batch, object_size, (double)elapsed / (double)total_ops,
           (double)total_ops / ((double)elapsed / 1e9) / 1e6);

    bc_allocators_pool_free(ctx, ptrs);
    bc_allocators_slab_destroy(slab);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.3: slab create+destroy ===== */

static void bench_slab_create_destroy(size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        bc_allocators_slab_t* slab = NULL;
        bc_allocators_slab_create(ctx, 64, 128, &slab);
        bc_allocators_slab_destroy(slab);
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  slab create+destroy    %5.0f ns/cycle  %.0f Kcycles/s\n", (double)elapsed / (double)iters,
           (double)iters / ((double)elapsed / 1e9) / 1e3);

    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.3: pool reallocate ===== */

static void bench_pool_reallocate(const char* label, size_t from_size, size_t to_size, size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    void* ptr = NULL;
    bc_allocators_pool_allocate(ctx, from_size, &ptr);
    bc_core_fill(ptr, from_size, (unsigned char)0x55);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        void* new_ptr = NULL;
        bc_allocators_pool_reallocate(ctx, ptr, to_size, &new_ptr);
        ptr = new_ptr;
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  pool reallocate %-8s %5.1f ns/op    %.0f Mops/s\n", label, (double)elapsed / (double)iters,
           (double)iters / ((double)elapsed / 1e9) / 1e6);

    bc_allocators_pool_free(ctx, ptr);
    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.3: arena alloc alignment sweep ===== */

static void bench_arena_alloc_alignment_sweep(void)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    static const size_t alignments[] = {1, 8, 16, 32, 64, 128};
    static const size_t num_alignments = sizeof(alignments) / sizeof(alignments[0]);

    for (size_t a = 0; a < num_alignments; a++) {
        bc_allocators_arena_t* arena = NULL;
        bc_allocators_arena_create(ctx, 16 * 1024 * 1024, &arena);

        size_t count = 1000000;
        uint64_t t0 = now_ns();
        for (size_t i = 0; i < count; i++) {
            void* ptr = NULL;
            bc_allocators_arena_allocate(arena, 64, alignments[a], &ptr);
        }
        uint64_t elapsed = now_ns() - t0;

        printf("  arena alloc align=%-4zu  %5.1f ns/op\n", alignments[a], (double)elapsed / (double)count);

        bc_allocators_arena_destroy(arena);
    }

    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.3: pool fragmentation pattern ===== */

static void bench_pool_fragmentation(size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        void* a = NULL;
        void* b = NULL;
        void* c = NULL;
        bc_allocators_pool_allocate(ctx, 64, &a);
        bc_allocators_pool_allocate(ctx, 128, &b);
        bc_allocators_pool_free(ctx, a);
        bc_allocators_pool_allocate(ctx, 64, &c);
        bc_allocators_pool_free(ctx, b);
        bc_allocators_pool_free(ctx, c);
    }
    uint64_t elapsed = now_ns() - t0;

    printf("  pool fragmentation     %5.1f ns/cycle\n", (double)elapsed / (double)iters);

    bc_allocators_context_destroy(ctx);
}

/* ===== Item 2.4: arena release pages ===== */

static void bench_arena_release_pages(size_t iters)
{
    bc_allocators_context_t* ctx = NULL;
    bc_allocators_context_create(NULL, &ctx);

    bc_allocators_arena_t* arena = NULL;
    bc_allocators_arena_create(ctx, 2 * 1024 * 1024, &arena);

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        void* ptr = NULL;
        bc_allocators_arena_allocate(arena, 64, 8, &ptr);
        bc_allocators_arena_reset(arena);
    }
    uint64_t elapsed_no_release = now_ns() - t0;

    t0 = now_ns();
    for (size_t i = 0; i < iters; i++) {
        void* ptr = NULL;
        bc_allocators_arena_allocate(arena, 64, 8, &ptr);
        bc_allocators_arena_reset(arena);
        bc_allocators_arena_release_pages(arena);
    }
    uint64_t elapsed_release = now_ns() - t0;

    printf("  arena reset 2MB  no_release: %6.0f ns/op  with_release: %6.0f ns/op\n", (double)elapsed_no_release / (double)iters,
           (double)elapsed_release / (double)iters);

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
}

/* ===== main ===== */

int main(void)
{
    printf("bench_memory_ops\n\n");

    /* Existing: pool and arena latency */
    bench_pool_alloc_free("64B", 64, 10000000);
    bench_pool_alloc_free("256B", 256, 10000000);
    bench_pool_alloc_free("4KB", 4096, 5000000);
    bench_pool_alloc_free("64KB", 65536, 1000000);
    printf("\n");
    bench_pool_batch(64, 100, 100000);
    bench_pool_batch(64, 1000, 10000);
    printf("\n");
    bench_arena_alloc(32, 10000000);
    bench_arena_alloc(64, 10000000);
    bench_arena_alloc(256, 5000000);
    printf("\n");
    bench_arena_reset(1000, 100000);
    bench_arena_create_destroy(100000);
    printf("\n");

    /* Item 2.2: throughput — copy/fill/zero on pool/arena/slab buffers */
    printf("--- item 2.2: throughput ---\n");
    bench_pool_copy_throughput("64B", 64, 10000000);
    bench_pool_copy_throughput("4KB", 4096, 1000000);
    bench_pool_copy_throughput("64KB", 65536, 100000);
    bench_pool_copy_throughput("1MB", 1048576, 10000);
    printf("\n");
    bench_arena_copy_throughput("4KB", 4096, 1000000);
    bench_arena_copy_throughput("64KB", 65536, 100000);
    bench_arena_copy_throughput("1MB", 1048576, 10000);
    printf("\n");
    bench_slab_copy_throughput(100000);
    printf("\n");
    bench_pool_fill_sweep("64B", 64, 10000000);
    bench_pool_fill_sweep("4KB", 4096, 1000000);
    bench_pool_fill_sweep("64KB", 65536, 100000);
    bench_pool_fill_sweep("1MB", 1048576, 10000);
    printf("\n");
    bench_pool_zero_sweep("64B", 64, 10000000);
    bench_pool_zero_sweep("4KB", 4096, 1000000);
    bench_pool_zero_sweep("64KB", 65536, 100000);
    bench_pool_zero_sweep("1MB", 1048576, 10000);
    printf("\n");
    bench_arena_reset_secure(4 * 1024 * 1024, 100);
    printf("\n");

    /* Item 2.3: slab and extended allocator latency */
    printf("--- item 2.3: allocator latency ---\n");
    bench_slab_alloc_free("32B", 32, 10000000);
    bench_slab_alloc_free("64B", 64, 10000000);
    bench_slab_alloc_free("128B", 128, 10000000);
    bench_slab_alloc_free("256B", 256, 5000000);
    printf("\n");
    bench_slab_batch(64, 100, 100000);
    bench_slab_batch(64, 1000, 10000);
    printf("\n");
    bench_slab_create_destroy(100000);
    printf("\n");
    bench_pool_reallocate("64→128B", 64, 128, 5000000);
    bench_pool_reallocate("4→8KB", 4096, 8192, 500000);
    printf("\n");
    bench_arena_alloc_alignment_sweep();
    printf("\n");
    bench_pool_fragmentation(5000000);
    printf("\n");

    /* Item 2.4: mmap benchmarks */
    printf("--- item 2.4: mmap ---\n");
    bench_arena_release_pages(10000);

    return 0;
}
