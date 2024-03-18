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

// We encode backwards, to avoid pre-computing lengths (one-pass encode).

#include "hpb/wire/encode.h"

#include <string.h>

#include "hpb/collections/internal/array.h"
#include "hpb/collections/internal/map_sorter.h"
#include "hpb/message/internal/accessors.h"
#include "hpb/message/internal/extension.h"
#include "hpb/mini_table/sub.h"
#include "hpb/wire/internal/common.h"
#include "hpb/wire/internal/swap.h"

// Must be last.
#include "hpb/port/def.inc"

#define HPB_PB_VARINT_MAX_LEN 10

HPB_NOINLINE
static size_t encode_varint64(uint64_t val, char* buf) {
  size_t i = 0;
  do {
    uint8_t byte = val & 0x7fU;
    val >>= 7;
    if (val) byte |= 0x80U;
    buf[i++] = byte;
  } while (val);
  return i;
}

static uint32_t encode_zz32(int32_t n) {
  return ((uint32_t)n << 1) ^ (n >> 31);
}
static uint64_t encode_zz64(int64_t n) {
  return ((uint64_t)n << 1) ^ (n >> 63);
}

typedef struct {
  hpb_EncodeStatus status;
  jmp_buf err;
  hpb_Arena* arena;
  char *buf, *ptr, *limit;
  int options;
  int depth;
  _hpb_mapsorter sorter;
} hpb_encstate;

static size_t hpb_roundup_pow2(size_t bytes) {
  size_t ret = 128;
  while (ret < bytes) {
    ret *= 2;
  }
  return ret;
}

HPB_NORETURN static void encode_err(hpb_encstate* e, hpb_EncodeStatus s) {
  HPB_ASSERT(s != kHpb_EncodeStatus_Ok);
  e->status = s;
  HPB_LONGJMP(e->err, 1);
}

HPB_NOINLINE
static void encode_growbuffer(hpb_encstate* e, size_t bytes) {
  size_t old_size = e->limit - e->buf;
  size_t new_size = hpb_roundup_pow2(bytes + (e->limit - e->ptr));
  char* new_buf = hpb_Arena_Realloc(e->arena, e->buf, old_size, new_size);

  if (!new_buf) encode_err(e, kHpb_EncodeStatus_OutOfMemory);

  // We want previous data at the end, realloc() put it at the beginning.
  // TODO(salo): This is somewhat inefficient since we are copying twice.
  // Maybe create a realloc() that copies to the end of the new buffer?
  if (old_size > 0) {
    memmove(new_buf + new_size - old_size, e->buf, old_size);
  }

  e->ptr = new_buf + new_size - (e->limit - e->ptr);
  e->limit = new_buf + new_size;
  e->buf = new_buf;

  e->ptr -= bytes;
}

/* Call to ensure that at least "bytes" bytes are available for writing at
 * e->ptr.  Returns false if the bytes could not be allocated. */
HPB_FORCEINLINE
static void encode_reserve(hpb_encstate* e, size_t bytes) {
  if ((size_t)(e->ptr - e->buf) < bytes) {
    encode_growbuffer(e, bytes);
    return;
  }

  e->ptr -= bytes;
}

/* Writes the given bytes to the buffer, handling reserve/advance. */
static void encode_bytes(hpb_encstate* e, const void* data, size_t len) {
  if (len == 0) return; /* memcpy() with zero size is UB */
  encode_reserve(e, len);
  memcpy(e->ptr, data, len);
}

static void encode_fixed64(hpb_encstate* e, uint64_t val) {
  val = _hpb_BigEndian_Swap64(val);
  encode_bytes(e, &val, sizeof(uint64_t));
}

static void encode_fixed32(hpb_encstate* e, uint32_t val) {
  val = _hpb_BigEndian_Swap32(val);
  encode_bytes(e, &val, sizeof(uint32_t));
}

