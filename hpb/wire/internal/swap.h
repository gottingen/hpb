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

#ifndef HPB_WIRE_INTERNAL_SWAP_H_
#define HPB_WIRE_INTERNAL_SWAP_H_

#include <stdint.h>

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

HPB_INLINE bool _hpb_IsLittleEndian(void) {
  int x = 1;
  return *(char*)&x == 1;
}

HPB_INLINE uint32_t _hpb_BigEndian_Swap32(uint32_t val) {
  if (_hpb_IsLittleEndian()) return val;

  return ((val & 0xff) << 24) | ((val & 0xff00) << 8) |
         ((val & 0xff0000) >> 8) | ((val & 0xff000000) >> 24);
}

HPB_INLINE uint64_t _hpb_BigEndian_Swap64(uint64_t val) {
  if (_hpb_IsLittleEndian()) return val;

  return ((uint64_t)_hpb_BigEndian_Swap32((uint32_t)val) << 32) |
         _hpb_BigEndian_Swap32((uint32_t)(val >> 32));
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_WIRE_INTERNAL_SWAP_H_ */
