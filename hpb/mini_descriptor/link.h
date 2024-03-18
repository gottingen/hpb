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

// Functions for linking MiniTables together once they are built from a
// MiniDescriptor.
//
// These functions have names like hpb_MiniTable_Link() because they operate on
// MiniTables.  We put them here, rather than in the mini_table/ directory,
// because they are only needed when building MiniTables from MiniDescriptors.
// The interfaces in mini_table/ assume that MiniTables are immutable.

#ifndef HPB_MINI_DESCRIPTOR_LINK_H_
#define HPB_MINI_DESCRIPTOR_LINK_H_

#include "hpb/base/status.h"
#include "hpb/mem/arena.h"
#include "hpb/mini_table/extension.h"
#include "hpb/mini_table/field.h"
#include "hpb/mini_table/message.h"
#include "hpb/mini_table/sub.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Links a sub-message field to a MiniTable for that sub-message. If a
// sub-message field is not linked, it will be treated as an unknown field
// during parsing, and setting the field will not be allowed. It is possible
// to link the message field later, at which point it will no longer be treated
// as unknown. However there is no synchronization for this operation, which
// means parallel mutation requires external synchronization.
// Returns success/failure.
HPB_API bool hpb_MiniTable_SetSubMessage(hpb_MiniTable* table,
                                         hpb_MiniTableField* field,
                                         const hpb_MiniTable* sub);

// Links an enum field to a MiniTable for that enum.
// All enum fields must be linked prior to parsing.
// Returns success/failure.
HPB_API bool hpb_MiniTable_SetSubEnum(hpb_MiniTable* table,
                                      hpb_MiniTableField* field,
                                      const hpb_MiniTableEnum* sub);

// Returns a list of fields that require linking at runtime, to connect the
// MiniTable to its sub-messages and sub-enums.  The list of fields will be
// written to the `subs` array, which must have been allocated by the caller
// and must be large enough to hold a list of all fields in the message.
//
// The order of the fields returned by this function is significant: it matches
// the order expected by hpb_MiniTable_Link() below.
//
// The return value packs the sub-message count and sub-enum count into a single
// integer like so:
//  return (msg_count << 16) | enum_count;
HPB_API uint32_t hpb_MiniTable_GetSubList(const hpb_MiniTable* mt,
                                          const hpb_MiniTableField** subs);

// Links a message to its sub-messages and sub-enums.  The caller must pass
// arrays of sub-tables and sub-enums, in the same length and order as is
// returned by hpb_MiniTable_GetSubList() above.  However, individual elements
// of the sub_tables may be NULL if those sub-messages were tree shaken.
//
// Returns false if either array is too short, or if any of the tables fails
// to link.
HPB_API bool hpb_MiniTable_Link(hpb_MiniTable* mt,
                                const hpb_MiniTable** sub_tables,
                                size_t sub_table_count,
                                const hpb_MiniTableEnum** sub_enums,
                                size_t sub_enum_count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_DESCRIPTOR_LINK_H_