HPB_NOINLINE
static void encode_longvarint(hpb_encstate* e, uint64_t val) {
  size_t len;
  char* start;

  encode_reserve(e, HPB_PB_VARINT_MAX_LEN);
  len = encode_varint64(val, e->ptr);
  start = e->ptr + HPB_PB_VARINT_MAX_LEN - len;
  memmove(start, e->ptr, len);
  e->ptr = start;
}

HPB_FORCEINLINE
static void encode_varint(hpb_encstate* e, uint64_t val) {
  if (val < 128 && e->ptr != e->buf) {
    --e->ptr;
    *e->ptr = val;
  } else {
    encode_longvarint(e, val);
  }
}

static void encode_double(hpb_encstate* e, double d) {
  uint64_t u64;
  HPB_ASSERT(sizeof(double) == sizeof(uint64_t));
  memcpy(&u64, &d, sizeof(uint64_t));
  encode_fixed64(e, u64);
}

static void encode_float(hpb_encstate* e, float d) {
  uint32_t u32;
  HPB_ASSERT(sizeof(float) == sizeof(uint32_t));
  memcpy(&u32, &d, sizeof(uint32_t));
  encode_fixed32(e, u32);
}

static void encode_tag(hpb_encstate* e, uint32_t field_number,
                       uint8_t wire_type) {
  encode_varint(e, (field_number << 3) | wire_type);
}

static void encode_fixedarray(hpb_encstate* e, const hpb_Array* arr,
                              size_t elem_size, uint32_t tag) {
  size_t bytes = arr->size * elem_size;
  const char* data = _hpb_array_constptr(arr);
  const char* ptr = data + bytes - elem_size;

  if (tag || !_hpb_IsLittleEndian()) {
    while (true) {
      if (elem_size == 4) {
        uint32_t val;
        memcpy(&val, ptr, sizeof(val));
        val = _hpb_BigEndian_Swap32(val);
        encode_bytes(e, &val, elem_size);
      } else {
        HPB_ASSERT(elem_size == 8);
        uint64_t val;
        memcpy(&val, ptr, sizeof(val));
        val = _hpb_BigEndian_Swap64(val);
        encode_bytes(e, &val, elem_size);
      }

      if (tag) encode_varint(e, tag);
      if (ptr == data) break;
      ptr -= elem_size;
    }
  } else {
    encode_bytes(e, data, bytes);
  }
}

static void encode_message(hpb_encstate* e, const hpb_Message* msg,
                           const hpb_MiniTable* m, size_t* size);

static void encode_TaggedMessagePtr(hpb_encstate* e,
                                    hpb_TaggedMessagePtr tagged,
                                    const hpb_MiniTable* m, size_t* size) {
  if (hpb_TaggedMessagePtr_IsEmpty(tagged)) {
    m = &_kHpb_MiniTable_Empty;
  }
  encode_message(e, _hpb_TaggedMessagePtr_GetMessage(tagged), m, size);
}

