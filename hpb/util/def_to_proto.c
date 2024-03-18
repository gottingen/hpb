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

#include "hpb/util/def_to_proto.h"

#include <inttypes.h>
#include <math.h>

#include "hpb/port/vsnprintf_compat.h"
#include "hpb/reflection/enum_reserved_range.h"
#include "hpb/reflection/extension_range.h"
#include "hpb/reflection/internal/field_def.h"
#include "hpb/reflection/internal/file_def.h"
#include "hpb/reflection/message.h"
#include "hpb/reflection/message_reserved_range.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct {
  hpb_Arena* arena;
  jmp_buf err;
} hpb_ToProto_Context;

#define CHK_OOM(val) \
  if (!(val)) HPB_LONGJMP(ctx->err, 1);

// We want to copy the options verbatim into the destination options proto.
// We use serialize+parse as our deep copy.
#define SET_OPTIONS(proto, desc_type, options_type, src)                  \
  {                                                                       \
    size_t size;                                                          \
    /* MEM: could use a temporary arena here instead. */                  \
    char* pb = google_protobuf_##options_type##_serialize(src, ctx->arena, &size); \
    CHK_OOM(pb);                                                          \
    google_protobuf_##options_type* dst =                                          \
        google_protobuf_##options_type##_parse(pb, size, ctx->arena);              \
    CHK_OOM(dst);                                                         \
    google_protobuf_##desc_type##_set_options(proto, dst);                         \
  }

static hpb_StringView strviewdup2(hpb_ToProto_Context* ctx,
                                  hpb_StringView str) {
  char* p = hpb_Arena_Malloc(ctx->arena, str.size);
  CHK_OOM(p);
  memcpy(p, str.data, str.size);
  return (hpb_StringView){.data = p, .size = str.size};
}

static hpb_StringView strviewdup(hpb_ToProto_Context* ctx, const char* s) {
  return strviewdup2(ctx, (hpb_StringView){.data = s, .size = strlen(s)});
}

static hpb_StringView qual_dup(hpb_ToProto_Context* ctx, const char* s) {
  size_t n = strlen(s);
  char* p = hpb_Arena_Malloc(ctx->arena, n + 1);
  CHK_OOM(p);
  p[0] = '.';
  memcpy(p + 1, s, n);
  return (hpb_StringView){.data = p, .size = n + 1};
}

HPB_PRINTF(2, 3)
static hpb_StringView printf_dup(hpb_ToProto_Context* ctx, const char* fmt,
                                 ...) {
  const size_t max = 32;
  char* p = hpb_Arena_Malloc(ctx->arena, max);
  CHK_OOM(p);
  va_list args;
  va_start(args, fmt);
  size_t n = _hpb_vsnprintf(p, max, fmt, args);
  va_end(args);
  HPB_ASSERT(n < max);
  return (hpb_StringView){.data = p, .size = n};
}

static bool hpb_isprint(char ch) { return ch >= 0x20 && ch <= 0x7f; }

static int special_escape(char ch) {
  switch (ch) {
    // This is the same set of special escapes recognized by
    // absl::CEscape().
    case '\n':
      return 'n';
    case '\r':
      return 'r';
    case '\t':
      return 't';
    case '\\':
      return '\\';
    case '\'':
      return '\'';
    case '"':
      return '"';
    default:
      return -1;
  }
}

static hpb_StringView default_bytes(hpb_ToProto_Context* ctx,
                                    hpb_StringView val) {
  size_t n = 0;
  for (size_t i = 0; i < val.size; i++) {
    char ch = val.data[i];
    if (special_escape(ch) >= 0)
      n += 2;  // '\C'
    else if (hpb_isprint(ch))
      n += 1;
    else
      n += 4;  // '\123'
  }
  char* p = hpb_Arena_Malloc(ctx->arena, n);
  CHK_OOM(p);
  char* dst = p;
  const char* src = val.data;
  const char* end = src + val.size;
  while (src < end) {
    unsigned char ch = *src++;
    if (special_escape(ch) >= 0) {
      *dst++ = '\\';
      *dst++ = (char)special_escape(ch);
    } else if (hpb_isprint(ch)) {
      *dst++ = ch;
    } else {
      *dst++ = '\\';
      *dst++ = '0' + (ch >> 6);
      *dst++ = '0' + ((ch >> 3) & 0x7);
      *dst++ = '0' + (ch & 0x7);
    }
  }
  return (hpb_StringView){.data = p, .size = n};
}

