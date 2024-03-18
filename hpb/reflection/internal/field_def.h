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

#ifndef HPB_REFLECTION_FIELD_DEF_INTERNAL_H_
#define HPB_REFLECTION_FIELD_DEF_INTERNAL_H_

#include "hpb/reflection/field_def.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

hpb_FieldDef* _hpb_FieldDef_At(const hpb_FieldDef* f, int i);

const hpb_MiniTableExtension* _hpb_FieldDef_ExtensionMiniTable(
    const hpb_FieldDef* f);
bool _hpb_FieldDef_IsClosedEnum(const hpb_FieldDef* f);
bool _hpb_FieldDef_IsProto3Optional(const hpb_FieldDef* f);
int _hpb_FieldDef_LayoutIndex(const hpb_FieldDef* f);
uint64_t _hpb_FieldDef_Modifiers(const hpb_FieldDef* f);
void _hpb_FieldDef_Resolve(hpb_DefBuilder* ctx, const char* prefix,
                           hpb_FieldDef* f);
void _hpb_FieldDef_BuildMiniTableExtension(hpb_DefBuilder* ctx,
                                           const hpb_FieldDef* f);

// Allocate and initialize an array of |n| extensions (field defs).
hpb_FieldDef* _hpb_Extensions_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(FieldDescriptorProto) * const* protos, const char* prefix,
    hpb_MessageDef* m);

// Allocate and initialize an array of |n| field defs.
hpb_FieldDef* _hpb_FieldDefs_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(FieldDescriptorProto) * const* protos, const char* prefix,
    hpb_MessageDef* m, bool* is_sorted);

// Allocate and return a list of pointers to the |n| field defs in |ff|,
// sorted by field number.
const hpb_FieldDef** _hpb_FieldDefs_Sorted(const hpb_FieldDef* f, int n,
                                           hpb_Arena* a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif // HPB_REFLECTION_FIELD_DEF_INTERNAL_H_
