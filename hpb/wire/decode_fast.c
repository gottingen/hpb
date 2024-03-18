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

// Fast decoder: ~3x the speed of decode.c, but requires x86-64/ARM64.
// Also the table size grows by 2x.
//
// Could potentially be ported to other 64-bit archs that pass at least six
// arguments in registers and have 8 unused high bits in pointers.
//
// The overall design is to create specialized functions for every possible
// field type (eg. oneof boolean field with a 1 byte tag) and then dispatch
// to the specialized function as quickly as possible.

#include "hpb/wire/decode_fast.h"

#include "hpb/collections/internal/array.h"
#include "hpb/wire/internal/decode.h"

// Must be last.
#include "hpb/port/def.inc"

#if HPB_FASTTABLE

// The standard set of arguments passed to each parsing function.
// Thanks to x86-64 calling conventions, these will stay in registers.
#define HPB_PARSE_PARAMS                                             \
  hpb_Decoder *d, const char *ptr, hpb_Message *msg, intptr_t table, \
      uint64_t hasbits, uint64_t data

#define HPB_PARSE_ARGS d, ptr, msg, table, hasbits, data

#define RETURN_GENERIC(m)                                 \
  /* Uncomment either of these for debugging purposes. */ \
  /* fprintf(stderr, m); */                               \
  /*__builtin_trap(); */                                  \
  return _hpb_FastDecoder_DecodeGeneric(d, ptr, msg, table, hasbits, 0);

typedef enum {
  CARD_s = 0, /* Singular (optional, non-repeated) */
  CARD_o = 1, /* Oneof */
  CARD_r = 2, /* Repeated */
  CARD_p = 3  /* Packed Repeated */
} hpb_card;

HPB_NOINLINE
static const char* fastdecode_isdonefallback(HPB_PARSE_PARAMS) {
  int overrun = data;
  ptr = _hpb_EpsCopyInputStream_IsDoneFallbackInline(
      &d->input, ptr, overrun, _hpb_Decoder_BufferFlipCallback);
  data = _hpb_FastDecoder_LoadTag(ptr);
  HPB_MUSTTAIL return _hpb_FastDecoder_TagDispatch(HPB_PARSE_ARGS);
}

HPB_FORCEINLINE
static const char* fastdecode_dispatch(HPB_PARSE_PARAMS) {
  int overrun;
  switch (hpb_EpsCopyInputStream_IsDoneStatus(&d->input, ptr, &overrun)) {
    case kHpb_IsDoneStatus_Done:
      *(uint32_t*)msg |= hasbits;  // Sync hasbits.
      const hpb_MiniTable* l = decode_totablep(table);
      return HPB_UNLIKELY(l->required_count)
                 ? _hpb_Decoder_CheckRequired(d, ptr, msg, l)
                 : ptr;
    case kHpb_IsDoneStatus_NotDone:
      break;
    case kHpb_IsDoneStatus_NeedFallback:
      data = overrun;
      HPB_MUSTTAIL return fastdecode_isdonefallback(HPB_PARSE_ARGS);
  }

  // Read two bytes of tag data (for a one-byte tag, the high byte is junk).
  data = _hpb_FastDecoder_LoadTag(ptr);
  HPB_MUSTTAIL return _hpb_FastDecoder_TagDispatch(HPB_PARSE_ARGS);
}

HPB_FORCEINLINE
static bool fastdecode_checktag(uint16_t data, int tagbytes) {
  if (tagbytes == 1) {
    return (data & 0xff) == 0;
  } else {
    return data == 0;
  }
}

HPB_FORCEINLINE
static const char* fastdecode_longsize(const char* ptr, int* size) {
  int i;
  HPB_ASSERT(*size & 0x80);
  *size &= 0xff;
  for (i = 0; i < 3; i++) {
    ptr++;
    size_t byte = (uint8_t)ptr[-1];
    *size += (byte - 1) << (7 + 7 * i);
    if (HPB_LIKELY((byte & 0x80) == 0)) return ptr;
  }
  ptr++;
  size_t byte = (uint8_t)ptr[-1];
  // len is limited by 2gb not 4gb, hence 8 and not 16 as normally expected
  // for a 32 bit varint.
  if (HPB_UNLIKELY(byte >= 8)) return NULL;
  *size += (byte - 1) << 28;
  return ptr;
}

HPB_FORCEINLINE
static const char* fastdecode_delimited(
    hpb_Decoder* d, const char* ptr,
    hpb_EpsCopyInputStream_ParseDelimitedFunc* func, void* ctx) {
  ptr++;

  // Sign-extend so varint greater than one byte becomes negative, causing
  // fast delimited parse to fail.
  int len = (int8_t)ptr[-1];

  if (!hpb_EpsCopyInputStream_TryParseDelimitedFast(&d->input, &ptr, len, func,
                                                    ctx)) {
    // Slow case: Sub-message is >=128 bytes and/or exceeds the current buffer.
    // If it exceeds the buffer limit, limit/limit_ptr will change during
    // sub-message parsing, so we need to preserve delta, not limit.
    if (HPB_UNLIKELY(len & 0x80)) {
      // Size varint >1 byte (length >= 128).
      ptr = fastdecode_longsize(ptr, &len);
      if (!ptr) {
        // Corrupt wire format: size exceeded INT_MAX.
        return NULL;
      }
    }
    if (!hpb_EpsCopyInputStream_CheckSize(&d->input, ptr, len)) {
      // Corrupt wire format: invalid limit.
      return NULL;
    }
    int delta = hpb_EpsCopyInputStream_PushLimit(&d->input, ptr, len);
    ptr = func(&d->input, ptr, ctx);
    hpb_EpsCopyInputStream_PopLimit(&d->input, ptr, delta);
  }
  return ptr;
}

