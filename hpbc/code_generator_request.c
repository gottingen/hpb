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

#include "hpbc/code_generator_request.h"

#include <inttypes.h>

#include "google/protobuf/compiler/plugin.hpb.h"
#include "hpb/mini_descriptor/decode.h"
#include "hpb/reflection/def.h"

// Must be last.
#include "hpb/port/def.inc"

/******************************************************************************/

// Kitchen sink storage for all of our state as we build the mini descriptors.

typedef struct {
    hpb_Arena *arena;
    hpb_Status *status;
    hpb_DefPool *symtab;

    hpbc_CodeGeneratorRequest *out;

    jmp_buf jmp;
} hpbc_State;

static void hpbc_State_Fini(hpbc_State *s) {
    if (s->symtab) hpb_DefPool_Free(s->symtab);
}

HPB_NORETURN static void hpbc_Error(hpbc_State *s, const char *fn,
                                    const char *msg) {
    hpb_Status_SetErrorFormat(s->status, "%s(): %s", fn, msg);
    hpbc_State_Fini(s);
    HPB_LONGJMP(s->jmp, -1);
}

static void hpbc_State_Init(hpbc_State *s) {
    s->symtab = hpb_DefPool_New();
    if (!s->symtab) hpbc_Error(s, __func__, "could not allocate def pool");

    s->out = hpbc_CodeGeneratorRequest_new(s->arena);
    if (!s->out) hpbc_Error(s, __func__, "could not allocate request");
}

static hpb_StringView hpbc_State_StrDup(hpbc_State *s, const char *str) {
    hpb_StringView from = hpb_StringView_FromString(str);
    char *to = hpb_Arena_Malloc(s->arena, from.size);
    if (!to) hpbc_Error(s, __func__, "Out of memory");
    memcpy(to, from.data, from.size);
    return hpb_StringView_FromDataAndSize(to, from.size);
}

static void hpbc_State_AddMiniDescriptor(hpbc_State *s, const char *name,
                                         hpb_StringView encoding) {
    const hpb_StringView key = hpb_StringView_FromString(name);
    hpbc_CodeGeneratorRequest_HpbInfo *info =
            hpbc_CodeGeneratorRequest_HpbInfo_new(s->arena);
    if (!info) hpbc_Error(s, __func__, "Out of memory");
    hpbc_CodeGeneratorRequest_HpbInfo_set_mini_descriptor(info, encoding);
    bool ok = hpbc_CodeGeneratorRequest_hpb_info_set(s->out, key, info, s->arena);
    if (!ok) hpbc_Error(s, __func__, "could not set mini descriptor in map");
}

/******************************************************************************/

// Forward declaration.
static void hpbc_Scrape_Message(hpbc_State *, const hpb_MessageDef *);

static void hpbc_Scrape_Enum(hpbc_State *s, const hpb_EnumDef *e) {
    hpb_StringView desc;
    bool ok = hpb_EnumDef_MiniDescriptorEncode(e, s->arena, &desc);
    if (!ok) hpbc_Error(s, __func__, "could not encode enum");

    hpbc_State_AddMiniDescriptor(s, hpb_EnumDef_FullName(e), desc);
}

static void hpbc_Scrape_Extension(hpbc_State *s, const hpb_FieldDef *f) {
    hpb_StringView desc;
    bool ok = hpb_FieldDef_MiniDescriptorEncode(f, s->arena, &desc);
    if (!ok) hpbc_Error(s, __func__, "could not encode extension");

    hpbc_State_AddMiniDescriptor(s, hpb_FieldDef_FullName(f), desc);
}

static void hpbc_Scrape_FileEnums(hpbc_State *s, const hpb_FileDef *f) {
    const size_t len = hpb_FileDef_TopLevelEnumCount(f);

    for (size_t i = 0; i < len; i++) {
        hpbc_Scrape_Enum(s, hpb_FileDef_TopLevelEnum(f, i));
    }
}

static void hpbc_Scrape_FileExtensions(hpbc_State *s, const hpb_FileDef *f) {
    const size_t len = hpb_FileDef_TopLevelExtensionCount(f);

    for (size_t i = 0; i < len; i++) {
        hpbc_Scrape_Extension(s, hpb_FileDef_TopLevelExtension(f, i));
    }
}

static void hpbc_Scrape_FileMessages(hpbc_State *s, const hpb_FileDef *f) {
    const size_t len = hpb_FileDef_TopLevelMessageCount(f);

    for (size_t i = 0; i < len; i++) {
        hpbc_Scrape_Message(s, hpb_FileDef_TopLevelMessage(f, i));
    }
}

static void hpbc_Scrape_File(hpbc_State *s, const hpb_FileDef *f) {
    hpbc_Scrape_FileEnums(s, f);
    hpbc_Scrape_FileExtensions(s, f);
    hpbc_Scrape_FileMessages(s, f);
}

static void hpbc_Scrape_Files(hpbc_State *s) {
    const google_protobuf_compiler_CodeGeneratorRequest *request =
            hpbc_CodeGeneratorRequest_request(s->out);

    size_t len = 0;
    const google_protobuf_FileDescriptorProto *const *files =
            google_protobuf_compiler_CodeGeneratorRequest_proto_file(request, &len);

    for (size_t i = 0; i < len; i++) {
        const hpb_FileDef *f = hpb_DefPool_AddFile(s->symtab, files[i], s->status);
        if (!f) hpbc_Error(s, __func__, "could not add file to def pool");

        hpbc_Scrape_File(s, f);
    }
}

