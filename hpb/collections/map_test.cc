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

#include "gtest/gtest.h"
#include "hpb/base/string_view.h"
#include "hpb/mem/arena.hpp"

TEST(MapTest, DeleteRegression) {
  hpb::Arena arena;
  hpb_Map* map = hpb_Map_New(arena.ptr(), kHpb_CType_Int32, kHpb_CType_String);

  hpb_MessageValue key;
  key.int32_val = 0;

  hpb_MessageValue insert_value;
  insert_value.str_val = hpb_StringView_FromString("abcde");

  hpb_MapInsertStatus st = hpb_Map_Insert(map, key, insert_value, arena.ptr());
  EXPECT_EQ(kHpb_MapInsertStatus_Inserted, st);

  hpb_MessageValue delete_value;
  bool removed = hpb_Map_Delete(map, key, &delete_value);
  EXPECT_TRUE(removed);

  EXPECT_TRUE(
      hpb_StringView_IsEqual(insert_value.str_val, delete_value.str_val));
}