/* singular, oneof, repeated field handling ***********************************/

typedef struct {
  hpb_Array* arr;
  void* end;
} fastdecode_arr;

typedef enum {
  FD_NEXT_ATLIMIT,
  FD_NEXT_SAMEFIELD,
  FD_NEXT_OTHERFIELD
} fastdecode_next;

typedef struct {
  void* dst;
  fastdecode_next next;
  uint32_t tag;
} fastdecode_nextret;

HPB_FORCEINLINE
static void* fastdecode_resizearr(hpb_Decoder* d, void* dst,
                                  fastdecode_arr* farr, int valbytes) {
  if (HPB_UNLIKELY(dst == farr->end)) {
    size_t old_size = farr->arr->capacity;
    size_t old_bytes = old_size * valbytes;
    size_t new_size = old_size * 2;
    size_t new_bytes = new_size * valbytes;
    char* old_ptr = _hpb_array_ptr(farr->arr);
    char* new_ptr = hpb_Arena_Realloc(&d->arena, old_ptr, old_bytes, new_bytes);
    uint8_t elem_size_lg2 = __builtin_ctz(valbytes);
    farr->arr->capacity = new_size;
    farr->arr->data = _hpb_array_tagptr(new_ptr, elem_size_lg2);
    dst = (void*)(new_ptr + (old_size * valbytes));
    farr->end = (void*)(new_ptr + (new_size * valbytes));
  }
  return dst;
}

HPB_FORCEINLINE
static bool fastdecode_tagmatch(uint32_t tag, uint64_t data, int tagbytes) {
  if (tagbytes == 1) {
    return (uint8_t)tag == (uint8_t)data;
  } else {
    return (uint16_t)tag == (uint16_t)data;
  }
}

HPB_FORCEINLINE
static void fastdecode_commitarr(void* dst, fastdecode_arr* farr,
                                 int valbytes) {
  farr->arr->size =
      (size_t)((char*)dst - (char*)_hpb_array_ptr(farr->arr)) / valbytes;
}

HPB_FORCEINLINE
static fastdecode_nextret fastdecode_nextrepeated(hpb_Decoder* d, void* dst,
                                                  const char** ptr,
                                                  fastdecode_arr* farr,
                                                  uint64_t data, int tagbytes,
                                                  int valbytes) {
  fastdecode_nextret ret;
  dst = (char*)dst + valbytes;

  if (HPB_LIKELY(!_hpb_Decoder_IsDone(d, ptr))) {
    ret.tag = _hpb_FastDecoder_LoadTag(*ptr);
    if (fastdecode_tagmatch(ret.tag, data, tagbytes)) {
      ret.next = FD_NEXT_SAMEFIELD;
    } else {
      fastdecode_commitarr(dst, farr, valbytes);
      ret.next = FD_NEXT_OTHERFIELD;
    }
  } else {
    fastdecode_commitarr(dst, farr, valbytes);
    ret.next = FD_NEXT_ATLIMIT;
  }

  ret.dst = dst;
  return ret;
}

HPB_FORCEINLINE
static void* fastdecode_fieldmem(hpb_Message* msg, uint64_t data) {
  size_t ofs = data >> 48;
  return (char*)msg + ofs;
}

HPB_FORCEINLINE
static void* fastdecode_getfield(hpb_Decoder* d, const char* ptr,
                                 hpb_Message* msg, uint64_t* data,
                                 uint64_t* hasbits, fastdecode_arr* farr,
                                 int valbytes, hpb_card card) {
  switch (card) {
    case CARD_s: {
      uint8_t hasbit_index = *data >> 24;
      // Set hasbit and return pointer to scalar field.
      *hasbits |= 1ull << hasbit_index;
      return fastdecode_fieldmem(msg, *data);
    }
    case CARD_o: {
      uint16_t case_ofs = *data >> 32;
      uint32_t* oneof_case = HPB_PTR_AT(msg, case_ofs, uint32_t);
      uint8_t field_number = *data >> 24;
      *oneof_case = field_number;
      return fastdecode_fieldmem(msg, *data);
    }
    case CARD_r: {
      // Get pointer to hpb_Array and allocate/expand if necessary.
      uint8_t elem_size_lg2 = __builtin_ctz(valbytes);
      hpb_Array** arr_p = fastdecode_fieldmem(msg, *data);
      char* begin;
      *(uint32_t*)msg |= *hasbits;
      *hasbits = 0;
      if (HPB_LIKELY(!*arr_p)) {
        farr->arr = _hpb_Array_New(&d->arena, 8, elem_size_lg2);
        *arr_p = farr->arr;
      } else {
        farr->arr = *arr_p;
      }
      begin = _hpb_array_ptr(farr->arr);
      farr->end = begin + (farr->arr->capacity * valbytes);
      *data = _hpb_FastDecoder_LoadTag(ptr);
      return begin + (farr->arr->size * valbytes);
    }
    default:
      HPB_UNREACHABLE();
  }
}

