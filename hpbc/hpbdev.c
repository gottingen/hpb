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

#include "hpbc/hpbdev.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else  // _WIN32

#include <unistd.h>

#endif  // !_WIN32

#include "google/protobuf/compiler/plugin.hpb.h"
#include "google/protobuf/compiler/plugin.hpbdefs.h"
#include "hpb/base/status.h"
#include "hpb/json/decode.h"
#include "hpb/json/encode.h"
#include "hpb/mem/arena.h"
#include "hpbc/code_generator_request.h"
#include "hpbc/code_generator_request.hpbb.h"
#include "hpbc/code_generator_request.hpbdefs.h"

static google_protobuf_compiler_CodeGeneratorResponse *hpbc_JsonDecode(
        const char *data, size_t size, hpb_Arena *arena, hpb_Status *status) {
    google_protobuf_compiler_CodeGeneratorResponse *response =
            google_protobuf_compiler_CodeGeneratorResponse_new(arena);

    hpb_DefPool *s = hpb_DefPool_New();
    const hpb_MessageDef *m = google_protobuf_compiler_CodeGeneratorResponse_getmsgdef(s);

    (void) hpb_JsonDecode(data, size, response, m, s, 0, arena, status);
    if (!hpb_Status_IsOk(status)) return NULL;

    hpb_DefPool_Free(s);

    return response;
}

static hpb_StringView hpbc_JsonEncode(const hpbc_CodeGeneratorRequest *request,
                                      hpb_Arena *arena, hpb_Status *status) {
    hpb_StringView out = {.data = NULL, .size = 0};

    hpb_DefPool *s = hpb_DefPool_New();
    const hpb_MessageDef *m = hpbc_CodeGeneratorRequest_getmsgdef(s);
    const int options = hpb_JsonEncode_FormatEnumsAsIntegers;

    out.size = hpb_JsonEncode(request, m, s, options, NULL, 0, status);
    if (!hpb_Status_IsOk(status)) goto done;

    char *data = (char *) hpb_Arena_Malloc(arena, out.size + 1);

    (void) hpb_JsonEncode(request, m, s, options, data, out.size + 1, status);
    if (!hpb_Status_IsOk(status)) goto done;

    out.data = (const char *) data;

    done:
    hpb_DefPool_Free(s);
    return out;
}

hpb_StringView hpbdev_ProcessInput(const char *buf, size_t size,
                                   hpb_Arena *arena, hpb_Status *status) {
    hpb_StringView out = {.data = NULL, .size = 0};

    google_protobuf_compiler_CodeGeneratorRequest *inner_request =
            google_protobuf_compiler_CodeGeneratorRequest_parse(buf, size, arena);

    const hpbc_CodeGeneratorRequest *outer_request =
            hpbc_MakeCodeGeneratorRequest(inner_request, arena, status);
    if (!hpb_Status_IsOk(status)) return out;

    return hpbc_JsonEncode(outer_request, arena, status);
}

hpb_StringView hpbdev_ProcessOutput(const char *buf, size_t size,
                                    hpb_Arena *arena, hpb_Status *status) {
    hpb_StringView out = {.data = NULL, .size = 0};

    const google_protobuf_compiler_CodeGeneratorResponse *response =
            hpbc_JsonDecode(buf, size, arena, status);
    if (!hpb_Status_IsOk(status)) return out;

    out.data = google_protobuf_compiler_CodeGeneratorResponse_serialize(response, arena,
                                                                        &out.size);
    return out;
}

void hpbdev_ProcessStdout(const char *buf, size_t size, hpb_Arena *arena,
                          hpb_Status *status) {
    const hpb_StringView sv = hpbdev_ProcessOutput(buf, size, arena, status);
    if (!hpb_Status_IsOk(status)) return;

    const char *ptr = sv.data;
    size_t len = sv.size;
    while (len) {
        int n = write(1, ptr, len);
        if (n > 0) {
            ptr += n;
            len -= n;
        }
    }
}

hpb_Arena *hpbdev_Arena_New() { return hpb_Arena_New(); }

void hpbdev_Status_Clear(hpb_Status *status) { hpb_Status_Clear(status); }
