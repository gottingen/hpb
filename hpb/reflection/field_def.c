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

#include "hpb/reflection/internal/field_def.h"

#include <ctype.h>
#include <errno.h>

#include "hpb/mini_descriptor/decode.h"
#include "hpb/mini_descriptor/internal/modifiers.h"
#include "hpb/reflection/def.h"
#include "hpb/reflection/def_pool.h"
#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/desc_state.h"
#include "hpb/reflection/internal/enum_def.h"
#include "hpb/reflection/internal/enum_value_def.h"
#include "hpb/reflection/internal/file_def.h"
#include "hpb/reflection/internal/message_def.h"
#include "hpb/reflection/internal/oneof_def.h"

// Must be last.
#include "hpb/port/def.inc"

#define HPB_FIELD_TYPE_UNSPECIFIED 0

typedef struct {
  size_t len;
  char str[1];  // Null-terminated string data follows.
} str_t;

struct hpb_FieldDef {
  const HPB_DESC(FieldOptions) * opts;
  const hpb_FileDef* file;
  const hpb_MessageDef* msgdef;
  const char* full_name;
  const char* json_name;
  union {
    int64_t sint;
    uint64_t uint;
    double dbl;
    float flt;
    bool boolean;
    str_t* str;
    void* msg;  // Always NULL.
  } defaultval;
  union {
    const hpb_OneofDef* oneof;
    const hpb_MessageDef* extension_scope;
  } scope;
  union {
    const hpb_MessageDef* msgdef;
    const hpb_EnumDef* enumdef;
    const HPB_DESC(FieldDescriptorProto) * unresolved;
  } sub;
  uint32_t number_;
  uint16_t index_;
  uint16_t layout_index;  // Index into msgdef->layout->fields or file->exts
  bool has_default;
  bool has_json_name;
  bool has_presence;
  bool is_extension;
  bool is_packed;
  bool is_proto3_optional;
  hpb_FieldType type_;
  hpb_Label label_;
#if UINTPTR_MAX == 0xffffffff
  uint32_t padding;  // Increase size to a multiple of 8.
#endif
};

hpb_FieldDef* _hpb_FieldDef_At(const hpb_FieldDef* f, int i) {
  return (hpb_FieldDef*)&f[i];
}

const HPB_DESC(FieldOptions) * hpb_FieldDef_Options(const hpb_FieldDef* f) {
  return f->opts;
}

bool hpb_FieldDef_HasOptions(const hpb_FieldDef* f) {
  return f->opts != (void*)kHpbDefOptDefault;
}

const char* hpb_FieldDef_FullName(const hpb_FieldDef* f) {
  return f->full_name;
}

hpb_CType hpb_FieldDef_CType(const hpb_FieldDef* f) {
  switch (f->type_) {
    case kHpb_FieldType_Double:
      return kHpb_CType_Double;
    case kHpb_FieldType_Float:
      return kHpb_CType_Float;
    case kHpb_FieldType_Int64:
    case kHpb_FieldType_SInt64:
    case kHpb_FieldType_SFixed64:
      return kHpb_CType_Int64;
    case kHpb_FieldType_Int32:
    case kHpb_FieldType_SFixed32:
    case kHpb_FieldType_SInt32:
      return kHpb_CType_Int32;
    case kHpb_FieldType_UInt64:
    case kHpb_FieldType_Fixed64:
      return kHpb_CType_UInt64;
    case kHpb_FieldType_UInt32:
    case kHpb_FieldType_Fixed32:
      return kHpb_CType_UInt32;
    case kHpb_FieldType_Enum:
      return kHpb_CType_Enum;
    case kHpb_FieldType_Bool:
      return kHpb_CType_Bool;
    case kHpb_FieldType_String:
      return kHpb_CType_String;
    case kHpb_FieldType_Bytes:
      return kHpb_CType_Bytes;
    case kHpb_FieldType_Group:
    case kHpb_FieldType_Message:
      return kHpb_CType_Message;
  }
  HPB_UNREACHABLE();
}

hpb_FieldType hpb_FieldDef_Type(const hpb_FieldDef* f) { return f->type_; }

uint32_t hpb_FieldDef_Index(const hpb_FieldDef* f) { return f->index_; }

hpb_Label hpb_FieldDef_Label(const hpb_FieldDef* f) { return f->label_; }

uint32_t hpb_FieldDef_Number(const hpb_FieldDef* f) { return f->number_; }

bool hpb_FieldDef_IsExtension(const hpb_FieldDef* f) { return f->is_extension; }

