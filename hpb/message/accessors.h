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

#ifndef HPB_MESSAGE_ACCESSORS_H_
#define HPB_MESSAGE_ACCESSORS_H_

#include "hpb/base/descriptor_constants.h"
#include "hpb/collections/array.h"
#include "hpb/collections/internal/array.h"
#include "hpb/collections/internal/map.h"
#include "hpb/collections/map.h"
#include "hpb/message/internal/accessors.h"
#include "hpb/message/internal/message.h"
#include "hpb/mini_table/enum.h"
#include "hpb/mini_table/field.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

HPB_API_INLINE void hpb_Message_ClearField(hpb_Message* msg,
                                           const hpb_MiniTableField* field) {
  if (hpb_MiniTableField_IsExtension(field)) {
    const hpb_MiniTableExtension* ext = (const hpb_MiniTableExtension*)field;
    _hpb_Message_ClearExtensionField(msg, ext);
  } else {
    _hpb_Message_ClearNonExtensionField(msg, field);
  }
}

HPB_API_INLINE void hpb_Message_Clear(hpb_Message* msg,
                                      const hpb_MiniTable* l) {
  // Note: Can't use HPB_PTR_AT() here because we are doing pointer subtraction.
  char* mem = (char*)msg - sizeof(hpb_Message_Internal);
  memset(mem, 0, hpb_msg_sizeof(l));
}

HPB_API_INLINE bool hpb_Message_HasField(const hpb_Message* msg,
                                         const hpb_MiniTableField* field) {
  if (hpb_MiniTableField_IsExtension(field)) {
    const hpb_MiniTableExtension* ext = (const hpb_MiniTableExtension*)field;
    return _hpb_Message_HasExtensionField(msg, ext);
  } else {
    return _hpb_Message_HasNonExtensionField(msg, field);
  }
}

HPB_API_INLINE uint32_t hpb_Message_WhichOneofFieldNumber(
    const hpb_Message* message, const hpb_MiniTableField* oneof_field) {
  HPB_ASSUME(_hpb_MiniTableField_InOneOf(oneof_field));
  return _hpb_getoneofcase_field(message, oneof_field);
}

HPB_API_INLINE bool hpb_Message_GetBool(const hpb_Message* msg,
                                        const hpb_MiniTableField* field,
                                        bool default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Bool);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_1Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  bool ret;
  _hpb_Message_GetField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetBool(hpb_Message* msg,
                                        const hpb_MiniTableField* field,
                                        bool value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Bool);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_1Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE int32_t hpb_Message_GetInt32(const hpb_Message* msg,
                                            const hpb_MiniTableField* field,
                                            int32_t default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Int32 ||
             hpb_MiniTableField_CType(field) == kHpb_CType_Enum);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_4Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  int32_t ret;
  _hpb_Message_GetField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetInt32(hpb_Message* msg,
                                         const hpb_MiniTableField* field,
                                         int32_t value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Int32 ||
             hpb_MiniTableField_CType(field) == kHpb_CType_Enum);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_4Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE uint32_t hpb_Message_GetUInt32(const hpb_Message* msg,
                                              const hpb_MiniTableField* field,
                                              uint32_t default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_UInt32);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_4Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  uint32_t ret;
  _hpb_Message_GetField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetUInt32(hpb_Message* msg,
                                          const hpb_MiniTableField* field,
                                          uint32_t value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_UInt32);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_4Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE void hpb_Message_SetClosedEnum(
    hpb_Message* msg, const hpb_MiniTable* msg_mini_table,
    const hpb_MiniTableField* field, int32_t value) {
  HPB_ASSERT(hpb_MiniTableField_IsClosedEnum(field));
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_4Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  HPB_ASSERT(hpb_MiniTableEnum_CheckValue(
      hpb_MiniTable_GetSubEnumTable(msg_mini_table, field), value));
  _hpb_Message_SetNonExtensionField(msg, field, &value);
}

