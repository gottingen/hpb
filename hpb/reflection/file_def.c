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

#include "hpb/reflection/internal/file_def.h"

#include "hpb/reflection/def_pool.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/enum_def.h"
#include "hpb/reflection/internal/field_def.h"
#include "hpb/reflection/internal/message_def.h"
#include "hpb/reflection/internal/service_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_FileDef {
  const HPB_DESC(FileOptions) * opts;
  const char* name;
  const char* package;
  const char* edition;

  const hpb_FileDef** deps;
  const int32_t* public_deps;
  const int32_t* weak_deps;
  const hpb_MessageDef* top_lvl_msgs;
  const hpb_EnumDef* top_lvl_enums;
  const hpb_FieldDef* top_lvl_exts;
  const hpb_ServiceDef* services;
  const hpb_MiniTableExtension** ext_layouts;
  const hpb_DefPool* symtab;

  int dep_count;
  int public_dep_count;
  int weak_dep_count;
  int top_lvl_msg_count;
  int top_lvl_enum_count;
  int top_lvl_ext_count;
  int service_count;
  int ext_count;  // All exts in the file.
  hpb_Syntax syntax;
};

const HPB_DESC(FileOptions) * hpb_FileDef_Options(const hpb_FileDef* f) {
  return f->opts;
}

bool hpb_FileDef_HasOptions(const hpb_FileDef* f) {
  return f->opts != (void*)kHpbDefOptDefault;
}

const char* hpb_FileDef_Name(const hpb_FileDef* f) { return f->name; }

const char* hpb_FileDef_Package(const hpb_FileDef* f) {
  return f->package ? f->package : "";
}

const char* hpb_FileDef_Edition(const hpb_FileDef* f) {
  return f->edition ? f->edition : "";
}

const char* _hpb_FileDef_RawPackage(const hpb_FileDef* f) { return f->package; }

hpb_Syntax hpb_FileDef_Syntax(const hpb_FileDef* f) { return f->syntax; }

int hpb_FileDef_TopLevelMessageCount(const hpb_FileDef* f) {
  return f->top_lvl_msg_count;
}

int hpb_FileDef_DependencyCount(const hpb_FileDef* f) { return f->dep_count; }

int hpb_FileDef_PublicDependencyCount(const hpb_FileDef* f) {
  return f->public_dep_count;
}

int hpb_FileDef_WeakDependencyCount(const hpb_FileDef* f) {
  return f->weak_dep_count;
}

const int32_t* _hpb_FileDef_PublicDependencyIndexes(const hpb_FileDef* f) {
  return f->public_deps;
}

const int32_t* _hpb_FileDef_WeakDependencyIndexes(const hpb_FileDef* f) {
  return f->weak_deps;
}

int hpb_FileDef_TopLevelEnumCount(const hpb_FileDef* f) {
  return f->top_lvl_enum_count;
}

int hpb_FileDef_TopLevelExtensionCount(const hpb_FileDef* f) {
  return f->top_lvl_ext_count;
}

int hpb_FileDef_ServiceCount(const hpb_FileDef* f) { return f->service_count; }

const hpb_FileDef* hpb_FileDef_Dependency(const hpb_FileDef* f, int i) {
  HPB_ASSERT(0 <= i && i < f->dep_count);
  return f->deps[i];
}

const hpb_FileDef* hpb_FileDef_PublicDependency(const hpb_FileDef* f, int i) {
  HPB_ASSERT(0 <= i && i < f->public_dep_count);
  return f->deps[f->public_deps[i]];
}

const hpb_FileDef* hpb_FileDef_WeakDependency(const hpb_FileDef* f, int i) {
  HPB_ASSERT(0 <= i && i < f->public_dep_count);
  return f->deps[f->weak_deps[i]];
}

const hpb_MessageDef* hpb_FileDef_TopLevelMessage(const hpb_FileDef* f, int i) {
  HPB_ASSERT(0 <= i && i < f->top_lvl_msg_count);
  return _hpb_MessageDef_At(f->top_lvl_msgs, i);
}

const hpb_EnumDef* hpb_FileDef_TopLevelEnum(const hpb_FileDef* f, int i) {
  HPB_ASSERT(0 <= i && i < f->top_lvl_enum_count);
  return _hpb_EnumDef_At(f->top_lvl_enums, i);
}

const hpb_FieldDef* hpb_FileDef_TopLevelExtension(const hpb_FileDef* f, int i) {
  HPB_ASSERT(0 <= i && i < f->top_lvl_ext_count);
  return _hpb_FieldDef_At(f->top_lvl_exts, i);
}

const hpb_ServiceDef* hpb_FileDef_Service(const hpb_FileDef* f, int i) {
  HPB_ASSERT(0 <= i && i < f->service_count);
  return _hpb_ServiceDef_At(f->services, i);
}

const hpb_DefPool* hpb_FileDef_Pool(const hpb_FileDef* f) { return f->symtab; }

