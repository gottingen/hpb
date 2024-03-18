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

#include "hpb/reflection/message.h"

#include <string.h>

#include "hpb/collections/map.h"
#include "hpb/hash/common.h"
#include "hpb/message/accessors.h"
#include "hpb/message/message.h"
#include "hpb/mini_table/field.h"
#include "hpb/reflection/def.h"
#include "hpb/reflection/def_pool.h"
#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/field_def.h"
#include "hpb/reflection/message_def.h"
#include "hpb/reflection/oneof_def.h"

// Must be last.
#include "hpb/port/def.inc"

bool hpb_Message_HasFieldByDef(const hpb_Message* msg, const hpb_FieldDef* f) {
  HPB_ASSERT(hpb_FieldDef_HasPresence(f));
  return hpb_Message_HasField(msg, hpb_FieldDef_MiniTable(f));
}

const hpb_FieldDef* hpb_Message_WhichOneof(const hpb_Message* msg,
                                           const hpb_OneofDef* o) {
  const hpb_FieldDef* f = hpb_OneofDef_Field(o, 0);
  if (hpb_OneofDef_IsSynthetic(o)) {
    HPB_ASSERT(hpb_OneofDef_FieldCount(o) == 1);
    return hpb_Message_HasFieldByDef(msg, f) ? f : NULL;
  } else {
    const hpb_MiniTableField* field = hpb_FieldDef_MiniTable(f);
    uint32_t oneof_case = hpb_Message_WhichOneofFieldNumber(msg, field);
    f = oneof_case ? hpb_OneofDef_LookupNumber(o, oneof_case) : NULL;
    HPB_ASSERT((f != NULL) == (oneof_case != 0));
    return f;
  }
}

hpb_MessageValue hpb_Message_GetFieldByDef(const hpb_Message* msg,
                                           const hpb_FieldDef* f) {
  hpb_MessageValue default_val = hpb_FieldDef_Default(f);
  hpb_MessageValue ret;
  _hpb_Message_GetField(msg, hpb_FieldDef_MiniTable(f), &default_val, &ret);
  return ret;
}

hpb_MutableMessageValue hpb_Message_Mutable(hpb_Message* msg,
                                            const hpb_FieldDef* f,
                                            hpb_Arena* a) {
  HPB_ASSERT(hpb_FieldDef_IsSubMessage(f) || hpb_FieldDef_IsRepeated(f));
  if (hpb_FieldDef_HasPresence(f) && !hpb_Message_HasFieldByDef(msg, f)) {
    // We need to skip the hpb_Message_GetFieldByDef() call in this case.
    goto make;
  }

  hpb_MessageValue val = hpb_Message_GetFieldByDef(msg, f);
  if (val.array_val) {
    return (hpb_MutableMessageValue){.array = (hpb_Array*)val.array_val};
  }

  hpb_MutableMessageValue ret;
make:
  if (!a) return (hpb_MutableMessageValue){.array = NULL};
  if (hpb_FieldDef_IsMap(f)) {
    const hpb_MessageDef* entry = hpb_FieldDef_MessageSubDef(f);
    const hpb_FieldDef* key =
        hpb_MessageDef_FindFieldByNumber(entry, kHpb_MapEntry_KeyFieldNumber);
    const hpb_FieldDef* value =
        hpb_MessageDef_FindFieldByNumber(entry, kHpb_MapEntry_ValueFieldNumber);
    ret.map =
        hpb_Map_New(a, hpb_FieldDef_CType(key), hpb_FieldDef_CType(value));
  } else if (hpb_FieldDef_IsRepeated(f)) {
    ret.array = hpb_Array_New(a, hpb_FieldDef_CType(f));
  } else {
    HPB_ASSERT(hpb_FieldDef_IsSubMessage(f));
    const hpb_MessageDef* m = hpb_FieldDef_MessageSubDef(f);
    ret.msg = hpb_Message_New(hpb_MessageDef_MiniTable(m), a);
  }

  val.array_val = ret.array;
  hpb_Message_SetFieldByDef(msg, f, val, a);

  return ret;
}

bool hpb_Message_SetFieldByDef(hpb_Message* msg, const hpb_FieldDef* f,
                               hpb_MessageValue val, hpb_Arena* a) {
  return _hpb_Message_SetField(msg, hpb_FieldDef_MiniTable(f), &val, a);
}

