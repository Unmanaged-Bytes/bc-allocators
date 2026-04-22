// SPDX-License-Identifier: MIT

#include "bc_allocators.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    bc_allocators_context_t* ctx = NULL;
    if (!bc_allocators_context_create(NULL, &ctx)) {
        return 0;
    }

    bc_allocators_arena_t* arena = NULL;
    if (!bc_allocators_arena_create(ctx, BC_ALLOCATORS_ARENA_SMALL_CAPACITY, &arena)) {
        bc_allocators_context_destroy(ctx);
        return 0;
    }

    for (size_t i = 0; i + 2 < size; i += 3) {
        uint8_t op = data[i] & 0x7;
        size_t requested = ((size_t)data[i + 1] << 3) | (size_t)data[i + 2];
        if (requested == 0) {
            requested = 1;
        }
        size_t alignment = (size_t)1 << ((data[i + 2] & 0x7));

        if (op == 0) {
            void* ptr = NULL;
            bc_allocators_arena_allocate(arena, requested, alignment, &ptr);
        } else if (op == 1) {
            char buffer[64];
            size_t n = requested < sizeof(buffer) - 1 ? requested : sizeof(buffer) - 1;
            for (size_t k = 0; k < n; k++) {
                buffer[k] = (char)(data[(i + k) % size] | 1);
            }
            buffer[n] = '\0';
            const char* copy = NULL;
            bc_allocators_arena_copy_string(arena, buffer, &copy);
        } else if (op == 2) {
            bc_allocators_arena_reset(arena);
        } else if (op == 3) {
            bc_allocators_arena_reset_secure(arena);
        } else if (op == 4) {
            bc_allocators_arena_release_pages(arena);
        } else if (op == 5) {
            bc_allocators_arena_stats_t stats;
            bc_allocators_arena_get_stats(arena, &stats);
        }
    }

    bc_allocators_arena_destroy(arena);
    bc_allocators_context_destroy(ctx);
    return 0;
}

#ifndef BC_FUZZ_LIBFUZZER
int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <iterations> [seed]\n", argv[0]);
        return 2;
    }
    unsigned long iterations = strtoul(argv[1], NULL, 10);
    unsigned long seed = (argc >= 3) ? strtoul(argv[2], NULL, 10) : 0;
    srand((unsigned int)seed);

    uint8_t buffer[2048];
    for (unsigned long i = 0; i < iterations; i++) {
        size_t len = (size_t)(rand() % (int)sizeof(buffer));
        for (size_t j = 0; j < len; j++) {
            buffer[j] = (uint8_t)(rand() & 0xFF);
        }
        LLVMFuzzerTestOneInput(buffer, len);
    }
    return 0;
}
#endif