HPB_FORCEINLINE
static bool fastdecode_flippacked(uint64_t* data, int tagbytes) {
  *data ^= (0x2 ^ 0x0);  // Patch data to match packed wiretype.
  return fastdecode_checktag(*data, tagbytes);
}

#define FASTDECODE_CHECKPACKED(tagbytes, card, func)                \
  if (HPB_UNLIKELY(!fastdecode_checktag(data, tagbytes))) {         \
    if (card == CARD_r && fastdecode_flippacked(&data, tagbytes)) { \
      HPB_MUSTTAIL return func(HPB_PARSE_ARGS);                     \
    }                                                               \
    RETURN_GENERIC("packed check tag mismatch\n");                  \
  }

/* varint fields **************************************************************/

HPB_FORCEINLINE
static uint64_t fastdecode_munge(uint64_t val, int valbytes, bool zigzag) {
  if (valbytes == 1) {
    return val != 0;
  } else if (zigzag) {
    if (valbytes == 4) {
      uint32_t n = val;
      return (n >> 1) ^ -(int32_t)(n & 1);
    } else if (valbytes == 8) {
      return (val >> 1) ^ -(int64_t)(val & 1);
    }
    HPB_UNREACHABLE();
  }
  return val;
}

HPB_FORCEINLINE
static const char* fastdecode_varint64(const char* ptr, uint64_t* val) {
  ptr++;
  *val = (uint8_t)ptr[-1];
  if (HPB_UNLIKELY(*val & 0x80)) {
    int i;
    for (i = 0; i < 8; i++) {
      ptr++;
      uint64_t byte = (uint8_t)ptr[-1];
      *val += (byte - 1) << (7 + 7 * i);
      if (HPB_LIKELY((byte & 0x80) == 0)) goto done;
    }
    ptr++;
    uint64_t byte = (uint8_t)ptr[-1];
    if (byte > 1) {
      return NULL;
    }
    *val += (byte - 1) << 63;
  }
done:
  HPB_ASSUME(ptr != NULL);
  return ptr;
}

#define FASTDECODE_UNPACKEDVARINT(d, ptr, msg, table, hasbits, data, tagbytes, \
                                  valbytes, card, zigzag, packed)              \
  uint64_t val;                                                                \
  void* dst;                                                                   \
  fastdecode_arr farr;                                                         \
                                                                               \
  FASTDECODE_CHECKPACKED(tagbytes, card, packed);                              \
                                                                               \
  dst = fastdecode_getfield(d, ptr, msg, &data, &hasbits, &farr, valbytes,     \
                            card);                                             \
  if (card == CARD_r) {                                                        \
    if (HPB_UNLIKELY(!dst)) {                                                  \
      RETURN_GENERIC("need array resize\n");                                   \
    }                                                                          \
  }                                                                            \
                                                                               \
  again:                                                                       \
  if (card == CARD_r) {                                                        \
    dst = fastdecode_resizearr(d, dst, &farr, valbytes);                       \
  }                                                                            \
                                                                               \
  ptr += tagbytes;                                                             \
  ptr = fastdecode_varint64(ptr, &val);                                        \
  if (ptr == NULL) _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);  \
  val = fastdecode_munge(val, valbytes, zigzag);                               \
  memcpy(dst, &val, valbytes);                                                 \
                                                                               \
  if (card == CARD_r) {                                                        \
    fastdecode_nextret ret = fastdecode_nextrepeated(                          \
        d, dst, &ptr, &farr, data, tagbytes, valbytes);                        \
    switch (ret.next) {                                                        \
      case FD_NEXT_SAMEFIELD:                                                  \
        dst = ret.dst;                                                         \
        goto again;                                                            \
      case FD_NEXT_OTHERFIELD:                                                 \
        data = ret.tag;                                                        \
        HPB_MUSTTAIL return _hpb_FastDecoder_TagDispatch(HPB_PARSE_ARGS);      \
      case FD_NEXT_ATLIMIT:                                                    \
        return ptr;                                                            \
    }                                                                          \
  }                                                                            \
                                                                               \
  HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);

typedef struct {
  uint8_t valbytes;
  bool zigzag;
  void* dst;
  fastdecode_arr farr;
} fastdecode_varintdata;

HPB_FORCEINLINE
static const char* fastdecode_topackedvarint(hpb_EpsCopyInputStream* e,
                                             const char* ptr, void* ctx) {
  hpb_Decoder* d = (hpb_Decoder*)e;
  fastdecode_varintdata* data = ctx;
  void* dst = data->dst;
  uint64_t val;

  while (!_hpb_Decoder_IsDone(d, &ptr)) {
    dst = fastdecode_resizearr(d, dst, &data->farr, data->valbytes);
    ptr = fastdecode_varint64(ptr, &val);
    if (ptr == NULL) return NULL;
    val = fastdecode_munge(val, data->valbytes, data->zigzag);
    memcpy(dst, &val, data->valbytes);
    dst = (char*)dst + data->valbytes;
  }

  fastdecode_commitarr(dst, &data->farr, data->valbytes);
  return ptr;
}

