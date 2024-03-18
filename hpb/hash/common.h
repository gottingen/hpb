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

/*
 * hpb_table
 *
 * This header is INTERNAL-ONLY!  Its interfaces are not public or stable!
 * This file defines very fast int->hpb_value (inttable) and string->hpb_value
 * (strtable) hash tables.
 *
 * The table uses chained scatter with Brent's variation (inspired by the Lua
 * implementation of hash tables).  The hash function for strings is Austin
 * Appleby's "MurmurHash."
 *
 * The inttable uses uintptr_t as its key, which guarantees it can be used to
 * store pointers or integers of at least 32 bits (hpb isn't really useful on
 * systems where sizeof(void*) < 4).
 *
 * The table must be homogeneous (all values of the same type).  In debug
 * mode, we check this on insert and lookup.
 */

#ifndef HPB_HASH_COMMON_H_
#define HPB_HASH_COMMON_H_

#include <string.h>

#include "hpb/base/string_view.h"
#include "hpb/mem/arena.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

/* hpb_value ******************************************************************/

typedef struct {
  uint64_t val;
} hpb_value;

/* Variant that works with a length-delimited rather than NULL-delimited string,
 * as supported by strtable. */
char* hpb_strdup2(const char* s, size_t len, hpb_Arena* a);

HPB_INLINE void _hpb_value_setval(hpb_value* v, uint64_t val) { v->val = val; }

/* For each value ctype, define the following set of functions:
 *
 * // Get/set an int32 from a hpb_value.
 * int32_t pb_value_getint32(hpb_value val);
 * void hpb_value_setint32(hpb_value *val, int32_t cval);
 *
 * // Construct a new hpb_value from an int32.
 * hpb_value hpb_value_int32(int32_t val); */
#define FUNCS(name, membername, type_t, converter, proto_type)       \
  HPB_INLINE void hpb_value_set##name(hpb_value* val, type_t cval) { \
    val->val = (converter)cval;                                      \
  }                                                                  \
  HPB_INLINE hpb_value hpb_value_##name(type_t val) {                \
    hpb_value ret;                                                   \
    hpb_value_set##name(&ret, val);                                  \
    return ret;                                                      \
  }                                                                  \
  HPB_INLINE type_t hpb_value_get##name(hpb_value val) {             \
    return (type_t)(converter)val.val;                               \
  }

FUNCS(int32, int32, int32_t, int32_t, HPB_CTYPE_INT32)
FUNCS(int64, int64, int64_t, int64_t, HPB_CTYPE_INT64)
FUNCS(uint32, uint32, uint32_t, uint32_t, HPB_CTYPE_UINT32)
FUNCS(uint64, uint64, uint64_t, uint64_t, HPB_CTYPE_UINT64)
FUNCS(bool, _bool, bool, bool, HPB_CTYPE_BOOL)
FUNCS(cstr, cstr, char*, uintptr_t, HPB_CTYPE_CSTR)
FUNCS(uintptr, uptr, uintptr_t, uintptr_t, HPB_CTYPE_UPTR)
FUNCS(ptr, ptr, void*, uintptr_t, HPB_CTYPE_PTR)
FUNCS(constptr, constptr, const void*, uintptr_t, HPB_CTYPE_CONSTPTR)

#undef FUNCS

HPB_INLINE void hpb_value_setfloat(hpb_value* val, float cval) {
  memcpy(&val->val, &cval, sizeof(cval));
}

HPB_INLINE void hpb_value_setdouble(hpb_value* val, double cval) {
  memcpy(&val->val, &cval, sizeof(cval));
}

HPB_INLINE hpb_value hpb_value_float(float cval) {
  hpb_value ret;
  hpb_value_setfloat(&ret, cval);
  return ret;
}

HPB_INLINE hpb_value hpb_value_double(double cval) {
  hpb_value ret;
  hpb_value_setdouble(&ret, cval);
  return ret;
}

#undef SET_TYPE

/* hpb_tabkey *****************************************************************/

/* Either:
 *   1. an actual integer key, or
 *   2. a pointer to a string prefixed by its uint32_t length, owned by us.
 *
 * ...depending on whether this is a string table or an int table.  We would
 * make this a union of those two types, but C89 doesn't support statically
 * initializing a non-first union member. */
typedef uintptr_t hpb_tabkey;

HPB_INLINE char* hpb_tabstr(hpb_tabkey key, uint32_t* len) {
  char* mem = (char*)key;
  if (len) memcpy(len, mem, sizeof(*len));
  return mem + sizeof(*len);
}

HPB_INLINE hpb_StringView hpb_tabstrview(hpb_tabkey key) {
  hpb_StringView ret;
  uint32_t len;
  ret.data = hpb_tabstr(key, &len);
  ret.size = len;
  return ret;
}

/* hpb_tabval *****************************************************************/

typedef struct hpb_tabval {
  uint64_t val;
} hpb_tabval;

#define HPB_TABVALUE_EMPTY_INIT \
  { -1 }

/* hpb_table ******************************************************************/

typedef struct _hpb_tabent {
  hpb_tabkey key;
  hpb_tabval val;

  /* Internal chaining.  This is const so we can create static initializers for
   * tables.  We cast away const sometimes, but *only* when the containing
   * hpb_table is known to be non-const.  This requires a bit of care, but
   * the subtlety is confined to table.c. */
  const struct _hpb_tabent* next;
} hpb_tabent;

typedef struct {
  size_t count;       /* Number of entries in the hash part. */
  uint32_t mask;      /* Mask to turn hash value -> bucket. */
  uint32_t max_count; /* Max count before we hit our load limit. */
  uint8_t size_lg2;   /* Size of the hashtable part is 2^size_lg2 entries. */
  hpb_tabent* entries;
} hpb_table;

HPB_INLINE size_t hpb_table_size(const hpb_table* t) {
  return t->size_lg2 ? 1 << t->size_lg2 : 0;
}

// Internal-only functions, in .h file only out of necessity.

HPB_INLINE bool hpb_tabent_isempty(const hpb_tabent* e) { return e->key == 0; }

uint32_t _hpb_Hash(const void* p, size_t n, uint64_t seed);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_HASH_COMMON_H_ */
