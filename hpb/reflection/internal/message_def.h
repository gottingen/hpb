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

#ifndef HPB_REFLECTION_MESSAGE_DEF_INTERNAL_H_
#define HPB_REFLECTION_MESSAGE_DEF_INTERNAL_H_

#include "hpb/reflection/message_def.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

hpb_MessageDef* _hpb_MessageDef_At(const hpb_MessageDef* m, int i);
bool _hpb_MessageDef_InMessageSet(const hpb_MessageDef* m);
bool _hpb_MessageDef_Insert(hpb_MessageDef* m, const char* name, size_t size,
                            hpb_value v, hpb_Arena* a);
void _hpb_MessageDef_InsertField(hpb_DefBuilder* ctx, hpb_MessageDef* m,
                                 const hpb_FieldDef* f);
bool _hpb_MessageDef_IsValidExtensionNumber(const hpb_MessageDef* m, int n);
void _hpb_MessageDef_CreateMiniTable(hpb_DefBuilder* ctx, hpb_MessageDef* m);
void _hpb_MessageDef_LinkMiniTable(hpb_DefBuilder* ctx,
                                   const hpb_MessageDef* m);
void _hpb_MessageDef_Resolve(hpb_DefBuilder* ctx, hpb_MessageDef* m);

// Allocate and initialize an array of |n| message defs.
hpb_MessageDef* _hpb_MessageDefs_New(
    hpb_DefBuilder* ctx, int n, const HPB_DESC(DescriptorProto) * const* protos,
    const hpb_MessageDef* containing_type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_REFLECTION_MESSAGE_DEF_INTERNAL_H_
