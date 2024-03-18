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

#include "hpb/mini_descriptor/internal/encode.h"

#include "hpb/base/internal/log2.h"
#include "hpb/mini_descriptor/internal/base92.h"
#include "hpb/mini_descriptor/internal/modifiers.h"
#include "hpb/mini_descriptor/internal/wire_constants.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct {
  uint64_t present_values_mask;
  uint32_t last_written_value;
} hpb_MtDataEncoderInternal_EnumState;

typedef struct {
  uint64_t msg_modifiers;
  uint32_t last_field_num;
  enum {
    kHpb_OneofState_NotStarted,
    kHpb_OneofState_StartedOneof,
    kHpb_OneofState_EmittedOneofField,
  } oneof_state;
} hpb_MtDataEncoderInternal_MsgState;

typedef struct {
  char* buf_start;  // Only for checking kHpb_MtDataEncoder_MinSize.
  union {
    hpb_MtDataEncoderInternal_EnumState enum_state;
    hpb_MtDataEncoderInternal_MsgState msg_state;
  } state;
} hpb_MtDataEncoderInternal;

static hpb_MtDataEncoderInternal* hpb_MtDataEncoder_GetInternal(
    hpb_MtDataEncoder* e, char* buf_start) {
  HPB_ASSERT(sizeof(hpb_MtDataEncoderInternal) <= sizeof(e->internal));
  hpb_MtDataEncoderInternal* ret = (hpb_MtDataEncoderInternal*)e->internal;
  ret->buf_start = buf_start;
  return ret;
}

static char* hpb_MtDataEncoder_PutRaw(hpb_MtDataEncoder* e, char* ptr,
                                      char ch) {
  hpb_MtDataEncoderInternal* in = (hpb_MtDataEncoderInternal*)e->internal;
  HPB_ASSERT(ptr - in->buf_start < kHpb_MtDataEncoder_MinSize);
  if (ptr == e->end) return NULL;
  *ptr++ = ch;
  return ptr;
}

static char* hpb_MtDataEncoder_Put(hpb_MtDataEncoder* e, char* ptr, char ch) {
  return hpb_MtDataEncoder_PutRaw(e, ptr, _hpb_ToBase92(ch));
}

static char* hpb_MtDataEncoder_PutBase92Varint(hpb_MtDataEncoder* e, char* ptr,
                                               uint32_t val, int min, int max) {
  int shift = hpb_Log2Ceiling(_hpb_FromBase92(max) - _hpb_FromBase92(min) + 1);
  HPB_ASSERT(shift <= 6);
  uint32_t mask = (1 << shift) - 1;
  do {
    uint32_t bits = val & mask;
    ptr = hpb_MtDataEncoder_Put(e, ptr, bits + _hpb_FromBase92(min));
    if (!ptr) return NULL;
    val >>= shift;
  } while (val);
  return ptr;
}

char* hpb_MtDataEncoder_PutModifier(hpb_MtDataEncoder* e, char* ptr,
                                    uint64_t mod) {
  if (mod) {
    ptr = hpb_MtDataEncoder_PutBase92Varint(e, ptr, mod,
                                            kHpb_EncodedValue_MinModifier,
                                            kHpb_EncodedValue_MaxModifier);
  }
  return ptr;
}

char* hpb_MtDataEncoder_EncodeExtension(hpb_MtDataEncoder* e, char* ptr,
                                        hpb_FieldType type, uint32_t field_num,
                                        uint64_t field_mod) {
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  in->state.msg_state.msg_modifiers = 0;
  in->state.msg_state.last_field_num = 0;
  in->state.msg_state.oneof_state = kHpb_OneofState_NotStarted;

  ptr = hpb_MtDataEncoder_PutRaw(e, ptr, kHpb_EncodedVersion_ExtensionV1);
  if (!ptr) return NULL;

  return hpb_MtDataEncoder_PutField(e, ptr, type, field_num, field_mod);
}

char* hpb_MtDataEncoder_EncodeMap(hpb_MtDataEncoder* e, char* ptr,
                                  hpb_FieldType key_type,
                                  hpb_FieldType value_type, uint64_t key_mod,
                                  uint64_t value_mod) {
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  in->state.msg_state.msg_modifiers = 0;
  in->state.msg_state.last_field_num = 0;
  in->state.msg_state.oneof_state = kHpb_OneofState_NotStarted;

  ptr = hpb_MtDataEncoder_PutRaw(e, ptr, kHpb_EncodedVersion_MapV1);
  if (!ptr) return NULL;

  ptr = hpb_MtDataEncoder_PutField(e, ptr, key_type, 1, key_mod);
  if (!ptr) return NULL;

  return hpb_MtDataEncoder_PutField(e, ptr, value_type, 2, value_mod);
}

