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

#ifndef HPB_JSON_ENCODE_H_
#define HPB_JSON_ENCODE_H_

#include "hpb/reflection/def.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  /* When set, emits 0/default values.  TODO(haberman): proto3 only? */
  hpb_JsonEncode_EmitDefaults = 1 << 0,

  /* When set, use normal (snake_case) field names instead of JSON (camelCase)
     names. */
  hpb_JsonEncode_UseProtoNames = 1 << 1,

  /* When set, emits enums as their integer values instead of as their names. */
  hpb_JsonEncode_FormatEnumsAsIntegers = 1 << 2
};

/* Encodes the given |msg| to JSON format.  The message's reflection is given in
 * |m|.  The symtab in |symtab| is used to find extensions (if NULL, extensions
 * will not be printed).
 *
 * Output is placed in the given buffer, and always NULL-terminated.  The output
 * size (excluding NULL) is returned.  This means that a return value >= |size|
 * implies that the output was truncated.  (These are the same semantics as
 * snprintf()). */
HPB_API size_t hpb_JsonEncode(const hpb_Message* msg, const hpb_MessageDef* m,
                              const hpb_DefPool* ext_pool, int options,
                              char* buf, size_t size, hpb_Status* status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_JSONENCODE_H_