HPB_API_INLINE int64_t hpb_Message_GetInt64(const hpb_Message* msg,
                                            const hpb_MiniTableField* field,
                                            uint64_t default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Int64);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_8Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  int64_t ret;
  _hpb_Message_GetField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetInt64(hpb_Message* msg,
                                         const hpb_MiniTableField* field,
                                         int64_t value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Int64);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_8Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE uint64_t hpb_Message_GetUInt64(const hpb_Message* msg,
                                              const hpb_MiniTableField* field,
                                              uint64_t default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_UInt64);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_8Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  uint64_t ret;
  _hpb_Message_GetField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetUInt64(hpb_Message* msg,
                                          const hpb_MiniTableField* field,
                                          uint64_t value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_UInt64);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_8Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE float hpb_Message_GetFloat(const hpb_Message* msg,
                                          const hpb_MiniTableField* field,
                                          float default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Float);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_4Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  float ret;
  _hpb_Message_GetField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetFloat(hpb_Message* msg,
                                         const hpb_MiniTableField* field,
                                         float value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Float);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_4Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE double hpb_Message_GetDouble(const hpb_Message* msg,
                                            const hpb_MiniTableField* field,
                                            double default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Double);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_8Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  double ret;
  _hpb_Message_GetField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetDouble(hpb_Message* msg,
                                          const hpb_MiniTableField* field,
                                          double value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Double);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_8Byte);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE hpb_StringView
hpb_Message_GetString(const hpb_Message* msg, const hpb_MiniTableField* field,
                      hpb_StringView def_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_String ||
             hpb_MiniTableField_CType(field) == kHpb_CType_Bytes);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_StringView);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  hpb_StringView ret;
  _hpb_Message_GetField(msg, field, &def_val, &ret);
  return ret;
}

HPB_API_INLINE bool hpb_Message_SetString(hpb_Message* msg,
                                          const hpb_MiniTableField* field,
                                          hpb_StringView value, hpb_Arena* a) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_String ||
             hpb_MiniTableField_CType(field) == kHpb_CType_Bytes);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_StringView);
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  return _hpb_Message_SetField(msg, field, &value, a);
}

HPB_API_INLINE hpb_TaggedMessagePtr hpb_Message_GetTaggedMessagePtr(
    const hpb_Message* msg, const hpb_MiniTableField* field,
    hpb_Message* default_val) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Message);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) ==
             HPB_SIZE(kHpb_FieldRep_4Byte, kHpb_FieldRep_8Byte));
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  hpb_TaggedMessagePtr tagged;
  _hpb_Message_GetNonExtensionField(msg, field, &default_val, &tagged);
  return tagged;
}

HPB_API_INLINE const hpb_Message* hpb_Message_GetMessage(
    const hpb_Message* msg, const hpb_MiniTableField* field,
    hpb_Message* default_val) {
  hpb_TaggedMessagePtr tagged =
      hpb_Message_GetTaggedMessagePtr(msg, field, default_val);
  return hpb_TaggedMessagePtr_GetNonEmptyMessage(tagged);
}

// For internal use only; users cannot set tagged messages because only the
// parser and the message copier are allowed to directly create an empty
// message.
HPB_API_INLINE void _hpb_Message_SetTaggedMessagePtr(
    hpb_Message* msg, const hpb_MiniTable* mini_table,
    const hpb_MiniTableField* field, hpb_TaggedMessagePtr sub_message) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Message);
  HPB_ASSUME(_hpb_MiniTableField_GetRep(field) ==
             HPB_SIZE(kHpb_FieldRep_4Byte, kHpb_FieldRep_8Byte));
  HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
  HPB_ASSERT(mini_table->subs[field->HPB_PRIVATE(submsg_index)].submsg);
  _hpb_Message_SetNonExtensionField(msg, field, &sub_message);
}

HPB_API_INLINE void hpb_Message_SetMessage(hpb_Message* msg,
                                           const hpb_MiniTable* mini_table,
                                           const hpb_MiniTableField* field,
                                           hpb_Message* sub_message) {
  _hpb_Message_SetTaggedMessagePtr(
      msg, mini_table, field, _hpb_TaggedMessagePtr_Pack(sub_message, false));
}

HPB_API_INLINE hpb_Message* hpb_Message_GetOrCreateMutableMessage(
    hpb_Message* msg, const hpb_MiniTable* mini_table,
    const hpb_MiniTableField* field, hpb_Arena* arena) {
  HPB_ASSERT(arena);
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Message);
  hpb_Message* sub_message = *HPB_PTR_AT(msg, field->offset, hpb_Message*);
  if (!sub_message) {
    const hpb_MiniTable* sub_mini_table =
        mini_table->subs[field->HPB_PRIVATE(submsg_index)].submsg;
    HPB_ASSERT(sub_mini_table);
    sub_message = _hpb_Message_New(sub_mini_table, arena);
    *HPB_PTR_AT(msg, field->offset, hpb_Message*) = sub_message;
    _hpb_Message_SetPresence(msg, field);
  }
  return sub_message;
}

