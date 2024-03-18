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

#include "hpb/wire/decode.h"

#include <string.h>

#include "hpb/base/descriptor_constants.h"
#include "hpb/collections/internal/array.h"
#include "hpb/collections/internal/map.h"
#include "hpb/mem/internal/arena.h"
#include "hpb/message/internal/accessors.h"
#include "hpb/message/internal/map_entry.h"
#include "hpb/mini_table/sub.h"
#include "hpb/port/atomic.h"
#include "hpb/wire/encode.h"
#include "hpb/wire/eps_copy_input_stream.h"
#include "hpb/wire/internal/common.h"
#include "hpb/wire/internal/decode.h"
#include "hpb/wire/internal/swap.h"
#include "hpb/wire/reader.h"

// Must be last.
#include "hpb/port/def.inc"

// A few fake field types for our tables.
enum {
  kHpb_FakeFieldType_FieldNotFound = 0,
  kHpb_FakeFieldType_MessageSetItem = 19,
};

// DecodeOp: an action to be performed for a wire-type/field-type combination.
enum {
  // Special ops: we don't write data to regular fields for these.
  kHpb_DecodeOp_UnknownField = -1,
  kHpb_DecodeOp_MessageSetItem = -2,

  // Scalar-only ops.
  kHpb_DecodeOp_Scalar1Byte = 0,
  kHpb_DecodeOp_Scalar4Byte = 2,
  kHpb_DecodeOp_Scalar8Byte = 3,
  kHpb_DecodeOp_Enum = 1,

  // Scalar/repeated ops.
  kHpb_DecodeOp_String = 4,
  kHpb_DecodeOp_Bytes = 5,
  kHpb_DecodeOp_SubMessage = 6,

  // Repeated-only ops (also see macros below).
  kHpb_DecodeOp_PackedEnum = 13,
};

// For packed fields it is helpful to be able to recover the lg2 of the data
// size from the op.
#define OP_FIXPCK_LG2(n) (n + 5) /* n in [2, 3] => op in [7, 8] */
#define OP_VARPCK_LG2(n) (n + 9) /* n in [0, 2, 3] => op in [9, 11, 12] */

typedef union {
  bool bool_val;
  uint32_t uint32_val;
  uint64_t uint64_val;
  uint32_t size;
} wireval;

static const char* _hpb_Decoder_DecodeMessage(hpb_Decoder* d, const char* ptr,
                                              hpb_Message* msg,
                                              const hpb_MiniTable* layout);

HPB_NORETURN static void* _hpb_Decoder_ErrorJmp(hpb_Decoder* d,
                                                hpb_DecodeStatus status) {
  assert(status != kHpb_DecodeStatus_Ok);
  d->status = status;
  HPB_LONGJMP(d->err, 1);
}

const char* _hpb_FastDecoder_ErrorJmp(hpb_Decoder* d, int status) {
  assert(status != kHpb_DecodeStatus_Ok);
  d->status = status;
  HPB_LONGJMP(d->err, 1);
  return NULL;
}

static void _hpb_Decoder_VerifyUtf8(hpb_Decoder* d, const char* buf, int len) {
  if (!_hpb_Decoder_VerifyUtf8Inline(buf, len)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_BadUtf8);
  }
}

static bool _hpb_Decoder_Reserve(hpb_Decoder* d, hpb_Array* arr, size_t elem) {
  bool need_realloc = arr->capacity - arr->size < elem;
  if (need_realloc && !_hpb_array_realloc(arr, arr->size + elem, &d->arena)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
  }
  return need_realloc;
}

typedef struct {
  const char* ptr;
  uint64_t val;
} _hpb_DecodeLongVarintReturn;

HPB_NOINLINE
static _hpb_DecodeLongVarintReturn _hpb_Decoder_DecodeLongVarint(
    const char* ptr, uint64_t val) {
  _hpb_DecodeLongVarintReturn ret = {NULL, 0};
  uint64_t byte;
  int i;
  for (i = 1; i < 10; i++) {
    byte = (uint8_t)ptr[i];
    val += (byte - 1) << (i * 7);
    if (!(byte & 0x80)) {
      ret.ptr = ptr + i + 1;
      ret.val = val;
      return ret;
    }
  }
  return ret;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeVarint(hpb_Decoder* d, const char* ptr,
                                             uint64_t* val) {
  uint64_t byte = (uint8_t)*ptr;
  if (HPB_LIKELY((byte & 0x80) == 0)) {
    *val = byte;
    return ptr + 1;
  } else {
    _hpb_DecodeLongVarintReturn res = _hpb_Decoder_DecodeLongVarint(ptr, byte);
    if (!res.ptr) _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
    *val = res.val;
    return res.ptr;
  }
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeTag(hpb_Decoder* d, const char* ptr,
                                          uint32_t* val) {
  uint64_t byte = (uint8_t)*ptr;
  if (HPB_LIKELY((byte & 0x80) == 0)) {
    *val = byte;
    return ptr + 1;
  } else {
    const char* start = ptr;
    _hpb_DecodeLongVarintReturn res = _hpb_Decoder_DecodeLongVarint(ptr, byte);
    if (!res.ptr || res.ptr - start > 5 || res.val > UINT32_MAX) {
      _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
    }
    *val = res.val;
    return res.ptr;
  }
}

HPB_FORCEINLINE
static const char* hpb_Decoder_DecodeSize(hpb_Decoder* d, const char* ptr,
                                          uint32_t* size) {
  uint64_t size64;
  ptr = _hpb_Decoder_DecodeVarint(d, ptr, &size64);
  if (size64 >= INT32_MAX ||
      !hpb_EpsCopyInputStream_CheckSize(&d->input, ptr, (int)size64)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
  }
  *size = size64;
  return ptr;
}

static void _hpb_Decoder_MungeInt32(wireval* val) {
  if (!_hpb_IsLittleEndian()) {
    /* The next stage will memcpy(dst, &val, 4) */
    val->uint32_val = val->uint64_val;
  }
}

static void _hpb_Decoder_Munge(int type, wireval* val) {
  switch (type) {
    case kHpb_FieldType_Bool:
      val->bool_val = val->uint64_val != 0;
      break;
    case kHpb_FieldType_SInt32: {
      uint32_t n = val->uint64_val;
      val->uint32_val = (n >> 1) ^ -(int32_t)(n & 1);
      break;
    }
    case kHpb_FieldType_SInt64: {
      uint64_t n = val->uint64_val;
      val->uint64_val = (n >> 1) ^ -(int64_t)(n & 1);
      break;
    }
    case kHpb_FieldType_Int32:
    case kHpb_FieldType_UInt32:
    case kHpb_FieldType_Enum:
      _hpb_Decoder_MungeInt32(val);
      break;
  }
}

static hpb_Message* _hpb_Decoder_NewSubMessage(hpb_Decoder* d,
                                               const hpb_MiniTableSub* subs,
                                               const hpb_MiniTableField* field,
                                               hpb_TaggedMessagePtr* target) {
  const hpb_MiniTable* subl = subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(subl);
  hpb_Message* msg = _hpb_Message_New(subl, &d->arena);
  if (!msg) _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);

  // Extensions should not be unlinked.  A message extension should not be
  // registered until its sub-message type is available to be linked.
  bool is_empty = subl == &_kHpb_MiniTable_Empty;
  bool is_extension = field->mode & kHpb_LabelFlags_IsExtension;
  HPB_ASSERT(!(is_empty && is_extension));

  if (is_empty && !(d->options & kHpb_DecodeOption_ExperimentalAllowUnlinked)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_UnlinkedSubMessage);
  }

  hpb_TaggedMessagePtr tagged = _hpb_TaggedMessagePtr_Pack(msg, is_empty);
  memcpy(target, &tagged, sizeof(tagged));
  return msg;
}

