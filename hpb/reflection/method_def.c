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

#include "hpb/reflection/internal/method_def.h"

#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/service_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_MethodDef {
  const HPB_DESC(MethodOptions) * opts;
  hpb_ServiceDef* service;
  const char* full_name;
  const hpb_MessageDef* input_type;
  const hpb_MessageDef* output_type;
  int index;
  bool client_streaming;
  bool server_streaming;
};

hpb_MethodDef* _hpb_MethodDef_At(const hpb_MethodDef* m, int i) {
  return (hpb_MethodDef*)&m[i];
}

const hpb_ServiceDef* hpb_MethodDef_Service(const hpb_MethodDef* m) {
  return m->service;
}

const HPB_DESC(MethodOptions) * hpb_MethodDef_Options(const hpb_MethodDef* m) {
  return m->opts;
}

bool hpb_MethodDef_HasOptions(const hpb_MethodDef* m) {
  return m->opts != (void*)kHpbDefOptDefault;
}

const char* hpb_MethodDef_FullName(const hpb_MethodDef* m) {
  return m->full_name;
}

const char* hpb_MethodDef_Name(const hpb_MethodDef* m) {
  return _hpb_DefBuilder_FullToShort(m->full_name);
}

int hpb_MethodDef_Index(const hpb_MethodDef* m) { return m->index; }

const hpb_MessageDef* hpb_MethodDef_InputType(const hpb_MethodDef* m) {
  return m->input_type;
}

const hpb_MessageDef* hpb_MethodDef_OutputType(const hpb_MethodDef* m) {
  return m->output_type;
}

bool hpb_MethodDef_ClientStreaming(const hpb_MethodDef* m) {
  return m->client_streaming;
}

bool hpb_MethodDef_ServerStreaming(const hpb_MethodDef* m) {
  return m->server_streaming;
}

static void create_method(hpb_DefBuilder* ctx,
                          const HPB_DESC(MethodDescriptorProto) * method_proto,
                          hpb_ServiceDef* s, hpb_MethodDef* m) {
  hpb_StringView name = HPB_DESC(MethodDescriptorProto_name)(method_proto);

  m->service = s;
  m->full_name =
      _hpb_DefBuilder_MakeFullName(ctx, hpb_ServiceDef_FullName(s), name);
  m->client_streaming =
      HPB_DESC(MethodDescriptorProto_client_streaming)(method_proto);
  m->server_streaming =
      HPB_DESC(MethodDescriptorProto_server_streaming)(method_proto);
  m->input_type = _hpb_DefBuilder_Resolve(
      ctx, m->full_name, m->full_name,
      HPB_DESC(MethodDescriptorProto_input_type)(method_proto),
      HPB_DEFTYPE_MSG);
  m->output_type = _hpb_DefBuilder_Resolve(
      ctx, m->full_name, m->full_name,
      HPB_DESC(MethodDescriptorProto_output_type)(method_proto),
      HPB_DEFTYPE_MSG);

  HPB_DEF_SET_OPTIONS(m->opts, MethodDescriptorProto, MethodOptions,
                      method_proto);
}

// Allocate and initialize an array of |n| method defs belonging to |s|.
hpb_MethodDef* _hpb_MethodDefs_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(MethodDescriptorProto) * const* protos, hpb_ServiceDef* s) {
  hpb_MethodDef* m = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_MethodDef) * n);
  for (int i = 0; i < n; i++) {
    create_method(ctx, protos[i], s, &m[i]);
    m[i].index = i;
  }
  return m;
}
