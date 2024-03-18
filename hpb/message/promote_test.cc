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

/* Test of mini table accessors.
 *
 * Messages are created and mutated using generated code, and then
 * accessed through reflective APIs exposed through mini table accessors.
 */

#include "hpb/message/promote.h"

#include <string>

#include "gtest/gtest.h"
#include "google/protobuf/test_messages_proto2.hpb.h"
#include "google/protobuf/test_messages_proto3.hpb.h"
#include "hpb/base/string_view.h"
#include "hpb/collections/array.h"
#include "hpb/mem/arena.hpp"
#include "hpb/message/accessors.h"
#include "hpb/message/copy.h"
#include "hpb/mini_descriptor/internal/encode.hpp"
#include "hpb/mini_descriptor/internal/modifiers.h"
#include "hpb/test/test.hpb.h"
#include "hpb/wire/decode.h"

// Must be last
#include "hpb/port/def.inc"

namespace {

TEST(GeneratedCode, FindUnknown) {
  hpb_Arena* arena = hpb_Arena_New();
  hpb_test_ModelWithExtensions* msg = hpb_test_ModelWithExtensions_new(arena);
  hpb_test_ModelWithExtensions_set_random_int32(msg, 10);
  hpb_test_ModelWithExtensions_set_random_name(
      msg, hpb_StringView_FromString("Hello"));

  hpb_test_ModelExtension1* extension1 = hpb_test_ModelExtension1_new(arena);
  hpb_test_ModelExtension1_set_str(extension1,
                                   hpb_StringView_FromString("World"));

  hpb_test_ModelExtension1_set_model_ext(msg, extension1, arena);

  size_t serialized_size;
  char* serialized =
      hpb_test_ModelWithExtensions_serialize(msg, arena, &serialized_size);

  hpb_test_EmptyMessageWithExtensions* base_msg =
      hpb_test_EmptyMessageWithExtensions_parse(serialized, serialized_size,
                                                arena);

  hpb_FindUnknownRet result = hpb_MiniTable_FindUnknown(
      base_msg, hpb_test_ModelExtension1_model_ext_ext.field.number,
      kHpb_WireFormat_DefaultDepthLimit);
  EXPECT_EQ(kHpb_FindUnknown_Ok, result.status);

  result = hpb_MiniTable_FindUnknown(
      base_msg, hpb_test_ModelExtension2_model_ext_ext.field.number,
      kHpb_WireFormat_DefaultDepthLimit);
  EXPECT_EQ(kHpb_FindUnknown_NotPresent, result.status);

  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, Extensions) {
  hpb_Arena* arena = hpb_Arena_New();
  hpb_test_ModelWithExtensions* msg = hpb_test_ModelWithExtensions_new(arena);
  hpb_test_ModelWithExtensions_set_random_int32(msg, 10);
  hpb_test_ModelWithExtensions_set_random_name(
      msg, hpb_StringView_FromString("Hello"));

  hpb_test_ModelExtension1* extension1 = hpb_test_ModelExtension1_new(arena);
  hpb_test_ModelExtension1_set_str(extension1,
                                   hpb_StringView_FromString("World"));

  hpb_test_ModelExtension2* extension2 = hpb_test_ModelExtension2_new(arena);
  hpb_test_ModelExtension2_set_i(extension2, 5);

  hpb_test_ModelExtension2* extension3 = hpb_test_ModelExtension2_new(arena);
  hpb_test_ModelExtension2_set_i(extension3, 6);

  hpb_test_ModelExtension2* extension4 = hpb_test_ModelExtension2_new(arena);
  hpb_test_ModelExtension2_set_i(extension4, 7);

  hpb_test_ModelExtension2* extension5 = hpb_test_ModelExtension2_new(arena);
  hpb_test_ModelExtension2_set_i(extension5, 8);

  hpb_test_ModelExtension2* extension6 = hpb_test_ModelExtension2_new(arena);
  hpb_test_ModelExtension2_set_i(extension6, 9);

  // Set many extensions, to exercise code paths that involve reallocating the
  // extensions and unknown fields array.
  hpb_test_ModelExtension1_set_model_ext(msg, extension1, arena);
  hpb_test_ModelExtension2_set_model_ext(msg, extension2, arena);
  hpb_test_ModelExtension2_set_model_ext_2(msg, extension3, arena);
  hpb_test_ModelExtension2_set_model_ext_3(msg, extension4, arena);
  hpb_test_ModelExtension2_set_model_ext_4(msg, extension5, arena);
  hpb_test_ModelExtension2_set_model_ext_5(msg, extension6, arena);

  size_t serialized_size;
  char* serialized =
      hpb_test_ModelWithExtensions_serialize(msg, arena, &serialized_size);

  const hpb_Message_Extension* hpb_ext2;
  hpb_test_ModelExtension1* ext1;
  hpb_test_ModelExtension2* ext2;
  hpb_GetExtension_Status promote_status;

  // Test known GetExtension 1
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      msg, &hpb_test_ModelExtension1_model_ext_ext, 0, arena, &hpb_ext2);
  ext1 = (hpb_test_ModelExtension1*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_TRUE(hpb_StringView_IsEqual(hpb_StringView_FromString("World"),
                                     hpb_test_ModelExtension1_str(ext1)));

  // Test known GetExtension 2
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      msg, &hpb_test_ModelExtension2_model_ext_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(5, hpb_test_ModelExtension2_i(ext2));

  // Test known GetExtension 3
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      msg, &hpb_test_ModelExtension2_model_ext_2_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(6, hpb_test_ModelExtension2_i(ext2));

  // Test known GetExtension 4
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      msg, &hpb_test_ModelExtension2_model_ext_3_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(7, hpb_test_ModelExtension2_i(ext2));

  // Test known GetExtension 5
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      msg, &hpb_test_ModelExtension2_model_ext_4_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(8, hpb_test_ModelExtension2_i(ext2));

  // Test known GetExtension 6
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      msg, &hpb_test_ModelExtension2_model_ext_5_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(9, hpb_test_ModelExtension2_i(ext2));

  hpb_test_EmptyMessageWithExtensions* base_msg =
      hpb_test_EmptyMessageWithExtensions_parse(serialized, serialized_size,
                                                arena);

  // Get unknown extension bytes before promotion.
  size_t start_len;
  hpb_Message_GetUnknown(base_msg, &start_len);
  EXPECT_GT(start_len, 0);
  EXPECT_EQ(0, hpb_Message_ExtensionCount(base_msg));

  // Test unknown GetExtension.
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      base_msg, &hpb_test_ModelExtension1_model_ext_ext, 0, arena, &hpb_ext2);
  ext1 = (hpb_test_ModelExtension1*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_TRUE(hpb_StringView_IsEqual(hpb_StringView_FromString("World"),
                                     hpb_test_ModelExtension1_str(ext1)));

  // Test unknown GetExtension.
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      base_msg, &hpb_test_ModelExtension2_model_ext_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(5, hpb_test_ModelExtension2_i(ext2));

  // Test unknown GetExtension.
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      base_msg, &hpb_test_ModelExtension2_model_ext_2_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(6, hpb_test_ModelExtension2_i(ext2));

  // Test unknown GetExtension.
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      base_msg, &hpb_test_ModelExtension2_model_ext_3_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(7, hpb_test_ModelExtension2_i(ext2));

  // Test unknown GetExtension.
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      base_msg, &hpb_test_ModelExtension2_model_ext_4_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(8, hpb_test_ModelExtension2_i(ext2));

  // Test unknown GetExtension.
  promote_status = hpb_MiniTable_GetOrPromoteExtension(
      base_msg, &hpb_test_ModelExtension2_model_ext_5_ext, 0, arena, &hpb_ext2);
  ext2 = (hpb_test_ModelExtension2*)hpb_ext2->data.ptr;
  EXPECT_EQ(kHpb_GetExtension_Ok, promote_status);
  EXPECT_EQ(9, hpb_test_ModelExtension2_i(ext2));

  size_t end_len;
  hpb_Message_GetUnknown(base_msg, &end_len);
  EXPECT_LT(end_len, start_len);
  EXPECT_EQ(6, hpb_Message_ExtensionCount(base_msg));

  hpb_Arena_Free(arena);
}

// Create a minitable to mimic ModelWithSubMessages with unlinked subs
// to lazily promote unknowns after parsing.
hpb_MiniTable* CreateMiniTableWithEmptySubTables(hpb_Arena* arena) {
  hpb::MtDataEncoder e;
  e.StartMessage(0);
  e.PutField(kHpb_FieldType_Int32, 4, 0);
  e.PutField(kHpb_FieldType_Message, 5, 0);
  e.PutField(kHpb_FieldType_Message, 6, kHpb_FieldModifier_IsRepeated);

  hpb_Status status;
  hpb_Status_Clear(&status);
  hpb_MiniTable* table =
      hpb_MiniTable_Build(e.data().data(), e.data().size(), arena, &status);
  EXPECT_EQ(status.ok, true);
  return table;
}

hpb_MiniTable* CreateMapEntryMiniTable(hpb_Arena* arena) {
  hpb::MtDataEncoder e;
  e.EncodeMap(kHpb_FieldType_Int32, kHpb_FieldType_Message, 0, 0);
  hpb_Status status;
  hpb_Status_Clear(&status);
  hpb_MiniTable* table =
      hpb_MiniTable_Build(e.data().data(), e.data().size(), arena, &status);
  EXPECT_EQ(status.ok, true);
  return table;
}

// Create a minitable to mimic ModelWithMaps with unlinked subs
// to lazily promote unknowns after parsing.
hpb_MiniTable* CreateMiniTableWithEmptySubTablesForMaps(hpb_Arena* arena) {
  hpb::MtDataEncoder e;
  e.StartMessage(0);
  e.PutField(kHpb_FieldType_Int32, 1, 0);
  e.PutField(kHpb_FieldType_Message, 3, kHpb_FieldModifier_IsRepeated);
  e.PutField(kHpb_FieldType_Message, 5, kHpb_FieldModifier_IsRepeated);

  hpb_Status status;
  hpb_Status_Clear(&status);
  hpb_MiniTable* table =
      hpb_MiniTable_Build(e.data().data(), e.data().size(), arena, &status);

  // Field 5 corresponds to ModelWithMaps.map_sm.
  hpb_MiniTableField* map_field = const_cast<hpb_MiniTableField*>(
      hpb_MiniTable_FindFieldByNumber(table, 5));
  EXPECT_NE(map_field, nullptr);
  hpb_MiniTable* sub_table = CreateMapEntryMiniTable(arena);
  hpb_MiniTable_SetSubMessage(table, map_field, sub_table);
  EXPECT_EQ(status.ok, true);
  return table;
}

void CheckReserialize(const hpb_Message* msg, const hpb_MiniTable* mini_table,
                      hpb_Arena* arena, char* serialized,
                      size_t serialized_size) {
  // We can safely encode the "empty" message.  We expect to get the same bytes
  // out as were parsed.
  size_t reserialized_size;
  char* reserialized;
  hpb_EncodeStatus encode_status =
      hpb_Encode(msg, mini_table, kHpb_EncodeOption_Deterministic, arena,
                 &reserialized, &reserialized_size);
  EXPECT_EQ(encode_status, kHpb_EncodeStatus_Ok);
  EXPECT_EQ(reserialized_size, serialized_size);
  EXPECT_EQ(0, memcmp(reserialized, serialized, serialized_size));

  // We should get the same result if we copy+reserialize.
  hpb_Message* clone = hpb_Message_DeepClone(msg, mini_table, arena);
  encode_status = hpb_Encode(clone, mini_table, kHpb_EncodeOption_Deterministic,
                             arena, &reserialized, &reserialized_size);
  EXPECT_EQ(encode_status, kHpb_EncodeStatus_Ok);
  EXPECT_EQ(reserialized_size, serialized_size);
  EXPECT_EQ(0, memcmp(reserialized, serialized, serialized_size));
}

TEST(GeneratedCode, PromoteUnknownMessage) {
  hpb::Arena arena;
  hpb_test_ModelWithSubMessages* input_msg =
      hpb_test_ModelWithSubMessages_new(arena.ptr());
  hpb_test_ModelWithExtensions* sub_message =
      hpb_test_ModelWithExtensions_new(arena.ptr());
  hpb_test_ModelWithSubMessages_set_id(input_msg, 11);
  hpb_test_ModelWithExtensions_set_random_int32(sub_message, 12);
  hpb_test_ModelWithSubMessages_set_optional_child(input_msg, sub_message);
  size_t serialized_size;
  char* serialized = hpb_test_ModelWithSubMessages_serialize(
      input_msg, arena.ptr(), &serialized_size);

  hpb_MiniTable* mini_table = CreateMiniTableWithEmptySubTables(arena.ptr());
  hpb_DecodeStatus decode_status;

  // If we parse without allowing unlinked objects, the parse will fail.
  // TODO(haberman): re-enable this test once the old method of tree shaking is
  // removed
  // hpb_Message* fail_msg = _hpb_Message_New(mini_table, arena.ptr());
  // decode_status =
  //     hpb_Decode(serialized, serialized_size, fail_msg, mini_table, nullptr,
  //     0,
  //                arena.ptr());
  // EXPECT_EQ(decode_status, kHpb_DecodeStatus_UnlinkedSubMessage);

  // if we parse while allowing unlinked objects, the parse will succeed.
  hpb_Message* msg = _hpb_Message_New(mini_table, arena.ptr());
  decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 kHpb_DecodeOption_ExperimentalAllowUnlinked, arena.ptr());
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);

  CheckReserialize(msg, mini_table, arena.ptr(), serialized, serialized_size);

  // We can encode the "empty" message and get the same output bytes.
  size_t reserialized_size;
  char* reserialized;
  hpb_EncodeStatus encode_status = hpb_Encode(
      msg, mini_table, 0, arena.ptr(), &reserialized, &reserialized_size);
  EXPECT_EQ(encode_status, kHpb_EncodeStatus_Ok);
  EXPECT_EQ(reserialized_size, serialized_size);
  EXPECT_EQ(0, memcmp(reserialized, serialized, serialized_size));

  // Int32 field is present, as normal.
  int32_t val = hpb_Message_GetInt32(
      msg, hpb_MiniTable_FindFieldByNumber(mini_table, 4), 0);
  EXPECT_EQ(val, 11);

  // Unlinked sub-message is present, but getting the value returns NULL.
  const hpb_MiniTableField* submsg_field =
      hpb_MiniTable_FindFieldByNumber(mini_table, 5);
  ASSERT_TRUE(submsg_field != nullptr);
  EXPECT_TRUE(hpb_Message_HasField(msg, submsg_field));
  hpb_TaggedMessagePtr tagged =
      hpb_Message_GetTaggedMessagePtr(msg, submsg_field, nullptr);
  EXPECT_TRUE(hpb_TaggedMessagePtr_IsEmpty(tagged));

  // Update mini table and promote unknown to a message.
  EXPECT_TRUE(
      hpb_MiniTable_SetSubMessage(mini_table, (hpb_MiniTableField*)submsg_field,
                                  &hpb_test_ModelWithExtensions_msg_init));

  const int decode_options = hpb_DecodeOptions_MaxDepth(
      kHpb_WireFormat_DefaultDepthLimit);  // UPB_DECODE_ALIAS disabled.
  hpb_test_ModelWithExtensions* promoted;
  hpb_DecodeStatus promote_result =
      hpb_Message_PromoteMessage(msg, mini_table, submsg_field, decode_options,
                                 arena.ptr(), (hpb_Message**)&promoted);
  EXPECT_EQ(promote_result, kHpb_DecodeStatus_Ok);
  EXPECT_NE(nullptr, promoted);
  EXPECT_EQ(promoted, hpb_Message_GetMessage(msg, submsg_field, nullptr));
  EXPECT_EQ(hpb_test_ModelWithExtensions_random_int32(promoted), 12);
}

