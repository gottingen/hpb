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

#ifndef HPB_WIRE_EPS_COPY_INPUT_STREAM_H_
#define HPB_WIRE_EPS_COPY_INPUT_STREAM_H_

#include <string.h>

#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// The maximum number of bytes a single protobuf field can take up in the
// wire format.  We only want to do one bounds check per field, so the input
// stream guarantees that after hpb_EpsCopyInputStream_IsDone() is called,
// the decoder can read this many bytes without performing another bounds
// check.  The stream will copy into a patch buffer as necessary to guarantee
// this invariant.
#define kHpb_EpsCopyInputStream_SlopBytes 16

enum {
  kHpb_EpsCopyInputStream_NoAliasing = 0,
  kHpb_EpsCopyInputStream_OnPatch = 1,
  kHpb_EpsCopyInputStream_NoDelta = 2
};

typedef struct {
  const char* end;        // Can read up to SlopBytes bytes beyond this.
  const char* limit_ptr;  // For bounds checks, = end + HPB_MIN(limit, 0)
  uintptr_t aliasing;
  int limit;              // Submessage limit relative to end
  bool error;             // To distinguish between EOF and error.
  char patch[kHpb_EpsCopyInputStream_SlopBytes * 2];
} hpb_EpsCopyInputStream;

// Returns true if the stream is in the error state. A stream enters the error
// state when the user reads past a limit (caught in IsDone()) or the
// ZeroCopyInputStream returns an error.
HPB_INLINE bool hpb_EpsCopyInputStream_IsError(hpb_EpsCopyInputStream* e) {
  return e->error;
}

typedef const char* hpb_EpsCopyInputStream_BufferFlipCallback(
    hpb_EpsCopyInputStream* e, const char* old_end, const char* new_start);

typedef const char* hpb_EpsCopyInputStream_IsDoneFallbackFunc(
    hpb_EpsCopyInputStream* e, const char* ptr, int overrun);

// Initializes a hpb_EpsCopyInputStream using the contents of the buffer
// [*ptr, size].  Updates `*ptr` as necessary to guarantee that at least
// kHpb_EpsCopyInputStream_SlopBytes are available to read.
HPB_INLINE void hpb_EpsCopyInputStream_Init(hpb_EpsCopyInputStream* e,
                                            const char** ptr, size_t size,
                                            bool enable_aliasing) {
  if (size <= kHpb_EpsCopyInputStream_SlopBytes) {
    memset(&e->patch, 0, 32);
    if (size) memcpy(&e->patch, *ptr, size);
    e->aliasing = enable_aliasing ? (uintptr_t)*ptr - (uintptr_t)e->patch
                                  : kHpb_EpsCopyInputStream_NoAliasing;
    *ptr = e->patch;
    e->end = *ptr + size;
    e->limit = 0;
  } else {
    e->end = *ptr + size - kHpb_EpsCopyInputStream_SlopBytes;
    e->limit = kHpb_EpsCopyInputStream_SlopBytes;
    e->aliasing = enable_aliasing ? kHpb_EpsCopyInputStream_NoDelta
                                  : kHpb_EpsCopyInputStream_NoAliasing;
  }
  e->limit_ptr = e->end;
  e->error = false;
}

typedef enum {
  // The current stream position is at a limit.
  kHpb_IsDoneStatus_Done,

  // The current stream position is not at a limit.
  kHpb_IsDoneStatus_NotDone,

  // The current stream position is not at a limit, and the stream needs to
  // be flipped to a new buffer before more data can be read.
  kHpb_IsDoneStatus_NeedFallback,
} hpb_IsDoneStatus;

// Returns the status of the current stream position.  This is a low-level
// function, it is simpler to call hpb_EpsCopyInputStream_IsDone() if possible.
HPB_INLINE hpb_IsDoneStatus hpb_EpsCopyInputStream_IsDoneStatus(
    hpb_EpsCopyInputStream* e, const char* ptr, int* overrun) {
  *overrun = ptr - e->end;
  if (HPB_LIKELY(ptr < e->limit_ptr)) {
    return kHpb_IsDoneStatus_NotDone;
  } else if (HPB_LIKELY(*overrun == e->limit)) {
    return kHpb_IsDoneStatus_Done;
  } else {
    return kHpb_IsDoneStatus_NeedFallback;
  }
}

