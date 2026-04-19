// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_SLAB_H
#define BC_ALLOCATORS_SLAB_H

#include "bc_allocators_context.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct bc_allocators_slab bc_allocators_slab_t;

typedef struct bc_allocators_slab_stats {
    size_t object_size;
    size_t objects_per_slab;
    size_t slab_count;
    size_t total_objects;
    size_t used_objects;
    size_t free_objects;
} bc_allocators_slab_stats_t;

bool bc_allocators_slab_create(bc_allocators_context_t* memory, size_t object_size, size_t objects_per_slab,
                               bc_allocators_slab_t** out_slab);
void bc_allocators_slab_destroy(bc_allocators_slab_t* slab);
// Returns a pointer aligned to the next power of two >= object_size.
bool bc_allocators_slab_allocate(bc_allocators_slab_t* slab, void** out_ptr);
void bc_allocators_slab_free(bc_allocators_slab_t* slab, void* ptr);
bool bc_allocators_slab_get_stats(const bc_allocators_slab_t* slab, bc_allocators_slab_stats_t* out_stats);

#endif /* BC_ALLOCATORS_SLAB_H */
