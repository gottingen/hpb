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

#include "hpb/mem/internal/arena.h"

#include "hpb/port/atomic.h"

// Must be last.
#include "hpb/port/def.inc"

struct _hpb_MemBlock {
  // Atomic only for the benefit of SpaceAllocated().
  HPB_ATOMIC(_hpb_MemBlock*) next;
  uint32_t size;
  // Data follows.
};

static const size_t memblock_reserve =
    HPB_ALIGN_UP(sizeof(_hpb_MemBlock), HPB_MALLOC_ALIGN);

typedef struct _hpb_ArenaRoot {
  hpb_Arena* root;
  uintptr_t tagged_count;
} _hpb_ArenaRoot;

static _hpb_ArenaRoot _hpb_Arena_FindRoot(hpb_Arena* a) {
  uintptr_t poc = hpb_Atomic_Load(&a->parent_or_count, memory_order_acquire);
  while (_hpb_Arena_IsTaggedPointer(poc)) {
    hpb_Arena* next = _hpb_Arena_PointerFromTagged(poc);
    HPB_ASSERT(a != next);
    uintptr_t next_poc =
        hpb_Atomic_Load(&next->parent_or_count, memory_order_acquire);

    if (_hpb_Arena_IsTaggedPointer(next_poc)) {
      // To keep complexity down, we lazily collapse levels of the tree.  This
      // keeps it flat in the final case, but doesn't cost much incrementally.
      //
      // Path splitting keeps time complexity down, see:
      //   https://en.wikipedia.org/wiki/Disjoint-set_data_structure
      //
      // We can safely use a relaxed atomic here because all threads doing this
      // will converge on the same value and we don't need memory orderings to
      // be visible.
      //
      // This is true because:
      // - If no fuses occur, this will eventually become the root.
      // - If fuses are actively occurring, the root may change, but the
      //   invariant is that `parent_or_count` merely points to *a* parent.
      //
      // In other words, it is moving towards "the" root, and that root may move
      // further away over time, but the path towards that root will continue to
      // be valid and the creation of the path carries all the memory orderings
      // required.
      HPB_ASSERT(a != _hpb_Arena_PointerFromTagged(next_poc));
      hpb_Atomic_Store(&a->parent_or_count, next_poc, memory_order_relaxed);
    }
    a = next;
    poc = next_poc;
  }
  return (_hpb_ArenaRoot){.root = a, .tagged_count = poc};
}

size_t hpb_Arena_SpaceAllocated(hpb_Arena* arena) {
  arena = _hpb_Arena_FindRoot(arena).root;
  size_t memsize = 0;

  while (arena != NULL) {
    _hpb_MemBlock* block =
        hpb_Atomic_Load(&arena->blocks, memory_order_relaxed);
    while (block != NULL) {
      memsize += sizeof(_hpb_MemBlock) + block->size;
      block = hpb_Atomic_Load(&block->next, memory_order_relaxed);
    }
    arena = hpb_Atomic_Load(&arena->next, memory_order_relaxed);
  }

  return memsize;
}

uint32_t hpb_Arena_DebugRefCount(hpb_Arena* a) {
  // These loads could probably be relaxed, but given that this is debug-only,
  // it's not worth introducing a new variant for it.
  uintptr_t poc = hpb_Atomic_Load(&a->parent_or_count, memory_order_acquire);
  while (_hpb_Arena_IsTaggedPointer(poc)) {
    a = _hpb_Arena_PointerFromTagged(poc);
    poc = hpb_Atomic_Load(&a->parent_or_count, memory_order_acquire);
  }
  return _hpb_Arena_RefCountFromTagged(poc);
}

static void hpb_Arena_AddBlock(hpb_Arena* a, void* ptr, size_t size) {
  _hpb_MemBlock* block = ptr;

  // Insert into linked list.
  block->size = (uint32_t)size;
  hpb_Atomic_Init(&block->next, a->blocks);
  hpb_Atomic_Store(&a->blocks, block, memory_order_release);

  a->head.ptr = HPB_PTR_AT(block, memblock_reserve, char);
  a->head.end = HPB_PTR_AT(block, size, char);

  HPB_POISON_MEMORY_REGION(a->head.ptr, a->head.end - a->head.ptr);
}

static bool hpb_Arena_AllocBlock(hpb_Arena* a, size_t size) {
  if (!a->block_alloc) return false;
  _hpb_MemBlock* last_block = hpb_Atomic_Load(&a->blocks, memory_order_acquire);
  size_t last_size = last_block != NULL ? last_block->size : 128;
  size_t block_size = HPB_MAX(size, last_size * 2) + memblock_reserve;
  _hpb_MemBlock* block = hpb_malloc(hpb_Arena_BlockAlloc(a), block_size);

  if (!block) return false;
  hpb_Arena_AddBlock(a, block, block_size);
  return true;
}

