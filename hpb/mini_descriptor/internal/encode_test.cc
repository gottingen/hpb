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

#include "hpb/mini_descriptor/internal/encode.hpp"

#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "google/protobuf/descriptor.h"
#include "hpb/base/status.hpp"
#include "hpb/mem/arena.hpp"
#include "hpb/message/internal/accessors.h"
#include "hpb/mini_descriptor/decode.h"
#include "hpb/mini_descriptor/internal/base92.h"
#include "hpb/mini_descriptor/internal/modifiers.h"
#include "hpb/mini_table/enum.h"
#include "hpb/wire/decode.h"

// begin:google_only
// #include "testing/fuzzing/fuzztest.h"
// end:google_only

namespace protobuf = ::google::protobuf;

class MiniTableTest : public testing::TestWithParam<hpb_MiniTablePlatform> {};

TEST_P(MiniTableTest, Empty) {
  hpb::Arena arena;
  hpb::Status status;
  hpb_MiniTable* table =
      _hpb_MiniTable_Build(nullptr, 0, GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(0, table->field_count);
  EXPECT_EQ(0, table->required_count);
}

TEST_P(MiniTableTest, AllScalarTypes) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;
  ASSERT_TRUE(e.StartMessage(0));
  int count = 0;
  for (int i = kHpb_FieldType_Double; i < kHpb_FieldType_SInt64; i++) {
    ASSERT_TRUE(e.PutField(static_cast<hpb_FieldType>(i), i, 0));
    count++;
  }
  hpb::Status status;
  hpb_MiniTable* table = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(count, table->field_count);
  absl::flat_hash_set<size_t> offsets;
  for (int i = 0; i < 16; i++) {
    const hpb_MiniTableField* f = &table->fields[i];
    EXPECT_EQ(i + 1, f->number);
    EXPECT_EQ(kHpb_FieldMode_Scalar, f->mode & kHpb_FieldMode_Mask);
    EXPECT_TRUE(offsets.insert(f->offset).second);
    EXPECT_TRUE(f->offset < table->size);
  }
  EXPECT_EQ(0, table->required_count);
}

TEST_P(MiniTableTest, AllRepeatedTypes) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;
  ASSERT_TRUE(e.StartMessage(0));
  int count = 0;
  for (int i = kHpb_FieldType_Double; i < kHpb_FieldType_SInt64; i++) {
    ASSERT_TRUE(e.PutField(static_cast<hpb_FieldType>(i), i,
                           kHpb_FieldModifier_IsRepeated));
    count++;
  }
  hpb::Status status;
  hpb_MiniTable* table = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(count, table->field_count);
  absl::flat_hash_set<size_t> offsets;
  for (int i = 0; i < 16; i++) {
    const hpb_MiniTableField* f = &table->fields[i];
    EXPECT_EQ(i + 1, f->number);
    EXPECT_EQ(kHpb_FieldMode_Array, f->mode & kHpb_FieldMode_Mask);
    EXPECT_TRUE(offsets.insert(f->offset).second);
    EXPECT_TRUE(f->offset < table->size);
  }
  EXPECT_EQ(0, table->required_count);
}

TEST_P(MiniTableTest, Skips) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;
  ASSERT_TRUE(e.StartMessage(0));
  int count = 0;
  std::vector<int> field_numbers;
  for (int i = 0; i < 25; i++) {
    int field_number = 1 << i;
    field_numbers.push_back(field_number);
    ASSERT_TRUE(e.PutField(kHpb_FieldType_Float, field_number, 0));
    count++;
  }
  hpb::Status status;
  hpb_MiniTable* table = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(count, table->field_count);
  absl::flat_hash_set<size_t> offsets;
  for (size_t i = 0; i < field_numbers.size(); i++) {
    const hpb_MiniTableField* f = &table->fields[i];
    EXPECT_EQ(field_numbers[i], f->number);
    EXPECT_EQ(kHpb_FieldType_Float, hpb_MiniTableField_Type(f));
    EXPECT_EQ(kHpb_FieldMode_Scalar, f->mode & kHpb_FieldMode_Mask);
    EXPECT_TRUE(offsets.insert(f->offset).second);
    EXPECT_TRUE(f->offset < table->size);
  }
  EXPECT_EQ(0, table->required_count);
}

TEST_P(MiniTableTest, AllScalarTypesOneof) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;
  ASSERT_TRUE(e.StartMessage(0));
  int count = 0;
  for (int i = kHpb_FieldType_Double; i < kHpb_FieldType_SInt64; i++) {
    ASSERT_TRUE(e.PutField(static_cast<hpb_FieldType>(i), i, 0));
    count++;
  }
  ASSERT_TRUE(e.StartOneof());
  for (int i = kHpb_FieldType_Double; i < kHpb_FieldType_SInt64; i++) {
    ASSERT_TRUE(e.PutOneofField(i));
  }
  hpb::Status status;
  hpb_MiniTable* table = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table) << status.error_message();
  EXPECT_EQ(count, table->field_count);
  absl::flat_hash_set<size_t> offsets;
  for (int i = 0; i < 16; i++) {
    const hpb_MiniTableField* f = &table->fields[i];
    EXPECT_EQ(i + 1, f->number);
    EXPECT_EQ(kHpb_FieldMode_Scalar, f->mode & kHpb_FieldMode_Mask);
    // For a oneof all fields have the same offset.
    EXPECT_EQ(table->fields[0].offset, f->offset);
    // All presence fields should point to the same oneof case offset.
    size_t case_ofs = _hpb_oneofcase_ofs(f);
    EXPECT_EQ(table->fields[0].presence, f->presence);
    EXPECT_TRUE(f->offset < table->size);
    EXPECT_TRUE(case_ofs < table->size);
    EXPECT_TRUE(case_ofs != f->offset);
  }
  EXPECT_EQ(0, table->required_count);
}

