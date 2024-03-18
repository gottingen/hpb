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

#include "hpb/reflection/internal/message_def.h"

#include "hpb/hash/int_table.h"
#include "hpb/hash/str_table.h"
#include "hpb/mini_descriptor/decode.h"
#include "hpb/mini_descriptor/internal/modifiers.h"
#include "hpb/reflection/def.h"
#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/desc_state.h"
#include "hpb/reflection/internal/enum_def.h"
#include "hpb/reflection/internal/extension_range.h"
#include "hpb/reflection/internal/field_def.h"
#include "hpb/reflection/internal/file_def.h"
#include "hpb/reflection/internal/message_reserved_range.h"
#include "hpb/reflection/internal/oneof_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_MessageDef {
  const HPB_DESC(MessageOptions) * opts;
  const hpb_MiniTable* layout;
  const hpb_FileDef* file;
  const hpb_MessageDef* containing_type;
  const char* full_name;

  // Tables for looking up fields by number and name.
  hpb_inttable itof;
  hpb_strtable ntof;

  /* All nested defs.
   * MEM: We could save some space here by putting nested defs in a contiguous
   * region and calculating counts from offsets or vice-versa. */
  const hpb_FieldDef* fields;
  const hpb_OneofDef* oneofs;
  const hpb_ExtensionRange* ext_ranges;
  const hpb_StringView* res_names;
  const hpb_MessageDef* nested_msgs;
  const hpb_MessageReservedRange* res_ranges;
  const hpb_EnumDef* nested_enums;
  const hpb_FieldDef* nested_exts;

  // TODO(salo): These counters don't need anywhere near 32 bits.
  int field_count;
  int real_oneof_count;
  int oneof_count;
  int ext_range_count;
  int res_range_count;
  int res_name_count;
  int nested_msg_count;
  int nested_enum_count;
  int nested_ext_count;
  bool in_message_set;
  bool is_sorted;
  hpb_WellKnown well_known_type;
#if UINTPTR_MAX == 0xffffffff
  uint32_t padding;  // Increase size to a multiple of 8.
#endif
};

static void assign_msg_wellknowntype(hpb_MessageDef* m) {
  const char* name = m->full_name;
  if (name == NULL) {
    m->well_known_type = kHpb_WellKnown_Unspecified;
    return;
  }
  if (!strcmp(name, "google.protobuf.Any")) {
    m->well_known_type = kHpb_WellKnown_Any;
  } else if (!strcmp(name, "google.protobuf.FieldMask")) {
    m->well_known_type = kHpb_WellKnown_FieldMask;
  } else if (!strcmp(name, "google.protobuf.Duration")) {
    m->well_known_type = kHpb_WellKnown_Duration;
  } else if (!strcmp(name, "google.protobuf.Timestamp")) {
    m->well_known_type = kHpb_WellKnown_Timestamp;
  } else if (!strcmp(name, "google.protobuf.DoubleValue")) {
    m->well_known_type = kHpb_WellKnown_DoubleValue;
  } else if (!strcmp(name, "google.protobuf.FloatValue")) {
    m->well_known_type = kHpb_WellKnown_FloatValue;
  } else if (!strcmp(name, "google.protobuf.Int64Value")) {
    m->well_known_type = kHpb_WellKnown_Int64Value;
  } else if (!strcmp(name, "google.protobuf.UInt64Value")) {
    m->well_known_type = kHpb_WellKnown_UInt64Value;
  } else if (!strcmp(name, "google.protobuf.Int32Value")) {
    m->well_known_type = kHpb_WellKnown_Int32Value;
  } else if (!strcmp(name, "google.protobuf.UInt32Value")) {
    m->well_known_type = kHpb_WellKnown_UInt32Value;
  } else if (!strcmp(name, "google.protobuf.BoolValue")) {
    m->well_known_type = kHpb_WellKnown_BoolValue;
  } else if (!strcmp(name, "google.protobuf.StringValue")) {
    m->well_known_type = kHpb_WellKnown_StringValue;
  } else if (!strcmp(name, "google.protobuf.BytesValue")) {
    m->well_known_type = kHpb_WellKnown_BytesValue;
  } else if (!strcmp(name, "google.protobuf.Value")) {
    m->well_known_type = kHpb_WellKnown_Value;
  } else if (!strcmp(name, "google.protobuf.ListValue")) {
    m->well_known_type = kHpb_WellKnown_ListValue;
  } else if (!strcmp(name, "google.protobuf.Struct")) {
    m->well_known_type = kHpb_WellKnown_Struct;
  } else {
    m->well_known_type = kHpb_WellKnown_Unspecified;
  }
}