static hpb_StringView default_string(hpb_ToProto_Context* ctx,
                                     const hpb_FieldDef* f) {
  hpb_MessageValue d = hpb_FieldDef_Default(f);
  hpb_CType type = hpb_FieldDef_CType(f);

  if (type == kHpb_CType_Float || type == kHpb_CType_Double) {
    double val = type == kHpb_CType_Float ? d.float_val : d.double_val;
    if (val == INFINITY) {
      return strviewdup(ctx, "inf");
    } else if (val == -INFINITY) {
      return strviewdup(ctx, "-inf");
    } else if (val != val) {
      return strviewdup(ctx, "nan");
    }
  }

  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Bool:
      return strviewdup(ctx, d.bool_val ? "true" : "false");
    case kHpb_CType_Enum: {
      const hpb_EnumDef* e = hpb_FieldDef_EnumSubDef(f);
      const hpb_EnumValueDef* ev =
          hpb_EnumDef_FindValueByNumber(e, d.int32_val);
      return strviewdup(ctx, hpb_EnumValueDef_Name(ev));
    }
    case kHpb_CType_Int64:
      return printf_dup(ctx, "%" PRId64, d.int64_val);
    case kHpb_CType_UInt64:
      return printf_dup(ctx, "%" PRIu64, d.uint64_val);
    case kHpb_CType_Int32:
      return printf_dup(ctx, "%" PRId32, d.int32_val);
    case kHpb_CType_UInt32:
      return printf_dup(ctx, "%" PRIu32, d.uint32_val);
    case kHpb_CType_Float:
      return printf_dup(ctx, "%.9g", d.float_val);
    case kHpb_CType_Double:
      return printf_dup(ctx, "%.17g", d.double_val);
    case kHpb_CType_String:
      return strviewdup2(ctx, d.str_val);
    case kHpb_CType_Bytes:
      return default_bytes(ctx, d.str_val);
    default:
      HPB_UNREACHABLE();
  }
}

static google_protobuf_DescriptorProto_ReservedRange* resrange_toproto(
    hpb_ToProto_Context* ctx, const hpb_MessageReservedRange* r) {
  google_protobuf_DescriptorProto_ReservedRange* proto =
      google_protobuf_DescriptorProto_ReservedRange_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_DescriptorProto_ReservedRange_set_start(
      proto, hpb_MessageReservedRange_Start(r));
  google_protobuf_DescriptorProto_ReservedRange_set_end(proto,
                                               hpb_MessageReservedRange_End(r));

  return proto;
}

static google_protobuf_EnumDescriptorProto_EnumReservedRange* enumresrange_toproto(
    hpb_ToProto_Context* ctx, const hpb_EnumReservedRange* r) {
  google_protobuf_EnumDescriptorProto_EnumReservedRange* proto =
      google_protobuf_EnumDescriptorProto_EnumReservedRange_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_EnumDescriptorProto_EnumReservedRange_set_start(
      proto, hpb_EnumReservedRange_Start(r));
  google_protobuf_EnumDescriptorProto_EnumReservedRange_set_end(
      proto, hpb_EnumReservedRange_End(r));

  return proto;
}

static google_protobuf_FieldDescriptorProto* fielddef_toproto(hpb_ToProto_Context* ctx,
                                                     const hpb_FieldDef* f) {
  google_protobuf_FieldDescriptorProto* proto =
      google_protobuf_FieldDescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_FieldDescriptorProto_set_name(proto,
                                       strviewdup(ctx, hpb_FieldDef_Name(f)));
  google_protobuf_FieldDescriptorProto_set_number(proto, hpb_FieldDef_Number(f));
  google_protobuf_FieldDescriptorProto_set_label(proto, hpb_FieldDef_Label(f));
  google_protobuf_FieldDescriptorProto_set_type(proto, hpb_FieldDef_Type(f));

  if (hpb_FieldDef_HasJsonName(f)) {
    google_protobuf_FieldDescriptorProto_set_json_name(
        proto, strviewdup(ctx, hpb_FieldDef_JsonName(f)));
  }

  if (hpb_FieldDef_IsSubMessage(f)) {
    google_protobuf_FieldDescriptorProto_set_type_name(
        proto,
        qual_dup(ctx, hpb_MessageDef_FullName(hpb_FieldDef_MessageSubDef(f))));
  } else if (hpb_FieldDef_CType(f) == kHpb_CType_Enum) {
    google_protobuf_FieldDescriptorProto_set_type_name(
        proto, qual_dup(ctx, hpb_EnumDef_FullName(hpb_FieldDef_EnumSubDef(f))));
  }

  if (hpb_FieldDef_IsExtension(f)) {
    google_protobuf_FieldDescriptorProto_set_extendee(
        proto,
        qual_dup(ctx, hpb_MessageDef_FullName(hpb_FieldDef_ContainingType(f))));
  }

  if (hpb_FieldDef_HasDefault(f)) {
    google_protobuf_FieldDescriptorProto_set_default_value(proto,
                                                  default_string(ctx, f));
  }

  const hpb_OneofDef* o = hpb_FieldDef_ContainingOneof(f);
  if (o) {
    google_protobuf_FieldDescriptorProto_set_oneof_index(proto, hpb_OneofDef_Index(o));
  }

  if (_hpb_FieldDef_IsProto3Optional(f)) {
    google_protobuf_FieldDescriptorProto_set_proto3_optional(proto, true);
  }

  if (hpb_FieldDef_HasOptions(f)) {
    SET_OPTIONS(proto, FieldDescriptorProto, FieldOptions,
                hpb_FieldDef_Options(f));
  }

  return proto;
}

