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

#ifndef HPB_MINI_DESCRIPTOR_BUILD_ENUM_H_
#define HPB_MINI_DESCRIPTOR_BUILD_ENUM_H_

#include "hpb/base/status.h"
#include "hpb/mem/arena.h"
#include "hpb/mini_table/enum.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Builds a hpb_MiniTableEnum from an enum MiniDescriptor.  The MiniDescriptor
// must be for an enum, not a message.
HPB_API hpb_MiniTableEnum* hpb_MiniDescriptor_BuildEnum(const char* data,
                                                        size_t len,
                                                        hpb_Arena* arena,
                                                        hpb_Status* status);

// TODO(b/289057707): Deprecated name; update callers.
HPB_API_INLINE hpb_MiniTableEnum* hpb_MiniTableEnum_Build(const char* data,
                                                          size_t len,
                                                          hpb_Arena* arena,
                                                          hpb_Status* status) {
  return hpb_MiniDescriptor_BuildEnum(data, len, arena, status);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_DESCRIPTOR_BUILD_ENUM_H_
