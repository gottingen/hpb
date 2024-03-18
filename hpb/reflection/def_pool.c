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

#include "hpb/reflection/internal/def_pool.h"

#include "hpb/hash/int_table.h"
#include "hpb/hash/str_table.h"
#include "hpb/reflection/def_type.h"
#include "hpb/reflection/internal/def_builder.h"
#include "hpb/reflection/internal/enum_def.h"
#include "hpb/reflection/internal/enum_value_def.h"
#include "hpb/reflection/internal/field_def.h"
#include "hpb/reflection/internal/file_def.h"
#include "hpb/reflection/internal/message_def.h"
#include "hpb/reflection/internal/service_def.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_DefPool {
  hpb_Arena* arena;
  hpb_strtable syms;   // full_name -> packed def ptr
  hpb_strtable files;  // file_name -> (hpb_FileDef*)
  hpb_inttable exts;   // (hpb_MiniTableExtension*) -> (hpb_FieldDef*)
  hpb_ExtensionRegistry* extreg;
  hpb_MiniTablePlatform platform;
  void* scratch_data;
  size_t scratch_size;
  size_t bytes_loaded;
};

void hpb_DefPool_Free(hpb_DefPool* s) {
  hpb_Arena_Free(s->arena);
  hpb_gfree(s->scratch_data);
  hpb_gfree(s);
}

hpb_DefPool* hpb_DefPool_New(void) {
  hpb_DefPool* s = hpb_gmalloc(sizeof(*s));
  if (!s) return NULL;

  s->arena = hpb_Arena_New();
  s->bytes_loaded = 0;

  s->scratch_size = 240;
  s->scratch_data = hpb_gmalloc(s->scratch_size);
  if (!s->scratch_data) goto err;

  if (!hpb_strtable_init(&s->syms, 32, s->arena)) goto err;
  if (!hpb_strtable_init(&s->files, 4, s->arena)) goto err;
  if (!hpb_inttable_init(&s->exts, s->arena)) goto err;

  s->extreg = hpb_ExtensionRegistry_New(s->arena);
  if (!s->extreg) goto err;

  s->platform = kHpb_MiniTablePlatform_Native;

  return s;

err:
  hpb_DefPool_Free(s);
  return NULL;
}

bool _hpb_DefPool_InsertExt(hpb_DefPool* s, const hpb_MiniTableExtension* ext,
                            const hpb_FieldDef* f) {
  return hpb_inttable_insert(&s->exts, (uintptr_t)ext, hpb_value_constptr(f),
                             s->arena);
}

bool _hpb_DefPool_InsertSym(hpb_DefPool* s, hpb_StringView sym, hpb_value v,
                            hpb_Status* status) {
  // TODO: table should support an operation "tryinsert" to avoid the double
  // lookup.
  if (hpb_strtable_lookup2(&s->syms, sym.data, sym.size, NULL)) {
    hpb_Status_SetErrorFormat(status, "duplicate symbol '%s'", sym.data);
    return false;
  }
  if (!hpb_strtable_insert(&s->syms, sym.data, sym.size, v, s->arena)) {
    hpb_Status_SetErrorMessage(status, "out of memory");
    return false;
  }
  return true;
}

static const void* _hpb_DefPool_Unpack(const hpb_DefPool* s, const char* sym,
                                       size_t size, hpb_deftype_t type) {
  hpb_value v;
  return hpb_strtable_lookup2(&s->syms, sym, size, &v)
             ? _hpb_DefType_Unpack(v, type)
             : NULL;
}

bool _hpb_DefPool_LookupSym(const hpb_DefPool* s, const char* sym, size_t size,
                            hpb_value* v) {
  return hpb_strtable_lookup2(&s->syms, sym, size, v);
}

hpb_ExtensionRegistry* _hpb_DefPool_ExtReg(const hpb_DefPool* s) {
  return s->extreg;
}

void** _hpb_DefPool_ScratchData(const hpb_DefPool* s) {
  return (void**)&s->scratch_data;
}

size_t* _hpb_DefPool_ScratchSize(const hpb_DefPool* s) {
  return (size_t*)&s->scratch_size;
}

void _hpb_DefPool_SetPlatform(hpb_DefPool* s, hpb_MiniTablePlatform platform) {
  assert(hpb_strtable_count(&s->files) == 0);
  s->platform = platform;
}

const hpb_MessageDef* hpb_DefPool_FindMessageByName(const hpb_DefPool* s,
                                                    const char* sym) {
  return _hpb_DefPool_Unpack(s, sym, strlen(sym), HPB_DEFTYPE_MSG);
}

