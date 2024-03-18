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

#ifndef HPB_MESSAGE_INTERNAL_ACCESSORS_H_
#define HPB_MESSAGE_INTERNAL_ACCESSORS_H_

#include "hpb/collections/internal/map.h"
#include "hpb/message/internal/extension.h"
#include "hpb/message/internal/message.h"
#include "hpb/mini_table/internal/field.h"

// Must be last.
#include "hpb/port/def.inc"

#if defined(__GNUC__) && !defined(__clang__)
// GCC raises incorrect warnings in these functions.  It thinks that we are
// overrunning buffers, but we carefully write the functions in this file to
// guarantee that this is impossible.  GCC gets this wrong due it its failure
// to perform constant propagation as we expect:
//   - https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108217
//   - https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108226
//
// Unfortunately this also indicates that GCC is not optimizing away the
// switch() in cases where it should be, compromising the performance.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#if __GNUC__ >= 11
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// LINT.IfChange(presence_logic)

// Hasbit access ///////////////////////////////////////////////////////////////

HPB_INLINE size_t _hpb_hasbit_ofs(size_t idx) { return idx / 8; }

HPB_INLINE char _hpb_hasbit_mask(size_t idx) { return 1 << (idx % 8); }

HPB_INLINE bool _hpb_hasbit(const hpb_Message* msg, size_t idx) {
  return (*HPB_PTR_AT(msg, _hpb_hasbit_ofs(idx), const char) &
          _hpb_hasbit_mask(idx)) != 0;
}

HPB_INLINE void _hpb_sethas(const hpb_Message* msg, size_t idx) {
  (*HPB_PTR_AT(msg, _hpb_hasbit_ofs(idx), char)) |= _hpb_hasbit_mask(idx);
}

HPB_INLINE void _hpb_clearhas(const hpb_Message* msg, size_t idx) {
  (*HPB_PTR_AT(msg, _hpb_hasbit_ofs(idx), char)) &= ~_hpb_hasbit_mask(idx);
}

HPB_INLINE size_t _hpb_Message_Hasidx(const hpb_MiniTableField* f) {
  HPB_ASSERT(f->presence > 0);
  return f->presence;
}

HPB_INLINE bool _hpb_hasbit_field(const hpb_Message* msg,
                                  const hpb_MiniTableField* f) {
  return _hpb_hasbit(msg, _hpb_Message_Hasidx(f));
}

HPB_INLINE void _hpb_sethas_field(const hpb_Message* msg,
                                  const hpb_MiniTableField* f) {
  _hpb_sethas(msg, _hpb_Message_Hasidx(f));
}

// Oneof case access ///////////////////////////////////////////////////////////

HPB_INLINE size_t _hpb_oneofcase_ofs(const hpb_MiniTableField* f) {
  HPB_ASSERT(f->presence < 0);
  return ~(ptrdiff_t)f->presence;
}

HPB_INLINE uint32_t* _hpb_oneofcase_field(hpb_Message* msg,
                                          const hpb_MiniTableField* f) {
  return HPB_PTR_AT(msg, _hpb_oneofcase_ofs(f), uint32_t);
}

HPB_INLINE uint32_t _hpb_getoneofcase_field(const hpb_Message* msg,
                                            const hpb_MiniTableField* f) {
  return *_hpb_oneofcase_field((hpb_Message*)msg, f);
}

// LINT.ThenChange(GoogleInternalName2)

HPB_INLINE bool _hpb_MiniTableField_InOneOf(const hpb_MiniTableField* field) {
  return field->presence < 0;
}

HPB_INLINE void* _hpb_MiniTableField_GetPtr(hpb_Message* msg,
                                            const hpb_MiniTableField* field) {
  return (char*)msg + field->offset;
}

HPB_INLINE const void* _hpb_MiniTableField_GetConstPtr(
    const hpb_Message* msg, const hpb_MiniTableField* field) {
  return (char*)msg + field->offset;
}

HPB_INLINE void _hpb_Message_SetPresence(hpb_Message* msg,
                                         const hpb_MiniTableField* field) {
  if (field->presence > 0) {
    _hpb_sethas_field(msg, field);
  } else if (_hpb_MiniTableField_InOneOf(field)) {
    *_hpb_oneofcase_field(msg, field) = field->number;
  }
}

HPB_INLINE bool _hpb_MiniTable_ValueIsNonZero(const void* default_val,
                                              const hpb_MiniTableField* field) {
  char zero[16] = {0};
  switch (_hpb_MiniTableField_GetRep(field)) {
    case kHpb_FieldRep_1Byte:
      return memcmp(&zero, default_val, 1) != 0;
    case kHpb_FieldRep_4Byte:
      return memcmp(&zero, default_val, 4) != 0;
    case kHpb_FieldRep_8Byte:
      return memcmp(&zero, default_val, 8) != 0;
    case kHpb_FieldRep_StringView: {
      const hpb_StringView* sv = (const hpb_StringView*)default_val;
      return sv->size != 0;
    }
  }
  HPB_UNREACHABLE();
}

