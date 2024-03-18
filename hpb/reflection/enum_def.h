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

#ifndef HPB_REFLECTION_ENUM_DEF_H_
#define HPB_REFLECTION_ENUM_DEF_H_

#include "hpb/base/string_view.h"
#include "hpb/reflection/common.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

bool hpb_EnumDef_CheckNumber(const hpb_EnumDef* e, int32_t num);
const hpb_MessageDef* hpb_EnumDef_ContainingType(const hpb_EnumDef* e);
int32_t hpb_EnumDef_Default(const hpb_EnumDef* e);
HPB_API const hpb_FileDef* hpb_EnumDef_File(const hpb_EnumDef* e);
const hpb_EnumValueDef* hpb_EnumDef_FindValueByName(const hpb_EnumDef* e,
                                                    const char* name);
HPB_API const hpb_EnumValueDef* hpb_EnumDef_FindValueByNameWithSize(
    const hpb_EnumDef* e, const char* name, size_t size);
HPB_API const hpb_EnumValueDef* hpb_EnumDef_FindValueByNumber(
    const hpb_EnumDef* e, int32_t num);
HPB_API const char* hpb_EnumDef_FullName(const hpb_EnumDef* e);
bool hpb_EnumDef_HasOptions(const hpb_EnumDef* e);
bool hpb_EnumDef_IsClosed(const hpb_EnumDef* e);

// Creates a mini descriptor string for an enum, returns true on success.
bool hpb_EnumDef_MiniDescriptorEncode(const hpb_EnumDef* e, hpb_Arena* a,
                                      hpb_StringView* out);

const char* hpb_EnumDef_Name(const hpb_EnumDef* e);
const HPB_DESC(EnumOptions) * hpb_EnumDef_Options(const hpb_EnumDef* e);

hpb_StringView hpb_EnumDef_ReservedName(const hpb_EnumDef* e, int i);
int hpb_EnumDef_ReservedNameCount(const hpb_EnumDef* e);

const hpb_EnumReservedRange* hpb_EnumDef_ReservedRange(const hpb_EnumDef* e,
                                                       int i);
int hpb_EnumDef_ReservedRangeCount(const hpb_EnumDef* e);

HPB_API const hpb_EnumValueDef* hpb_EnumDef_Value(const hpb_EnumDef* e, int i);
HPB_API int hpb_EnumDef_ValueCount(const hpb_EnumDef* e);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_ENUM_DEF_H_ */
