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

#include "hpbc/get_used_fields.h"

#include "google/protobuf/descriptor.hpb.h"
#include "google/protobuf/compiler/plugin.hpb.h"
#include "hpb/reflection/def_pool.h"
#include "hpb/reflection/field_def.h"
#include "hpb/reflection/message.h"
#include "hpb/reflection/message_def.h"
#include "hpb/wire/decode.h"

// Must be last.
#include "hpb/port/def.inc"

#define hpbdev_Err(...)           \
  {                               \
    fprintf(stderr, __VA_ARGS__); \
    exit(1);                      \
  }

typedef struct {
  char* buf;
  size_t size;
  size_t capacity;
  hpb_Arena* arena;
} hpbdev_StringBuf;

void hpbdev_StringBuf_Add(hpbdev_StringBuf* buf, const char* sym) {
  size_t len = strlen(sym);
  size_t need = buf->size + len + (buf->size != 0);
  if (need > buf->capacity) {
    size_t new_cap = HPB_MAX(buf->capacity, 32);
    while (need > new_cap) new_cap *= 2;
    buf->buf = hpb_Arena_Realloc(buf->arena, buf->buf, buf->capacity, new_cap);
    buf->capacity = new_cap;
  }
  if (buf->size != 0) {
    buf->buf[buf->size++] = '\n';  // Separator
  }
  memcpy(buf->buf + buf->size, sym, len);
  buf->size = need;
}

void hpbdev_VisitMessage(hpbdev_StringBuf* buf, const hpb_Message* msg,
                         const hpb_MessageDef* m) {
  size_t iter = kHpb_Message_Begin;
  const hpb_FieldDef* f;
  hpb_MessageValue val;
  while (hpb_Message_Next(msg, m, NULL, &f, &val, &iter)) {
    // This could be a duplicate, but we don't worry about it; we'll dedupe
    // one level up.
    hpbdev_StringBuf_Add(buf, hpb_FieldDef_FullName(f));

    if (hpb_FieldDef_CType(f) != kHpb_CType_Message) continue;
    const hpb_MessageDef* sub = hpb_FieldDef_MessageSubDef(f);

    if (hpb_FieldDef_IsMap(f)) {
      const hpb_Map* map = val.map_val;
      size_t iter = kHpb_Map_Begin;
      hpb_MessageValue map_key, map_val;
      while (hpb_Map_Next(map, &map_key, &map_val, &iter)) {
        hpbdev_VisitMessage(buf, map_val.msg_val, sub);
      }
    } else if (hpb_FieldDef_IsRepeated(f)) {
      const hpb_Array* arr = val.array_val;
      size_t n = hpb_Array_Size(arr);
      for (size_t i = 0; i < n; i++) {
        hpb_MessageValue val = hpb_Array_Get(arr, i);
        hpbdev_VisitMessage(buf, val.msg_val, sub);
      }
    } else {
      hpbdev_VisitMessage(buf, val.msg_val, sub);
    }
  }
}

hpb_StringView hpbdev_GetUsedFields(const char* request, size_t request_size,
                                    const char* payload, size_t payload_size,
                                    const char* message_name,
                                    hpb_Arena* arena) {
  hpb_Arena* tmp_arena = hpb_Arena_New();
  google_protobuf_compiler_CodeGeneratorRequest* request_proto =
      google_protobuf_compiler_CodeGeneratorRequest_parse(request, request_size,
                                                 tmp_arena);
  if (!request_proto) hpbdev_Err("Couldn't parse request proto\n");

  size_t len;
  const google_protobuf_FileDescriptorProto* const* files =
      google_protobuf_compiler_CodeGeneratorRequest_proto_file(request_proto, &len);

  hpb_DefPool* pool = hpb_DefPool_New();
  for (size_t i = 0; i < len; i++) {
    const hpb_FileDef* f = hpb_DefPool_AddFile(pool, files[i], NULL);
    if (!f) hpbdev_Err("could not add file to def pool\n");
  }

  const hpb_MessageDef* m = hpb_DefPool_FindMessageByName(pool, message_name);
  if (!m) hpbdev_Err("Couldn't find message name\n");

  const hpb_MiniTable* mt = hpb_MessageDef_MiniTable(m);
  hpb_Message* msg = hpb_Message_New(mt, tmp_arena);
  hpb_DecodeStatus st =
      hpb_Decode(payload, payload_size, msg, mt, NULL, 0, tmp_arena);
  if (st != kHpb_DecodeStatus_Ok) hpbdev_Err("Error parsing payload: %d\n", st);

  hpbdev_StringBuf buf = {
      .buf = NULL,
      .size = 0,
      .capacity = 0,
      .arena = arena,
  };
  hpbdev_VisitMessage(&buf, msg, m);
  return hpb_StringView_FromDataAndSize(buf.buf, buf.size);
}
