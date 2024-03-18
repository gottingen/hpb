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

#ifndef HPB_COLLECTIONS_MAP_H_
#define HPB_COLLECTIONS_MAP_H_

#include "hpb/base/descriptor_constants.h"
#include "hpb/collections/message_value.h"
#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Creates a new map on the given arena with the given key/value size.
HPB_API hpb_Map* hpb_Map_New(hpb_Arena* a, hpb_CType key_type,
                             hpb_CType value_type);

// Returns the number of entries in the map.
HPB_API size_t hpb_Map_Size(const hpb_Map* map);

// Stores a value for the given key into |*val| (or the zero value if the key is
// not present). Returns whether the key was present. The |val| pointer may be
// NULL, in which case the function tests whether the given key is present.
HPB_API bool hpb_Map_Get(const hpb_Map* map, hpb_MessageValue key,
                         hpb_MessageValue* val);

// Removes all entries in the map.
HPB_API void hpb_Map_Clear(hpb_Map* map);

typedef enum {
  kHpb_MapInsertStatus_Inserted = 0,
  kHpb_MapInsertStatus_Replaced = 1,
  kHpb_MapInsertStatus_OutOfMemory = 2,
} hpb_MapInsertStatus;

// Sets the given key to the given value, returning whether the key was inserted
// or replaced. If the key was inserted, then any existing iterators will be
// invalidated.
HPB_API hpb_MapInsertStatus hpb_Map_Insert(hpb_Map* map, hpb_MessageValue key,
                                           hpb_MessageValue val,
                                           hpb_Arena* arena);

// Sets the given key to the given value. Returns false if memory allocation
// failed. If the key is newly inserted, then any existing iterators will be
// invalidated.
HPB_API_INLINE bool hpb_Map_Set(hpb_Map* map, hpb_MessageValue key,
                                hpb_MessageValue val, hpb_Arena* arena) {
  return hpb_Map_Insert(map, key, val, arena) !=
         kHpb_MapInsertStatus_OutOfMemory;
}

// Deletes this key from the table. Returns true if the key was present.
// If present and |val| is non-NULL, stores the deleted value.
HPB_API bool hpb_Map_Delete(hpb_Map* map, hpb_MessageValue key,
                            hpb_MessageValue* val);

// (DEPRECATED and going away soon. Do not use.)
HPB_INLINE bool hpb_Map_Delete2(hpb_Map* map, hpb_MessageValue key,
                                hpb_MessageValue* val) {
  return hpb_Map_Delete(map, key, val);
}

// Map iteration:
//
// size_t iter = kHpb_Map_Begin;
// hpb_MessageValue key, val;
// while (hpb_Map_Next(map, &key, &val, &iter)) {
//   ...
// }

#define kHpb_Map_Begin ((size_t)-1)

// Advances to the next entry. Returns false if no more entries are present.
// Otherwise returns true and populates both *key and *value.
HPB_API bool hpb_Map_Next(const hpb_Map* map, hpb_MessageValue* key,
                          hpb_MessageValue* val, size_t* iter);

// Sets the value for the entry pointed to by iter.
// WARNING: this does not currently work for string values!
HPB_API void hpb_Map_SetEntryValue(hpb_Map* map, size_t iter,
                                   hpb_MessageValue val);

// DEPRECATED iterator, slated for removal.

/* Map iteration:
 *
 * size_t iter = kHpb_Map_Begin;
 * while (hpb_MapIterator_Next(map, &iter)) {
 *   hpb_MessageValue key = hpb_MapIterator_Key(map, iter);
 *   hpb_MessageValue val = hpb_MapIterator_Value(map, iter);
 * }
 */

// Advances to the next entry. Returns false if no more entries are present.
HPB_API bool hpb_MapIterator_Next(const hpb_Map* map, size_t* iter);

// Returns true if the iterator still points to a valid entry, or false if the
// iterator is past the last element. It is an error to call this function with
// kHpb_Map_Begin (you must call next() at least once first).
HPB_API bool hpb_MapIterator_Done(const hpb_Map* map, size_t iter);

// Returns the key and value for this entry of the map.
HPB_API hpb_MessageValue hpb_MapIterator_Key(const hpb_Map* map, size_t iter);
HPB_API hpb_MessageValue hpb_MapIterator_Value(const hpb_Map* map, size_t iter);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_COLLECTIONS_MAP_H_ */
