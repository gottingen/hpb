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

#ifndef HPB_COLLECTIONS_INTERNAL_MAP_SORTER_H_
#define HPB_COLLECTIONS_INTERNAL_MAP_SORTER_H_

#include <stdlib.h>

#include "hpb/collections/internal/map.h"
#include "hpb/message/internal/extension.h"
#include "hpb/message/internal/map_entry.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// _hpb_mapsorter sorts maps and provides ordered iteration over the entries.
// Since maps can be recursive (map values can be messages which contain other
// maps), _hpb_mapsorter can contain a stack of maps.

typedef struct {
  void const** entries;
  int size;
  int cap;
} _hpb_mapsorter;

typedef struct {
  int start;
  int pos;
  int end;
} _hpb_sortedmap;

HPB_INLINE void _hpb_mapsorter_init(_hpb_mapsorter* s) {
  s->entries = NULL;
  s->size = 0;
  s->cap = 0;
}

HPB_INLINE void _hpb_mapsorter_destroy(_hpb_mapsorter* s) {
  if (s->entries) free(s->entries);
}

HPB_INLINE bool _hpb_sortedmap_next(_hpb_mapsorter* s, const hpb_Map* map,
                                    _hpb_sortedmap* sorted, hpb_MapEntry* ent) {
  if (sorted->pos == sorted->end) return false;
  const hpb_tabent* tabent = (const hpb_tabent*)s->entries[sorted->pos++];
  hpb_StringView key = hpb_tabstrview(tabent->key);
  _hpb_map_fromkey(key, &ent->data.k, map->key_size);
  hpb_value val = {tabent->val.val};
  _hpb_map_fromvalue(val, &ent->data.v, map->val_size);
  return true;
}

HPB_INLINE bool _hpb_sortedmap_nextext(_hpb_mapsorter* s,
                                       _hpb_sortedmap* sorted,
                                       const hpb_Message_Extension** ext) {
  if (sorted->pos == sorted->end) return false;
  *ext = (const hpb_Message_Extension*)s->entries[sorted->pos++];
  return true;
}

HPB_INLINE void _hpb_mapsorter_popmap(_hpb_mapsorter* s,
                                      _hpb_sortedmap* sorted) {
  s->size = sorted->start;
}

bool _hpb_mapsorter_pushmap(_hpb_mapsorter* s, hpb_FieldType key_type,
                            const hpb_Map* map, _hpb_sortedmap* sorted);

bool _hpb_mapsorter_pushexts(_hpb_mapsorter* s,
                             const hpb_Message_Extension* exts, size_t count,
                             _hpb_sortedmap* sorted);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_COLLECTIONS_INTERNAL_MAP_SORTER_H_ */
