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

#ifndef HPB_MESSAGE_COPY_H_
#define HPB_MESSAGE_COPY_H_

#include "hpb/collections/message_value.h"
#include "hpb/message/internal/message.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Deep clones a message using the provided target arena.
hpb_Message* hpb_Message_DeepClone(const hpb_Message* message,
                                   const hpb_MiniTable* mini_table,
                                   hpb_Arena* arena);

// Deep clones array contents.
hpb_Array* hpb_Array_DeepClone(const hpb_Array* array, hpb_CType value_type,
                               const hpb_MiniTable* sub, hpb_Arena* arena);

// Deep clones map contents.
hpb_Map* hpb_Map_DeepClone(const hpb_Map* map, hpb_CType key_type,
                           hpb_CType value_type,
                           const hpb_MiniTable* map_entry_table,
                           hpb_Arena* arena);

// Deep copies the message from src to dst.
bool hpb_Message_DeepCopy(hpb_Message* dst, const hpb_Message* src,
                          const hpb_MiniTable* mini_table, hpb_Arena* arena);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MESSAGE_COPY_H_
