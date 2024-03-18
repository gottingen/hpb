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

#ifndef HPB_BASE_DESCRIPTOR_CONSTANTS_H_
#define HPB_BASE_DESCRIPTOR_CONSTANTS_H_

// Must be last.
#include "hpb/port/def.inc"

// The types a field can have. Note that this list is not identical to the
// types defined in descriptor.proto, which gives INT32 and SINT32 separate
// types (we distinguish the two with the "integer encoding" enum below).
// This enum is an internal convenience only and has no meaning outside of hpb.
typedef enum {
  kHpb_CType_Bool = 1,
  kHpb_CType_Float = 2,
  kHpb_CType_Int32 = 3,
  kHpb_CType_UInt32 = 4,
  kHpb_CType_Enum = 5,  // Enum values are int32. TODO(b/279178239): rename
  kHpb_CType_Message = 6,
  kHpb_CType_Double = 7,
  kHpb_CType_Int64 = 8,
  kHpb_CType_UInt64 = 9,
  kHpb_CType_String = 10,
  kHpb_CType_Bytes = 11
} hpb_CType;

// The repeated-ness of each field; this matches descriptor.proto.
typedef enum {
  kHpb_Label_Optional = 1,
  kHpb_Label_Required = 2,
  kHpb_Label_Repeated = 3
} hpb_Label;

// Descriptor types, as defined in descriptor.proto.
typedef enum {
  kHpb_FieldType_Double = 1,
  kHpb_FieldType_Float = 2,
  kHpb_FieldType_Int64 = 3,
  kHpb_FieldType_UInt64 = 4,
  kHpb_FieldType_Int32 = 5,
  kHpb_FieldType_Fixed64 = 6,
  kHpb_FieldType_Fixed32 = 7,
  kHpb_FieldType_Bool = 8,
  kHpb_FieldType_String = 9,
  kHpb_FieldType_Group = 10,
  kHpb_FieldType_Message = 11,
  kHpb_FieldType_Bytes = 12,
  kHpb_FieldType_UInt32 = 13,
  kHpb_FieldType_Enum = 14,
  kHpb_FieldType_SFixed32 = 15,
  kHpb_FieldType_SFixed64 = 16,
  kHpb_FieldType_SInt32 = 17,
  kHpb_FieldType_SInt64 = 18,
} hpb_FieldType;

#define kHpb_FieldType_SizeOf 19

#ifdef __cplusplus
extern "C" {
#endif

HPB_INLINE bool hpb_FieldType_IsPackable(hpb_FieldType type) {
  // clang-format off
  const unsigned kUnpackableTypes =
      (1 << kHpb_FieldType_String) |
      (1 << kHpb_FieldType_Bytes) |
      (1 << kHpb_FieldType_Message) |
      (1 << kHpb_FieldType_Group);
  // clang-format on
  return (1 << type) & ~kUnpackableTypes;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_BASE_DESCRIPTOR_CONSTANTS_H_ */