const hpb_MessageDef* hpb_DefPool_FindMessageByNameWithSize(
    const hpb_DefPool* s, const char* sym, size_t len) {
  return _hpb_DefPool_Unpack(s, sym, len, HPB_DEFTYPE_MSG);
}

const hpb_EnumDef* hpb_DefPool_FindEnumByName(const hpb_DefPool* s,
                                              const char* sym) {
  return _hpb_DefPool_Unpack(s, sym, strlen(sym), HPB_DEFTYPE_ENUM);
}

const hpb_EnumValueDef* hpb_DefPool_FindEnumByNameval(const hpb_DefPool* s,
                                                      const char* sym) {
  return _hpb_DefPool_Unpack(s, sym, strlen(sym), HPB_DEFTYPE_ENUMVAL);
}

const hpb_FileDef* hpb_DefPool_FindFileByName(const hpb_DefPool* s,
                                              const char* name) {
  hpb_value v;
  return hpb_strtable_lookup(&s->files, name, &v) ? hpb_value_getconstptr(v)
                                                  : NULL;
}

const hpb_FileDef* hpb_DefPool_FindFileByNameWithSize(const hpb_DefPool* s,
                                                      const char* name,
                                                      size_t len) {
  hpb_value v;
  return hpb_strtable_lookup2(&s->files, name, len, &v)
             ? hpb_value_getconstptr(v)
             : NULL;
}

const hpb_FieldDef* hpb_DefPool_FindExtensionByNameWithSize(
    const hpb_DefPool* s, const char* name, size_t size) {
  hpb_value v;
  if (!hpb_strtable_lookup2(&s->syms, name, size, &v)) return NULL;

  switch (_hpb_DefType_Type(v)) {
    case HPB_DEFTYPE_FIELD:
      return _hpb_DefType_Unpack(v, HPB_DEFTYPE_FIELD);
    case HPB_DEFTYPE_MSG: {
      const hpb_MessageDef* m = _hpb_DefType_Unpack(v, HPB_DEFTYPE_MSG);
      return _hpb_MessageDef_InMessageSet(m)
                 ? hpb_MessageDef_NestedExtension(m, 0)
                 : NULL;
    }
    default:
      break;
  }

  return NULL;
}

const hpb_FieldDef* hpb_DefPool_FindExtensionByName(const hpb_DefPool* s,
                                                    const char* sym) {
  return hpb_DefPool_FindExtensionByNameWithSize(s, sym, strlen(sym));
}

const hpb_ServiceDef* hpb_DefPool_FindServiceByName(const hpb_DefPool* s,
                                                    const char* name) {
  return _hpb_DefPool_Unpack(s, name, strlen(name), HPB_DEFTYPE_SERVICE);
}

const hpb_ServiceDef* hpb_DefPool_FindServiceByNameWithSize(
    const hpb_DefPool* s, const char* name, size_t size) {
  return _hpb_DefPool_Unpack(s, name, size, HPB_DEFTYPE_SERVICE);
}

const hpb_FileDef* hpb_DefPool_FindFileContainingSymbol(const hpb_DefPool* s,
                                                        const char* name) {
  hpb_value v;
  // TODO(haberman): non-extension fields and oneofs.
  if (hpb_strtable_lookup(&s->syms, name, &v)) {
    switch (_hpb_DefType_Type(v)) {
      case HPB_DEFTYPE_EXT: {
        const hpb_FieldDef* f = _hpb_DefType_Unpack(v, HPB_DEFTYPE_EXT);
        return hpb_FieldDef_File(f);
      }
      case HPB_DEFTYPE_MSG: {
        const hpb_MessageDef* m = _hpb_DefType_Unpack(v, HPB_DEFTYPE_MSG);
        return hpb_MessageDef_File(m);
      }
      case HPB_DEFTYPE_ENUM: {
        const hpb_EnumDef* e = _hpb_DefType_Unpack(v, HPB_DEFTYPE_ENUM);
        return hpb_EnumDef_File(e);
      }
      case HPB_DEFTYPE_ENUMVAL: {
        const hpb_EnumValueDef* ev =
            _hpb_DefType_Unpack(v, HPB_DEFTYPE_ENUMVAL);
        return hpb_EnumDef_File(hpb_EnumValueDef_Enum(ev));
      }
      case HPB_DEFTYPE_SERVICE: {
        const hpb_ServiceDef* service =
            _hpb_DefType_Unpack(v, HPB_DEFTYPE_SERVICE);
        return hpb_ServiceDef_File(service);
      }
      default:
        HPB_UNREACHABLE();
    }
  }

  const char* last_dot = strrchr(name, '.');
  if (last_dot) {
    const hpb_MessageDef* parent =
        hpb_DefPool_FindMessageByNameWithSize(s, name, last_dot - name);
    if (parent) {
      const char* shortname = last_dot + 1;
      if (hpb_MessageDef_FindByNameWithSize(parent, shortname,
                                            strlen(shortname), NULL, NULL)) {
        return hpb_MessageDef_File(parent);
      }
    }
  }

  return NULL;
}