#define FASTDECODE_PACKEDVARINT(d, ptr, msg, table, hasbits, data, tagbytes, \
                                valbytes, zigzag, unpacked)                  \
  fastdecode_varintdata ctx = {valbytes, zigzag};                            \
                                                                             \
  FASTDECODE_CHECKPACKED(tagbytes, CARD_r, unpacked);                        \
                                                                             \
  ctx.dst = fastdecode_getfield(d, ptr, msg, &data, &hasbits, &ctx.farr,     \
                                valbytes, CARD_r);                           \
  if (HPB_UNLIKELY(!ctx.dst)) {                                              \
    RETURN_GENERIC("need array resize\n");                                   \
  }                                                                          \
                                                                             \
  ptr += tagbytes;                                                           \
  ptr = fastdecode_delimited(d, ptr, &fastdecode_topackedvarint, &ctx);      \
                                                                             \
  if (HPB_UNLIKELY(ptr == NULL)) {                                           \
    _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);               \
  }                                                                          \
                                                                             \
  HPB_MUSTTAIL return fastdecode_dispatch(d, ptr, msg, table, hasbits, 0);

#define FASTDECODE_VARINT(d, ptr, msg, table, hasbits, data, tagbytes,     \
                          valbytes, card, zigzag, unpacked, packed)        \
  if (card == CARD_p) {                                                    \
    FASTDECODE_PACKEDVARINT(d, ptr, msg, table, hasbits, data, tagbytes,   \
                            valbytes, zigzag, unpacked);                   \
  } else {                                                                 \
    FASTDECODE_UNPACKEDVARINT(d, ptr, msg, table, hasbits, data, tagbytes, \
                              valbytes, card, zigzag, packed);             \
  }

#define z_ZZ true
#define b_ZZ false
#define v_ZZ false

/* Generate all combinations:
 * {s,o,r,p} x {b1,v4,z4,v8,z8} x {1bt,2bt} */

#define F(card, type, valbytes, tagbytes)                                      \
  HPB_NOINLINE                                                                 \
  const char* hpb_p##card##type##valbytes##_##tagbytes##bt(HPB_PARSE_PARAMS) { \
    FASTDECODE_VARINT(d, ptr, msg, table, hasbits, data, tagbytes, valbytes,   \
                      CARD_##card, type##_ZZ,                                  \
                      hpb_pr##type##valbytes##_##tagbytes##bt,                 \
                      hpb_pp##type##valbytes##_##tagbytes##bt);                \
  }

#define TYPES(card, tagbytes) \
  F(card, b, 1, tagbytes)     \
  F(card, v, 4, tagbytes)     \
  F(card, v, 8, tagbytes)     \
  F(card, z, 4, tagbytes)     \
  F(card, z, 8, tagbytes)

#define TAGBYTES(card) \
  TYPES(card, 1)       \
  TYPES(card, 2)

TAGBYTES(s)
TAGBYTES(o)
TAGBYTES(r)
TAGBYTES(p)

#undef z_ZZ
#undef b_ZZ
#undef v_ZZ
#undef o_ONEOF
#undef s_ONEOF
#undef r_ONEOF
#undef F
#undef TYPES
#undef TAGBYTES
#undef FASTDECODE_UNPACKEDVARINT
#undef FASTDECODE_PACKEDVARINT
#undef FASTDECODE_VARINT

/* fixed fields ***************************************************************/

#define FASTDECODE_UNPACKEDFIXED(d, ptr, msg, table, hasbits, data, tagbytes, \
                                 valbytes, card, packed)                      \
  void* dst;                                                                  \
  fastdecode_arr farr;                                                        \
                                                                              \
  FASTDECODE_CHECKPACKED(tagbytes, card, packed)                              \
                                                                              \
  dst = fastdecode_getfield(d, ptr, msg, &data, &hasbits, &farr, valbytes,    \
                            card);                                            \
  if (card == CARD_r) {                                                       \
    if (HPB_UNLIKELY(!dst)) {                                                 \
      RETURN_GENERIC("couldn't allocate array in arena\n");                   \
    }                                                                         \
  }                                                                           \
                                                                              \
  again:                                                                      \
  if (card == CARD_r) {                                                       \
    dst = fastdecode_resizearr(d, dst, &farr, valbytes);                      \
  }                                                                           \
                                                                              \
  ptr += tagbytes;                                                            \
  memcpy(dst, ptr, valbytes);                                                 \
  ptr += valbytes;                                                            \
                                                                              \
  if (card == CARD_r) {                                                       \
    fastdecode_nextret ret = fastdecode_nextrepeated(                         \
        d, dst, &ptr, &farr, data, tagbytes, valbytes);                       \
    switch (ret.next) {                                                       \
      case FD_NEXT_SAMEFIELD:                                                 \
        dst = ret.dst;                                                        \
        goto again;                                                           \
      case FD_NEXT_OTHERFIELD:                                                \
        data = ret.tag;                                                       \
        HPB_MUSTTAIL return _hpb_FastDecoder_TagDispatch(HPB_PARSE_ARGS);     \
      case FD_NEXT_ATLIMIT:                                                   \
        return ptr;                                                           \
    }                                                                         \
  }                                                                           \
                                                                              \
  HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);

