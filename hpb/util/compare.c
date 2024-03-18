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

#include "hpb/util/compare.h"

#include <stdlib.h>

#include "hpb/base/string_view.h"
#include "hpb/wire/eps_copy_input_stream.h"
#include "hpb/wire/reader.h"
#include "hpb/wire/types.h"
// Must be last.
#include "hpb/port/def.inc"

struct hpb_UnknownFields;
typedef struct hpb_UnknownFields hpb_UnknownFields;

typedef struct {
  uint32_t tag;
  union {
    uint64_t varint;
    uint64_t uint64;
    uint32_t uint32;
    hpb_StringView delimited;
    hpb_UnknownFields* group;
  } data;
} hpb_UnknownField;

struct hpb_UnknownFields {
  size_t size;
  size_t capacity;
  hpb_UnknownField* fields;
};

typedef struct {
  hpb_EpsCopyInputStream stream;
  hpb_Arena* arena;
  hpb_UnknownField* tmp;
  size_t tmp_size;
  int depth;
  hpb_UnknownCompareResult status;
  jmp_buf err;
} hpb_UnknownField_Context;

HPB_NORETURN static void hpb_UnknownFields_OutOfMemory(
    hpb_UnknownField_Context* ctx) {
  ctx->status = kHpb_UnknownCompareResult_OutOfMemory;
  HPB_LONGJMP(ctx->err, 1);
}

static void hpb_UnknownFields_Grow(hpb_UnknownField_Context* ctx,
                                   hpb_UnknownField** base,
                                   hpb_UnknownField** ptr,
                                   hpb_UnknownField** end) {
  size_t old = (*ptr - *base);
  size_t new = HPB_MAX(4, old * 2);

  *base = hpb_Arena_Realloc(ctx->arena, *base, old * sizeof(**base),
                            new * sizeof(**base));
  if (!*base) hpb_UnknownFields_OutOfMemory(ctx);

  *ptr = *base + old;
  *end = *base + new;
}

// We have to implement our own sort here, since qsort() is not an in-order
// sort. Here we use merge sort, the simplest in-order sort.
static void hpb_UnknownFields_Merge(hpb_UnknownField* arr, size_t start,
                                    size_t mid, size_t end,
                                    hpb_UnknownField* tmp) {
  memcpy(tmp, &arr[start], (end - start) * sizeof(*tmp));

  hpb_UnknownField* ptr1 = tmp;
  hpb_UnknownField* end1 = &tmp[mid - start];
  hpb_UnknownField* ptr2 = &tmp[mid - start];
  hpb_UnknownField* end2 = &tmp[end - start];
  hpb_UnknownField* out = &arr[start];

  while (ptr1 < end1 && ptr2 < end2) {
    if (ptr1->tag <= ptr2->tag) {
      *out++ = *ptr1++;
    } else {
      *out++ = *ptr2++;
    }
  }

  if (ptr1 < end1) {
    memcpy(out, ptr1, (end1 - ptr1) * sizeof(*out));
  } else if (ptr2 < end2) {
    memcpy(out, ptr1, (end2 - ptr2) * sizeof(*out));
  }
}

static void hpb_UnknownFields_SortRecursive(hpb_UnknownField* arr, size_t start,
                                            size_t end, hpb_UnknownField* tmp) {
  if (end - start > 1) {
    size_t mid = start + ((end - start) / 2);
    hpb_UnknownFields_SortRecursive(arr, start, mid, tmp);
    hpb_UnknownFields_SortRecursive(arr, mid, end, tmp);
    hpb_UnknownFields_Merge(arr, start, mid, end, tmp);
  }
}

static void hpb_UnknownFields_Sort(hpb_UnknownField_Context* ctx,
                                   hpb_UnknownFields* fields) {
  if (ctx->tmp_size < fields->size) {
    ctx->tmp_size = HPB_MAX(8, ctx->tmp_size);
    while (ctx->tmp_size < fields->size) ctx->tmp_size *= 2;
    ctx->tmp = realloc(ctx->tmp, ctx->tmp_size * sizeof(*ctx->tmp));
  }
  hpb_UnknownFields_SortRecursive(fields->fields, 0, fields->size, ctx->tmp);
}

static hpb_UnknownFields* hpb_UnknownFields_DoBuild(
    hpb_UnknownField_Context* ctx, const char** buf) {
  hpb_UnknownField* arr_base = NULL;
  hpb_UnknownField* arr_ptr = NULL;
  hpb_UnknownField* arr_end = NULL;
  const char* ptr = *buf;
  uint32_t last_tag = 0;
  bool sorted = true;
  while (!hpb_EpsCopyInputStream_IsDone(&ctx->stream, &ptr)) {
    uint32_t tag;
    ptr = hpb_WireReader_ReadTag(ptr, &tag);
    HPB_ASSERT(tag <= UINT32_MAX);
    int wire_type = hpb_WireReader_GetWireType(tag);
    if (wire_type == kHpb_WireType_EndGroup) break;
    if (tag < last_tag) sorted = false;
    last_tag = tag;

    if (arr_ptr == arr_end) {
      hpb_UnknownFields_Grow(ctx, &arr_base, &arr_ptr, &arr_end);
    }
    hpb_UnknownField* field = arr_ptr;
    field->tag = tag;
    arr_ptr++;

    switch (wire_type) {
      case kHpb_WireType_Varint:
        ptr = hpb_WireReader_ReadVarint(ptr, &field->data.varint);
        break;
      case kHpb_WireType_64Bit:
        ptr = hpb_WireReader_ReadFixed64(ptr, &field->data.uint64);
        break;
      case kHpb_WireType_32Bit:
        ptr = hpb_WireReader_ReadFixed32(ptr, &field->data.uint32);
        break;
      case kHpb_WireType_Delimited: {
        int size;
        ptr = hpb_WireReader_ReadSize(ptr, &size);
        const char* s_ptr = ptr;
        ptr = hpb_EpsCopyInputStream_ReadStringAliased(&ctx->stream, &s_ptr,
                                                       size);
        field->data.delimited.data = s_ptr;
        field->data.delimited.size = size;
        break;
      }
      case kHpb_WireType_StartGroup:
        if (--ctx->depth == 0) {
          ctx->status = kHpb_UnknownCompareResult_MaxDepthExceeded;
          HPB_LONGJMP(ctx->err, 1);
        }
        field->data.group = hpb_UnknownFields_DoBuild(ctx, &ptr);
        ctx->depth++;
        break;
      default:
        HPB_UNREACHABLE();
    }
  }

  *buf = ptr;
  hpb_UnknownFields* ret = hpb_Arena_Malloc(ctx->arena, sizeof(*ret));
  if (!ret) hpb_UnknownFields_OutOfMemory(ctx);
  ret->fields = arr_base;
  ret->size = arr_ptr - arr_base;
  ret->capacity = arr_end - arr_base;
  if (!sorted) {
    hpb_UnknownFields_Sort(ctx, ret);
  }
  return ret;
}