HPB_INLINE void _hpb_MiniTable_CopyFieldData(void* to, const void* from,
                                             const hpb_MiniTableField* field) {
  switch (_hpb_MiniTableField_GetRep(field)) {
    case kHpb_FieldRep_1Byte:
      memcpy(to, from, 1);
      return;
    case kHpb_FieldRep_4Byte:
      memcpy(to, from, 4);
      return;
    case kHpb_FieldRep_8Byte:
      memcpy(to, from, 8);
      return;
    case kHpb_FieldRep_StringView: {
      memcpy(to, from, sizeof(hpb_StringView));
      return;
    }
  }
  HPB_UNREACHABLE();
}

HPB_INLINE size_t
_hpb_MiniTable_ElementSizeLg2(const hpb_MiniTableField* field) {
  const unsigned char table[] = {
      0,
      3,               // kHpb_FieldType_Double = 1,
      2,               // kHpb_FieldType_Float = 2,
      3,               // kHpb_FieldType_Int64 = 3,
      3,               // kHpb_FieldType_UInt64 = 4,
      2,               // kHpb_FieldType_Int32 = 5,
      3,               // kHpb_FieldType_Fixed64 = 6,
      2,               // kHpb_FieldType_Fixed32 = 7,
      0,               // kHpb_FieldType_Bool = 8,
      HPB_SIZE(3, 4),  // kHpb_FieldType_String = 9,
      HPB_SIZE(2, 3),  // kHpb_FieldType_Group = 10,
      HPB_SIZE(2, 3),  // kHpb_FieldType_Message = 11,
      HPB_SIZE(3, 4),  // kHpb_FieldType_Bytes = 12,
      2,               // kHpb_FieldType_UInt32 = 13,
      2,               // kHpb_FieldType_Enum = 14,
      2,               // kHpb_FieldType_SFixed32 = 15,
      3,               // kHpb_FieldType_SFixed64 = 16,
      2,               // kHpb_FieldType_SInt32 = 17,
      3,               // kHpb_FieldType_SInt64 = 18,
  };
  return table[field->HPB_PRIVATE(descriptortype)];
}

// Here we define universal getter/setter functions for message fields.
// These look very branchy and inefficient, but as long as the MiniTableField
// values are known at compile time, all the branches are optimized away and
// we are left with ideal code.  This can happen either through through
// literals or HPB_ASSUME():
//
//   // Via struct literals.
//   bool FooMessage_set_bool_field(const hpb_Message* msg, bool val) {
//     const hpb_MiniTableField field = {1, 0, 0, /* etc... */};
//     // All value in "field" are compile-time known.
//     _hpb_Message_SetNonExtensionField(msg, &field, &value);
//   }
//
//   // Via HPB_ASSUME().
//   HPB_INLINE bool hpb_Message_SetBool(hpb_Message* msg,
//                                       const hpb_MiniTableField* field,
//                                       bool value, hpb_Arena* a) {
//     HPB_ASSUME(field->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Bool);
//     HPB_ASSUME(!hpb_IsRepeatedOrMap(field));
//     HPB_ASSUME(_hpb_MiniTableField_GetRep(field) == kHpb_FieldRep_1Byte);
//     _hpb_Message_SetField(msg, field, &value, a);
//   }
//
// As a result, we can use these universal getters/setters for *all* message
// accessors: generated code, MiniTable accessors, and reflection.  The only
// exception is the binary encoder/decoder, which need to be a bit more clever
// about how they read/write the message data, for efficiency.
//
// These functions work on both extensions and non-extensions. If the field
// of a setter is known to be a non-extension, the arena may be NULL and the
// returned bool value may be ignored since it will always succeed.

HPB_INLINE bool _hpb_Message_HasExtensionField(
    const hpb_Message* msg, const hpb_MiniTableExtension* ext) {
  HPB_ASSERT(hpb_MiniTableField_HasPresence(&ext->field));
  return _hpb_Message_Getext(msg, ext) != NULL;
}

HPB_INLINE bool _hpb_Message_HasNonExtensionField(
    const hpb_Message* msg, const hpb_MiniTableField* field) {
  HPB_ASSERT(hpb_MiniTableField_HasPresence(field));
  HPB_ASSUME(!hpb_MiniTableField_IsExtension(field));
  if (_hpb_MiniTableField_InOneOf(field)) {
    return _hpb_getoneofcase_field(msg, field) == field->number;
  } else {
    return _hpb_hasbit_field(msg, field);
  }
}

static HPB_FORCEINLINE void _hpb_Message_GetNonExtensionField(
    const hpb_Message* msg, const hpb_MiniTableField* field,
    const void* default_val, void* val) {
  HPB_ASSUME(!hpb_MiniTableField_IsExtension(field));
  if ((_hpb_MiniTableField_InOneOf(field) ||
       _hpb_MiniTable_ValueIsNonZero(default_val, field)) &&
      !_hpb_Message_HasNonExtensionField(msg, field)) {
    _hpb_MiniTable_CopyFieldData(val, default_val, field);
    return;
  }
  _hpb_MiniTable_CopyFieldData(val, _hpb_MiniTableField_GetConstPtr(msg, field),
                               field);
}

