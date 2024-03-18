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

#include "hpb/message/promote.h"

#include "hpb/collections/array.h"
#include "hpb/collections/internal/array.h"
#include "hpb/collections/map.h"
#include "hpb/message/accessors.h"
#include "hpb/message/message.h"
#include "hpb/mini_table/field.h"
#include "hpb/wire/decode.h"
#include "hpb/wire/encode.h"
#include "hpb/wire/eps_copy_input_stream.h"
#include "hpb/wire/reader.h"

// Must be last.
#include "hpb/port/def.inc"

// Parses unknown data by merging into existing base_message or creating a
// new message usingg mini_table.
static hpb_UnknownToMessageRet hpb_MiniTable_ParseUnknownMessage(
    const char* unknown_data, size_t unknown_size,
    const hpb_MiniTable* mini_table, hpb_Message* base_message,
    int decode_options, hpb_Arena* arena) {
  hpb_UnknownToMessageRet ret;
  ret.message =
      base_message ? base_message : _hpb_Message_New(mini_table, arena);
  if (!ret.message) {
    ret.status = kHpb_UnknownToMessage_OutOfMemory;
    return ret;
  }
  // Decode sub message using unknown field contents.
  const char* data = unknown_data;
  uint32_t tag;
  uint64_t message_len = 0;
  data = hpb_WireReader_ReadTag(data, &tag);
  data = hpb_WireReader_ReadVarint(data, &message_len);
  hpb_DecodeStatus status = hpb_Decode(data, message_len, ret.message,
                                       mini_table, NULL, decode_options, arena);
  if (status == kHpb_DecodeStatus_OutOfMemory) {
    ret.status = kHpb_UnknownToMessage_OutOfMemory;
  } else if (status == kHpb_DecodeStatus_Ok) {
    ret.status = kHpb_UnknownToMessage_Ok;
  } else {
    ret.status = kHpb_UnknownToMessage_ParseError;
  }
  return ret;
}

hpb_GetExtension_Status hpb_MiniTable_GetOrPromoteExtension(
    hpb_Message* msg, const hpb_MiniTableExtension* ext_table,
    int decode_options, hpb_Arena* arena,
    const hpb_Message_Extension** extension) {
  HPB_ASSERT(hpb_MiniTableField_CType(&ext_table->field) == kHpb_CType_Message);
  *extension = _hpb_Message_Getext(msg, ext_table);
  if (*extension) {
    return kHpb_GetExtension_Ok;
  }

  // Check unknown fields, if available promote.
  int field_number = ext_table->field.number;
  hpb_FindUnknownRet result = hpb_MiniTable_FindUnknown(
      msg, field_number, kHpb_WireFormat_DefaultDepthLimit);
  if (result.status != kHpb_FindUnknown_Ok) {
    return kHpb_GetExtension_NotPresent;
  }
  size_t len;
  size_t ofs = result.ptr - hpb_Message_GetUnknown(msg, &len);
  // Decode and promote from unknown.
  const hpb_MiniTable* extension_table = ext_table->sub.submsg;
  hpb_UnknownToMessageRet parse_result = hpb_MiniTable_ParseUnknownMessage(
      result.ptr, result.len, extension_table,
      /* base_message= */ NULL, decode_options, arena);
  switch (parse_result.status) {
    case kHpb_UnknownToMessage_OutOfMemory:
      return kHpb_GetExtension_OutOfMemory;
    case kHpb_UnknownToMessage_ParseError:
      return kHpb_GetExtension_ParseError;
    case kHpb_UnknownToMessage_NotFound:
      return kHpb_GetExtension_NotPresent;
    case kHpb_UnknownToMessage_Ok:
      break;
  }
  hpb_Message* extension_msg = parse_result.message;
  // Add to extensions.
  hpb_Message_Extension* ext =
      _hpb_Message_GetOrCreateExtension(msg, ext_table, arena);
  if (!ext) {
    return kHpb_GetExtension_OutOfMemory;
  }
  memcpy(&ext->data, &extension_msg, sizeof(extension_msg));
  *extension = ext;
  const char* delete_ptr = hpb_Message_GetUnknown(msg, &len) + ofs;
  hpb_Message_DeleteUnknown(msg, delete_ptr, result.len);
  return kHpb_GetExtension_Ok;
}