// Tests a second parse that reuses an empty/unlinked message while the message
// is still unlinked.
TEST(GeneratedCode, ReparseUnlinked) {
  hpb::Arena arena;
  hpb_test_ModelWithSubMessages* input_msg =
      hpb_test_ModelWithSubMessages_new(arena.ptr());
  hpb_test_ModelWithExtensions* sub_message =
      hpb_test_ModelWithExtensions_new(arena.ptr());
  hpb_test_ModelWithSubMessages_set_id(input_msg, 11);
  hpb_test_ModelWithExtensions_add_repeated_int32(sub_message, 12, arena.ptr());
  hpb_test_ModelWithSubMessages_set_optional_child(input_msg, sub_message);
  size_t serialized_size;
  char* serialized = hpb_test_ModelWithSubMessages_serialize(
      input_msg, arena.ptr(), &serialized_size);

  hpb_MiniTable* mini_table = CreateMiniTableWithEmptySubTables(arena.ptr());

  // Parse twice without linking the MiniTable.
  hpb_Message* msg = _hpb_Message_New(mini_table, arena.ptr());
  hpb_DecodeStatus decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 kHpb_DecodeOption_ExperimentalAllowUnlinked, arena.ptr());
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);

  decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 kHpb_DecodeOption_ExperimentalAllowUnlinked, arena.ptr());
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);

  // Update mini table and promote unknown to a message.
  const hpb_MiniTableField* submsg_field =
      hpb_MiniTable_FindFieldByNumber(mini_table, 5);
  EXPECT_TRUE(
      hpb_MiniTable_SetSubMessage(mini_table, (hpb_MiniTableField*)submsg_field,
                                  &hpb_test_ModelWithExtensions_msg_init));

  const int decode_options = hpb_DecodeOptions_MaxDepth(
      kHpb_WireFormat_DefaultDepthLimit);  // UPB_DECODE_ALIAS disabled.
  hpb_test_ModelWithExtensions* promoted;
  hpb_DecodeStatus promote_result =
      hpb_Message_PromoteMessage(msg, mini_table, submsg_field, decode_options,
                                 arena.ptr(), (hpb_Message**)&promoted);
  EXPECT_EQ(promote_result, kHpb_DecodeStatus_Ok);
  EXPECT_NE(nullptr, promoted);
  EXPECT_EQ(promoted, hpb_Message_GetMessage(msg, submsg_field, nullptr));

  // The repeated field should have two entries for the two parses.
  size_t repeated_size;
  const int32_t* entries =
      hpb_test_ModelWithExtensions_repeated_int32(promoted, &repeated_size);
  EXPECT_EQ(repeated_size, 2);
  EXPECT_EQ(entries[0], 12);
  EXPECT_EQ(entries[1], 12);
}

