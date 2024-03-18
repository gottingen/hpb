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

#include "hpb/reflection/internal/enum_def.h"

#include "hpb/hash/int_table.h"
#include "hpb/hash/str_table.h"
#include "hpb/mini_descriptor/decode.h"
#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/desc_state.h"
#include "hpb/reflection/internal/enum_reserved_range.h"
#include "hpb/reflection/internal/enum_value_def.h"
#include "hpb/reflection/internal/file_def.h"
#include "hpb/reflection/internal/message_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_EnumDef {
  const HPB_DESC(EnumOptions) * opts;
  const hpb_MiniTableEnum* layout;  // Only for proto2.
  const hpb_FileDef* file;
  const hpb_MessageDef* containing_type;  // Could be merged with "file".
  const char* full_name;
  hpb_strtable ntoi;
  hpb_inttable iton;
  const hpb_EnumValueDef* values;
  const hpb_EnumReservedRange* res_ranges;
  const hpb_StringView* res_names;
  int value_count;
  int res_range_count;
  int res_name_count;
  int32_t defaultval;
  bool is_closed;
  bool is_sorted;  // Whether all of the values are defined in ascending order.
};

hpb_EnumDef* _hpb_EnumDef_At(const hpb_EnumDef* e, int i) {
  return (hpb_EnumDef*)&e[i];
}

const hpb_MiniTableEnum* _hpb_EnumDef_MiniTable(const hpb_EnumDef* e) {
  return e->layout;
}

bool _hpb_EnumDef_Insert(hpb_EnumDef* e, hpb_EnumValueDef* v, hpb_Arena* a) {
  const char* name = hpb_EnumValueDef_Name(v);
  const hpb_value val = hpb_value_constptr(v);
  bool ok = hpb_strtable_insert(&e->ntoi, name, strlen(name), val, a);
  if (!ok) return false;

  // Multiple enumerators can have the same number, first one wins.
  const int number = hpb_EnumValueDef_Number(v);
  if (!hpb_inttable_lookup(&e->iton, number, NULL)) {
    return hpb_inttable_insert(&e->iton, number, val, a);
  }
  return true;
}

const HPB_DESC(EnumOptions) * hpb_EnumDef_Options(const hpb_EnumDef* e) {
  return e->opts;
}

bool hpb_EnumDef_HasOptions(const hpb_EnumDef* e) {
  return e->opts != (void*)kHpbDefOptDefault;
}

const char* hpb_EnumDef_FullName(const hpb_EnumDef* e) { return e->full_name; }

const char* hpb_EnumDef_Name(const hpb_EnumDef* e) {
  return _hpb_DefBuilder_FullToShort(e->full_name);
}

const hpb_FileDef* hpb_EnumDef_File(const hpb_EnumDef* e) { return e->file; }

const hpb_MessageDef* hpb_EnumDef_ContainingType(const hpb_EnumDef* e) {
  return e->containing_type;
}

int32_t hpb_EnumDef_Default(const hpb_EnumDef* e) {
  HPB_ASSERT(hpb_EnumDef_FindValueByNumber(e, e->defaultval));
  return e->defaultval;
}

int hpb_EnumDef_ReservedRangeCount(const hpb_EnumDef* e) {
  return e->res_range_count;
}

const hpb_EnumReservedRange* hpb_EnumDef_ReservedRange(const hpb_EnumDef* e,
                                                       int i) {
  HPB_ASSERT(0 <= i && i < e->res_range_count);
  return _hpb_EnumReservedRange_At(e->res_ranges, i);
}

int hpb_EnumDef_ReservedNameCount(const hpb_EnumDef* e) {
  return e->res_name_count;
}

hpb_StringView hpb_EnumDef_ReservedName(const hpb_EnumDef* e, int i) {
  HPB_ASSERT(0 <= i && i < e->res_name_count);
  return e->res_names[i];
}

int hpb_EnumDef_ValueCount(const hpb_EnumDef* e) { return e->value_count; }