static google_protobuf_OneofDescriptorProto* oneofdef_toproto(hpb_ToProto_Context* ctx,
                                                     const hpb_OneofDef* o) {
  google_protobuf_OneofDescriptorProto* proto =
      google_protobuf_OneofDescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_OneofDescriptorProto_set_name(proto,
                                       strviewdup(ctx, hpb_OneofDef_Name(o)));

  if (hpb_OneofDef_HasOptions(o)) {
    SET_OPTIONS(proto, OneofDescriptorProto, OneofOptions,
                hpb_OneofDef_Options(o));
  }

  return proto;
}

static google_protobuf_EnumValueDescriptorProto* enumvaldef_toproto(
    hpb_ToProto_Context* ctx, const hpb_EnumValueDef* e) {
  google_protobuf_EnumValueDescriptorProto* proto =
      google_protobuf_EnumValueDescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_EnumValueDescriptorProto_set_name(
      proto, strviewdup(ctx, hpb_EnumValueDef_Name(e)));
  google_protobuf_EnumValueDescriptorProto_set_number(proto, hpb_EnumValueDef_Number(e));

  if (hpb_EnumValueDef_HasOptions(e)) {
    SET_OPTIONS(proto, EnumValueDescriptorProto, EnumValueOptions,
                hpb_EnumValueDef_Options(e));
  }

  return proto;
}

static google_protobuf_EnumDescriptorProto* enumdef_toproto(hpb_ToProto_Context* ctx,
                                                   const hpb_EnumDef* e) {
  google_protobuf_EnumDescriptorProto* proto =
      google_protobuf_EnumDescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_EnumDescriptorProto_set_name(proto,
                                      strviewdup(ctx, hpb_EnumDef_Name(e)));

  int n = hpb_EnumDef_ValueCount(e);
  google_protobuf_EnumValueDescriptorProto** vals =
      google_protobuf_EnumDescriptorProto_resize_value(proto, n, ctx->arena);
  CHK_OOM(vals);
  for (int i = 0; i < n; i++) {
    vals[i] = enumvaldef_toproto(ctx, hpb_EnumDef_Value(e, i));
  }

  n = hpb_EnumDef_ReservedRangeCount(e);
  google_protobuf_EnumDescriptorProto_EnumReservedRange** res_ranges =
      google_protobuf_EnumDescriptorProto_resize_reserved_range(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    res_ranges[i] = enumresrange_toproto(ctx, hpb_EnumDef_ReservedRange(e, i));
  }

  n = hpb_EnumDef_ReservedNameCount(e);
  hpb_StringView* res_names =
      google_protobuf_EnumDescriptorProto_resize_reserved_name(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    res_names[i] = hpb_EnumDef_ReservedName(e, i);
  }

  if (hpb_EnumDef_HasOptions(e)) {
    SET_OPTIONS(proto, EnumDescriptorProto, EnumOptions,
                hpb_EnumDef_Options(e));
  }

  return proto;
}