void* _hpb_Arena_SlowMalloc(hpb_Arena* a, size_t size) {
  if (!hpb_Arena_AllocBlock(a, size)) return NULL; /* Out of memory. */
  HPB_ASSERT(_hpb_ArenaHas(a) >= size);
  return hpb_Arena_Malloc(a, size);
}

/* Public Arena API ***********************************************************/

static hpb_Arena* hpb_Arena_InitSlow(hpb_alloc* alloc) {
  const size_t first_block_overhead = sizeof(hpb_Arena) + memblock_reserve;
  hpb_Arena* a;

  /* We need to malloc the initial block. */
  char* mem;
  size_t n = first_block_overhead + 256;
  if (!alloc || !(mem = hpb_malloc(alloc, n))) {
    return NULL;
  }

  a = HPB_PTR_AT(mem, n - sizeof(*a), hpb_Arena);
  n -= sizeof(*a);

  a->block_alloc = hpb_Arena_MakeBlockAlloc(alloc, 0);
  hpb_Atomic_Init(&a->parent_or_count, _hpb_Arena_TaggedFromRefcount(1));
  hpb_Atomic_Init(&a->next, NULL);
  hpb_Atomic_Init(&a->tail, a);
  hpb_Atomic_Init(&a->blocks, NULL);

  hpb_Arena_AddBlock(a, mem, n);

  return a;
}

hpb_Arena* hpb_Arena_Init(void* mem, size_t n, hpb_alloc* alloc) {
  hpb_Arena* a;

  if (n) {
    /* Align initial pointer up so that we return properly-aligned pointers. */
    void* aligned = (void*)HPB_ALIGN_UP((uintptr_t)mem, HPB_MALLOC_ALIGN);
    size_t delta = (uintptr_t)aligned - (uintptr_t)mem;
    n = delta <= n ? n - delta : 0;
    mem = aligned;
  }

  /* Round block size down to alignof(*a) since we will allocate the arena
   * itself at the end. */
  n = HPB_ALIGN_DOWN(n, HPB_ALIGN_OF(hpb_Arena));

  if (HPB_UNLIKELY(n < sizeof(hpb_Arena))) {
    return hpb_Arena_InitSlow(alloc);
  }

  a = HPB_PTR_AT(mem, n - sizeof(*a), hpb_Arena);

  hpb_Atomic_Init(&a->parent_or_count, _hpb_Arena_TaggedFromRefcount(1));
  hpb_Atomic_Init(&a->next, NULL);
  hpb_Atomic_Init(&a->tail, a);
  hpb_Atomic_Init(&a->blocks, NULL);
  a->block_alloc = hpb_Arena_MakeBlockAlloc(alloc, 1);
  a->head.ptr = mem;
  a->head.end = HPB_PTR_AT(mem, n - sizeof(*a), char);

  return a;
}

static void arena_dofree(hpb_Arena* a) {
  HPB_ASSERT(_hpb_Arena_RefCountFromTagged(a->parent_or_count) == 1);

  while (a != NULL) {
    // Load first since arena itself is likely from one of its blocks.
    hpb_Arena* next_arena =
        (hpb_Arena*)hpb_Atomic_Load(&a->next, memory_order_acquire);
    hpb_alloc* block_alloc = hpb_Arena_BlockAlloc(a);
    _hpb_MemBlock* block = hpb_Atomic_Load(&a->blocks, memory_order_acquire);
    while (block != NULL) {
      // Load first since we are deleting block.
      _hpb_MemBlock* next_block =
          hpb_Atomic_Load(&block->next, memory_order_acquire);
      hpb_free(block_alloc, block);
      block = next_block;
    }
    a = next_arena;
  }
}

void hpb_Arena_Free(hpb_Arena* a) {
  uintptr_t poc = hpb_Atomic_Load(&a->parent_or_count, memory_order_acquire);
retry:
  while (_hpb_Arena_IsTaggedPointer(poc)) {
    a = _hpb_Arena_PointerFromTagged(poc);
    poc = hpb_Atomic_Load(&a->parent_or_count, memory_order_acquire);
  }

  // compare_exchange or fetch_sub are RMW operations, which are more
  // expensive then direct loads.  As an optimization, we only do RMW ops
  // when we need to update things for other threads to see.
  if (poc == _hpb_Arena_TaggedFromRefcount(1)) {
    arena_dofree(a);
    return;
  }

  if (hpb_Atomic_CompareExchangeWeak(
          &a->parent_or_count, &poc,
          _hpb_Arena_TaggedFromRefcount(_hpb_Arena_RefCountFromTagged(poc) - 1),
          memory_order_release, memory_order_acquire)) {
    // We were >1 and we decremented it successfully, so we are done.
    return;
  }

  // We failed our update, so someone has done something, retry the whole
  // process, but the failed exchange reloaded `poc` for us.
  goto retry;
}

