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

#ifndef HPB_WIRE_READER_H_
#define HPB_WIRE_READER_H_

#include "hpb/wire/eps_copy_input_stream.h"
#include "hpb/wire/internal/swap.h"
#include "hpb/wire/types.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// The hpb_WireReader interface is suitable for general-purpose parsing of
// protobuf binary wire format.  It is designed to be used along with
// hpb_EpsCopyInputStream for buffering, and all parsing routines in this file
// assume that at least kHpb_EpsCopyInputStream_SlopBytes worth of data is
// available to read without any bounds checks.

#define kHpb_WireReader_WireTypeMask 7
#define kHpb_WireReader_WireTypeBits 3

typedef struct {
  const char* ptr;
  uint64_t val;
} _hpb_WireReader_ReadLongVarintRet;

_hpb_WireReader_ReadLongVarintRet _hpb_WireReader_ReadLongVarint(
    const char* ptr, uint64_t val);

static HPB_FORCEINLINE const char* _hpb_WireReader_ReadVarint(const char* ptr,
                                                              uint64_t* val,
                                                              int maxlen,
                                                              uint64_t maxval) {
  uint64_t byte = (uint8_t)*ptr;
  if (HPB_LIKELY((byte & 0x80) == 0)) {
    *val = (uint32_t)byte;
    return ptr + 1;
  }
  const char* start = ptr;
  _hpb_WireReader_ReadLongVarintRet res =
      _hpb_WireReader_ReadLongVarint(ptr, byte);
  if (!res.ptr || (maxlen < 10 && res.ptr - start > maxlen) ||
      res.val > maxval) {
    return NULL;  // Malformed.
  }
  *val = res.val;
  return res.ptr;
}

// Parses a tag into `tag`, and returns a pointer past the end of the tag, or
// NULL if there was an error in the tag data.
//
// REQUIRES: there must be at least 10 bytes of data available at `ptr`.
// Bounds checks must be performed before calling this function, preferably
// by calling hpb_EpsCopyInputStream_IsDone().
static HPB_FORCEINLINE const char* hpb_WireReader_ReadTag(const char* ptr,
                                                          uint32_t* tag) {
  uint64_t val;
  ptr = _hpb_WireReader_ReadVarint(ptr, &val, 5, UINT32_MAX);
  if (!ptr) return NULL;
  *tag = val;
  return ptr;
}

// Given a tag, returns the field number.
HPB_INLINE uint32_t hpb_WireReader_GetFieldNumber(uint32_t tag) {
  return tag >> kHpb_WireReader_WireTypeBits;
}

// Given a tag, returns the wire type.
HPB_INLINE uint8_t hpb_WireReader_GetWireType(uint32_t tag) {
  return tag & kHpb_WireReader_WireTypeMask;
}

HPB_INLINE const char* hpb_WireReader_ReadVarint(const char* ptr,
                                                 uint64_t* val) {
  return _hpb_WireReader_ReadVarint(ptr, val, 10, UINT64_MAX);
}

// Skips data for a varint, returning a pointer past the end of the varint, or
// NULL if there was an error in the varint data.
//
// REQUIRES: there must be at least 10 bytes of data available at `ptr`.
// Bounds checks must be performed before calling this function, preferably
// by calling hpb_EpsCopyInputStream_IsDone().
HPB_INLINE const char* hpb_WireReader_SkipVarint(const char* ptr) {
  uint64_t val;
  return hpb_WireReader_ReadVarint(ptr, &val);
}

// Reads a varint indicating the size of a delimited field into `size`, or
// NULL if there was an error in the varint data.
//
// REQUIRES: there must be at least 10 bytes of data available at `ptr`.
// Bounds checks must be performed before calling this function, preferably
// by calling hpb_EpsCopyInputStream_IsDone().
HPB_INLINE const char* hpb_WireReader_ReadSize(const char* ptr, int* size) {
  uint64_t size64;
  ptr = hpb_WireReader_ReadVarint(ptr, &size64);
  if (!ptr || size64 >= INT32_MAX) return NULL;
  *size = size64;
  return ptr;
}