static google_protobuf_DescriptorProto_ExtensionRange* extrange_toproto(
    hpb_ToProto_Context* ctx, const hpb_ExtensionRange* e) {
  google_protobuf_DescriptorProto_ExtensionRange* proto =
      google_protobuf_DescriptorProto_ExtensionRange_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_DescriptorProto_ExtensionRange_set_start(proto,
                                                  hpb_ExtensionRange_Start(e));
  google_protobuf_DescriptorProto_ExtensionRange_set_end(proto,
                                                hpb_ExtensionRange_End(e));

  if (hpb_ExtensionRange_HasOptions(e)) {
    SET_OPTIONS(proto, DescriptorProto_ExtensionRange, ExtensionRangeOptions,
                hpb_ExtensionRange_Options(e));
  }

  return proto;
}

static google_protobuf_DescriptorProto* msgdef_toproto(hpb_ToProto_Context* ctx,
                                              const hpb_MessageDef* m) {
  google_protobuf_DescriptorProto* proto = google_protobuf_DescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_DescriptorProto_set_name(proto,
                                  strviewdup(ctx, hpb_MessageDef_Name(m)));

  int n;

  n = hpb_MessageDef_FieldCount(m);
  google_protobuf_FieldDescriptorProto** fields =
      google_protobuf_DescriptorProto_resize_field(proto, n, ctx->arena);
  CHK_OOM(fields);
  for (int i = 0; i < n; i++) {
    fields[i] = fielddef_toproto(ctx, hpb_MessageDef_Field(m, i));
  }

  n = hpb_MessageDef_OneofCount(m);
  google_protobuf_OneofDescriptorProto** oneofs =
      google_protobuf_DescriptorProto_resize_oneof_decl(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    oneofs[i] = oneofdef_toproto(ctx, hpb_MessageDef_Oneof(m, i));
  }

  n = hpb_MessageDef_NestedMessageCount(m);
  google_protobuf_DescriptorProto** nested_msgs =
      google_protobuf_DescriptorProto_resize_nested_type(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    nested_msgs[i] = msgdef_toproto(ctx, hpb_MessageDef_NestedMessage(m, i));
  }

  n = hpb_MessageDef_NestedEnumCount(m);
  google_protobuf_EnumDescriptorProto** nested_enums =
      google_protobuf_DescriptorProto_resize_enum_type(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    nested_enums[i] = enumdef_toproto(ctx, hpb_MessageDef_NestedEnum(m, i));
  }

  n = hpb_MessageDef_NestedExtensionCount(m);
  google_protobuf_FieldDescriptorProto** nested_exts =
      google_protobuf_DescriptorProto_resize_extension(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    nested_exts[i] =
        fielddef_toproto(ctx, hpb_MessageDef_NestedExtension(m, i));
  }

  n = hpb_MessageDef_ExtensionRangeCount(m);
  google_protobuf_DescriptorProto_ExtensionRange** ext_ranges =
      google_protobuf_DescriptorProto_resize_extension_range(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    ext_ranges[i] = extrange_toproto(ctx, hpb_MessageDef_ExtensionRange(m, i));
  }

  n = hpb_MessageDef_ReservedRangeCount(m);
  google_protobuf_DescriptorProto_ReservedRange** res_ranges =
      google_protobuf_DescriptorProto_resize_reserved_range(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    res_ranges[i] = resrange_toproto(ctx, hpb_MessageDef_ReservedRange(m, i));
  }

  n = hpb_MessageDef_ReservedNameCount(m);
  hpb_StringView* res_names =
      google_protobuf_DescriptorProto_resize_reserved_name(proto, n, ctx->arena);
  for (int i = 0; i < n; i++) {
    res_names[i] = hpb_MessageDef_ReservedName(m, i);
  }

  if (hpb_MessageDef_HasOptions(m)) {
    SET_OPTIONS(proto, DescriptorProto, MessageOptions,
                hpb_MessageDef_Options(m));
  }

  return proto;
}

static google_protobuf_MethodDescriptorProto* methoddef_toproto(hpb_ToProto_Context* ctx,
                                                       const hpb_MethodDef* m) {
  google_protobuf_MethodDescriptorProto* proto =
      google_protobuf_MethodDescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_MethodDescriptorProto_set_name(proto,
                                        strviewdup(ctx, hpb_MethodDef_Name(m)));

  google_protobuf_MethodDescriptorProto_set_input_type(
      proto,
      qual_dup(ctx, hpb_MessageDef_FullName(hpb_MethodDef_InputType(m))));
  google_protobuf_MethodDescriptorProto_set_output_type(
      proto,
      qual_dup(ctx, hpb_MessageDef_FullName(hpb_MethodDef_OutputType(m))));

  if (hpb_MethodDef_ClientStreaming(m)) {
    google_protobuf_MethodDescriptorProto_set_client_streaming(proto, true);
  }

  if (hpb_MethodDef_ServerStreaming(m)) {
    google_protobuf_MethodDescriptorProto_set_server_streaming(proto, true);
  }

  if (hpb_MethodDef_HasOptions(m)) {
    SET_OPTIONS(proto, MethodDescriptorProto, MethodOptions,
                hpb_MethodDef_Options(m));
  }

  return proto;
}

