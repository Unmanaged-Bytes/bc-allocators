// SPDX-License-Identifier: MIT
#ifndef BC_ALLOCATORS_SLAB_INTERNAL_H
#define BC_ALLOCATORS_SLAB_INTERNAL_H

#include "bc_allocators.h"

#include <stddef.h>

typedef struct bc_allocators_slab_page {
    void* objects;
    struct bc_allocators_slab_page* next;
} bc_allocators_slab_page_t;

struct bc_allocators_slab {
    bc_allocators_context_t* memory;
    size_t object_size;
    size_t objects_per_slab;
    void* free_list_head;
    bc_allocators_slab_page_t* pages;
    size_t slab_count;
    size_t used_objects;
};

#endif