static hpb_Message* _hpb_Decoder_ReuseSubMessage(
    hpb_Decoder* d, const hpb_MiniTableSub* subs,
    const hpb_MiniTableField* field, hpb_TaggedMessagePtr* target) {
  hpb_TaggedMessagePtr tagged = *target;
  const hpb_MiniTable* subl = subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(subl);
  if (!hpb_TaggedMessagePtr_IsEmpty(tagged) || subl == &_kHpb_MiniTable_Empty) {
    return _hpb_TaggedMessagePtr_GetMessage(tagged);
  }

  // We found an empty message from a previous parse that was performed before
  // this field was linked.  But it is linked now, so we want to allocate a new
  // message of the correct type and promote data into it before continuing.
  hpb_Message* existing = _hpb_TaggedMessagePtr_GetEmptyMessage(tagged);
  hpb_Message* promoted = _hpb_Decoder_NewSubMessage(d, subs, field, target);
  size_t size;
  const char* unknown = hpb_Message_GetUnknown(existing, &size);
  hpb_DecodeStatus status = hpb_Decode(unknown, size, promoted, subl, d->extreg,
                                       d->options, &d->arena);
  if (status != kHpb_DecodeStatus_Ok) _hpb_Decoder_ErrorJmp(d, status);
  return promoted;
}

