// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google LLC.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef HPB_COLLECTIONS_INTERNAL_ARRAY_H_
#define HPB_COLLECTIONS_INTERNAL_ARRAY_H_

#include <string.h>

#include "hpb/collections/array.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// LINT.IfChange(struct_definition)
// Our internal representation for repeated fields.
struct hpb_Array {
  uintptr_t data;  /* Tagged ptr: low 3 bits of ptr are lg2(elem size). */
  size_t size;     /* The number of elements in the array. */
  size_t capacity; /* Allocated storage. Measured in elements. */
};
// LINT.ThenChange(GoogleInternalName1)

HPB_INLINE size_t _hpb_Array_ElementSizeLg2(const hpb_Array* arr) {
  size_t ret = arr->data & 7;
  HPB_ASSERT(ret <= 4);
  return ret;
}

HPB_INLINE const void* _hpb_array_constptr(const hpb_Array* arr) {
  _hpb_Array_ElementSizeLg2(arr);  // Check assertion.
  return (void*)(arr->data & ~(uintptr_t)7);
}

HPB_INLINE uintptr_t _hpb_array_tagptr(void* ptr, int elem_size_lg2) {
  HPB_ASSERT(elem_size_lg2 <= 4);
  return (uintptr_t)ptr | elem_size_lg2;
}

HPB_INLINE void* _hpb_array_ptr(hpb_Array* arr) {
  return (void*)_hpb_array_constptr(arr);
}

HPB_INLINE uintptr_t _hpb_tag_arrptr(void* ptr, int elem_size_lg2) {
  HPB_ASSERT(elem_size_lg2 <= 4);
  HPB_ASSERT(((uintptr_t)ptr & 7) == 0);
  return (uintptr_t)ptr | (unsigned)elem_size_lg2;
}

extern const char _hpb_Array_CTypeSizeLg2Table[];

HPB_INLINE size_t _hpb_Array_CTypeSizeLg2(hpb_CType ctype) {
  return _hpb_Array_CTypeSizeLg2Table[ctype];
}

HPB_INLINE hpb_Array* _hpb_Array_New(hpb_Arena* a, size_t init_capacity,
                                     int elem_size_lg2) {
  HPB_ASSERT(elem_size_lg2 <= 4);
  const size_t arr_size = HPB_ALIGN_UP(sizeof(hpb_Array), HPB_MALLOC_ALIGN);
  const size_t bytes = arr_size + (init_capacity << elem_size_lg2);
  hpb_Array* arr = (hpb_Array*)hpb_Arena_Malloc(a, bytes);
  if (!arr) return NULL;
  arr->data = _hpb_tag_arrptr(HPB_PTR_AT(arr, arr_size, void), elem_size_lg2);
  arr->size = 0;
  arr->capacity = init_capacity;
  return arr;
}

// Resizes the capacity of the array to be at least min_size.
bool _hpb_array_realloc(hpb_Array* arr, size_t min_size, hpb_Arena* arena);

HPB_INLINE bool _hpb_array_reserve(hpb_Array* arr, size_t size,
                                   hpb_Arena* arena) {
  if (arr->capacity < size) return _hpb_array_realloc(arr, size, arena);
  return true;
}

// Resize without initializing new elements.
HPB_INLINE bool _hpb_Array_ResizeUninitialized(hpb_Array* arr, size_t size,
                                               hpb_Arena* arena) {
  HPB_ASSERT(size <= arr->size || arena);  // Allow NULL arena when shrinking.
  if (!_hpb_array_reserve(arr, size, arena)) return false;
  arr->size = size;
  return true;
}

// This function is intended for situations where elem_size is compile-time
// constant or a known expression of the form (1 << lg2), so that the expression
// i*elem_size does not result in an actual multiplication.
HPB_INLINE void _hpb_Array_Set(hpb_Array* arr, size_t i, const void* data,
                               size_t elem_size) {
  HPB_ASSERT(i < arr->size);
  HPB_ASSERT(elem_size == 1U << _hpb_Array_ElementSizeLg2(arr));
  char* arr_data = (char*)_hpb_array_ptr(arr);
  memcpy(arr_data + (i * elem_size), data, elem_size);
}

HPB_INLINE void _hpb_array_detach(const void* msg, size_t ofs) {
  *HPB_PTR_AT(msg, ofs, hpb_Array*) = NULL;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_COLLECTIONS_INTERNAL_ARRAY_H_ */