// Returns true if the stream has hit a limit, either the current delimited
// limit or the overall end-of-stream. As a side effect, this function may flip
// the pointer to a new buffer if there are less than
// kHpb_EpsCopyInputStream_SlopBytes of data to be read in the current buffer.
//
// Postcondition: if the function returns false, there are at least
// kHpb_EpsCopyInputStream_SlopBytes of data available to read at *ptr.
HPB_INLINE bool hpb_EpsCopyInputStream_IsDoneWithCallback(
    hpb_EpsCopyInputStream* e, const char** ptr,
    hpb_EpsCopyInputStream_IsDoneFallbackFunc* func) {
  int overrun;
  switch (hpb_EpsCopyInputStream_IsDoneStatus(e, *ptr, &overrun)) {
    case kHpb_IsDoneStatus_Done:
      return true;
    case kHpb_IsDoneStatus_NotDone:
      return false;
    case kHpb_IsDoneStatus_NeedFallback:
      *ptr = func(e, *ptr, overrun);
      return *ptr == NULL;
  }
  HPB_UNREACHABLE();
}

const char* _hpb_EpsCopyInputStream_IsDoneFallbackNoCallback(
    hpb_EpsCopyInputStream* e, const char* ptr, int overrun);

// A simpler version of IsDoneWithCallback() that does not support a buffer flip
// callback. Useful in cases where we do not need to insert custom logic at
// every buffer flip.
//
// If this returns true, the user must call hpb_EpsCopyInputStream_IsError()
// to distinguish between EOF and error.
HPB_INLINE bool hpb_EpsCopyInputStream_IsDone(hpb_EpsCopyInputStream* e,
                                              const char** ptr) {
  return hpb_EpsCopyInputStream_IsDoneWithCallback(
      e, ptr, _hpb_EpsCopyInputStream_IsDoneFallbackNoCallback);
}

// Returns the total number of bytes that are safe to read from the current
// buffer without reading uninitialized or unallocated memory.
//
// Note that this check does not respect any semantic limits on the stream,
// either limits from PushLimit() or the overall stream end, so some of these
// bytes may have unpredictable, nonsense values in them. The guarantee is only
// that the bytes are valid to read from the perspective of the C language
// (ie. you can read without triggering UBSAN or ASAN).
HPB_INLINE size_t hpb_EpsCopyInputStream_BytesAvailable(
    hpb_EpsCopyInputStream* e, const char* ptr) {
  return (e->end - ptr) + kHpb_EpsCopyInputStream_SlopBytes;
}

// Returns true if the given delimited field size is valid (it does not extend
// beyond any previously-pushed limits).  `ptr` should point to the beginning
// of the field data, after the delimited size.
//
// Note that this does *not* guarantee that all of the data for this field is in
// the current buffer.
HPB_INLINE bool hpb_EpsCopyInputStream_CheckSize(
    const hpb_EpsCopyInputStream* e, const char* ptr, int size) {
  HPB_ASSERT(size >= 0);
  return ptr - e->end + size <= e->limit;
}

HPB_INLINE bool _hpb_EpsCopyInputStream_CheckSizeAvailable(
    hpb_EpsCopyInputStream* e, const char* ptr, int size, bool submessage) {
  // This is one extra branch compared to the more normal:
  //   return (size_t)(end - ptr) < size;
  // However it is one less computation if we are just about to use "ptr + len":
  //   https://godbolt.org/z/35YGPz
  // In microbenchmarks this shows a small improvement.
  uintptr_t uptr = (uintptr_t)ptr;
  uintptr_t uend = (uintptr_t)e->limit_ptr;
  uintptr_t res = uptr + (size_t)size;
  if (!submessage) uend += kHpb_EpsCopyInputStream_SlopBytes;
  // NOTE: this check depends on having a linear address space.  This is not
  // technically guaranteed by uintptr_t.
  bool ret = res >= uptr && res <= uend;
  if (size < 0) HPB_ASSERT(!ret);
  return ret;
}

// Returns true if the given delimited field size is valid (it does not extend
// beyond any previously-pushed limited) *and* all of the data for this field is
// available to be read in the current buffer.
//
// If the size is negative, this function will always return false. This
// property can be useful in some cases.
HPB_INLINE bool hpb_EpsCopyInputStream_CheckDataSizeAvailable(
    hpb_EpsCopyInputStream* e, const char* ptr, int size) {
  return _hpb_EpsCopyInputStream_CheckSizeAvailable(e, ptr, size, false);
}

// Returns true if the given sub-message size is valid (it does not extend
// beyond any previously-pushed limited) *and* all of the data for this
// sub-message is available to be parsed in the current buffer.
//
// This implies that all fields from the sub-message can be parsed from the
// current buffer while maintaining the invariant that we always have at least
// kHpb_EpsCopyInputStream_SlopBytes of data available past the beginning of
// any individual field start.
//
// If the size is negative, this function will always return false. This
// property can be useful in some cases.
HPB_INLINE bool hpb_EpsCopyInputStream_CheckSubMessageSizeAvailable(
    hpb_EpsCopyInputStream* e, const char* ptr, int size) {
  return _hpb_EpsCopyInputStream_CheckSizeAvailable(e, ptr, size, true);
}