static hpb_FindUnknownRet hpb_FindUnknownRet_ParseError(void) {
  return (hpb_FindUnknownRet){.status = kHpb_FindUnknown_ParseError};
}

hpb_FindUnknownRet hpb_MiniTable_FindUnknown(const hpb_Message* msg,
                                             uint32_t field_number,
                                             int depth_limit) {
  size_t size;
  hpb_FindUnknownRet ret;

  const char* ptr = hpb_Message_GetUnknown(msg, &size);
  hpb_EpsCopyInputStream stream;
  hpb_EpsCopyInputStream_Init(&stream, &ptr, size, true);

  while (!hpb_EpsCopyInputStream_IsDone(&stream, &ptr)) {
    uint32_t tag;
    const char* unknown_begin = ptr;
    ptr = hpb_WireReader_ReadTag(ptr, &tag);
    if (!ptr) return hpb_FindUnknownRet_ParseError();
    if (field_number == hpb_WireReader_GetFieldNumber(tag)) {
      ret.status = kHpb_FindUnknown_Ok;
      ret.ptr = hpb_EpsCopyInputStream_GetAliasedPtr(&stream, unknown_begin);
      ptr = _hpb_WireReader_SkipValue(ptr, tag, depth_limit, &stream);
      // Because we know that the input is a flat buffer, it is safe to perform
      // pointer arithmetic on aliased pointers.
      ret.len = hpb_EpsCopyInputStream_GetAliasedPtr(&stream, ptr) - ret.ptr;
      return ret;
    }

    ptr = _hpb_WireReader_SkipValue(ptr, tag, depth_limit, &stream);
    if (!ptr) return hpb_FindUnknownRet_ParseError();
  }
  ret.status = kHpb_FindUnknown_NotPresent;
  ret.ptr = NULL;
  ret.len = 0;
  return ret;
}

static hpb_DecodeStatus hpb_Message_PromoteOne(hpb_TaggedMessagePtr* tagged,
                                               const hpb_MiniTable* mini_table,
                                               int decode_options,
                                               hpb_Arena* arena) {
  hpb_Message* empty = _hpb_TaggedMessagePtr_GetEmptyMessage(*tagged);
  size_t unknown_size;
  const char* unknown_data = hpb_Message_GetUnknown(empty, &unknown_size);
  hpb_Message* promoted = hpb_Message_New(mini_table, arena);
  if (!promoted) return kHpb_DecodeStatus_OutOfMemory;
  hpb_DecodeStatus status = hpb_Decode(unknown_data, unknown_size, promoted,
                                       mini_table, NULL, decode_options, arena);
  if (status == kHpb_DecodeStatus_Ok) {
    *tagged = _hpb_TaggedMessagePtr_Pack(promoted, false);
  }
  return status;
}

hpb_DecodeStatus hpb_Message_PromoteMessage(hpb_Message* parent,
                                            const hpb_MiniTable* mini_table,
                                            const hpb_MiniTableField* field,
                                            int decode_options,
                                            hpb_Arena* arena,
                                            hpb_Message** promoted) {
  const hpb_MiniTable* sub_table =
      hpb_MiniTable_GetSubMessageTable(mini_table, field);
  HPB_ASSERT(sub_table);
  hpb_TaggedMessagePtr tagged =
      hpb_Message_GetTaggedMessagePtr(parent, field, NULL);
  hpb_DecodeStatus ret =
      hpb_Message_PromoteOne(&tagged, sub_table, decode_options, arena);
  if (ret == kHpb_DecodeStatus_Ok) {
    *promoted = hpb_TaggedMessagePtr_GetNonEmptyMessage(tagged);
    hpb_Message_SetMessage(parent, mini_table, field, *promoted);
  }
  return ret;
}

