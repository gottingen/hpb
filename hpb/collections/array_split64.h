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

#ifndef HPB_COLLECTIONS_ARRAY_SPLIT64_H_
#define HPB_COLLECTIONS_ARRAY_SPLIT64_H_

#include "hpb/collections/array.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// JavaScript doesn't directly support 64-bit ints so we must split them.

HPB_API_INLINE uint32_t hpb_Array_GetInt64Hi(const hpb_Array* array, size_t i) {
  return (uint32_t)(hpb_Array_Get(array, i).int64_val >> 32);
}

HPB_API_INLINE uint32_t hpb_Array_GetInt64Lo(const hpb_Array* array, size_t i) {
  return (uint32_t)hpb_Array_Get(array, i).int64_val;
}

HPB_API_INLINE void hpb_Array_SetInt64Split(hpb_Array* array, size_t i,
                                            uint32_t hi, uint32_t lo) {
  const hpb_MessageValue val = {.int64_val = ((int64_t)hi) << 32 | lo};
  hpb_Array_Set(array, i, val);
}

HPB_API_INLINE bool hpb_Array_AppendInt64Split(hpb_Array* array, uint32_t hi,
                                               uint32_t lo, hpb_Arena* arena) {
  const hpb_MessageValue val = {.int64_val = ((int64_t)hi) << 32 | lo};
  return hpb_Array_Append(array, val, arena);
}

HPB_API_INLINE uint32_t hpb_Array_GetUInt64Hi(const hpb_Array* array,
                                              size_t i) {
  return (uint32_t)(hpb_Array_Get(array, i).uint64_val >> 32);
}

HPB_API_INLINE uint32_t hpb_Array_GetUInt64Lo(const hpb_Array* array,
                                              size_t i) {
  return (uint32_t)hpb_Array_Get(array, i).uint64_val;
}

HPB_API_INLINE void hpb_Array_SetUInt64Split(hpb_Array* array, size_t i,
                                             uint32_t hi, uint32_t lo) {
  const hpb_MessageValue val = {.uint64_val = ((uint64_t)hi) << 32 | lo};
  hpb_Array_Set(array, i, val);
}

HPB_API_INLINE bool hpb_Array_AppendUInt64Split(hpb_Array* array, uint32_t hi,
                                                uint32_t lo, hpb_Arena* arena) {
  const hpb_MessageValue val = {.uint64_val = ((uint64_t)hi) << 32 | lo};
  return hpb_Array_Append(array, val, arena);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_COLLECTIONS_ARRAY_SPLIT64_H_
