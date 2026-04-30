// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_FREE_LIST_INTERNAL_H
#define BC_ALLOCATORS_FREE_LIST_INTERNAL_H

#include <stddef.h>

static inline void bc_allocators_free_list_push(void** head, void* block)
{
    *(void**)block = *head;
    *head = block;
}

static inline void* bc_allocators_free_list_pop(void** head)
{
    void* block = *head;
    if (block != NULL) {
        *head = *(void**)block;
    }
    return block;
}

#endif /* BC_ALLOCATORS_FREE_LIST_INTERNAL_H */
