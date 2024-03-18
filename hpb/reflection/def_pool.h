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

#ifndef HPB_REFLECTION_DEF_POOL_H_
#define HPB_REFLECTION_DEF_POOL_H_

#include "hpb/base/status.h"
#include "hpb/base/string_view.h"
#include "hpb/reflection/common.h"
#include "hpb/reflection/def_type.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

HPB_API void hpb_DefPool_Free(hpb_DefPool* s);

HPB_API hpb_DefPool* hpb_DefPool_New(void);

HPB_API const hpb_MessageDef* hpb_DefPool_FindMessageByName(
    const hpb_DefPool* s, const char* sym);

const hpb_MessageDef* hpb_DefPool_FindMessageByNameWithSize(
    const hpb_DefPool* s, const char* sym, size_t len);

HPB_API const hpb_EnumDef* hpb_DefPool_FindEnumByName(const hpb_DefPool* s,
                                                      const char* sym);

const hpb_EnumValueDef* hpb_DefPool_FindEnumByNameval(const hpb_DefPool* s,
                                                      const char* sym);

const hpb_FileDef* hpb_DefPool_FindFileByName(const hpb_DefPool* s,
                                              const char* name);

const hpb_FileDef* hpb_DefPool_FindFileByNameWithSize(const hpb_DefPool* s,
                                                      const char* name,
                                                      size_t len);

const hpb_FieldDef* hpb_DefPool_FindExtensionByMiniTable(
    const hpb_DefPool* s, const hpb_MiniTableExtension* ext);

const hpb_FieldDef* hpb_DefPool_FindExtensionByName(const hpb_DefPool* s,
                                                    const char* sym);

const hpb_FieldDef* hpb_DefPool_FindExtensionByNameWithSize(
    const hpb_DefPool* s, const char* name, size_t size);

const hpb_FieldDef* hpb_DefPool_FindExtensionByNumber(const hpb_DefPool* s,
                                                      const hpb_MessageDef* m,
                                                      int32_t fieldnum);

const hpb_ServiceDef* hpb_DefPool_FindServiceByName(const hpb_DefPool* s,
                                                    const char* name);

const hpb_ServiceDef* hpb_DefPool_FindServiceByNameWithSize(
    const hpb_DefPool* s, const char* name, size_t size);

const hpb_FileDef* hpb_DefPool_FindFileContainingSymbol(const hpb_DefPool* s,
                                                        const char* name);

HPB_API const hpb_FileDef* hpb_DefPool_AddFile(
    hpb_DefPool* s, const HPB_DESC(FileDescriptorProto) * file_proto,
    hpb_Status* status);

const hpb_ExtensionRegistry* hpb_DefPool_ExtensionRegistry(
    const hpb_DefPool* s);

const hpb_FieldDef** hpb_DefPool_GetAllExtensions(const hpb_DefPool* s,
                                                  const hpb_MessageDef* m,
                                                  size_t* count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_DEF_POOL_H_ */
