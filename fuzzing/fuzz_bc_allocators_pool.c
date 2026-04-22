// SPDX-License-Identifier: MIT

#include "bc_allocators.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_MAX_LIVE 128

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    bc_allocators_context_t* ctx = NULL;
    if (!bc_allocators_context_create(NULL, &ctx)) {
        return 0;
    }

    void* live[FUZZ_MAX_LIVE] = {0};
    size_t live_sizes[FUZZ_MAX_LIVE] = {0};
    size_t live_count = 0;

    for (size_t i = 0; i + 2 < size; i += 3) {
        uint8_t op = data[i] & 0x3;
        size_t requested = ((size_t)data[i + 1] << 4) | (size_t)data[i + 2];
        if (requested == 0) {
            requested = 1;
        }
        if (requested > (4U << 20)) {
            requested = 4U << 20;
        }

        if (op == 0 && live_count < FUZZ_MAX_LIVE) {
            void* ptr = NULL;
            if (bc_allocators_pool_allocate(ctx, requested, &ptr) && ptr != NULL) {
                live[live_count] = ptr;
                live_sizes[live_count] = requested;
                live_count++;
            }
        } else if (op == 1 && live_count > 0) {
            size_t idx = (size_t)data[i + 1] % live_count;
            void* new_ptr = NULL;
            if (bc_allocators_pool_reallocate(ctx, live[idx], requested, &new_ptr) && new_ptr != NULL) {
                live[idx] = new_ptr;
                live_sizes[idx] = requested;
            }
        } else if (op == 2 && live_count > 0) {
            size_t idx = (size_t)data[i + 1] % live_count;
            bc_allocators_pool_free(ctx, live[idx]);
            live[idx] = live[live_count - 1];
            live_sizes[idx] = live_sizes[live_count - 1];
            live_count--;
        }
    }

    for (size_t j = 0; j < live_count; j++) {
        bc_allocators_pool_free(ctx, live[j]);
    }
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
