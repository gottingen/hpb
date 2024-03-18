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

#ifndef HPB_PORT_ATOMIC_H_
#define HPB_PORT_ATOMIC_H_

#include "hpb/port/def.inc"

#ifdef HPB_USE_C11_ATOMICS

#include <stdatomic.h>
#include <stdbool.h>

#define hpb_Atomic_Init(addr, val) atomic_init(addr, val)
#define hpb_Atomic_Load(addr, order) atomic_load_explicit(addr, order)
#define hpb_Atomic_Store(addr, val, order) \
  atomic_store_explicit(addr, val, order)
#define hpb_Atomic_Add(addr, val, order) \
  atomic_fetch_add_explicit(addr, val, order)
#define hpb_Atomic_Sub(addr, val, order) \
  atomic_fetch_sub_explicit(addr, val, order)
#define hpb_Atomic_Exchange(addr, val, order) \
  atomic_exchange_explicit(addr, val, order)
#define hpb_Atomic_CompareExchangeStrong(addr, expected, desired,      \
                                         success_order, failure_order) \
  atomic_compare_exchange_strong_explicit(addr, expected, desired,     \
                                          success_order, failure_order)
#define hpb_Atomic_CompareExchangeWeak(addr, expected, desired, success_order, \
                                       failure_order)                          \
  atomic_compare_exchange_weak_explicit(addr, expected, desired,               \
                                        success_order, failure_order)

#else  // !HPB_USE_C11_ATOMICS

#include <string.h>

#define hpb_Atomic_Init(addr, val) (*addr = val)
#define hpb_Atomic_Load(addr, order) (*addr)
#define hpb_Atomic_Store(addr, val, order) (*(addr) = val)
#define hpb_Atomic_Add(addr, val, order) (*(addr) += val)
#define hpb_Atomic_Sub(addr, val, order) (*(addr) -= val)

HPB_INLINE void* _hpb_NonAtomic_Exchange(void* addr, void* value) {
  void* old;
  memcpy(&old, addr, sizeof(value));
  memcpy(addr, &value, sizeof(value));
  return old;
}

#define hpb_Atomic_Exchange(addr, val, order) _hpb_NonAtomic_Exchange(addr, val)

// `addr` and `expected` are logically double pointers.
HPB_INLINE bool _hpb_NonAtomic_CompareExchangeStrongP(void* addr,
                                                      void* expected,
                                                      void* desired) {
  if (memcmp(addr, expected, sizeof(desired)) == 0) {
    memcpy(addr, &desired, sizeof(desired));
    return true;
  } else {
    memcpy(expected, addr, sizeof(desired));
    return false;
  }
}

#define hpb_Atomic_CompareExchangeStrong(addr, expected, desired,      \
                                         success_order, failure_order) \
  _hpb_NonAtomic_CompareExchangeStrongP((void*)addr, (void*)expected,  \
                                        (void*)desired)
#define hpb_Atomic_CompareExchangeWeak(addr, expected, desired, success_order, \
                                       failure_order)                          \
  hpb_Atomic_CompareExchangeStrong(addr, expected, desired, 0, 0)

#endif

#include "hpb/port/undef.inc"

#endif  // HPB_PORT_ATOMIC_H_