static const char* _hpb_Decoder_ReadString(hpb_Decoder* d, const char* ptr,
                                           int size, hpb_StringView* str) {
  const char* str_ptr = ptr;
  ptr = hpb_EpsCopyInputStream_ReadString(&d->input, &str_ptr, size, &d->arena);
  if (!ptr) _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
  str->data = str_ptr;
  str->size = size;
  return ptr;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_RecurseSubMessage(hpb_Decoder* d,
                                                  const char* ptr,
                                                  hpb_Message* submsg,
                                                  const hpb_MiniTable* subl,
                                                  uint32_t expected_end_group) {
  if (--d->depth < 0) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_MaxDepthExceeded);
  }
  ptr = _hpb_Decoder_DecodeMessage(d, ptr, submsg, subl);
  d->depth++;
  if (d->end_group != expected_end_group) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
  }
  return ptr;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeSubMessage(
    hpb_Decoder* d, const char* ptr, hpb_Message* submsg,
    const hpb_MiniTableSub* subs, const hpb_MiniTableField* field, int size) {
  int saved_delta = hpb_EpsCopyInputStream_PushLimit(&d->input, ptr, size);
  const hpb_MiniTable* subl = subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(subl);
  ptr = _hpb_Decoder_RecurseSubMessage(d, ptr, submsg, subl, DECODE_NOGROUP);
  hpb_EpsCopyInputStream_PopLimit(&d->input, ptr, saved_delta);
  return ptr;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeGroup(hpb_Decoder* d, const char* ptr,
                                            hpb_Message* submsg,
                                            const hpb_MiniTable* subl,
                                            uint32_t number) {
  if (_hpb_Decoder_IsDone(d, &ptr)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
  }
  ptr = _hpb_Decoder_RecurseSubMessage(d, ptr, submsg, subl, number);
  d->end_group = DECODE_NOGROUP;
  return ptr;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeUnknownGroup(hpb_Decoder* d,
                                                   const char* ptr,
                                                   uint32_t number) {
  return _hpb_Decoder_DecodeGroup(d, ptr, NULL, NULL, number);
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeKnownGroup(
    hpb_Decoder* d, const char* ptr, hpb_Message* submsg,
    const hpb_MiniTableSub* subs, const hpb_MiniTableField* field) {
  const hpb_MiniTable* subl = subs[field->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(subl);
  return _hpb_Decoder_DecodeGroup(d, ptr, submsg, subl, field->number);
}

static char* hpb_Decoder_EncodeVarint32(uint32_t val, char* ptr) {
  do {
    uint8_t byte = val & 0x7fU;
    val >>= 7;
    if (val) byte |= 0x80U;
    *(ptr++) = byte;
  } while (val);
  return ptr;
}

static void _hpb_Decoder_AddUnknownVarints(hpb_Decoder* d, hpb_Message* msg,
                                           uint32_t val1, uint32_t val2) {
  char buf[20];
  char* end = buf;
  end = hpb_Decoder_EncodeVarint32(val1, end);
  end = hpb_Decoder_EncodeVarint32(val2, end);

  if (!_hpb_Message_AddUnknown(msg, buf, end - buf, &d->arena)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
  }
}

HPB_NOINLINE
static bool _hpb_Decoder_CheckEnumSlow(hpb_Decoder* d, const char* ptr,
                                       hpb_Message* msg,
                                       const hpb_MiniTableEnum* e,
                                       const hpb_MiniTableField* field,
                                       uint32_t v) {
  if (_hpb_MiniTable_CheckEnumValueSlow(e, v)) return true;

  // Unrecognized enum goes into unknown fields.
  // For packed fields the tag could be arbitrarily far in the past, so we
  // just re-encode the tag and value here.
  uint32_t tag = ((uint32_t)field->number << 3) | kHpb_WireType_Varint;
  hpb_Message* unknown_msg =
      field->mode & kHpb_LabelFlags_IsExtension ? d->unknown_msg : msg;
  _hpb_Decoder_AddUnknownVarints(d, unknown_msg, tag, v);
  return false;
}

HPB_FORCEINLINE
static bool _hpb_Decoder_CheckEnum(hpb_Decoder* d, const char* ptr,
                                   hpb_Message* msg, const hpb_MiniTableEnum* e,
                                   const hpb_MiniTableField* field,
                                   wireval* val) {
  uint32_t v = val->uint32_val;

  _kHpb_FastEnumCheck_Status status = _hpb_MiniTable_CheckEnumValueFast(e, v);
  if (HPB_LIKELY(status == _kHpb_FastEnumCheck_ValueIsInEnum)) return true;
  return _hpb_Decoder_CheckEnumSlow(d, ptr, msg, e, field, v);
}

HPB_NOINLINE
static const char* _hpb_Decoder_DecodeEnumArray(hpb_Decoder* d, const char* ptr,
                                                hpb_Message* msg,
                                                hpb_Array* arr,
                                                const hpb_MiniTableSub* subs,
                                                const hpb_MiniTableField* field,
                                                wireval* val) {
  const hpb_MiniTableEnum* e = subs[field->HPB_PRIVATE(submsg_index)].subenum;
  if (!_hpb_Decoder_CheckEnum(d, ptr, msg, e, field, val)) return ptr;
  void* mem = HPB_PTR_AT(_hpb_array_ptr(arr), arr->size * 4, void);
  arr->size++;
  memcpy(mem, val, 4);
  return ptr;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeFixedPacked(
    hpb_Decoder* d, const char* ptr, hpb_Array* arr, wireval* val,
    const hpb_MiniTableField* field, int lg2) {
  int mask = (1 << lg2) - 1;
  size_t count = val->size >> lg2;
  if ((val->size & mask) != 0) {
    // Length isn't a round multiple of elem size.
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
  }
  _hpb_Decoder_Reserve(d, arr, count);
  void* mem = HPB_PTR_AT(_hpb_array_ptr(arr), arr->size << lg2, void);
  arr->size += count;
  // Note: if/when the decoder supports multi-buffer input, we will need to
  // handle buffer seams here.
  if (_hpb_IsLittleEndian()) {
    ptr = hpb_EpsCopyInputStream_Copy(&d->input, ptr, mem, val->size);
  } else {
    int delta = hpb_EpsCopyInputStream_PushLimit(&d->input, ptr, val->size);
    char* dst = mem;
    while (!_hpb_Decoder_IsDone(d, &ptr)) {
      if (lg2 == 2) {
        ptr = hpb_WireReader_ReadFixed32(ptr, dst);
        dst += 4;
      } else {
        HPB_ASSERT(lg2 == 3);
        ptr = hpb_WireReader_ReadFixed64(ptr, dst);
        dst += 8;
      }
    }
    hpb_EpsCopyInputStream_PopLimit(&d->input, ptr, delta);
  }

  return ptr;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeVarintPacked(
    hpb_Decoder* d, const char* ptr, hpb_Array* arr, wireval* val,
    const hpb_MiniTableField* field, int lg2) {
  int scale = 1 << lg2;
  int saved_limit = hpb_EpsCopyInputStream_PushLimit(&d->input, ptr, val->size);
  char* out = HPB_PTR_AT(_hpb_array_ptr(arr), arr->size << lg2, void);
  while (!_hpb_Decoder_IsDone(d, &ptr)) {
    wireval elem;
    ptr = _hpb_Decoder_DecodeVarint(d, ptr, &elem.uint64_val);
    _hpb_Decoder_Munge(field->HPB_PRIVATE(descriptortype), &elem);
    if (_hpb_Decoder_Reserve(d, arr, 1)) {
      out = HPB_PTR_AT(_hpb_array_ptr(arr), arr->size << lg2, void);
    }
    arr->size++;
    memcpy(out, &elem, scale);
    out += scale;
  }
  hpb_EpsCopyInputStream_PopLimit(&d->input, ptr, saved_limit);
  return ptr;
}

HPB_NOINLINE
static const char* _hpb_Decoder_DecodeEnumPacked(
    hpb_Decoder* d, const char* ptr, hpb_Message* msg, hpb_Array* arr,
    const hpb_MiniTableSub* subs, const hpb_MiniTableField* field,
    wireval* val) {
  const hpb_MiniTableEnum* e = subs[field->HPB_PRIVATE(submsg_index)].subenum;
  int saved_limit = hpb_EpsCopyInputStream_PushLimit(&d->input, ptr, val->size);
  char* out = HPB_PTR_AT(_hpb_array_ptr(arr), arr->size * 4, void);
  while (!_hpb_Decoder_IsDone(d, &ptr)) {
    wireval elem;
    ptr = _hpb_Decoder_DecodeVarint(d, ptr, &elem.uint64_val);
    _hpb_Decoder_MungeInt32(&elem);
    if (!_hpb_Decoder_CheckEnum(d, ptr, msg, e, field, &elem)) {
      continue;
    }
    if (_hpb_Decoder_Reserve(d, arr, 1)) {
      out = HPB_PTR_AT(_hpb_array_ptr(arr), arr->size * 4, void);
    }
    arr->size++;
    memcpy(out, &elem, 4);
    out += 4;
  }
  hpb_EpsCopyInputStream_PopLimit(&d->input, ptr, saved_limit);
  return ptr;
}

hpb_Array* _hpb_Decoder_CreateArray(hpb_Decoder* d,
                                    const hpb_MiniTableField* field) {
  /* Maps descriptor type -> elem_size_lg2.  */
  static const uint8_t kElemSizeLg2[] = {
      [0] = -1,  // invalid descriptor type
      [kHpb_FieldType_Double] = 3,
      [kHpb_FieldType_Float] = 2,
      [kHpb_FieldType_Int64] = 3,
      [kHpb_FieldType_UInt64] = 3,
      [kHpb_FieldType_Int32] = 2,
      [kHpb_FieldType_Fixed64] = 3,
      [kHpb_FieldType_Fixed32] = 2,
      [kHpb_FieldType_Bool] = 0,
      [kHpb_FieldType_String] = HPB_SIZE(3, 4),
      [kHpb_FieldType_Group] = HPB_SIZE(2, 3),
      [kHpb_FieldType_Message] = HPB_SIZE(2, 3),
      [kHpb_FieldType_Bytes] = HPB_SIZE(3, 4),
      [kHpb_FieldType_UInt32] = 2,
      [kHpb_FieldType_Enum] = 2,
      [kHpb_FieldType_SFixed32] = 2,
      [kHpb_FieldType_SFixed64] = 3,
      [kHpb_FieldType_SInt32] = 2,
      [kHpb_FieldType_SInt64] = 3,
  };

  size_t lg2 = kElemSizeLg2[field->HPB_PRIVATE(descriptortype)];
  hpb_Array* ret = _hpb_Array_New(&d->arena, 4, lg2);
  if (!ret) _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
  return ret;
}

static const char* _hpb_Decoder_DecodeToArray(hpb_Decoder* d, const char* ptr,
                                              hpb_Message* msg,
                                              const hpb_MiniTableSub* subs,
                                              const hpb_MiniTableField* field,
                                              wireval* val, int op) {
  hpb_Array** arrp = HPB_PTR_AT(msg, field->offset, void);
  hpb_Array* arr = *arrp;
  void* mem;

  if (arr) {
    _hpb_Decoder_Reserve(d, arr, 1);
  } else {
    arr = _hpb_Decoder_CreateArray(d, field);
    *arrp = arr;
  }

  switch (op) {
    case kHpb_DecodeOp_Scalar1Byte:
    case kHpb_DecodeOp_Scalar4Byte:
    case kHpb_DecodeOp_Scalar8Byte:
      /* Append scalar value. */
      mem = HPB_PTR_AT(_hpb_array_ptr(arr), arr->size << op, void);
      arr->size++;
      memcpy(mem, val, 1 << op);
      return ptr;
    case kHpb_DecodeOp_String:
      _hpb_Decoder_VerifyUtf8(d, ptr, val->size);
      /* Fallthrough. */
    case kHpb_DecodeOp_Bytes: {
      /* Append bytes. */
      hpb_StringView* str = (hpb_StringView*)_hpb_array_ptr(arr) + arr->size;
      arr->size++;
      return _hpb_Decoder_ReadString(d, ptr, val->size, str);
    }
    case kHpb_DecodeOp_SubMessage: {
      /* Append submessage / group. */
      hpb_TaggedMessagePtr* target = HPB_PTR_AT(
          _hpb_array_ptr(arr), arr->size * sizeof(void*), hpb_TaggedMessagePtr);
      hpb_Message* submsg = _hpb_Decoder_NewSubMessage(d, subs, field, target);
      arr->size++;
      if (HPB_UNLIKELY(field->HPB_PRIVATE(descriptortype) ==
                       kHpb_FieldType_Group)) {
        return _hpb_Decoder_DecodeKnownGroup(d, ptr, submsg, subs, field);
      } else {
        return _hpb_Decoder_DecodeSubMessage(d, ptr, submsg, subs, field,
                                             val->size);
      }
    }
    case OP_FIXPCK_LG2(2):
    case OP_FIXPCK_LG2(3):
      return _hpb_Decoder_DecodeFixedPacked(d, ptr, arr, val, field,
                                            op - OP_FIXPCK_LG2(0));
    case OP_VARPCK_LG2(0):
    case OP_VARPCK_LG2(2):
    case OP_VARPCK_LG2(3):
      return _hpb_Decoder_DecodeVarintPacked(d, ptr, arr, val, field,
                                             op - OP_VARPCK_LG2(0));
    case kHpb_DecodeOp_Enum:
      return _hpb_Decoder_DecodeEnumArray(d, ptr, msg, arr, subs, field, val);
    case kHpb_DecodeOp_PackedEnum:
      return _hpb_Decoder_DecodeEnumPacked(d, ptr, msg, arr, subs, field, val);
    default:
      HPB_UNREACHABLE();
  }
}

hpb_Map* _hpb_Decoder_CreateMap(hpb_Decoder* d, const hpb_MiniTable* entry) {
  /* Maps descriptor type -> hpb map size.  */
  static const uint8_t kSizeInMap[] = {
      [0] = -1,  // invalid descriptor type */
      [kHpb_FieldType_Double] = 8,
      [kHpb_FieldType_Float] = 4,
      [kHpb_FieldType_Int64] = 8,
      [kHpb_FieldType_UInt64] = 8,
      [kHpb_FieldType_Int32] = 4,
      [kHpb_FieldType_Fixed64] = 8,
      [kHpb_FieldType_Fixed32] = 4,
      [kHpb_FieldType_Bool] = 1,
      [kHpb_FieldType_String] = HPB_MAPTYPE_STRING,
      [kHpb_FieldType_Group] = sizeof(void*),
      [kHpb_FieldType_Message] = sizeof(void*),
      [kHpb_FieldType_Bytes] = HPB_MAPTYPE_STRING,
      [kHpb_FieldType_UInt32] = 4,
      [kHpb_FieldType_Enum] = 4,
      [kHpb_FieldType_SFixed32] = 4,
      [kHpb_FieldType_SFixed64] = 8,
      [kHpb_FieldType_SInt32] = 4,
      [kHpb_FieldType_SInt64] = 8,
  };

  const hpb_MiniTableField* key_field = &entry->fields[0];
  const hpb_MiniTableField* val_field = &entry->fields[1];
  char key_size = kSizeInMap[key_field->HPB_PRIVATE(descriptortype)];
  char val_size = kSizeInMap[val_field->HPB_PRIVATE(descriptortype)];
  HPB_ASSERT(key_field->offset == offsetof(hpb_MapEntryData, k));
  HPB_ASSERT(val_field->offset == offsetof(hpb_MapEntryData, v));
  hpb_Map* ret = _hpb_Map_New(&d->arena, key_size, val_size);
  if (!ret) _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
  return ret;
}

static const char* _hpb_Decoder_DecodeToMap(hpb_Decoder* d, const char* ptr,
                                            hpb_Message* msg,
                                            const hpb_MiniTableSub* subs,
                                            const hpb_MiniTableField* field,
                                            wireval* val) {
  hpb_Map** map_p = HPB_PTR_AT(msg, field->offset, hpb_Map*);
  hpb_Map* map = *map_p;
  hpb_MapEntry ent;
  HPB_ASSERT(hpb_MiniTableField_Type(field) == kHpb_FieldType_Message);
  const hpb_MiniTable* entry = subs[field->HPB_PRIVATE(submsg_index)].submsg;

  HPB_ASSERT(entry);
  HPB_ASSERT(entry->field_count == 2);
  HPB_ASSERT(!hpb_IsRepeatedOrMap(&entry->fields[0]));
  HPB_ASSERT(!hpb_IsRepeatedOrMap(&entry->fields[1]));

  if (!map) {
    map = _hpb_Decoder_CreateMap(d, entry);
    *map_p = map;
  }

  // Parse map entry.
  memset(&ent, 0, sizeof(ent));

  if (entry->fields[1].HPB_PRIVATE(descriptortype) == kHpb_FieldType_Message ||
      entry->fields[1].HPB_PRIVATE(descriptortype) == kHpb_FieldType_Group) {
    // Create proactively to handle the case where it doesn't appear.
    hpb_TaggedMessagePtr msg;
    _hpb_Decoder_NewSubMessage(d, entry->subs, &entry->fields[1], &msg);
    ent.data.v.val = hpb_value_uintptr(msg);
  }

  ptr =
      _hpb_Decoder_DecodeSubMessage(d, ptr, &ent.data, subs, field, val->size);
  // check if ent had any unknown fields
  size_t size;
  hpb_Message_GetUnknown(&ent.data, &size);
  if (size != 0) {
    char* buf;
    size_t size;
    uint32_t tag = ((uint32_t)field->number << 3) | kHpb_WireType_Delimited;
    hpb_EncodeStatus status =
        hpb_Encode(&ent.data, entry, 0, &d->arena, &buf, &size);
    if (status != kHpb_EncodeStatus_Ok) {
      _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
    }
    _hpb_Decoder_AddUnknownVarints(d, msg, tag, size);
    if (!_hpb_Message_AddUnknown(msg, buf, size, &d->arena)) {
      _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
    }
  } else {
    if (_hpb_Map_Insert(map, &ent.data.k, map->key_size, &ent.data.v,
                        map->val_size,
                        &d->arena) == kHpb_MapInsertStatus_OutOfMemory) {
      _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
    }
  }
  return ptr;
}

static const char* _hpb_Decoder_DecodeToSubMessage(
    hpb_Decoder* d, const char* ptr, hpb_Message* msg,
    const hpb_MiniTableSub* subs, const hpb_MiniTableField* field, wireval* val,
    int op) {
  void* mem = HPB_PTR_AT(msg, field->offset, void);
  int type = field->HPB_PRIVATE(descriptortype);

  if (HPB_UNLIKELY(op == kHpb_DecodeOp_Enum) &&
      !_hpb_Decoder_CheckEnum(d, ptr, msg,
                              subs[field->HPB_PRIVATE(submsg_index)].subenum,
                              field, val)) {
    return ptr;
  }

  /* Set presence if necessary. */
  if (field->presence > 0) {
    _hpb_sethas_field(msg, field);
  } else if (field->presence < 0) {
    /* Oneof case */
    uint32_t* oneof_case = _hpb_oneofcase_field(msg, field);
    if (op == kHpb_DecodeOp_SubMessage && *oneof_case != field->number) {
      memset(mem, 0, sizeof(void*));
    }
    *oneof_case = field->number;
  }

  /* Store into message. */
  switch (op) {
    case kHpb_DecodeOp_SubMessage: {
      hpb_TaggedMessagePtr* submsgp = mem;
      hpb_Message* submsg;
      if (*submsgp) {
        submsg = _hpb_Decoder_ReuseSubMessage(d, subs, field, submsgp);
      } else {
        submsg = _hpb_Decoder_NewSubMessage(d, subs, field, submsgp);
      }
      if (HPB_UNLIKELY(type == kHpb_FieldType_Group)) {
        ptr = _hpb_Decoder_DecodeKnownGroup(d, ptr, submsg, subs, field);
      } else {
        ptr = _hpb_Decoder_DecodeSubMessage(d, ptr, submsg, subs, field,
                                            val->size);
      }
      break;
    }
    case kHpb_DecodeOp_String:
      _hpb_Decoder_VerifyUtf8(d, ptr, val->size);
      /* Fallthrough. */
    case kHpb_DecodeOp_Bytes:
      return _hpb_Decoder_ReadString(d, ptr, val->size, mem);
    case kHpb_DecodeOp_Scalar8Byte:
      memcpy(mem, val, 8);
      break;
    case kHpb_DecodeOp_Enum:
    case kHpb_DecodeOp_Scalar4Byte:
      memcpy(mem, val, 4);
      break;
    case kHpb_DecodeOp_Scalar1Byte:
      memcpy(mem, val, 1);
      break;
    default:
      HPB_UNREACHABLE();
  }

  return ptr;
}

HPB_NOINLINE
const char* _hpb_Decoder_CheckRequired(hpb_Decoder* d, const char* ptr,
                                       const hpb_Message* msg,
                                       const hpb_MiniTable* l) {
  assert(l->required_count);
  if (HPB_LIKELY((d->options & kHpb_DecodeOption_CheckRequired) == 0)) {
    return ptr;
  }
  uint64_t msg_head;
  memcpy(&msg_head, msg, 8);
  msg_head = _hpb_BigEndian_Swap64(msg_head);
  if (hpb_MiniTable_requiredmask(l) & ~msg_head) {
    d->missing_required = true;
  }
  return ptr;
}

HPB_FORCEINLINE
static bool _hpb_Decoder_TryFastDispatch(hpb_Decoder* d, const char** ptr,
                                         hpb_Message* msg,
                                         const hpb_MiniTable* layout) {
#if HPB_FASTTABLE
  if (layout && layout->table_mask != (unsigned char)-1) {
    uint16_t tag = _hpb_FastDecoder_LoadTag(*ptr);
    intptr_t table = decode_totable(layout);
    *ptr = _hpb_FastDecoder_TagDispatch(d, *ptr, msg, table, 0, tag);
    return true;
  }
#endif
  return false;
}

static const char* hpb_Decoder_SkipField(hpb_Decoder* d, const char* ptr,
                                         uint32_t tag) {
  int field_number = tag >> 3;
  int wire_type = tag & 7;
  switch (wire_type) {
    case kHpb_WireType_Varint: {
      uint64_t val;
      return _hpb_Decoder_DecodeVarint(d, ptr, &val);
    }
    case kHpb_WireType_64Bit:
      return ptr + 8;
    case kHpb_WireType_32Bit:
      return ptr + 4;
    case kHpb_WireType_Delimited: {
      uint32_t size;
      ptr = hpb_Decoder_DecodeSize(d, ptr, &size);
      return ptr + size;
    }
    case kHpb_WireType_StartGroup:
      return _hpb_Decoder_DecodeUnknownGroup(d, ptr, field_number);
    default:
      _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
  }
}

enum {
  kStartItemTag = ((kHpb_MsgSet_Item << 3) | kHpb_WireType_StartGroup),
  kEndItemTag = ((kHpb_MsgSet_Item << 3) | kHpb_WireType_EndGroup),
  kTypeIdTag = ((kHpb_MsgSet_TypeId << 3) | kHpb_WireType_Varint),
  kMessageTag = ((kHpb_MsgSet_Message << 3) | kHpb_WireType_Delimited),
};

static void hpb_Decoder_AddKnownMessageSetItem(
    hpb_Decoder* d, hpb_Message* msg, const hpb_MiniTableExtension* item_mt,
    const char* data, uint32_t size) {
  hpb_Message_Extension* ext =
      _hpb_Message_GetOrCreateExtension(msg, item_mt, &d->arena);
  if (HPB_UNLIKELY(!ext)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
  }
  hpb_Message* submsg = _hpb_Decoder_NewSubMessage(
      d, &ext->ext->sub, &ext->ext->field, (hpb_TaggedMessagePtr*)&ext->data);
  hpb_DecodeStatus status = hpb_Decode(data, size, submsg, item_mt->sub.submsg,
                                       d->extreg, d->options, &d->arena);
  if (status != kHpb_DecodeStatus_Ok) _hpb_Decoder_ErrorJmp(d, status);
}

static void hpb_Decoder_AddUnknownMessageSetItem(hpb_Decoder* d,
                                                 hpb_Message* msg,
                                                 uint32_t type_id,
                                                 const char* message_data,
                                                 uint32_t message_size) {
  char buf[60];
  char* ptr = buf;
  ptr = hpb_Decoder_EncodeVarint32(kStartItemTag, ptr);
  ptr = hpb_Decoder_EncodeVarint32(kTypeIdTag, ptr);
  ptr = hpb_Decoder_EncodeVarint32(type_id, ptr);
  ptr = hpb_Decoder_EncodeVarint32(kMessageTag, ptr);
  ptr = hpb_Decoder_EncodeVarint32(message_size, ptr);
  char* split = ptr;

  ptr = hpb_Decoder_EncodeVarint32(kEndItemTag, ptr);
  char* end = ptr;

  if (!_hpb_Message_AddUnknown(msg, buf, split - buf, &d->arena) ||
      !_hpb_Message_AddUnknown(msg, message_data, message_size, &d->arena) ||
      !_hpb_Message_AddUnknown(msg, split, end - split, &d->arena)) {
    _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
  }
}

static void hpb_Decoder_AddMessageSetItem(hpb_Decoder* d, hpb_Message* msg,
                                          const hpb_MiniTable* t,
                                          uint32_t type_id, const char* data,
                                          uint32_t size) {
  const hpb_MiniTableExtension* item_mt =
      hpb_ExtensionRegistry_Lookup(d->extreg, t, type_id);
  if (item_mt) {
    hpb_Decoder_AddKnownMessageSetItem(d, msg, item_mt, data, size);
  } else {
    hpb_Decoder_AddUnknownMessageSetItem(d, msg, type_id, data, size);
  }
}

static const char* hpb_Decoder_DecodeMessageSetItem(
    hpb_Decoder* d, const char* ptr, hpb_Message* msg,
    const hpb_MiniTable* layout) {
  uint32_t type_id = 0;
  hpb_StringView preserved = {NULL, 0};
  typedef enum {
    kHpb_HaveId = 1 << 0,
    kHpb_HavePayload = 1 << 1,
  } StateMask;
  StateMask state_mask = 0;
  while (!_hpb_Decoder_IsDone(d, &ptr)) {
    uint32_t tag;
    ptr = _hpb_Decoder_DecodeTag(d, ptr, &tag);
    switch (tag) {
      case kEndItemTag:
        return ptr;
      case kTypeIdTag: {
        uint64_t tmp;
        ptr = _hpb_Decoder_DecodeVarint(d, ptr, &tmp);
        if (state_mask & kHpb_HaveId) break;  // Ignore dup.
        state_mask |= kHpb_HaveId;
        type_id = tmp;
        if (state_mask & kHpb_HavePayload) {
          hpb_Decoder_AddMessageSetItem(d, msg, layout, type_id, preserved.data,
                                        preserved.size);
        }
        break;
      }
      case kMessageTag: {
        uint32_t size;
        ptr = hpb_Decoder_DecodeSize(d, ptr, &size);
        const char* data = ptr;
        ptr += size;
        if (state_mask & kHpb_HavePayload) break;  // Ignore dup.
        state_mask |= kHpb_HavePayload;
        if (state_mask & kHpb_HaveId) {
          hpb_Decoder_AddMessageSetItem(d, msg, layout, type_id, data, size);
        } else {
          // Out of order, we must preserve the payload.
          preserved.data = data;
          preserved.size = size;
        }
        break;
      }
      default:
        // We do not preserve unexpected fields inside a message set item.
        ptr = hpb_Decoder_SkipField(d, ptr, tag);
        break;
    }
  }
  _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
}

static const hpb_MiniTableField* _hpb_Decoder_FindField(hpb_Decoder* d,
                                                        const hpb_MiniTable* t,
                                                        uint32_t field_number,
                                                        int* last_field_index) {
  static hpb_MiniTableField none = {
      0, 0, 0, 0, kHpb_FakeFieldType_FieldNotFound, 0};
  if (t == NULL) return &none;

  size_t idx = ((size_t)field_number) - 1;  // 0 wraps to SIZE_MAX
  if (idx < t->dense_below) {
    /* Fastest case: index into dense fields. */
    goto found;
  }

  if (t->dense_below < t->field_count) {
    /* Linear search non-dense fields. Resume scanning from last_field_index
     * since fields are usually in order. */
    size_t last = *last_field_index;
    for (idx = last; idx < t->field_count; idx++) {
      if (t->fields[idx].number == field_number) {
        goto found;
      }
    }

    for (idx = t->dense_below; idx < last; idx++) {
      if (t->fields[idx].number == field_number) {
        goto found;
      }
    }
  }

  if (d->extreg) {
    switch (t->ext) {
      case kHpb_ExtMode_Extendable: {
        const hpb_MiniTableExtension* ext =
            hpb_ExtensionRegistry_Lookup(d->extreg, t, field_number);
        if (ext) return &ext->field;
        break;
      }
      case kHpb_ExtMode_IsMessageSet:
        if (field_number == kHpb_MsgSet_Item) {
          static hpb_MiniTableField item = {
              0, 0, 0, 0, kHpb_FakeFieldType_MessageSetItem, 0};
          return &item;
        }
        break;
    }
  }

  return &none; /* Unknown field. */

found:
  HPB_ASSERT(t->fields[idx].number == field_number);
  *last_field_index = idx;
  return &t->fields[idx];
}

int _hpb_Decoder_GetVarintOp(const hpb_MiniTableField* field) {
  static const int8_t kVarintOps[] = {
      [kHpb_FakeFieldType_FieldNotFound] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Double] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Float] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Int64] = kHpb_DecodeOp_Scalar8Byte,
      [kHpb_FieldType_UInt64] = kHpb_DecodeOp_Scalar8Byte,
      [kHpb_FieldType_Int32] = kHpb_DecodeOp_Scalar4Byte,
      [kHpb_FieldType_Fixed64] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Fixed32] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Bool] = kHpb_DecodeOp_Scalar1Byte,
      [kHpb_FieldType_String] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Group] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Message] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Bytes] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_UInt32] = kHpb_DecodeOp_Scalar4Byte,
      [kHpb_FieldType_Enum] = kHpb_DecodeOp_Enum,
      [kHpb_FieldType_SFixed32] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_SFixed64] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_SInt32] = kHpb_DecodeOp_Scalar4Byte,
      [kHpb_FieldType_SInt64] = kHpb_DecodeOp_Scalar8Byte,
      [kHpb_FakeFieldType_MessageSetItem] = kHpb_DecodeOp_UnknownField,
  };

  return kVarintOps[field->HPB_PRIVATE(descriptortype)];
}