static void encode_scalar(hpb_encstate* e, const void* _field_mem,
                          const hpb_MiniTableSub* subs,
                          const hpb_MiniTableField* f) {
  const char* field_mem = _field_mem;
  int wire_type;

#define CASE(ctype, type, wtype, encodeval) \
  {                                         \
    ctype val = *(ctype*)field_mem;         \
    encode_##type(e, encodeval);            \
    wire_type = wtype;                      \
    break;                                  \
  }

  switch (f->HPB_PRIVATE(descriptortype)) {
    case kHpb_FieldType_Double:
      CASE(double, double, kHpb_WireType_64Bit, val);
    case kHpb_FieldType_Float:
      CASE(float, float, kHpb_WireType_32Bit, val);
    case kHpb_FieldType_Int64:
    case kHpb_FieldType_UInt64:
      CASE(uint64_t, varint, kHpb_WireType_Varint, val);
    case kHpb_FieldType_UInt32:
      CASE(uint32_t, varint, kHpb_WireType_Varint, val);
    case kHpb_FieldType_Int32:
    case kHpb_FieldType_Enum:
      CASE(int32_t, varint, kHpb_WireType_Varint, (int64_t)val);
    case kHpb_FieldType_SFixed64:
    case kHpb_FieldType_Fixed64:
      CASE(uint64_t, fixed64, kHpb_WireType_64Bit, val);
    case kHpb_FieldType_Fixed32:
    case kHpb_FieldType_SFixed32:
      CASE(uint32_t, fixed32, kHpb_WireType_32Bit, val);
    case kHpb_FieldType_Bool:
      CASE(bool, varint, kHpb_WireType_Varint, val);
    case kHpb_FieldType_SInt32:
      CASE(int32_t, varint, kHpb_WireType_Varint, encode_zz32(val));
    case kHpb_FieldType_SInt64:
      CASE(int64_t, varint, kHpb_WireType_Varint, encode_zz64(val));
    case kHpb_FieldType_String:
    case kHpb_FieldType_Bytes: {
      hpb_StringView view = *(hpb_StringView*)field_mem;
      encode_bytes(e, view.data, view.size);
      encode_varint(e, view.size);
      wire_type = kHpb_WireType_Delimited;
      break;
    }
    case kHpb_FieldType_Group: {
      size_t size;
      hpb_TaggedMessagePtr submsg = *(hpb_TaggedMessagePtr*)field_mem;
      const hpb_MiniTable* subm = subs[f->HPB_PRIVATE(submsg_index)].submsg;
      if (submsg == 0) {
        return;
      }
      if (--e->depth == 0) encode_err(e, kHpb_EncodeStatus_MaxDepthExceeded);
      encode_tag(e, f->number, kHpb_WireType_EndGroup);
      encode_TaggedMessagePtr(e, submsg, subm, &size);
      wire_type = kHpb_WireType_StartGroup;
      e->depth++;
      break;
    }
    case kHpb_FieldType_Message: {
      size_t size;
      hpb_TaggedMessagePtr submsg = *(hpb_TaggedMessagePtr*)field_mem;
      const hpb_MiniTable* subm = subs[f->HPB_PRIVATE(submsg_index)].submsg;
      if (submsg == 0) {
        return;
      }
      if (--e->depth == 0) encode_err(e, kHpb_EncodeStatus_MaxDepthExceeded);
      encode_TaggedMessagePtr(e, submsg, subm, &size);
      encode_varint(e, size);
      wire_type = kHpb_WireType_Delimited;
      e->depth++;
      break;
    }
    default:
      HPB_UNREACHABLE();
  }
#undef CASE

  encode_tag(e, f->number, wire_type);
}