#define FASTDECODE_PACKEDFIXED(d, ptr, msg, table, hasbits, data, tagbytes, \
                               valbytes, unpacked)                          \
  FASTDECODE_CHECKPACKED(tagbytes, CARD_r, unpacked)                        \
                                                                            \
  ptr += tagbytes;                                                          \
  int size = (uint8_t)ptr[0];                                               \
  ptr++;                                                                    \
  if (size & 0x80) {                                                        \
    ptr = fastdecode_longsize(ptr, &size);                                  \
  }                                                                         \
                                                                            \
  if (HPB_UNLIKELY(!hpb_EpsCopyInputStream_CheckDataSizeAvailable(          \
                       &d->input, ptr, size) ||                             \
                   (size % valbytes) != 0)) {                               \
    _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);              \
  }                                                                         \
                                                                            \
  hpb_Array** arr_p = fastdecode_fieldmem(msg, data);                       \
  hpb_Array* arr = *arr_p;                                                  \
  uint8_t elem_size_lg2 = __builtin_ctz(valbytes);                          \
  int elems = size / valbytes;                                              \
                                                                            \
  if (HPB_LIKELY(!arr)) {                                                   \
    *arr_p = arr = _hpb_Array_New(&d->arena, elems, elem_size_lg2);         \
    if (!arr) {                                                             \
      _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);            \
    }                                                                       \
  } else {                                                                  \
    _hpb_Array_ResizeUninitialized(arr, elems, &d->arena);                  \
  }                                                                         \
                                                                            \
  char* dst = _hpb_array_ptr(arr);                                          \
  memcpy(dst, ptr, size);                                                   \
  arr->size = elems;                                                        \
                                                                            \
  ptr += size;                                                              \
  HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);

#define FASTDECODE_FIXED(d, ptr, msg, table, hasbits, data, tagbytes,     \
                         valbytes, card, unpacked, packed)                \
  if (card == CARD_p) {                                                   \
    FASTDECODE_PACKEDFIXED(d, ptr, msg, table, hasbits, data, tagbytes,   \
                           valbytes, unpacked);                           \
  } else {                                                                \
    FASTDECODE_UNPACKEDFIXED(d, ptr, msg, table, hasbits, data, tagbytes, \
                             valbytes, card, packed);                     \
  }

/* Generate all combinations:
 * {s,o,r,p} x {f4,f8} x {1bt,2bt} */

#define F(card, valbytes, tagbytes)                                         \
  HPB_NOINLINE                                                              \
  const char* hpb_p##card##f##valbytes##_##tagbytes##bt(HPB_PARSE_PARAMS) { \
    FASTDECODE_FIXED(d, ptr, msg, table, hasbits, data, tagbytes, valbytes, \
                     CARD_##card, hpb_ppf##valbytes##_##tagbytes##bt,       \
                     hpb_prf##valbytes##_##tagbytes##bt);                   \
  }

#define TYPES(card, tagbytes) \
  F(card, 4, tagbytes)        \
  F(card, 8, tagbytes)

#define TAGBYTES(card) \
  TYPES(card, 1)       \
  TYPES(card, 2)

TAGBYTES(s)
TAGBYTES(o)
TAGBYTES(r)
TAGBYTES(p)

#undef F
#undef TYPES
#undef TAGBYTES
#undef FASTDECODE_UNPACKEDFIXED
#undef FASTDECODE_PACKEDFIXED

/* string fields **************************************************************/

typedef const char* fastdecode_copystr_func(struct hpb_Decoder* d,
                                            const char* ptr, hpb_Message* msg,
                                            const hpb_MiniTable* table,
                                            uint64_t hasbits,
                                            hpb_StringView* dst);

HPB_NOINLINE
static const char* fastdecode_verifyutf8(hpb_Decoder* d, const char* ptr,
                                         hpb_Message* msg, intptr_t table,
                                         uint64_t hasbits, uint64_t data) {
  hpb_StringView* dst = (hpb_StringView*)data;
  if (!_hpb_Decoder_VerifyUtf8Inline(dst->data, dst->size)) {
    _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_BadUtf8);
  }
  HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);
}

#define FASTDECODE_LONGSTRING(d, ptr, msg, table, hasbits, dst, validate_utf8) \
  int size = (uint8_t)ptr[0]; /* Could plumb through hasbits. */               \
  ptr++;                                                                       \
  if (size & 0x80) {                                                           \
    ptr = fastdecode_longsize(ptr, &size);                                     \
  }                                                                            \
                                                                               \
  if (HPB_UNLIKELY(!hpb_EpsCopyInputStream_CheckSize(&d->input, ptr, size))) { \
    dst->size = 0;                                                             \
    _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);                 \
  }                                                                            \
                                                                               \
  const char* s_ptr = ptr;                                                     \
  ptr = hpb_EpsCopyInputStream_ReadString(&d->input, &s_ptr, size, &d->arena); \
  if (!ptr) _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);       \
  dst->data = s_ptr;                                                           \
  dst->size = size;                                                            \
                                                                               \
  if (validate_utf8) {                                                         \
    data = (uint64_t)dst;                                                      \
    HPB_MUSTTAIL return fastdecode_verifyutf8(HPB_PARSE_ARGS);                 \
  } else {                                                                     \
    HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);                   \
  }

HPB_NOINLINE
static const char* fastdecode_longstring_utf8(struct hpb_Decoder* d,
                                              const char* ptr, hpb_Message* msg,
                                              intptr_t table, uint64_t hasbits,
                                              uint64_t data) {
  hpb_StringView* dst = (hpb_StringView*)data;
  FASTDECODE_LONGSTRING(d, ptr, msg, table, hasbits, dst, true);
}