HPB_FORCEINLINE
static void _hpb_Decoder_CheckUnlinked(hpb_Decoder* d, const hpb_MiniTable* mt,
                                       const hpb_MiniTableField* field,
                                       int* op) {
  // If sub-message is not linked, treat as unknown.
  if (field->mode & kHpb_LabelFlags_IsExtension) return;
  const hpb_MiniTableSub* sub = &mt->subs[field->HPB_PRIVATE(submsg_index)];
  if ((d->options & kHpb_DecodeOption_ExperimentalAllowUnlinked) ||
      sub->submsg != &_kHpb_MiniTable_Empty) {
    return;
  }
#ifndef NDEBUG
  const hpb_MiniTableField* oneof = hpb_MiniTable_GetOneof(mt, field);
  if (oneof) {
    // All other members of the oneof must be message fields that are also
    // unlinked.
    do {
      assert(hpb_MiniTableField_CType(oneof) == kHpb_CType_Message);
      const hpb_MiniTableSub* oneof_sub =
          &mt->subs[oneof->HPB_PRIVATE(submsg_index)];
      assert(!oneof_sub);
    } while (hpb_MiniTable_NextOneofField(mt, &oneof));
  }
#endif  // NDEBUG
  *op = kHpb_DecodeOp_UnknownField;
}