bool hpb_FieldDef_IsPacked(const hpb_FieldDef* f) { return f->is_packed; }

const char* hpb_FieldDef_Name(const hpb_FieldDef* f) {
  return _hpb_DefBuilder_FullToShort(f->full_name);
}

const char* hpb_FieldDef_JsonName(const hpb_FieldDef* f) {
  return f->json_name;
}

bool hpb_FieldDef_HasJsonName(const hpb_FieldDef* f) {
  return f->has_json_name;
}

const hpb_FileDef* hpb_FieldDef_File(const hpb_FieldDef* f) { return f->file; }

const hpb_MessageDef* hpb_FieldDef_ContainingType(const hpb_FieldDef* f) {
  return f->msgdef;
}

const hpb_MessageDef* hpb_FieldDef_ExtensionScope(const hpb_FieldDef* f) {
  return f->is_extension ? f->scope.extension_scope : NULL;
}

const hpb_OneofDef* hpb_FieldDef_ContainingOneof(const hpb_FieldDef* f) {
  return f->is_extension ? NULL : f->scope.oneof;
}

const hpb_OneofDef* hpb_FieldDef_RealContainingOneof(const hpb_FieldDef* f) {
  const hpb_OneofDef* oneof = hpb_FieldDef_ContainingOneof(f);
  if (!oneof || hpb_OneofDef_IsSynthetic(oneof)) return NULL;
  return oneof;
}

hpb_MessageValue hpb_FieldDef_Default(const hpb_FieldDef* f) {
  hpb_MessageValue ret;

  if (hpb_FieldDef_IsRepeated(f) || hpb_FieldDef_IsSubMessage(f)) {
    return (hpb_MessageValue){.msg_val = NULL};
  }

  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Bool:
      return (hpb_MessageValue){.bool_val = f->defaultval.boolean};
    case kHpb_CType_Int64:
      return (hpb_MessageValue){.int64_val = f->defaultval.sint};
    case kHpb_CType_UInt64:
      return (hpb_MessageValue){.uint64_val = f->defaultval.uint};
    case kHpb_CType_Enum:
    case kHpb_CType_Int32:
      return (hpb_MessageValue){.int32_val = (int32_t)f->defaultval.sint};
    case kHpb_CType_UInt32:
      return (hpb_MessageValue){.uint32_val = (uint32_t)f->defaultval.uint};
    case kHpb_CType_Float:
      return (hpb_MessageValue){.float_val = f->defaultval.flt};
    case kHpb_CType_Double:
      return (hpb_MessageValue){.double_val = f->defaultval.dbl};
    case kHpb_CType_String:
    case kHpb_CType_Bytes: {
      str_t* str = f->defaultval.str;
      if (str) {
        return (hpb_MessageValue){
            .str_val = (hpb_StringView){.data = str->str, .size = str->len}};
      } else {
        return (hpb_MessageValue){
            .str_val = (hpb_StringView){.data = NULL, .size = 0}};
      }
    }
    default:
      HPB_UNREACHABLE();
  }

  return ret;
}

const hpb_MessageDef* hpb_FieldDef_MessageSubDef(const hpb_FieldDef* f) {
  return hpb_FieldDef_CType(f) == kHpb_CType_Message ? f->sub.msgdef : NULL;
}

const hpb_EnumDef* hpb_FieldDef_EnumSubDef(const hpb_FieldDef* f) {
  return hpb_FieldDef_CType(f) == kHpb_CType_Enum ? f->sub.enumdef : NULL;
}

const hpb_MiniTableField* hpb_FieldDef_MiniTable(const hpb_FieldDef* f) {
  if (hpb_FieldDef_IsExtension(f)) {
    const hpb_FileDef* file = hpb_FieldDef_File(f);
    return (hpb_MiniTableField*)_hpb_FileDef_ExtensionMiniTable(
        file, f->layout_index);
  } else {
    const hpb_MiniTable* layout = hpb_MessageDef_MiniTable(f->msgdef);
    return &layout->fields[f->layout_index];
  }
}

const hpb_MiniTableExtension* _hpb_FieldDef_ExtensionMiniTable(
    const hpb_FieldDef* f) {
  HPB_ASSERT(hpb_FieldDef_IsExtension(f));
  const hpb_FileDef* file = hpb_FieldDef_File(f);
  return _hpb_FileDef_ExtensionMiniTable(file, f->layout_index);
}

