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

#ifndef HPB_REFLECTION_DEF_POOL_INTERNAL_H_
#define HPB_REFLECTION_DEF_POOL_INTERNAL_H_

#include "hpb/mini_descriptor/decode.h"
#include "hpb/reflection/def_pool.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

hpb_Arena* _hpb_DefPool_Arena(const hpb_DefPool* s);
size_t _hpb_DefPool_BytesLoaded(const hpb_DefPool* s);
hpb_ExtensionRegistry* _hpb_DefPool_ExtReg(const hpb_DefPool* s);

bool _hpb_DefPool_InsertExt(hpb_DefPool* s, const hpb_MiniTableExtension* ext,
                            const hpb_FieldDef* f);
bool _hpb_DefPool_InsertSym(hpb_DefPool* s, hpb_StringView sym, hpb_value v,
                            hpb_Status* status);
bool _hpb_DefPool_LookupSym(const hpb_DefPool* s, const char* sym, size_t size,
                            hpb_value* v);

void** _hpb_DefPool_ScratchData(const hpb_DefPool* s);
size_t* _hpb_DefPool_ScratchSize(const hpb_DefPool* s);
void _hpb_DefPool_SetPlatform(hpb_DefPool* s, hpb_MiniTablePlatform platform);

// For generated code only: loads a generated descriptor.
typedef struct _hpb_DefPool_Init {
  struct _hpb_DefPool_Init** deps;  // Dependencies of this file.
  const hpb_MiniTableFile* layout;
  const char* filename;
  hpb_StringView descriptor;  // Serialized descriptor.
} _hpb_DefPool_Init;

bool _hpb_DefPool_LoadDefInit(hpb_DefPool* s, const _hpb_DefPool_Init* init);

// Should only be directly called by tests. This variant lets us suppress
// the use of compiled-in tables, forcing a rebuild of the tables at runtime.
bool _hpb_DefPool_LoadDefInitEx(hpb_DefPool* s, const _hpb_DefPool_Init* init,
                                bool rebuild_minitable);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_REFLECTION_DEF_POOL_INTERNAL_H_
