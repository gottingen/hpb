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

#ifndef HPB_MINI_TABLE_INTERNAL_FIELD_H_
#define HPB_MINI_TABLE_INTERNAL_FIELD_H_

#include <stdint.h>

#include "hpb/base/descriptor_constants.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_MiniTableField {
  uint32_t number;
  uint16_t offset;
  int16_t presence;       // If >0, hasbit_index.  If <0, ~oneof_index

  // Indexes into `hpb_MiniTable.subs`
  // Will be set to `kHpb_NoSub` if `descriptortype` != MESSAGE/GROUP/ENUM
  uint16_t HPB_PRIVATE(submsg_index);

  uint8_t HPB_PRIVATE(descriptortype);

  // hpb_FieldMode | hpb_LabelFlags | (hpb_FieldRep << kHpb_FieldRep_Shift)
  uint8_t mode;
};

#define kHpb_NoSub ((uint16_t)-1)

typedef enum {
  kHpb_FieldMode_Map = 0,
  kHpb_FieldMode_Array = 1,
  kHpb_FieldMode_Scalar = 2,
} hpb_FieldMode;

// Mask to isolate the hpb_FieldMode from field.mode.
#define kHpb_FieldMode_Mask 3

// Extra flags on the mode field.
typedef enum {
  kHpb_LabelFlags_IsPacked = 4,
  kHpb_LabelFlags_IsExtension = 8,
  // Indicates that this descriptor type is an "alternate type":
  //   - for Int32, this indicates that the actual type is Enum (but was
  //     rewritten to Int32 because it is an open enum that requires no check).
  //   - for Bytes, this indicates that the actual type is String (but does
  //     not require any UTF-8 check).
  kHpb_LabelFlags_IsAlternate = 16,
} hpb_LabelFlags;

// Note: we sort by this number when calculating layout order.
typedef enum {
  kHpb_FieldRep_1Byte = 0,
  kHpb_FieldRep_4Byte = 1,
  kHpb_FieldRep_StringView = 2,
  kHpb_FieldRep_8Byte = 3,

  kHpb_FieldRep_NativePointer =
      HPB_SIZE(kHpb_FieldRep_4Byte, kHpb_FieldRep_8Byte),
  kHpb_FieldRep_Max = kHpb_FieldRep_8Byte,
} hpb_FieldRep;

#define kHpb_FieldRep_Shift 6

HPB_INLINE hpb_FieldRep
_hpb_MiniTableField_GetRep(const struct hpb_MiniTableField* field) {
  return (hpb_FieldRep)(field->mode >> kHpb_FieldRep_Shift);
}

#ifdef __cplusplus
extern "C" {
#endif

HPB_INLINE hpb_FieldMode
hpb_FieldMode_Get(const struct hpb_MiniTableField* field) {
  return (hpb_FieldMode)(field->mode & 3);
}

HPB_INLINE void _hpb_MiniTableField_CheckIsArray(
    const struct hpb_MiniTableField* field) {
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_NativePointer);
  HPB_ASSUME(hpb_FieldMode_Get(field) == kHpb_FieldMode_Array);
  HPB_ASSUME(field->presence == 0);
}

HPB_INLINE void _hpb_MiniTableField_CheckIsMap(
    const struct hpb_MiniTableField* field) {
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_NativePointer);
  HPB_ASSUME(hpb_FieldMode_Get(field) == kHpb_FieldMode_Map);
  HPB_ASSUME(field->presence == 0);
}

HPB_INLINE bool hpb_IsRepeatedOrMap(const struct hpb_MiniTableField* field) {
  // This works because hpb_FieldMode has no value 3.
  return !(field->mode & kHpb_FieldMode_Scalar);
}

HPB_INLINE bool hpb_IsSubMessage(const struct hpb_MiniTableField* field) {
  return field->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Message ||
         field->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Group;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_TABLE_INTERNAL_FIELD_H_
