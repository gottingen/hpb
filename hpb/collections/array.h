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

#ifndef HPB_COLLECTIONS_ARRAY_H_
#define HPB_COLLECTIONS_ARRAY_H_

#include "hpb/base/descriptor_constants.h"
#include "hpb/collections/message_value.h"
#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Creates a new array on the given arena that holds elements of this type.
HPB_API hpb_Array* hpb_Array_New(hpb_Arena* a, hpb_CType type);

// Returns the number of elements in the array.
HPB_API size_t hpb_Array_Size(const hpb_Array* arr);

// Returns the given element, which must be within the array's current size.
HPB_API hpb_MessageValue hpb_Array_Get(const hpb_Array* arr, size_t i);

// Sets the given element, which must be within the array's current size.
HPB_API void hpb_Array_Set(hpb_Array* arr, size_t i, hpb_MessageValue val);

// Appends an element to the array. Returns false on allocation failure.
HPB_API bool hpb_Array_Append(hpb_Array* array, hpb_MessageValue val,
                              hpb_Arena* arena);

// Moves elements within the array using memmove().
// Like memmove(), the source and destination elements may be overlapping.
HPB_API void hpb_Array_Move(hpb_Array* array, size_t dst_idx, size_t src_idx,
                            size_t count);

// Inserts one or more empty elements into the array.
// Existing elements are shifted right.
// The new elements have undefined state and must be set with `hpb_Array_Set()`.
// REQUIRES: `i <= hpb_Array_Size(arr)`
HPB_API bool hpb_Array_Insert(hpb_Array* array, size_t i, size_t count,
                              hpb_Arena* arena);

// Deletes one or more elements from the array.
// Existing elements are shifted left.
// REQUIRES: `i + count <= hpb_Array_Size(arr)`
HPB_API void hpb_Array_Delete(hpb_Array* array, size_t i, size_t count);

// Changes the size of a vector. New elements are initialized to NULL/0.
// Returns false on allocation failure.
HPB_API bool hpb_Array_Resize(hpb_Array* array, size_t size, hpb_Arena* arena);

// Returns pointer to array data.
HPB_API const void* hpb_Array_DataPtr(const hpb_Array* arr);

// Returns mutable pointer to array data.
HPB_API void* hpb_Array_MutableDataPtr(hpb_Array* arr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_COLLECTIONS_ARRAY_H_ */
