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

/* hpb_Arena is a specific allocator implementation that uses arena allocation.
 * The user provides an allocator that will be used to allocate the underlying
 * arena blocks.  Arenas by nature do not require the individual allocations
 * to be freed.  However the Arena does allow users to register cleanup
 * functions that will run when the arena is destroyed.
 *
 * A hpb_Arena is *not* thread-safe.
 *
 * You could write a thread-safe arena allocator that satisfies the
 * hpb_alloc interface, but it would not be as efficient for the
 * single-threaded case. */

#ifndef HPB_MEM_ARENA_H_
#define HPB_MEM_ARENA_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hpb/mem/alloc.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct hpb_Arena hpb_Arena;

typedef struct {
  char *ptr, *end;
} _hpb_ArenaHead;

#ifdef __cplusplus
extern "C" {
#endif

// Creates an arena from the given initial block (if any -- n may be 0).
// Additional blocks will be allocated from |alloc|.  If |alloc| is NULL, this
// is a fixed-size arena and cannot grow.
HPB_API hpb_Arena* hpb_Arena_Init(void* mem, size_t n, hpb_alloc* alloc);

HPB_API void hpb_Arena_Free(hpb_Arena* a);
HPB_API bool hpb_Arena_Fuse(hpb_Arena* a, hpb_Arena* b);

void* _hpb_Arena_SlowMalloc(hpb_Arena* a, size_t size);
size_t hpb_Arena_SpaceAllocated(hpb_Arena* arena);
uint32_t hpb_Arena_DebugRefCount(hpb_Arena* arena);

HPB_INLINE size_t _hpb_ArenaHas(hpb_Arena* a) {
  _hpb_ArenaHead* h = (_hpb_ArenaHead*)a;
  return (size_t)(h->end - h->ptr);
}

HPB_API_INLINE void* hpb_Arena_Malloc(hpb_Arena* a, size_t size) {
  size = HPB_ALIGN_MALLOC(size);
  size_t span = size + HPB_ASAN_GUARD_SIZE;
  if (HPB_UNLIKELY(_hpb_ArenaHas(a) < span)) {
    return _hpb_Arena_SlowMalloc(a, size);
  }

  // We have enough space to do a fast malloc.
  _hpb_ArenaHead* h = (_hpb_ArenaHead*)a;
  void* ret = h->ptr;
  HPB_ASSERT(HPB_ALIGN_MALLOC((uintptr_t)ret) == (uintptr_t)ret);
  HPB_ASSERT(HPB_ALIGN_MALLOC(size) == size);
  HPB_UNPOISON_MEMORY_REGION(ret, size);

  h->ptr += span;

  return ret;
}

// Shrinks the last alloc from arena.
// REQUIRES: (ptr, oldsize) was the last malloc/realloc from this arena.
// We could also add a hpb_Arena_TryShrinkLast() which is simply a no-op if
// this was not the last alloc.
HPB_API_INLINE void hpb_Arena_ShrinkLast(hpb_Arena* a, void* ptr,
                                         size_t oldsize, size_t size) {
  _hpb_ArenaHead* h = (_hpb_ArenaHead*)a;
  oldsize = HPB_ALIGN_MALLOC(oldsize);
  size = HPB_ALIGN_MALLOC(size);
  // Must be the last alloc.
  HPB_ASSERT((char*)ptr + oldsize == h->ptr - HPB_ASAN_GUARD_SIZE);
  HPB_ASSERT(size <= oldsize);
  h->ptr = (char*)ptr + size;
}

HPB_API_INLINE void* hpb_Arena_Realloc(hpb_Arena* a, void* ptr, size_t oldsize,
                                       size_t size) {
  _hpb_ArenaHead* h = (_hpb_ArenaHead*)a;
  oldsize = HPB_ALIGN_MALLOC(oldsize);
  size = HPB_ALIGN_MALLOC(size);
  bool is_most_recent_alloc = (uintptr_t)ptr + oldsize == (uintptr_t)h->ptr;

  if (is_most_recent_alloc) {
    ptrdiff_t diff = size - oldsize;
    if ((ptrdiff_t)_hpb_ArenaHas(a) >= diff) {
      h->ptr += diff;
      return ptr;
    }
  } else if (size <= oldsize) {
    return ptr;
  }

  void* ret = hpb_Arena_Malloc(a, size);

  if (ret && oldsize > 0) {
    memcpy(ret, ptr, HPB_MIN(oldsize, size));
  }

  return ret;
}

HPB_API_INLINE hpb_Arena* hpb_Arena_New(void) {
  return hpb_Arena_Init(NULL, 0, &hpb_alloc_global);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_MEM_ARENA_H_ */
