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

#include "hpb/message/copy.h"

#include <stdbool.h>
#include <string.h>

#include "hpb/base/descriptor_constants.h"
#include "hpb/base/string_view.h"
#include "hpb/mem/arena.h"
#include "hpb/message/accessors.h"
#include "hpb/message/internal/message.h"
#include "hpb/message/message.h"
#include "hpb/mini_table/field.h"
#include "hpb/mini_table/internal/field.h"

// Must be last.
#include "hpb/port/def.inc"

static bool hpb_MessageField_IsMap(const hpb_MiniTableField* field) {
  return hpb_FieldMode_Get(field) == kHpb_FieldMode_Map;
}

static hpb_StringView hpb_Clone_StringView(hpb_StringView str,
                                           hpb_Arena* arena) {
  if (str.size == 0) {
    return hpb_StringView_FromDataAndSize(NULL, 0);
  }
  void* cloned_data = hpb_Arena_Malloc(arena, str.size);
  hpb_StringView cloned_str =
      hpb_StringView_FromDataAndSize(cloned_data, str.size);
  memcpy(cloned_data, str.data, str.size);
  return cloned_str;
}

static bool hpb_Clone_MessageValue(void* value, hpb_CType value_type,
                                   const hpb_MiniTable* sub, hpb_Arena* arena) {
  switch (value_type) {
    case kHpb_CType_Bool:
    case kHpb_CType_Float:
    case kHpb_CType_Int32:
    case kHpb_CType_UInt32:
    case kHpb_CType_Enum:
    case kHpb_CType_Double:
    case kHpb_CType_Int64:
    case kHpb_CType_UInt64:
      return true;
    case kHpb_CType_String:
    case kHpb_CType_Bytes: {
      hpb_StringView source = *(hpb_StringView*)value;
      int size = source.size;
      void* cloned_data = hpb_Arena_Malloc(arena, size);
      if (cloned_data == NULL) {
        return false;
      }
      *(hpb_StringView*)value =
          hpb_StringView_FromDataAndSize(cloned_data, size);
      memcpy(cloned_data, source.data, size);
      return true;
    } break;
    case kHpb_CType_Message: {
      const hpb_TaggedMessagePtr source = *(hpb_TaggedMessagePtr*)value;
      bool is_empty = hpb_TaggedMessagePtr_IsEmpty(source);
      if (is_empty) sub = &_kHpb_MiniTable_Empty;
      HPB_ASSERT(source);
      hpb_Message* clone = hpb_Message_DeepClone(
          _hpb_TaggedMessagePtr_GetMessage(source), sub, arena);
      *(hpb_TaggedMessagePtr*)value =
          _hpb_TaggedMessagePtr_Pack(clone, is_empty);
      return clone != NULL;
    } break;
  }
  HPB_UNREACHABLE();
}

hpb_Map* hpb_Map_DeepClone(const hpb_Map* map, hpb_CType key_type,
                           hpb_CType value_type,
                           const hpb_MiniTable* map_entry_table,
                           hpb_Arena* arena) {
  hpb_Map* cloned_map = _hpb_Map_New(arena, map->key_size, map->val_size);
  if (cloned_map == NULL) {
    return NULL;
  }
  hpb_MessageValue key, val;
  size_t iter = kHpb_Map_Begin;
  while (hpb_Map_Next(map, &key, &val, &iter)) {
    const hpb_MiniTableField* value_field = &map_entry_table->fields[1];
    const hpb_MiniTable* value_sub =
        (value_field->HPB_PRIVATE(submsg_index) != kHpb_NoSub)
            ? hpb_MiniTable_GetSubMessageTable(map_entry_table, value_field)
            : NULL;
    hpb_CType value_field_type = hpb_MiniTableField_CType(value_field);
    if (!hpb_Clone_MessageValue(&val, value_field_type, value_sub, arena)) {
      return NULL;
    }
    if (hpb_Map_Insert(cloned_map, key, val, arena) ==
        kHpb_MapInsertStatus_OutOfMemory) {
      return NULL;
    }
  }
  return cloned_map;
}