TEST_P(MiniTableTest, SizeOverflow) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;
  // upb can only handle messages up to UINT16_MAX.
  size_t max_double_fields = UINT16_MAX / (sizeof(double) + 1);

  // A bit under max_double_fields is ok.
  ASSERT_TRUE(e.StartMessage(0));
  for (size_t i = 1; i < max_double_fields; i++) {
    ASSERT_TRUE(e.PutField(kHpb_FieldType_Double, i, 0));
  }
  hpb::Status status;
  hpb_MiniTable* table = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table) << status.error_message();

  // A bit over max_double_fields fails.
  ASSERT_TRUE(e.StartMessage(0));
  for (size_t i = 1; i < max_double_fields + 2; i++) {
    ASSERT_TRUE(e.PutField(kHpb_FieldType_Double, i, 0));
  }
  hpb_MiniTable* table2 = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_EQ(nullptr, table2) << status.error_message();
}

INSTANTIATE_TEST_SUITE_P(Platforms, MiniTableTest,
                         testing::Values(kHpb_MiniTablePlatform_32Bit,
                                         kHpb_MiniTablePlatform_64Bit));

TEST(MiniTablePlatformIndependentTest, Base92Roundtrip) {
  for (char i = 0; i < 92; i++) {
    EXPECT_EQ(i, _hpb_FromBase92(_hpb_ToBase92(i)));
  }
}

TEST(MiniTablePlatformIndependentTest, IsTypePackable) {
  for (int i = 1; i <= protobuf::FieldDescriptor::MAX_TYPE; i++) {
    EXPECT_EQ(hpb_FieldType_IsPackable(static_cast<hpb_FieldType>(i)),
              protobuf::FieldDescriptor::IsTypePackable(
                  static_cast<protobuf::FieldDescriptor::Type>(i)));
  }
}

TEST(MiniTableEnumTest, Enum) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;

  ASSERT_TRUE(e.StartEnum());
  absl::flat_hash_set<int32_t> values;
  for (int i = 0; i < 256; i++) {
    values.insert(i * 2);
    e.PutEnumValue(i * 2);
  }
  e.EndEnum();

  hpb::Status status;
  hpb_MiniTableEnum* table = hpb_MiniTableEnum_Build(
      e.data().data(), e.data().size(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table) << status.error_message();

  for (int i = 0; i < UINT16_MAX; i++) {
    EXPECT_EQ(values.contains(i), hpb_MiniTableEnum_CheckValue(table, i)) << i;
  }
}

TEST_P(MiniTableTest, SubsInitializedToEmpty) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;
  // Create mini table with 2 message fields.
  ASSERT_TRUE(e.StartMessage(0));
  ASSERT_TRUE(e.PutField(kHpb_FieldType_Message, 15, 0));
  ASSERT_TRUE(e.PutField(kHpb_FieldType_Message, 16, 0));
  hpb::Status status;
  hpb_MiniTable* table = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(table->field_count, 2);
  EXPECT_EQ(table->subs[0].submsg, &_kHpb_MiniTable_Empty);
  EXPECT_EQ(table->subs[1].submsg, &_kHpb_MiniTable_Empty);
}

TEST(MiniTableEnumTest, PositiveAndNegative) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;

  ASSERT_TRUE(e.StartEnum());
  absl::flat_hash_set<int32_t> values;
  for (int i = 0; i < 100; i++) {
    values.insert(i);
    e.PutEnumValue(i);
  }
  for (int i = 100; i > 0; i--) {
    values.insert(-i);
    e.PutEnumValue(-i);
  }
  e.EndEnum();

  hpb::Status status;
  hpb_MiniTableEnum* table = hpb_MiniTableEnum_Build(
      e.data().data(), e.data().size(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table) << status.error_message();

  for (int i = -UINT16_MAX; i < UINT16_MAX; i++) {
    EXPECT_EQ(values.contains(i), hpb_MiniTableEnum_CheckValue(table, i)) << i;
  }
}

TEST_P(MiniTableTest, Extendible) {
  hpb::Arena arena;
  hpb::MtDataEncoder e;
  ASSERT_TRUE(e.StartMessage(kHpb_MessageModifier_IsExtendable));
  for (int i = kHpb_FieldType_Double; i < kHpb_FieldType_SInt64; i++) {
    ASSERT_TRUE(e.PutField(static_cast<hpb_FieldType>(i), i, 0));
  }
  hpb::Status status;
  hpb_MiniTable* table = _hpb_MiniTable_Build(
      e.data().data(), e.data().size(), GetParam(), arena.ptr(), status.ptr());
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(kHpb_ExtMode_Extendable, table->ext & kHpb_ExtMode_Extendable);
}

// begin:google_only
//
// static void BuildMiniTable(std::string_view s, bool is_32bit) {
//   hpb::Arena arena;
//   hpb::Status status;
//   _hpb_MiniTable_Build(
//       s.data(), s.size(),
//       is_32bit ? kHpb_MiniTablePlatform_32Bit : kHpb_MiniTablePlatform_64Bit,
//       arena.ptr(), status.ptr());
// }
// FUZZ_TEST(FuzzTest, BuildMiniTable);
//
// TEST(FuzzTest, BuildMiniTableRegression) {
//   BuildMiniTable("g}{v~fq{\271", false);
// }
//
// end:google_only