bool _hpb_FieldDef_IsClosedEnum(const hpb_FieldDef* f) {
  if (f->type_ != kHpb_FieldType_Enum) return false;
  return hpb_EnumDef_IsClosed(f->sub.enumdef);
}

bool _hpb_FieldDef_IsProto3Optional(const hpb_FieldDef* f) {
  return f->is_proto3_optional;
}

int _hpb_FieldDef_LayoutIndex(const hpb_FieldDef* f) { return f->layout_index; }

uint64_t _hpb_FieldDef_Modifiers(const hpb_FieldDef* f) {
  uint64_t out = f->is_packed ? kHpb_FieldModifier_IsPacked : 0;

  switch (f->label_) {
    case kHpb_Label_Optional:
      if (!hpb_FieldDef_HasPresence(f)) {
        out |= kHpb_FieldModifier_IsProto3Singular;
      }
      break;
    case kHpb_Label_Repeated:
      out |= kHpb_FieldModifier_IsRepeated;
      break;
    case kHpb_Label_Required:
      out |= kHpb_FieldModifier_IsRequired;
      break;
  }

  if (_hpb_FieldDef_IsClosedEnum(f)) {
    out |= kHpb_FieldModifier_IsClosedEnum;
  }
  return out;
}

bool hpb_FieldDef_HasDefault(const hpb_FieldDef* f) { return f->has_default; }
bool hpb_FieldDef_HasPresence(const hpb_FieldDef* f) { return f->has_presence; }

bool hpb_FieldDef_HasSubDef(const hpb_FieldDef* f) {
  return hpb_FieldDef_IsSubMessage(f) ||
         hpb_FieldDef_CType(f) == kHpb_CType_Enum;
}

bool hpb_FieldDef_IsMap(const hpb_FieldDef* f) {
  return hpb_FieldDef_IsRepeated(f) && hpb_FieldDef_IsSubMessage(f) &&
         hpb_MessageDef_IsMapEntry(hpb_FieldDef_MessageSubDef(f));
}

bool hpb_FieldDef_IsOptional(const hpb_FieldDef* f) {
  return hpb_FieldDef_Label(f) == kHpb_Label_Optional;
}

bool hpb_FieldDef_IsPrimitive(const hpb_FieldDef* f) {
  return !hpb_FieldDef_IsString(f) && !hpb_FieldDef_IsSubMessage(f);
}

bool hpb_FieldDef_IsRepeated(const hpb_FieldDef* f) {
  return hpb_FieldDef_Label(f) == kHpb_Label_Repeated;
}

bool hpb_FieldDef_IsRequired(const hpb_FieldDef* f) {
  return hpb_FieldDef_Label(f) == kHpb_Label_Required;
}

bool hpb_FieldDef_IsString(const hpb_FieldDef* f) {
  return hpb_FieldDef_CType(f) == kHpb_CType_String ||
         hpb_FieldDef_CType(f) == kHpb_CType_Bytes;
}

bool hpb_FieldDef_IsSubMessage(const hpb_FieldDef* f) {
  return hpb_FieldDef_CType(f) == kHpb_CType_Message;
}

static bool between(int32_t x, int32_t low, int32_t high) {
  return x >= low && x <= high;
}

bool hpb_FieldDef_checklabel(int32_t label) { return between(label, 1, 3); }
bool hpb_FieldDef_checktype(int32_t type) { return between(type, 1, 11); }
bool hpb_FieldDef_checkintfmt(int32_t fmt) { return between(fmt, 1, 3); }

bool hpb_FieldDef_checkdescriptortype(int32_t type) {
  return between(type, 1, 18);
}

static bool streql2(const char* a, size_t n, const char* b) {
  return n == strlen(b) && memcmp(a, b, n) == 0;
}

// Implement the transformation as described in the spec:
//   1. upper case all letters after an underscore.
//   2. remove all underscores.
static char* make_json_name(const char* name, size_t size, hpb_Arena* a) {
  char* out = hpb_Arena_Malloc(a, size + 1);  // +1 is to add a trailing '\0'
  if (out == NULL) return NULL;

  bool ucase_next = false;
  char* des = out;
  for (size_t i = 0; i < size; i++) {
    if (name[i] == '_') {
      ucase_next = true;
    } else {
      *des++ = ucase_next ? toupper(name[i]) : name[i];
      ucase_next = false;
    }
  }
  *des++ = '\0';
  return out;
}