// Tests a second parse that promotes a message within the parser because we are
// merging into an empty/unlinked message after the message has been linked.
TEST(GeneratedCode, PromoteInParser) {
  hpb::Arena arena;
  hpb_test_ModelWithSubMessages* input_msg =
      hpb_test_ModelWithSubMessages_new(arena.ptr());
  hpb_test_ModelWithExtensions* sub_message =
      hpb_test_ModelWithExtensions_new(arena.ptr());
  hpb_test_ModelWithSubMessages_set_id(input_msg, 11);
  hpb_test_ModelWithExtensions_add_repeated_int32(sub_message, 12, arena.ptr());
  hpb_test_ModelWithSubMessages_set_optional_child(input_msg, sub_message);
  size_t serialized_size;
  char* serialized = hpb_test_ModelWithSubMessages_serialize(
      input_msg, arena.ptr(), &serialized_size);

  hpb_MiniTable* mini_table = CreateMiniTableWithEmptySubTables(arena.ptr());

  // Parse once without linking the MiniTable.
  hpb_Message* msg = _hpb_Message_New(mini_table, arena.ptr());
  hpb_DecodeStatus decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 kHpb_DecodeOption_ExperimentalAllowUnlinked, arena.ptr());
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);

  // Link the MiniTable.
  const hpb_MiniTableField* submsg_field =
      hpb_MiniTable_FindFieldByNumber(mini_table, 5);
  EXPECT_TRUE(
      hpb_MiniTable_SetSubMessage(mini_table, (hpb_MiniTableField*)submsg_field,
                                  &hpb_test_ModelWithExtensions_msg_init));

  // Parse again.  This will promote the message.  An explicit promote will not
  // be required.
  decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 kHpb_DecodeOption_ExperimentalAllowUnlinked, arena.ptr());
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);
  hpb_test_ModelWithExtensions* promoted =
      (hpb_test_ModelWithExtensions*)hpb_Message_GetMessage(msg, submsg_field,
                                                            nullptr);

  EXPECT_NE(nullptr, promoted);
  EXPECT_EQ(promoted, hpb_Message_GetMessage(msg, submsg_field, nullptr));

  // The repeated field should have two entries for the two parses.
  size_t repeated_size;
  const int32_t* entries =
      hpb_test_ModelWithExtensions_repeated_int32(promoted, &repeated_size);
  EXPECT_EQ(repeated_size, 2);
  EXPECT_EQ(entries[0], 12);
  EXPECT_EQ(entries[1], 12);
}

