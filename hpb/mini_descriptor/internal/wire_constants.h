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

#ifndef HPB_MINI_DESCRIPTOR_INTERNAL_WIRE_CONSTANTS_H_
#define HPB_MINI_DESCRIPTOR_INTERNAL_WIRE_CONSTANTS_H_

#include "hpb/base/descriptor_constants.h"

// Must be last.
#include "hpb/port/def.inc"

typedef enum {
  kHpb_EncodedType_Double = 0,
  kHpb_EncodedType_Float = 1,
  kHpb_EncodedType_Fixed32 = 2,
  kHpb_EncodedType_Fixed64 = 3,
  kHpb_EncodedType_SFixed32 = 4,
  kHpb_EncodedType_SFixed64 = 5,
  kHpb_EncodedType_Int32 = 6,
  kHpb_EncodedType_UInt32 = 7,
  kHpb_EncodedType_SInt32 = 8,
  kHpb_EncodedType_Int64 = 9,
  kHpb_EncodedType_UInt64 = 10,
  kHpb_EncodedType_SInt64 = 11,
  kHpb_EncodedType_OpenEnum = 12,
  kHpb_EncodedType_Bool = 13,
  kHpb_EncodedType_Bytes = 14,
  kHpb_EncodedType_String = 15,
  kHpb_EncodedType_Group = 16,
  kHpb_EncodedType_Message = 17,
  kHpb_EncodedType_ClosedEnum = 18,

  kHpb_EncodedType_RepeatedBase = 20,
} hpb_EncodedType;

typedef enum {
  kHpb_EncodedFieldModifier_FlipPacked = 1 << 0,
  kHpb_EncodedFieldModifier_IsRequired = 1 << 1,
  kHpb_EncodedFieldModifier_IsProto3Singular = 1 << 2,
} hpb_EncodedFieldModifier;

enum {
  kHpb_EncodedValue_MinField = ' ',
  kHpb_EncodedValue_MaxField = 'I',
  kHpb_EncodedValue_MinModifier = 'L',
  kHpb_EncodedValue_MaxModifier = '[',
  kHpb_EncodedValue_End = '^',
  kHpb_EncodedValue_MinSkip = '_',
  kHpb_EncodedValue_MaxSkip = '~',
  kHpb_EncodedValue_OneofSeparator = '~',
  kHpb_EncodedValue_FieldSeparator = '|',
  kHpb_EncodedValue_MinOneofField = ' ',
  kHpb_EncodedValue_MaxOneofField = 'b',
  kHpb_EncodedValue_MaxEnumMask = 'A',
};

enum {
  kHpb_EncodedVersion_EnumV1 = '!',
  kHpb_EncodedVersion_ExtensionV1 = '#',
  kHpb_EncodedVersion_MapV1 = '%',
  kHpb_EncodedVersion_MessageV1 = '$',
  kHpb_EncodedVersion_MessageSetV1 = '&',
};

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_DESCRIPTOR_INTERNAL_WIRE_CONSTANTS_H_