static void remove_filedef(hpb_DefPool* s, hpb_FileDef* file) {
  intptr_t iter = HPB_INTTABLE_BEGIN;
  hpb_StringView key;
  hpb_value val;
  while (hpb_strtable_next2(&s->syms, &key, &val, &iter)) {
    const hpb_FileDef* f;
    switch (_hpb_DefType_Type(val)) {
      case HPB_DEFTYPE_EXT:
        f = hpb_FieldDef_File(_hpb_DefType_Unpack(val, HPB_DEFTYPE_EXT));
        break;
      case HPB_DEFTYPE_MSG:
        f = hpb_MessageDef_File(_hpb_DefType_Unpack(val, HPB_DEFTYPE_MSG));
        break;
      case HPB_DEFTYPE_ENUM:
        f = hpb_EnumDef_File(_hpb_DefType_Unpack(val, HPB_DEFTYPE_ENUM));
        break;
      case HPB_DEFTYPE_ENUMVAL:
        f = hpb_EnumDef_File(hpb_EnumValueDef_Enum(
            _hpb_DefType_Unpack(val, HPB_DEFTYPE_ENUMVAL)));
        break;
      case HPB_DEFTYPE_SERVICE:
        f = hpb_ServiceDef_File(_hpb_DefType_Unpack(val, HPB_DEFTYPE_SERVICE));
        break;
      default:
        HPB_UNREACHABLE();
    }

    if (f == file) hpb_strtable_removeiter(&s->syms, &iter);
  }
}

static const hpb_FileDef* hpb_DefBuilder_AddFileToPool(
    hpb_DefBuilder* const builder, hpb_DefPool* const s,
    const HPB_DESC(FileDescriptorProto) * const file_proto,
    const hpb_StringView name, hpb_Status* const status) {
  if (HPB_SETJMP(builder->err) != 0) {
    HPB_ASSERT(!hpb_Status_IsOk(status));
    if (builder->file) {
      remove_filedef(s, builder->file);
      builder->file = NULL;
    }
  } else if (!builder->arena || !builder->tmp_arena) {
    _hpb_DefBuilder_OomErr(builder);
  } else {
    _hpb_FileDef_Create(builder, file_proto);
    hpb_strtable_insert(&s->files, name.data, name.size,
                        hpb_value_constptr(builder->file), builder->arena);
    HPB_ASSERT(hpb_Status_IsOk(status));
    hpb_Arena_Fuse(s->arena, builder->arena);
  }

  if (builder->arena) hpb_Arena_Free(builder->arena);
  if (builder->tmp_arena) hpb_Arena_Free(builder->tmp_arena);
  return builder->file;
}

static const hpb_FileDef* _hpb_DefPool_AddFile(
    hpb_DefPool* s, const HPB_DESC(FileDescriptorProto) * file_proto,
    const hpb_MiniTableFile* layout, hpb_Status* status) {
  const hpb_StringView name = HPB_DESC(FileDescriptorProto_name)(file_proto);

  // Determine whether we already know about this file.
  {
    hpb_value v;
    if (hpb_strtable_lookup2(&s->files, name.data, name.size, &v)) {
      hpb_Status_SetErrorFormat(status,
                                "duplicate file name " HPB_STRINGVIEW_FORMAT,
                                HPB_STRINGVIEW_ARGS(name));
      return NULL;
    }
  }

  hpb_DefBuilder ctx = {
      .symtab = s,
      .layout = layout,
      .platform = s->platform,
      .msg_count = 0,
      .enum_count = 0,
      .ext_count = 0,
      .status = status,
      .file = NULL,
      .arena = hpb_Arena_New(),
      .tmp_arena = hpb_Arena_New(),
  };

  return hpb_DefBuilder_AddFileToPool(&ctx, s, file_proto, name, status);
}