static void encode_array(hpb_encstate* e, const hpb_Message* msg,
                         const hpb_MiniTableSub* subs,
                         const hpb_MiniTableField* f) {
  const hpb_Array* arr = *HPB_PTR_AT(msg, f->offset, hpb_Array*);
  bool packed = f->mode & kHpb_LabelFlags_IsPacked;
  size_t pre_len = e->limit - e->ptr;

  if (arr == NULL || arr->size == 0) {
    return;
  }

#define VARINT_CASE(ctype, encode)                                       \
  {                                                                      \
    const ctype* start = _hpb_array_constptr(arr);                       \
    const ctype* ptr = start + arr->size;                                \
    uint32_t tag = packed ? 0 : (f->number << 3) | kHpb_WireType_Varint; \
    do {                                                                 \
      ptr--;                                                             \
      encode_varint(e, encode);                                          \
      if (tag) encode_varint(e, tag);                                    \
    } while (ptr != start);                                              \
  }                                                                      \
  break;

#define TAG(wire_type) (packed ? 0 : (f->number << 3 | wire_type))

  switch (f->HPB_PRIVATE(descriptortype)) {
    case kHpb_FieldType_Double:
      encode_fixedarray(e, arr, sizeof(double), TAG(kHpb_WireType_64Bit));
      break;
    case kHpb_FieldType_Float:
      encode_fixedarray(e, arr, sizeof(float), TAG(kHpb_WireType_32Bit));
      break;
    case kHpb_FieldType_SFixed64:
    case kHpb_FieldType_Fixed64:
      encode_fixedarray(e, arr, sizeof(uint64_t), TAG(kHpb_WireType_64Bit));
      break;
    case kHpb_FieldType_Fixed32:
    case kHpb_FieldType_SFixed32:
      encode_fixedarray(e, arr, sizeof(uint32_t), TAG(kHpb_WireType_32Bit));
      break;
    case kHpb_FieldType_Int64:
    case kHpb_FieldType_UInt64:
      VARINT_CASE(uint64_t, *ptr);
    case kHpb_FieldType_UInt32:
      VARINT_CASE(uint32_t, *ptr);
    case kHpb_FieldType_Int32:
    case kHpb_FieldType_Enum:
      VARINT_CASE(int32_t, (int64_t)*ptr);
    case kHpb_FieldType_Bool:
      VARINT_CASE(bool, *ptr);
    case kHpb_FieldType_SInt32:
      VARINT_CASE(int32_t, encode_zz32(*ptr));
    case kHpb_FieldType_SInt64:
      VARINT_CASE(int64_t, encode_zz64(*ptr));
    case kHpb_FieldType_String:
    case kHpb_FieldType_Bytes: {
      const hpb_StringView* start = _hpb_array_constptr(arr);
      const hpb_StringView* ptr = start + arr->size;
      do {
        ptr--;
        encode_bytes(e, ptr->data, ptr->size);
        encode_varint(e, ptr->size);
        encode_tag(e, f->number, kHpb_WireType_Delimited);
      } while (ptr != start);
      return;
    }
    case kHpb_FieldType_Group: {
      const hpb_TaggedMessagePtr* start = _hpb_array_constptr(arr);
      const hpb_TaggedMessagePtr* ptr = start + arr->size;
      const hpb_MiniTable* subm = subs[f->HPB_PRIVATE(submsg_index)].submsg;
      if (--e->depth == 0) encode_err(e, kHpb_EncodeStatus_MaxDepthExceeded);
      do {
        size_t size;
        ptr--;
        encode_tag(e, f->number, kHpb_WireType_EndGroup);
        encode_TaggedMessagePtr(e, *ptr, subm, &size);
        encode_tag(e, f->number, kHpb_WireType_StartGroup);
      } while (ptr != start);
      e->depth++;
      return;
    }
    case kHpb_FieldType_Message: {
      const hpb_TaggedMessagePtr* start = _hpb_array_constptr(arr);
      const hpb_TaggedMessagePtr* ptr = start + arr->size;
      const hpb_MiniTable* subm = subs[f->HPB_PRIVATE(submsg_index)].submsg;
      if (--e->depth == 0) encode_err(e, kHpb_EncodeStatus_MaxDepthExceeded);
      do {
        size_t size;
        ptr--;
        encode_TaggedMessagePtr(e, *ptr, subm, &size);
        encode_varint(e, size);
        encode_tag(e, f->number, kHpb_WireType_Delimited);
      } while (ptr != start);
      e->depth++;
      return;
    }
  }
#undef VARINT_CASE

  if (packed) {
    encode_varint(e, e->limit - e->ptr - pre_len);
    encode_tag(e, f->number, kHpb_WireType_Delimited);
  }
}

static void encode_mapentry(hpb_encstate* e, uint32_t number,
                            const hpb_MiniTable* layout,
                            const hpb_MapEntry* ent) {
  const hpb_MiniTableField* key_field = &layout->fields[0];
  const hpb_MiniTableField* val_field = &layout->fields[1];
  size_t pre_len = e->limit - e->ptr;
  size_t size;
  encode_scalar(e, &ent->data.v, layout->subs, val_field);
  encode_scalar(e, &ent->data.k, layout->subs, key_field);
  size = (e->limit - e->ptr) - pre_len;
  encode_varint(e, size);
  encode_tag(e, number, kHpb_WireType_Delimited);
}