static void _hpb_Arena_DoFuseArenaLists(hpb_Arena* const parent,
                                        hpb_Arena* child) {
  hpb_Arena* parent_tail = hpb_Atomic_Load(&parent->tail, memory_order_relaxed);
  do {
    // Our tail might be stale, but it will always converge to the true tail.
    hpb_Arena* parent_tail_next =
        hpb_Atomic_Load(&parent_tail->next, memory_order_relaxed);
    while (parent_tail_next != NULL) {
      parent_tail = parent_tail_next;
      parent_tail_next =
          hpb_Atomic_Load(&parent_tail->next, memory_order_relaxed);
    }

    hpb_Arena* displaced =
        hpb_Atomic_Exchange(&parent_tail->next, child, memory_order_relaxed);
    parent_tail = hpb_Atomic_Load(&child->tail, memory_order_relaxed);

    // If we displaced something that got installed racily, we can simply
    // reinstall it on our new tail.
    child = displaced;
  } while (child != NULL);

  hpb_Atomic_Store(&parent->tail, parent_tail, memory_order_relaxed);
}

static hpb_Arena* _hpb_Arena_DoFuse(hpb_Arena* a1, hpb_Arena* a2,
                                    uintptr_t* ref_delta) {
  // `parent_or_count` has two disctint modes
  // -  parent pointer mode
  // -  refcount mode
  //
  // In parent pointer mode, it may change what pointer it refers to in the
  // tree, but it will always approach a root.  Any operation that walks the
  // tree to the root may collapse levels of the tree concurrently.
  _hpb_ArenaRoot r1 = _hpb_Arena_FindRoot(a1);
  _hpb_ArenaRoot r2 = _hpb_Arena_FindRoot(a2);

  if (r1.root == r2.root) return r1.root;  // Already fused.

  // Avoid cycles by always fusing into the root with the lower address.
  if ((uintptr_t)r1.root > (uintptr_t)r2.root) {
    _hpb_ArenaRoot tmp = r1;
    r1 = r2;
    r2 = tmp;
  }

  // The moment we install `r1` as the parent for `r2` all racing frees may
  // immediately begin decrementing `r1`'s refcount (including pending
  // increments to that refcount and their frees!).  We need to add `r2`'s refs
  // now, so that `r1` can withstand any unrefs that come from r2.
  //
  // Note that while it is possible for `r2`'s refcount to increase
  // asynchronously, we will not actually do the reparenting operation below
  // unless `r2`'s refcount is unchanged from when we read it.
  //
  // Note that we may have done this previously, either to this node or a
  // different node, during a previous and failed DoFuse() attempt. But we will
  // not lose track of these refs because we always add them to our overall
  // delta.
  uintptr_t r2_untagged_count = r2.tagged_count & ~1;
  uintptr_t with_r2_refs = r1.tagged_count + r2_untagged_count;
  if (!hpb_Atomic_CompareExchangeStrong(
          &r1.root->parent_or_count, &r1.tagged_count, with_r2_refs,
          memory_order_release, memory_order_acquire)) {
    return NULL;
  }

  // Perform the actual fuse by removing the refs from `r2` and swapping in the
  // parent pointer.
  if (!hpb_Atomic_CompareExchangeStrong(
          &r2.root->parent_or_count, &r2.tagged_count,
          _hpb_Arena_TaggedFromPointer(r1.root), memory_order_release,
          memory_order_acquire)) {
    // We'll need to remove the excess refs we added to r1 previously.
    *ref_delta += r2_untagged_count;
    return NULL;
  }

  // Now that the fuse has been performed (and can no longer fail) we need to
  // append `r2` to `r1`'s linked list.
  _hpb_Arena_DoFuseArenaLists(r1.root, r2.root);
  return r1.root;
}

static bool _hpb_Arena_FixupRefs(hpb_Arena* new_root, uintptr_t ref_delta) {
  if (ref_delta == 0) return true;  // No fixup required.
  uintptr_t poc =
      hpb_Atomic_Load(&new_root->parent_or_count, memory_order_relaxed);
  if (_hpb_Arena_IsTaggedPointer(poc)) return false;
  uintptr_t with_refs = poc - ref_delta;
  HPB_ASSERT(!_hpb_Arena_IsTaggedPointer(with_refs));
  return hpb_Atomic_CompareExchangeStrong(&new_root->parent_or_count, &poc,
                                          with_refs, memory_order_relaxed,
                                          memory_order_relaxed);
}

bool hpb_Arena_Fuse(hpb_Arena* a1, hpb_Arena* a2) {
  if (a1 == a2) return true;  // trivial fuse

  // Do not fuse initial blocks since we cannot lifetime extend them.
  // Any other fuse scenario is allowed.
  if (hpb_Arena_HasInitialBlock(a1) || hpb_Arena_HasInitialBlock(a2)) {
    return false;
  }

  // The number of refs we ultimately need to transfer to the new root.
  uintptr_t ref_delta = 0;
  while (true) {
    hpb_Arena* new_root = _hpb_Arena_DoFuse(a1, a2, &ref_delta);
    if (new_root != NULL && _hpb_Arena_FixupRefs(new_root, ref_delta)) {
      return true;
    }
  }
}