const hpb_EnumValueDef* hpb_EnumDef_FindValueByName(const hpb_EnumDef* e,
                                                    const char* name) {
  return hpb_EnumDef_FindValueByNameWithSize(e, name, strlen(name));
}

const hpb_EnumValueDef* hpb_EnumDef_FindValueByNameWithSize(
    const hpb_EnumDef* e, const char* name, size_t size) {
  hpb_value v;
  return hpb_strtable_lookup2(&e->ntoi, name, size, &v)
             ? hpb_value_getconstptr(v)
             : NULL;
}

const hpb_EnumValueDef* hpb_EnumDef_FindValueByNumber(const hpb_EnumDef* e,
                                                      int32_t num) {
  hpb_value v;
  return hpb_inttable_lookup(&e->iton, num, &v) ? hpb_value_getconstptr(v)
                                                : NULL;
}

bool hpb_EnumDef_CheckNumber(const hpb_EnumDef* e, int32_t num) {
  // We could use hpb_EnumDef_FindValueByNumber(e, num) != NULL, but we expect
  // this to be faster (especially for small numbers).
  return hpb_MiniTableEnum_CheckValue(e->layout, num);
}

const hpb_EnumValueDef* hpb_EnumDef_Value(const hpb_EnumDef* e, int i) {
  HPB_ASSERT(0 <= i && i < e->value_count);
  return _hpb_EnumValueDef_At(e->values, i);
}

bool hpb_EnumDef_IsClosed(const hpb_EnumDef* e) { return e->is_closed; }

bool hpb_EnumDef_MiniDescriptorEncode(const hpb_EnumDef* e, hpb_Arena* a,
                                      hpb_StringView* out) {
  hpb_DescState s;
  _hpb_DescState_Init(&s);

  const hpb_EnumValueDef** sorted = NULL;
  if (!e->is_sorted) {
    sorted = _hpb_EnumValueDefs_Sorted(e->values, e->value_count, a);
    if (!sorted) return false;
  }

  if (!_hpb_DescState_Grow(&s, a)) return false;
  s.ptr = hpb_MtDataEncoder_StartEnum(&s.e, s.ptr);

  // Duplicate values are allowed but we only encode each value once.
  uint32_t previous = 0;

  for (int i = 0; i < e->value_count; i++) {
    const uint32_t current =
        hpb_EnumValueDef_Number(sorted ? sorted[i] : hpb_EnumDef_Value(e, i));
    if (i != 0 && previous == current) continue;

    if (!_hpb_DescState_Grow(&s, a)) return false;
    s.ptr = hpb_MtDataEncoder_PutEnumValue(&s.e, s.ptr, current);
    previous = current;
  }

  if (!_hpb_DescState_Grow(&s, a)) return false;
  s.ptr = hpb_MtDataEncoder_EndEnum(&s.e, s.ptr);

  // There will always be room for this '\0' in the encoder buffer because
  // kHpb_MtDataEncoder_MinSize is overkill for hpb_MtDataEncoder_EndEnum().
  HPB_ASSERT(s.ptr < s.buf + s.bufsize);
  *s.ptr = '\0';

  out->data = s.buf;
  out->size = s.ptr - s.buf;
  return true;
}

static hpb_MiniTableEnum* create_enumlayout(hpb_DefBuilder* ctx,
                                            const hpb_EnumDef* e) {
  hpb_StringView sv;
  bool ok = hpb_EnumDef_MiniDescriptorEncode(e, ctx->tmp_arena, &sv);
  if (!ok) _hpb_DefBuilder_Errf(ctx, "OOM while building enum MiniDescriptor");

  hpb_Status status;
  hpb_MiniTableEnum* layout =
      hpb_MiniTableEnum_Build(sv.data, sv.size, ctx->arena, &status);
  if (!layout)
    _hpb_DefBuilder_Errf(ctx, "Error building enum MiniTable: %s", status.msg);
  return layout;
}

