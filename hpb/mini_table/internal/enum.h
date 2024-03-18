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

#ifndef HPB_MINI_TABLE_INTERNAL_ENUM_H_
#define HPB_MINI_TABLE_INTERNAL_ENUM_H_

#include <stdint.h>

// Must be last.
#include "hpb/port/def.inc"

struct hpb_MiniTableEnum {
  uint32_t mask_limit;   // Limit enum value that can be tested with mask.
  uint32_t value_count;  // Number of values after the bitfield.
  uint32_t data[];       // Bitmask + enumerated values follow.
};

typedef enum {
  _kHpb_FastEnumCheck_ValueIsInEnum = 0,
  _kHpb_FastEnumCheck_ValueIsNotInEnum = 1,
  _kHpb_FastEnumCheck_CannotCheckFast = 2,
} _kHpb_FastEnumCheck_Status;

#ifdef __cplusplus
extern "C" {
#endif

HPB_INLINE _kHpb_FastEnumCheck_Status _hpb_MiniTable_CheckEnumValueFast(
    const struct hpb_MiniTableEnum* e, uint32_t val) {
  if (HPB_UNLIKELY(val >= 64)) return _kHpb_FastEnumCheck_CannotCheckFast;
  uint64_t mask = e->data[0] | ((uint64_t)e->data[1] << 32);
  return (mask & (1ULL << val)) ? _kHpb_FastEnumCheck_ValueIsInEnum
                                : _kHpb_FastEnumCheck_ValueIsNotInEnum;
}

HPB_INLINE bool _hpb_MiniTable_CheckEnumValueSlow(
    const struct hpb_MiniTableEnum* e, uint32_t val) {
  if (val < e->mask_limit) return e->data[val / 32] & (1ULL << (val % 32));
  // OPT: binary search long lists?
  const uint32_t* start = &e->data[e->mask_limit / 32];
  const uint32_t* limit = &e->data[(e->mask_limit / 32) + e->value_count];
  for (const uint32_t* p = start; p < limit; p++) {
    if (*p == val) return true;
  }
  return false;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_TABLE_INTERNAL_ENUM_H_