static google_protobuf_ServiceDescriptorProto* servicedef_toproto(
    hpb_ToProto_Context* ctx, const hpb_ServiceDef* s) {
  google_protobuf_ServiceDescriptorProto* proto =
      google_protobuf_ServiceDescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_ServiceDescriptorProto_set_name(
      proto, strviewdup(ctx, hpb_ServiceDef_Name(s)));

  size_t n = hpb_ServiceDef_MethodCount(s);
  google_protobuf_MethodDescriptorProto** methods =
      google_protobuf_ServiceDescriptorProto_resize_method(proto, n, ctx->arena);
  for (size_t i = 0; i < n; i++) {
    methods[i] = methoddef_toproto(ctx, hpb_ServiceDef_Method(s, i));
  }

  if (hpb_ServiceDef_HasOptions(s)) {
    SET_OPTIONS(proto, ServiceDescriptorProto, ServiceOptions,
                hpb_ServiceDef_Options(s));
  }

  return proto;
}

static google_protobuf_FileDescriptorProto* filedef_toproto(hpb_ToProto_Context* ctx,
                                                   const hpb_FileDef* f) {
  google_protobuf_FileDescriptorProto* proto =
      google_protobuf_FileDescriptorProto_new(ctx->arena);
  CHK_OOM(proto);

  google_protobuf_FileDescriptorProto_set_name(proto,
                                      strviewdup(ctx, hpb_FileDef_Name(f)));

  const char* package = hpb_FileDef_Package(f);
  if (package) {
    size_t n = strlen(package);
    if (n) {
      google_protobuf_FileDescriptorProto_set_package(proto, strviewdup(ctx, package));
    }
  }

  const char* edition = hpb_FileDef_Edition(f);
  if (edition != NULL) {
    size_t n = strlen(edition);
    if (n != 0) {
      google_protobuf_FileDescriptorProto_set_edition(proto, strviewdup(ctx, edition));
    }
  }

  if (hpb_FileDef_Syntax(f) == kHpb_Syntax_Proto3) {
    google_protobuf_FileDescriptorProto_set_syntax(proto, strviewdup(ctx, "proto3"));
  }

  size_t n;
  n = hpb_FileDef_DependencyCount(f);
  hpb_StringView* deps =
      google_protobuf_FileDescriptorProto_resize_dependency(proto, n, ctx->arena);
  for (size_t i = 0; i < n; i++) {
    deps[i] = strviewdup(ctx, hpb_FileDef_Name(hpb_FileDef_Dependency(f, i)));
  }

  n = hpb_FileDef_PublicDependencyCount(f);
  int32_t* public_deps =
      google_protobuf_FileDescriptorProto_resize_public_dependency(proto, n, ctx->arena);
  const int32_t* public_dep_nums = _hpb_FileDef_PublicDependencyIndexes(f);
  if (n) memcpy(public_deps, public_dep_nums, n * sizeof(int32_t));

  n = hpb_FileDef_WeakDependencyCount(f);
  int32_t* weak_deps =
      google_protobuf_FileDescriptorProto_resize_weak_dependency(proto, n, ctx->arena);
  const int32_t* weak_dep_nums = _hpb_FileDef_WeakDependencyIndexes(f);
  if (n) memcpy(weak_deps, weak_dep_nums, n * sizeof(int32_t));

  n = hpb_FileDef_TopLevelMessageCount(f);
  google_protobuf_DescriptorProto** msgs =
      google_protobuf_FileDescriptorProto_resize_message_type(proto, n, ctx->arena);
  for (size_t i = 0; i < n; i++) {
    msgs[i] = msgdef_toproto(ctx, hpb_FileDef_TopLevelMessage(f, i));
  }

  n = hpb_FileDef_TopLevelEnumCount(f);
  google_protobuf_EnumDescriptorProto** enums =
      google_protobuf_FileDescriptorProto_resize_enum_type(proto, n, ctx->arena);
  for (size_t i = 0; i < n; i++) {
    enums[i] = enumdef_toproto(ctx, hpb_FileDef_TopLevelEnum(f, i));
  }

  n = hpb_FileDef_ServiceCount(f);
  google_protobuf_ServiceDescriptorProto** services =
      google_protobuf_FileDescriptorProto_resize_service(proto, n, ctx->arena);
  for (size_t i = 0; i < n; i++) {
    services[i] = servicedef_toproto(ctx, hpb_FileDef_Service(f, i));
  }

  n = hpb_FileDef_TopLevelExtensionCount(f);
  google_protobuf_FieldDescriptorProto** exts =
      google_protobuf_FileDescriptorProto_resize_extension(proto, n, ctx->arena);
  for (size_t i = 0; i < n; i++) {
    exts[i] = fielddef_toproto(ctx, hpb_FileDef_TopLevelExtension(f, i));
  }

  if (hpb_FileDef_HasOptions(f)) {
    SET_OPTIONS(proto, FileDescriptorProto, FileOptions,
                hpb_FileDef_Options(f));
  }

  return proto;
}