static hpb_StringView* _hpb_EnumReservedNames_New(
    hpb_DefBuilder* ctx, int n, const hpb_StringView* protos) {
  hpb_StringView* sv = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_StringView) * n);
  for (int i = 0; i < n; i++) {
    sv[i].data =
        hpb_strdup2(protos[i].data, protos[i].size, _hpb_DefBuilder_Arena(ctx));
    sv[i].size = protos[i].size;
  }
  return sv;
}

static void create_enumdef(hpb_DefBuilder* ctx, const char* prefix,
                           const HPB_DESC(EnumDescriptorProto) * enum_proto,
                           hpb_EnumDef* e) {
  const HPB_DESC(EnumValueDescriptorProto)* const* values;
  const HPB_DESC(EnumDescriptorProto_EnumReservedRange)* const* res_ranges;
  const hpb_StringView* res_names;
  hpb_StringView name;
  size_t n_value, n_res_range, n_res_name;

  // Must happen before _hpb_DefBuilder_Add()
  e->file = _hpb_DefBuilder_File(ctx);

  name = HPB_DESC(EnumDescriptorProto_name)(enum_proto);

  e->full_name = _hpb_DefBuilder_MakeFullName(ctx, prefix, name);
  _hpb_DefBuilder_Add(ctx, e->full_name,
                      _hpb_DefType_Pack(e, HPB_DEFTYPE_ENUM));

  e->is_closed = (!HPB_TREAT_PROTO2_ENUMS_LIKE_PROTO3) &&
                 (hpb_FileDef_Syntax(e->file) == kHpb_Syntax_Proto2);

  values = HPB_DESC(EnumDescriptorProto_value)(enum_proto, &n_value);

  bool ok = hpb_strtable_init(&e->ntoi, n_value, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  ok = hpb_inttable_init(&e->iton, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  e->defaultval = 0;
  e->value_count = n_value;
  e->values =
      _hpb_EnumValueDefs_New(ctx, prefix, n_value, values, e, &e->is_sorted);

  if (n_value == 0) {
    _hpb_DefBuilder_Errf(ctx, "enums must contain at least one value (%s)",
                         e->full_name);
  }

  res_ranges =
      HPB_DESC(EnumDescriptorProto_reserved_range)(enum_proto, &n_res_range);
  e->res_range_count = n_res_range;
  e->res_ranges = _hpb_EnumReservedRanges_New(ctx, n_res_range, res_ranges, e);

  res_names =
      HPB_DESC(EnumDescriptorProto_reserved_name)(enum_proto, &n_res_name);
  e->res_name_count = n_res_name;
  e->res_names = _hpb_EnumReservedNames_New(ctx, n_res_name, res_names);

  HPB_DEF_SET_OPTIONS(e->opts, EnumDescriptorProto, EnumOptions, enum_proto);

  hpb_inttable_compact(&e->iton, ctx->arena);

  if (e->is_closed) {
    if (ctx->layout) {
      HPB_ASSERT(ctx->enum_count < ctx->layout->enum_count);
      e->layout = ctx->layout->enums[ctx->enum_count++];
    } else {
      e->layout = create_enumlayout(ctx, e);
    }
  } else {
    e->layout = NULL;
  }
}

hpb_EnumDef* _hpb_EnumDefs_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(EnumDescriptorProto) * const* protos,
    const hpb_MessageDef* containing_type) {
  _hpb_DefType_CheckPadding(sizeof(hpb_EnumDef));

  // If a containing type is defined then get the full name from that.
  // Otherwise use the package name from the file def.
  const char* name = containing_type ? hpb_MessageDef_FullName(containing_type)
                                     : _hpb_FileDef_RawPackage(ctx->file);

  hpb_EnumDef* e = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_EnumDef) * n);
  for (int i = 0; i < n; i++) {
    create_enumdef(ctx, name, protos[i], &e[i]);
    e[i].containing_type = containing_type;
  }
  return e;
}