HPB_NOINLINE
static const char* fastdecode_longstring_noutf8(
    struct hpb_Decoder* d, const char* ptr, hpb_Message* msg, intptr_t table,
    uint64_t hasbits, uint64_t data) {
  hpb_StringView* dst = (hpb_StringView*)data;
  FASTDECODE_LONGSTRING(d, ptr, msg, table, hasbits, dst, false);
}

HPB_FORCEINLINE
static void fastdecode_docopy(hpb_Decoder* d, const char* ptr, uint32_t size,
                              int copy, char* data, hpb_StringView* dst) {
  d->arena.head.ptr += copy;
  dst->data = data;
  HPB_UNPOISON_MEMORY_REGION(data, copy);
  memcpy(data, ptr, copy);
  HPB_POISON_MEMORY_REGION(data + size, copy - size);
}

#define FASTDECODE_COPYSTRING(d, ptr, msg, table, hasbits, data, tagbytes,     \
                              card, validate_utf8)                             \
  hpb_StringView* dst;                                                         \
  fastdecode_arr farr;                                                         \
  int64_t size;                                                                \
  size_t arena_has;                                                            \
  size_t common_has;                                                           \
  char* buf;                                                                   \
                                                                               \
  HPB_ASSERT(!hpb_EpsCopyInputStream_AliasingAvailable(&d->input, ptr, 0));    \
  HPB_ASSERT(fastdecode_checktag(data, tagbytes));                             \
                                                                               \
  dst = fastdecode_getfield(d, ptr, msg, &data, &hasbits, &farr,               \
                            sizeof(hpb_StringView), card);                     \
                                                                               \
  again:                                                                       \
  if (card == CARD_r) {                                                        \
    dst = fastdecode_resizearr(d, dst, &farr, sizeof(hpb_StringView));         \
  }                                                                            \
                                                                               \
  size = (uint8_t)ptr[tagbytes];                                               \
  ptr += tagbytes + 1;                                                         \
  dst->size = size;                                                            \
                                                                               \
  buf = d->arena.head.ptr;                                                     \
  arena_has = _hpb_ArenaHas(&d->arena);                                        \
  common_has = HPB_MIN(arena_has,                                              \
                       hpb_EpsCopyInputStream_BytesAvailable(&d->input, ptr)); \
                                                                               \
  if (HPB_LIKELY(size <= 15 - tagbytes)) {                                     \
    if (arena_has < 16) goto longstr;                                          \
    d->arena.head.ptr += 16;                                                   \
    memcpy(buf, ptr - tagbytes - 1, 16);                                       \
    dst->data = buf + tagbytes + 1;                                            \
  } else if (HPB_LIKELY(size <= 32)) {                                         \
    if (HPB_UNLIKELY(common_has < 32)) goto longstr;                           \
    fastdecode_docopy(d, ptr, size, 32, buf, dst);                             \
  } else if (HPB_LIKELY(size <= 64)) {                                         \
    if (HPB_UNLIKELY(common_has < 64)) goto longstr;                           \
    fastdecode_docopy(d, ptr, size, 64, buf, dst);                             \
  } else if (HPB_LIKELY(size < 128)) {                                         \
    if (HPB_UNLIKELY(common_has < 128)) goto longstr;                          \
    fastdecode_docopy(d, ptr, size, 128, buf, dst);                            \
  } else {                                                                     \
    goto longstr;                                                              \
  }                                                                            \
                                                                               \
  ptr += size;                                                                 \
                                                                               \
  if (card == CARD_r) {                                                        \
    if (validate_utf8 &&                                                       \
        !_hpb_Decoder_VerifyUtf8Inline(dst->data, dst->size)) {                \
      _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_BadUtf8);                 \
    }                                                                          \
    fastdecode_nextret ret = fastdecode_nextrepeated(                          \
        d, dst, &ptr, &farr, data, tagbytes, sizeof(hpb_StringView));          \
    switch (ret.next) {                                                        \
      case FD_NEXT_SAMEFIELD:                                                  \
        dst = ret.dst;                                                         \
        goto again;                                                            \
      case FD_NEXT_OTHERFIELD:                                                 \
        data = ret.tag;                                                        \
        HPB_MUSTTAIL return _hpb_FastDecoder_TagDispatch(HPB_PARSE_ARGS);      \
      case FD_NEXT_ATLIMIT:                                                    \
        return ptr;                                                            \
    }                                                                          \
  }                                                                            \
                                                                               \
  if (card != CARD_r && validate_utf8) {                                       \
    data = (uint64_t)dst;                                                      \
    HPB_MUSTTAIL return fastdecode_verifyutf8(HPB_PARSE_ARGS);                 \
  }                                                                            \
                                                                               \
  HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);                     \
                                                                               \
  longstr:                                                                     \
  if (card == CARD_r) {                                                        \
    fastdecode_commitarr(dst + 1, &farr, sizeof(hpb_StringView));              \
  }                                                                            \
  ptr--;                                                                       \
  if (validate_utf8) {                                                         \
    HPB_MUSTTAIL return fastdecode_longstring_utf8(d, ptr, msg, table,         \
                                                   hasbits, (uint64_t)dst);    \
  } else {                                                                     \
    HPB_MUSTTAIL return fastdecode_longstring_noutf8(d, ptr, msg, table,       \
                                                     hasbits, (uint64_t)dst);  \
  }