static void hpbc_Scrape_NestedEnums(hpbc_State *s, const hpb_MessageDef *m) {
    const size_t len = hpb_MessageDef_NestedEnumCount(m);

    for (size_t i = 0; i < len; i++) {
        hpbc_Scrape_Enum(s, hpb_MessageDef_NestedEnum(m, i));
    }
}

static void hpbc_Scrape_NestedExtensions(hpbc_State *s,
                                         const hpb_MessageDef *m) {
    const size_t len = hpb_MessageDef_NestedExtensionCount(m);

    for (size_t i = 0; i < len; i++) {
        hpbc_Scrape_Extension(s, hpb_MessageDef_NestedExtension(m, i));
    }
}

static void hpbc_Scrape_NestedMessages(hpbc_State *s, const hpb_MessageDef *m) {
    const size_t len = hpb_MessageDef_NestedMessageCount(m);

    for (size_t i = 0; i < len; i++) {
        hpbc_Scrape_Message(s, hpb_MessageDef_NestedMessage(m, i));
    }
}

static void hpbc_Scrape_MessageSubs(hpbc_State *s,
                                    hpbc_CodeGeneratorRequest_HpbInfo *info,
                                    const hpb_MessageDef *m) {
    const hpb_MiniTableField **fields =
            malloc(hpb_MessageDef_FieldCount(m) * sizeof(*fields));
    const hpb_MiniTable *mt = hpb_MessageDef_MiniTable(m);
    uint32_t counts = hpb_MiniTable_GetSubList(mt, fields);
    uint32_t msg_count = counts >> 16;
    uint32_t enum_count = counts & 0xffff;

    for (uint32_t i = 0; i < msg_count; i++) {
        const hpb_FieldDef *f =
                hpb_MessageDef_FindFieldByNumber(m, fields[i]->number);
        if (!f) hpbc_Error(s, __func__, "Missing f");
        const hpb_MessageDef *sub = hpb_FieldDef_MessageSubDef(f);
        if (!sub) hpbc_Error(s, __func__, "Missing sub");
        hpb_StringView name = hpbc_State_StrDup(s, hpb_MessageDef_FullName(sub));
        hpbc_CodeGeneratorRequest_HpbInfo_add_sub_message(info, name, s->arena);
    }

    for (uint32_t i = 0; i < enum_count; i++) {
        const hpb_FieldDef *f =
                hpb_MessageDef_FindFieldByNumber(m, fields[msg_count + i]->number);
        if (!f) hpbc_Error(s, __func__, "Missing f (2)");
        const hpb_EnumDef *sub = hpb_FieldDef_EnumSubDef(f);
        if (!sub) hpbc_Error(s, __func__, "Missing sub (2)");
        hpb_StringView name = hpbc_State_StrDup(s, hpb_EnumDef_FullName(sub));
        hpbc_CodeGeneratorRequest_HpbInfo_add_sub_enum(info, name, s->arena);
    }

    free(fields);
}

static void hpbc_Scrape_Message(hpbc_State *s, const hpb_MessageDef *m) {
    hpb_StringView desc;
    bool ok = hpb_MessageDef_MiniDescriptorEncode(m, s->arena, &desc);
    if (!ok) hpbc_Error(s, __func__, "could not encode message");

    hpbc_CodeGeneratorRequest_HpbInfo *info =
            hpbc_CodeGeneratorRequest_HpbInfo_new(s->arena);
    if (!info) hpbc_Error(s, __func__, "Out of memory");
    hpbc_CodeGeneratorRequest_HpbInfo_set_mini_descriptor(info, desc);

    hpbc_Scrape_MessageSubs(s, info, m);

    const hpb_StringView key = hpbc_State_StrDup(s, hpb_MessageDef_FullName(m));
    ok = hpbc_CodeGeneratorRequest_hpb_info_set(s->out, key, info, s->arena);
    if (!ok) hpbc_Error(s, __func__, "could not set mini descriptor in map");

    hpbc_Scrape_NestedEnums(s, m);
    hpbc_Scrape_NestedExtensions(s, m);
    hpbc_Scrape_NestedMessages(s, m);
}

static hpbc_CodeGeneratorRequest *hpbc_State_MakeCodeGeneratorRequest(
        hpbc_State *const s, google_protobuf_compiler_CodeGeneratorRequest *const request) {
    if (HPB_SETJMP(s->jmp)) return NULL;
    hpbc_State_Init(s);

    hpbc_CodeGeneratorRequest_set_request(s->out, request);
    hpbc_Scrape_Files(s);
    hpbc_State_Fini(s);
    return s->out;
}

hpbc_CodeGeneratorRequest *hpbc_MakeCodeGeneratorRequest(
        google_protobuf_compiler_CodeGeneratorRequest *request, hpb_Arena *arena,
        hpb_Status *status) {
    hpbc_State s = {
            .arena = arena,
            .status = status,
            .symtab = NULL,
            .out = NULL,
    };

    return hpbc_State_MakeCodeGeneratorRequest(&s, request);
}
