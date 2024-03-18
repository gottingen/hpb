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

// Users should include array.h or map.h instead.
// IWYU pragma: private, include "hpb/collections/array.h"

#ifndef HPB_MESSAGE_VALUE_H_
#define HPB_MESSAGE_VALUE_H_

#include "hpb/base/string_view.h"
#include "hpb/message/tagged_ptr.h"
#include "hpb/mini_table/message.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct hpb_Array hpb_Array;
typedef struct hpb_Map hpb_Map;

typedef union {
  bool bool_val;
  float float_val;
  double double_val;
  int32_t int32_val;
  int64_t int64_val;
  uint32_t uint32_val;
  uint64_t uint64_val;
  const hpb_Array* array_val;
  const hpb_Map* map_val;
  const hpb_Message* msg_val;
  hpb_StringView str_val;

  // EXPERIMENTAL: A tagged hpb_Message*.  Users must use this instead of
  // msg_val if unlinked sub-messages may possibly be in use.  See the
  // documentation in kHpb_DecodeOption_ExperimentalAllowUnlinked for more
  // information.
  hpb_TaggedMessagePtr tagged_msg_val;
} hpb_MessageValue;

typedef union {
  hpb_Array* array;
  hpb_Map* map;
  hpb_Message* msg;
} hpb_MutableMessageValue;

#include "hpb/port/undef.inc"

#endif /* HPB_MESSAGE_VALUE_H_ */
