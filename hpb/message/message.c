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

#include "hpb/message/message.h"

#include <math.h>

#include "hpb/base/internal/log2.h"
#include "hpb/message/internal/message.h"

// Must be last.
#include "hpb/port/def.inc"

const float kHpb_FltInfinity = INFINITY;
const double kHpb_Infinity = INFINITY;
const double kHpb_NaN = NAN;

static const size_t overhead = sizeof(hpb_Message_InternalData);

hpb_Message* hpb_Message_New(const hpb_MiniTable* mini_table,
                             hpb_Arena* arena) {
  return _hpb_Message_New(mini_table, arena);
}

static bool realloc_internal(hpb_Message* msg, size_t need, hpb_Arena* arena) {
  hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  if (!in->internal) {
    /* No internal data, allocate from scratch. */
    size_t size = HPB_MAX(128, hpb_Log2CeilingSize(need + overhead));
    hpb_Message_InternalData* internal = hpb_Arena_Malloc(arena, size);
    if (!internal) return false;
    internal->size = size;
    internal->unknown_end = overhead;
    internal->ext_begin = size;
    in->internal = internal;
  } else if (in->internal->ext_begin - in->internal->unknown_end < need) {
    /* Internal data is too small, reallocate. */
    size_t new_size = hpb_Log2CeilingSize(in->internal->size + need);
    size_t ext_bytes = in->internal->size - in->internal->ext_begin;
    size_t new_ext_begin = new_size - ext_bytes;
    hpb_Message_InternalData* internal =
        hpb_Arena_Realloc(arena, in->internal, in->internal->size, new_size);
    if (!internal) return false;
    if (ext_bytes) {
      /* Need to move extension data to the end. */
      char* ptr = (char*)internal;
      memmove(ptr + new_ext_begin, ptr + internal->ext_begin, ext_bytes);
    }
    internal->ext_begin = new_ext_begin;
    internal->size = new_size;
    in->internal = internal;
  }
  HPB_ASSERT(in->internal->ext_begin - in->internal->unknown_end >= need);
  return true;
}

bool _hpb_Message_AddUnknown(hpb_Message* msg, const char* data, size_t len,
                             hpb_Arena* arena) {
  if (!realloc_internal(msg, len, arena)) return false;
  hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  memcpy(HPB_PTR_AT(in->internal, in->internal->unknown_end, char), data, len);
  in->internal->unknown_end += len;
  return true;
}

void _hpb_Message_DiscardUnknown_shallow(hpb_Message* msg) {
  hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  if (in->internal) {
    in->internal->unknown_end = overhead;
  }
}

const char* hpb_Message_GetUnknown(const hpb_Message* msg, size_t* len) {
  const hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  if (in->internal) {
    *len = in->internal->unknown_end - overhead;
    return (char*)(in->internal + 1);
  } else {
    *len = 0;
    return NULL;
  }
}

void hpb_Message_DeleteUnknown(hpb_Message* msg, const char* data, size_t len) {
  hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  const char* internal_unknown_end =
      HPB_PTR_AT(in->internal, in->internal->unknown_end, char);
#ifndef NDEBUG
  size_t full_unknown_size;
  const char* full_unknown = hpb_Message_GetUnknown(msg, &full_unknown_size);
  HPB_ASSERT((uintptr_t)data >= (uintptr_t)full_unknown);
  HPB_ASSERT((uintptr_t)data < (uintptr_t)(full_unknown + full_unknown_size));
  HPB_ASSERT((uintptr_t)(data + len) > (uintptr_t)data);
  HPB_ASSERT((uintptr_t)(data + len) <= (uintptr_t)internal_unknown_end);
#endif
  if ((data + len) != internal_unknown_end) {
    memmove((char*)data, data + len, internal_unknown_end - data - len);
  }
  in->internal->unknown_end -= len;
}

const hpb_Message_Extension* _hpb_Message_Getexts(const hpb_Message* msg,
                                                  size_t* count) {
  const hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  if (in->internal) {
    *count = (in->internal->size - in->internal->ext_begin) /
             sizeof(hpb_Message_Extension);
    return HPB_PTR_AT(in->internal, in->internal->ext_begin, void);
  } else {
    *count = 0;
    return NULL;
  }
}

const hpb_Message_Extension* _hpb_Message_Getext(
    const hpb_Message* msg, const hpb_MiniTableExtension* e) {
  size_t n;
  const hpb_Message_Extension* ext = _hpb_Message_Getexts(msg, &n);

  /* For now we use linear search exclusively to find extensions. If this
   * becomes an issue due to messages with lots of extensions, we can introduce
   * a table of some sort. */
  for (size_t i = 0; i < n; i++) {
    if (ext[i].ext == e) {
      return &ext[i];
    }
  }

  return NULL;
}

hpb_Message_Extension* _hpb_Message_GetOrCreateExtension(
    hpb_Message* msg, const hpb_MiniTableExtension* e, hpb_Arena* arena) {
  hpb_Message_Extension* ext =
      (hpb_Message_Extension*)_hpb_Message_Getext(msg, e);
  if (ext) return ext;
  if (!realloc_internal(msg, sizeof(hpb_Message_Extension), arena)) return NULL;
  hpb_Message_Internal* in = hpb_Message_Getinternal(msg);
  in->internal->ext_begin -= sizeof(hpb_Message_Extension);
  ext = HPB_PTR_AT(in->internal, in->internal->ext_begin, void);
  memset(ext, 0, sizeof(hpb_Message_Extension));
  ext->ext = e;
  return ext;
}

size_t hpb_Message_ExtensionCount(const hpb_Message* msg) {
  size_t count;
  _hpb_Message_Getexts(msg, &count);
  return count;
}