#define FASTDECODE_STRING(d, ptr, msg, table, hasbits, data, tagbytes, card,  \
                          copyfunc, validate_utf8)                            \
  hpb_StringView* dst;                                                        \
  fastdecode_arr farr;                                                        \
  int64_t size;                                                               \
                                                                              \
  if (HPB_UNLIKELY(!fastdecode_checktag(data, tagbytes))) {                   \
    RETURN_GENERIC("string field tag mismatch\n");                            \
  }                                                                           \
                                                                              \
  if (HPB_UNLIKELY(                                                           \
          !hpb_EpsCopyInputStream_AliasingAvailable(&d->input, ptr, 0))) {    \
    HPB_MUSTTAIL return copyfunc(HPB_PARSE_ARGS);                             \
  }                                                                           \
                                                                              \
  dst = fastdecode_getfield(d, ptr, msg, &data, &hasbits, &farr,              \
                            sizeof(hpb_StringView), card);                    \
                                                                              \
  again:                                                                      \
  if (card == CARD_r) {                                                       \
    dst = fastdecode_resizearr(d, dst, &farr, sizeof(hpb_StringView));        \
  }                                                                           \
                                                                              \
  size = (int8_t)ptr[tagbytes];                                               \
  ptr += tagbytes + 1;                                                        \
                                                                              \
  if (HPB_UNLIKELY(                                                           \
          !hpb_EpsCopyInputStream_AliasingAvailable(&d->input, ptr, size))) { \
    ptr--;                                                                    \
    if (validate_utf8) {                                                      \
      return fastdecode_longstring_utf8(d, ptr, msg, table, hasbits,          \
                                        (uint64_t)dst);                       \
    } else {                                                                  \
      return fastdecode_longstring_noutf8(d, ptr, msg, table, hasbits,        \
                                          (uint64_t)dst);                     \
    }                                                                         \
  }                                                                           \
                                                                              \
  dst->data = ptr;                                                            \
  dst->size = size;                                                           \
  ptr = hpb_EpsCopyInputStream_ReadStringAliased(&d->input, &dst->data,       \
                                                 dst->size);                  \
                                                                              \
  if (card == CARD_r) {                                                       \
    if (validate_utf8 &&                                                      \
        !_hpb_Decoder_VerifyUtf8Inline(dst->data, dst->size)) {               \
      _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_BadUtf8);                \
    }                                                                         \
    fastdecode_nextret ret = fastdecode_nextrepeated(                         \
        d, dst, &ptr, &farr, data, tagbytes, sizeof(hpb_StringView));         \
    switch (ret.next) {                                                       \
      case FD_NEXT_SAMEFIELD:                                                 \
        dst = ret.dst;                                                        \
        goto again;                                                           \
      case FD_NEXT_OTHERFIELD:                                                \
        data = ret.tag;                                                       \
        HPB_MUSTTAIL return _hpb_FastDecoder_TagDispatch(HPB_PARSE_ARGS);     \
      case FD_NEXT_ATLIMIT:                                                   \
        return ptr;                                                           \
    }                                                                         \
  }                                                                           \
                                                                              \
  if (card != CARD_r && validate_utf8) {                                      \
    data = (uint64_t)dst;                                                     \
    HPB_MUSTTAIL return fastdecode_verifyutf8(HPB_PARSE_ARGS);                \
  }                                                                           \
                                                                              \
  HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);

/* Generate all combinations:
 * {p,c} x {s,o,r} x {s, b} x {1bt,2bt} */

#define s_VALIDATE true
#define b_VALIDATE false

#define F(card, tagbytes, type)                                        \
  HPB_NOINLINE                                                         \
  const char* hpb_c##card##type##_##tagbytes##bt(HPB_PARSE_PARAMS) {   \
    FASTDECODE_COPYSTRING(d, ptr, msg, table, hasbits, data, tagbytes, \
                          CARD_##card, type##_VALIDATE);               \
  }                                                                    \
  const char* hpb_p##card##type##_##tagbytes##bt(HPB_PARSE_PARAMS) {   \
    FASTDECODE_STRING(d, ptr, msg, table, hasbits, data, tagbytes,     \
                      CARD_##card, hpb_c##card##type##_##tagbytes##bt, \
                      type##_VALIDATE);                                \
  }

#define UTF8(card, tagbytes) \
  F(card, tagbytes, s)       \
  F(card, tagbytes, b)

#define TAGBYTES(card) \
  UTF8(card, 1)        \
  UTF8(card, 2)

TAGBYTES(s)
TAGBYTES(o)
TAGBYTES(r)

#undef s_VALIDATE
#undef b_VALIDATE
#undef F
#undef TAGBYTES
#undef FASTDECODE_LONGSTRING
#undef FASTDECODE_COPYSTRING
#undef FASTDECODE_STRING

/* message fields *************************************************************/

HPB_INLINE
hpb_Message* decode_newmsg_ceil(hpb_Decoder* d, const hpb_MiniTable* l,
                                int msg_ceil_bytes) {
  size_t size = l->size + sizeof(hpb_Message_Internal);
  char* msg_data;
  if (HPB_LIKELY(msg_ceil_bytes > 0 &&
                 _hpb_ArenaHas(&d->arena) >= msg_ceil_bytes)) {
    HPB_ASSERT(size <= (size_t)msg_ceil_bytes);
    msg_data = d->arena.head.ptr;
    d->arena.head.ptr += size;
    HPB_UNPOISON_MEMORY_REGION(msg_data, msg_ceil_bytes);
    memset(msg_data, 0, msg_ceil_bytes);
    HPB_POISON_MEMORY_REGION(msg_data + size, msg_ceil_bytes - size);
  } else {
    msg_data = (char*)hpb_Arena_Malloc(&d->arena, size);
    memset(msg_data, 0, size);
  }
  return msg_data + sizeof(hpb_Message_Internal);
}