hpb_MessageDef* _hpb_MessageDef_At(const hpb_MessageDef* m, int i) {
  return (hpb_MessageDef*)&m[i];
}

bool _hpb_MessageDef_IsValidExtensionNumber(const hpb_MessageDef* m, int n) {
  for (int i = 0; i < m->ext_range_count; i++) {
    const hpb_ExtensionRange* r = hpb_MessageDef_ExtensionRange(m, i);
    if (hpb_ExtensionRange_Start(r) <= n && n < hpb_ExtensionRange_End(r)) {
      return true;
    }
  }
  return false;
}

const HPB_DESC(MessageOptions) *
    hpb_MessageDef_Options(const hpb_MessageDef* m) {
  return m->opts;
}

bool hpb_MessageDef_HasOptions(const hpb_MessageDef* m) {
  return m->opts != (void*)kHpbDefOptDefault;
}

const char* hpb_MessageDef_FullName(const hpb_MessageDef* m) {
  return m->full_name;
}

const hpb_FileDef* hpb_MessageDef_File(const hpb_MessageDef* m) {
  return m->file;
}

const hpb_MessageDef* hpb_MessageDef_ContainingType(const hpb_MessageDef* m) {
  return m->containing_type;
}

const char* hpb_MessageDef_Name(const hpb_MessageDef* m) {
  return _hpb_DefBuilder_FullToShort(m->full_name);
}

hpb_Syntax hpb_MessageDef_Syntax(const hpb_MessageDef* m) {
  return hpb_FileDef_Syntax(m->file);
}

const hpb_FieldDef* hpb_MessageDef_FindFieldByNumber(const hpb_MessageDef* m,
                                                     uint32_t i) {
  hpb_value val;
  return hpb_inttable_lookup(&m->itof, i, &val) ? hpb_value_getconstptr(val)
                                                : NULL;
}

const hpb_FieldDef* hpb_MessageDef_FindFieldByNameWithSize(
    const hpb_MessageDef* m, const char* name, size_t size) {
  hpb_value val;

  if (!hpb_strtable_lookup2(&m->ntof, name, size, &val)) {
    return NULL;
  }

  return _hpb_DefType_Unpack(val, HPB_DEFTYPE_FIELD);
}

const hpb_OneofDef* hpb_MessageDef_FindOneofByNameWithSize(
    const hpb_MessageDef* m, const char* name, size_t size) {
  hpb_value val;

  if (!hpb_strtable_lookup2(&m->ntof, name, size, &val)) {
    return NULL;
  }

  return _hpb_DefType_Unpack(val, HPB_DEFTYPE_ONEOF);
}

bool _hpb_MessageDef_Insert(hpb_MessageDef* m, const char* name, size_t len,
                            hpb_value v, hpb_Arena* a) {
  return hpb_strtable_insert(&m->ntof, name, len, v, a);
}