TEST(GeneratedCode, PromoteUnknownRepeatedMessage) {
  hpb::Arena arena;
  hpb_test_ModelWithSubMessages* input_msg =
      hpb_test_ModelWithSubMessages_new(arena.ptr());
  hpb_test_ModelWithSubMessages_set_id(input_msg, 123);

  // Add 2 repeated messages to input_msg.
  hpb_test_ModelWithExtensions* item =
      hpb_test_ModelWithSubMessages_add_items(input_msg, arena.ptr());
  hpb_test_ModelWithExtensions_set_random_int32(item, 5);
  item = hpb_test_ModelWithSubMessages_add_items(input_msg, arena.ptr());
  hpb_test_ModelWithExtensions_set_random_int32(item, 6);

  size_t serialized_size;
  char* serialized = hpb_test_ModelWithSubMessages_serialize(
      input_msg, arena.ptr(), &serialized_size);

  hpb_MiniTable* mini_table = CreateMiniTableWithEmptySubTables(arena.ptr());
  hpb_DecodeStatus decode_status;

  // If we parse without allowing unlinked objects, the parse will fail.
  // TODO(haberman): re-enable this test once the old method of tree shaking is
  // removed
  // hpb_Message* fail_msg = _hpb_Message_New(mini_table, arena.ptr());
  // decode_status =
  //     hpb_Decode(serialized, serialized_size, fail_msg, mini_table, nullptr,
  //     0,
  //                arena.ptr());
  // EXPECT_EQ(decode_status, kHpb_DecodeStatus_UnlinkedSubMessage);

  // if we parse while allowing unlinked objects, the parse will succeed.
  hpb_Message* msg = _hpb_Message_New(mini_table, arena.ptr());
  decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 kHpb_DecodeOption_ExperimentalAllowUnlinked, arena.ptr());

  CheckReserialize(msg, mini_table, arena.ptr(), serialized, serialized_size);

  // Int32 field is present, as normal.
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);
  int32_t val = hpb_Message_GetInt32(
      msg, hpb_MiniTable_FindFieldByNumber(mini_table, 4), 0);
  EXPECT_EQ(val, 123);

  const hpb_MiniTableField* repeated_field =
      hpb_MiniTable_FindFieldByNumber(mini_table, 6);

  hpb_Array* array = hpb_Message_GetMutableArray(msg, repeated_field);

  // Array length is 2 even though the messages are empty.
  EXPECT_EQ(2, hpb_Array_Size(array));

  // Update mini table and promote unknown to a message.
  EXPECT_TRUE(hpb_MiniTable_SetSubMessage(
      mini_table, (hpb_MiniTableField*)repeated_field,
      &hpb_test_ModelWithExtensions_msg_init));
  const int decode_options = hpb_DecodeOptions_MaxDepth(
      kHpb_WireFormat_DefaultDepthLimit);  // UPB_DECODE_ALIAS disabled.
  hpb_DecodeStatus promote_result =
      hpb_Array_PromoteMessages(array, &hpb_test_ModelWithExtensions_msg_init,
                                decode_options, arena.ptr());
  EXPECT_EQ(promote_result, kHpb_DecodeStatus_Ok);
  const hpb_Message* promoted_message = hpb_Array_Get(array, 0).msg_val;
  EXPECT_EQ(hpb_test_ModelWithExtensions_random_int32(
                (hpb_test_ModelWithExtensions*)promoted_message),
            5);
  promoted_message = hpb_Array_Get(array, 1).msg_val;
  EXPECT_EQ(hpb_test_ModelWithExtensions_random_int32(
                (hpb_test_ModelWithExtensions*)promoted_message),
            6);
}