char* hpb_MtDataEncoder_EncodeMessageSet(hpb_MtDataEncoder* e, char* ptr) {
  (void)hpb_MtDataEncoder_GetInternal(e, ptr);
  return hpb_MtDataEncoder_PutRaw(e, ptr, kHpb_EncodedVersion_MessageSetV1);
}

char* hpb_MtDataEncoder_StartMessage(hpb_MtDataEncoder* e, char* ptr,
                                     uint64_t msg_mod) {
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  in->state.msg_state.msg_modifiers = msg_mod;
  in->state.msg_state.last_field_num = 0;
  in->state.msg_state.oneof_state = kHpb_OneofState_NotStarted;

  ptr = hpb_MtDataEncoder_PutRaw(e, ptr, kHpb_EncodedVersion_MessageV1);
  if (!ptr) return NULL;

  return hpb_MtDataEncoder_PutModifier(e, ptr, msg_mod);
}

static char* _hpb_MtDataEncoder_MaybePutFieldSkip(hpb_MtDataEncoder* e,
                                                  char* ptr,
                                                  uint32_t field_num) {
  hpb_MtDataEncoderInternal* in = (hpb_MtDataEncoderInternal*)e->internal;
  if (field_num <= in->state.msg_state.last_field_num) return NULL;
  if (in->state.msg_state.last_field_num + 1 != field_num) {
    // Put skip.
    HPB_ASSERT(field_num > in->state.msg_state.last_field_num);
    uint32_t skip = field_num - in->state.msg_state.last_field_num;
    ptr = hpb_MtDataEncoder_PutBase92Varint(
        e, ptr, skip, kHpb_EncodedValue_MinSkip, kHpb_EncodedValue_MaxSkip);
    if (!ptr) return NULL;
  }
  in->state.msg_state.last_field_num = field_num;
  return ptr;
}

static char* _hpb_MtDataEncoder_PutFieldType(hpb_MtDataEncoder* e, char* ptr,
                                             hpb_FieldType type,
                                             uint64_t field_mod) {
  static const char kHpb_TypeToEncoded[] = {
      [kHpb_FieldType_Double] = kHpb_EncodedType_Double,
      [kHpb_FieldType_Float] = kHpb_EncodedType_Float,
      [kHpb_FieldType_Int64] = kHpb_EncodedType_Int64,
      [kHpb_FieldType_UInt64] = kHpb_EncodedType_UInt64,
      [kHpb_FieldType_Int32] = kHpb_EncodedType_Int32,
      [kHpb_FieldType_Fixed64] = kHpb_EncodedType_Fixed64,
      [kHpb_FieldType_Fixed32] = kHpb_EncodedType_Fixed32,
      [kHpb_FieldType_Bool] = kHpb_EncodedType_Bool,
      [kHpb_FieldType_String] = kHpb_EncodedType_String,
      [kHpb_FieldType_Group] = kHpb_EncodedType_Group,
      [kHpb_FieldType_Message] = kHpb_EncodedType_Message,
      [kHpb_FieldType_Bytes] = kHpb_EncodedType_Bytes,
      [kHpb_FieldType_UInt32] = kHpb_EncodedType_UInt32,
      [kHpb_FieldType_Enum] = kHpb_EncodedType_OpenEnum,
      [kHpb_FieldType_SFixed32] = kHpb_EncodedType_SFixed32,
      [kHpb_FieldType_SFixed64] = kHpb_EncodedType_SFixed64,
      [kHpb_FieldType_SInt32] = kHpb_EncodedType_SInt32,
      [kHpb_FieldType_SInt64] = kHpb_EncodedType_SInt64,
  };

  int encoded_type = kHpb_TypeToEncoded[type];

  if (field_mod & kHpb_FieldModifier_IsClosedEnum) {
    HPB_ASSERT(type == kHpb_FieldType_Enum);
    encoded_type = kHpb_EncodedType_ClosedEnum;
  }

  if (field_mod & kHpb_FieldModifier_IsRepeated) {
    // Repeated fields shift the type number up (unlike other modifiers which
    // are bit flags).
    encoded_type += kHpb_EncodedType_RepeatedBase;
  }

  return hpb_MtDataEncoder_Put(e, ptr, encoded_type);
}

static char* _hpb_MtDataEncoder_MaybePutModifiers(hpb_MtDataEncoder* e,
                                                  char* ptr, hpb_FieldType type,
                                                  uint64_t field_mod) {
  hpb_MtDataEncoderInternal* in = (hpb_MtDataEncoderInternal*)e->internal;
  uint32_t encoded_modifiers = 0;
  if ((field_mod & kHpb_FieldModifier_IsRepeated) &&
      hpb_FieldType_IsPackable(type)) {
    bool field_is_packed = field_mod & kHpb_FieldModifier_IsPacked;
    bool default_is_packed = in->state.msg_state.msg_modifiers &
                             kHpb_MessageModifier_DefaultIsPacked;
    if (field_is_packed != default_is_packed) {
      encoded_modifiers |= kHpb_EncodedFieldModifier_FlipPacked;
    }
  }

  if (field_mod & kHpb_FieldModifier_IsProto3Singular) {
    encoded_modifiers |= kHpb_EncodedFieldModifier_IsProto3Singular;
  }

  if (field_mod & kHpb_FieldModifier_IsRequired) {
    encoded_modifiers |= kHpb_EncodedFieldModifier_IsRequired;
  }

  return hpb_MtDataEncoder_PutModifier(e, ptr, encoded_modifiers);
}