int _hpb_Decoder_GetDelimitedOp(hpb_Decoder* d, const hpb_MiniTable* mt,
                                const hpb_MiniTableField* field) {
  enum { kRepeatedBase = 19 };

  static const int8_t kDelimitedOps[] = {
      /* For non-repeated field type. */
      [kHpb_FakeFieldType_FieldNotFound] =
          kHpb_DecodeOp_UnknownField,  // Field not found.
      [kHpb_FieldType_Double] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Float] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Int64] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_UInt64] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Int32] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Fixed64] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Fixed32] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Bool] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_String] = kHpb_DecodeOp_String,
      [kHpb_FieldType_Group] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Message] = kHpb_DecodeOp_SubMessage,
      [kHpb_FieldType_Bytes] = kHpb_DecodeOp_Bytes,
      [kHpb_FieldType_UInt32] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_Enum] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_SFixed32] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_SFixed64] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_SInt32] = kHpb_DecodeOp_UnknownField,
      [kHpb_FieldType_SInt64] = kHpb_DecodeOp_UnknownField,
      [kHpb_FakeFieldType_MessageSetItem] = kHpb_DecodeOp_UnknownField,
      // For repeated field type. */
      [kRepeatedBase + kHpb_FieldType_Double] = OP_FIXPCK_LG2(3),
      [kRepeatedBase + kHpb_FieldType_Float] = OP_FIXPCK_LG2(2),
      [kRepeatedBase + kHpb_FieldType_Int64] = OP_VARPCK_LG2(3),
      [kRepeatedBase + kHpb_FieldType_UInt64] = OP_VARPCK_LG2(3),
      [kRepeatedBase + kHpb_FieldType_Int32] = OP_VARPCK_LG2(2),
      [kRepeatedBase + kHpb_FieldType_Fixed64] = OP_FIXPCK_LG2(3),
      [kRepeatedBase + kHpb_FieldType_Fixed32] = OP_FIXPCK_LG2(2),
      [kRepeatedBase + kHpb_FieldType_Bool] = OP_VARPCK_LG2(0),
      [kRepeatedBase + kHpb_FieldType_String] = kHpb_DecodeOp_String,
      [kRepeatedBase + kHpb_FieldType_Group] = kHpb_DecodeOp_SubMessage,
      [kRepeatedBase + kHpb_FieldType_Message] = kHpb_DecodeOp_SubMessage,
      [kRepeatedBase + kHpb_FieldType_Bytes] = kHpb_DecodeOp_Bytes,
      [kRepeatedBase + kHpb_FieldType_UInt32] = OP_VARPCK_LG2(2),
      [kRepeatedBase + kHpb_FieldType_Enum] = kHpb_DecodeOp_PackedEnum,
      [kRepeatedBase + kHpb_FieldType_SFixed32] = OP_FIXPCK_LG2(2),
      [kRepeatedBase + kHpb_FieldType_SFixed64] = OP_FIXPCK_LG2(3),
      [kRepeatedBase + kHpb_FieldType_SInt32] = OP_VARPCK_LG2(2),
      [kRepeatedBase + kHpb_FieldType_SInt64] = OP_VARPCK_LG2(3),
      // Omitting kHpb_FakeFieldType_MessageSetItem, because we never emit a
      // repeated msgset type
  };

  int ndx = field->HPB_PRIVATE(descriptortype);
  if (hpb_FieldMode_Get(field) == kHpb_FieldMode_Array) ndx += kRepeatedBase;
  int op = kDelimitedOps[ndx];

  if (op == kHpb_DecodeOp_SubMessage) {
    _hpb_Decoder_CheckUnlinked(d, mt, field, &op);
  }

  return op;
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeWireValue(hpb_Decoder* d, const char* ptr,
                                                const hpb_MiniTable* mt,
                                                const hpb_MiniTableField* field,
                                                int wire_type, wireval* val,
                                                int* op) {
  static const unsigned kFixed32OkMask = (1 << kHpb_FieldType_Float) |
                                         (1 << kHpb_FieldType_Fixed32) |
                                         (1 << kHpb_FieldType_SFixed32);

  static const unsigned kFixed64OkMask = (1 << kHpb_FieldType_Double) |
                                         (1 << kHpb_FieldType_Fixed64) |
                                         (1 << kHpb_FieldType_SFixed64);

  switch (wire_type) {
    case kHpb_WireType_Varint:
      ptr = _hpb_Decoder_DecodeVarint(d, ptr, &val->uint64_val);
      *op = _hpb_Decoder_GetVarintOp(field);
      _hpb_Decoder_Munge(field->HPB_PRIVATE(descriptortype), val);
      return ptr;
    case kHpb_WireType_32Bit:
      *op = kHpb_DecodeOp_Scalar4Byte;
      if (((1 << field->HPB_PRIVATE(descriptortype)) & kFixed32OkMask) == 0) {
        *op = kHpb_DecodeOp_UnknownField;
      }
      return hpb_WireReader_ReadFixed32(ptr, &val->uint32_val);
    case kHpb_WireType_64Bit:
      *op = kHpb_DecodeOp_Scalar8Byte;
      if (((1 << field->HPB_PRIVATE(descriptortype)) & kFixed64OkMask) == 0) {
        *op = kHpb_DecodeOp_UnknownField;
      }
      return hpb_WireReader_ReadFixed64(ptr, &val->uint64_val);
    case kHpb_WireType_Delimited:
      ptr = hpb_Decoder_DecodeSize(d, ptr, &val->size);
      *op = _hpb_Decoder_GetDelimitedOp(d, mt, field);
      return ptr;
    case kHpb_WireType_StartGroup:
      val->uint32_val = field->number;
      if (field->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Group) {
        *op = kHpb_DecodeOp_SubMessage;
        _hpb_Decoder_CheckUnlinked(d, mt, field, op);
      } else if (field->HPB_PRIVATE(descriptortype) ==
                 kHpb_FakeFieldType_MessageSetItem) {
        *op = kHpb_DecodeOp_MessageSetItem;
      } else {
        *op = kHpb_DecodeOp_UnknownField;
      }
      return ptr;
    default:
      break;
  }
  _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);
}