const hpb_FileDef* hpb_DefPool_AddFile(hpb_DefPool* s,
                                       const HPB_DESC(FileDescriptorProto) *
                                           file_proto,
                                       hpb_Status* status) {
  return _hpb_DefPool_AddFile(s, file_proto, NULL, status);
}

bool _hpb_DefPool_LoadDefInitEx(hpb_DefPool* s, const _hpb_DefPool_Init* init,
                                bool rebuild_minitable) {
  /* Since this function should never fail (it would indicate a bug in hpb) we
   * print errors to stderr instead of returning error status to the user. */
  _hpb_DefPool_Init** deps = init->deps;
  HPB_DESC(FileDescriptorProto) * file;
  hpb_Arena* arena;
  hpb_Status status;

  hpb_Status_Clear(&status);

  if (hpb_DefPool_FindFileByName(s, init->filename)) {
    return true;
  }

  arena = hpb_Arena_New();

  for (; *deps; deps++) {
    if (!_hpb_DefPool_LoadDefInitEx(s, *deps, rebuild_minitable)) goto err;
  }

  file = HPB_DESC(FileDescriptorProto_parse_ex)(
      init->descriptor.data, init->descriptor.size, NULL,
      kHpb_DecodeOption_AliasString, arena);
  s->bytes_loaded += init->descriptor.size;

  if (!file) {
    hpb_Status_SetErrorFormat(
        &status,
        "Failed to parse compiled-in descriptor for file '%s'. This should "
        "never happen.",
        init->filename);
    goto err;
  }

  const hpb_MiniTableFile* mt = rebuild_minitable ? NULL : init->layout;
  if (!_hpb_DefPool_AddFile(s, file, mt, &status)) {
    goto err;
  }

  hpb_Arena_Free(arena);
  return true;

err:
  fprintf(stderr,
          "Error loading compiled-in descriptor for file '%s' (this should "
          "never happen): %s\n",
          init->filename, hpb_Status_ErrorMessage(&status));
  hpb_Arena_Free(arena);
  return false;
}

size_t _hpb_DefPool_BytesLoaded(const hpb_DefPool* s) {
  return s->bytes_loaded;
}

hpb_Arena* _hpb_DefPool_Arena(const hpb_DefPool* s) { return s->arena; }

const hpb_FieldDef* hpb_DefPool_FindExtensionByMiniTable(
    const hpb_DefPool* s, const hpb_MiniTableExtension* ext) {
  hpb_value v;
  bool ok = hpb_inttable_lookup(&s->exts, (uintptr_t)ext, &v);
  HPB_ASSERT(ok);
  return hpb_value_getconstptr(v);
}

const hpb_FieldDef* hpb_DefPool_FindExtensionByNumber(const hpb_DefPool* s,
                                                      const hpb_MessageDef* m,
                                                      int32_t fieldnum) {
  const hpb_MiniTable* t = hpb_MessageDef_MiniTable(m);
  const hpb_MiniTableExtension* ext =
      hpb_ExtensionRegistry_Lookup(s->extreg, t, fieldnum);
  return ext ? hpb_DefPool_FindExtensionByMiniTable(s, ext) : NULL;
}

const hpb_ExtensionRegistry* hpb_DefPool_ExtensionRegistry(
    const hpb_DefPool* s) {
  return s->extreg;
}

const hpb_FieldDef** hpb_DefPool_GetAllExtensions(const hpb_DefPool* s,
                                                  const hpb_MessageDef* m,
                                                  size_t* count) {
  size_t n = 0;
  intptr_t iter = HPB_INTTABLE_BEGIN;
  uintptr_t key;
  hpb_value val;
  // This is O(all exts) instead of O(exts for m).  If we need this to be
  // efficient we may need to make extreg into a two-level table, or have a
  // second per-message index.
  while (hpb_inttable_next(&s->exts, &key, &val, &iter)) {
    const hpb_FieldDef* f = hpb_value_getconstptr(val);
    if (hpb_FieldDef_ContainingType(f) == m) n++;
  }
  const hpb_FieldDef** exts = malloc(n * sizeof(*exts));
  iter = HPB_INTTABLE_BEGIN;
  size_t i = 0;
  while (hpb_inttable_next(&s->exts, &key, &val, &iter)) {
    const hpb_FieldDef* f = hpb_value_getconstptr(val);
    if (hpb_FieldDef_ContainingType(f) == m) exts[i++] = f;
  }
  *count = n;
  return exts;
}

bool _hpb_DefPool_LoadDefInit(hpb_DefPool* s, const _hpb_DefPool_Init* init) {
  return _hpb_DefPool_LoadDefInitEx(s, init, false);
}
