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

// An attempt to provide some of the C++ string functionality in C.
// Function names generally match those of corresponding C++ string methods.
// All buffers are copied so operations are relatively expensive.
// Internal character strings are always NULL-terminated.
// All bool functions return true on success, false on failure.

#ifndef HPB_IO_STRING_H_
#define HPB_IO_STRING_H_

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "hpb/mem/arena.h"
#include "hpb/port/vsnprintf_compat.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Do not directly access the fields of this struct - use the accessors only.
// TODO(salo): Add a small (16 bytes, maybe?) internal buffer so we can avoid
// hitting the arena for short strings.
typedef struct {
  size_t size_;
  size_t capacity_;
  char* data_;
  hpb_Arena* arena_;
} hpb_String;

// Initialize an already-allocted hpb_String object.
HPB_INLINE bool hpb_String_Init(hpb_String* s, hpb_Arena* a) {
  static const int kDefaultCapacity = 16;

  s->size_ = 0;
  s->capacity_ = kDefaultCapacity;
  s->data_ = (char*)hpb_Arena_Malloc(a, kDefaultCapacity);
  s->arena_ = a;
  if (!s->data_) return false;
  s->data_[0] = '\0';
  return true;
}

HPB_INLINE void hpb_String_Clear(hpb_String* s) {
  s->size_ = 0;
  s->data_[0] = '\0';
}

HPB_INLINE char* hpb_String_Data(const hpb_String* s) { return s->data_; }

HPB_INLINE size_t hpb_String_Size(const hpb_String* s) { return s->size_; }

HPB_INLINE bool hpb_String_Empty(const hpb_String* s) { return s->size_ == 0; }

HPB_INLINE void hpb_String_Erase(hpb_String* s, size_t pos, size_t len) {
  if (pos >= s->size_) return;
  char* des = s->data_ + pos;
  if (pos + len > s->size_) len = s->size_ - pos;
  char* src = des + len;
  memmove(des, src, s->size_ - (src - s->data_) + 1);
  s->size_ -= len;
}

HPB_INLINE bool hpb_String_Reserve(hpb_String* s, size_t size) {
  if (s->capacity_ <= size) {
    const size_t new_cap = size + 1;
    s->data_ =
        (char*)hpb_Arena_Realloc(s->arena_, s->data_, s->capacity_, new_cap);
    if (!s->data_) return false;
    s->capacity_ = new_cap;
  }
  return true;
}

HPB_INLINE bool hpb_String_Append(hpb_String* s, const char* data,
                                  size_t size) {
  if (s->capacity_ <= s->size_ + size) {
    const size_t new_cap = 2 * (s->size_ + size) + 1;
    if (!hpb_String_Reserve(s, new_cap)) return false;
  }

  memcpy(s->data_ + s->size_, data, size);
  s->size_ += size;
  s->data_[s->size_] = '\0';
  return true;
}

HPB_PRINTF(2, 0)
HPB_INLINE bool hpb_String_AppendFmtV(hpb_String* s, const char* fmt,
                                      va_list args) {
  size_t capacity = 1000;
  char* buf = (char*)malloc(capacity);
  bool out = false;
  for (;;) {
    const int n = _hpb_vsnprintf(buf, capacity, fmt, args);
    if (n < 0) break;
    if (n < capacity) {
      out = hpb_String_Append(s, buf, n);
      break;
    }
    capacity *= 2;
    buf = (char*)realloc(buf, capacity);
  }
  free(buf);
  return out;
}

HPB_PRINTF(2, 3)
HPB_INLINE bool hpb_String_AppendFmt(hpb_String* s, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const bool ok = hpb_String_AppendFmtV(s, fmt, args);
  va_end(args);
  return ok;
}

HPB_INLINE bool hpb_String_Assign(hpb_String* s, const char* data,
                                  size_t size) {
  hpb_String_Clear(s);
  return hpb_String_Append(s, data, size);
}

HPB_INLINE bool hpb_String_Copy(hpb_String* des, const hpb_String* src) {
  return hpb_String_Assign(des, src->data_, src->size_);
}

HPB_INLINE bool hpb_String_PushBack(hpb_String* s, char ch) {
  return hpb_String_Append(s, &ch, 1);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif // HPB_IO_STRING_H_
