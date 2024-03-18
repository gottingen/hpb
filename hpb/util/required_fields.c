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

#include "hpb/util/required_fields.h"

#include <inttypes.h>
#include <stdarg.h>

#include "hpb/collections/map.h"
#include "hpb/port/vsnprintf_compat.h"
#include "hpb/reflection/message.h"

// Must be last.
#include "hpb/port/def.inc"

////////////////////////////////////////////////////////////////////////////////
// hpb_FieldPath_ToText()
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  char* buf;
  char* ptr;
  char* end;
  size_t overflow;
} hpb_PrintfAppender;

HPB_PRINTF(2, 3)
static void hpb_FieldPath_Printf(hpb_PrintfAppender* a, const char* fmt, ...) {
  size_t n;
  size_t have = a->end - a->ptr;
  va_list args;

  va_start(args, fmt);
  n = _hpb_vsnprintf(a->ptr, have, fmt, args);
  va_end(args);

  if (HPB_LIKELY(have > n)) {
    // We can't end up here if the user passed (NULL, 0), therefore ptr is known
    // to be non-NULL, and HPB_PTRADD() is not necessary.
    assert(a->ptr);
    a->ptr += n;
  } else {
    a->ptr = HPB_PTRADD(a->ptr, have);
    a->overflow += (n - have);
  }
}

static size_t hpb_FieldPath_NullTerminate(hpb_PrintfAppender* d, size_t size) {
  size_t ret = d->ptr - d->buf + d->overflow;

  if (size > 0) {
    if (d->ptr == d->end) d->ptr--;
    *d->ptr = '\0';
  }

  return ret;
}

static void hpb_FieldPath_PutMapKey(hpb_PrintfAppender* a,
                                    hpb_MessageValue map_key,
                                    const hpb_FieldDef* key_f) {
  switch (hpb_FieldDef_CType(key_f)) {
    case kHpb_CType_Int32:
      hpb_FieldPath_Printf(a, "[%" PRId32 "]", map_key.int32_val);
      break;
    case kHpb_CType_Int64:
      hpb_FieldPath_Printf(a, "[%" PRId64 "]", map_key.int64_val);
      break;
    case kHpb_CType_UInt32:
      hpb_FieldPath_Printf(a, "[%" PRIu32 "]", map_key.uint32_val);
      break;
    case kHpb_CType_UInt64:
      hpb_FieldPath_Printf(a, "[%" PRIu64 "]", map_key.uint64_val);
      break;
    case kHpb_CType_Bool:
      hpb_FieldPath_Printf(a, "[%s]", map_key.bool_val ? "true" : "false");
      break;
    case kHpb_CType_String:
      hpb_FieldPath_Printf(a, "[\"");
      for (size_t i = 0; i < map_key.str_val.size; i++) {
        char ch = map_key.str_val.data[i];
        if (ch == '"') {
          hpb_FieldPath_Printf(a, "\\\"");
        } else {
          hpb_FieldPath_Printf(a, "%c", ch);
        }
      }
      hpb_FieldPath_Printf(a, "\"]");
      break;
    default:
      HPB_UNREACHABLE();  // Other types can't be map keys.
  }
}

size_t hpb_FieldPath_ToText(hpb_FieldPathEntry** path, char* buf, size_t size) {
  hpb_FieldPathEntry* ptr = *path;
  hpb_PrintfAppender appender;
  appender.buf = buf;
  appender.ptr = buf;
  appender.end = HPB_PTRADD(buf, size);
  appender.overflow = 0;
  bool first = true;

  while (ptr->field) {
    const hpb_FieldDef* f = ptr->field;

    hpb_FieldPath_Printf(&appender, first ? "%s" : ".%s", hpb_FieldDef_Name(f));
    first = false;
    ptr++;

    if (hpb_FieldDef_IsMap(f)) {
      const hpb_FieldDef* key_f =
          hpb_MessageDef_Field(hpb_FieldDef_MessageSubDef(f), 0);
      hpb_FieldPath_PutMapKey(&appender, ptr->map_key, key_f);
      ptr++;
    } else if (hpb_FieldDef_IsRepeated(f)) {
      hpb_FieldPath_Printf(&appender, "[%zu]", ptr->array_index);
      ptr++;
    }
  }

  // Advance beyond terminating NULL.
  ptr++;
  *path = ptr;
  return hpb_FieldPath_NullTerminate(&appender, size);
}

////////////////////////////////////////////////////////////////////////////////
// hpb_util_HasUnsetRequired()
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  hpb_FieldPathEntry* path;
  size_t size;
  size_t cap;
} hpb_FieldPathVector;

typedef struct {
  hpb_FieldPathVector stack;
  hpb_FieldPathVector out_fields;
  const hpb_DefPool* ext_pool;
  jmp_buf err;
  bool has_unset_required;
  bool save_paths;
} hpb_FindContext;

static void hpb_FieldPathVector_Init(hpb_FieldPathVector* vec) {
  vec->path = NULL;
  vec->size = 0;
  vec->cap = 0;
}

static void hpb_FieldPathVector_Reserve(hpb_FindContext* ctx,
                                        hpb_FieldPathVector* vec,
                                        size_t elems) {
  if (vec->cap - vec->size < elems) {
    size_t need = vec->size + elems;
    vec->cap = HPB_MAX(4, vec->cap);
    while (vec->cap < need) vec->cap *= 2;
    vec->path = realloc(vec->path, vec->cap * sizeof(*vec->path));
    if (!vec->path) {
      HPB_LONGJMP(ctx->err, 1);
    }
  }
}