static str_t* newstr(hpb_DefBuilder* ctx, const char* data, size_t len) {
  str_t* ret = _hpb_DefBuilder_Alloc(ctx, sizeof(*ret) + len);
  if (!ret) _hpb_DefBuilder_OomErr(ctx);
  ret->len = len;
  if (len) memcpy(ret->str, data, len);
  ret->str[len] = '\0';
  return ret;
}

static str_t* unescape(hpb_DefBuilder* ctx, const hpb_FieldDef* f,
                       const char* data, size_t len) {
  // Size here is an upper bound; escape sequences could ultimately shrink it.
  str_t* ret = _hpb_DefBuilder_Alloc(ctx, sizeof(*ret) + len);
  char* dst = &ret->str[0];
  const char* src = data;
  const char* end = data + len;

  while (src < end) {
    if (*src == '\\') {
      src++;
      *dst++ = _hpb_DefBuilder_ParseEscape(ctx, f, &src, end);
    } else {
      *dst++ = *src++;
    }
  }

  ret->len = dst - &ret->str[0];
  return ret;
}

static void parse_default(hpb_DefBuilder* ctx, const char* str, size_t len,
                          hpb_FieldDef* f) {
  char* end;
  char nullz[64];
  errno = 0;

  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Int32:
    case kHpb_CType_Int64:
    case kHpb_CType_UInt32:
    case kHpb_CType_UInt64:
    case kHpb_CType_Double:
    case kHpb_CType_Float:
      // Standard C number parsing functions expect null-terminated strings.
      if (len >= sizeof(nullz) - 1) {
        _hpb_DefBuilder_Errf(ctx, "Default too long: %.*s", (int)len, str);
      }
      memcpy(nullz, str, len);
      nullz[len] = '\0';
      str = nullz;
      break;
    default:
      break;
  }

  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Int32: {
      long val = strtol(str, &end, 0);
      if (val > INT32_MAX || val < INT32_MIN || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.sint = val;
      break;
    }
    case kHpb_CType_Enum: {
      const hpb_EnumDef* e = f->sub.enumdef;
      const hpb_EnumValueDef* ev =
          hpb_EnumDef_FindValueByNameWithSize(e, str, len);
      if (!ev) {
        goto invalid;
      }
      f->defaultval.sint = hpb_EnumValueDef_Number(ev);
      break;
    }
    case kHpb_CType_Int64: {
      long long val = strtoll(str, &end, 0);
      if (val > INT64_MAX || val < INT64_MIN || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.sint = val;
      break;
    }
    case kHpb_CType_UInt32: {
      unsigned long val = strtoul(str, &end, 0);
      if (val > UINT32_MAX || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.uint = val;
      break;
    }
    case kHpb_CType_UInt64: {
      unsigned long long val = strtoull(str, &end, 0);
      if (val > UINT64_MAX || errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.uint = val;
      break;
    }
    case kHpb_CType_Double: {
      double val = strtod(str, &end);
      if (errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.dbl = val;
      break;
    }
    case kHpb_CType_Float: {
      float val = strtof(str, &end);
      if (errno == ERANGE || *end) {
        goto invalid;
      }
      f->defaultval.flt = val;
      break;
    }
    case kHpb_CType_Bool: {
      if (streql2(str, len, "false")) {
        f->defaultval.boolean = false;
      } else if (streql2(str, len, "true")) {
        f->defaultval.boolean = true;
      } else {
        goto invalid;
      }
      break;
    }
    case kHpb_CType_String:
      f->defaultval.str = newstr(ctx, str, len);
      break;
    case kHpb_CType_Bytes:
      f->defaultval.str = unescape(ctx, f, str, len);
      break;
    case kHpb_CType_Message:
      /* Should not have a default value. */
      _hpb_DefBuilder_Errf(ctx, "Message should not have a default (%s)",
                           hpb_FieldDef_FullName(f));
  }

  return;

invalid:
  _hpb_DefBuilder_Errf(ctx, "Invalid default '%.*s' for field %s of type %d",
                       (int)len, str, hpb_FieldDef_FullName(f),
                       (int)hpb_FieldDef_Type(f));
}

static void set_default_default(hpb_DefBuilder* ctx, hpb_FieldDef* f) {
  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Int32:
    case kHpb_CType_Int64:
      f->defaultval.sint = 0;
      break;
    case kHpb_CType_UInt64:
    case kHpb_CType_UInt32:
      f->defaultval.uint = 0;
      break;
    case kHpb_CType_Double:
    case kHpb_CType_Float:
      f->defaultval.dbl = 0;
      break;
    case kHpb_CType_String:
    case kHpb_CType_Bytes:
      f->defaultval.str = newstr(ctx, NULL, 0);
      break;
    case kHpb_CType_Bool:
      f->defaultval.boolean = false;
      break;
    case kHpb_CType_Enum: {
      const hpb_EnumValueDef* v = hpb_EnumDef_Value(f->sub.enumdef, 0);
      f->defaultval.sint = hpb_EnumValueDef_Number(v);
      break;
    }
    case kHpb_CType_Message:
      break;
  }
}

static void _hpb_FieldDef_Create(hpb_DefBuilder* ctx, const char* prefix,
                                 const HPB_DESC(FieldDescriptorProto) *
                                     field_proto,
                                 hpb_MessageDef* m, hpb_FieldDef* f) {
  // Must happen before _hpb_DefBuilder_Add()
  f->file = _hpb_DefBuilder_File(ctx);

  if (!HPB_DESC(FieldDescriptorProto_has_name)(field_proto)) {
    _hpb_DefBuilder_Errf(ctx, "field has no name");
  }

  const hpb_StringView name = HPB_DESC(FieldDescriptorProto_name)(field_proto);

  f->full_name = _hpb_DefBuilder_MakeFullName(ctx, prefix, name);
  f->label_ = (int)HPB_DESC(FieldDescriptorProto_label)(field_proto);
  f->number_ = HPB_DESC(FieldDescriptorProto_number)(field_proto);
  f->is_proto3_optional =
      HPB_DESC(FieldDescriptorProto_proto3_optional)(field_proto);
  f->msgdef = m;
  f->scope.oneof = NULL;

  f->has_json_name = HPB_DESC(FieldDescriptorProto_has_json_name)(field_proto);
  if (f->has_json_name) {
    const hpb_StringView sv =
        HPB_DESC(FieldDescriptorProto_json_name)(field_proto);
    f->json_name = hpb_strdup2(sv.data, sv.size, ctx->arena);
  } else {
    f->json_name = make_json_name(name.data, name.size, ctx->arena);
  }
  if (!f->json_name) _hpb_DefBuilder_OomErr(ctx);

  const bool has_type = HPB_DESC(FieldDescriptorProto_has_type)(field_proto);
  const bool has_type_name =
      HPB_DESC(FieldDescriptorProto_has_type_name)(field_proto);

  f->type_ = (int)HPB_DESC(FieldDescriptorProto_type)(field_proto);

  if (has_type) {
    switch (f->type_) {
      case kHpb_FieldType_Message:
      case kHpb_FieldType_Group:
      case kHpb_FieldType_Enum:
        if (!has_type_name) {
          _hpb_DefBuilder_Errf(ctx, "field of type %d requires type name (%s)",
                               (int)f->type_, f->full_name);
        }
        break;
      default:
        if (has_type_name) {
          _hpb_DefBuilder_Errf(
              ctx, "invalid type for field with type_name set (%s, %d)",
              f->full_name, (int)f->type_);
        }
    }
  }

  if (!has_type && has_type_name) {
    f->type_ =
        HPB_FIELD_TYPE_UNSPECIFIED;  // We'll assign this in resolve_subdef()
  } else {
    if (f->type_ < kHpb_FieldType_Double || f->type_ > kHpb_FieldType_SInt64) {
      _hpb_DefBuilder_Errf(ctx, "invalid type for field %s (%d)", f->full_name,
                           f->type_);
    }
  }

  if (f->label_ < kHpb_Label_Optional || f->label_ > kHpb_Label_Repeated) {
    _hpb_DefBuilder_Errf(ctx, "invalid label for field %s (%d)", f->full_name,
                         f->label_);
  }

  /* We can't resolve the subdef or (in the case of extensions) the containing
   * message yet, because it may not have been defined yet.  We stash a pointer
   * to the field_proto until later when we can properly resolve it. */
  f->sub.unresolved = field_proto;

  if (f->label_ == kHpb_Label_Required &&
      hpb_FileDef_Syntax(f->file) == kHpb_Syntax_Proto3) {
    _hpb_DefBuilder_Errf(ctx, "proto3 fields cannot be required (%s)",
                         f->full_name);
  }

  if (HPB_DESC(FieldDescriptorProto_has_oneof_index)(field_proto)) {
    int oneof_index = HPB_DESC(FieldDescriptorProto_oneof_index)(field_proto);

    if (hpb_FieldDef_Label(f) != kHpb_Label_Optional) {
      _hpb_DefBuilder_Errf(ctx, "fields in oneof must have OPTIONAL label (%s)",
                           f->full_name);
    }

    if (!m) {
      _hpb_DefBuilder_Errf(ctx, "oneof field (%s) has no containing msg",
                           f->full_name);
    }

    if (oneof_index >= hpb_MessageDef_OneofCount(m)) {
      _hpb_DefBuilder_Errf(ctx, "oneof_index out of range (%s)", f->full_name);
    }

    hpb_OneofDef* oneof = (hpb_OneofDef*)hpb_MessageDef_Oneof(m, oneof_index);
    f->scope.oneof = oneof;

    _hpb_OneofDef_Insert(ctx, oneof, f, name.data, name.size);
  }

  HPB_DEF_SET_OPTIONS(f->opts, FieldDescriptorProto, FieldOptions, field_proto);

  if (HPB_DESC(FieldOptions_has_packed)(f->opts)) {
    f->is_packed = HPB_DESC(FieldOptions_packed)(f->opts);
  } else {
    // Repeated fields default to packed for proto3 only.
    f->is_packed = has_type && hpb_FieldDef_IsPrimitive(f) &&
                   f->label_ == kHpb_Label_Repeated &&
                   hpb_FileDef_Syntax(f->file) == kHpb_Syntax_Proto3;
  }

  f->has_presence =
      (!hpb_FieldDef_IsRepeated(f)) &&
      (f->type_ == kHpb_FieldType_Message || f->type_ == kHpb_FieldType_Group ||
       hpb_FieldDef_ContainingOneof(f) ||
       (hpb_FileDef_Syntax(f->file) == kHpb_Syntax_Proto2));
}

static void _hpb_FieldDef_CreateExt(hpb_DefBuilder* ctx, const char* prefix,
                                    const HPB_DESC(FieldDescriptorProto) *
                                        field_proto,
                                    hpb_MessageDef* m, hpb_FieldDef* f) {
  f->is_extension = true;
  _hpb_FieldDef_Create(ctx, prefix, field_proto, m, f);

  if (HPB_DESC(FieldDescriptorProto_has_oneof_index)(field_proto)) {
    _hpb_DefBuilder_Errf(ctx, "oneof_index provided for extension field (%s)",
                         f->full_name);
  }

  f->scope.extension_scope = m;
  _hpb_DefBuilder_Add(ctx, f->full_name, _hpb_DefType_Pack(f, HPB_DEFTYPE_EXT));
  f->layout_index = ctx->ext_count++;

  if (ctx->layout) {
    HPB_ASSERT(_hpb_FieldDef_ExtensionMiniTable(f)->field.number == f->number_);
  }
}

static void _hpb_FieldDef_CreateNotExt(hpb_DefBuilder* ctx, const char* prefix,
                                       const HPB_DESC(FieldDescriptorProto) *
                                           field_proto,
                                       hpb_MessageDef* m, hpb_FieldDef* f) {
  f->is_extension = false;
  _hpb_FieldDef_Create(ctx, prefix, field_proto, m, f);

  if (!HPB_DESC(FieldDescriptorProto_has_oneof_index)(field_proto)) {
    if (f->is_proto3_optional) {
      _hpb_DefBuilder_Errf(
          ctx,
          "non-extension field (%s) with proto3_optional was not in a oneof",
          f->full_name);
    }
  }

  _hpb_MessageDef_InsertField(ctx, m, f);
}

hpb_FieldDef* _hpb_Extensions_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(FieldDescriptorProto) * const* protos, const char* prefix,
    hpb_MessageDef* m) {
  _hpb_DefType_CheckPadding(sizeof(hpb_FieldDef));
  hpb_FieldDef* defs =
      (hpb_FieldDef*)_hpb_DefBuilder_Alloc(ctx, sizeof(hpb_FieldDef) * n);

  for (int i = 0; i < n; i++) {
    hpb_FieldDef* f = &defs[i];

    _hpb_FieldDef_CreateExt(ctx, prefix, protos[i], m, f);
    f->index_ = i;
  }

  return defs;
}

hpb_FieldDef* _hpb_FieldDefs_New(
    hpb_DefBuilder* ctx, int n,
    const HPB_DESC(FieldDescriptorProto) * const* protos, const char* prefix,
    hpb_MessageDef* m, bool* is_sorted) {
  _hpb_DefType_CheckPadding(sizeof(hpb_FieldDef));
  hpb_FieldDef* defs =
      (hpb_FieldDef*)_hpb_DefBuilder_Alloc(ctx, sizeof(hpb_FieldDef) * n);

  uint32_t previous = 0;
  for (int i = 0; i < n; i++) {
    hpb_FieldDef* f = &defs[i];

    _hpb_FieldDef_CreateNotExt(ctx, prefix, protos[i], m, f);
    f->index_ = i;
    if (!ctx->layout) {
      // Speculate that the def fields are sorted.  We will always sort the
      // MiniTable fields, so if defs are sorted then indices will match.
      //
      // If this is incorrect, we will overwrite later.
      f->layout_index = i;
    }

    const uint32_t current = f->number_;
    if (previous > current) *is_sorted = false;
    previous = current;
  }

  return defs;
}

static void resolve_subdef(hpb_DefBuilder* ctx, const char* prefix,
                           hpb_FieldDef* f) {
  const HPB_DESC(FieldDescriptorProto)* field_proto = f->sub.unresolved;
  hpb_StringView name = HPB_DESC(FieldDescriptorProto_type_name)(field_proto);
  bool has_name = HPB_DESC(FieldDescriptorProto_has_type_name)(field_proto);
  switch ((int)f->type_) {
    case HPB_FIELD_TYPE_UNSPECIFIED: {
      // Type was not specified and must be inferred.
      HPB_ASSERT(has_name);
      hpb_deftype_t type;
      const void* def =
          _hpb_DefBuilder_ResolveAny(ctx, f->full_name, prefix, name, &type);
      switch (type) {
        case HPB_DEFTYPE_ENUM:
          f->sub.enumdef = def;
          f->type_ = kHpb_FieldType_Enum;
          if (!HPB_DESC(FieldOptions_has_packed)(f->opts)) {
            f->is_packed = f->label_ == kHpb_Label_Repeated &&
                           hpb_FileDef_Syntax(f->file) == kHpb_Syntax_Proto3;
          }
          break;
        case HPB_DEFTYPE_MSG:
          f->sub.msgdef = def;
          f->type_ = kHpb_FieldType_Message;  // It appears there is no way of
                                              // this being a group.
          f->has_presence = !hpb_FieldDef_IsRepeated(f);
          break;
        default:
          _hpb_DefBuilder_Errf(ctx, "Couldn't resolve type name for field %s",
                               f->full_name);
      }
      break;
    }
    case kHpb_FieldType_Message:
    case kHpb_FieldType_Group:
      HPB_ASSERT(has_name);
      f->sub.msgdef = _hpb_DefBuilder_Resolve(ctx, f->full_name, prefix, name,
                                              HPB_DEFTYPE_MSG);
      break;
    case kHpb_FieldType_Enum:
      HPB_ASSERT(has_name);
      f->sub.enumdef = _hpb_DefBuilder_Resolve(ctx, f->full_name, prefix, name,
                                               HPB_DEFTYPE_ENUM);
      break;
    default:
      // No resolution necessary.
      break;
  }
}

static int _hpb_FieldDef_Compare(const void* p1, const void* p2) {
  const uint32_t v1 = (*(hpb_FieldDef**)p1)->number_;
  const uint32_t v2 = (*(hpb_FieldDef**)p2)->number_;
  return (v1 < v2) ? -1 : (v1 > v2);
}

// _hpb_FieldDefs_Sorted() is mostly a pure function of its inputs, but has one
// critical side effect that we depend on: it sets layout_index appropriately
// for non-sorted lists of fields.
const hpb_FieldDef** _hpb_FieldDefs_Sorted(const hpb_FieldDef* f, int n,
                                           hpb_Arena* a) {
  // TODO(salo): Replace this arena alloc with a persistent scratch buffer.
  hpb_FieldDef** out = (hpb_FieldDef**)hpb_Arena_Malloc(a, n * sizeof(void*));
  if (!out) return NULL;

  for (int i = 0; i < n; i++) {
    out[i] = (hpb_FieldDef*)&f[i];
  }
  qsort(out, n, sizeof(void*), _hpb_FieldDef_Compare);

  for (int i = 0; i < n; i++) {
    out[i]->layout_index = i;
  }
  return (const hpb_FieldDef**)out;
}

bool hpb_FieldDef_MiniDescriptorEncode(const hpb_FieldDef* f, hpb_Arena* a,
                                       hpb_StringView* out) {
  HPB_ASSERT(f->is_extension);

  hpb_DescState s;
  _hpb_DescState_Init(&s);

  const int number = hpb_FieldDef_Number(f);
  const uint64_t modifiers = _hpb_FieldDef_Modifiers(f);

  if (!_hpb_DescState_Grow(&s, a)) return false;
  s.ptr = hpb_MtDataEncoder_EncodeExtension(&s.e, s.ptr, f->type_, number,
                                            modifiers);
  *s.ptr = '\0';

  out->data = s.buf;
  out->size = s.ptr - s.buf;
  return true;
}

static void resolve_extension(hpb_DefBuilder* ctx, const char* prefix,
                              hpb_FieldDef* f,
                              const HPB_DESC(FieldDescriptorProto) *
                                  field_proto) {
  if (!HPB_DESC(FieldDescriptorProto_has_extendee)(field_proto)) {
    _hpb_DefBuilder_Errf(ctx, "extension for field '%s' had no extendee",
                         f->full_name);
  }

  hpb_StringView name = HPB_DESC(FieldDescriptorProto_extendee)(field_proto);
  const hpb_MessageDef* m =
      _hpb_DefBuilder_Resolve(ctx, f->full_name, prefix, name, HPB_DEFTYPE_MSG);
  f->msgdef = m;

  if (!_hpb_MessageDef_IsValidExtensionNumber(m, f->number_)) {
    _hpb_DefBuilder_Errf(
        ctx,
        "field number %u in extension %s has no extension range in message %s",
        (unsigned)f->number_, f->full_name, hpb_MessageDef_FullName(m));
  }
}

void _hpb_FieldDef_BuildMiniTableExtension(hpb_DefBuilder* ctx,
                                           const hpb_FieldDef* f) {
  const hpb_MiniTableExtension* ext = _hpb_FieldDef_ExtensionMiniTable(f);

  if (ctx->layout) {
    HPB_ASSERT(hpb_FieldDef_Number(f) == ext->field.number);
  } else {
    hpb_StringView desc;
    if (!hpb_FieldDef_MiniDescriptorEncode(f, ctx->tmp_arena, &desc)) {
      _hpb_DefBuilder_OomErr(ctx);
    }

    hpb_MiniTableExtension* mut_ext = (hpb_MiniTableExtension*)ext;
    hpb_MiniTableSub sub = {NULL};
    if (hpb_FieldDef_IsSubMessage(f)) {
      sub.submsg = hpb_MessageDef_MiniTable(f->sub.msgdef);
    } else if (_hpb_FieldDef_IsClosedEnum(f)) {
      sub.subenum = _hpb_EnumDef_MiniTable(f->sub.enumdef);
    }
    bool ok2 = hpb_MiniTableExtension_Init(desc.data, desc.size, mut_ext,
                                           hpb_MessageDef_MiniTable(f->msgdef),
                                           sub, ctx->status);
    if (!ok2) _hpb_DefBuilder_Errf(ctx, "Could not build extension mini table");
  }

  bool ok = _hpb_DefPool_InsertExt(ctx->symtab, ext, f);
  if (!ok) _hpb_DefBuilder_OomErr(ctx);
}

static void resolve_default(hpb_DefBuilder* ctx, hpb_FieldDef* f,
                            const HPB_DESC(FieldDescriptorProto) *
                                field_proto) {
  // Have to delay resolving of the default value until now because of the enum
  // case, since enum defaults are specified with a label.
  if (HPB_DESC(FieldDescriptorProto_has_default_value)(field_proto)) {
    hpb_StringView defaultval =
        HPB_DESC(FieldDescriptorProto_default_value)(field_proto);

    if (hpb_FileDef_Syntax(f->file) == kHpb_Syntax_Proto3) {
      _hpb_DefBuilder_Errf(ctx,
                           "proto3 fields cannot have explicit defaults (%s)",
                           f->full_name);
    }

    if (hpb_FieldDef_IsSubMessage(f)) {
      _hpb_DefBuilder_Errf(ctx,
                           "message fields cannot have explicit defaults (%s)",
                           f->full_name);
    }

    parse_default(ctx, defaultval.data, defaultval.size, f);
    f->has_default = true;
  } else {
    set_default_default(ctx, f);
    f->has_default = false;
  }
}

void _hpb_FieldDef_Resolve(hpb_DefBuilder* ctx, const char* prefix,
                           hpb_FieldDef* f) {
  // We have to stash this away since resolve_subdef() may overwrite it.
  const HPB_DESC(FieldDescriptorProto)* field_proto = f->sub.unresolved;

  resolve_subdef(ctx, prefix, f);
  resolve_default(ctx, f, field_proto);

  if (f->is_extension) {
    resolve_extension(ctx, prefix, f, field_proto);
  }
}
