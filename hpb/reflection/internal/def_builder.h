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

#ifndef HPB_REFLECTION_DEF_BUILDER_INTERNAL_H_
#define HPB_REFLECTION_DEF_BUILDER_INTERNAL_H_

#include "hpb/reflection/common.h"
#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_pool.h"

// Must be last.
#include "hpb/port/def.inc"

// We want to copy the options verbatim into the destination options proto.
// We use serialize+parse as our deep copy.
#define HPB_DEF_SET_OPTIONS(target, desc_type, options_type, proto)           \
  if (HPB_DESC(desc_type##_has_options)(proto)) {                             \
    size_t size;                                                              \
    char* pb = HPB_DESC(options_type##_serialize)(                            \
        HPB_DESC(desc_type##_options)(proto), ctx->tmp_arena, &size);         \
    if (!pb) _hpb_DefBuilder_OomErr(ctx);                                     \
    target =                                                                  \
        HPB_DESC(options_type##_parse)(pb, size, _hpb_DefBuilder_Arena(ctx)); \
    if (!target) _hpb_DefBuilder_OomErr(ctx);                                 \
  } else {                                                                    \
    target = (const HPB_DESC(options_type)*)kHpbDefOptDefault;                \
  }

#ifdef __cplusplus
extern "C" {
#endif

struct hpb_DefBuilder {
  hpb_DefPool* symtab;
  hpb_FileDef* file;                 // File we are building.
  hpb_Arena* arena;                  // Allocate defs here.
  hpb_Arena* tmp_arena;              // For temporary allocations.
  hpb_Status* status;                // Record errors here.
  const hpb_MiniTableFile* layout;   // NULL if we should build layouts.
  hpb_MiniTablePlatform platform;    // Platform we are targeting.
  int enum_count;                    // Count of enums built so far.
  int msg_count;                     // Count of messages built so far.
  int ext_count;                     // Count of extensions built so far.
  jmp_buf err;                       // longjmp() on error.
};

extern const char* kHpbDefOptDefault;

// ctx->status has already been set elsewhere so just fail/longjmp()
HPB_NORETURN void _hpb_DefBuilder_FailJmp(hpb_DefBuilder* ctx);

HPB_NORETURN void _hpb_DefBuilder_Errf(hpb_DefBuilder* ctx, const char* fmt,
                                       ...) HPB_PRINTF(2, 3);
HPB_NORETURN void _hpb_DefBuilder_OomErr(hpb_DefBuilder* ctx);

const char* _hpb_DefBuilder_MakeFullName(hpb_DefBuilder* ctx,
                                         const char* prefix,
                                         hpb_StringView name);

// Given a symbol and the base symbol inside which it is defined,
// find the symbol's definition.
const void* _hpb_DefBuilder_ResolveAny(hpb_DefBuilder* ctx,
                                       const char* from_name_dbg,
                                       const char* base, hpb_StringView sym,
                                       hpb_deftype_t* type);

const void* _hpb_DefBuilder_Resolve(hpb_DefBuilder* ctx,
                                    const char* from_name_dbg, const char* base,
                                    hpb_StringView sym, hpb_deftype_t type);

char _hpb_DefBuilder_ParseEscape(hpb_DefBuilder* ctx, const hpb_FieldDef* f,
                                 const char** src, const char* end);

const char* _hpb_DefBuilder_FullToShort(const char* fullname);

HPB_INLINE void* _hpb_DefBuilder_Alloc(hpb_DefBuilder* ctx, size_t bytes) {
  if (bytes == 0) return NULL;
  void* ret = hpb_Arena_Malloc(ctx->arena, bytes);
  if (!ret) _hpb_DefBuilder_OomErr(ctx);
  return ret;
}

// Adds a symbol |v| to the symtab, which must be a def pointer previously
// packed with pack_def(). The def's pointer to hpb_FileDef* must be set before
// adding, so we know which entries to remove if building this file fails.
HPB_INLINE void _hpb_DefBuilder_Add(hpb_DefBuilder* ctx, const char* name,
                                    hpb_value v) {
  hpb_StringView sym = {.data = name, .size = strlen(name)};
  bool ok = _hpb_DefPool_InsertSym(ctx->symtab, sym, v, ctx->status);
  if (!ok) _hpb_DefBuilder_FailJmp(ctx);
}

HPB_INLINE hpb_Arena* _hpb_DefBuilder_Arena(const hpb_DefBuilder* ctx) {
  return ctx->arena;
}

HPB_INLINE hpb_FileDef* _hpb_DefBuilder_File(const hpb_DefBuilder* ctx) {
  return ctx->file;
}

// This version of CheckIdent() is only called by other, faster versions after
// they detect a parsing error.
void _hpb_DefBuilder_CheckIdentSlow(hpb_DefBuilder* ctx, hpb_StringView name,
                                    bool full);

// Verify a full identifier string. This is slightly more complicated than
// verifying a relative identifier string because we must track '.' chars.
HPB_INLINE void _hpb_DefBuilder_CheckIdentFull(hpb_DefBuilder* ctx,
                                               hpb_StringView name) {
  bool good = name.size > 0;
  bool start = true;

  for (size_t i = 0; i < name.size; i++) {
    const char c = name.data[i];
    const char d = c | 0x20;  // force lowercase
    const bool is_alpha = (('a' <= d) & (d <= 'z')) | (c == '_');
    const bool is_numer = ('0' <= c) & (c <= '9') & !start;
    const bool is_dot = (c == '.') & !start;

    good &= is_alpha | is_numer | is_dot;
    start = is_dot;
  }

  if (!good) _hpb_DefBuilder_CheckIdentSlow(ctx, name, true);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_DEF_BUILDER_INTERNAL_H_ */