HPB_FORCEINLINE
static const char* _hpb_Decoder_DecodeKnownField(
    hpb_Decoder* d, const char* ptr, hpb_Message* msg,
    const hpb_MiniTable* layout, const hpb_MiniTableField* field, int op,
    wireval* val) {
  const hpb_MiniTableSub* subs = layout->subs;
  uint8_t mode = field->mode;

  if (HPB_UNLIKELY(mode & kHpb_LabelFlags_IsExtension)) {
    const hpb_MiniTableExtension* ext_layout =
        (const hpb_MiniTableExtension*)field;
    hpb_Message_Extension* ext =
        _hpb_Message_GetOrCreateExtension(msg, ext_layout, &d->arena);
    if (HPB_UNLIKELY(!ext)) {
      _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
    }
    d->unknown_msg = msg;
    msg = &ext->data;
    subs = &ext->ext->sub;
  }

  switch (mode & kHpb_FieldMode_Mask) {
    case kHpb_FieldMode_Array:
      return _hpb_Decoder_DecodeToArray(d, ptr, msg, subs, field, val, op);
    case kHpb_FieldMode_Map:
      return _hpb_Decoder_DecodeToMap(d, ptr, msg, subs, field, val);
    case kHpb_FieldMode_Scalar:
      return _hpb_Decoder_DecodeToSubMessage(d, ptr, msg, subs, field, val, op);
    default:
      HPB_UNREACHABLE();
  }
}

