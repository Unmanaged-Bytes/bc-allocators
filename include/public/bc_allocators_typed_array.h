// SPDX-License-Identifier: MIT

/*
 * BC_TYPED_ARRAY_DEFINE — typed array for internal module buffers.
 *
 * Purpose:
 *   Generic containers (bc_containers_vector_t) use runtime element_size and
 *   dispatch copies via bc_core_copy, which goes through a function
 *   pointer to the AVX2 implementation — even for 16-byte structs.  This
 *   macro generates a typed array whose push hot-path is a direct struct
 *   assignment that the compiler reduces to 1-2 store instructions.
 *
 * Usage rule:
 *   Use this macro for INTERNAL module buffers where the element type is
 *   known at design time.  Use bc_containers_vector_t for user-facing structures
 *   where the element type is provided by the caller.
 *
 * Example:
 *
 *   // in module .c — internal, never exposed in a public header
 *   BC_TYPED_ARRAY_DEFINE(my_entry_t, my_entry_array)
 *
 *   my_entry_array_t arr = {0};          // zero-init = valid empty array
 *   my_entry_array_push(mem, &arr, e);   // O(1) amortized, direct store
 *   my_entry_array_clear(&arr);          // arr->length = 0, O(1)
 *   my_entry_array_destroy(mem, &arr);   // free data buffer
 *
 * Struct layout:
 *   { element_type* data; size_t length; size_t capacity; }
 *   Embed by value in the owning struct — no extra heap allocation.
 *
 * Performance contract:
 *   push hot path:  length < capacity  →  data[length++] = value  (1-2 stores)
 *   push slow path: length == capacity →  double capacity, memcpy, then push
 *   The slow path is O(log N) total calls over the array's lifetime.
 */

#ifndef BC_ALLOCATORS_TYPED_ARRAY_H
#define BC_ALLOCATORS_TYPED_ARRAY_H

#include "bc_allocators.h"
#include "bc_allocators_pool.h"
#include "bc_core.h"

#include <stdbool.h>
#include <stddef.h>

/* ===== BC_TYPED_ARRAY_DEFINE(element_type, prefix) =====
 *
 * Generates:
 *   prefix_t                                     — the array struct
 *   prefix_length(arr)         → size_t          — current element count
 *   prefix_capacity(arr)       → size_t          — allocated slots
 *   prefix_data(arr)           → element_type*   — pointer to first element
 *   prefix_clear(arr)          → void            — reset length, keep capacity
 *   prefix_reserve(mem, arr, cap) → bool         — ensure capacity >= cap
 *   prefix_push(mem, arr, val) → bool            — append element (HOT PATH)
 *   prefix_destroy(mem, arr)   → void            — free data buffer
 */

#define BC_TYPED_ARRAY_DEFINE(element_type, prefix)                                                                                        \
                                                                                                                                           \
    typedef struct {                                                                                                                       \
        element_type* data;                                                                                                                \
        size_t length;                                                                                                                     \
        size_t capacity;                                                                                                                   \
    } prefix##_t;                                                                                                                          \
                                                                                                                                           \
    static inline __attribute__((unused)) size_t prefix##_length(const prefix##_t* arr)                                                    \
    {                                                                                                                                      \
        return arr->length;                                                                                                                \
    }                                                                                                                                      \
                                                                                                                                           \
    static inline __attribute__((unused)) size_t prefix##_capacity(const prefix##_t* arr)                                                  \
    {                                                                                                                                      \
        return arr->capacity;                                                                                                              \
    }                                                                                                                                      \
                                                                                                                                           \
    static inline __attribute__((unused)) element_type* prefix##_data(const prefix##_t* arr)                                               \
    {                                                                                                                                      \
        return arr->data;                                                                                                                  \
    }                                                                                                                                      \
                                                                                                                                           \
    static inline __attribute__((unused)) void prefix##_clear(prefix##_t* arr)                                                             \
    {                                                                                                                                      \
        arr->length = 0;                                                                                                                   \
    }                                                                                                                                      \
                                                                                                                                           \
    static inline __attribute__((unused)) void prefix##_destroy(bc_allocators_context_t* mem, prefix##_t* arr)                             \
    {                                                                                                                                      \
        bc_allocators_pool_free(mem, arr->data); /* no-op if NULL */                                                                       \
        arr->data = NULL;                                                                                                                  \
        arr->length = 0;                                                                                                                   \
        arr->capacity = 0;                                                                                                                 \
    }                                                                                                                                      \
                                                                                                                                           \
    static inline __attribute__((unused)) bool prefix##_reserve(bc_allocators_context_t* mem, prefix##_t* arr, size_t new_cap)             \
    {                                                                                                                                      \
        if (new_cap <= arr->capacity) {                                                                                                    \
            return true;                                                                                                                   \
        }                                                                                                                                  \
        size_t new_size = 0;                                                                                                               \
        if (!bc_core_safe_multiply(new_cap, sizeof(element_type), &new_size)) {                                                            \
            return false;                                                                                                                  \
        }                                                                                                                                  \
        element_type* new_data = NULL;                                                                                                     \
        if (!bc_allocators_pool_allocate(mem, new_size, (void**)&new_data)) {                                                              \
            return false;                                                                                                                  \
        }                                                                                                                                  \
        if (arr->length > 0) {                                                                                                             \
            /* Copy only valid elements. sizeof(element_type) is compile-time       */                                                     \
            /* constant here — the compiler emits an optimized loop or rep movs.    */                                                     \
            __builtin_memcpy(new_data, arr->data, arr->length * sizeof(element_type));                                                     \
        }                                                                                                                                  \
        bc_allocators_pool_free(mem, arr->data); /* no-op if NULL */                                                                       \
        arr->data = new_data;                                                                                                              \
        arr->capacity = new_cap;                                                                                                           \
        return true;                                                                                                                       \
    }                                                                                                                                      \
                                                                                                                                           \
    static inline __attribute__((unused)) bool prefix##_push(bc_allocators_context_t* mem, prefix##_t* arr, element_type value)            \
    {                                                                                                                                      \
        /* HOT PATH: direct assignment — compiler emits 1-2 stores for small   */                                                          \
        /* structs; no bc_core_copy, no function pointer, no AVX2 dispatch.  */                                                            \
        if (__builtin_expect(arr->length < arr->capacity, 1)) {                                                                            \
            arr->data[arr->length++] = value;                                                                                              \
            return true;                                                                                                                   \
        }                                                                                                                                  \
        /* SLOW PATH (O(log N) calls total): grow then push. */                                                                            \
        size_t new_cap = arr->capacity == 0 ? 8 : arr->capacity * 2;                                                                       \
        if (!prefix##_reserve(mem, arr, new_cap)) {                                                                                        \
            return false;                                                                                                                  \
        }                                                                                                                                  \
        arr->data[arr->length++] = value;                                                                                                  \
        return true;                                                                                                                       \
    }

#endif /* BC_ALLOCATORS_TYPED_ARRAY_H */
