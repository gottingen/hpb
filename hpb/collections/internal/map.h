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

// EVERYTHING BELOW THIS LINE IS INTERNAL - DO NOT USE /////////////////////////

#ifndef HPB_COLLECTIONS_INTERNAL_MAP_H_
#define HPB_COLLECTIONS_INTERNAL_MAP_H_

#include "hpb/base/string_view.h"
#include "hpb/collections/map.h"
#include "hpb/hash/str_table.h"
#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

struct hpb_Map {
  // Size of key and val, based on the map type.
  // Strings are represented as '0' because they must be handled specially.
  char key_size;
  char val_size;

  hpb_strtable table;
};

#ifdef __cplusplus
extern "C" {
#endif

// Converting between internal table representation and user values.
//
// _hpb_map_tokey() and _hpb_map_fromkey() are inverses.
// _hpb_map_tovalue() and _hpb_map_fromvalue() are inverses.
//
// These functions account for the fact that strings are treated differently
// from other types when stored in a map.

HPB_INLINE hpb_StringView _hpb_map_tokey(const void* key, size_t size) {
  if (size == HPB_MAPTYPE_STRING) {
    return *(hpb_StringView*)key;
  } else {
    return hpb_StringView_FromDataAndSize((const char*)key, size);
  }
}

HPB_INLINE void _hpb_map_fromkey(hpb_StringView key, void* out, size_t size) {
  if (size == HPB_MAPTYPE_STRING) {
    memcpy(out, &key, sizeof(key));
  } else {
    memcpy(out, key.data, size);
  }
}

HPB_INLINE bool _hpb_map_tovalue(const void* val, size_t size,
                                 hpb_value* msgval, hpb_Arena* a) {
  if (size == HPB_MAPTYPE_STRING) {
    hpb_StringView* strp = (hpb_StringView*)hpb_Arena_Malloc(a, sizeof(*strp));
    if (!strp) return false;
    *strp = *(hpb_StringView*)val;
    *msgval = hpb_value_ptr(strp);
  } else {
    memcpy(msgval, val, size);
  }
  return true;
}

HPB_INLINE void _hpb_map_fromvalue(hpb_value val, void* out, size_t size) {
  if (size == HPB_MAPTYPE_STRING) {
    const hpb_StringView* strp = (const hpb_StringView*)hpb_value_getptr(val);
    memcpy(out, strp, sizeof(hpb_StringView));
  } else {
    memcpy(out, &val, size);
  }
}

HPB_INLINE void* _hpb_map_next(const hpb_Map* map, size_t* iter) {
  hpb_strtable_iter it;
  it.t = &map->table;
  it.index = *iter;
  hpb_strtable_next(&it);
  *iter = it.index;
  if (hpb_strtable_done(&it)) return NULL;
  return (void*)str_tabent(&it);
}

HPB_INLINE void _hpb_Map_Clear(hpb_Map* map) {
  hpb_strtable_clear(&map->table);
}

HPB_INLINE bool _hpb_Map_Delete(hpb_Map* map, const void* key, size_t key_size,
                                hpb_value* val) {
  hpb_StringView k = _hpb_map_tokey(key, key_size);
  return hpb_strtable_remove2(&map->table, k.data, k.size, val);
}

HPB_INLINE bool _hpb_Map_Get(const hpb_Map* map, const void* key,
                             size_t key_size, void* val, size_t val_size) {
  hpb_value tabval;
  hpb_StringView k = _hpb_map_tokey(key, key_size);
  bool ret = hpb_strtable_lookup2(&map->table, k.data, k.size, &tabval);
  if (ret && val) {
    _hpb_map_fromvalue(tabval, val, val_size);
  }
  return ret;
}

HPB_INLINE hpb_MapInsertStatus _hpb_Map_Insert(hpb_Map* map, const void* key,
                                               size_t key_size, void* val,
                                               size_t val_size, hpb_Arena* a) {
  hpb_StringView strkey = _hpb_map_tokey(key, key_size);
  hpb_value tabval = {0};
  if (!_hpb_map_tovalue(val, val_size, &tabval, a)) {
    return kHpb_MapInsertStatus_OutOfMemory;
  }

  // TODO(haberman): add overwrite operation to minimize number of lookups.
  bool removed =
      hpb_strtable_remove2(&map->table, strkey.data, strkey.size, NULL);
  if (!hpb_strtable_insert(&map->table, strkey.data, strkey.size, tabval, a)) {
    return kHpb_MapInsertStatus_OutOfMemory;
  }
  return removed ? kHpb_MapInsertStatus_Replaced
                 : kHpb_MapInsertStatus_Inserted;
}

HPB_INLINE size_t _hpb_Map_Size(const hpb_Map* map) {
  return map->table.t.count;
}

// Strings/bytes are special-cased in maps.
extern char _hpb_Map_CTypeSizeTable[12];

HPB_INLINE size_t _hpb_Map_CTypeSize(hpb_CType ctype) {
  return _hpb_Map_CTypeSizeTable[ctype];
}

// Creates a new map on the given arena with this key/value type.
hpb_Map* _hpb_Map_New(hpb_Arena* a, size_t key_size, size_t value_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_COLLECTIONS_INTERNAL_MAP_H_ */
