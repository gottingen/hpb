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

/* Test of generated code, with a special focus on features that are not used in
 * descriptor.proto or conformance.proto (since these get some testing from
 * upb/def.c and tests/conformance_hpb.c, respectively).
 */

#include <cstddef>
#include <cstdint>

#include "gtest/gtest.h"
#include "google/protobuf/test_messages_proto2.hpb.h"
#include "google/protobuf/test_messages_proto3.hpb.h"
#include "hpb/base/status.h"
#include "hpb/base/string_view.h"
#include "hpb/collections/array.h"
#include "hpb/mem/arena.hpp"
#include "hpb/test/test.hpb.h"

// Must be last.
#include "hpb/port/def.inc"

#if !defined(MIN)
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

const char test_str[] = "abcdefg";
const char test_str2[] = "12345678910";
const char test_str3[] = "rstlnezxcvbnm";
const char test_str4[] = "just another test string";

const hpb_StringView test_str_view = {test_str, sizeof(test_str) - 1};
const hpb_StringView test_str_view2 = {test_str2, sizeof(test_str2) - 1};
const hpb_StringView test_str_view3 = {test_str3, sizeof(test_str3) - 1};
const hpb_StringView test_str_view4 = {test_str4, sizeof(test_str4) - 1};

const int32_t test_int32 = 10;
const int32_t test_int32_2 = -20;
const int32_t test_int32_3 = 30;
const int32_t test_int32_4 = -40;

