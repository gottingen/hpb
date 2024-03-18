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

#ifndef HPB_MEM_ALLOC_H_
#define HPB_MEM_ALLOC_H_

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hpb_alloc hpb_alloc;

/* A combined `malloc()`/`free()` function.
 * If `size` is 0 then the function acts like `free()`, otherwise it acts like
 * `realloc()`.  Only `oldsize` bytes from a previous allocation are
 * preserved. */
typedef void* hpb_alloc_func(hpb_alloc* alloc, void* ptr, size_t oldsize,
                             size_t size);

/* A hpb_alloc is a possibly-stateful allocator object.
 *
 * It could either be an arena allocator (which doesn't require individual
 * `free()` calls) or a regular `malloc()` (which does).  The client must
 * therefore free memory unless it knows that the allocator is an arena
 * allocator. */
struct hpb_alloc {
  hpb_alloc_func* func;
};

HPB_INLINE void* hpb_malloc(hpb_alloc* alloc, size_t size) {
  HPB_ASSERT(alloc);
  return alloc->func(alloc, NULL, 0, size);
}

HPB_INLINE void* hpb_realloc(hpb_alloc* alloc, void* ptr, size_t oldsize,
                             size_t size) {
  HPB_ASSERT(alloc);
  return alloc->func(alloc, ptr, oldsize, size);
}

HPB_INLINE void hpb_free(hpb_alloc* alloc, void* ptr) {
  HPB_ASSERT(alloc);
  alloc->func(alloc, ptr, 0, 0);
}

// The global allocator used by hpb. Uses the standard malloc()/free().

extern hpb_alloc hpb_alloc_global;

/* Functions that hard-code the global malloc.
 *
 * We still get benefit because we can put custom logic into our global
 * allocator, like injecting out-of-memory faults in debug/testing builds. */

HPB_INLINE void* hpb_gmalloc(size_t size) {
  return hpb_malloc(&hpb_alloc_global, size);
}

HPB_INLINE void* hpb_grealloc(void* ptr, size_t oldsize, size_t size) {
  return hpb_realloc(&hpb_alloc_global, ptr, oldsize, size);
}

HPB_INLINE void hpb_gfree(void* ptr) { hpb_free(&hpb_alloc_global, ptr); }

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif // HPB_MEM_ALLOC_H_