hpb_DecodeStatus hpb_Array_PromoteMessages(hpb_Array* arr,
                                           const hpb_MiniTable* mini_table,
                                           int decode_options,
                                           hpb_Arena* arena) {
  void** data = _hpb_array_ptr(arr);
  size_t size = arr->size;
  for (size_t i = 0; i < size; i++) {
    hpb_TaggedMessagePtr tagged;
    memcpy(&tagged, &data[i], sizeof(tagged));
    if (!hpb_TaggedMessagePtr_IsEmpty(tagged)) continue;
    hpb_DecodeStatus status =
        hpb_Message_PromoteOne(&tagged, mini_table, decode_options, arena);
    if (status != kHpb_DecodeStatus_Ok) return status;
    memcpy(&data[i], &tagged, sizeof(tagged));
  }
  return kHpb_DecodeStatus_Ok;
}

hpb_DecodeStatus hpb_Map_PromoteMessages(hpb_Map* map,
                                         const hpb_MiniTable* mini_table,
                                         int decode_options, hpb_Arena* arena) {
  size_t iter = kHpb_Map_Begin;
  hpb_MessageValue key, val;
  while (hpb_Map_Next(map, &key, &val, &iter)) {
    if (!hpb_TaggedMessagePtr_IsEmpty(val.tagged_msg_val)) continue;
    hpb_DecodeStatus status = hpb_Message_PromoteOne(
        &val.tagged_msg_val, mini_table, decode_options, arena);
    if (status != kHpb_DecodeStatus_Ok) return status;
    hpb_Map_SetEntryValue(map, iter, val);
  }
  return kHpb_DecodeStatus_Ok;
}

////////////////////////////////////////////////////////////////////////////////
// OLD promotion functions, will be removed!
////////////////////////////////////////////////////////////////////////////////

// Warning: See TODO(b/267655898)
hpb_UnknownToMessageRet hpb_MiniTable_PromoteUnknownToMessage(
    hpb_Message* msg, const hpb_MiniTable* mini_table,
    const hpb_MiniTableField* field, const hpb_MiniTable* sub_mini_table,
    int decode_options, hpb_Arena* arena) {
  hpb_FindUnknownRet unknown;
  // We need to loop and merge unknowns that have matching tag field->number.
  hpb_Message* message = NULL;
  // Callers should check that message is not set first before calling
  // PromotoUnknownToMessage.
  HPB_ASSERT(hpb_MiniTable_GetSubMessageTable(mini_table, field) ==
             sub_mini_table);
  bool is_oneof = _hpb_MiniTableField_InOneOf(field);
  if (!is_oneof || _hpb_getoneofcase_field(msg, field) == field->number) {
    HPB_ASSERT(hpb_Message_GetMessage(msg, field, NULL) == NULL);
  }
  hpb_UnknownToMessageRet ret;
  ret.status = kHpb_UnknownToMessage_Ok;
  do {
    unknown = hpb_MiniTable_FindUnknown(
        msg, field->number, hpb_DecodeOptions_GetMaxDepth(decode_options));
    switch (unknown.status) {
      case kHpb_FindUnknown_Ok: {
        const char* unknown_data = unknown.ptr;
        size_t unknown_size = unknown.len;
        ret = hpb_MiniTable_ParseUnknownMessage(unknown_data, unknown_size,
                                                sub_mini_table, message,
                                                decode_options, arena);
        if (ret.status == kHpb_UnknownToMessage_Ok) {
          message = ret.message;
          hpb_Message_DeleteUnknown(msg, unknown_data, unknown_size);
        }
      } break;
      case kHpb_FindUnknown_ParseError:
        ret.status = kHpb_UnknownToMessage_ParseError;
        break;
      case kHpb_FindUnknown_NotPresent:
        // If we parsed at least one unknown, we are done.
        ret.status =
            message ? kHpb_UnknownToMessage_Ok : kHpb_UnknownToMessage_NotFound;
        break;
    }
  } while (unknown.status == kHpb_FindUnknown_Ok);
  if (message) {
    if (is_oneof) {
      *_hpb_oneofcase_field(msg, field) = field->number;
    }
    hpb_Message_SetMessage(msg, mini_table, field, message);
    ret.message = message;
  }
  return ret;
}