char* hpb_MtDataEncoder_PutField(hpb_MtDataEncoder* e, char* ptr,
                                 hpb_FieldType type, uint32_t field_num,
                                 uint64_t field_mod) {
  hpb_MtDataEncoder_GetInternal(e, ptr);

  ptr = _hpb_MtDataEncoder_MaybePutFieldSkip(e, ptr, field_num);
  if (!ptr) return NULL;

  ptr = _hpb_MtDataEncoder_PutFieldType(e, ptr, type, field_mod);
  if (!ptr) return NULL;

  return _hpb_MtDataEncoder_MaybePutModifiers(e, ptr, type, field_mod);
}

char* hpb_MtDataEncoder_StartOneof(hpb_MtDataEncoder* e, char* ptr) {
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  if (in->state.msg_state.oneof_state == kHpb_OneofState_NotStarted) {
    ptr = hpb_MtDataEncoder_Put(e, ptr, _hpb_FromBase92(kHpb_EncodedValue_End));
  } else {
    ptr = hpb_MtDataEncoder_Put(
        e, ptr, _hpb_FromBase92(kHpb_EncodedValue_OneofSeparator));
  }
  in->state.msg_state.oneof_state = kHpb_OneofState_StartedOneof;
  return ptr;
}

char* hpb_MtDataEncoder_PutOneofField(hpb_MtDataEncoder* e, char* ptr,
                                      uint32_t field_num) {
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  if (in->state.msg_state.oneof_state == kHpb_OneofState_EmittedOneofField) {
    ptr = hpb_MtDataEncoder_Put(
        e, ptr, _hpb_FromBase92(kHpb_EncodedValue_FieldSeparator));
    if (!ptr) return NULL;
  }
  ptr = hpb_MtDataEncoder_PutBase92Varint(e, ptr, field_num, _hpb_ToBase92(0),
                                          _hpb_ToBase92(63));
  in->state.msg_state.oneof_state = kHpb_OneofState_EmittedOneofField;
  return ptr;
}

char* hpb_MtDataEncoder_StartEnum(hpb_MtDataEncoder* e, char* ptr) {
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  in->state.enum_state.present_values_mask = 0;
  in->state.enum_state.last_written_value = 0;

  return hpb_MtDataEncoder_PutRaw(e, ptr, kHpb_EncodedVersion_EnumV1);
}

static char* hpb_MtDataEncoder_FlushDenseEnumMask(hpb_MtDataEncoder* e,
                                                  char* ptr) {
  hpb_MtDataEncoderInternal* in = (hpb_MtDataEncoderInternal*)e->internal;
  ptr = hpb_MtDataEncoder_Put(e, ptr, in->state.enum_state.present_values_mask);
  in->state.enum_state.present_values_mask = 0;
  in->state.enum_state.last_written_value += 5;
  return ptr;
}

char* hpb_MtDataEncoder_PutEnumValue(hpb_MtDataEncoder* e, char* ptr,
                                     uint32_t val) {
  // TODO(b/229641772): optimize this encoding.
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  HPB_ASSERT(val >= in->state.enum_state.last_written_value);
  uint32_t delta = val - in->state.enum_state.last_written_value;
  if (delta >= 5 && in->state.enum_state.present_values_mask) {
    ptr = hpb_MtDataEncoder_FlushDenseEnumMask(e, ptr);
    if (!ptr) {
      return NULL;
    }
    delta -= 5;
  }

  if (delta >= 5) {
    ptr = hpb_MtDataEncoder_PutBase92Varint(
        e, ptr, delta, kHpb_EncodedValue_MinSkip, kHpb_EncodedValue_MaxSkip);
    in->state.enum_state.last_written_value += delta;
    delta = 0;
  }

  HPB_ASSERT((in->state.enum_state.present_values_mask >> delta) == 0);
  in->state.enum_state.present_values_mask |= 1ULL << delta;
  return ptr;
}

char* hpb_MtDataEncoder_EndEnum(hpb_MtDataEncoder* e, char* ptr) {
  hpb_MtDataEncoderInternal* in = hpb_MtDataEncoder_GetInternal(e, ptr);
  if (!in->state.enum_state.present_values_mask) return ptr;
  return hpb_MtDataEncoder_FlushDenseEnumMask(e, ptr);
}