const hpb_MiniTableExtension* _hpb_FileDef_ExtensionMiniTable(
    const hpb_FileDef* f, int i) {
  return f->ext_layouts[i];
}

static char* strviewdup(hpb_DefBuilder* ctx, hpb_StringView view) {
  char* ret = hpb_strdup2(view.data, view.size, _hpb_DefBuilder_Arena(ctx));
  if (!ret) _hpb_DefBuilder_OomErr(ctx);
  return ret;
}

static bool streql_view(hpb_StringView view, const char* b) {
  return view.size == strlen(b) && memcmp(view.data, b, view.size) == 0;
}

static int count_exts_in_msg(const HPB_DESC(DescriptorProto) * msg_proto) {
  size_t n;
  HPB_DESC(DescriptorProto_extension)(msg_proto, &n);
  int ext_count = n;

  const HPB_DESC(DescriptorProto)* const* nested_msgs =
      HPB_DESC(DescriptorProto_nested_type)(msg_proto, &n);
  for (size_t i = 0; i < n; i++) {
    ext_count += count_exts_in_msg(nested_msgs[i]);
  }

  return ext_count;
}

// Allocate and initialize one file def, and add it to the context object.
void _hpb_FileDef_Create(hpb_DefBuilder* ctx,
                         const HPB_DESC(FileDescriptorProto) * file_proto) {
  hpb_FileDef* file = _hpb_DefBuilder_Alloc(ctx, sizeof(hpb_FileDef));
  ctx->file = file;

  const HPB_DESC(DescriptorProto)* const* msgs;
  const HPB_DESC(EnumDescriptorProto)* const* enums;
  const HPB_DESC(FieldDescriptorProto)* const* exts;
  const HPB_DESC(ServiceDescriptorProto)* const* services;
  const hpb_StringView* strs;
  const int32_t* public_deps;
  const int32_t* weak_deps;
  size_t n;

  file->symtab = ctx->symtab;

  // Count all extensions in the file, to build a flat array of layouts.
  HPB_DESC(FileDescriptorProto_extension)(file_proto, &n);
  int ext_count = n;
  msgs = HPB_DESC(FileDescriptorProto_message_type)(file_proto, &n);
  for (size_t i = 0; i < n; i++) {
    ext_count += count_exts_in_msg(msgs[i]);
  }
  file->ext_count = ext_count;

  if (ctx->layout) {
    // We are using the ext layouts that were passed in.
    file->ext_layouts = ctx->layout->exts;
    if (ctx->layout->ext_count != file->ext_count) {
      _hpb_DefBuilder_Errf(ctx,
                           "Extension count did not match layout (%d vs %d)",
                           ctx->layout->ext_count, file->ext_count);
    }
  } else {
    // We are building ext layouts from scratch.
    file->ext_layouts = _hpb_DefBuilder_Alloc(
        ctx, sizeof(*file->ext_layouts) * file->ext_count);
    hpb_MiniTableExtension* ext =
        _hpb_DefBuilder_Alloc(ctx, sizeof(*ext) * file->ext_count);
    for (int i = 0; i < file->ext_count; i++) {
      file->ext_layouts[i] = &ext[i];
    }
  }

  hpb_StringView name = HPB_DESC(FileDescriptorProto_name)(file_proto);
  file->name = strviewdup(ctx, name);
  if (strlen(file->name) != name.size) {
    _hpb_DefBuilder_Errf(ctx, "File name contained embedded NULL");
  }

  hpb_StringView package = HPB_DESC(FileDescriptorProto_package)(file_proto);

  if (package.size) {
    _hpb_DefBuilder_CheckIdentFull(ctx, package);
    file->package = strviewdup(ctx, package);
  } else {
    file->package = NULL;
  }

  hpb_StringView edition = HPB_DESC(FileDescriptorProto_edition)(file_proto);

  if (edition.size == 0) {
    file->edition = NULL;
  } else {
    // TODO(b/267770604): How should we validate this?
    file->edition = strviewdup(ctx, edition);
    if (strlen(file->edition) != edition.size) {
      _hpb_DefBuilder_Errf(ctx, "Edition name contained embedded NULL");
    }
  }

  if (HPB_DESC(FileDescriptorProto_has_syntax)(file_proto)) {
    hpb_StringView syntax = HPB_DESC(FileDescriptorProto_syntax)(file_proto);

    if (streql_view(syntax, "proto2")) {
      file->syntax = kHpb_Syntax_Proto2;
    } else if (streql_view(syntax, "proto3")) {
      file->syntax = kHpb_Syntax_Proto3;
    } else {
      _hpb_DefBuilder_Errf(ctx, "Invalid syntax '" HPB_STRINGVIEW_FORMAT "'",
                           HPB_STRINGVIEW_ARGS(syntax));
    }
  } else {
    file->syntax = kHpb_Syntax_Proto2;
  }

  // Read options.
  HPB_DEF_SET_OPTIONS(file->opts, FileDescriptorProto, FileOptions, file_proto);

  // Verify dependencies.
  strs = HPB_DESC(FileDescriptorProto_dependency)(file_proto, &n);
  file->dep_count = n;
  file->deps = _hpb_DefBuilder_Alloc(ctx, sizeof(*file->deps) * n);

  for (size_t i = 0; i < n; i++) {
    hpb_StringView str = strs[i];
    file->deps[i] =
        hpb_DefPool_FindFileByNameWithSize(ctx->symtab, str.data, str.size);
    if (!file->deps[i]) {
      _hpb_DefBuilder_Errf(ctx,
                           "Depends on file '" HPB_STRINGVIEW_FORMAT
                           "', but it has not been loaded",
                           HPB_STRINGVIEW_ARGS(str));
    }
  }

  public_deps = HPB_DESC(FileDescriptorProto_public_dependency)(file_proto, &n);
  file->public_dep_count = n;
  file->public_deps =
      _hpb_DefBuilder_Alloc(ctx, sizeof(*file->public_deps) * n);
  int32_t* mutable_public_deps = (int32_t*)file->public_deps;
  for (size_t i = 0; i < n; i++) {
    if (public_deps[i] >= file->dep_count) {
      _hpb_DefBuilder_Errf(ctx, "public_dep %d is out of range",
                           (int)public_deps[i]);
    }
    mutable_public_deps[i] = public_deps[i];
  }

  weak_deps = HPB_DESC(FileDescriptorProto_weak_dependency)(file_proto, &n);
  file->weak_dep_count = n;
  file->weak_deps = _hpb_DefBuilder_Alloc(ctx, sizeof(*file->weak_deps) * n);
  int32_t* mutable_weak_deps = (int32_t*)file->weak_deps;
  for (size_t i = 0; i < n; i++) {
    if (weak_deps[i] >= file->dep_count) {
      _hpb_DefBuilder_Errf(ctx, "weak_dep %d is out of range",
                           (int)weak_deps[i]);
    }
    mutable_weak_deps[i] = weak_deps[i];
  }

  // Create enums.
  enums = HPB_DESC(FileDescriptorProto_enum_type)(file_proto, &n);
  file->top_lvl_enum_count = n;
  file->top_lvl_enums = _hpb_EnumDefs_New(ctx, n, enums, NULL);

  // Create extensions.
  exts = HPB_DESC(FileDescriptorProto_extension)(file_proto, &n);
  file->top_lvl_ext_count = n;
  file->top_lvl_exts = _hpb_Extensions_New(ctx, n, exts, file->package, NULL);

  // Create messages.
  msgs = HPB_DESC(FileDescriptorProto_message_type)(file_proto, &n);
  file->top_lvl_msg_count = n;
  file->top_lvl_msgs = _hpb_MessageDefs_New(ctx, n, msgs, NULL);

  // Create services.
  services = HPB_DESC(FileDescriptorProto_service)(file_proto, &n);
  file->service_count = n;
  file->services = _hpb_ServiceDefs_New(ctx, n, services);

  // Now that all names are in the table, build layouts and resolve refs.

  for (int i = 0; i < file->top_lvl_msg_count; i++) {
    hpb_MessageDef* m = (hpb_MessageDef*)hpb_FileDef_TopLevelMessage(file, i);
    _hpb_MessageDef_Resolve(ctx, m);
  }

  for (int i = 0; i < file->top_lvl_ext_count; i++) {
    hpb_FieldDef* f = (hpb_FieldDef*)hpb_FileDef_TopLevelExtension(file, i);
    _hpb_FieldDef_Resolve(ctx, file->package, f);
  }

  for (int i = 0; i < file->top_lvl_msg_count; i++) {
    hpb_MessageDef* m = (hpb_MessageDef*)hpb_FileDef_TopLevelMessage(file, i);
    _hpb_MessageDef_CreateMiniTable(ctx, (hpb_MessageDef*)m);
  }

  for (int i = 0; i < file->top_lvl_ext_count; i++) {
    hpb_FieldDef* f = (hpb_FieldDef*)hpb_FileDef_TopLevelExtension(file, i);
    _hpb_FieldDef_BuildMiniTableExtension(ctx, f);
  }

  for (int i = 0; i < file->top_lvl_msg_count; i++) {
    hpb_MessageDef* m = (hpb_MessageDef*)hpb_FileDef_TopLevelMessage(file, i);
    _hpb_MessageDef_LinkMiniTable(ctx, m);
  }

  if (file->ext_count) {
    bool ok = hpb_ExtensionRegistry_AddArray(
        _hpb_DefPool_ExtReg(ctx->symtab), file->ext_layouts, file->ext_count);
    if (!ok) _hpb_DefBuilder_OomErr(ctx);
  }
}
