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

#include "hpb/io/chunked_output_stream.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct {
  hpb_ZeroCopyOutputStream base;

  char* data;
  size_t size;
  size_t limit;
  size_t position;
  size_t last_returned_size;
} hpb_ChunkedOutputStream;

static void* hpb_ChunkedOutputStream_Next(hpb_ZeroCopyOutputStream* z,
                                          size_t* count, hpb_Status* status) {
  hpb_ChunkedOutputStream* c = (hpb_ChunkedOutputStream*)z;
  HPB_ASSERT(c->position <= c->size);

  char* out = c->data + c->position;

  const size_t chunk = HPB_MIN(c->limit, c->size - c->position);
  c->position += chunk;
  c->last_returned_size = chunk;
  *count = chunk;

  return chunk ? out : NULL;
}

static void hpb_ChunkedOutputStream_BackUp(hpb_ZeroCopyOutputStream* z,
                                           size_t count) {
  hpb_ChunkedOutputStream* c = (hpb_ChunkedOutputStream*)z;

  HPB_ASSERT(c->last_returned_size >= count);
  c->position -= count;
  c->last_returned_size -= count;
}

static size_t hpb_ChunkedOutputStream_ByteCount(
    const hpb_ZeroCopyOutputStream* z) {
  const hpb_ChunkedOutputStream* c = (const hpb_ChunkedOutputStream*)z;

  return c->position;
}

static const _hpb_ZeroCopyOutputStream_VTable hpb_ChunkedOutputStream_vtable = {
    hpb_ChunkedOutputStream_Next,
    hpb_ChunkedOutputStream_BackUp,
    hpb_ChunkedOutputStream_ByteCount,
};

hpb_ZeroCopyOutputStream* hpb_ChunkedOutputStream_New(void* data, size_t size,
                                                      size_t limit,
                                                      hpb_Arena* arena) {
  hpb_ChunkedOutputStream* c = hpb_Arena_Malloc(arena, sizeof(*c));
  if (!c || !limit) return NULL;

  c->base.vtable = &hpb_ChunkedOutputStream_vtable;
  c->data = data;
  c->size = size;
  c->limit = limit;
  c->position = 0;
  c->last_returned_size = 0;

  return (hpb_ZeroCopyOutputStream*)c;
}