static const char* _hpb_Decoder_ReverseSkipVarint(const char* ptr,
                                                  uint32_t val) {
  uint32_t seen = 0;
  do {
    ptr--;
    seen <<= 7;
    seen |= *ptr & 0x7f;
  } while (seen != val);
  return ptr;
}

static const char* _hpb_Decoder_DecodeUnknownField(hpb_Decoder* d,
                                                   const char* ptr,
                                                   hpb_Message* msg,
                                                   int field_number,
                                                   int wire_type, wireval val) {
  if (field_number == 0) _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);

  // Since unknown fields are the uncommon case, we do a little extra work here
  // to walk backwards through the buffer to find the field start.  This frees
  // up a register in the fast paths (when the field is known), which leads to
  // significant speedups in benchmarks.
  const char* start = ptr;

  if (wire_type == kHpb_WireType_Delimited) ptr += val.size;
  if (msg) {
    switch (wire_type) {
      case kHpb_WireType_Varint:
      case kHpb_WireType_Delimited:
        start--;
        while (start[-1] & 0x80) start--;
        break;
      case kHpb_WireType_32Bit:
        start -= 4;
        break;
      case kHpb_WireType_64Bit:
        start -= 8;
        break;
      default:
        break;
    }

    assert(start == d->debug_valstart);
    uint32_t tag = ((uint32_t)field_number << 3) | wire_type;
    start = _hpb_Decoder_ReverseSkipVarint(start, tag);
    assert(start == d->debug_tagstart);

    if (wire_type == kHpb_WireType_StartGroup) {
      d->unknown = start;
      d->unknown_msg = msg;
      ptr = _hpb_Decoder_DecodeUnknownGroup(d, ptr, field_number);
      start = d->unknown;
      d->unknown = NULL;
    }
    if (!_hpb_Message_AddUnknown(msg, start, ptr - start, &d->arena)) {
      _hpb_Decoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
    }
  } else if (wire_type == kHpb_WireType_StartGroup) {
    ptr = _hpb_Decoder_DecodeUnknownGroup(d, ptr, field_number);
  }
  return ptr;
}

