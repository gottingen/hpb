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

#ifndef HPBC_HPBDEV_H_
#define HPBC_HPBDEV_H_

#include "hpb/base/status.h"
#include "hpb/base/string_view.h"
#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Consume |buf|, deserialize it to a Code_Generator_Request proto, construct a
// hpbc_Code_Generator_Request, and return it as a JSON-encoded string.
HPB_API hpb_StringView hpbdev_ProcessInput(const char *buf, size_t size,
                                           hpb_Arena *arena,
                                           hpb_Status *status);

// Decode |buf| from JSON, serialize to wire format, and return it.
HPB_API hpb_StringView hpbdev_ProcessOutput(const char *buf, size_t size,
                                            hpb_Arena *arena,
                                            hpb_Status *status);

// Decode |buf| from JSON, serialize to wire format, and write it to stdout.
HPB_API void hpbdev_ProcessStdout(const char *buf, size_t size,
                                  hpb_Arena *arena, hpb_Status *status);

// The following wrappers allow the protoc plugins to call the above functions
// without pulling in the entire pb_runtime library.
HPB_API hpb_Arena *hpbdev_Arena_New(void);

HPB_API void hpbdev_Status_Clear(hpb_Status *status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPBC_HPBDEV_H_
