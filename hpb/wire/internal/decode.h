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

/*
 * Internal implementation details of the decoder that are shared between
 * decode.c and decode_fast.c.
 */

#ifndef HPB_WIRE_INTERNAL_DECODE_H_
#define HPB_WIRE_INTERNAL_DECODE_H_

#include "hpb/mem/internal/arena.h"
#include "hpb/message/internal/message.h"
#include "hpb/wire/decode.h"
#include "hpb/wire/eps_copy_input_stream.h"
#include "utf8_range.h"

// Must be last.
#include "hpb/port/def.inc"

#define DECODE_NOGROUP (uint32_t) - 1

typedef struct hpb_Decoder {
  hpb_EpsCopyInputStream input;
  const hpb_ExtensionRegistry* extreg;
  const char* unknown;       // Start of unknown data, preserve at buffer flip
  hpb_Message* unknown_msg;  // Pointer to preserve data to
  int depth;                 // Tracks recursion depth to bound stack usage.
  uint32_t end_group;  // field number of END_GROUP tag, else DECODE_NOGROUP.
  uint16_t options;
  bool missing_required;
  hpb_Arena arena;
  hpb_DecodeStatus status;
  jmp_buf err;

#ifndef NDEBUG
  const char* debug_tagstart;
  const char* debug_valstart;
#endif
} hpb_Decoder;

/* Error function that will abort decoding with longjmp(). We can't declare this
 * HPB_NORETURN, even though it is appropriate, because if we do then compilers
 * will "helpfully" refuse to tailcall to it
 * (see: https://stackoverflow.com/a/55657013), which will defeat a major goal
 * of our optimizations. That is also why we must declare it in a separate file,
 * otherwise the compiler will see that it calls longjmp() and deduce that it is
 * noreturn. */
const char* _hpb_FastDecoder_ErrorJmp(hpb_Decoder* d, int status);

extern const uint8_t hpb_utf8_offsets[];

HPB_INLINE
bool _hpb_Decoder_VerifyUtf8Inline(const char* ptr, int len) {
  const char* end = ptr + len;

  // Check 8 bytes at a time for any non-ASCII char.
  while (end - ptr >= 8) {
    uint64_t data;
    memcpy(&data, ptr, 8);
    if (data & 0x8080808080808080) goto non_ascii;
    ptr += 8;
  }

  // Check one byte at a time for non-ASCII.
  while (ptr < end) {
    if (*ptr & 0x80) goto non_ascii;
    ptr++;
  }

  return true;

non_ascii:
  return utf8_range2((const unsigned char*)ptr, end - ptr) == 0;
}

const char* _hpb_Decoder_CheckRequired(hpb_Decoder* d, const char* ptr,
                                       const hpb_Message* msg,
                                       const hpb_MiniTable* l);

/* x86-64 pointers always have the high 16 bits matching. So we can shift
 * left 8 and right 8 without loss of information. */
HPB_INLINE intptr_t decode_totable(const hpb_MiniTable* tablep) {
  return ((intptr_t)tablep << 8) | tablep->table_mask;
}

HPB_INLINE const hpb_MiniTable* decode_totablep(intptr_t table) {
  return (const hpb_MiniTable*)(table >> 8);
}

const char* _hpb_Decoder_IsDoneFallback(hpb_EpsCopyInputStream* e,
                                        const char* ptr, int overrun);

HPB_INLINE bool _hpb_Decoder_IsDone(hpb_Decoder* d, const char** ptr) {
  return hpb_EpsCopyInputStream_IsDoneWithCallback(
      &d->input, ptr, &_hpb_Decoder_IsDoneFallback);
}

HPB_INLINE const char* _hpb_Decoder_BufferFlipCallback(
    hpb_EpsCopyInputStream* e, const char* old_end, const char* new_start) {
  hpb_Decoder* d = (hpb_Decoder*)e;
  if (!old_end) _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_Malformed);

  if (d->unknown) {
    if (!_hpb_Message_AddUnknown(d->unknown_msg, d->unknown,
                                 old_end - d->unknown, &d->arena)) {
      _hpb_FastDecoder_ErrorJmp(d, kHpb_DecodeStatus_OutOfMemory);
    }
    d->unknown = new_start;
  }
  return new_start;
}

#if HPB_FASTTABLE
HPB_INLINE
const char* _hpb_FastDecoder_TagDispatch(hpb_Decoder* d, const char* ptr,
                                         hpb_Message* msg, intptr_t table,
                                         uint64_t hasbits, uint64_t tag) {
  const hpb_MiniTable* table_p = decode_totablep(table);
  uint8_t mask = table;
  uint64_t data;
  size_t idx = tag & mask;
  HPB_ASSUME((idx & 7) == 0);
  idx >>= 3;
  data = table_p->fasttable[idx].field_data ^ tag;
  HPB_MUSTTAIL return table_p->fasttable[idx].field_parser(d, ptr, msg, table,
                                                           hasbits, data);
}
#endif

HPB_INLINE uint32_t _hpb_FastDecoder_LoadTag(const char* ptr) {
  uint16_t tag;
  memcpy(&tag, ptr, 2);
  return tag;
}

#include "hpb/port/undef.inc"

#endif /* HPB_WIRE_INTERNAL_DECODE_H_ */