static hpb_Map* hpb_Message_Map_DeepClone(const hpb_Map* map,
                                          const hpb_MiniTable* mini_table,
                                          const hpb_MiniTableField* field,
                                          hpb_Message* clone,
                                          hpb_Arena* arena) {
  const hpb_MiniTable* map_entry_table =
      mini_table->subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(map_entry_table);

  const hpb_MiniTableField* key_field = &map_entry_table->fields[0];
  const hpb_MiniTableField* value_field = &map_entry_table->fields[1];

  hpb_Map* cloned_map = hpb_Map_DeepClone(
      map, hpb_MiniTableField_CType(key_field),
      hpb_MiniTableField_CType(value_field), map_entry_table, arena);
  if (!cloned_map) {
    return NULL;
  }
  _hpb_Message_SetNonExtensionField(clone, field, &cloned_map);
  return cloned_map;
}

hpb_Array* hpb_Array_DeepClone(const hpb_Array* array, hpb_CType value_type,
                               const hpb_MiniTable* sub, hpb_Arena* arena) {
  size_t size = array->size;
  hpb_Array* cloned_array =
      _hpb_Array_New(arena, size, _hpb_Array_CTypeSizeLg2(value_type));
  if (!cloned_array) {
    return NULL;
  }
  if (!_hpb_Array_ResizeUninitialized(cloned_array, size, arena)) {
    return NULL;
  }
  for (size_t i = 0; i < size; ++i) {
    hpb_MessageValue val = hpb_Array_Get(array, i);
    if (!hpb_Clone_MessageValue(&val, value_type, sub, arena)) {
      return false;
    }
    hpb_Array_Set(cloned_array, i, val);
  }
  return cloned_array;
}

static bool hpb_Message_Array_DeepClone(const hpb_Array* array,
                                        const hpb_MiniTable* mini_table,
                                        const hpb_MiniTableField* field,
                                        hpb_Message* clone, hpb_Arena* arena) {
  _hpb_MiniTableField_CheckIsArray(field);
  hpb_Array* cloned_array = hpb_Array_DeepClone(
      array, hpb_MiniTableField_CType(field),
      hpb_MiniTableField_CType(field) == kHpb_CType_Message &&
              field->HPB_PRIVATE(submsg_index) != kHpb_NoSub
          ? hpb_MiniTable_GetSubMessageTable(mini_table, field)
          : NULL,
      arena);

  // Clear out hpb_Array* due to parent memcpy.
  _hpb_Message_SetNonExtensionField(clone, field, &cloned_array);
  return true;
}

static bool hpb_Clone_ExtensionValue(
    const hpb_MiniTableExtension* mini_table_ext,
    const hpb_Message_Extension* source, hpb_Message_Extension* dest,
    hpb_Arena* arena) {
  dest->data = source->data;
  return hpb_Clone_MessageValue(
      &dest->data, hpb_MiniTableField_CType(&mini_table_ext->field),
      mini_table_ext->sub.submsg, arena);
}

