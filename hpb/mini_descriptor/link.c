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

#include "hpb/mini_descriptor/link.h"

// Must be last.
#include "hpb/port/def.inc"

bool hpb_MiniTable_SetSubMessage(hpb_MiniTable* table,
                                 hpb_MiniTableField* field,
                                 const hpb_MiniTable* sub) {
  HPB_ASSERT((uintptr_t)table->fields <= (uintptr_t)field &&
             (uintptr_t)field <
                 (uintptr_t)(table->fields + table->field_count));
  HPB_ASSERT(sub);

  const bool sub_is_map = sub->ext & kHpb_ExtMode_IsMapEntry;

  switch (field->HPB_PRIVATE(descriptortype)) {
    case kHpb_FieldType_Message:
      if (sub_is_map) {
        const bool table_is_map = table->ext & kHpb_ExtMode_IsMapEntry;
        if (HPB_UNLIKELY(table_is_map)) return false;

        field->mode = (field->mode & ~kHpb_FieldMode_Mask) | kHpb_FieldMode_Map;
      }
      break;

    case kHpb_FieldType_Group:
      if (HPB_UNLIKELY(sub_is_map)) return false;
      break;

    default:
      return false;
  }

  hpb_MiniTableSub* table_sub =
      (void*)&table->subs[field->HPB_PRIVATE(submsg_index)];
  // TODO(haberman): Add this assert back once YouTube is updated to not call
  // this function repeatedly.
  // HPB_ASSERT(table_sub->submsg == &_kHpb_MiniTable_Empty);
  table_sub->submsg = sub;
  return true;
}

bool hpb_MiniTable_SetSubEnum(hpb_MiniTable* table, hpb_MiniTableField* field,
                              const hpb_MiniTableEnum* sub) {
  HPB_ASSERT((uintptr_t)table->fields <= (uintptr_t)field &&
             (uintptr_t)field <
                 (uintptr_t)(table->fields + table->field_count));
  HPB_ASSERT(sub);

  hpb_MiniTableSub* table_sub =
      (void*)&table->subs[field->HPB_PRIVATE(submsg_index)];
  table_sub->subenum = sub;
  return true;
}

uint32_t hpb_MiniTable_GetSubList(const hpb_MiniTable* mt,
                                  const hpb_MiniTableField** subs) {
  uint32_t msg_count = 0;
  uint32_t enum_count = 0;

  for (int i = 0; i < mt->field_count; i++) {
    const hpb_MiniTableField* f = &mt->fields[i];
    if (hpb_MiniTableField_CType(f) == kHpb_CType_Message) {
      *subs = f;
      ++subs;
      msg_count++;
    }
  }

  for (int i = 0; i < mt->field_count; i++) {
    const hpb_MiniTableField* f = &mt->fields[i];
    if (hpb_MiniTableField_CType(f) == kHpb_CType_Enum) {
      *subs = f;
      ++subs;
      enum_count++;
    }
  }

  return (msg_count << 16) | enum_count;
}

// The list of sub_tables and sub_enums must exactly match the number and order
// of sub-message fields and sub-enum fields given by hpb_MiniTable_GetSubList()
// above.
bool hpb_MiniTable_Link(hpb_MiniTable* mt, const hpb_MiniTable** sub_tables,
                        size_t sub_table_count,
                        const hpb_MiniTableEnum** sub_enums,
                        size_t sub_enum_count) {
  uint32_t msg_count = 0;
  uint32_t enum_count = 0;

  for (int i = 0; i < mt->field_count; i++) {
    hpb_MiniTableField* f = (hpb_MiniTableField*)&mt->fields[i];
    if (hpb_MiniTableField_CType(f) == kHpb_CType_Message) {
      const hpb_MiniTable* sub = sub_tables[msg_count++];
      if (msg_count > sub_table_count) return false;
      if (sub != NULL) {
        if (!hpb_MiniTable_SetSubMessage(mt, f, sub)) return false;
      }
    }
  }

  for (int i = 0; i < mt->field_count; i++) {
    hpb_MiniTableField* f = (hpb_MiniTableField*)&mt->fields[i];
    if (hpb_MiniTableField_IsClosedEnum(f)) {
      const hpb_MiniTableEnum* sub = sub_enums[enum_count++];
      if (enum_count > sub_enum_count) return false;
      if (sub != NULL) {
        if (!hpb_MiniTable_SetSubEnum(mt, f, sub)) return false;
      }
    }
  }

  return true;
}
