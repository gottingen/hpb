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

#ifndef HPB_MINI_DESCRIPTOR_INTERNAL_DECODER_H_
#define HPB_MINI_DESCRIPTOR_INTERNAL_DECODER_H_

#include "hpb/base/status.h"
#include "hpb/mini_descriptor/internal/base92.h"

// Must be last.
#include "hpb/port/def.inc"

// hpb_MdDecoder: used internally for decoding MiniDescriptors for messages,
// extensions, and enums.
typedef struct {
  const char* end;
  hpb_Status* status;
  jmp_buf err;
} hpb_MdDecoder;

HPB_PRINTF(2, 3)
HPB_NORETURN HPB_INLINE void hpb_MdDecoder_ErrorJmp(hpb_MdDecoder* d,
                                                    const char* fmt, ...) {
  if (d->status) {
    va_list argp;
    hpb_Status_SetErrorMessage(d->status, "Error building mini table: ");
    va_start(argp, fmt);
    hpb_Status_VAppendErrorFormat(d->status, fmt, argp);
    va_end(argp);
  }
  HPB_LONGJMP(d->err, 1);
}

HPB_INLINE void hpb_MdDecoder_CheckOutOfMemory(hpb_MdDecoder* d,
                                               const void* ptr) {
  if (!ptr) hpb_MdDecoder_ErrorJmp(d, "Out of memory");
}

HPB_INLINE const char* hpb_MdDecoder_DecodeBase92Varint(
    hpb_MdDecoder* d, const char* ptr, char first_ch, uint8_t min, uint8_t max,
    uint32_t* out_val) {
  ptr = _hpb_Base92_DecodeVarint(ptr, d->end, first_ch, min, max, out_val);
  if (!ptr) hpb_MdDecoder_ErrorJmp(d, "Overlong varint");
  return ptr;
}

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_DESCRIPTOR_INTERNAL_DECODER_H_