bool hpb_MessageDef_FindByNameWithSize(const hpb_MessageDef* m,
                                       const char* name, size_t len,
                                       const hpb_FieldDef** out_f,
                                       const hpb_OneofDef** out_o) {
  hpb_value val;

  if (!hpb_strtable_lookup2(&m->ntof, name, len, &val)) {
    return false;
  }

  const hpb_FieldDef* f = _hpb_DefType_Unpack(val, HPB_DEFTYPE_FIELD);
  const hpb_OneofDef* o = _hpb_DefType_Unpack(val, HPB_DEFTYPE_ONEOF);
  if (out_f) *out_f = f;
  if (out_o) *out_o = o;
  return f || o; /* False if this was a JSON name. */
}

const hpb_FieldDef* hpb_MessageDef_FindByJsonNameWithSize(
    const hpb_MessageDef* m, const char* name, size_t size) {
  hpb_value val;
  const hpb_FieldDef* f;

  if (!hpb_strtable_lookup2(&m->ntof, name, size, &val)) {
    return NULL;
  }

  f = _hpb_DefType_Unpack(val, HPB_DEFTYPE_FIELD);
  if (!f) f = _hpb_DefType_Unpack(val, HPB_DEFTYPE_FIELD_JSONNAME);

  return f;
}

int hpb_MessageDef_ExtensionRangeCount(const hpb_MessageDef* m) {
  return m->ext_range_count;
}

int hpb_MessageDef_ReservedRangeCount(const hpb_MessageDef* m) {
  return m->res_range_count;
}

int hpb_MessageDef_ReservedNameCount(const hpb_MessageDef* m) {
  return m->res_name_count;
}

int hpb_MessageDef_FieldCount(const hpb_MessageDef* m) {
  return m->field_count;
}

int hpb_MessageDef_OneofCount(const hpb_MessageDef* m) {
  return m->oneof_count;
}

int hpb_MessageDef_RealOneofCount(const hpb_MessageDef* m) {
  return m->real_oneof_count;
}

int hpb_MessageDef_NestedMessageCount(const hpb_MessageDef* m) {
  return m->nested_msg_count;
}

int hpb_MessageDef_NestedEnumCount(const hpb_MessageDef* m) {
  return m->nested_enum_count;
}

int hpb_MessageDef_NestedExtensionCount(const hpb_MessageDef* m) {
  return m->nested_ext_count;
}

const hpb_MiniTable* hpb_MessageDef_MiniTable(const hpb_MessageDef* m) {
  return m->layout;
}

const hpb_ExtensionRange* hpb_MessageDef_ExtensionRange(const hpb_MessageDef* m,
                                                        int i) {
  HPB_ASSERT(0 <= i && i < m->ext_range_count);
  return _hpb_ExtensionRange_At(m->ext_ranges, i);
}

const hpb_MessageReservedRange* hpb_MessageDef_ReservedRange(
    const hpb_MessageDef* m, int i) {
  HPB_ASSERT(0 <= i && i < m->res_range_count);
  return _hpb_MessageReservedRange_At(m->res_ranges, i);
}

hpb_StringView hpb_MessageDef_ReservedName(const hpb_MessageDef* m, int i) {
  HPB_ASSERT(0 <= i && i < m->res_name_count);
  return m->res_names[i];
}

const hpb_FieldDef* hpb_MessageDef_Field(const hpb_MessageDef* m, int i) {
  HPB_ASSERT(0 <= i && i < m->field_count);
  return _hpb_FieldDef_At(m->fields, i);
}

const hpb_OneofDef* hpb_MessageDef_Oneof(const hpb_MessageDef* m, int i) {
  HPB_ASSERT(0 <= i && i < m->oneof_count);
  return _hpb_OneofDef_At(m->oneofs, i);
}

const hpb_MessageDef* hpb_MessageDef_NestedMessage(const hpb_MessageDef* m,
                                                   int i) {
  HPB_ASSERT(0 <= i && i < m->nested_msg_count);
  return &m->nested_msgs[i];
}

const hpb_EnumDef* hpb_MessageDef_NestedEnum(const hpb_MessageDef* m, int i) {
  HPB_ASSERT(0 <= i && i < m->nested_enum_count);
  return _hpb_EnumDef_At(m->nested_enums, i);
}

