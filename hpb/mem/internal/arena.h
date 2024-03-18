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

#ifndef HPB_MEM_INTERNAL_ARENA_H_
#define HPB_MEM_INTERNAL_ARENA_H_

#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct _hpb_MemBlock _hpb_MemBlock;

struct hpb_Arena {
  _hpb_ArenaHead head;

  // hpb_alloc* together with a low bit which signals if there is an initial
  // block.
  uintptr_t block_alloc;

  // When multiple arenas are fused together, each arena points to a parent
  // arena (root points to itself). The root tracks how many live arenas
  // reference it.

  // The low bit is tagged:
  //   0: pointer to parent
  //   1: count, left shifted by one
  HPB_ATOMIC(uintptr_t) parent_or_count;

  // All nodes that are fused together are in a singly-linked list.
  HPB_ATOMIC(hpb_Arena*) next;  // NULL at end of list.

  // The last element of the linked list.  This is present only as an
  // optimization, so that we do not have to iterate over all members for every
  // fuse.  Only significant for an arena root.  In other cases it is ignored.
  HPB_ATOMIC(hpb_Arena*) tail;  // == self when no other list members.

  // Linked list of blocks to free/cleanup.  Atomic only for the benefit of
  // hpb_Arena_SpaceAllocated().
  HPB_ATOMIC(_hpb_MemBlock*) blocks;
};

HPB_INLINE bool _hpb_Arena_IsTaggedRefcount(uintptr_t parent_or_count) {
  return (parent_or_count & 1) == 1;
}

HPB_INLINE bool _hpb_Arena_IsTaggedPointer(uintptr_t parent_or_count) {
  return (parent_or_count & 1) == 0;
}

HPB_INLINE uintptr_t _hpb_Arena_RefCountFromTagged(uintptr_t parent_or_count) {
  HPB_ASSERT(_hpb_Arena_IsTaggedRefcount(parent_or_count));
  return parent_or_count >> 1;
}

HPB_INLINE uintptr_t _hpb_Arena_TaggedFromRefcount(uintptr_t refcount) {
  uintptr_t parent_or_count = (refcount << 1) | 1;
  HPB_ASSERT(_hpb_Arena_IsTaggedRefcount(parent_or_count));
  return parent_or_count;
}

HPB_INLINE hpb_Arena* _hpb_Arena_PointerFromTagged(uintptr_t parent_or_count) {
  HPB_ASSERT(_hpb_Arena_IsTaggedPointer(parent_or_count));
  return (hpb_Arena*)parent_or_count;
}

HPB_INLINE uintptr_t _hpb_Arena_TaggedFromPointer(hpb_Arena* a) {
  uintptr_t parent_or_count = (uintptr_t)a;
  HPB_ASSERT(_hpb_Arena_IsTaggedPointer(parent_or_count));
  return parent_or_count;
}

HPB_INLINE hpb_alloc* hpb_Arena_BlockAlloc(hpb_Arena* arena) {
  return (hpb_alloc*)(arena->block_alloc & ~0x1);
}

HPB_INLINE uintptr_t hpb_Arena_MakeBlockAlloc(hpb_alloc* alloc,
                                              bool has_initial) {
  uintptr_t alloc_uint = (uintptr_t)alloc;
  HPB_ASSERT((alloc_uint & 1) == 0);
  return alloc_uint | (has_initial ? 1 : 0);
}

HPB_INLINE bool hpb_Arena_HasInitialBlock(hpb_Arena* arena) {
  return arena->block_alloc & 0x1;
}

#include "hpb/port/undef.inc"

#endif  // HPB_MEM_INTERNAL_ARENA_H_