// Returns true if aliasing_enabled=true was passed to
// hpb_EpsCopyInputStream_Init() when this stream was initialized.
HPB_INLINE bool hpb_EpsCopyInputStream_AliasingEnabled(
    hpb_EpsCopyInputStream* e) {
  return e->aliasing != kHpb_EpsCopyInputStream_NoAliasing;
}

// Returns true if aliasing_enabled=true was passed to
// hpb_EpsCopyInputStream_Init() when this stream was initialized *and* we can
// alias into the region [ptr, size] in an input buffer.
HPB_INLINE bool hpb_EpsCopyInputStream_AliasingAvailable(
    hpb_EpsCopyInputStream* e, const char* ptr, size_t size) {
  // When EpsCopyInputStream supports streaming, this will need to become a
  // runtime check.
  return hpb_EpsCopyInputStream_CheckDataSizeAvailable(e, ptr, size) &&
         e->aliasing >= kHpb_EpsCopyInputStream_NoDelta;
}

// Returns a pointer into an input buffer that corresponds to the parsing
// pointer `ptr`.  The returned pointer may be the same as `ptr`, but also may
// be different if we are currently parsing out of the patch buffer.
//
// REQUIRES: Aliasing must be available for the given pointer. If the input is a
// flat buffer and aliasing is enabled, then aliasing will always be available.
HPB_INLINE const char* hpb_EpsCopyInputStream_GetAliasedPtr(
    hpb_EpsCopyInputStream* e, const char* ptr) {
  HPB_ASSUME(hpb_EpsCopyInputStream_AliasingAvailable(e, ptr, 0));
  uintptr_t delta =
      e->aliasing == kHpb_EpsCopyInputStream_NoDelta ? 0 : e->aliasing;
  return (const char*)((uintptr_t)ptr + delta);
}

// Reads string data from the input, aliasing into the input buffer instead of
// copying. The parsing pointer is passed in `*ptr`, and will be updated if
// necessary to point to the actual input buffer. Returns the new parsing
// pointer, which will be advanced past the string data.
//
// REQUIRES: Aliasing must be available for this data region (test with
// hpb_EpsCopyInputStream_AliasingAvailable().
HPB_INLINE const char* hpb_EpsCopyInputStream_ReadStringAliased(
    hpb_EpsCopyInputStream* e, const char** ptr, size_t size) {
  HPB_ASSUME(hpb_EpsCopyInputStream_AliasingAvailable(e, *ptr, size));
  const char* ret = *ptr + size;
  *ptr = hpb_EpsCopyInputStream_GetAliasedPtr(e, *ptr);
  HPB_ASSUME(ret != NULL);
  return ret;
}

// Skips `size` bytes of data from the input and returns a pointer past the end.
// Returns NULL on end of stream or error.
HPB_INLINE const char* hpb_EpsCopyInputStream_Skip(hpb_EpsCopyInputStream* e,
                                                   const char* ptr, int size) {
  if (!hpb_EpsCopyInputStream_CheckDataSizeAvailable(e, ptr, size)) return NULL;
  return ptr + size;
}

// Copies `size` bytes of data from the input `ptr` into the buffer `to`, and
// returns a pointer past the end. Returns NULL on end of stream or error.
HPB_INLINE const char* hpb_EpsCopyInputStream_Copy(hpb_EpsCopyInputStream* e,
                                                   const char* ptr, void* to,
                                                   int size) {
  if (!hpb_EpsCopyInputStream_CheckDataSizeAvailable(e, ptr, size)) return NULL;
  memcpy(to, ptr, size);
  return ptr + size;
}

// Reads string data from the stream and advances the pointer accordingly.
// If aliasing was enabled when the stream was initialized, then the returned
// pointer will point into the input buffer if possible, otherwise new data
// will be allocated from arena and copied into. We may be forced to copy even
// if aliasing was enabled if the input data spans input buffers.
//
// Returns NULL if memory allocation failed, or we reached a premature EOF.
HPB_INLINE const char* hpb_EpsCopyInputStream_ReadString(
    hpb_EpsCopyInputStream* e, const char** ptr, size_t size,
    hpb_Arena* arena) {
  if (hpb_EpsCopyInputStream_AliasingAvailable(e, *ptr, size)) {
    return hpb_EpsCopyInputStream_ReadStringAliased(e, ptr, size);
  } else {
    // We need to allocate and copy.
    if (!hpb_EpsCopyInputStream_CheckDataSizeAvailable(e, *ptr, size)) {
      return NULL;
    }
    HPB_ASSERT(arena);
    char* data = (char*)hpb_Arena_Malloc(arena, size);
    if (!data) return NULL;
    const char* ret = hpb_EpsCopyInputStream_Copy(e, *ptr, data, size);
    *ptr = data;
    return ret;
  }
}