static void encode_map(hpb_encstate* e, const hpb_Message* msg,
                       const hpb_MiniTableSub* subs,
                       const hpb_MiniTableField* f) {
  const hpb_Map* map = *HPB_PTR_AT(msg, f->offset, const hpb_Map*);
  const hpb_MiniTable* layout = subs[f->HPB_PRIVATE(submsg_index)].submsg;
  HPB_ASSERT(layout->field_count == 2);

  if (map == NULL) return;

  if (e->options & kHpb_EncodeOption_Deterministic) {
    _hpb_sortedmap sorted;
    _hpb_mapsorter_pushmap(&e->sorter,
                           layout->fields[0].HPB_PRIVATE(descriptortype), map,
                           &sorted);
    hpb_MapEntry ent;
    while (_hpb_sortedmap_next(&e->sorter, map, &sorted, &ent)) {
      encode_mapentry(e, f->number, layout, &ent);
    }
    _hpb_mapsorter_popmap(&e->sorter, &sorted);
  } else {
    intptr_t iter = HPB_STRTABLE_BEGIN;
    hpb_StringView key;
    hpb_value val;
    while (hpb_strtable_next2(&map->table, &key, &val, &iter)) {
      hpb_MapEntry ent;
      _hpb_map_fromkey(key, &ent.data.k, map->key_size);
      _hpb_map_fromvalue(val, &ent.data.v, map->val_size);
      encode_mapentry(e, f->number, layout, &ent);
    }
  }
}

static bool encode_shouldencode(hpb_encstate* e, const hpb_Message* msg,
                                const hpb_MiniTableSub* subs,
                                const hpb_MiniTableField* f) {
  if (f->presence == 0) {
    /* Proto3 presence or map/array. */
    const void* mem = HPB_PTR_AT(msg, f->offset, void);
    switch (_hpb_MiniTableField_GetRep(f)) {
      case kHpb_FieldRep_1Byte: {
        char ch;
        memcpy(&ch, mem, 1);
        return ch != 0;
      }
      case kHpb_FieldRep_4Byte: {
        uint32_t u32;
        memcpy(&u32, mem, 4);
        return u32 != 0;
      }
      case kHpb_FieldRep_8Byte: {
        uint64_t u64;
        memcpy(&u64, mem, 8);
        return u64 != 0;
      }
      case kHpb_FieldRep_StringView: {
        const hpb_StringView* str = (const hpb_StringView*)mem;
        return str->size != 0;
      }
      default:
        HPB_UNREACHABLE();
    }
  } else if (f->presence > 0) {
    /* Proto2 presence: hasbit. */
    return _hpb_hasbit_field(msg, f);
  } else {
    /* Field is in a oneof. */
    return _hpb_getoneofcase_field(msg, f) == f->number;
  }
}

static void encode_field(hpb_encstate* e, const hpb_Message* msg,
                         const hpb_MiniTableSub* subs,
                         const hpb_MiniTableField* field) {
  switch (hpb_FieldMode_Get(field)) {
    case kHpb_FieldMode_Array:
      encode_array(e, msg, subs, field);
      break;
    case kHpb_FieldMode_Map:
      encode_map(e, msg, subs, field);
      break;
    case kHpb_FieldMode_Scalar:
      encode_scalar(e, HPB_PTR_AT(msg, field->offset, void), subs, field);
      break;
    default:
      HPB_UNREACHABLE();
  }
}

static void encode_msgset_item(hpb_encstate* e,
                               const hpb_Message_Extension* ext) {
  size_t size;
  encode_tag(e, kHpb_MsgSet_Item, kHpb_WireType_EndGroup);
  encode_message(e, ext->data.ptr, ext->ext->sub.submsg, &size);
  encode_varint(e, size);
  encode_tag(e, kHpb_MsgSet_Message, kHpb_WireType_Delimited);
  encode_varint(e, ext->ext->field.number);
  encode_tag(e, kHpb_MsgSet_TypeId, kHpb_WireType_Varint);
  encode_tag(e, kHpb_MsgSet_Item, kHpb_WireType_StartGroup);
}

static void encode_ext(hpb_encstate* e, const hpb_Message_Extension* ext,
                       bool is_message_set) {
  if (HPB_UNLIKELY(is_message_set)) {
    encode_msgset_item(e, ext);
  } else {
    encode_field(e, &ext->data, &ext->ext->sub, &ext->ext->field);
  }
}

