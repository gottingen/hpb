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

#include "hpb/test/fuzz_util.h"

#include "hpb/base/status.hpp"
#include "hpb/message/message.h"
#include "hpb/mini_descriptor/decode.h"
#include "hpb/mini_table/extension.h"
#include "hpb/mini_table/extension_registry.h"

// Must be last
#include "hpb/port/def.inc"

namespace hpb {
namespace fuzz {

namespace {

class Builder {
 public:
  Builder(const MiniTableFuzzInput& input, hpb_Arena* arena)
      : input_(&input), arena_(arena) {}

  const hpb_MiniTable* Build(hpb_ExtensionRegistry** exts) {
    BuildMessages();
    BuildEnums();
    BuildExtensions(exts);
    if (!LinkMessages()) return nullptr;
    return mini_tables_.empty() ? nullptr : mini_tables_.front();
  }

 private:
  void BuildMessages();
  void BuildEnums();
  void BuildExtensions(hpb_ExtensionRegistry** exts);
  bool LinkExtension(hpb_MiniTableExtension* ext);
  bool LinkMessages();

  size_t NextLink() {
    if (input_->links.empty()) return 0;
    if (link_ == input_->links.size()) link_ = 0;
    return input_->links[link_++];
  }

  const hpb_MiniTable* NextMiniTable() {
    return mini_tables_.empty()
               ? nullptr
               : mini_tables_[NextLink() % mini_tables_.size()];
  }

  const hpb_MiniTableEnum* NextEnumTable() {
    return enum_tables_.empty()
               ? nullptr
               : enum_tables_[NextLink() % enum_tables_.size()];
  }

  const MiniTableFuzzInput* input_;
  hpb_Arena* arena_;
  std::vector<const hpb_MiniTable*> mini_tables_;
  std::vector<const hpb_MiniTableEnum*> enum_tables_;
  size_t link_ = 0;
};

void Builder::BuildMessages() {
  hpb::Status status;
  mini_tables_.reserve(input_->mini_descriptors.size());
  for (const auto& d : input_->mini_descriptors) {
    hpb_MiniTable* table =
        hpb_MiniTable_Build(d.data(), d.size(), arena_, status.ptr());
    if (table) mini_tables_.push_back(table);
  }
}

void Builder::BuildEnums() {
  hpb::Status status;
  enum_tables_.reserve(input_->enum_mini_descriptors.size());
  for (const auto& d : input_->enum_mini_descriptors) {
    hpb_MiniTableEnum* enum_table =
        hpb_MiniTableEnum_Build(d.data(), d.size(), arena_, status.ptr());
    if (enum_table) enum_tables_.push_back(enum_table);
  }
}

bool Builder::LinkExtension(hpb_MiniTableExtension* ext) {
  hpb_MiniTableField* field = &ext->field;
  if (hpb_MiniTableField_CType(field) == kHpb_CType_Message) {
    auto mt = NextMiniTable();
    if (!mt) field->HPB_PRIVATE(descriptortype) = kHpb_FieldType_Int32;
    ext->sub.submsg = mt;
  }
  if (hpb_MiniTableField_IsClosedEnum(field)) {
    auto et = NextEnumTable();
    if (!et) field->HPB_PRIVATE(descriptortype) = kHpb_FieldType_Int32;
    ext->sub.subenum = et;
  }
  return true;
}

void Builder::BuildExtensions(hpb_ExtensionRegistry** exts) {
  hpb::Status status;
  if (input_->extensions.empty()) {
    *exts = nullptr;
  } else {
    *exts = hpb_ExtensionRegistry_New(arena_);
    const char* ptr = input_->extensions.data();
    const char* end = ptr + input_->extensions.size();
    // Iterate through the buffer, building extensions as long as we can.
    while (ptr < end) {
      hpb_MiniTableExtension* ext = reinterpret_cast<hpb_MiniTableExtension*>(
          hpb_Arena_Malloc(arena_, sizeof(*ext)));
      hpb_MiniTableSub sub;
      const hpb_MiniTable* extendee = NextMiniTable();
      if (!extendee) break;
      ptr = hpb_MiniTableExtension_Init(ptr, end - ptr, ext, extendee, sub,
                                        status.ptr());
      if (!ptr) break;
      if (!LinkExtension(ext)) continue;
      if (hpb_ExtensionRegistry_Lookup(*exts, ext->extendee, ext->field.number))
        continue;
      hpb_ExtensionRegistry_AddArray(
          *exts, const_cast<const hpb_MiniTableExtension**>(&ext), 1);
    }
  }
}

bool Builder::LinkMessages() {
  for (auto* t : mini_tables_) {
    hpb_MiniTable* table = const_cast<hpb_MiniTable*>(t);
    // For each field that requires a sub-table, assign one as appropriate.
    for (size_t i = 0; i < table->field_count; i++) {
      hpb_MiniTableField* field =
          const_cast<hpb_MiniTableField*>(&table->fields[i]);
      if (link_ == input_->links.size()) link_ = 0;
      if (hpb_MiniTableField_CType(field) == kHpb_CType_Message &&
          !hpb_MiniTable_SetSubMessage(table, field, NextMiniTable())) {
        return false;
      }
      if (hpb_MiniTableField_IsClosedEnum(field)) {
        auto* et = NextEnumTable();
        if (et) {
          if (!hpb_MiniTable_SetSubEnum(table, field, et)) return false;
        } else {
          // We don't have any sub-enums.  Override the field type so that it is
          // not needed.
          field->HPB_PRIVATE(descriptortype) = kHpb_FieldType_Int32;
        }
      }
    }
  }
  return true;
}

}  // namespace

const hpb_MiniTable* BuildMiniTable(const MiniTableFuzzInput& input,
                                    hpb_ExtensionRegistry** exts,
                                    hpb_Arena* arena) {
  Builder builder(input, arena);
  return builder.Build(exts);
}

}  // namespace fuzz
}  // namespace hpb