hpb_Message* _hpb_Message_Copy(hpb_Message* dst, const hpb_Message* src,
                               const hpb_MiniTable* mini_table,
                               hpb_Arena* arena) {
  hpb_StringView empty_string = hpb_StringView_FromDataAndSize(NULL, 0);
  // Only copy message area skipping hpb_Message_Internal.
  memcpy(dst, src, mini_table->size);
  for (size_t i = 0; i < mini_table->field_count; ++i) {
    const hpb_MiniTableField* field = &mini_table->fields[i];
    if (!hpb_IsRepeatedOrMap(field)) {
      switch (hpb_MiniTableField_CType(field)) {
        case kHpb_CType_Message: {
          hpb_TaggedMessagePtr tagged =
              hpb_Message_GetTaggedMessagePtr(src, field, NULL);
          const hpb_Message* sub_message =
              _hpb_TaggedMessagePtr_GetMessage(tagged);
          if (sub_message != NULL) {
            // If the message is currently in an unlinked, "empty" state we keep
            // it that way, because we don't want to deal with decode options,
            // decode status, or possible parse failure here.
            bool is_empty = hpb_TaggedMessagePtr_IsEmpty(tagged);
            const hpb_MiniTable* sub_message_table =
                is_empty ? &_kHpb_MiniTable_Empty
                         : hpb_MiniTable_GetSubMessageTable(mini_table, field);
            hpb_Message* dst_sub_message =
                hpb_Message_DeepClone(sub_message, sub_message_table, arena);
            if (dst_sub_message == NULL) {
              return NULL;
            }
            _hpb_Message_SetTaggedMessagePtr(
                dst, mini_table, field,
                _hpb_TaggedMessagePtr_Pack(dst_sub_message, is_empty));
          }
        } break;
        case kHpb_CType_String:
        case kHpb_CType_Bytes: {
          hpb_StringView str = hpb_Message_GetString(src, field, empty_string);
          if (str.size != 0) {
            if (!hpb_Message_SetString(
                    dst, field, hpb_Clone_StringView(str, arena), arena)) {
              return NULL;
            }
          }
        } break;
        default:
          // Scalar, already copied.
          break;
      }
    } else {
      if (hpb_MessageField_IsMap(field)) {
        const hpb_Map* map = hpb_Message_GetMap(src, field);
        if (map != NULL) {
          if (!hpb_Message_Map_DeepClone(map, mini_table, field, dst, arena)) {
            return NULL;
          }
        }
      } else {
        const hpb_Array* array = hpb_Message_GetArray(src, field);
        if (array != NULL) {
          if (!hpb_Message_Array_DeepClone(array, mini_table, field, dst,
                                           arena)) {
            return NULL;
          }
        }
      }
    }
  }
  // Clone extensions.
  size_t ext_count;
  const hpb_Message_Extension* ext = _hpb_Message_Getexts(src, &ext_count);
  for (size_t i = 0; i < ext_count; ++i) {
    const hpb_Message_Extension* msg_ext = &ext[i];
    const hpb_MiniTableField* field = &msg_ext->ext->field;
    hpb_Message_Extension* dst_ext =
        _hpb_Message_GetOrCreateExtension(dst, msg_ext->ext, arena);
    if (!dst_ext) return NULL;
    if (!hpb_IsRepeatedOrMap(field)) {
      if (!hpb_Clone_ExtensionValue(msg_ext->ext, msg_ext, dst_ext, arena)) {
        return NULL;
      }
    } else {
      hpb_Array* msg_array = (hpb_Array*)msg_ext->data.ptr;
      HPB_ASSERT(msg_array);
      hpb_Array* cloned_array =
          hpb_Array_DeepClone(msg_array, hpb_MiniTableField_CType(field),
                              msg_ext->ext->sub.submsg, arena);
      if (!cloned_array) {
        return NULL;
      }
      dst_ext->data.ptr = (void*)cloned_array;
    }
  }

  // Clone unknowns.
  size_t unknown_size = 0;
  const char* ptr = hpb_Message_GetUnknown(src, &unknown_size);
  if (unknown_size != 0) {
    HPB_ASSERT(ptr);
    // Make a copy into destination arena.
    if (!_hpb_Message_AddUnknown(dst, ptr, unknown_size, arena)) {
      return NULL;
    }
  }
  return dst;
}

bool hpb_Message_DeepCopy(hpb_Message* dst, const hpb_Message* src,
                          const hpb_MiniTable* mini_table, hpb_Arena* arena) {
  hpb_Message_Clear(dst, mini_table);
  return _hpb_Message_Copy(dst, src, mini_table, arena) != NULL;
}

// Deep clones a message using the provided target arena.
//
// Returns NULL on failure.
hpb_Message* hpb_Message_DeepClone(const hpb_Message* message,
                                   const hpb_MiniTable* mini_table,
                                   hpb_Arena* arena) {
  hpb_Message* clone = hpb_Message_New(mini_table, arena);
  return _hpb_Message_Copy(clone, message, mini_table, arena);
}