HPB_API_INLINE const hpb_Array* hpb_Message_GetArray(
    const hpb_Message* msg, const hpb_MiniTableField* field) {
  _hpb_MiniTableField_CheckIsArray(field);
  hpb_Array* ret;
  const hpb_Array* default_val = NULL;
  _hpb_Message_GetNonExtensionField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE hpb_Array* hpb_Message_GetMutableArray(
    hpb_Message* msg, const hpb_MiniTableField* field) {
  _hpb_MiniTableField_CheckIsArray(field);
  return (hpb_Array*)hpb_Message_GetArray(msg, field);
}

HPB_API_INLINE hpb_Array* hpb_Message_GetOrCreateMutableArray(
    hpb_Message* msg, const hpb_MiniTableField* field, hpb_Arena* arena) {
  HPB_ASSERT(arena);
  _hpb_MiniTableField_CheckIsArray(field);
  hpb_Array* array = hpb_Message_GetMutableArray(msg, field);
  if (!array) {
    array = _hpb_Array_New(arena, 4, _hpb_MiniTable_ElementSizeLg2(field));
    // Check again due to: https://godbolt.org/z/7WfaoKG1r
    _hpb_MiniTableField_CheckIsArray(field);
    _hpb_Message_SetField(msg, field, &array, arena);
  }
  return array;
}

HPB_API_INLINE void* hpb_Message_ResizeArrayUninitialized(
    hpb_Message* msg, const hpb_MiniTableField* field, size_t size,
    hpb_Arena* arena) {
  _hpb_MiniTableField_CheckIsArray(field);
  hpb_Array* arr = hpb_Message_GetOrCreateMutableArray(msg, field, arena);
  if (!arr || !_hpb_Array_ResizeUninitialized(arr, size, arena)) return NULL;
  return _hpb_array_ptr(arr);
}

HPB_API_INLINE const hpb_Map* hpb_Message_GetMap(
    const hpb_Message* msg, const hpb_MiniTableField* field) {
  _hpb_MiniTableField_CheckIsMap(field);
  _hpb_Message_AssertMapIsUntagged(msg, field);
  hpb_Map* ret;
  const hpb_Map* default_val = NULL;
  _hpb_Message_GetNonExtensionField(msg, field, &default_val, &ret);
  return ret;
}

HPB_API_INLINE hpb_Map* hpb_Message_GetMutableMap(
    hpb_Message* msg, const hpb_MiniTableField* field) {
  return (hpb_Map*)hpb_Message_GetMap(msg, field);
}

HPB_API_INLINE hpb_Map* hpb_Message_GetOrCreateMutableMap(
    hpb_Message* msg, const hpb_MiniTable* map_entry_mini_table,
    const hpb_MiniTableField* field, hpb_Arena* arena) {
  HPB_ASSUME(hpb_MiniTableField_CType(field) == kHpb_CType_Message);
  const hpb_MiniTableField* map_entry_key_field =
      &map_entry_mini_table->fields[0];
  const hpb_MiniTableField* map_entry_value_field =
      &map_entry_mini_table->fields[1];
  return _hpb_Message_GetOrCreateMutableMap(
      msg, field,
      _hpb_Map_CTypeSize(hpb_MiniTableField_CType(map_entry_key_field)),
      _hpb_Map_CTypeSize(hpb_MiniTableField_CType(map_entry_value_field)),
      arena);
}

// Updates a map entry given an entry message.
hpb_MapInsertStatus hpb_Message_InsertMapEntry(hpb_Map* map,
                                               const hpb_MiniTable* mini_table,
                                               const hpb_MiniTableField* field,
                                               hpb_Message* map_entry_message,
                                               hpb_Arena* arena);

// Compares two messages by serializing them and calling memcmp().
bool hpb_Message_IsExactlyEqual(const hpb_Message* m1, const hpb_Message* m2,
                                const hpb_MiniTable* layout);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MESSAGE_ACCESSORS_H_
