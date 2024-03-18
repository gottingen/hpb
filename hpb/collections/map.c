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

#include "hpb/collections/map.h"

#include <string.h>

#include "hpb/collections/internal/map.h"
#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

// Strings/bytes are special-cased in maps.
char _hpb_Map_CTypeSizeTable[12] = {
    [kHpb_CType_Bool] = 1,
    [kHpb_CType_Float] = 4,
    [kHpb_CType_Int32] = 4,
    [kHpb_CType_UInt32] = 4,
    [kHpb_CType_Enum] = 4,
    [kHpb_CType_Message] = sizeof(void*),
    [kHpb_CType_Double] = 8,
    [kHpb_CType_Int64] = 8,
    [kHpb_CType_UInt64] = 8,
    [kHpb_CType_String] = HPB_MAPTYPE_STRING,
    [kHpb_CType_Bytes] = HPB_MAPTYPE_STRING,
};

hpb_Map* hpb_Map_New(hpb_Arena* a, hpb_CType key_type, hpb_CType value_type) {
  return _hpb_Map_New(a, _hpb_Map_CTypeSize(key_type),
                      _hpb_Map_CTypeSize(value_type));
}

size_t hpb_Map_Size(const hpb_Map* map) { return _hpb_Map_Size(map); }

bool hpb_Map_Get(const hpb_Map* map, hpb_MessageValue key,
                 hpb_MessageValue* val) {
  return _hpb_Map_Get(map, &key, map->key_size, val, map->val_size);
}

void hpb_Map_Clear(hpb_Map* map) { _hpb_Map_Clear(map); }

hpb_MapInsertStatus hpb_Map_Insert(hpb_Map* map, hpb_MessageValue key,
                                   hpb_MessageValue val, hpb_Arena* arena) {
  HPB_ASSERT(arena);
  return (hpb_MapInsertStatus)_hpb_Map_Insert(map, &key, map->key_size, &val,
                                              map->val_size, arena);
}

bool hpb_Map_Delete(hpb_Map* map, hpb_MessageValue key, hpb_MessageValue* val) {
  hpb_value v;
  const bool removed = _hpb_Map_Delete(map, &key, map->key_size, &v);
  if (val) _hpb_map_fromvalue(v, val, map->val_size);
  return removed;
}

bool hpb_Map_Next(const hpb_Map* map, hpb_MessageValue* key,
                  hpb_MessageValue* val, size_t* iter) {
  hpb_StringView k;
  hpb_value v;
  const bool ok = hpb_strtable_next2(&map->table, &k, &v, (intptr_t*)iter);
  if (ok) {
    _hpb_map_fromkey(k, key, map->key_size);
    _hpb_map_fromvalue(v, val, map->val_size);
  }
  return ok;
}

HPB_API void hpb_Map_SetEntryValue(hpb_Map* map, size_t iter,
                                   hpb_MessageValue val) {
  hpb_value v;
  _hpb_map_tovalue(&val, map->val_size, &v, NULL);
  hpb_strtable_setentryvalue(&map->table, iter, v);
}

bool hpb_MapIterator_Next(const hpb_Map* map, size_t* iter) {
  return _hpb_map_next(map, iter);
}

bool hpb_MapIterator_Done(const hpb_Map* map, size_t iter) {
  hpb_strtable_iter i;
  HPB_ASSERT(iter != kHpb_Map_Begin);
  i.t = &map->table;
  i.index = iter;
  return hpb_strtable_done(&i);
}

// Returns the key and value for this entry of the map.
hpb_MessageValue hpb_MapIterator_Key(const hpb_Map* map, size_t iter) {
  hpb_strtable_iter i;
  hpb_MessageValue ret;
  i.t = &map->table;
  i.index = iter;
  _hpb_map_fromkey(hpb_strtable_iter_key(&i), &ret, map->key_size);
  return ret;
}

hpb_MessageValue hpb_MapIterator_Value(const hpb_Map* map, size_t iter) {
  hpb_strtable_iter i;
  hpb_MessageValue ret;
  i.t = &map->table;
  i.index = iter;
  _hpb_map_fromvalue(hpb_strtable_iter_value(&i), &ret, map->val_size);
  return ret;
}

// EVERYTHING BELOW THIS LINE IS INTERNAL - DO NOT USE /////////////////////////

hpb_Map* _hpb_Map_New(hpb_Arena* a, size_t key_size, size_t value_size) {
  hpb_Map* map = hpb_Arena_Malloc(a, sizeof(hpb_Map));
  if (!map) return NULL;

  hpb_strtable_init(&map->table, 4, a);
  map->key_size = key_size;
  map->val_size = value_size;

  return map;
}
