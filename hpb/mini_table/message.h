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

#ifndef HPB_MINI_TABLE_MESSAGE_H_
#define HPB_MINI_TABLE_MESSAGE_H_

#include "hpb/mini_table/enum.h"
#include "hpb/mini_table/field.h"
#include "hpb/mini_table/internal/message.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hpb_MiniTable hpb_MiniTable;

HPB_API const hpb_MiniTableField* hpb_MiniTable_FindFieldByNumber(
    const hpb_MiniTable* table, uint32_t number);

HPB_API_INLINE const hpb_MiniTableField* hpb_MiniTable_GetFieldByIndex(
    const hpb_MiniTable* t, uint32_t index) {
  return &t->fields[index];
}

// Returns the MiniTable for this message field.  If the field is unlinked,
// returns NULL.
HPB_API_INLINE const hpb_MiniTable* hpb_MiniTable_GetSubMessageTable(
    const hpb_MiniTable* mini_table, const hpb_MiniTableField* field) {
  HPB_ASSERT(hpb_MiniTableField_CType(field) == kHpb_CType_Message);
  const hpb_MiniTable* ret =
      mini_table->subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSUME(ret);
  return ret == &_kHpb_MiniTable_Empty ? NULL : ret;
}

// Returns the MiniTableEnum for this enum field.  If the field is unlinked,
// returns NULL.
HPB_API_INLINE const hpb_MiniTableEnum* hpb_MiniTable_GetSubEnumTable(
    const hpb_MiniTable* mini_table, const hpb_MiniTableField* field) {
  HPB_ASSERT(hpb_MiniTableField_CType(field) == kHpb_CType_Enum);
  return mini_table->subs[field->HPB_PRIVATE(submsg_index)].subenum;
}

// Returns true if this MiniTable field is linked to a MiniTable for the
// sub-message.
HPB_API_INLINE bool hpb_MiniTable_MessageFieldIsLinked(
    const hpb_MiniTable* mini_table, const hpb_MiniTableField* field) {
  return hpb_MiniTable_GetSubMessageTable(mini_table, field) != NULL;
}

// If this field is in a oneof, returns the first field in the oneof.
//
// Otherwise returns NULL.
//
// Usage:
//   const hpb_MiniTableField* field = hpb_MiniTable_GetOneof(m, f);
//   do {
//       ..
//   } while (hpb_MiniTable_NextOneofField(m, &field);
//
const hpb_MiniTableField* hpb_MiniTable_GetOneof(const hpb_MiniTable* m,
                                                 const hpb_MiniTableField* f);

// Iterates to the next field in the oneof. If this is the last field in the
// oneof, returns false. The ordering of fields in the oneof is not
// guaranteed.
// REQUIRES: |f| is the field initialized by hpb_MiniTable_GetOneof and updated
//           by prior hpb_MiniTable_NextOneofField calls.
bool hpb_MiniTable_NextOneofField(const hpb_MiniTable* m,
                                  const hpb_MiniTableField** f);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_TABLE_MESSAGE_H_
