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

#ifndef HPB_HASH_STR_TABLE_H_
#define HPB_HASH_STR_TABLE_H_

#include "hpb/hash/common.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct {
  hpb_table t;
} hpb_strtable;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize a table. If memory allocation failed, false is returned and
// the table is uninitialized.
bool hpb_strtable_init(hpb_strtable* table, size_t expected_size, hpb_Arena* a);

// Returns the number of values in the table.
HPB_INLINE size_t hpb_strtable_count(const hpb_strtable* t) {
  return t->t.count;
}

void hpb_strtable_clear(hpb_strtable* t);

// Inserts the given key into the hashtable with the given value.
// The key must not already exist in the hash table. The key is not required
// to be NULL-terminated, and the table will make an internal copy of the key.
//
// If a table resize was required but memory allocation failed, false is
// returned and the table is unchanged. */
bool hpb_strtable_insert(hpb_strtable* t, const char* key, size_t len,
                         hpb_value val, hpb_Arena* a);

// Looks up key in this table, returning "true" if the key was found.
// If v is non-NULL, copies the value for this key into *v.
bool hpb_strtable_lookup2(const hpb_strtable* t, const char* key, size_t len,
                          hpb_value* v);

// For NULL-terminated strings.
HPB_INLINE bool hpb_strtable_lookup(const hpb_strtable* t, const char* key,
                                    hpb_value* v) {
  return hpb_strtable_lookup2(t, key, strlen(key), v);
}

// Removes an item from the table. Returns true if the remove was successful,
// and stores the removed item in *val if non-NULL.
bool hpb_strtable_remove2(hpb_strtable* t, const char* key, size_t len,
                          hpb_value* val);

HPB_INLINE bool hpb_strtable_remove(hpb_strtable* t, const char* key,
                                    hpb_value* v) {
  return hpb_strtable_remove2(t, key, strlen(key), v);
}

// Exposed for testing only.
bool hpb_strtable_resize(hpb_strtable* t, size_t size_lg2, hpb_Arena* a);

/* Iteration over strtable:
 *
 *   intptr_t iter = HPB_STRTABLE_BEGIN;
 *   hpb_StringView key;
 *   hpb_value val;
 *   while (hpb_strtable_next2(t, &key, &val, &iter)) {
 *      // ...
 *   }
 */

#define HPB_STRTABLE_BEGIN -1

bool hpb_strtable_next2(const hpb_strtable* t, hpb_StringView* key,
                        hpb_value* val, intptr_t* iter);
void hpb_strtable_removeiter(hpb_strtable* t, intptr_t* iter);
void hpb_strtable_setentryvalue(hpb_strtable* t, intptr_t iter, hpb_value v);

/* DEPRECATED iterators, slated for removal.
 *
 * Iterators for string tables.  We are subject to some kind of unusual
 * design constraints:
 *
 * For high-level languages:
 *  - we must be able to guarantee that we don't crash or corrupt memory even if
 *    the program accesses an invalidated iterator.
 *
 * For C++11 range-based for:
 *  - iterators must be copyable
 *  - iterators must be comparable
 *  - it must be possible to construct an "end" value.
 *
 * Iteration order is undefined.
 *
 * Modifying the table invalidates iterators.  hpb_{str,int}table_done() is
 * guaranteed to work even on an invalidated iterator, as long as the table it
 * is iterating over has not been freed.  Calling next() or accessing data from
 * an invalidated iterator yields unspecified elements from the table, but it is
 * guaranteed not to crash and to return real table elements (except when done()
 * is true). */
/* hpb_strtable_iter **********************************************************/

/*   hpb_strtable_iter i;
 *   hpb_strtable_begin(&i, t);
 *   for(; !hpb_strtable_done(&i); hpb_strtable_next(&i)) {
 *     const char *key = hpb_strtable_iter_key(&i);
 *     const hpb_value val = hpb_strtable_iter_value(&i);
 *     // ...
 *   }
 */

typedef struct {
  const hpb_strtable* t;
  size_t index;
} hpb_strtable_iter;

HPB_INLINE const hpb_tabent* str_tabent(const hpb_strtable_iter* i) {
  return &i->t->t.entries[i->index];
}

void hpb_strtable_begin(hpb_strtable_iter* i, const hpb_strtable* t);
void hpb_strtable_next(hpb_strtable_iter* i);
bool hpb_strtable_done(const hpb_strtable_iter* i);
hpb_StringView hpb_strtable_iter_key(const hpb_strtable_iter* i);
hpb_value hpb_strtable_iter_value(const hpb_strtable_iter* i);
void hpb_strtable_iter_setdone(hpb_strtable_iter* i);
bool hpb_strtable_iter_isequal(const hpb_strtable_iter* i1,
                               const hpb_strtable_iter* i2);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_HASH_STR_TABLE_H_ */