static void hpb_FindContext_Push(hpb_FindContext* ctx, hpb_FieldPathEntry ent) {
  if (!ctx->save_paths) return;
  hpb_FieldPathVector_Reserve(ctx, &ctx->stack, 1);
  ctx->stack.path[ctx->stack.size++] = ent;
}

static void hpb_FindContext_Pop(hpb_FindContext* ctx) {
  if (!ctx->save_paths) return;
  assert(ctx->stack.size != 0);
  ctx->stack.size--;
}

static void hpb_util_FindUnsetInMessage(hpb_FindContext* ctx,
                                        const hpb_Message* msg,
                                        const hpb_MessageDef* m) {
  // Iterate over all fields to see if any required fields are missing.
  for (int i = 0, n = hpb_MessageDef_FieldCount(m); i < n; i++) {
    const hpb_FieldDef* f = hpb_MessageDef_Field(m, i);
    if (hpb_FieldDef_Label(f) != kHpb_Label_Required) continue;

    if (!msg || !hpb_Message_HasFieldByDef(msg, f)) {
      // A required field is missing.
      ctx->has_unset_required = true;

      if (ctx->save_paths) {
        // Append the contents of the stack to the out array, then
        // NULL-terminate.
        hpb_FieldPathVector_Reserve(ctx, &ctx->out_fields, ctx->stack.size + 2);
        if (ctx->stack.size) {
          memcpy(&ctx->out_fields.path[ctx->out_fields.size], ctx->stack.path,
                 ctx->stack.size * sizeof(*ctx->stack.path));
        }
        ctx->out_fields.size += ctx->stack.size;
        ctx->out_fields.path[ctx->out_fields.size++] =
            (hpb_FieldPathEntry){.field = f};
        ctx->out_fields.path[ctx->out_fields.size++] =
            (hpb_FieldPathEntry){.field = NULL};
      }
    }
  }
}

static void hpb_util_FindUnsetRequiredInternal(hpb_FindContext* ctx,
                                               const hpb_Message* msg,
                                               const hpb_MessageDef* m) {
  // OPT: add markers in the schema for where we can avoid iterating:
  // 1. messages with no required fields.
  // 2. messages that cannot possibly reach any required fields.

  hpb_util_FindUnsetInMessage(ctx, msg, m);
  if (!msg) return;

  // Iterate over all present fields to find sub-messages that might be missing
  // required fields.  This may revisit some of the fields already inspected
  // in the previous loop.  We do this separately because this loop will also
  // find present extensions, which the previous loop will not.
  //
  // TODO(haberman): consider changing hpb_Message_Next() to be capable of
  // visiting extensions only, for example with a kHpb_Message_BeginEXT
  // constant.
  size_t iter = kHpb_Message_Begin;
  const hpb_FieldDef* f;
  hpb_MessageValue val;
  while (hpb_Message_Next(msg, m, ctx->ext_pool, &f, &val, &iter)) {
    // Skip non-submessage fields.
    if (!hpb_FieldDef_IsSubMessage(f)) continue;

    hpb_FindContext_Push(ctx, (hpb_FieldPathEntry){.field = f});
    const hpb_MessageDef* sub_m = hpb_FieldDef_MessageSubDef(f);

    if (hpb_FieldDef_IsMap(f)) {
      // Map field.
      const hpb_FieldDef* val_f = hpb_MessageDef_Field(sub_m, 1);
      const hpb_MessageDef* val_m = hpb_FieldDef_MessageSubDef(val_f);
      if (!val_m) continue;
      const hpb_Map* map = val.map_val;
      size_t iter = kHpb_Map_Begin;
      hpb_MessageValue key, map_val;
      while (hpb_Map_Next(map, &key, &map_val, &iter)) {
        hpb_FindContext_Push(ctx, (hpb_FieldPathEntry){.map_key = key});
        hpb_util_FindUnsetRequiredInternal(ctx, map_val.msg_val, val_m);
        hpb_FindContext_Pop(ctx);
      }
    } else if (hpb_FieldDef_IsRepeated(f)) {
      // Repeated field.
      const hpb_Array* arr = val.array_val;
      for (size_t i = 0, n = hpb_Array_Size(arr); i < n; i++) {
        hpb_MessageValue elem = hpb_Array_Get(arr, i);
        hpb_FindContext_Push(ctx, (hpb_FieldPathEntry){.array_index = i});
        hpb_util_FindUnsetRequiredInternal(ctx, elem.msg_val, sub_m);
        hpb_FindContext_Pop(ctx);
      }
    } else {
      // Scalar sub-message field.
      hpb_util_FindUnsetRequiredInternal(ctx, val.msg_val, sub_m);
    }

    hpb_FindContext_Pop(ctx);
  }
}

bool hpb_util_HasUnsetRequired(const hpb_Message* msg, const hpb_MessageDef* m,
                               const hpb_DefPool* ext_pool,
                               hpb_FieldPathEntry** fields) {
  hpb_FindContext ctx;
  ctx.has_unset_required = false;
  ctx.save_paths = fields != NULL;
  ctx.ext_pool = ext_pool;
  hpb_FieldPathVector_Init(&ctx.stack);
  hpb_FieldPathVector_Init(&ctx.out_fields);
  hpb_util_FindUnsetRequiredInternal(&ctx, msg, m);
  free(ctx.stack.path);
  if (fields) {
    hpb_FieldPathVector_Reserve(&ctx, &ctx.out_fields, 1);
    ctx.out_fields.path[ctx.out_fields.size] =
        (hpb_FieldPathEntry){.field = NULL};
    *fields = ctx.out_fields.path;
  }
  return ctx.has_unset_required;
}
