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

#include "hpb/reflection/internal/oneof_def.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "hpb/hash/int_table.h"
#include "hpb/hash/str_table.h"
#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/field_def.h"
#include "hpb/reflection/internal/message_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_OneofDef {
  const HPB_DESC(OneofOptions) * opts;
  const hpb_MessageDef* parent;
  const char* full_name;
  int field_count;
  bool synthetic;
  const hpb_FieldDef** fields;
  hpb_strtable ntof;  // lookup a field by name
  hpb_inttable itof;  // lookup a field by number (index)
#if UINTPTR_MAX == 0xffffffff
  uint32_t padding;  // Increase size to a multiple of 8.
#endif
};

hpb_OneofDef* _hpb_OneofDef_At(const hpb_OneofDef* o, int i) {
  return (hpb_OneofDef*)&o[i];
}

const HPB_DESC(OneofOptions) * hpb_OneofDef_Options(const hpb_OneofDef* o) {
  return o->opts;
}

bool hpb_OneofDef_HasOptions(const hpb_OneofDef* o) {
  return o->opts != (void*)kHpbDefOptDefault;
}

const char* hpb_OneofDef_FullName(const hpb_OneofDef* o) {
  return o->full_name;
}

const char* hpb_OneofDef_Name(const hpb_OneofDef* o) {
  return _hpb_DefBuilder_FullToShort(o->full_name);
}

const hpb_MessageDef* hpb_OneofDef_ContainingType(const hpb_OneofDef* o) {
  return o->parent;
}

int hpb_OneofDef_FieldCount(const hpb_OneofDef* o) { return o->field_count; }

const hpb_FieldDef* hpb_OneofDef_Field(const hpb_OneofDef* o, int i) {
  HPB_ASSERT(i < o->field_count);
  return o->fields[i];
}

int hpb_OneofDef_numfields(const hpb_OneofDef* o) { return o->field_count; }

uint32_t hpb_OneofDef_Index(const hpb_OneofDef* o) {
  // Compute index in our parent's array.
  return o - hpb_MessageDef_Oneof(o->parent, 0);
}

bool hpb_OneofDef_IsSynthetic(const hpb_OneofDef* o) { return o->synthetic; }

const hpb_FieldDef* hpb_OneofDef_LookupNameWithSize(const hpb_OneofDef* o,
                                                    const char* name,
                                                    size_t size) {
  hpb_value val;
  return hpb_strtable_lookup2(&o->ntof, name, size, &val)
             ? hpb_value_getptr(val)
             : NULL;
}

const hpb_FieldDef* hpb_OneofDef_LookupName(const hpb_OneofDef* o,
                                            const char* name) {
  return hpb_OneofDef_LookupNameWithSize(o, name, strlen(name));
}

const hpb_FieldDef* hpb_OneofDef_LookupNumber(const hpb_OneofDef* o,
                                              uint32_t num) {
  hpb_value val;
  return hpb_inttable_lookup(&o->itof, num, &val) ? hpb_value_getptr(val)
                                                  : NULL;
}

void _hpb_OneofDef_Insert(hpb_DefBuilder* ctx, hpb_OneofDef* o,
                          const hpb_FieldDef* f, const char* name,
                          size_t size) {
  o->field_count++;
  if (_hpb_FieldDef_IsProto3Optional(f)) o->synthetic = true;

  const int number = hpb_FieldDef_Number(f);
  const hpb_value v = hpb_value_constptr(f);

  // TODO(salo): This lookup is unfortunate because we also perform it when
  // inserting into the message's table. Unfortunately that step occurs after
  // this one and moving things around could be tricky so let's leave it for
  // a future refactoring.
  const bool number_exists = hpb_inttable_lookup(&o->itof, number, NULL);
  if (HPB_UNLIKELY(number_exists)) {
    _hpb_DefBuilder_Errf(ctx, "oneof fields have the same number (%d)", number);
  }

  // TODO(salo): More redundant work happening here.
  const bool name_exists = hpb_strtable_lookup2(&o->ntof, name, size, NULL);
  if (HPB_UNLIKELY(name_exists)) {
    _hpb_DefBuilder_Errf(ctx, "oneof fields have the same name (%.*s)",
                         (int)size, name);
  }

  const bool ok = hpb_inttable_insert(&o->itof, number, v, ctx->arena) &&
                  hpb_strtable_insert(&o->ntof, name, size, v, ctx->arena);
  if (HPB_UNLIKELY(!ok)) {
    _hpb_DefBuilder_OomErr(ctx);
  }
}

// Returns the synthetic count.
size_t _hpb_OneofDefs_Finalize(hpb_DefBuilder* ctx, hpb_MessageDef* m) {
  int synthetic_count = 0;

  for (int i = 0; i < hpb_MessageDef_OneofCount(m); i++) {
    hpb_OneofDef* o = (hpb_OneofDef*)hpb_MessageDef_Oneof(m, i);

    if (o->synthetic && o->field_count != 1) {
      _hpb_DefBuilder_Errf(ctx,
                           "Synthetic oneofs must have one field, not %d: %s",
                           o->field_count, hpb_OneofDef_Name(o));
    }

    if (o->synthetic) {
      synthetic_count++;
    } else if (synthetic_count != 0) {
      _hpb_DefBuilder_Errf(
          ctx, "Synthetic oneofs must be after all other oneofs: %s",
          hpb_OneofDef_Name(o));
    }

    o->fields =
        _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_FieldDef*) * o->field_count);
    o->field_count = 0;
  }

  for (int i = 0; i < hpb_MessageDef_FieldCount(m); i++) {
    const hpb_FieldDef* f = hpb_MessageDef_Field(m, i);
    hpb_OneofDef* o = (hpb_OneofDef*)hpb_FieldDef_ContainingOneof(f);
    if (o) {
      o->fields[o->field_count++] = f;
    }
  }

  return synthetic_count;
}

static void create_oneofdef(hpb_DefBuilder* ctx, hpb_MessageDef* m,
                            const HPB_DESC(OneofDescriptorProto) * oneof_proto,
                            const hpb_OneofDef* _o) {
  hpb_OneofDef* o = (hpb_OneofDef*)_o;
  hpb_StringView name = HPB_DESC(OneofDescriptorProto_name)(oneof_proto);

  o->parent = m;
  o->full_name =
      _hpb_DefBuilder_MakeFullName(ctx, hpb_MessageDef_FullName(m), name);
  o->field_count = 0;
  o->synthetic = false;

  HPB_DEF_SET_OPTIONS(o->opts, OneofDescriptorProto, OneofOptions, oneof_proto);

  if (hpb_MessageDef_FindByNameWithSize(m, name.data, name.size, NULL, NULL)) {
    _hpb_DefBuilder_Errf(ctx, "duplicate oneof name (%s)", o->full_name);
  }

  hpb_value v = _hpb_DefType_Pack(o, HPB_DEFTYPE_ONEOF);
  bool ok = _hpb_MessageDef_Insert(m, name.data, name.size, v, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  ok = hpb_inttable_init(&o->itof, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);

  ok = hpb_strtable_init(&o->ntof, 4, ctx->arena);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);
}

// Allocate and initialize an array of |n| oneof defs.
hpb_OneofDef* _hpb_OneofDefs_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(OneofDescriptorProto) * const* protos, hpb_MessageDef* m) {
  _hpb_DefType_CheckPadding(sizeof(hpb_OneofDef));

  hpb_OneofDef* o = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_OneofDef) * n);
  for (int i = 0; i < n; i++) {
    create_oneofdef(ctx, m, protos[i], &o[i]);
  }
  return o;
}
