// SPDX-License-Identifier: MIT

#ifndef BC_ALLOCATORS_PLATFORM_INTERNAL_H
#define BC_ALLOCATORS_PLATFORM_INTERNAL_H

#include <sys/mman.h>

#include <stdbool.h>
#include <stddef.h>

#define BC_ALLOCATORS_ADVISE_DONTNEED MADV_DONTNEED

bool bc_allocators_platform_map(size_t size, void** out_pointer);
bool bc_allocators_platform_unmap(void* pointer, size_t size);
bool bc_allocators_platform_get_page_size(size_t* out_page_size);
bool bc_allocators_platform_get_cache_line_size(size_t* out_cache_line_size);
int bc_allocators_platform_advise(void* addr, size_t len, int advice);

#endif /* BC_ALLOCATORS_PLATFORM_INTERNAL_H */
