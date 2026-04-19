// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_POOL_H
#define BC_ALLOCATORS_POOL_H

#include "bc_allocators_context.h"

#include <stdbool.h>
#include <stddef.h>

/* ===== Pool allocator ===== */

// Returns a pointer aligned to min(size_class_size, 4096) bytes.
bool bc_allocators_pool_allocate(bc_allocators_context_t* ctx, size_t size, void** out_ptr);
bool bc_allocators_pool_reallocate(bc_allocators_context_t* ctx, void* ptr, size_t new_size, void** out_ptr);
void bc_allocators_pool_free(bc_allocators_context_t* ctx, void* ptr);

#endif /* BC_ALLOCATORS_POOL_H */
