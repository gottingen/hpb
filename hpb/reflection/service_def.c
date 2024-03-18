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

#include "hpb/reflection/internal/service_def.h"

#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/file_def.h"
#include "hpb/reflection/internal/method_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_ServiceDef {
  const HPB_DESC(ServiceOptions) * opts;
  const hpb_FileDef* file;
  const char* full_name;
  hpb_MethodDef* methods;
  int method_count;
  int index;
};

hpb_ServiceDef* _hpb_ServiceDef_At(const hpb_ServiceDef* s, int index) {
  return (hpb_ServiceDef*)&s[index];
}

const HPB_DESC(ServiceOptions) *
    hpb_ServiceDef_Options(const hpb_ServiceDef* s) {
  return s->opts;
}

bool hpb_ServiceDef_HasOptions(const hpb_ServiceDef* s) {
  return s->opts != (void*)kHpbDefOptDefault;
}

const char* hpb_ServiceDef_FullName(const hpb_ServiceDef* s) {
  return s->full_name;
}

const char* hpb_ServiceDef_Name(const hpb_ServiceDef* s) {
  return _hpb_DefBuilder_FullToShort(s->full_name);
}

int hpb_ServiceDef_Index(const hpb_ServiceDef* s) { return s->index; }

const hpb_FileDef* hpb_ServiceDef_File(const hpb_ServiceDef* s) {
  return s->file;
}

int hpb_ServiceDef_MethodCount(const hpb_ServiceDef* s) {
  return s->method_count;
}

const hpb_MethodDef* hpb_ServiceDef_Method(const hpb_ServiceDef* s, int i) {
  return (i < 0 || i >= s->method_count) ? NULL
                                         : _hpb_MethodDef_At(s->methods, i);
}

const hpb_MethodDef* hpb_ServiceDef_FindMethodByName(const hpb_ServiceDef* s,
                                                     const char* name) {
  for (int i = 0; i < s->method_count; i++) {
    const hpb_MethodDef* m = _hpb_MethodDef_At(s->methods, i);
    if (strcmp(name, hpb_MethodDef_Name(m)) == 0) {
      return m;
    }
  }
  return NULL;
}

static void create_service(hpb_DefBuilder* ctx,
                           const HPB_DESC(ServiceDescriptorProto) * svc_proto,
                           hpb_ServiceDef* s) {
  hpb_StringView name;
  size_t n;

  // Must happen before _hpb_DefBuilder_Add()
  s->file = _hpb_DefBuilder_File(ctx);

  name = HPB_DESC(ServiceDescriptorProto_name)(svc_proto);
  const char* package = _hpb_FileDef_RawPackage(s->file);
  s->full_name = _hpb_DefBuilder_MakeFullName(ctx, package, name);
  _hpb_DefBuilder_Add(ctx, s->full_name,
                      _hpb_DefType_Pack(s, HPB_DEFTYPE_SERVICE));

  const HPB_DESC(MethodDescriptorProto)* const* methods =
      HPB_DESC(ServiceDescriptorProto_method)(svc_proto, &n);
  s->method_count = n;
  s->methods = _hpb_MethodDefs_New(ctx, n, methods, s);

  HPB_DEF_SET_OPTIONS(s->opts, ServiceDescriptorProto, ServiceOptions,
                      svc_proto);
}

hpb_ServiceDef* _hpb_ServiceDefs_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(ServiceDescriptorProto) * const* protos) {
  _hpb_DefType_CheckPadding(sizeof(hpb_ServiceDef));

  hpb_ServiceDef* s = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_ServiceDef) * n);
  for (int i = 0; i < n; i++) {
    create_service(ctx, protos[i], &s[i]);
    s[i].index = i;
  }
  return s;
}