// Builds a hpb_UnknownFields data structure from the binary data in buf.
static hpb_UnknownFields* hpb_UnknownFields_Build(hpb_UnknownField_Context* ctx,
                                                  const char* ptr,
                                                  size_t size) {
  hpb_EpsCopyInputStream_Init(&ctx->stream, &ptr, size, true);
  hpb_UnknownFields* fields = hpb_UnknownFields_DoBuild(ctx, &ptr);
  HPB_ASSERT(hpb_EpsCopyInputStream_IsDone(&ctx->stream, &ptr) &&
             !hpb_EpsCopyInputStream_IsError(&ctx->stream));
  return fields;
}

// Compares two sorted hpb_UnknownFields structures for equality.
static bool hpb_UnknownFields_IsEqual(const hpb_UnknownFields* uf1,
                                      const hpb_UnknownFields* uf2) {
  if (uf1->size != uf2->size) return false;
  for (size_t i = 0, n = uf1->size; i < n; i++) {
    hpb_UnknownField* f1 = &uf1->fields[i];
    hpb_UnknownField* f2 = &uf2->fields[i];
    if (f1->tag != f2->tag) return false;
    int wire_type = f1->tag & 7;
    switch (wire_type) {
      case kHpb_WireType_Varint:
        if (f1->data.varint != f2->data.varint) return false;
        break;
      case kHpb_WireType_64Bit:
        if (f1->data.uint64 != f2->data.uint64) return false;
        break;
      case kHpb_WireType_32Bit:
        if (f1->data.uint32 != f2->data.uint32) return false;
        break;
      case kHpb_WireType_Delimited:
        if (!hpb_StringView_IsEqual(f1->data.delimited, f2->data.delimited)) {
          return false;
        }
        break;
      case kHpb_WireType_StartGroup:
        if (!hpb_UnknownFields_IsEqual(f1->data.group, f2->data.group)) {
          return false;
        }
        break;
      default:
        HPB_UNREACHABLE();
    }
  }
  return true;
}

static hpb_UnknownCompareResult hpb_UnknownField_DoCompare(
    hpb_UnknownField_Context* ctx, const char* buf1, size_t size1,
    const char* buf2, size_t size2) {
  hpb_UnknownCompareResult ret;
  // First build both unknown fields into a sorted data structure (similar
  // to the UnknownFieldSet in C++).
  hpb_UnknownFields* uf1 = hpb_UnknownFields_Build(ctx, buf1, size1);
  hpb_UnknownFields* uf2 = hpb_UnknownFields_Build(ctx, buf2, size2);

  // Now perform the equality check on the sorted structures.
  if (hpb_UnknownFields_IsEqual(uf1, uf2)) {
    ret = kHpb_UnknownCompareResult_Equal;
  } else {
    ret = kHpb_UnknownCompareResult_NotEqual;
  }
  return ret;
}

static hpb_UnknownCompareResult hpb_UnknownField_Compare(
    hpb_UnknownField_Context* const ctx, const char* const buf1,
    const size_t size1, const char* const buf2, const size_t size2) {
  hpb_UnknownCompareResult ret;
  if (HPB_SETJMP(ctx->err) == 0) {
    ret = hpb_UnknownField_DoCompare(ctx, buf1, size1, buf2, size2);
  } else {
    ret = ctx->status;
    HPB_ASSERT(ret != kHpb_UnknownCompareResult_Equal);
  }

  hpb_Arena_Free(ctx->arena);
  free(ctx->tmp);
  return ret;
}

hpb_UnknownCompareResult hpb_Message_UnknownFieldsAreEqual(const char* buf1,
                                                           size_t size1,
                                                           const char* buf2,
                                                           size_t size2,
                                                           int max_depth) {
  if (size1 == 0 && size2 == 0) return kHpb_UnknownCompareResult_Equal;
  if (size1 == 0 || size2 == 0) return kHpb_UnknownCompareResult_NotEqual;
  if (memcmp(buf1, buf2, size1) == 0) return kHpb_UnknownCompareResult_Equal;

  hpb_UnknownField_Context ctx = {
      .arena = hpb_Arena_New(),
      .depth = max_depth,
      .tmp = NULL,
      .tmp_size = 0,
      .status = kHpb_UnknownCompareResult_Equal,
  };

  if (!ctx.arena) return kHpb_UnknownCompareResult_OutOfMemory;

  return hpb_UnknownField_Compare(&ctx, buf1, size1, buf2, size2);
}
