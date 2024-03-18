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

#ifndef HPB_MESSAGE_PROMOTE_H_
#define HPB_MESSAGE_PROMOTE_H_

#include "hpb/collections/array.h"
#include "hpb/message/internal/extension.h"
#include "hpb/wire/decode.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  kHpb_GetExtension_Ok,
  kHpb_GetExtension_NotPresent,
  kHpb_GetExtension_ParseError,
  kHpb_GetExtension_OutOfMemory,
} hpb_GetExtension_Status;

typedef enum {
  kHpb_GetExtensionAsBytes_Ok,
  kHpb_GetExtensionAsBytes_NotPresent,
  kHpb_GetExtensionAsBytes_EncodeError,
} hpb_GetExtensionAsBytes_Status;

// Returns a message extension or promotes an unknown field to
// an extension.
//
// TODO(ferhat): Only supports extension fields that are messages,
// expand support to include non-message types.
hpb_GetExtension_Status hpb_MiniTable_GetOrPromoteExtension(
    hpb_Message* msg, const hpb_MiniTableExtension* ext_table,
    int decode_options, hpb_Arena* arena,
    const hpb_Message_Extension** extension);

typedef enum {
  kHpb_FindUnknown_Ok,
  kHpb_FindUnknown_NotPresent,
  kHpb_FindUnknown_ParseError,
} hpb_FindUnknown_Status;

typedef struct {
  hpb_FindUnknown_Status status;
  // Start of unknown field data in message arena.
  const char* ptr;
  // Size of unknown field data.
  size_t len;
} hpb_FindUnknownRet;

// Finds first occurrence of unknown data by tag id in message.
hpb_FindUnknownRet hpb_MiniTable_FindUnknown(const hpb_Message* msg,
                                             uint32_t field_number,
                                             int depth_limit);

typedef enum {
  kHpb_UnknownToMessage_Ok,
  kHpb_UnknownToMessage_ParseError,
  kHpb_UnknownToMessage_OutOfMemory,
  kHpb_UnknownToMessage_NotFound,
} hpb_UnknownToMessage_Status;

typedef struct {
  hpb_UnknownToMessage_Status status;
  hpb_Message* message;
} hpb_UnknownToMessageRet;

// Promotes an "empty" non-repeated message field in `parent` to a message of
// the correct type.
//
// Preconditions:
//
// 1. The message field must currently be in the "empty" state (this must have
//    been previously verified by the caller by calling
//    `hpb_Message_GetTaggedMessagePtr()` and observing that the message is
//    indeed empty).
//
// 2. This `field` must have previously been linked.
//
// If the promotion succeeds, `parent` will have its data for `field` replaced
// by the promoted message, which is also returned in `*promoted`.  If the
// return value indicates an error status, `parent` and `promoted` are
// unchanged.
hpb_DecodeStatus hpb_Message_PromoteMessage(hpb_Message* parent,
                                            const hpb_MiniTable* mini_table,
                                            const hpb_MiniTableField* field,
                                            int decode_options,
                                            hpb_Arena* arena,
                                            hpb_Message** promoted);

// Promotes any "empty" messages in this array to a message of the correct type
// `mini_table`.  This function should only be called for arrays of messages.
//
// If the return value indicates an error status, some but not all elements may
// have been promoted, but the array itself will not be corrupted.
hpb_DecodeStatus hpb_Array_PromoteMessages(hpb_Array* arr,
                                           const hpb_MiniTable* mini_table,
                                           int decode_options,
                                           hpb_Arena* arena);

// Promotes any "empty" entries in this map to a message of the correct type
// `mini_table`.  This function should only be called for maps that have a
// message type as the map value.
//
// If the return value indicates an error status, some but not all elements may
// have been promoted, but the map itself will not be corrupted.
hpb_DecodeStatus hpb_Map_PromoteMessages(hpb_Map* map,
                                         const hpb_MiniTable* mini_table,
                                         int decode_options, hpb_Arena* arena);

////////////////////////////////////////////////////////////////////////////////
// OLD promotion interfaces, will be removed!
////////////////////////////////////////////////////////////////////////////////

// Promotes unknown data inside message to a hpb_Message parsing the unknown.
//
// The unknown data is removed from message after field value is set
// using hpb_Message_SetMessage.
//
// WARNING!: See b/267655898
hpb_UnknownToMessageRet hpb_MiniTable_PromoteUnknownToMessage(
    hpb_Message* msg, const hpb_MiniTable* mini_table,
    const hpb_MiniTableField* field, const hpb_MiniTable* sub_mini_table,
    int decode_options, hpb_Arena* arena);

// Promotes all unknown data that matches field tag id to repeated messages
// in hpb_Array.
//
// The unknown data is removed from message after hpb_Array is populated.
// Since repeated messages can't be packed we remove each unknown that
// contains the target tag id.
hpb_UnknownToMessage_Status hpb_MiniTable_PromoteUnknownToMessageArray(
    hpb_Message* msg, const hpb_MiniTableField* field,
    const hpb_MiniTable* mini_table, int decode_options, hpb_Arena* arena);

// Promotes all unknown data that matches field tag id to hpb_Map.
//
// The unknown data is removed from message after hpb_Map is populated.
// Since repeated messages can't be packed we remove each unknown that
// contains the target tag id.
hpb_UnknownToMessage_Status hpb_MiniTable_PromoteUnknownToMap(
    hpb_Message* msg, const hpb_MiniTable* mini_table,
    const hpb_MiniTableField* field, int decode_options, hpb_Arena* arena);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MESSAGE_PROMOTE_H_
