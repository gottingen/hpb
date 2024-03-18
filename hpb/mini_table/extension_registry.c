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

#include "hpb/mini_table/extension_registry.h"

#include "hpb/hash/str_table.h"
#include "hpb/mini_table/extension.h"

// Must be last.
#include "hpb/port/def.inc"

#define EXTREG_KEY_SIZE (sizeof(hpb_MiniTable*) + sizeof(uint32_t))

struct hpb_ExtensionRegistry {
  hpb_Arena* arena;
  hpb_strtable exts;  // Key is hpb_MiniTable* concatenated with fieldnum.
};

static void extreg_key(char* buf, const hpb_MiniTable* l, uint32_t fieldnum) {
  memcpy(buf, &l, sizeof(l));
  memcpy(buf + sizeof(l), &fieldnum, sizeof(fieldnum));
}

hpb_ExtensionRegistry* hpb_ExtensionRegistry_New(hpb_Arena* arena) {
  hpb_ExtensionRegistry* r = hpb_Arena_Malloc(arena, sizeof(*r));
  if (!r) return NULL;
  r->arena = arena;
  if (!hpb_strtable_init(&r->exts, 8, arena)) return NULL;
  return r;
}

HPB_API bool hpb_ExtensionRegistry_Add(hpb_ExtensionRegistry* r,
                                       const hpb_MiniTableExtension* e) {
  char buf[EXTREG_KEY_SIZE];
  extreg_key(buf, e->extendee, e->field.number);
  if (hpb_strtable_lookup2(&r->exts, buf, EXTREG_KEY_SIZE, NULL)) return false;
  return hpb_strtable_insert(&r->exts, buf, EXTREG_KEY_SIZE,
                             hpb_value_constptr(e), r->arena);
}

bool hpb_ExtensionRegistry_AddArray(hpb_ExtensionRegistry* r,
                                    const hpb_MiniTableExtension** e,
                                    size_t count) {
  const hpb_MiniTableExtension** start = e;
  const hpb_MiniTableExtension** end = HPB_PTRADD(e, count);
  for (; e < end; e++) {
    if (!hpb_ExtensionRegistry_Add(r, *e)) goto failure;
  }
  return true;

failure:
  // Back out the entries previously added.
  for (end = e, e = start; e < end; e++) {
    const hpb_MiniTableExtension* ext = *e;
    char buf[EXTREG_KEY_SIZE];
    extreg_key(buf, ext->extendee, ext->field.number);
    hpb_strtable_remove2(&r->exts, buf, EXTREG_KEY_SIZE, NULL);
  }
  return false;
}

const hpb_MiniTableExtension* hpb_ExtensionRegistry_Lookup(
    const hpb_ExtensionRegistry* r, const hpb_MiniTable* t, uint32_t num) {
  char buf[EXTREG_KEY_SIZE];
  hpb_value v;
  extreg_key(buf, t, num);
  if (hpb_strtable_lookup2(&r->exts, buf, EXTREG_KEY_SIZE, &v)) {
    return hpb_value_getconstptr(v);
  } else {
    return NULL;
  }
}