TEST(GeneratedCode, PromoteUnknownToMap) {
  hpb::Arena arena;
  hpb_test_ModelWithMaps* input_msg = hpb_test_ModelWithMaps_new(arena.ptr());
  hpb_test_ModelWithMaps_set_id(input_msg, 123);

  hpb_test_ModelWithExtensions* submsg1 =
      hpb_test_ModelWithExtensions_new(arena.ptr());
  hpb_test_ModelWithExtensions_set_random_int32(submsg1, 123);
  hpb_test_ModelWithExtensions* submsg2 =
      hpb_test_ModelWithExtensions_new(arena.ptr());
  hpb_test_ModelWithExtensions_set_random_int32(submsg2, 456);

  // Add 2 map entries.
  hpb_test_ModelWithMaps_map_im_set(input_msg, 111, submsg1, arena.ptr());
  hpb_test_ModelWithMaps_map_im_set(input_msg, 222, submsg2, arena.ptr());

  size_t serialized_size;
  char* serialized = hpb_test_ModelWithMaps_serialize_ex(
      input_msg, kHpb_EncodeOption_Deterministic, arena.ptr(),
      &serialized_size);

  hpb_MiniTable* mini_table =
      CreateMiniTableWithEmptySubTablesForMaps(arena.ptr());

  // If we parse without allowing unlinked objects, the parse will fail.
  hpb_Message* fail_msg1 = _hpb_Message_New(mini_table, arena.ptr());
  hpb_DecodeStatus decode_status =
      hpb_Decode(serialized, serialized_size, fail_msg1, mini_table, nullptr, 0,
                 arena.ptr());
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_UnlinkedSubMessage);

  // if we parse while allowing unlinked objects, the parse will succeed.
  hpb_Message* msg = _hpb_Message_New(mini_table, arena.ptr());
  decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 kHpb_DecodeOption_ExperimentalAllowUnlinked, arena.ptr());
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);

  CheckReserialize(msg, mini_table, arena.ptr(), serialized, serialized_size);

  hpb_MiniTableField* map_field = const_cast<hpb_MiniTableField*>(
      hpb_MiniTable_FindFieldByNumber(mini_table, 5));

  hpb_Map* map = hpb_Message_GetMutableMap(msg, map_field);

  // Map size is 2 even though messages are unlinked.
  EXPECT_EQ(2, hpb_Map_Size(map));

  // Update mini table and promote unknown to a message.
  hpb_MiniTable* entry = const_cast<hpb_MiniTable*>(
      hpb_MiniTable_GetSubMessageTable(mini_table, map_field));
  hpb_MiniTableField* entry_value = const_cast<hpb_MiniTableField*>(
      hpb_MiniTable_FindFieldByNumber(entry, 2));
  hpb_MiniTable_SetSubMessage(entry, entry_value,
                              &hpb_test_ModelWithExtensions_msg_init);
  hpb_DecodeStatus promote_result = hpb_Map_PromoteMessages(
      map, &hpb_test_ModelWithExtensions_msg_init, 0, arena.ptr());
  EXPECT_EQ(promote_result, kHpb_DecodeStatus_Ok);

  hpb_MessageValue key;
  hpb_MessageValue val;
  key.int32_val = 111;
  EXPECT_TRUE(hpb_Map_Get(map, key, &val));
  EXPECT_EQ(123,
            hpb_test_ModelWithExtensions_random_int32(
                static_cast<const hpb_test_ModelWithExtensions*>(val.msg_val)));

  key.int32_val = 222;
  EXPECT_TRUE(hpb_Map_Get(map, key, &val));
  EXPECT_EQ(456,
            hpb_test_ModelWithExtensions_random_int32(
                static_cast<const hpb_test_ModelWithExtensions*>(val.msg_val)));
}

}  // namespace