// Moves repeated messages in unknowns to a hpb_Array.
//
// Since the repeated field is not a scalar type we don't check for
// kHpb_LabelFlags_IsPacked.
// TODO(b/251007554): Optimize. Instead of converting messages one at a time,
// scan all unknown data once and compact.
hpb_UnknownToMessage_Status hpb_MiniTable_PromoteUnknownToMessageArray(
    hpb_Message* msg, const hpb_MiniTableField* field,
    const hpb_MiniTable* mini_table, int decode_options, hpb_Arena* arena) {
  hpb_Array* repeated_messages = hpb_Message_GetMutableArray(msg, field);
  // Find all unknowns with given field number and parse.
  hpb_FindUnknownRet unknown;
  do {
    unknown = hpb_MiniTable_FindUnknown(
        msg, field->number, hpb_DecodeOptions_GetMaxDepth(decode_options));
    if (unknown.status == kHpb_FindUnknown_Ok) {
      hpb_UnknownToMessageRet ret = hpb_MiniTable_ParseUnknownMessage(
          unknown.ptr, unknown.len, mini_table,
          /* base_message= */ NULL, decode_options, arena);
      if (ret.status == kHpb_UnknownToMessage_Ok) {
        hpb_MessageValue value;
        value.msg_val = ret.message;
        // Allocate array on demand before append.
        if (!repeated_messages) {
          hpb_Message_ResizeArrayUninitialized(msg, field, 0, arena);
          repeated_messages = hpb_Message_GetMutableArray(msg, field);
        }
        if (!hpb_Array_Append(repeated_messages, value, arena)) {
          return kHpb_UnknownToMessage_OutOfMemory;
        }
        hpb_Message_DeleteUnknown(msg, unknown.ptr, unknown.len);
      } else {
        return ret.status;
      }
    }
  } while (unknown.status == kHpb_FindUnknown_Ok);
  return kHpb_UnknownToMessage_Ok;
}

// Moves repeated messages in unknowns to a hpb_Map.
hpb_UnknownToMessage_Status hpb_MiniTable_PromoteUnknownToMap(
    hpb_Message* msg, const hpb_MiniTable* mini_table,
    const hpb_MiniTableField* field, int decode_options, hpb_Arena* arena) {
  const hpb_MiniTable* map_entry_mini_table =
      mini_table->subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(map_entry_mini_table);
  HPB_ASSERT(map_entry_mini_table);
  HPB_ASSERT(map_entry_mini_table->field_count == 2);
  HPB_ASSERT(hpb_FieldMode_Get(field) == kHpb_FieldMode_Map);
  // Find all unknowns with given field number and parse.
  hpb_FindUnknownRet unknown;
  while (1) {
    unknown = hpb_MiniTable_FindUnknown(
        msg, field->number, hpb_DecodeOptions_GetMaxDepth(decode_options));
    if (unknown.status != kHpb_FindUnknown_Ok) break;
    hpb_UnknownToMessageRet ret = hpb_MiniTable_ParseUnknownMessage(
        unknown.ptr, unknown.len, map_entry_mini_table,
        /* base_message= */ NULL, decode_options, arena);
    if (ret.status != kHpb_UnknownToMessage_Ok) return ret.status;
    // Allocate map on demand before append.
    hpb_Map* map = hpb_Message_GetOrCreateMutableMap(msg, map_entry_mini_table,
                                                     field, arena);
    hpb_Message* map_entry_message = ret.message;
    hpb_MapInsertStatus insert_status = hpb_Message_InsertMapEntry(
        map, mini_table, field, map_entry_message, arena);
    if (insert_status == kHpb_MapInsertStatus_OutOfMemory) {
      return kHpb_UnknownToMessage_OutOfMemory;
    }
    HPB_ASSUME(insert_status == kHpb_MapInsertStatus_Inserted ||
               insert_status == kHpb_MapInsertStatus_Replaced);
    hpb_Message_DeleteUnknown(msg, unknown.ptr, unknown.len);
  }
  return kHpb_UnknownToMessage_Ok;
}
