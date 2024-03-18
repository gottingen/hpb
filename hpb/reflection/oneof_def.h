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

#ifndef HPB_REFLECTION_ONEOF_DEF_H_
#define HPB_REFLECTION_ONEOF_DEF_H_

#include "hpb/reflection/common.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

HPB_API const hpb_MessageDef* hpb_OneofDef_ContainingType(
    const hpb_OneofDef* o);
HPB_API const hpb_FieldDef* hpb_OneofDef_Field(const hpb_OneofDef* o, int i);
HPB_API int hpb_OneofDef_FieldCount(const hpb_OneofDef* o);
const char* hpb_OneofDef_FullName(const hpb_OneofDef* o);
bool hpb_OneofDef_HasOptions(const hpb_OneofDef* o);
uint32_t hpb_OneofDef_Index(const hpb_OneofDef* o);
bool hpb_OneofDef_IsSynthetic(const hpb_OneofDef* o);
const hpb_FieldDef* hpb_OneofDef_LookupName(const hpb_OneofDef* o,
                                            const char* name);
const hpb_FieldDef* hpb_OneofDef_LookupNameWithSize(const hpb_OneofDef* o,
                                                    const char* name,
                                                    size_t size);
const hpb_FieldDef* hpb_OneofDef_LookupNumber(const hpb_OneofDef* o,
                                              uint32_t num);
HPB_API const char* hpb_OneofDef_Name(const hpb_OneofDef* o);
int hpb_OneofDef_numfields(const hpb_OneofDef* o);
const HPB_DESC(OneofOptions) * hpb_OneofDef_Options(const hpb_OneofDef* o);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_ONEOF_DEF_H_ */