// OLD tests, to be removed!

namespace {

// Create a minitable to mimic ModelWithSubMessages with unlinked subs
// to lazily promote unknowns after parsing.
hpb_MiniTable* CreateMiniTableWithEmptySubTablesOld(hpb_Arena* arena) {
  hpb::MtDataEncoder e;
  e.StartMessage(0);
  e.PutField(kHpb_FieldType_Int32, 4, 0);
  e.PutField(kHpb_FieldType_Message, 5, 0);
  e.PutField(kHpb_FieldType_Message, 6, kHpb_FieldModifier_IsRepeated);

  hpb_Status status;
  hpb_Status_Clear(&status);
  hpb_MiniTable* table =
      hpb_MiniTable_Build(e.data().data(), e.data().size(), arena, &status);
  EXPECT_EQ(status.ok, true);
  // Initialize sub table to null. Not using hpb_MiniTable_SetSubMessage
  // since it checks ->ext on parameter.
  hpb_MiniTableSub* sub = const_cast<hpb_MiniTableSub*>(
      &table->subs[table->fields[1].HPB_PRIVATE(submsg_index)]);
  sub = const_cast<hpb_MiniTableSub*>(
      &table->subs[table->fields[2].HPB_PRIVATE(submsg_index)]);
  return table;
}

// Create a minitable to mimic ModelWithMaps with unlinked subs
// to lazily promote unknowns after parsing.
hpb_MiniTable* CreateMiniTableWithEmptySubTablesForMapsOld(hpb_Arena* arena) {
  hpb::MtDataEncoder e;
  e.StartMessage(0);
  e.PutField(kHpb_FieldType_Int32, 1, 0);
  e.PutField(kHpb_FieldType_Message, 3, kHpb_FieldModifier_IsRepeated);
  e.PutField(kHpb_FieldType_Message, 4, kHpb_FieldModifier_IsRepeated);

  hpb_Status status;
  hpb_Status_Clear(&status);
  hpb_MiniTable* table =
      hpb_MiniTable_Build(e.data().data(), e.data().size(), arena, &status);
  EXPECT_EQ(status.ok, true);
  // Initialize sub table to null. Not using hpb_MiniTable_SetSubMessage
  // since it checks ->ext on parameter.
  hpb_MiniTableSub* sub = const_cast<hpb_MiniTableSub*>(
      &table->subs[table->fields[1].HPB_PRIVATE(submsg_index)]);
  sub = const_cast<hpb_MiniTableSub*>(
      &table->subs[table->fields[2].HPB_PRIVATE(submsg_index)]);
  return table;
}

hpb_MiniTable* CreateMapEntryMiniTableOld(hpb_Arena* arena) {
  hpb::MtDataEncoder e;
  e.EncodeMap(kHpb_FieldType_String, kHpb_FieldType_String, 0, 0);
  hpb_Status status;
  hpb_Status_Clear(&status);
  hpb_MiniTable* table =
      hpb_MiniTable_Build(e.data().data(), e.data().size(), arena, &status);
  EXPECT_EQ(status.ok, true);
  return table;
}

TEST(GeneratedCode, PromoteUnknownMessageOld) {
  hpb_Arena* arena = hpb_Arena_New();
  hpb_test_ModelWithSubMessages* input_msg =
      hpb_test_ModelWithSubMessages_new(arena);
  hpb_test_ModelWithExtensions* sub_message =
      hpb_test_ModelWithExtensions_new(arena);
  hpb_test_ModelWithSubMessages_set_id(input_msg, 11);
  hpb_test_ModelWithExtensions_set_random_int32(sub_message, 12);
  hpb_test_ModelWithSubMessages_set_optional_child(input_msg, sub_message);
  size_t serialized_size;
  char* serialized = hpb_test_ModelWithSubMessages_serialize(input_msg, arena,
                                                             &serialized_size);

  hpb_MiniTable* mini_table = CreateMiniTableWithEmptySubTablesOld(arena);
  hpb_Message* msg = _hpb_Message_New(mini_table, arena);
  hpb_DecodeStatus decode_status = hpb_Decode(serialized, serialized_size, msg,
                                              mini_table, nullptr, 0, arena);
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);
  int32_t val = hpb_Message_GetInt32(
      msg, hpb_MiniTable_FindFieldByNumber(mini_table, 4), 0);
  EXPECT_EQ(val, 11);
  hpb_FindUnknownRet unknown =
      hpb_MiniTable_FindUnknown(msg, 5, kHpb_WireFormat_DefaultDepthLimit);
  EXPECT_EQ(unknown.status, kHpb_FindUnknown_Ok);
  // Update mini table and promote unknown to a message.
  EXPECT_TRUE(hpb_MiniTable_SetSubMessage(
      mini_table, (hpb_MiniTableField*)&mini_table->fields[1],
      &hpb_test_ModelWithExtensions_msg_init));
  const int decode_options = hpb_DecodeOptions_MaxDepth(
      kHpb_WireFormat_DefaultDepthLimit);  // UPB_DECODE_ALIAS disabled.
  hpb_UnknownToMessageRet promote_result =
      hpb_MiniTable_PromoteUnknownToMessage(
          msg, mini_table, &mini_table->fields[1],
          &hpb_test_ModelWithExtensions_msg_init, decode_options, arena);
  EXPECT_EQ(promote_result.status, kHpb_UnknownToMessage_Ok);
  const hpb_Message* promoted_message =
      hpb_Message_GetMessage(msg, &mini_table->fields[1], nullptr);
  EXPECT_EQ(hpb_test_ModelWithExtensions_random_int32(
                (hpb_test_ModelWithExtensions*)promoted_message),
            12);
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, PromoteUnknownRepeatedMessageOld) {
  hpb_Arena* arena = hpb_Arena_New();
  hpb_test_ModelWithSubMessages* input_msg =
      hpb_test_ModelWithSubMessages_new(arena);
  hpb_test_ModelWithSubMessages_set_id(input_msg, 123);

  // Add 2 repeated messages to input_msg.
  hpb_test_ModelWithExtensions* item =
      hpb_test_ModelWithSubMessages_add_items(input_msg, arena);
  hpb_test_ModelWithExtensions_set_random_int32(item, 5);
  item = hpb_test_ModelWithSubMessages_add_items(input_msg, arena);
  hpb_test_ModelWithExtensions_set_random_int32(item, 6);

  size_t serialized_size;
  char* serialized = hpb_test_ModelWithSubMessages_serialize(input_msg, arena,
                                                             &serialized_size);

  hpb_MiniTable* mini_table = CreateMiniTableWithEmptySubTablesOld(arena);
  hpb_Message* msg = _hpb_Message_New(mini_table, arena);
  hpb_DecodeStatus decode_status = hpb_Decode(serialized, serialized_size, msg,
                                              mini_table, nullptr, 0, arena);
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);
  int32_t val = hpb_Message_GetInt32(
      msg, hpb_MiniTable_FindFieldByNumber(mini_table, 4), 0);
  EXPECT_EQ(val, 123);

  // Check that we have repeated field data in an unknown.
  hpb_FindUnknownRet unknown =
      hpb_MiniTable_FindUnknown(msg, 6, kHpb_WireFormat_DefaultDepthLimit);
  EXPECT_EQ(unknown.status, kHpb_FindUnknown_Ok);

  // Update mini table and promote unknown to a message.
  EXPECT_TRUE(hpb_MiniTable_SetSubMessage(
      mini_table, (hpb_MiniTableField*)&mini_table->fields[2],
      &hpb_test_ModelWithExtensions_msg_init));
  const int decode_options = hpb_DecodeOptions_MaxDepth(
      kHpb_WireFormat_DefaultDepthLimit);  // UPB_DECODE_ALIAS disabled.
  hpb_UnknownToMessage_Status promote_result =
      hpb_MiniTable_PromoteUnknownToMessageArray(
          msg, &mini_table->fields[2], &hpb_test_ModelWithExtensions_msg_init,
          decode_options, arena);
  EXPECT_EQ(promote_result, kHpb_UnknownToMessage_Ok);

  hpb_Array* array = hpb_Message_GetMutableArray(msg, &mini_table->fields[2]);
  const hpb_Message* promoted_message = hpb_Array_Get(array, 0).msg_val;
  EXPECT_EQ(hpb_test_ModelWithExtensions_random_int32(
                (hpb_test_ModelWithExtensions*)promoted_message),
            5);
  promoted_message = hpb_Array_Get(array, 1).msg_val;
  EXPECT_EQ(hpb_test_ModelWithExtensions_random_int32(
                (hpb_test_ModelWithExtensions*)promoted_message),
            6);
  hpb_Arena_Free(arena);
}