TEST(GeneratedCode, ScalarsProto3) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto3_TestAllTypesProto3* msg =
      protobuf_test_messages_proto3_TestAllTypesProto3_new(arena);
  protobuf_test_messages_proto3_TestAllTypesProto3* msg2;
  hpb_StringView serialized;
  hpb_StringView val;

  // Test serialization.
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_int32(msg, 10);
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_int64(msg, 20);
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_uint32(msg, 30);
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_uint64(msg, 40);
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_float(msg,
                                                                      50.5);
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_double(msg,
                                                                       60.6);
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_bool(msg, true);
  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_string(
      msg, test_str_view);

  serialized.data = protobuf_test_messages_proto3_TestAllTypesProto3_serialize(
      msg, arena, &serialized.size);

  msg2 = protobuf_test_messages_proto3_TestAllTypesProto3_parse(
      serialized.data, serialized.size, arena);

  EXPECT_EQ(10, protobuf_test_messages_proto3_TestAllTypesProto3_optional_int32(
                    msg2));
  EXPECT_EQ(20, protobuf_test_messages_proto3_TestAllTypesProto3_optional_int64(
                    msg2));
  EXPECT_EQ(
      30,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_uint32(msg2));
  EXPECT_EQ(
      40,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_uint64(msg2));
  EXPECT_EQ(
      50.5,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_float(msg2));
  EXPECT_EQ(
      60.6,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_double(msg2));
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_bool(msg2));
  val = protobuf_test_messages_proto3_TestAllTypesProto3_optional_string(msg2);
  EXPECT_TRUE(hpb_StringView_IsEqual(val, test_str_view));

  // Test clear.
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_int32(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto3_TestAllTypesProto3_optional_int32(msg));
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_int64(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto3_TestAllTypesProto3_optional_int64(msg));
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_uint32(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto3_TestAllTypesProto3_optional_uint32(msg));
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_uint64(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto3_TestAllTypesProto3_optional_uint64(msg));
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_float(msg);
  EXPECT_EQ(
      0.0f,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_float(msg));
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_double(msg);
  EXPECT_EQ(
      0.0,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_double(msg));
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_bool(msg);
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto3_TestAllTypesProto3_optional_bool(msg));
  protobuf_test_messages_proto3_TestAllTypesProto3_clear_optional_string(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto3_TestAllTypesProto3_optional_string(msg)
             .size);
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, ScalarsProto2) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto2_TestAllTypesProto2* msg =
      protobuf_test_messages_proto2_TestAllTypesProto2_new(arena);
  protobuf_test_messages_proto2_TestAllTypesProto2* msg2;
  hpb_StringView serialized;

  // Test hazzer and serialization.
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int32(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_int32(msg, 10);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int32(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int64(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_int64(msg, 20);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int64(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_uint32(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_uint32(msg, 30);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_uint32(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_uint64(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_uint64(msg, 40);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_uint64(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sint32(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_sint32(msg, 50);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sint32(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sint64(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_sint64(msg, 60);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sint64(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_fixed32(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_fixed32(msg,
                                                                        70);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_fixed32(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_fixed64(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_fixed64(msg,
                                                                        80);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_fixed64(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sfixed32(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_sfixed32(msg,
                                                                         90);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sfixed32(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sfixed64(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_sfixed64(msg,
                                                                         100);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_sfixed64(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_float(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_float(msg,
                                                                      50.5);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_float(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_double(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_double(msg,
                                                                       60.6);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_double(
          msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_bool(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_bool(msg, true);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_bool(msg));

  serialized.data = protobuf_test_messages_proto2_TestAllTypesProto2_serialize(
      msg, arena, &serialized.size);

  msg2 = protobuf_test_messages_proto2_TestAllTypesProto2_parse(
      serialized.data, serialized.size, arena);

  EXPECT_EQ(10, protobuf_test_messages_proto2_TestAllTypesProto2_optional_int32(
                    msg2));
  EXPECT_EQ(20, protobuf_test_messages_proto2_TestAllTypesProto2_optional_int64(
                    msg2));
  EXPECT_EQ(
      30,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_uint32(msg2));
  EXPECT_EQ(
      40,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_uint64(msg2));
  EXPECT_EQ(
      50,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_sint32(msg2));
  EXPECT_EQ(
      60,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_sint64(msg2));
  EXPECT_EQ(
      70,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_fixed32(msg2));
  EXPECT_EQ(
      80,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_fixed64(msg2));
  EXPECT_EQ(
      90,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_sfixed32(msg2));
  EXPECT_EQ(
      100,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_sfixed64(msg2));
  EXPECT_EQ(
      50.5,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_float(msg2));
  EXPECT_EQ(
      60.6,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_double(msg2));
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_bool(msg2));

  // Test clear.
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_int32(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_int32(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int32(msg));

  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_int64(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_int64(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int64(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_uint32(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_uint32(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_uint32(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_uint64(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_uint64(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_uint64(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_float(msg);
  EXPECT_EQ(
      0.0f,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_float(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_float(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_double(msg);
  EXPECT_EQ(
      0.0,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_double(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_double(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_bool(msg);
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_bool(msg));
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_bool(msg));

  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, RepeatedClear) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto2_TestAllTypesProto2* msg =
      protobuf_test_messages_proto2_TestAllTypesProto2_new(arena);
  size_t len = 0;
  protobuf_test_messages_proto2_TestAllTypesProto2_repeated_int32(msg, &len);
  EXPECT_EQ(0, len);
  protobuf_test_messages_proto2_TestAllTypesProto2_add_repeated_int32(msg, 2,
                                                                      arena);
  protobuf_test_messages_proto2_TestAllTypesProto2_add_repeated_int32(msg, 3,
                                                                      arena);
  protobuf_test_messages_proto2_TestAllTypesProto2_add_repeated_int32(msg, 4,
                                                                      arena);
  protobuf_test_messages_proto2_TestAllTypesProto2_repeated_int32(msg, &len);
  EXPECT_EQ(3, len);
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_repeated_int32(msg);
  protobuf_test_messages_proto2_TestAllTypesProto2_repeated_int32(msg, &len);
  EXPECT_EQ(0, len);
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, Clear) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto2_TestAllTypesProto2* msg =
      protobuf_test_messages_proto2_TestAllTypesProto2_new(arena);
  // Test clear.
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_int32(msg, 1);
  EXPECT_TRUE(
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int32(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_int32(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_int32(msg));
  EXPECT_FALSE(
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_int32(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_int64(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_int64(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_uint32(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_uint32(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_uint64(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_uint64(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_float(msg);
  EXPECT_EQ(
      0.0f,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_float(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_double(msg);
  EXPECT_EQ(
      0.0,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_double(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_bool(msg);
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_optional_bool(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_string(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_string(msg)
             .size);
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, Bytes) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto2_TestAllTypesProto2* msg =
      protobuf_test_messages_proto2_TestAllTypesProto2_new(arena);
  protobuf_test_messages_proto2_TestAllTypesProto2* msg2;
  hpb_StringView serialized;
  const char data[] = "ABCDEF";
  hpb_StringView bytes = hpb_StringView_FromString(data);
  hpb_StringView val;

  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_bytes(msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_bytes(msg,
                                                                      bytes);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_bytes(msg));

  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_string(
          msg));
  protobuf_test_messages_proto2_TestAllTypesProto2_set_optional_string(
      msg, test_str_view);
  EXPECT_EQ(
      true,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_string(
          msg));

  serialized.data = protobuf_test_messages_proto2_TestAllTypesProto2_serialize(
      msg, arena, &serialized.size);

  msg2 = protobuf_test_messages_proto2_TestAllTypesProto2_parse(
      serialized.data, serialized.size, arena);

  EXPECT_EQ(bytes.size,
            protobuf_test_messages_proto2_TestAllTypesProto2_optional_bytes(msg)
                .size);
  EXPECT_EQ(
      0, memcmp(bytes.data,
                protobuf_test_messages_proto2_TestAllTypesProto2_optional_bytes(
                    msg)
                    .data,
                bytes.size));
  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_bytes(msg);
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_bytes(msg));

  val = protobuf_test_messages_proto2_TestAllTypesProto2_optional_string(msg2);
  EXPECT_TRUE(hpb_StringView_IsEqual(val, test_str_view));

  protobuf_test_messages_proto2_TestAllTypesProto2_clear_optional_string(msg);
  EXPECT_EQ(
      0, protobuf_test_messages_proto2_TestAllTypesProto2_optional_string(msg)
             .size);
  EXPECT_EQ(
      false,
      protobuf_test_messages_proto2_TestAllTypesProto2_has_optional_string(
          msg));
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, UTF8) {
  const char invalid_utf8[] = "\xff";
  const hpb_StringView invalid_utf8_view =
      hpb_StringView_FromDataAndSize(invalid_utf8, 1);
  hpb_Arena* arena = hpb_Arena_New();
  hpb_StringView serialized;
  protobuf_test_messages_proto3_TestAllTypesProto3* msg =
      protobuf_test_messages_proto3_TestAllTypesProto3_new(arena);
  protobuf_test_messages_proto3_TestAllTypesProto3* msg2;

  protobuf_test_messages_proto3_TestAllTypesProto3_set_optional_string(
      msg, invalid_utf8_view);

  serialized.data = protobuf_test_messages_proto3_TestAllTypesProto3_serialize(
      msg, arena, &serialized.size);

  msg2 = protobuf_test_messages_proto3_TestAllTypesProto3_parse(
      serialized.data, serialized.size, arena);
  EXPECT_EQ(nullptr, msg2);

  hpb_Arena_Free(arena);
}

static void check_string_map_empty(
    protobuf_test_messages_proto3_TestAllTypesProto3* msg) {
  size_t iter = kHpb_Map_Begin;

  EXPECT_EQ(
      0,
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_size(
          msg));
  EXPECT_FALSE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_next(
          msg, &iter));
}

static void check_string_map_one_entry(
    protobuf_test_messages_proto3_TestAllTypesProto3* msg) {
  const protobuf_test_messages_proto3_TestAllTypesProto3_MapStringStringEntry*
      const_ent;
  size_t iter;
  hpb_StringView str;

  EXPECT_EQ(
      1,
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_size(
          msg));
  EXPECT_TRUE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_get(
          msg, test_str_view, &str));
  EXPECT_TRUE(hpb_StringView_IsEqual(str, test_str_view2));

  EXPECT_FALSE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_get(
          msg, test_str_view3, &str));

  /* Test that iteration reveals a single k/v pair in the map. */
  iter = kHpb_Map_Begin;
  const_ent =
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_next(
          msg, &iter);
  ASSERT_NE(nullptr, const_ent);
  EXPECT_TRUE(hpb_StringView_IsEqual(
      test_str_view,
      protobuf_test_messages_proto3_TestAllTypesProto3_MapStringStringEntry_key(
          const_ent)));
  EXPECT_TRUE(hpb_StringView_IsEqual(
      test_str_view2,
      protobuf_test_messages_proto3_TestAllTypesProto3_MapStringStringEntry_value(
          const_ent)));

  const_ent =
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_next(
          msg, &iter);
  EXPECT_EQ(nullptr, const_ent);
}

TEST(GeneratedCode, StringDoubleMap) {
  hpb_Arena* arena = hpb_Arena_New();
  hpb_StringView serialized;
  hpb_test_MapTest* msg = hpb_test_MapTest_new(arena);
  hpb_test_MapTest* msg2;
  double val;

  hpb_test_MapTest_map_string_double_set(msg, test_str_view, 1.5, arena);
  ASSERT_NE(nullptr, msg);
  EXPECT_TRUE(hpb_test_MapTest_map_string_double_get(msg, test_str_view, &val));
  EXPECT_EQ(1.5, val);
  val = 0;

  serialized.data = hpb_test_MapTest_serialize(msg, arena, &serialized.size);
  EXPECT_NE(nullptr, serialized.data);

  msg2 = hpb_test_MapTest_parse(serialized.data, serialized.size, arena);
  ASSERT_NE(nullptr, msg2);
  EXPECT_TRUE(
      hpb_test_MapTest_map_string_double_get(msg2, test_str_view, &val));
  EXPECT_EQ(1.5, val);

  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, StringMap) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto3_TestAllTypesProto3* msg =
      protobuf_test_messages_proto3_TestAllTypesProto3_new(arena);
  const protobuf_test_messages_proto3_TestAllTypesProto3_MapStringStringEntry*
      const_ent;
  size_t iter, count;

  check_string_map_empty(msg);

  /* Set map[test_str_view] = test_str_view2 */
  protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_set(
      msg, test_str_view, test_str_view2, arena);
  check_string_map_one_entry(msg);

  /* Deleting a non-existent key does nothing. */
  EXPECT_FALSE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_delete(
          msg, test_str_view3));
  check_string_map_one_entry(msg);

  /* Deleting the key sets the map back to empty. */
  EXPECT_TRUE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_delete(
          msg, test_str_view));
  check_string_map_empty(msg);

  /* Set two keys this time:
   *   map[test_str_view] = test_str_view2
   *   map[test_str_view3] = test_str_view4
   */
  protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_set(
      msg, test_str_view, test_str_view2, arena);
  protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_set(
      msg, test_str_view3, test_str_view4, arena);

  /* Test iteration */
  iter = kHpb_Map_Begin;
  count = 0;

  while (
      (const_ent =
           protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_next(
               msg, &iter)) != nullptr) {
    hpb_StringView key =
        protobuf_test_messages_proto3_TestAllTypesProto3_MapStringStringEntry_key(
            const_ent);
    hpb_StringView val =
        protobuf_test_messages_proto3_TestAllTypesProto3_MapStringStringEntry_value(
            const_ent);

    count++;
    if (hpb_StringView_IsEqual(key, test_str_view)) {
      EXPECT_TRUE(hpb_StringView_IsEqual(val, test_str_view2));
    } else {
      EXPECT_TRUE(hpb_StringView_IsEqual(key, test_str_view3));
      EXPECT_TRUE(hpb_StringView_IsEqual(val, test_str_view4));
    }
  }

  EXPECT_EQ(2, count);

  /* Clearing the map goes back to empty. */
  protobuf_test_messages_proto3_TestAllTypesProto3_map_string_string_clear(msg);
  check_string_map_empty(msg);

  hpb_Arena_Free(arena);
}

static void check_int32_map_empty(
    protobuf_test_messages_proto3_TestAllTypesProto3* msg) {
  size_t iter = kHpb_Map_Begin;

  EXPECT_EQ(
      0, protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_size(
             msg));
  EXPECT_FALSE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_next(
          msg, &iter));
}

static void check_int32_map_one_entry(
    protobuf_test_messages_proto3_TestAllTypesProto3* msg) {
  const protobuf_test_messages_proto3_TestAllTypesProto3_MapInt32Int32Entry*
      const_ent;
  size_t iter;
  int32_t val;

  EXPECT_EQ(
      1, protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_size(
             msg));
  EXPECT_TRUE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_get(
          msg, test_int32, &val));
  EXPECT_EQ(val, test_int32_2);

  EXPECT_FALSE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_get(
          msg, test_int32_3, &val));

  /* Test that iteration reveals a single k/v pair in the map. */
  iter = kHpb_Map_Begin;
  const_ent =
      protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_next(
          msg, &iter);
  ASSERT_NE(nullptr, const_ent);
  EXPECT_EQ(
      test_int32,
      protobuf_test_messages_proto3_TestAllTypesProto3_MapInt32Int32Entry_key(
          const_ent));
  EXPECT_EQ(
      test_int32_2,
      protobuf_test_messages_proto3_TestAllTypesProto3_MapInt32Int32Entry_value(
          const_ent));

  const_ent =
      protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_next(
          msg, &iter);
  EXPECT_EQ(nullptr, const_ent);
}

TEST(GeneratedCode, Int32Map) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto3_TestAllTypesProto3* msg =
      protobuf_test_messages_proto3_TestAllTypesProto3_new(arena);
  const protobuf_test_messages_proto3_TestAllTypesProto3_MapInt32Int32Entry*
      const_ent;
  size_t iter, count;

  check_int32_map_empty(msg);

  /* Set map[test_int32] = test_int32_2 */
  protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_set(
      msg, test_int32, test_int32_2, arena);
  check_int32_map_one_entry(msg);

  /* Deleting a non-existent key does nothing. */
  EXPECT_FALSE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_delete(
          msg, test_int32_3));
  check_int32_map_one_entry(msg);

  /* Deleting the key sets the map back to empty. */
  EXPECT_TRUE(
      protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_delete(
          msg, test_int32));
  check_int32_map_empty(msg);

  /* Set two keys this time:
   *   map[test_int32] = test_int32_2
   *   map[test_int32_3] = test_int32_4
   */
  protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_set(
      msg, test_int32, test_int32_2, arena);
  protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_set(
      msg, test_int32_3, test_int32_4, arena);

  /* Test iteration */
  iter = kHpb_Map_Begin;
  count = 0;

  while (
      (const_ent =
           protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_next(
               msg, &iter)) != nullptr) {
    int32_t key =
        protobuf_test_messages_proto3_TestAllTypesProto3_MapInt32Int32Entry_key(
            const_ent);
    int32_t val =
        protobuf_test_messages_proto3_TestAllTypesProto3_MapInt32Int32Entry_value(
            const_ent);

    count++;
    if (key == test_int32) {
      EXPECT_EQ(val, test_int32_2);
    } else {
      EXPECT_EQ(key, test_int32_3);
      EXPECT_EQ(val, test_int32_4);
    }
  }

  EXPECT_EQ(2, count);

  /* Clearing the map goes back to empty. */
  protobuf_test_messages_proto3_TestAllTypesProto3_map_int32_int32_clear(msg);
  check_int32_map_empty(msg);

  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, TestRepeated) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto3_TestAllTypesProto3* msg =
      protobuf_test_messages_proto3_TestAllTypesProto3_new(arena);
  size_t size;
  const int* elems;

  EXPECT_EQ(
      _protobuf_test_messages_proto3_TestAllTypesProto3_repeated_int32_upb_array(
          msg, &size),
      nullptr);

  protobuf_test_messages_proto3_TestAllTypesProto3_add_repeated_int32(msg, 5,
                                                                      arena);

  EXPECT_NE(
      _protobuf_test_messages_proto3_TestAllTypesProto3_repeated_int32_upb_array(
          msg, &size),
      nullptr);

  elems = protobuf_test_messages_proto3_TestAllTypesProto3_repeated_int32(
      msg, &size);

  EXPECT_EQ(size, 1);
  EXPECT_EQ(elems[0], 5);

  const hpb_Array* arr =
      _protobuf_test_messages_proto3_TestAllTypesProto3_repeated_int32_upb_array(
          msg, &size);
  EXPECT_EQ(size, 1);
  hpb_Array* mutable_arr =
      _protobuf_test_messages_proto3_TestAllTypesProto3_repeated_int32_mutable_upb_array(
          msg, &size, arena);
  EXPECT_EQ(mutable_arr, arr);
  EXPECT_EQ(hpb_Array_Size(arr), 1);
  EXPECT_EQ(size, 1);

  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, Issue9440) {
  hpb::Arena arena;
  hpb_test_HelloRequest* msg = hpb_test_HelloRequest_new(arena.ptr());
  hpb_test_HelloRequest_set_id(msg, 8);
  EXPECT_EQ(8, hpb_test_HelloRequest_id(msg));
  char str[] = "1";
  hpb_test_HelloRequest_set_version(msg, hpb_StringView{str, strlen(str)});
  EXPECT_EQ(8, hpb_test_HelloRequest_id(msg));
}

TEST(GeneratedCode, NullDecodeBuffer) {
  hpb_Arena* arena = hpb_Arena_New();
  protobuf_test_messages_proto3_TestAllTypesProto3* msg =
      protobuf_test_messages_proto3_TestAllTypesProto3_parse(nullptr, 0, arena);
  size_t size;

  ASSERT_NE(nullptr, msg);
  protobuf_test_messages_proto3_TestAllTypesProto3_serialize(msg, arena, &size);
  EXPECT_EQ(0, size);
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, StatusTruncation) {
  int i, j;
  hpb_Status status;
  hpb_Status status2;
  for (i = 0; i < _kHpb_Status_MaxMessage + 20; i++) {
    char* msg = static_cast<char*>(malloc(i + 1));
    int end;
    char ch = (i % 96) + 33; /* Cycle through printable chars. */

    for (j = 0; j < i; j++) {
      msg[j] = ch;
    }
    msg[i] = '\0';

    hpb_Status_SetErrorMessage(&status, msg);
    hpb_Status_SetErrorFormat(&status2, "%s", msg);
    end = MIN(i, _kHpb_Status_MaxMessage - 1);
    EXPECT_EQ(end, strlen(status.msg));
    EXPECT_EQ(end, strlen(status2.msg));

    for (j = 0; j < end; j++) {
      EXPECT_EQ(ch, status.msg[j]);
      EXPECT_EQ(ch, status2.msg[j]);
    }

    free(msg);
  }
}

TEST(GeneratedCode, ArenaUnaligned) {
  char buf1[1024];
  // Force the pointer to be unaligned.
  uintptr_t low_bits = HPB_MALLOC_ALIGN - 1;
  char* unaligned_buf_ptr = (char*)((uintptr_t)buf1 | low_bits);
  hpb_Arena* arena = hpb_Arena_Init(
      unaligned_buf_ptr, &buf1[sizeof(buf1)] - unaligned_buf_ptr, nullptr);
  char* mem = static_cast<char*>(hpb_Arena_Malloc(arena, 5));
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(mem) & low_bits);
  hpb_Arena_Free(arena);

  // Try the same, but with a size so small that aligning up will overflow.
  arena = hpb_Arena_Init(unaligned_buf_ptr, 5, &hpb_alloc_global);
  mem = static_cast<char*>(hpb_Arena_Malloc(arena, 5));
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(mem) & low_bits);
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, Extensions) {
  hpb::Arena arena;
  hpb_test_ModelExtension1* extension1 =
      hpb_test_ModelExtension1_new(arena.ptr());
  hpb_test_ModelExtension1_set_str(extension1,
                                   hpb_StringView_FromString("Hello"));

  hpb_test_ModelExtension2* extension2 =
      hpb_test_ModelExtension2_new(arena.ptr());
  hpb_test_ModelExtension2_set_i(extension2, 5);

  hpb_test_ModelWithExtensions* msg1 =
      hpb_test_ModelWithExtensions_new(arena.ptr());
  hpb_test_ModelWithExtensions* msg2 =
      hpb_test_ModelWithExtensions_new(arena.ptr());

  // msg1: [extension1, extension2]
  hpb_test_ModelExtension1_set_model_ext(msg1, extension1, arena.ptr());
  hpb_test_ModelExtension2_set_model_ext(msg1, extension2, arena.ptr());

  // msg2: [extension2, extension1]
  hpb_test_ModelExtension2_set_model_ext(msg2, extension2, arena.ptr());
  hpb_test_ModelExtension1_set_model_ext(msg2, extension1, arena.ptr());

  size_t size1, size2;
  int opts = kHpb_EncodeOption_Deterministic;
  char* pb1 = hpb_test_ModelWithExtensions_serialize_ex(msg1, opts, arena.ptr(),
                                                        &size1);
  char* pb2 = hpb_test_ModelWithExtensions_serialize_ex(msg2, opts, arena.ptr(),
                                                        &size2);

  ASSERT_EQ(size1, size2);
  ASSERT_EQ(0, memcmp(pb1, pb2, size1));
}