static google_protobuf_DescriptorProto* hpb_ToProto_ConvertMessageDef(
    hpb_ToProto_Context* const ctx, const hpb_MessageDef* const m) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return msgdef_toproto(ctx, m);
}

google_protobuf_DescriptorProto* hpb_MessageDef_ToProto(const hpb_MessageDef* m,
                                               hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertMessageDef(&ctx, m);
}

google_protobuf_EnumDescriptorProto* hpb_ToProto_ConvertEnumDef(
    hpb_ToProto_Context* const ctx, const hpb_EnumDef* const e) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return enumdef_toproto(ctx, e);
}

google_protobuf_EnumDescriptorProto* hpb_EnumDef_ToProto(const hpb_EnumDef* e,
                                                hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertEnumDef(&ctx, e);
}

google_protobuf_EnumValueDescriptorProto* hpb_ToProto_ConvertEnumValueDef(
    hpb_ToProto_Context* const ctx, const hpb_EnumValueDef* e) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return enumvaldef_toproto(ctx, e);
}

google_protobuf_EnumValueDescriptorProto* hpb_EnumValueDef_ToProto(
    const hpb_EnumValueDef* e, hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertEnumValueDef(&ctx, e);
}

google_protobuf_FieldDescriptorProto* hpb_ToProto_ConvertFieldDef(
    hpb_ToProto_Context* const ctx, const hpb_FieldDef* f) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return fielddef_toproto(ctx, f);
}

google_protobuf_FieldDescriptorProto* hpb_FieldDef_ToProto(const hpb_FieldDef* f,
                                                  hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertFieldDef(&ctx, f);
}

google_protobuf_OneofDescriptorProto* hpb_ToProto_ConvertOneofDef(
    hpb_ToProto_Context* const ctx, const hpb_OneofDef* o) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return oneofdef_toproto(ctx, o);
}

google_protobuf_OneofDescriptorProto* hpb_OneofDef_ToProto(const hpb_OneofDef* o,
                                                  hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertOneofDef(&ctx, o);
}

google_protobuf_FileDescriptorProto* hpb_ToProto_ConvertFileDef(
    hpb_ToProto_Context* const ctx, const hpb_FileDef* const f) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return filedef_toproto(ctx, f);
}

google_protobuf_FileDescriptorProto* hpb_FileDef_ToProto(const hpb_FileDef* f,
                                                hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertFileDef(&ctx, f);
}

google_protobuf_MethodDescriptorProto* hpb_ToProto_ConvertMethodDef(
    hpb_ToProto_Context* const ctx, const hpb_MethodDef* m) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return methoddef_toproto(ctx, m);
}

google_protobuf_MethodDescriptorProto* hpb_MethodDef_ToProto(
    const hpb_MethodDef* const m, hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertMethodDef(&ctx, m);
}

google_protobuf_ServiceDescriptorProto* hpb_ToProto_ConvertServiceDef(
    hpb_ToProto_Context* const ctx, const hpb_ServiceDef* const s) {
  if (HPB_SETJMP(ctx->err)) return NULL;
  return servicedef_toproto(ctx, s);
}

google_protobuf_ServiceDescriptorProto* hpb_ServiceDef_ToProto(const hpb_ServiceDef* s,
                                                      hpb_Arena* a) {
  hpb_ToProto_Context ctx = {a};
  return hpb_ToProto_ConvertServiceDef(&ctx, s);
}