TEST(GeneratedCode, PromoteUnknownToMapOld) {
  hpb_Arena* arena = hpb_Arena_New();
  hpb_test_ModelWithMaps* input_msg = hpb_test_ModelWithMaps_new(arena);
  hpb_test_ModelWithMaps_set_id(input_msg, 123);

  // Add 2 map entries.
  hpb_test_ModelWithMaps_map_ss_set(input_msg,
                                    hpb_StringView_FromString("key1"),
                                    hpb_StringView_FromString("value1"), arena);
  hpb_test_ModelWithMaps_map_ss_set(input_msg,
                                    hpb_StringView_FromString("key2"),
                                    hpb_StringView_FromString("value2"), arena);

  size_t serialized_size;
  char* serialized =
      hpb_test_ModelWithMaps_serialize(input_msg, arena, &serialized_size);

  hpb_MiniTable* mini_table =
      CreateMiniTableWithEmptySubTablesForMapsOld(arena);
  hpb_MiniTable* map_entry_mini_table = CreateMapEntryMiniTableOld(arena);
  hpb_Message* msg = _hpb_Message_New(mini_table, arena);
  const int decode_options =
      hpb_DecodeOptions_MaxDepth(kHpb_WireFormat_DefaultDepthLimit);
  hpb_DecodeStatus decode_status =
      hpb_Decode(serialized, serialized_size, msg, mini_table, nullptr,
                 decode_options, arena);
  EXPECT_EQ(decode_status, kHpb_DecodeStatus_Ok);
  int32_t val = hpb_Message_GetInt32(
      msg, hpb_MiniTable_FindFieldByNumber(mini_table, 1), 0);
  EXPECT_EQ(val, 123);

  // Check that we have map data in an unknown.
  hpb_FindUnknownRet unknown =
      hpb_MiniTable_FindUnknown(msg, 3, kHpb_WireFormat_DefaultDepthLimit);
  EXPECT_EQ(unknown.status, kHpb_FindUnknown_Ok);

  // Update mini table and promote unknown to a message.
  EXPECT_TRUE(hpb_MiniTable_SetSubMessage(
      mini_table, (hpb_MiniTableField*)&mini_table->fields[1],
      map_entry_mini_table));
  hpb_UnknownToMessage_Status promote_result =
      hpb_MiniTable_PromoteUnknownToMap(msg, mini_table, &mini_table->fields[1],
                                        decode_options, arena);
  EXPECT_EQ(promote_result, kHpb_UnknownToMessage_Ok);

  hpb_Map* map = hpb_Message_GetOrCreateMutableMap(
      msg, map_entry_mini_table, &mini_table->fields[1], arena);
  EXPECT_NE(map, nullptr);
  // Lookup in map.
  hpb_MessageValue key;
  key.str_val = hpb_StringView_FromString("key2");
  hpb_MessageValue value;
  EXPECT_TRUE(hpb_Map_Get(map, key, &value));
  EXPECT_EQ(0, strncmp(value.str_val.data, "value2", 5));
  hpb_Arena_Free(arena);
}

}  // namespace
