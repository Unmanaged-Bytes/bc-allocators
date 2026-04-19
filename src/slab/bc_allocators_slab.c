// SPDX-License-Identifier: MIT

#include "bc_allocators_slab.h"

#include "bc_allocators_slab_internal.h"

#include "bc_allocators_pool.h"

#include <stdbool.h>
#include <stddef.h>

static bool allocate_new_page(bc_allocators_slab_t* slab)
{
    void* page_ptr = NULL;
    if (!bc_allocators_pool_allocate(slab->memory, sizeof(bc_allocators_slab_page_t), &page_ptr)) {
        return false;
    }
    bc_allocators_slab_page_t* page = (bc_allocators_slab_page_t*)page_ptr;

    void* objects_ptr = NULL;
    if (!bc_allocators_pool_allocate(slab->memory, slab->objects_per_slab * slab->object_size, &objects_ptr)) {
        bc_allocators_pool_free(slab->memory, page_ptr);
        return false;
    }
    page->objects = objects_ptr;
    page->next = slab->pages;
    slab->pages = page;
    slab->slab_count++;

    void* first_slot = objects_ptr;
    *(void**)first_slot = slab->free_list_head;
    slab->free_list_head = first_slot;

    for (size_t slot_index = 1; slot_index < slab->objects_per_slab; slot_index++) {
        void* slot = (unsigned char*)objects_ptr + slot_index * slab->object_size;
        *(void**)slot = slab->free_list_head;
        slab->free_list_head = slot;
    }

    return true;
}

bool bc_allocators_slab_create(bc_allocators_context_t* memory, size_t object_size, size_t objects_per_slab,
                               bc_allocators_slab_t** out_slab)
{
    *out_slab = NULL;

    if (object_size < sizeof(void*) || objects_per_slab == 0) {
        return false;
    }

    void* slab_ptr = NULL;
    if (!bc_allocators_pool_allocate(memory, sizeof(bc_allocators_slab_t), &slab_ptr)) {
        return false;
    }

    bc_allocators_slab_t* slab = (bc_allocators_slab_t*)slab_ptr;
    slab->memory = memory;
    slab->object_size = object_size;
    slab->objects_per_slab = objects_per_slab;
    slab->free_list_head = NULL;
    slab->pages = NULL;
    slab->slab_count = 0;
    slab->used_objects = 0;

    if (!allocate_new_page(slab)) {
        bc_allocators_pool_free(memory, slab_ptr);
        return false;
    }

    *out_slab = slab;
    return true;
}

void bc_allocators_slab_destroy(bc_allocators_slab_t* slab)
{
    bc_allocators_context_t* memory = slab->memory;

    bc_allocators_slab_page_t* page = slab->pages;
    while (page != NULL) {
        bc_allocators_slab_page_t* next = page->next;
        bc_allocators_pool_free(memory, page->objects);
        bc_allocators_pool_free(memory, page);
        page = next;
    }

    bc_allocators_pool_free(memory, slab);
}

bool bc_allocators_slab_allocate(bc_allocators_slab_t* slab, void** out_ptr)
{
    *out_ptr = NULL;

    if (slab->free_list_head == NULL) {
        if (!allocate_new_page(slab)) {
            return false;
        }
    }

    *out_ptr = slab->free_list_head;
    slab->free_list_head = *(void**)slab->free_list_head;
    slab->used_objects++;
    return true;
}

void bc_allocators_slab_free(bc_allocators_slab_t* slab, void* ptr)
{
    *(void**)ptr = slab->free_list_head;
    slab->free_list_head = ptr;
    slab->used_objects--;
}

bool bc_allocators_slab_get_stats(const bc_allocators_slab_t* slab, bc_allocators_slab_stats_t* out_stats)
{
    out_stats->object_size = slab->object_size;
    out_stats->objects_per_slab = slab->objects_per_slab;
    out_stats->slab_count = slab->slab_count;
    out_stats->total_objects = slab->slab_count * slab->objects_per_slab;
    out_stats->used_objects = slab->used_objects;
    out_stats->free_objects = out_stats->total_objects - slab->used_objects;
    return true;
}
