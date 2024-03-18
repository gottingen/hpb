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

#include "hpb/collections/internal/map_sorter.h"

#include "hpb/base/internal/log2.h"

// Must be last.
#include "hpb/port/def.inc"

static void _hpb_mapsorter_getkeys(const void* _a, const void* _b, void* a_key,
                                   void* b_key, size_t size) {
  const hpb_tabent* const* a = _a;
  const hpb_tabent* const* b = _b;
  hpb_StringView a_tabkey = hpb_tabstrview((*a)->key);
  hpb_StringView b_tabkey = hpb_tabstrview((*b)->key);
  _hpb_map_fromkey(a_tabkey, a_key, size);
  _hpb_map_fromkey(b_tabkey, b_key, size);
}

static int _hpb_mapsorter_cmpi64(const void* _a, const void* _b) {
  int64_t a, b;
  _hpb_mapsorter_getkeys(_a, _b, &a, &b, 8);
  return a < b ? -1 : a > b;
}

static int _hpb_mapsorter_cmpu64(const void* _a, const void* _b) {
  uint64_t a, b;
  _hpb_mapsorter_getkeys(_a, _b, &a, &b, 8);
  return a < b ? -1 : a > b;
}

static int _hpb_mapsorter_cmpi32(const void* _a, const void* _b) {
  int32_t a, b;
  _hpb_mapsorter_getkeys(_a, _b, &a, &b, 4);
  return a < b ? -1 : a > b;
}

static int _hpb_mapsorter_cmpu32(const void* _a, const void* _b) {
  uint32_t a, b;
  _hpb_mapsorter_getkeys(_a, _b, &a, &b, 4);
  return a < b ? -1 : a > b;
}

static int _hpb_mapsorter_cmpbool(const void* _a, const void* _b) {
  bool a, b;
  _hpb_mapsorter_getkeys(_a, _b, &a, &b, 1);
  return a < b ? -1 : a > b;
}

static int _hpb_mapsorter_cmpstr(const void* _a, const void* _b) {
  hpb_StringView a, b;
  _hpb_mapsorter_getkeys(_a, _b, &a, &b, HPB_MAPTYPE_STRING);
  size_t common_size = HPB_MIN(a.size, b.size);
  int cmp = memcmp(a.data, b.data, common_size);
  if (cmp) return -cmp;
  return a.size < b.size ? -1 : a.size > b.size;
}

static int (*const compar[kHpb_FieldType_SizeOf])(const void*, const void*) = {
    [kHpb_FieldType_Int64] = _hpb_mapsorter_cmpi64,
    [kHpb_FieldType_SFixed64] = _hpb_mapsorter_cmpi64,
    [kHpb_FieldType_SInt64] = _hpb_mapsorter_cmpi64,

    [kHpb_FieldType_UInt64] = _hpb_mapsorter_cmpu64,
    [kHpb_FieldType_Fixed64] = _hpb_mapsorter_cmpu64,

    [kHpb_FieldType_Int32] = _hpb_mapsorter_cmpi32,
    [kHpb_FieldType_SInt32] = _hpb_mapsorter_cmpi32,
    [kHpb_FieldType_SFixed32] = _hpb_mapsorter_cmpi32,
    [kHpb_FieldType_Enum] = _hpb_mapsorter_cmpi32,

    [kHpb_FieldType_UInt32] = _hpb_mapsorter_cmpu32,
    [kHpb_FieldType_Fixed32] = _hpb_mapsorter_cmpu32,

    [kHpb_FieldType_Bool] = _hpb_mapsorter_cmpbool,

    [kHpb_FieldType_String] = _hpb_mapsorter_cmpstr,
    [kHpb_FieldType_Bytes] = _hpb_mapsorter_cmpstr,
};

static bool _hpb_mapsorter_resize(_hpb_mapsorter* s, _hpb_sortedmap* sorted,
                                  int size) {
  sorted->start = s->size;
  sorted->pos = sorted->start;
  sorted->end = sorted->start + size;

  if (sorted->end > s->cap) {
    s->cap = hpb_Log2CeilingSize(sorted->end);
    s->entries = realloc(s->entries, s->cap * sizeof(*s->entries));
    if (!s->entries) return false;
  }

  s->size = sorted->end;
  return true;
}

bool _hpb_mapsorter_pushmap(_hpb_mapsorter* s, hpb_FieldType key_type,
                            const hpb_Map* map, _hpb_sortedmap* sorted) {
  int map_size = _hpb_Map_Size(map);

  if (!_hpb_mapsorter_resize(s, sorted, map_size)) return false;

  // Copy non-empty entries from the table to s->entries.
  const void** dst = &s->entries[sorted->start];
  const hpb_tabent* src = map->table.t.entries;
  const hpb_tabent* end = src + hpb_table_size(&map->table.t);
  for (; src < end; src++) {
    if (!hpb_tabent_isempty(src)) {
      *dst = src;
      dst++;
    }
  }
  HPB_ASSERT(dst == &s->entries[sorted->end]);

  // Sort entries according to the key type.
  qsort(&s->entries[sorted->start], map_size, sizeof(*s->entries),
        compar[key_type]);
  return true;
}

static int _hpb_mapsorter_cmpext(const void* _a, const void* _b) {
  const hpb_Message_Extension* const* a = _a;
  const hpb_Message_Extension* const* b = _b;
  uint32_t a_num = (*a)->ext->field.number;
  uint32_t b_num = (*b)->ext->field.number;
  assert(a_num != b_num);
  return a_num < b_num ? -1 : 1;
}

bool _hpb_mapsorter_pushexts(_hpb_mapsorter* s,
                             const hpb_Message_Extension* exts, size_t count,
                             _hpb_sortedmap* sorted) {
  if (!_hpb_mapsorter_resize(s, sorted, count)) return false;

  for (size_t i = 0; i < count; i++) {
    s->entries[sorted->start + i] = &exts[i];
  }

  qsort(&s->entries[sorted->start], count, sizeof(*s->entries),
        _hpb_mapsorter_cmpext);
  return true;
}
