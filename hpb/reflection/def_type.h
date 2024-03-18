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

#ifndef HPB_REFLECTION_DEF_TYPE_H_
#define HPB_REFLECTION_DEF_TYPE_H_

#include "hpb/hash/common.h"

// Must be last.
#include "hpb/port/def.inc"

// Inside a symtab we store tagged pointers to specific def types.
typedef enum {
  HPB_DEFTYPE_MASK = 7,

  // Only inside symtab table.
  HPB_DEFTYPE_EXT = 0,
  HPB_DEFTYPE_MSG = 1,
  HPB_DEFTYPE_ENUM = 2,
  HPB_DEFTYPE_ENUMVAL = 3,
  HPB_DEFTYPE_SERVICE = 4,

  // Only inside message table.
  HPB_DEFTYPE_FIELD = 0,
  HPB_DEFTYPE_ONEOF = 1,
  HPB_DEFTYPE_FIELD_JSONNAME = 2,
} hpb_deftype_t;

#ifdef __cplusplus
extern "C" {
#endif

// Our 3-bit pointer tagging requires all pointers to be multiples of 8.
// The arena will always yield 8-byte-aligned addresses, however we put
// the defs into arrays. For each element in the array to be 8-byte-aligned,
// the sizes of each def type must also be a multiple of 8.
//
// If any of these asserts fail, we need to add or remove padding on 32-bit
// machines (64-bit machines will have 8-byte alignment already due to
// pointers, which all of these structs have).
HPB_INLINE void _hpb_DefType_CheckPadding(size_t size) {
  HPB_ASSERT((size & HPB_DEFTYPE_MASK) == 0);
}

hpb_deftype_t _hpb_DefType_Type(hpb_value v);

hpb_value _hpb_DefType_Pack(const void* ptr, hpb_deftype_t type);

const void* _hpb_DefType_Unpack(hpb_value v, hpb_deftype_t type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_DEF_TYPE_H_ */
