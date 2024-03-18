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

#include "hpb/collections/internal/array.h"

#include <string.h>

// Must be last.
#include "hpb/port/def.inc"

const char _hpb_Array_CTypeSizeLg2Table[] = {
    [kHpb_CType_Bool] = 0,
    [kHpb_CType_Float] = 2,
    [kHpb_CType_Int32] = 2,
    [kHpb_CType_UInt32] = 2,
    [kHpb_CType_Enum] = 2,
    [kHpb_CType_Message] = HPB_SIZE(2, 3),
    [kHpb_CType_Double] = 3,
    [kHpb_CType_Int64] = 3,
    [kHpb_CType_UInt64] = 3,
    [kHpb_CType_String] = HPB_SIZE(3, 4),
    [kHpb_CType_Bytes] = HPB_SIZE(3, 4),
};

hpb_Array* hpb_Array_New(hpb_Arena* a, hpb_CType type) {
  return _hpb_Array_New(a, 4, _hpb_Array_CTypeSizeLg2(type));
}

const void* hpb_Array_DataPtr(const hpb_Array* arr) {
  return _hpb_array_ptr((hpb_Array*)arr);
}

void* hpb_Array_MutableDataPtr(hpb_Array* arr) { return _hpb_array_ptr(arr); }

size_t hpb_Array_Size(const hpb_Array* arr) { return arr->size; }

hpb_MessageValue hpb_Array_Get(const hpb_Array* arr, size_t i) {
  hpb_MessageValue ret;
  const char* data = _hpb_array_constptr(arr);
  int lg2 = arr->data & 7;
  HPB_ASSERT(i < arr->size);
  memcpy(&ret, data + (i << lg2), 1 << lg2);
  return ret;
}

void hpb_Array_Set(hpb_Array* arr, size_t i, hpb_MessageValue val) {
  char* data = _hpb_array_ptr(arr);
  int lg2 = arr->data & 7;
  HPB_ASSERT(i < arr->size);
  memcpy(data + (i << lg2), &val, 1 << lg2);
}

bool hpb_Array_Append(hpb_Array* arr, hpb_MessageValue val, hpb_Arena* arena) {
  HPB_ASSERT(arena);
  if (!hpb_Array_Resize(arr, arr->size + 1, arena)) {
    return false;
  }
  hpb_Array_Set(arr, arr->size - 1, val);
  return true;
}

void hpb_Array_Move(hpb_Array* arr, size_t dst_idx, size_t src_idx,
                    size_t count) {
  const int lg2 = arr->data & 7;
  char* data = _hpb_array_ptr(arr);
  memmove(&data[dst_idx << lg2], &data[src_idx << lg2], count << lg2);
}

bool hpb_Array_Insert(hpb_Array* arr, size_t i, size_t count,
                      hpb_Arena* arena) {
  HPB_ASSERT(arena);
  HPB_ASSERT(i <= arr->size);
  HPB_ASSERT(count + arr->size >= count);
  const size_t oldsize = arr->size;
  if (!hpb_Array_Resize(arr, arr->size + count, arena)) {
    return false;
  }
  hpb_Array_Move(arr, i + count, i, oldsize - i);
  return true;
}

/*
 *              i        end      arr->size
 * |------------|XXXXXXXX|--------|
 */
void hpb_Array_Delete(hpb_Array* arr, size_t i, size_t count) {
  const size_t end = i + count;
  HPB_ASSERT(i <= end);
  HPB_ASSERT(end <= arr->size);
  hpb_Array_Move(arr, i, end, arr->size - end);
  arr->size -= count;
}

bool hpb_Array_Resize(hpb_Array* arr, size_t size, hpb_Arena* arena) {
  const size_t oldsize = arr->size;
  if (HPB_UNLIKELY(!_hpb_Array_ResizeUninitialized(arr, size, arena))) {
    return false;
  }
  const size_t newsize = arr->size;
  if (newsize > oldsize) {
    const int lg2 = arr->data & 7;
    char* data = _hpb_array_ptr(arr);
    memset(data + (oldsize << lg2), 0, (newsize - oldsize) << lg2);
  }
  return true;
}

// EVERYTHING BELOW THIS LINE IS INTERNAL - DO NOT USE /////////////////////////

bool _hpb_array_realloc(hpb_Array* arr, size_t min_capacity, hpb_Arena* arena) {
  size_t new_capacity = HPB_MAX(arr->capacity, 4);
  int elem_size_lg2 = arr->data & 7;
  size_t old_bytes = arr->capacity << elem_size_lg2;
  size_t new_bytes;
  void* ptr = _hpb_array_ptr(arr);

  // Log2 ceiling of size.
  while (new_capacity < min_capacity) new_capacity *= 2;

  new_bytes = new_capacity << elem_size_lg2;
  ptr = hpb_Arena_Realloc(arena, ptr, old_bytes, new_bytes);
  if (!ptr) return false;

  arr->data = _hpb_tag_arrptr(ptr, elem_size_lg2);
  arr->capacity = new_capacity;
  return true;
}
