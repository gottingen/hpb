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

#ifndef HPB_REFLECTION_FIELD_DEF_H_
#define HPB_REFLECTION_FIELD_DEF_H_

#include "hpb/base/string_view.h"
#include "hpb/reflection/common.h"

// Must be last.
#include "hpb/port/def.inc"

// Maximum field number allowed for FieldDefs.
// This is an inherent limit of the protobuf wire format.
#define kHpb_MaxFieldNumber ((1 << 29) - 1)

#ifdef __cplusplus
extern "C" {
#endif

const hpb_OneofDef* hpb_FieldDef_ContainingOneof(const hpb_FieldDef* f);
HPB_API const hpb_MessageDef* hpb_FieldDef_ContainingType(
    const hpb_FieldDef* f);
HPB_API hpb_CType hpb_FieldDef_CType(const hpb_FieldDef* f);
HPB_API hpb_MessageValue hpb_FieldDef_Default(const hpb_FieldDef* f);
HPB_API const hpb_EnumDef* hpb_FieldDef_EnumSubDef(const hpb_FieldDef* f);
const hpb_MessageDef* hpb_FieldDef_ExtensionScope(const hpb_FieldDef* f);
HPB_API const hpb_FileDef* hpb_FieldDef_File(const hpb_FieldDef* f);
const char* hpb_FieldDef_FullName(const hpb_FieldDef* f);
bool hpb_FieldDef_HasDefault(const hpb_FieldDef* f);
bool hpb_FieldDef_HasJsonName(const hpb_FieldDef* f);
bool hpb_FieldDef_HasOptions(const hpb_FieldDef* f);
HPB_API bool hpb_FieldDef_HasPresence(const hpb_FieldDef* f);
bool hpb_FieldDef_HasSubDef(const hpb_FieldDef* f);
uint32_t hpb_FieldDef_Index(const hpb_FieldDef* f);
bool hpb_FieldDef_IsExtension(const hpb_FieldDef* f);
HPB_API bool hpb_FieldDef_IsMap(const hpb_FieldDef* f);
bool hpb_FieldDef_IsOptional(const hpb_FieldDef* f);
bool hpb_FieldDef_IsPacked(const hpb_FieldDef* f);
bool hpb_FieldDef_IsPrimitive(const hpb_FieldDef* f);
HPB_API bool hpb_FieldDef_IsRepeated(const hpb_FieldDef* f);
bool hpb_FieldDef_IsRequired(const hpb_FieldDef* f);
bool hpb_FieldDef_IsString(const hpb_FieldDef* f);
HPB_API bool hpb_FieldDef_IsSubMessage(const hpb_FieldDef* f);
HPB_API const char* hpb_FieldDef_JsonName(const hpb_FieldDef* f);
HPB_API hpb_Label hpb_FieldDef_Label(const hpb_FieldDef* f);
HPB_API const hpb_MessageDef* hpb_FieldDef_MessageSubDef(const hpb_FieldDef* f);

// Creates a mini descriptor string for a field, returns true on success.
bool hpb_FieldDef_MiniDescriptorEncode(const hpb_FieldDef* f, hpb_Arena* a,
                                       hpb_StringView* out);

const hpb_MiniTableField* hpb_FieldDef_MiniTable(const hpb_FieldDef* f);
HPB_API const char* hpb_FieldDef_Name(const hpb_FieldDef* f);
HPB_API uint32_t hpb_FieldDef_Number(const hpb_FieldDef* f);
const HPB_DESC(FieldOptions) * hpb_FieldDef_Options(const hpb_FieldDef* f);
HPB_API const hpb_OneofDef* hpb_FieldDef_RealContainingOneof(
    const hpb_FieldDef* f);
HPB_API hpb_FieldType hpb_FieldDef_Type(const hpb_FieldDef* f);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_FIELD_DEF_H_ */