static void encode_message(hpb_encstate* e, const hpb_Message* msg,
                           const hpb_MiniTable* m, size_t* size) {
  size_t pre_len = e->limit - e->ptr;

  if ((e->options & kHpb_EncodeOption_CheckRequired) && m->required_count) {
    uint64_t msg_head;
    memcpy(&msg_head, msg, 8);
    msg_head = _hpb_BigEndian_Swap64(msg_head);
    if (hpb_MiniTable_requiredmask(m) & ~msg_head) {
      encode_err(e, kHpb_EncodeStatus_MissingRequired);
    }
  }

  if ((e->options & kHpb_EncodeOption_SkipUnknown) == 0) {
    size_t unknown_size;
    const char* unknown = hpb_Message_GetUnknown(msg, &unknown_size);

    if (unknown) {
      encode_bytes(e, unknown, unknown_size);
    }
  }

  if (m->ext != kHpb_ExtMode_NonExtendable) {
    /* Encode all extensions together. Unlike C++, we do not attempt to keep
     * these in field number order relative to normal fields or even to each
     * other. */
    size_t ext_count;
    const hpb_Message_Extension* ext = _hpb_Message_Getexts(msg, &ext_count);
    if (ext_count) {
      if (e->options & kHpb_EncodeOption_Deterministic) {
        _hpb_sortedmap sorted;
        _hpb_mapsorter_pushexts(&e->sorter, ext, ext_count, &sorted);
        while (_hpb_sortedmap_nextext(&e->sorter, &sorted, &ext)) {
          encode_ext(e, ext, m->ext == kHpb_ExtMode_IsMessageSet);
        }
        _hpb_mapsorter_popmap(&e->sorter, &sorted);
      } else {
        const hpb_Message_Extension* end = ext + ext_count;
        for (; ext != end; ext++) {
          encode_ext(e, ext, m->ext == kHpb_ExtMode_IsMessageSet);
        }
      }
    }
  }

  if (m->field_count) {
    const hpb_MiniTableField* f = &m->fields[m->field_count];
    const hpb_MiniTableField* first = &m->fields[0];
    while (f != first) {
      f--;
      if (encode_shouldencode(e, msg, m->subs, f)) {
        encode_field(e, msg, m->subs, f);
      }
    }
  }

  *size = (e->limit - e->ptr) - pre_len;
}

static hpb_EncodeStatus hpb_Encoder_Encode(hpb_encstate* const encoder,
                                           const void* const msg,
                                           const hpb_MiniTable* const l,
                                           char** const buf,
                                           size_t* const size) {
  // Unfortunately we must continue to perform hackery here because there are
  // code paths which blindly copy the returned pointer without bothering to
  // check for errors until much later (b/235839510). So we still set *buf to
  // NULL on error and we still set it to non-NULL on a successful empty result.
  if (HPB_SETJMP(encoder->err) == 0) {
    encode_message(encoder, msg, l, size);
    *size = encoder->limit - encoder->ptr;
    if (*size == 0) {
      static char ch;
      *buf = &ch;
    } else {
      HPB_ASSERT(encoder->ptr);
      *buf = encoder->ptr;
    }
  } else {
    HPB_ASSERT(encoder->status != kHpb_EncodeStatus_Ok);
    *buf = NULL;
    *size = 0;
  }

  _hpb_mapsorter_destroy(&encoder->sorter);
  return encoder->status;
}

hpb_EncodeStatus hpb_Encode(const void* msg, const hpb_MiniTable* l,
                            int options, hpb_Arena* arena, char** buf,
                            size_t* size) {
  hpb_encstate e;
  unsigned depth = (unsigned)options >> 16;

  e.status = kHpb_EncodeStatus_Ok;
  e.arena = arena;
  e.buf = NULL;
  e.limit = NULL;
  e.ptr = NULL;
  e.depth = depth ? depth : kHpb_WireFormat_DefaultDepthLimit;
  e.options = options;
  _hpb_mapsorter_init(&e.sorter);

  return hpb_Encoder_Encode(&e, msg, l, buf, size);
}