const hpb_FieldDef* hpb_MessageDef_NestedExtension(const hpb_MessageDef* m,
                                                   int i) {
  HPB_ASSERT(0 <= i && i < m->nested_ext_count);
  return _hpb_FieldDef_At(m->nested_exts, i);
}

hpb_WellKnown hpb_MessageDef_WellKnownType(const hpb_MessageDef* m) {
  return m->well_known_type;
}

bool _hpb_MessageDef_InMessageSet(const hpb_MessageDef* m) {
  return m->in_message_set;
}

const hpb_FieldDef* hpb_MessageDef_FindFieldByName(const hpb_MessageDef* m,
                                                   const char* name) {
  return hpb_MessageDef_FindFieldByNameWithSize(m, name, strlen(name));
}

const hpb_OneofDef* hpb_MessageDef_FindOneofByName(const hpb_MessageDef* m,
                                                   const char* name) {
  return hpb_MessageDef_FindOneofByNameWithSize(m, name, strlen(name));
}

bool hpb_MessageDef_IsMapEntry(const hpb_MessageDef* m) {
  return HPB_DESC(MessageOptions_map_entry)(m->opts);
}

bool hpb_MessageDef_IsMessageSet(const hpb_MessageDef* m) {
  return HPB_DESC(MessageOptions_message_set_wire_format)(m->opts);
}

static hpb_MiniTable* _hpb_MessageDef_MakeMiniTable(hpb_DefBuilder* ctx,
                                                    const hpb_MessageDef* m) {
  hpb_StringView desc;
  // Note: this will assign layout_index for fields, so hpb_FieldDef_MiniTable()
  // is safe to call only after this call.
  bool ok = hpb_MessageDef_MiniDescriptorEncode(m, ctx->tmp_arena, &desc);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  void** scratch_data = _hpb_DefPool_ScratchData(ctx->symtab);
  size_t* scratch_size = _hpb_DefPool_ScratchSize(ctx->symtab);
  hpb_MiniTable* ret = hpb_MiniTable_BuildWithBuf(
      desc.data, desc.size, ctx->platform, ctx->arena, scratch_data,
      scratch_size, ctx->status);
  if (!ret) _hpb_DefBuilder_FailJmp(ctx);

  return ret;
}

void _hpb_MessageDef_Resolve(hpb_DefBuilder* ctx, hpb_MessageDef* m) {
  for (int i = 0; i < m->field_count; i++) {
    hpb_FieldDef* f = (hpb_FieldDef*)hpb_MessageDef_Field(m, i);
    _hpb_FieldDef_Resolve(ctx, m->full_name, f);
  }

  m->in_message_set = false;
  for (int i = 0; i < hpb_MessageDef_NestedExtensionCount(m); i++) {
    hpb_FieldDef* ext = (hpb_FieldDef*)hpb_MessageDef_NestedExtension(m, i);
    _hpb_FieldDef_Resolve(ctx, m->full_name, ext);
    if (hpb_FieldDef_Type(ext) == kHpb_FieldType_Message &&
        hpb_FieldDef_Label(ext) == kHpb_Label_Optional &&
        hpb_FieldDef_MessageSubDef(ext) == m &&
        HPB_DESC(MessageOptions_message_set_wire_format)(
            hpb_MessageDef_Options(hpb_FieldDef_ContainingType(ext)))) {
      m->in_message_set = true;
    }
  }

  for (int i = 0; i < hpb_MessageDef_NestedMessageCount(m); i++) {
    hpb_MessageDef* n = (hpb_MessageDef*)hpb_MessageDef_NestedMessage(m, i);
    _hpb_MessageDef_Resolve(ctx, n);
  }
}