HPB_NOINLINE
static const char* _hpb_Decoder_DecodeMessage(hpb_Decoder* d, const char* ptr,
                                              hpb_Message* msg,
                                              const hpb_MiniTable* layout) {
  int last_field_index = 0;

#if HPB_FASTTABLE
  // The first time we want to skip fast dispatch, because we may have just been
  // invoked by the fast parser to handle a case that it bailed on.
  if (!_hpb_Decoder_IsDone(d, &ptr)) goto nofast;
#endif

  while (!_hpb_Decoder_IsDone(d, &ptr)) {
    uint32_t tag;
    const hpb_MiniTableField* field;
    int field_number;
    int wire_type;
    wireval val;
    int op;

    if (_hpb_Decoder_TryFastDispatch(d, &ptr, msg, layout)) break;

#if HPB_FASTTABLE
  nofast:
#endif

#ifndef NDEBUG
    d->debug_tagstart = ptr;
#endif

    HPB_ASSERT(ptr < d->input.limit_ptr);
    ptr = _hpb_Decoder_DecodeTag(d, ptr, &tag);
    field_number = tag >> 3;
    wire_type = tag & 7;

#ifndef NDEBUG
    d->debug_valstart = ptr;
#endif

    if (wire_type == kHpb_WireType_EndGroup) {
      d->end_group = field_number;
      return ptr;
    }

    field = _hpb_Decoder_FindField(d, layout, field_number, &last_field_index);
    ptr = _hpb_Decoder_DecodeWireValue(d, ptr, layout, field, wire_type, &val,
                                       &op);

    if (op >= 0) {
      ptr = _hpb_Decoder_DecodeKnownField(d, ptr, msg, layout, field, op, &val);
    } else {
      switch (op) {
        case kHpb_DecodeOp_UnknownField:
          ptr = _hpb_Decoder_DecodeUnknownField(d, ptr, msg, field_number,
                                                wire_type, val);
          break;
        case kHpb_DecodeOp_MessageSetItem:
          ptr = hpb_Decoder_DecodeMessageSetItem(d, ptr, msg, layout);
          break;
      }
    }
  }

  return HPB_UNLIKELY(layout && layout->required_count)
             ? _hpb_Decoder_CheckRequired(d, ptr, msg, layout)
             : ptr;
}

const char* _hpb_FastDecoder_DecodeGeneric(struct hpb_Decoder* d,
                                           const char* ptr, hpb_Message* msg,
                                           intptr_t table, uint64_t hasbits,
                                           uint64_t data) {
  (void)data;
  *(uint32_t*)msg |= hasbits;
  return _hpb_Decoder_DecodeMessage(d, ptr, msg, decode_totablep(table));
}

static hpb_DecodeStatus _hpb_Decoder_DecodeTop(struct hpb_Decoder* d,
                                               const char* buf, void* msg,
                                               const hpb_MiniTable* l) {
  if (!_hpb_Decoder_TryFastDispatch(d, &buf, msg, l)) {
    _hpb_Decoder_DecodeMessage(d, buf, msg, l);
  }
  if (d->end_group != DECODE_NOGROUP) return kHpb_DecodeStatus_Malformed;
  if (d->missing_required) return kHpb_DecodeStatus_MissingRequired;
  return kHpb_DecodeStatus_Ok;
}

HPB_NOINLINE
const char* _hpb_Decoder_IsDoneFallback(hpb_EpsCopyInputStream* e,
                                        const char* ptr, int overrun) {
  return _hpb_EpsCopyInputStream_IsDoneFallbackInline(
      e, ptr, overrun, _hpb_Decoder_BufferFlipCallback);
}

static hpb_DecodeStatus hpb_Decoder_Decode(hpb_Decoder* const decoder,
                                           const char* const buf,
                                           void* const msg,
                                           const hpb_MiniTable* const l,
                                           hpb_Arena* const arena) {
  if (HPB_SETJMP(decoder->err) == 0) {
    decoder->status = _hpb_Decoder_DecodeTop(decoder, buf, msg, l);
  } else {
    HPB_ASSERT(decoder->status != kHpb_DecodeStatus_Ok);
  }

  _hpb_MemBlock* blocks =
      hpb_Atomic_Load(&decoder->arena.blocks, memory_order_relaxed);
  arena->head = decoder->arena.head;
  hpb_Atomic_Store(&arena->blocks, blocks, memory_order_relaxed);
  return decoder->status;
}

hpb_DecodeStatus hpb_Decode(const char* buf, size_t size, void* msg,
                            const hpb_MiniTable* l,
                            const hpb_ExtensionRegistry* extreg, int options,
                            hpb_Arena* arena) {
  hpb_Decoder decoder;
  unsigned depth = (unsigned)options >> 16;

  hpb_EpsCopyInputStream_Init(&decoder.input, &buf, size,
                              options & kHpb_DecodeOption_AliasString);

  decoder.extreg = extreg;
  decoder.unknown = NULL;
  decoder.depth = depth ? depth : kHpb_WireFormat_DefaultDepthLimit;
  decoder.end_group = DECODE_NOGROUP;
  decoder.options = (uint16_t)options;
  decoder.missing_required = false;
  decoder.status = kHpb_DecodeStatus_Ok;

  // Violating the encapsulation of the arena for performance reasons.
  // This is a temporary arena that we swap into and swap out of when we are
  // done.  The temporary arena only needs to be able to handle allocation,
  // not fuse or free, so it does not need many of the members to be initialized
  // (particularly parent_or_count).
  _hpb_MemBlock* blocks = hpb_Atomic_Load(&arena->blocks, memory_order_relaxed);
  decoder.arena.head = arena->head;
  decoder.arena.block_alloc = arena->block_alloc;
  hpb_Atomic_Init(&decoder.arena.blocks, blocks);

  return hpb_Decoder_Decode(&decoder, buf, msg, l, arena);
}

#undef OP_FIXPCK_LG2
#undef OP_VARPCK_LG2
