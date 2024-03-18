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

#ifndef HPB_BASE_STRING_VIEW_H_
#define HPB_BASE_STRING_VIEW_H_

#include <string.h>

// Must be last.
#include "hpb/port/def.inc"

#define HPB_STRINGVIEW_INIT(ptr, len) \
  { ptr, len }

#define HPB_STRINGVIEW_FORMAT "%.*s"
#define HPB_STRINGVIEW_ARGS(view) (int)(view).size, (view).data

// LINT.IfChange(struct_definition)
typedef struct {
  const char* data;
  size_t size;
} hpb_StringView;

#ifdef __cplusplus
extern "C" {
#endif

HPB_API_INLINE hpb_StringView hpb_StringView_FromDataAndSize(const char* data,
                                                             size_t size) {
  hpb_StringView ret;
  ret.data = data;
  ret.size = size;
  return ret;
}

HPB_INLINE hpb_StringView hpb_StringView_FromString(const char* data) {
  return hpb_StringView_FromDataAndSize(data, strlen(data));
}

HPB_INLINE bool hpb_StringView_IsEqual(hpb_StringView a, hpb_StringView b) {
  return a.size == b.size && memcmp(a.data, b.data, a.size) == 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_BASE_STRING_VIEW_H_ */