void _hpb_MessageDef_InsertField(hpb_DefBuilder* ctx, hpb_MessageDef* m,
                                 const hpb_FieldDef* f) {
  const int32_t field_number = hpb_FieldDef_Number(f);

  if (field_number <= 0 || field_number > kHpb_MaxFieldNumber) {
    _hpb_DefBuilder_Errf(ctx, "invalid field number (%u)", field_number);
  }

  const char* json_name = hpb_FieldDef_JsonName(f);
  const char* shortname = hpb_FieldDef_Name(f);
  const size_t shortnamelen = strlen(shortname);

  hpb_value v = hpb_value_constptr(f);

  hpb_value existing_v;
  if (hpb_strtable_lookup(&m->ntof, shortname, &existing_v)) {
    _hpb_DefBuilder_Errf(ctx, "duplicate field name (%s)", shortname);
  }

  const hpb_value field_v = _hpb_DefType_Pack(f, HPB_DEFTYPE_FIELD);
  bool ok =
      _hpb_MessageDef_Insert(m, shortname, shortnamelen, field_v, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  if (strcmp(shortname, json_name) != 0) {
    if (hpb_strtable_lookup(&m->ntof, json_name, &v)) {
      _hpb_DefBuilder_Errf(ctx, "duplicate json_name (%s)", json_name);
    }

    const size_t json_size = strlen(json_name);
    const hpb_value json_v = _hpb_DefType_Pack(f, HPB_DEFTYPE_FIELD_JSONNAME);
    ok = _hpb_MessageDef_Insert(m, json_name, json_size, json_v, ctx->arena);
    if (!ok) _hpb_DefBuilder_OomErr(ctx);
  }

  if (hpb_inttable_lookup(&m->itof, field_number, NULL)) {
    _hpb_DefBuilder_Errf(ctx, "duplicate field number (%u)", field_number);
  }

  ok = hpb_inttable_insert(&m->itof, field_number, v, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);
}

void _hpb_MessageDef_CreateMiniTable(hpb_DefBuilder* ctx, hpb_MessageDef* m) {
  if (ctx->layout == NULL) {
    m->layout = _hpb_MessageDef_MakeMiniTable(ctx, m);
  } else {
    HPB_ASSERT(ctx->msg_count < ctx->layout->msg_count);
    m->layout = ctx->layout->msgs[ctx->msg_count++];
    HPB_ASSERT(m->field_count == m->layout->field_count);

    // We don't need the result of this call, but it will assign layout_index
    // for all the fields in O(n lg n) time.
    _hpb_FieldDefs_Sorted(m->fields, m->field_count, ctx->tmp_arena);
  }

  for (int i = 0; i < m->nested_msg_count; i++) {
    hpb_MessageDef* nested =
        (hpb_MessageDef*)hpb_MessageDef_NestedMessage(m, i);
    _hpb_MessageDef_CreateMiniTable(ctx, nested);
  }
}

void _hpb_MessageDef_LinkMiniTable(hpb_DefBuilder* ctx,
                                   const hpb_MessageDef* m) {
  for (int i = 0; i < hpb_MessageDef_NestedExtensionCount(m); i++) {
    const hpb_FieldDef* ext = hpb_MessageDef_NestedExtension(m, i);
    _hpb_FieldDef_BuildMiniTableExtension(ctx, ext);
  }

  for (int i = 0; i < m->nested_msg_count; i++) {
    _hpb_MessageDef_LinkMiniTable(ctx, hpb_MessageDef_NestedMessage(m, i));
  }

  if (ctx->layout) return;

  for (int i = 0; i < m->field_count; i++) {
    const hpb_FieldDef* f = hpb_MessageDef_Field(m, i);
    const hpb_MessageDef* sub_m = hpb_FieldDef_MessageSubDef(f);
    const hpb_EnumDef* sub_e = hpb_FieldDef_EnumSubDef(f);
    const int layout_index = _hpb_FieldDef_LayoutIndex(f);
    hpb_MiniTable* mt = (hpb_MiniTable*)hpb_MessageDef_MiniTable(m);

    HPB_ASSERT(layout_index < m->field_count);
    hpb_MiniTableField* mt_f =
        (hpb_MiniTableField*)&m->layout->fields[layout_index];
    if (sub_m) {
      if (!mt->subs) {
        _hpb_DefBuilder_Errf(ctx, "unexpected submsg for (%s)", m->full_name);
      }
      HPB_ASSERT(mt_f);
      HPB_ASSERT(sub_m->layout);
      if (HPB_UNLIKELY(!hpb_MiniTable_SetSubMessage(mt, mt_f, sub_m->layout))) {
        _hpb_DefBuilder_Errf(ctx, "invalid submsg for (%s)", m->full_name);
      }
    } else if (_hpb_FieldDef_IsClosedEnum(f)) {
      const hpb_MiniTableEnum* mt_e = _hpb_EnumDef_MiniTable(sub_e);
      if (HPB_UNLIKELY(!hpb_MiniTable_SetSubEnum(mt, mt_f, mt_e))) {
        _hpb_DefBuilder_Errf(ctx, "invalid subenum for (%s)", m->full_name);
      }
    }
  }

#ifndef NDEBUG
  for (int i = 0; i < m->field_count; i++) {
    const hpb_FieldDef* f = hpb_MessageDef_Field(m, i);
    const int layout_index = _hpb_FieldDef_LayoutIndex(f);
    HPB_ASSERT(layout_index < m->layout->field_count);
    const hpb_MiniTableField* mt_f = &m->layout->fields[layout_index];
    HPB_ASSERT(hpb_FieldDef_Type(f) == hpb_MiniTableField_Type(mt_f));
    HPB_ASSERT(hpb_FieldDef_CType(f) == hpb_MiniTableField_CType(mt_f));
    HPB_ASSERT(hpb_FieldDef_HasPresence(f) ==
               hpb_MiniTableField_HasPresence(mt_f));
  }
#endif
}

static uint64_t _hpb_MessageDef_Modifiers(const hpb_MessageDef* m) {
  uint64_t out = 0;
  if (hpb_FileDef_Syntax(m->file) == kHpb_Syntax_Proto3) {
    out |= kHpb_MessageModifier_ValidateUtf8;
    out |= kHpb_MessageModifier_DefaultIsPacked;
  }
  if (m->ext_range_count) {
    out |= kHpb_MessageModifier_IsExtendable;
  }
  return out;
}

static bool _hpb_MessageDef_EncodeMap(hpb_DescState* s, const hpb_MessageDef* m,
                                      hpb_Arena* a) {
  if (m->field_count != 2) return false;

  const hpb_FieldDef* key_field = hpb_MessageDef_Field(m, 0);
  const hpb_FieldDef* val_field = hpb_MessageDef_Field(m, 1);
  if (key_field == NULL || val_field == NULL) return false;

  HPB_ASSERT(_hpb_FieldDef_LayoutIndex(key_field) == 0);
  HPB_ASSERT(_hpb_FieldDef_LayoutIndex(val_field) == 1);

  s->ptr = hpb_MtDataEncoder_EncodeMap(
      &s->e, s->ptr, hpb_FieldDef_Type(key_field), hpb_FieldDef_Type(val_field),
      _hpb_FieldDef_Modifiers(key_field), _hpb_FieldDef_Modifiers(val_field));
  return true;
}

static bool _hpb_MessageDef_EncodeMessage(hpb_DescState* s,
                                          const hpb_MessageDef* m,
                                          hpb_Arena* a) {
  const hpb_FieldDef** sorted = NULL;
  if (!m->is_sorted) {
    sorted = _hpb_FieldDefs_Sorted(m->fields, m->field_count, a);
    if (!sorted) return false;
  }

  s->ptr = hpb_MtDataEncoder_StartMessage(&s->e, s->ptr,
                                          _hpb_MessageDef_Modifiers(m));

  for (int i = 0; i < m->field_count; i++) {
    const hpb_FieldDef* f = sorted ? sorted[i] : hpb_MessageDef_Field(m, i);
    const hpb_FieldType type = hpb_FieldDef_Type(f);
    const int number = hpb_FieldDef_Number(f);
    const uint64_t modifiers = _hpb_FieldDef_Modifiers(f);

    if (!_hpb_DescState_Grow(s, a)) return false;
    s->ptr = hpb_MtDataEncoder_PutField(&s->e, s->ptr, type, number, modifiers);
  }

  for (int i = 0; i < m->real_oneof_count; i++) {
    if (!_hpb_DescState_Grow(s, a)) return false;
    s->ptr = hpb_MtDataEncoder_StartOneof(&s->e, s->ptr);

    const hpb_OneofDef* o = hpb_MessageDef_Oneof(m, i);
    const int field_count = hpb_OneofDef_FieldCount(o);
    for (int j = 0; j < field_count; j++) {
      const int number = hpb_FieldDef_Number(hpb_OneofDef_Field(o, j));

      if (!_hpb_DescState_Grow(s, a)) return false;
      s->ptr = hpb_MtDataEncoder_PutOneofField(&s->e, s->ptr, number);
    }
  }

  return true;
}

static bool _hpb_MessageDef_EncodeMessageSet(hpb_DescState* s,
                                             const hpb_MessageDef* m,
                                             hpb_Arena* a) {
  s->ptr = hpb_MtDataEncoder_EncodeMessageSet(&s->e, s->ptr);

  return true;
}

bool hpb_MessageDef_MiniDescriptorEncode(const hpb_MessageDef* m, hpb_Arena* a,
                                         hpb_StringView* out) {
  hpb_DescState s;
  _hpb_DescState_Init(&s);

  if (!_hpb_DescState_Grow(&s, a)) return false;

  if (hpb_MessageDef_IsMapEntry(m)) {
    if (!_hpb_MessageDef_EncodeMap(&s, m, a)) return false;
  } else if (HPB_DESC(MessageOptions_message_set_wire_format)(m->opts)) {
    if (!_hpb_MessageDef_EncodeMessageSet(&s, m, a)) return false;
  } else {
    if (!_hpb_MessageDef_EncodeMessage(&s, m, a)) return false;
  }

  if (!_hpb_DescState_Grow(&s, a)) return false;
  *s.ptr = '\0';

  out->data = s.buf;
  out->size = s.ptr - s.buf;
  return true;
}

static hpb_StringView* _hpb_ReservedNames_New(hpb_DefBuilder* ctx, int n,
                                              const hpb_StringView* protos) {
  hpb_StringView* sv = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_StringView) * n);
  for (int i = 0; i < n; i++) {
    sv[i].data =
        hpb_strdup2(protos[i].data, protos[i].size, _hpb_DefBuilder_Arena(ctx));
    sv[i].size = protos[i].size;
  }
  return sv;
}