HPB_INLINE void _hpb_EpsCopyInputStream_CheckLimit(hpb_EpsCopyInputStream* e) {
  HPB_ASSERT(e->limit_ptr == e->end + HPB_MIN(0, e->limit));
}

// Pushes a limit onto the stack of limits for the current stream.  The limit
// will extend for `size` bytes beyond the position in `ptr`.  Future calls to
// hpb_EpsCopyInputStream_IsDone() will return `true` when the stream position
// reaches this limit.
//
// Returns a delta that the caller must store and supply to PopLimit() below.
HPB_INLINE int hpb_EpsCopyInputStream_PushLimit(hpb_EpsCopyInputStream* e,
                                                const char* ptr, int size) {
  int limit = size + (int)(ptr - e->end);
  int delta = e->limit - limit;
  _hpb_EpsCopyInputStream_CheckLimit(e);
  HPB_ASSERT(limit <= e->limit);
  e->limit = limit;
  e->limit_ptr = e->end + HPB_MIN(0, limit);
  _hpb_EpsCopyInputStream_CheckLimit(e);
  return delta;
}

// Pops the last limit that was pushed on this stream.  This may only be called
// once IsDone() returns true.  The user must pass the delta that was returned
// from PushLimit().
HPB_INLINE void hpb_EpsCopyInputStream_PopLimit(hpb_EpsCopyInputStream* e,
                                                const char* ptr,
                                                int saved_delta) {
  HPB_ASSERT(ptr - e->end == e->limit);
  _hpb_EpsCopyInputStream_CheckLimit(e);
  e->limit += saved_delta;
  e->limit_ptr = e->end + HPB_MIN(0, e->limit);
  _hpb_EpsCopyInputStream_CheckLimit(e);
}

HPB_INLINE const char* _hpb_EpsCopyInputStream_IsDoneFallbackInline(
    hpb_EpsCopyInputStream* e, const char* ptr, int overrun,
    hpb_EpsCopyInputStream_BufferFlipCallback* callback) {
  if (overrun < e->limit) {
    // Need to copy remaining data into patch buffer.
    HPB_ASSERT(overrun < kHpb_EpsCopyInputStream_SlopBytes);
    const char* old_end = ptr;
    const char* new_start = &e->patch[0] + overrun;
    memset(e->patch + kHpb_EpsCopyInputStream_SlopBytes, 0,
           kHpb_EpsCopyInputStream_SlopBytes);
    memcpy(e->patch, e->end, kHpb_EpsCopyInputStream_SlopBytes);
    ptr = new_start;
    e->end = &e->patch[kHpb_EpsCopyInputStream_SlopBytes];
    e->limit -= kHpb_EpsCopyInputStream_SlopBytes;
    e->limit_ptr = e->end + e->limit;
    HPB_ASSERT(ptr < e->limit_ptr);
    if (e->aliasing != kHpb_EpsCopyInputStream_NoAliasing) {
      e->aliasing = (uintptr_t)old_end - (uintptr_t)new_start;
    }
    return callback(e, old_end, new_start);
  } else {
    HPB_ASSERT(overrun > e->limit);
    e->error = true;
    return callback(e, NULL, NULL);
  }
}

typedef const char* hpb_EpsCopyInputStream_ParseDelimitedFunc(
    hpb_EpsCopyInputStream* e, const char* ptr, void* ctx);

// Tries to perform a fast-path handling of the given delimited message data.
// If the sub-message beginning at `*ptr` and extending for `len` is short and
// fits within this buffer, calls `func` with `ctx` as a parameter, where the
// pushing and popping of limits is handled automatically and with lower cost
// than the normal PushLimit()/PopLimit() sequence.
static HPB_FORCEINLINE bool hpb_EpsCopyInputStream_TryParseDelimitedFast(
    hpb_EpsCopyInputStream* e, const char** ptr, int len,
    hpb_EpsCopyInputStream_ParseDelimitedFunc* func, void* ctx) {
  if (!hpb_EpsCopyInputStream_CheckSubMessageSizeAvailable(e, *ptr, len)) {
    return false;
  }

  // Fast case: Sub-message is <128 bytes and fits in the current buffer.
  // This means we can preserve limit/limit_ptr verbatim.
  const char* saved_limit_ptr = e->limit_ptr;
  int saved_limit = e->limit;
  e->limit_ptr = *ptr + len;
  e->limit = e->limit_ptr - e->end;
  HPB_ASSERT(e->limit_ptr == e->end + HPB_MIN(0, e->limit));
  *ptr = func(e, *ptr, ctx);
  e->limit_ptr = saved_limit_ptr;
  e->limit = saved_limit;
  HPB_ASSERT(e->limit_ptr == e->end + HPB_MIN(0, e->limit));
  return true;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_WIRE_EPS_COPY_INPUT_STREAM_H_
