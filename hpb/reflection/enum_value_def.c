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

#include "hpb/reflection/internal/enum_value_def.h"

#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/enum_def.h"
#include "hpb/reflection/internal/file_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_EnumValueDef {
  const HPB_DESC(EnumValueOptions) * opts;
  const hpb_EnumDef* parent;
  const char* full_name;
  int32_t number;
};

hpb_EnumValueDef* _hpb_EnumValueDef_At(const hpb_EnumValueDef* v, int i) {
  return (hpb_EnumValueDef*)&v[i];
}

static int _hpb_EnumValueDef_Compare(const void* p1, const void* p2) {
  const uint32_t v1 = (*(const hpb_EnumValueDef**)p1)->number;
  const uint32_t v2 = (*(const hpb_EnumValueDef**)p2)->number;
  return (v1 < v2) ? -1 : (v1 > v2);
}

const hpb_EnumValueDef** _hpb_EnumValueDefs_Sorted(const hpb_EnumValueDef* v,
                                                   int n, hpb_Arena* a) {
  // TODO: Try to replace this arena alloc with a persistent scratch buffer.
  hpb_EnumValueDef** out =
      (hpb_EnumValueDef**)hpb_Arena_Malloc(a, n * sizeof(void*));
  if (!out) return NULL;

  for (int i = 0; i < n; i++) {
    out[i] = (hpb_EnumValueDef*)&v[i];
  }
  qsort(out, n, sizeof(void*), _hpb_EnumValueDef_Compare);

  return (const hpb_EnumValueDef**)out;
}

const HPB_DESC(EnumValueOptions) *
    hpb_EnumValueDef_Options(const hpb_EnumValueDef* v) {
  return v->opts;
}

bool hpb_EnumValueDef_HasOptions(const hpb_EnumValueDef* v) {
  return v->opts != (void*)kHpbDefOptDefault;
}

const hpb_EnumDef* hpb_EnumValueDef_Enum(const hpb_EnumValueDef* v) {
  return v->parent;
}

const char* hpb_EnumValueDef_FullName(const hpb_EnumValueDef* v) {
  return v->full_name;
}

const char* hpb_EnumValueDef_Name(const hpb_EnumValueDef* v) {
  return _hpb_DefBuilder_FullToShort(v->full_name);
}

int32_t hpb_EnumValueDef_Number(const hpb_EnumValueDef* v) { return v->number; }

uint32_t hpb_EnumValueDef_Index(const hpb_EnumValueDef* v) {
  // Compute index in our parent's array.
  return v - hpb_EnumDef_Value(v->parent, 0);
}

static void create_enumvaldef(hpb_DefBuilder* ctx, const char* prefix,
                              const HPB_DESC(EnumValueDescriptorProto) *
                                  val_proto,
                              hpb_EnumDef* e, hpb_EnumValueDef* v) {
  hpb_StringView name = HPB_DESC(EnumValueDescriptorProto_name)(val_proto);

  v->parent = e;  // Must happen prior to _hpb_DefBuilder_Add()
  v->full_name = _hpb_DefBuilder_MakeFullName(ctx, prefix, name);
  v->number = HPB_DESC(EnumValueDescriptorProto_number)(val_proto);
  _hpb_DefBuilder_Add(ctx, v->full_name,
                      _hpb_DefType_Pack(v, HPB_DEFTYPE_ENUMVAL));

  HPB_DEF_SET_OPTIONS(v->opts, EnumValueDescriptorProto, EnumValueOptions,
                      val_proto);

  bool ok = _hpb_EnumDef_Insert(e, v, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);
}

// Allocate and initialize an array of |n| enum value defs owned by |e|.
hpb_EnumValueDef* _hpb_EnumValueDefs_New(
    hpb_DefBuilder* ctx, const char* prefix, int n,
    const HPB_DESC(EnumValueDescriptorProto) * const* protos, hpb_EnumDef* e,
    bool* is_sorted) {
  _hpb_DefType_CheckPadding(sizeof(hpb_EnumValueDef));

  hpb_EnumValueDef* v =
      _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_EnumValueDef) * n);

  *is_sorted = true;
  uint32_t previous = 0;
  for (int i = 0; i < n; i++) {
    create_enumvaldef(ctx, prefix, protos[i], e, &v[i]);

    const uint32_t current = v[i].number;
    if (previous > current) *is_sorted = false;
    previous = current;
  }

  if (hpb_FileDef_Syntax(ctx->file) == kHpb_Syntax_Proto3 && n > 0 &&
      v[0].number != 0) {
    _hpb_DefBuilder_Errf(ctx,
                         "for proto3, the first enum value must be zero (%s)",
                         hpb_EnumDef_FullName(e));
  }

  return v;
}