// Reads a fixed32 field, performing byte swapping if necessary.
//
// REQUIRES: there must be at least 4 bytes of data available at `ptr`.
// Bounds checks must be performed before calling this function, preferably
// by calling hpb_EpsCopyInputStream_IsDone().
HPB_INLINE const char* hpb_WireReader_ReadFixed32(const char* ptr, void* val) {
  uint32_t uval;
  memcpy(&uval, ptr, 4);
  uval = _hpb_BigEndian_Swap32(uval);
  memcpy(val, &uval, 4);
  return ptr + 4;
}

// Reads a fixed64 field, performing byte swapping if necessary.
//
// REQUIRES: there must be at least 4 bytes of data available at `ptr`.
// Bounds checks must be performed before calling this function, preferably
// by calling hpb_EpsCopyInputStream_IsDone().
HPB_INLINE const char* hpb_WireReader_ReadFixed64(const char* ptr, void* val) {
  uint64_t uval;
  memcpy(&uval, ptr, 8);
  uval = _hpb_BigEndian_Swap64(uval);
  memcpy(val, &uval, 8);
  return ptr + 8;
}

const char* _hpb_WireReader_SkipGroup(const char* ptr, uint32_t tag,
                                      int depth_limit,
                                      hpb_EpsCopyInputStream* stream);

// Skips data for a group, returning a pointer past the end of the group, or
// NULL if there was an error parsing the group.  The `tag` argument should be
// the start group tag that begins the group.  The `depth_limit` argument
// indicates how many levels of recursion the group is allowed to have before
// reporting a parse error (this limit exists to protect against stack
// overflow).
//
// TODO: evaluate how the depth_limit should be specified. Do users need
// control over this?
HPB_INLINE const char* hpb_WireReader_SkipGroup(
    const char* ptr, uint32_t tag, hpb_EpsCopyInputStream* stream) {
  return _hpb_WireReader_SkipGroup(ptr, tag, 100, stream);
}

HPB_INLINE const char* _hpb_WireReader_SkipValue(
    const char* ptr, uint32_t tag, int depth_limit,
    hpb_EpsCopyInputStream* stream) {
  switch (hpb_WireReader_GetWireType(tag)) {
    case kHpb_WireType_Varint:
      return hpb_WireReader_SkipVarint(ptr);
    case kHpb_WireType_32Bit:
      return ptr + 4;
    case kHpb_WireType_64Bit:
      return ptr + 8;
    case kHpb_WireType_Delimited: {
      int size;
      ptr = hpb_WireReader_ReadSize(ptr, &size);
      if (!ptr) return NULL;
      ptr += size;
      return ptr;
    }
    case kHpb_WireType_StartGroup:
      return _hpb_WireReader_SkipGroup(ptr, tag, depth_limit, stream);
    case kHpb_WireType_EndGroup:
      return NULL;  // Should be handled before now.
    default:
      return NULL;  // Unknown wire type.
  }
}

// Skips data for a wire value of any type, returning a pointer past the end of
// the data, or NULL if there was an error parsing the group. The `tag` argument
// should be the tag that was just parsed. The `depth_limit` argument indicates
// how many levels of recursion a group is allowed to have before reporting a
// parse error (this limit exists to protect against stack overflow).
//
// REQUIRES: there must be at least 10 bytes of data available at `ptr`.
// Bounds checks must be performed before calling this function, preferably
// by calling hpb_EpsCopyInputStream_IsDone().
//
// TODO: evaluate how the depth_limit should be specified. Do users need
// control over this?
HPB_INLINE const char* hpb_WireReader_SkipValue(
    const char* ptr, uint32_t tag, hpb_EpsCopyInputStream* stream) {
  return _hpb_WireReader_SkipValue(ptr, tag, 100, stream);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_WIRE_READER_H_
