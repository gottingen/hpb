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

#ifndef HPB_MINI_DESCRIPTOR_INTERNAL_BASE92_H_
#define HPB_MINI_DESCRIPTOR_INTERNAL_BASE92_H_

#include <stdint.h>

#include "hpb/base/internal/log2.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

HPB_INLINE char _hpb_ToBase92(int8_t ch) {
  extern const char _kHpb_ToBase92[];
  HPB_ASSERT(0 <= ch && ch < 92);
  return _kHpb_ToBase92[ch];
}

HPB_INLINE char _hpb_FromBase92(uint8_t ch) {
  extern const int8_t _kHpb_FromBase92[];
  if (' ' > ch || ch > '~') return -1;
  return _kHpb_FromBase92[ch - ' '];
}

HPB_INLINE const char* _hpb_Base92_DecodeVarint(const char* ptr,
                                                const char* end, char first_ch,
                                                uint8_t min, uint8_t max,
                                                uint32_t* out_val) {
  uint32_t val = 0;
  uint32_t shift = 0;
  const int bits_per_char =
      hpb_Log2Ceiling(_hpb_FromBase92(max) - _hpb_FromBase92(min));
  char ch = first_ch;
  while (1) {
    uint32_t bits = _hpb_FromBase92(ch) - _hpb_FromBase92(min);
    val |= bits << shift;
    if (ptr == end || *ptr < min || max < *ptr) {
      *out_val = val;
      HPB_ASSUME(ptr != NULL);
      return ptr;
    }
    ch = *ptr++;
    shift += bits_per_char;
    if (shift >= 32) return NULL;
  }
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_DESCRIPTOR_INTERNAL_BASE92_H_
