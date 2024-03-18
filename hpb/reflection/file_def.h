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

#ifndef HPB_REFLECTION_FILE_DEF_H_
#define HPB_REFLECTION_FILE_DEF_H_

#include "hpb/reflection/common.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

const hpb_FileDef* hpb_FileDef_Dependency(const hpb_FileDef* f, int i);
int hpb_FileDef_DependencyCount(const hpb_FileDef* f);
bool hpb_FileDef_HasOptions(const hpb_FileDef* f);
HPB_API const char* hpb_FileDef_Name(const hpb_FileDef* f);
const HPB_DESC(FileOptions) * hpb_FileDef_Options(const hpb_FileDef* f);
const char* hpb_FileDef_Package(const hpb_FileDef* f);
const char* hpb_FileDef_Edition(const hpb_FileDef* f);
HPB_API const hpb_DefPool* hpb_FileDef_Pool(const hpb_FileDef* f);

const hpb_FileDef* hpb_FileDef_PublicDependency(const hpb_FileDef* f, int i);
int hpb_FileDef_PublicDependencyCount(const hpb_FileDef* f);

const hpb_ServiceDef* hpb_FileDef_Service(const hpb_FileDef* f, int i);
int hpb_FileDef_ServiceCount(const hpb_FileDef* f);

HPB_API hpb_Syntax hpb_FileDef_Syntax(const hpb_FileDef* f);

const hpb_EnumDef* hpb_FileDef_TopLevelEnum(const hpb_FileDef* f, int i);
int hpb_FileDef_TopLevelEnumCount(const hpb_FileDef* f);

const hpb_FieldDef* hpb_FileDef_TopLevelExtension(const hpb_FileDef* f, int i);
int hpb_FileDef_TopLevelExtensionCount(const hpb_FileDef* f);

const hpb_MessageDef* hpb_FileDef_TopLevelMessage(const hpb_FileDef* f, int i);
int hpb_FileDef_TopLevelMessageCount(const hpb_FileDef* f);

const hpb_FileDef* hpb_FileDef_WeakDependency(const hpb_FileDef* f, int i);
int hpb_FileDef_WeakDependencyCount(const hpb_FileDef* f);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_FILE_DEF_H_ */
