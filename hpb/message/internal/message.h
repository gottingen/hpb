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
** Our memory representation for parsing tables and messages themselves.
** Functions in this file are used by generated code and possibly reflection.
**
** The definitions in this file are internal to hpb.
**/

#ifndef HPB_MESSAGE_INTERNAL_H_
#define HPB_MESSAGE_INTERNAL_H_

#include <stdlib.h>
#include <string.h>

#include "hpb/hash/common.h"
#include "hpb/message/internal/extension.h"
#include "hpb/message/message.h"
#include "hpb/mini_table/extension.h"
#include "hpb/mini_table/extension_registry.h"
#include "hpb/mini_table/message.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

extern const float kHpb_FltInfinity;
extern const double kHpb_Infinity;
extern const double kHpb_NaN;

/* Internal members of a hpb_Message that track unknown fields and/or
 * extensions. We can change this without breaking binary compatibility.  We put
 * these before the user's data.  The user's hpb_Message* points after the
 * hpb_Message_Internal. */

typedef struct {
  /* Total size of this structure, including the data that follows.
   * Must be aligned to 8, which is alignof(hpb_Message_Extension) */
  uint32_t size;

  /* Offsets relative to the beginning of this structure.
   *
   * Unknown data grows forward from the beginning to unknown_end.
   * Extension data grows backward from size to ext_begin.
   * When the two meet, we're out of data and have to realloc.
   *
   * If we imagine that the final member of this struct is:
   *   char data[size - overhead];  // overhead =
   * sizeof(hpb_Message_InternalData)
   *
   * Then we have:
   *   unknown data: data[0 .. (unknown_end - overhead)]
   *   extensions data: data[(ext_begin - overhead) .. (size - overhead)] */
  uint32_t unknown_end;
  uint32_t ext_begin;
  /* Data follows, as if there were an array:
   *   char data[size - sizeof(hpb_Message_InternalData)]; */
} hpb_Message_InternalData;

typedef struct {
  // LINT.IfChange(internal_layout)
  union {
    hpb_Message_InternalData* internal;

    // Force 8-byte alignment, since the data members may contain members that
    // require 8-byte alignment.
    double d;
  };
} hpb_Message_Internal;

/* Maps hpb_CType -> memory size. */
extern char _hpb_CTypeo_size[12];

HPB_INLINE size_t hpb_msg_sizeof(const hpb_MiniTable* t) {
  return t->size + sizeof(hpb_Message_Internal);
}

// Inline version hpb_Message_New(), for internal use.
HPB_INLINE hpb_Message* _hpb_Message_New(const hpb_MiniTable* mini_table,
                                         hpb_Arena* arena) {
  size_t size = hpb_msg_sizeof(mini_table);
  void* mem = hpb_Arena_Malloc(arena, size + sizeof(hpb_Message_Internal));
  if (HPB_UNLIKELY(!mem)) return NULL;
  hpb_Message* msg = HPB_PTR_AT(mem, sizeof(hpb_Message_Internal), hpb_Message);
  memset(mem, 0, size);
  return msg;
}

HPB_INLINE hpb_Message_Internal* hpb_Message_Getinternal(
    const hpb_Message* msg) {
  ptrdiff_t size = sizeof(hpb_Message_Internal);
  return (hpb_Message_Internal*)((char*)msg - size);
}

// Discards the unknown fields for this message only.
void _hpb_Message_DiscardUnknown_shallow(hpb_Message* msg);

// Adds unknown data (serialized protobuf data) to the given message.
// The data is copied into the message instance.
bool _hpb_Message_AddUnknown(hpb_Message* msg, const char* data, size_t len,
                             hpb_Arena* arena);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MESSAGE_INTERNAL_H_