static void create_msgdef(hpb_DefBuilder* ctx, const char* prefix,
                          const HPB_DESC(DescriptorProto) * msg_proto,
                          const hpb_MessageDef* containing_type,
                          hpb_MessageDef* m) {
  const HPB_DESC(OneofDescriptorProto)* const* oneofs;
  const HPB_DESC(FieldDescriptorProto)* const* fields;
  const HPB_DESC(DescriptorProto_ExtensionRange)* const* ext_ranges;
  const HPB_DESC(DescriptorProto_ReservedRange)* const* res_ranges;
  const hpb_StringView* res_names;
  size_t n_oneof, n_field, n_enum, n_ext, n_msg;
  size_t n_ext_range, n_res_range, n_res_name;
  hpb_StringView name;

  // Must happen before _hpb_DefBuilder_Add()
  m->file = _hpb_DefBuilder_File(ctx);

  m->containing_type = containing_type;
  m->is_sorted = true;

  name = HPB_DESC(DescriptorProto_name)(msg_proto);

  m->full_name = _hpb_DefBuilder_MakeFullName(ctx, prefix, name);
  _hpb_DefBuilder_Add(ctx, m->full_name, _hpb_DefType_Pack(m, HPB_DEFTYPE_MSG));

  oneofs = HPB_DESC(DescriptorProto_oneof_decl)(msg_proto, &n_oneof);
  fields = HPB_DESC(DescriptorProto_field)(msg_proto, &n_field);
  ext_ranges =
      HPB_DESC(DescriptorProto_extension_range)(msg_proto, &n_ext_range);
  res_ranges =
      HPB_DESC(DescriptorProto_reserved_range)(msg_proto, &n_res_range);
  res_names = HPB_DESC(DescriptorProto_reserved_name)(msg_proto, &n_res_name);

  bool ok = hpb_inttable_init(&m->itof, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  ok = hpb_strtable_init(&m->ntof, n_oneof + n_field, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  HPB_DEF_SET_OPTIONS(m->opts, DescriptorProto, MessageOptions, msg_proto);

  m->oneof_count = n_oneof;
  m->oneofs = _hpb_OneofDefs_New(ctx, n_oneof, oneofs, m);

  m->field_count = n_field;
  m->fields =
      _hpb_FieldDefs_New(ctx, n_field, fields, m->full_name, m, &m->is_sorted);

  // Message Sets may not contain fields.
  if (HPB_UNLIKELY(HPB_DESC(MessageOptions_message_set_wire_format)(m->opts))) {
    if (HPB_UNLIKELY(n_field > 0)) {
      _hpb_DefBuilder_Errf(ctx, "invalid message set (%s)", m->full_name);
    }
  }

  m->ext_range_count = n_ext_range;
  m->ext_ranges = _hpb_ExtensionRanges_New(ctx, n_ext_range, ext_ranges, m);

  m->res_range_count = n_res_range;
  m->res_ranges =
      _hpb_MessageReservedRanges_New(ctx, n_res_range, res_ranges, m);

  m->res_name_count = n_res_name;
  m->res_names = _hpb_ReservedNames_New(ctx, n_res_name, res_names);

  const size_t synthetic_count = _hpb_OneofDefs_Finalize(ctx, m);
  m->real_oneof_count = m->oneof_count - synthetic_count;

  assign_msg_wellknowntype(m);
  hpb_inttable_compact(&m->itof, ctx->arena);

  const HPB_DESC(EnumDescriptorProto)* const* enums =
      HPB_DESC(DescriptorProto_enum_type)(msg_proto, &n_enum);
  m->nested_enum_count = n_enum;
  m->nested_enums = _hpb_EnumDefs_New(ctx, n_enum, enums, m);

  const HPB_DESC(FieldDescriptorProto)* const* exts =
      HPB_DESC(DescriptorProto_extension)(msg_proto, &n_ext);
  m->nested_ext_count = n_ext;
  m->nested_exts = _hpb_Extensions_New(ctx, n_ext, exts, m->full_name, m);

  const HPB_DESC(DescriptorProto)* const* msgs =
      HPB_DESC(DescriptorProto_nested_type)(msg_proto, &n_msg);
  m->nested_msg_count = n_msg;
  m->nested_msgs = _hpb_MessageDefs_New(ctx, n_msg, msgs, m);
}

// Allocate and initialize an array of |n| message defs.
hpb_MessageDef* _hpb_MessageDefs_New(
    hpb_DefBuilder* ctx, int n, const HPB_DESC(DescriptorProto) * const* protos,
    const hpb_MessageDef* containing_type) {
  _hpb_DefType_CheckPadding(sizeof(hpb_MessageDef));

  const char* name = containing_type ? containing_type->full_name
                                     : _hpb_FileDef_RawPackage(ctx->file);

  hpb_MessageDef* m = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_MessageDef) * n);
  for (int i = 0; i < n; i++) {
    create_msgdef(ctx, name, protos[i], containing_type, &m[i]);
  }
  return m;
}
