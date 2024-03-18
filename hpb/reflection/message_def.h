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

// IWYU pragma: private, include "hpb/reflection/def.h"

#ifndef HPB_REFLECTION_MESSAGE_DEF_H_
#define HPB_REFLECTION_MESSAGE_DEF_H_

#include "hpb/base/string_view.h"
#include "hpb/reflection/common.h"

// Must be last.
#include "hpb/port/def.inc"

// Well-known field tag numbers for map-entry messages.
#define kHpb_MapEntry_KeyFieldNumber 1
#define kHpb_MapEntry_ValueFieldNumber 2

// Well-known field tag numbers for Any messages.
#define kHpb_Any_TypeFieldNumber 1
#define kHpb_Any_ValueFieldNumber 2

// Well-known field tag numbers for duration messages.
#define kHpb_Duration_SecondsFieldNumber 1
#define kHpb_Duration_NanosFieldNumber 2

// Well-known field tag numbers for timestamp messages.
#define kHpb_Timestamp_SecondsFieldNumber 1
#define kHpb_Timestamp_NanosFieldNumber 2

// All the different kind of well known type messages. For simplicity of check,
// number wrappers and string wrappers are grouped together. Make sure the
// order and number of these groups are not changed.
typedef enum {
  kHpb_WellKnown_Unspecified,
  kHpb_WellKnown_Any,
  kHpb_WellKnown_FieldMask,
  kHpb_WellKnown_Duration,
  kHpb_WellKnown_Timestamp,

  // number wrappers
  kHpb_WellKnown_DoubleValue,
  kHpb_WellKnown_FloatValue,
  kHpb_WellKnown_Int64Value,
  kHpb_WellKnown_UInt64Value,
  kHpb_WellKnown_Int32Value,
  kHpb_WellKnown_UInt32Value,

  // string wrappers
  kHpb_WellKnown_StringValue,
  kHpb_WellKnown_BytesValue,
  kHpb_WellKnown_BoolValue,
  kHpb_WellKnown_Value,
  kHpb_WellKnown_ListValue,
  kHpb_WellKnown_Struct,
} hpb_WellKnown;

#ifdef __cplusplus
extern "C" {
#endif

const hpb_MessageDef* hpb_MessageDef_ContainingType(const hpb_MessageDef* m);

const hpb_ExtensionRange* hpb_MessageDef_ExtensionRange(const hpb_MessageDef* m,
                                                        int i);
int hpb_MessageDef_ExtensionRangeCount(const hpb_MessageDef* m);

HPB_API const hpb_FieldDef* hpb_MessageDef_Field(const hpb_MessageDef* m,
                                                 int i);
HPB_API int hpb_MessageDef_FieldCount(const hpb_MessageDef* m);

HPB_API const hpb_FileDef* hpb_MessageDef_File(const hpb_MessageDef* m);

// Returns a field by either JSON name or regular proto name.
const hpb_FieldDef* hpb_MessageDef_FindByJsonNameWithSize(
    const hpb_MessageDef* m, const char* name, size_t size);
HPB_INLINE const hpb_FieldDef* hpb_MessageDef_FindByJsonName(
    const hpb_MessageDef* m, const char* name) {
  return hpb_MessageDef_FindByJsonNameWithSize(m, name, strlen(name));
}

// Lookup of either field or oneof by name. Returns whether either was found.
// If the return is true, then the found def will be set, and the non-found
// one set to NULL.
HPB_API bool hpb_MessageDef_FindByNameWithSize(const hpb_MessageDef* m,
                                               const char* name, size_t size,
                                               const hpb_FieldDef** f,
                                               const hpb_OneofDef** o);
HPB_INLINE bool hpb_MessageDef_FindByName(const hpb_MessageDef* m,
                                          const char* name,
                                          const hpb_FieldDef** f,
                                          const hpb_OneofDef** o) {
  return hpb_MessageDef_FindByNameWithSize(m, name, strlen(name), f, o);
}

const hpb_FieldDef* hpb_MessageDef_FindFieldByName(const hpb_MessageDef* m,
                                                   const char* name);
HPB_API const hpb_FieldDef* hpb_MessageDef_FindFieldByNameWithSize(
    const hpb_MessageDef* m, const char* name, size_t size);
HPB_API const hpb_FieldDef* hpb_MessageDef_FindFieldByNumber(
    const hpb_MessageDef* m, uint32_t i);
const hpb_OneofDef* hpb_MessageDef_FindOneofByName(const hpb_MessageDef* m,
                                                   const char* name);
HPB_API const hpb_OneofDef* hpb_MessageDef_FindOneofByNameWithSize(
    const hpb_MessageDef* m, const char* name, size_t size);
HPB_API const char* hpb_MessageDef_FullName(const hpb_MessageDef* m);
bool hpb_MessageDef_HasOptions(const hpb_MessageDef* m);
bool hpb_MessageDef_IsMapEntry(const hpb_MessageDef* m);
bool hpb_MessageDef_IsMessageSet(const hpb_MessageDef* m);

// Creates a mini descriptor string for a message, returns true on success.
bool hpb_MessageDef_MiniDescriptorEncode(const hpb_MessageDef* m, hpb_Arena* a,
                                         hpb_StringView* out);

HPB_API const hpb_MiniTable* hpb_MessageDef_MiniTable(const hpb_MessageDef* m);
const char* hpb_MessageDef_Name(const hpb_MessageDef* m);

const hpb_EnumDef* hpb_MessageDef_NestedEnum(const hpb_MessageDef* m, int i);
const hpb_FieldDef* hpb_MessageDef_NestedExtension(const hpb_MessageDef* m,
                                                   int i);
const hpb_MessageDef* hpb_MessageDef_NestedMessage(const hpb_MessageDef* m,
                                                   int i);

int hpb_MessageDef_NestedEnumCount(const hpb_MessageDef* m);
int hpb_MessageDef_NestedExtensionCount(const hpb_MessageDef* m);
int hpb_MessageDef_NestedMessageCount(const hpb_MessageDef* m);

HPB_API const hpb_OneofDef* hpb_MessageDef_Oneof(const hpb_MessageDef* m,
                                                 int i);
HPB_API int hpb_MessageDef_OneofCount(const hpb_MessageDef* m);
int hpb_MessageDef_RealOneofCount(const hpb_MessageDef* m);

const HPB_DESC(MessageOptions) *
    hpb_MessageDef_Options(const hpb_MessageDef* m);

hpb_StringView hpb_MessageDef_ReservedName(const hpb_MessageDef* m, int i);
int hpb_MessageDef_ReservedNameCount(const hpb_MessageDef* m);

const hpb_MessageReservedRange* hpb_MessageDef_ReservedRange(
    const hpb_MessageDef* m, int i);
int hpb_MessageDef_ReservedRangeCount(const hpb_MessageDef* m);

HPB_API hpb_Syntax hpb_MessageDef_Syntax(const hpb_MessageDef* m);
HPB_API hpb_WellKnown hpb_MessageDef_WellKnownType(const hpb_MessageDef* m);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_REFLECTION_MESSAGE_DEF_H_