void hpb_Message_ClearFieldByDef(hpb_Message* msg, const hpb_FieldDef* f) {
  hpb_Message_ClearField(msg, hpb_FieldDef_MiniTable(f));
}

void hpb_Message_ClearByDef(hpb_Message* msg, const hpb_MessageDef* m) {
  hpb_Message_Clear(msg, hpb_MessageDef_MiniTable(m));
}

bool hpb_Message_Next(const hpb_Message* msg, const hpb_MessageDef* m,
                      const hpb_DefPool* ext_pool, const hpb_FieldDef** out_f,
                      hpb_MessageValue* out_val, size_t* iter) {
  size_t i = *iter;
  size_t n = hpb_MessageDef_FieldCount(m);
  HPB_UNUSED(ext_pool);

  // Iterate over normal fields, returning the first one that is set.
  while (++i < n) {
    const hpb_FieldDef* f = hpb_MessageDef_Field(m, i);
    const hpb_MiniTableField* field = hpb_FieldDef_MiniTable(f);
    hpb_MessageValue val = hpb_Message_GetFieldByDef(msg, f);

    // Skip field if unset or empty.
    if (hpb_MiniTableField_HasPresence(field)) {
      if (!hpb_Message_HasFieldByDef(msg, f)) continue;
    } else {
      switch (hpb_FieldMode_Get(field)) {
        case kHpb_FieldMode_Map:
          if (!val.map_val || hpb_Map_Size(val.map_val) == 0) continue;
          break;
        case kHpb_FieldMode_Array:
          if (!val.array_val || hpb_Array_Size(val.array_val) == 0) continue;
          break;
        case kHpb_FieldMode_Scalar:
          if (!_hpb_MiniTable_ValueIsNonZero(&val, field)) continue;
          break;
      }
    }

    *out_val = val;
    *out_f = f;
    *iter = i;
    return true;
  }

  if (ext_pool) {
    // Return any extensions that are set.
    size_t count;
    const hpb_Message_Extension* ext = _hpb_Message_Getexts(msg, &count);
    if (i - n < count) {
      ext += count - 1 - (i - n);
      memcpy(out_val, &ext->data, sizeof(*out_val));
      *out_f = hpb_DefPool_FindExtensionByMiniTable(ext_pool, ext->ext);
      *iter = i;
      return true;
    }
  }

  *iter = i;
  return false;
}

bool _hpb_Message_DiscardUnknown(hpb_Message* msg, const hpb_MessageDef* m,
                                 int depth) {
  size_t iter = kHpb_Message_Begin;
  const hpb_FieldDef* f;
  hpb_MessageValue val;
  bool ret = true;

  if (--depth == 0) return false;

  _hpb_Message_DiscardUnknown_shallow(msg);

  while (hpb_Message_Next(msg, m, NULL /*ext_pool*/, &f, &val, &iter)) {
    const hpb_MessageDef* subm = hpb_FieldDef_MessageSubDef(f);
    if (!subm) continue;
    if (hpb_FieldDef_IsMap(f)) {
      const hpb_FieldDef* val_f = hpb_MessageDef_FindFieldByNumber(subm, 2);
      const hpb_MessageDef* val_m = hpb_FieldDef_MessageSubDef(val_f);
      hpb_Map* map = (hpb_Map*)val.map_val;
      size_t iter = kHpb_Map_Begin;

      if (!val_m) continue;

      hpb_MessageValue map_key, map_val;
      while (hpb_Map_Next(map, &map_key, &map_val, &iter)) {
        if (!_hpb_Message_DiscardUnknown((hpb_Message*)map_val.msg_val, val_m,
                                         depth)) {
          ret = false;
        }
      }
    } else if (hpb_FieldDef_IsRepeated(f)) {
      const hpb_Array* arr = val.array_val;
      size_t i, n = hpb_Array_Size(arr);
      for (i = 0; i < n; i++) {
        hpb_MessageValue elem = hpb_Array_Get(arr, i);
        if (!_hpb_Message_DiscardUnknown((hpb_Message*)elem.msg_val, subm,
                                         depth)) {
          ret = false;
        }
      }
    } else {
      if (!_hpb_Message_DiscardUnknown((hpb_Message*)val.msg_val, subm,
                                       depth)) {
        ret = false;
      }
    }
  }

  return ret;
}

bool hpb_Message_DiscardUnknown(hpb_Message* msg, const hpb_MessageDef* m,
                                int maxdepth) {
  return _hpb_Message_DiscardUnknown(msg, m, maxdepth);
}
