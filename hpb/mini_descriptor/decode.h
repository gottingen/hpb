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

#ifndef HPB_MINI_TABLE_DECODE_H_
#define HPB_MINI_TABLE_DECODE_H_

#include "hpb/base/status.h"
#include "hpb/mem/arena.h"
#include "hpb/mini_table/extension.h"
#include "hpb/mini_table/field.h"
#include "hpb/mini_table/message.h"
#include "hpb/mini_table/sub.h"

// Export the newer headers, for legacy users.  New users should include the
// more specific headers directly.
// IWYU pragma: begin_exports
#include "hpb/mini_descriptor/build_enum.h"
#include "hpb/mini_descriptor/link.h"
// IWYU pragma: end_exports

// Must be last.
#include "hpb/port/def.inc"

typedef enum {
    kHpb_MiniTablePlatform_32Bit,
    kHpb_MiniTablePlatform_64Bit,
    kHpb_MiniTablePlatform_Native =
    HPB_SIZE(kHpb_MiniTablePlatform_32Bit, kHpb_MiniTablePlatform_64Bit),
} hpb_MiniTablePlatform;

#ifdef __cplusplus
extern "C" {
#endif

// Builds a mini table from the data encoded in the buffer [data, len]. If any
// errors occur, returns NULL and sets a status message. In the success case,
// the caller must call hpb_MiniTable_SetSub*() for all message or proto2 enum
// fields to link the table to the appropriate sub-tables.
hpb_MiniTable *_hpb_MiniTable_Build(const char *data, size_t len,
                                    hpb_MiniTablePlatform platform,
                                    hpb_Arena *arena, hpb_Status *status);

HPB_API_INLINE hpb_MiniTable *hpb_MiniTable_Build(const char *data, size_t len,
                                                  hpb_Arena *arena,
                                                  hpb_Status *status) {
    return _hpb_MiniTable_Build(data, len, kHpb_MiniTablePlatform_Native, arena,
                                status);
}

// Initializes a MiniTableExtension buffer that has already been allocated.
// This is needed by hpb_FileDef and hpb_MessageDef, which allocate all of the
// extensions together in a single contiguous array.
const char *_hpb_MiniTableExtension_Init(const char *data, size_t len,
                                         hpb_MiniTableExtension *ext,
                                         const hpb_MiniTable *extendee,
                                         hpb_MiniTableSub sub,
                                         hpb_MiniTablePlatform platform,
                                         hpb_Status *status);

HPB_API_INLINE const char *hpb_MiniTableExtension_Init(
        const char *data, size_t len, hpb_MiniTableExtension *ext,
        const hpb_MiniTable *extendee, hpb_MiniTableSub sub, hpb_Status *status) {
    return _hpb_MiniTableExtension_Init(data, len, ext, extendee, sub,
                                        kHpb_MiniTablePlatform_Native, status);
}

HPB_API hpb_MiniTableExtension *_hpb_MiniTableExtension_Build(
        const char *data, size_t len, const hpb_MiniTable *extendee,
        hpb_MiniTableSub sub, hpb_MiniTablePlatform platform, hpb_Arena *arena,
        hpb_Status *status);

HPB_API_INLINE hpb_MiniTableExtension *hpb_MiniTableExtension_Build(
        const char *data, size_t len, const hpb_MiniTable *extendee,
        hpb_Arena *arena, hpb_Status *status) {
    hpb_MiniTableSub sub;
    sub.submsg = NULL;
    return _hpb_MiniTableExtension_Build(
            data, len, extendee, sub, kHpb_MiniTablePlatform_Native, arena, status);
}

HPB_API_INLINE hpb_MiniTableExtension *hpb_MiniTableExtension_BuildMessage(
        const char *data, size_t len, const hpb_MiniTable *extendee,
        hpb_MiniTable *submsg, hpb_Arena *arena, hpb_Status *status) {
    hpb_MiniTableSub sub;
    sub.submsg = submsg;
    return _hpb_MiniTableExtension_Build(
            data, len, extendee, sub, kHpb_MiniTablePlatform_Native, arena, status);
}

HPB_API_INLINE hpb_MiniTableExtension *hpb_MiniTableExtension_BuildEnum(
        const char *data, size_t len, const hpb_MiniTable *extendee,
        hpb_MiniTableEnum *subenum, hpb_Arena *arena, hpb_Status *status) {
    hpb_MiniTableSub sub;
    sub.subenum = subenum;
    return _hpb_MiniTableExtension_Build(
            data, len, extendee, sub, kHpb_MiniTablePlatform_Native, arena, status);
}

// Like hpb_MiniTable_Build(), but the user provides a buffer of layout data so
// it can be reused from call to call, avoiding repeated realloc()/free().
//
// The caller owns `*buf` both before and after the call, and must free() it
// when it is no longer in use.  The function will realloc() `*buf` as
// necessary, updating `*size` accordingly.
hpb_MiniTable *hpb_MiniTable_BuildWithBuf(const char *data, size_t len,
                                          hpb_MiniTablePlatform platform,
                                          hpb_Arena *arena, void **buf,
                                          size_t *buf_size, hpb_Status *status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_TABLE_DECODE_H_