typedef struct {
  intptr_t table;
  hpb_Message* msg;
} fastdecode_submsgdata;

HPB_FORCEINLINE
static const char* fastdecode_tosubmsg(hpb_EpsCopyInputStream* e,
                                       const char* ptr, void* ctx) {
  hpb_Decoder* d = (hpb_Decoder*)e;
  fastdecode_submsgdata* submsg = ctx;
  ptr = fastdecode_dispatch(d, ptr, submsg->msg, submsg->table, 0, 0);
  HPB_ASSUME(ptr != NULL);
  return ptr;
}

#define FASTDECODE_SUBMSG(d, ptr, msg, table, hasbits, data, tagbytes,    \
                          msg_ceil_bytes, card)                           \
                                                                          \
  if (HPB_UNLIKELY(!fastdecode_checktag(data, tagbytes))) {               \
    RETURN_GENERIC("submessage field tag mismatch\n");                    \
  }                                                                       \
                                                                          \
  if (--d->depth == 0) {                                                  \
    _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_MaxDepthExceeded);     \
  }                                                                       \
                                                                          \
  hpb_Message** dst;                                                      \
  uint32_t submsg_idx = (data >> 16) & 0xff;                              \
  const hpb_MiniTable* tablep = decode_totablep(table);                   \
  const hpb_MiniTable* subtablep = tablep->subs[submsg_idx].submsg;       \
  fastdecode_submsgdata submsg = {decode_totable(subtablep)};             \
  fastdecode_arr farr;                                                    \
                                                                          \
  if (subtablep->table_mask == (uint8_t)-1) {                             \
    RETURN_GENERIC("submessage doesn't have fast tables.");               \
  }                                                                       \
                                                                          \
  dst = fastdecode_getfield(d, ptr, msg, &data, &hasbits, &farr,          \
                            sizeof(hpb_Message*), card);                  \
                                                                          \
  if (card == CARD_s) {                                                   \
    *(uint32_t*)msg |= hasbits;                                           \
    hasbits = 0;                                                          \
  }                                                                       \
                                                                          \
  again:                                                                  \
  if (card == CARD_r) {                                                   \
    dst = fastdecode_resizearr(d, dst, &farr, sizeof(hpb_Message*));      \
  }                                                                       \
                                                                          \
  submsg.msg = *dst;                                                      \
                                                                          \
  if (card == CARD_r || HPB_LIKELY(!submsg.msg)) {                        \
    *dst = submsg.msg = decode_newmsg_ceil(d, subtablep, msg_ceil_bytes); \
  }                                                                       \
                                                                          \
  ptr += tagbytes;                                                        \
  ptr = fastdecode_delimited(d, ptr, fastdecode_tosubmsg, &submsg);       \
                                                                          \
  if (HPB_UNLIKELY(ptr == NULL || d->end_group != DECODE_NOGROUP)) {      \
    _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);            \
  }                                                                       \
                                                                          \
  if (card == CARD_r) {                                                   \
    fastdecode_nextret ret = fastdecode_nextrepeated(                     \
        d, dst, &ptr, &farr, data, tagbytes, sizeof(hpb_Message*));       \
    switch (ret.next) {                                                   \
      case FD_NEXT_SAMEFIELD:                                             \
        dst = ret.dst;                                                    \
        goto again;                                                       \
      case FD_NEXT_OTHERFIELD:                                            \
        d->depth++;                                                       \
        data = ret.tag;                                                   \
        HPB_MUSTTAIL return _hpb_FastDecoder_TagDispatch(HPB_PARSE_ARGS); \
      case FD_NEXT_ATLIMIT:                                               \
        d->depth++;                                                       \
        return ptr;                                                       \
    }                                                                     \
  }                                                                       \
                                                                          \
  d->depth++;                                                             \
  HPB_MUSTTAIL return fastdecode_dispatch(HPB_PARSE_ARGS);

#define F(card, tagbytes, size_ceil, ceil_arg)                               \
  const char* hpb_p##card##m_##tagbytes##bt_max##size_ceil##b(               \
      HPB_PARSE_PARAMS) {                                                    \
    FASTDECODE_SUBMSG(d, ptr, msg, table, hasbits, data, tagbytes, ceil_arg, \
                      CARD_##card);                                          \
  }

#define SIZES(card, tagbytes) \
  F(card, tagbytes, 64, 64)   \
  F(card, tagbytes, 128, 128) \
  F(card, tagbytes, 192, 192) \
  F(card, tagbytes, 256, 256) \
  F(card, tagbytes, max, -1)

#define TAGBYTES(card) \
  SIZES(card, 1)       \
  SIZES(card, 2)

TAGBYTES(s)
TAGBYTES(o)
TAGBYTES(r)

#undef TAGBYTES
#undef SIZES
#undef F
#undef FASTDECODE_SUBMSG

#endif /* HPB_FASTTABLE */
