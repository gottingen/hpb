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

#include "hpb/message/accessors.h"

#include "hpb/collections/array.h"
#include "hpb/collections/internal/array.h"
#include "hpb/collections/map.h"
#include "hpb/message/message.h"
#include "hpb/mini_table/field.h"
#include "hpb/wire/decode.h"
#include "hpb/wire/encode.h"
#include "hpb/wire/eps_copy_input_stream.h"
#include "hpb/wire/reader.h"

// Must be last.
#include "hpb/port/def.inc"

hpb_MapInsertStatus hpb_Message_InsertMapEntry(hpb_Map* map,
                                               const hpb_MiniTable* mini_table,
                                               const hpb_MiniTableField* field,
                                               hpb_Message* map_entry_message,
                                               hpb_Arena* arena) {
  const hpb_MiniTable* map_entry_mini_table =
      mini_table->subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(map_entry_mini_table);
  HPB_ASSERT(map_entry_mini_table->field_count == 2);
  const hpb_MiniTableField* map_entry_key_field =
      &map_entry_mini_table->fields[0];
  const hpb_MiniTableField* map_entry_value_field =
      &map_entry_mini_table->fields[1];
  // Map key/value cannot have explicit defaults,
  // hence assuming a zero default is valid.
  hpb_MessageValue default_val;
  memset(&default_val, 0, sizeof(hpb_MessageValue));
  hpb_MessageValue map_entry_key;
  hpb_MessageValue map_entry_value;
  _hpb_Message_GetField(map_entry_message, map_entry_key_field, &default_val,
                        &map_entry_key);
  _hpb_Message_GetField(map_entry_message, map_entry_value_field, &default_val,
                        &map_entry_value);
  return hpb_Map_Insert(map, map_entry_key, map_entry_value, arena);
}

bool hpb_Message_IsExactlyEqual(const hpb_Message* m1, const hpb_Message* m2,
                                const hpb_MiniTable* layout) {
  if (m1 == m2) return true;

  int opts = kHpb_EncodeOption_SkipUnknown | kHpb_EncodeOption_Deterministic;
  hpb_Arena* a = hpb_Arena_New();

  // Compare deterministically serialized payloads with no unknown fields.
  size_t size1, size2;
  char *data1, *data2;
  hpb_EncodeStatus status1 = hpb_Encode(m1, layout, opts, a, &data1, &size1);
  hpb_EncodeStatus status2 = hpb_Encode(m2, layout, opts, a, &data2, &size2);

  if (status1 != kHpb_EncodeStatus_Ok || status2 != kHpb_EncodeStatus_Ok) {
    // TODO(salo): How should we fail here? (In Ruby we throw an exception.)
    hpb_Arena_Free(a);
    return false;
  }

  const bool ret = (size1 == size2) && (memcmp(data1, data2, size1) == 0);
  hpb_Arena_Free(a);
  return ret;
}
