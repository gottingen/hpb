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

#ifndef HPB_MINI_TABLE_FIELD_H_
#define HPB_MINI_TABLE_FIELD_H_

#include "hpb/mini_table/internal/field.h"
#include "hpb/mini_table/internal/message.h"
#include "hpb/mini_table/internal/sub.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hpb_MiniTableField hpb_MiniTableField;

HPB_API_INLINE hpb_FieldType
hpb_MiniTableField_Type(const hpb_MiniTableField* field) {
  if (field->mode & kHpb_LabelFlags_IsAlternate) {
    if (field->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Int32) {
      return kHpb_FieldType_Enum;
    } else if (field->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Bytes) {
      return kHpb_FieldType_String;
    } else {
      HPB_ASSERT(false);
    }
  }
  return (hpb_FieldType)field->HPB_PRIVATE(descriptortype);
}

HPB_API_INLINE hpb_CType hpb_MiniTableField_CType(const hpb_MiniTableField* f) {
  switch (hpb_MiniTableField_Type(f)) {
    case kHpb_FieldType_Double:
      return kHpb_CType_Double;
    case kHpb_FieldType_Float:
      return kHpb_CType_Float;
    case kHpb_FieldType_Int64:
    case kHpb_FieldType_SInt64:
    case kHpb_FieldType_SFixed64:
      return kHpb_CType_Int64;
    case kHpb_FieldType_Int32:
    case kHpb_FieldType_SFixed32:
    case kHpb_FieldType_SInt32:
      return kHpb_CType_Int32;
    case kHpb_FieldType_UInt64:
    case kHpb_FieldType_Fixed64:
      return kHpb_CType_UInt64;
    case kHpb_FieldType_UInt32:
    case kHpb_FieldType_Fixed32:
      return kHpb_CType_UInt32;
    case kHpb_FieldType_Enum:
      return kHpb_CType_Enum;
    case kHpb_FieldType_Bool:
      return kHpb_CType_Bool;
    case kHpb_FieldType_String:
      return kHpb_CType_String;
    case kHpb_FieldType_Bytes:
      return kHpb_CType_Bytes;
    case kHpb_FieldType_Group:
    case kHpb_FieldType_Message:
      return kHpb_CType_Message;
  }
  HPB_UNREACHABLE();
}

HPB_API_INLINE bool hpb_MiniTableField_IsExtension(
    const hpb_MiniTableField* field) {
  return field->mode & kHpb_LabelFlags_IsExtension;
}

HPB_API_INLINE bool hpb_MiniTableField_IsClosedEnum(
    const hpb_MiniTableField* field) {
  return field->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Enum;
}

HPB_API_INLINE bool hpb_MiniTableField_HasPresence(
    const hpb_MiniTableField* field) {
  if (hpb_MiniTableField_IsExtension(field)) {
    return !hpb_IsRepeatedOrMap(field);
  } else {
    return field->presence != 0;
  }
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_TABLE_FIELD_H_