HPB_INLINE void _hpb_Message_GetExtensionField(
    const hpb_Message* msg, const hpb_MiniTableExtension* mt_ext,
    const void* default_val, void* val) {
  HPB_ASSUME(hpb_MiniTableField_IsExtension(&mt_ext->field));
  const hpb_Message_Extension* ext = _hpb_Message_Getext(msg, mt_ext);
  if (ext) {
    _hpb_MiniTable_CopyFieldData(val, &ext->data, &mt_ext->field);
  } else {
    _hpb_MiniTable_CopyFieldData(val, default_val, &mt_ext->field);
  }
}

HPB_INLINE void _hpb_Message_GetField(const hpb_Message* msg,
                                      const hpb_MiniTableField* field,
                                      const void* default_val, void* val) {
  if (hpb_MiniTableField_IsExtension(field)) {
    _hpb_Message_GetExtensionField(msg, (hpb_MiniTableExtension*)field,
                                   default_val, val);
  } else {
    _hpb_Message_GetNonExtensionField(msg, field, default_val, val);
  }
}

HPB_INLINE void _hpb_Message_SetNonExtensionField(
    hpb_Message* msg, const hpb_MiniTableField* field, const void* val) {
  HPB_ASSUME(!hpb_MiniTableField_IsExtension(field));
  _hpb_Message_SetPresence(msg, field);
  _hpb_MiniTable_CopyFieldData(_hpb_MiniTableField_GetPtr(msg, field), val,
                               field);
}

HPB_INLINE bool _hpb_Message_SetExtensionField(
    hpb_Message* msg, const hpb_MiniTableExtension* mt_ext, const void* val,
    hpb_Arena* a) {
  HPB_ASSERT(a);
  hpb_Message_Extension* ext =
      _hpb_Message_GetOrCreateExtension(msg, mt_ext, a);
  if (!ext) return false;
  _hpb_MiniTable_CopyFieldData(&ext->data, val, &mt_ext->field);
  return true;
}

HPB_INLINE bool _hpb_Message_SetField(hpb_Message* msg,
                                      const hpb_MiniTableField* field,
                                      const void* val, hpb_Arena* a) {
  if (hpb_MiniTableField_IsExtension(field)) {
    const hpb_MiniTableExtension* ext = (const hpb_MiniTableExtension*)field;
    return _hpb_Message_SetExtensionField(msg, ext, val, a);
  } else {
    _hpb_Message_SetNonExtensionField(msg, field, val);
    return true;
  }
}

HPB_INLINE void _hpb_Message_ClearExtensionField(
    hpb_Message* msg, const hpb_MiniTableExtension* ext_l) {
  hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  if (!in->internal) return;
  const hpb_Message_Extension* base =
      HPB_PTR_AT(in->internal, in->internal->ext_begin, hpb_Message_Extension);
  hpb_Message_Extension* ext =
      (hpb_Message_Extension*)_hpb_Message_Getext(msg, ext_l);
  if (ext) {
    *ext = *base;
    in->internal->ext_begin += sizeof(hpb_Message_Extension);
  }
}

HPB_INLINE void _hpb_Message_ClearNonExtensionField(
    hpb_Message* msg, const hpb_MiniTableField* field) {
  if (field->presence > 0) {
    _hpb_clearhas(msg, _hpb_Message_Hasidx(field));
  } else if (_hpb_MiniTableField_InOneOf(field)) {
    uint32_t* oneof_case = _hpb_oneofcase_field(msg, field);
    if (*oneof_case != field->number) return;
    *oneof_case = 0;
  }
  const char zeros[16] = {0};
  _hpb_MiniTable_CopyFieldData(_hpb_MiniTableField_GetPtr(msg, field), zeros,
                               field);
}

HPB_INLINE void _hpb_Message_AssertMapIsUntagged(
    const hpb_Message* msg, const hpb_MiniTableField* field) {
  HPB_UNUSED(msg);
  _hpb_MiniTableField_CheckIsMap(field);
#ifndef NDEBUG
  hpb_TaggedMessagePtr default_val = 0;
  hpb_TaggedMessagePtr tagged;
  _hpb_Message_GetNonExtensionField(msg, field, &default_val, &tagged);
  HPB_ASSERT(!hpb_TaggedMessagePtr_IsEmpty(tagged));
#endif
}

HPB_INLINE hpb_Map* _hpb_Message_GetOrCreateMutableMap(
    hpb_Message* msg, const hpb_MiniTableField* field, size_t key_size,
    size_t val_size, hpb_Arena* arena) {
  _hpb_MiniTableField_CheckIsMap(field);
  _hpb_Message_AssertMapIsUntagged(msg, field);
  hpb_Map* map = NULL;
  hpb_Map* default_map_value = NULL;
  _hpb_Message_GetNonExtensionField(msg, field, &default_map_value, &map);
  if (!map) {
    map = _hpb_Map_New(arena, key_size, val_size);
    // Check again due to: https://godbolt.org/z/7WfaoKG1r
    _hpb_MiniTableField_CheckIsMap(field);
    _hpb_Message_SetNonExtensionField(msg, field, &map);
  }
  return map;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MESSAGE_INTERNAL_ACCESSORS_H_
