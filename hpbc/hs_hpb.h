// Copyright 2023 The Elastic-AI Authors.
// part of Elastic AI Search
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#ifndef HPB_HS_HPB_H_
#define HPB_HS_HPB_H_


#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "hpb/base/descriptor_constants.h"
#include "hpb/base/string_view.h"
#include "hpb/reflection/def.hpp"
#include "hpb/wire/types.h"
#include "hpbc/common.h"
#include "hpbc/file_layout.h"
#include "hpbc/names.h"
#include "hpbc/plugin.h"

// Must be last.
#include "hpb/port/def.inc"

namespace hpbc {
    typedef std::pair<std::string, uint64_t> TableEntry;
    class Hshpb {
    public:
        static const char* kEnumsInit;
        static const char* kExtensionsInit;
        static const char* kMessagesInit;
    public:
        Hshpb(bool bootstrap) : bootstrap(bootstrap) {}

        ~Hshpb() {}

        void GenerateFile(const DefPoolPair &pools, hpb::FileDefPtr file, Plugin *plugin);

    private:

        void WriteDeclare(const DefPoolPair &pools, hpb::FileDefPtr file, Output &output) const;

        void WriteHs(const DefPoolPair &pools, hpb::FileDefPtr file, Output &output) const;

        void WriteHeader(const DefPoolPair &pools, hpb::FileDefPtr file, Output &output);

        void GenerateMessage(hpb::MessageDefPtr message,
                                     const DefPoolPair &pools,
                                     Output &output) const;


        void ForwardDeclareMiniTableInit(hpb::MessageDefPtr message, Output &output) const;

        std::string MessageInitName(hpb::MessageDefPtr descriptor) const;

        std::string ExtensionLayout(hpb::FieldDefPtr ext) const;

        std::string ExtensionIdentBase(hpb::FieldDefPtr ext) const;

        void DumpEnumValues(hpb::EnumDefPtr desc, Output &output) const;

        std::string EnumValueSymbol(hpb::EnumValDefPtr value) const;

        void GenerateMessageInHeader(hpb::MessageDefPtr message,
                                     const DefPoolPair &pools,
                                     Output &output) const;

        void GenerateMessageFunctions(hpb::MessageDefPtr message, Output &output) const;

        std::string MessageMiniTableRef(hpb::MessageDefPtr descriptor) const;

        void GenerateOneofInHeader(hpb::OneofDefPtr oneof, const DefPoolPair &pools, absl::string_view msg_name,
                                   Output &output) const;

        std::string FieldInitializer(hpb::FieldDefPtr field, const hpb_MiniTableField *field64,
                                     const hpb_MiniTableField *field32) const;

        std::string FieldInitializer(const DefPoolPair &pools, hpb::FieldDefPtr field) const;

        std::string ArchDependentSize(int64_t size32, int64_t size64) const;

        std::string GetModeInit(const hpb_MiniTableField *field32, const hpb_MiniTableField *field64) const;

        std::string GetFieldRep(const hpb_MiniTableField *field32, const hpb_MiniTableField *field64) const;

        std::string GetFieldRep(const DefPoolPair& pools, hpb::FieldDefPtr field) const {
            return GetFieldRep(pools.GetField32(field), pools.GetField64(field));
        }

        void GenerateExtensionInHeader(const DefPoolPair &pools, hpb::FieldDefPtr ext, Output &output) const;

        std::string CTypeConst(hpb::FieldDefPtr field) const;

        std::string CTypeInternal(hpb::FieldDefPtr field, bool is_const) const;

        std::string FieldDefault(hpb::FieldDefPtr field) const;

        std::string FloatToCLiteral(float value) const;

        std::string DoubleToCLiteral(double value) const;

        void GenerateClear(hpb::FieldDefPtr field, const DefPoolPair &pools, absl::string_view msg_name,
                           const NameToFieldDefMap &field_names,
                           Output &output) const;

        void GenerateGetters(hpb::FieldDefPtr field, const DefPoolPair &pools,
                             absl::string_view msg_name,
                             const NameToFieldDefMap &field_names, Output &output) const;

        void GenerateMapGetters(hpb::FieldDefPtr field, const DefPoolPair &pools,
                                absl::string_view msg_name,
                                const NameToFieldDefMap &field_names, Output &output) const;

        std::string MapKeyCType(hpb::FieldDefPtr map_field) const;

        std::string MapValueCType(hpb::FieldDefPtr map_field) const;

        std::string CType(hpb::FieldDefPtr field) const;

        std::string MapKeySize(hpb::FieldDefPtr map_field, absl::string_view expr) const;

        std::string MapValueSize(hpb::FieldDefPtr map_field, absl::string_view expr) const;

        void GenerateMapEntryGetters(hpb::FieldDefPtr field, absl::string_view msg_name,
                                     Output &output) const;

        void GenerateRepeatedGetters(hpb::FieldDefPtr field, const DefPoolPair &pools,
                                     absl::string_view msg_name,
                                     const NameToFieldDefMap &field_names, Output &output) const;

        void GenerateScalarGetters(hpb::FieldDefPtr field, const DefPoolPair &pools,
                                   absl::string_view msg_name,
                                   const NameToFieldDefMap &field_names, Output &output) const;

        void GenerateHazzer(hpb::FieldDefPtr field, const DefPoolPair &pools,
                            absl::string_view msg_name,
                            const NameToFieldDefMap &field_names, Output &output) const;

        void GenerateSetters(hpb::FieldDefPtr field, const DefPoolPair &pools,
                             absl::string_view msg_name,
                             const NameToFieldDefMap &field_names, Output &output) const;

        void GenerateMapSetters(hpb::FieldDefPtr field, const DefPoolPair &pools,
                                absl::string_view msg_name,
                                const NameToFieldDefMap &field_names, Output &output) const;

        void GenerateRepeatedSetters(hpb::FieldDefPtr field, const DefPoolPair &pools,
                                     absl::string_view msg_name,
                                     const NameToFieldDefMap &field_names, Output &output) const;

        void GenerateNonRepeatedSetters(hpb::FieldDefPtr field,
                                        const DefPoolPair &pools,
                                        absl::string_view msg_name,
                                        const NameToFieldDefMap &field_names, Output &output) const;

        void WriteSource(const DefPoolPair &pools, hpb::FileDefPtr file, Output &output) const;

        void WriteMiniDescriptorSource(const DefPoolPair &pools, hpb::FileDefPtr file, Output &output) const;

        void WriteMessageMiniDescriptorInitializer(hpb::MessageDefPtr msg, Output &output) const;

        std::string EnumMiniTableRef(hpb::EnumDefPtr descriptor) const;

        std::string EnumInitName(hpb::EnumDefPtr descriptor) const;

        void WriteEnumMiniDescriptorInitializer(hpb::EnumDefPtr enum_def, Output &output) const;

        void WriteMiniTableSource(const DefPoolPair &pools, hpb::FileDefPtr file, Output &output) const;

        int WriteMessages(const DefPoolPair &pools, hpb::FileDefPtr file, Output &output) const;

        void WriteMessage(hpb::MessageDefPtr message, const DefPoolPair &pools, Output &output) const;

        std::string GetSub(hpb::FieldDefPtr field) const;
        int WriteExtensions(const DefPoolPair& pools, hpb::FileDefPtr file, Output& output) const;

        void WriteExtension(hpb::FieldDefPtr ext, const DefPoolPair& pools,Output& output) const;

        int WriteEnums(const DefPoolPair& pools, hpb::FileDefPtr file, Output& output) const;

        void WriteEnum(hpb::EnumDefPtr e, Output& output) const;
        void WriteMessageField(hpb::FieldDefPtr field,
                                     const hpb_MiniTableField* field64,
                                     const hpb_MiniTableField* field32,Output& output) const;
        std::vector<TableEntry> FastDecodeTable(hpb::MessageDefPtr message,
                                                      const DefPoolPair& pools) const;

        std::vector<hpb::FieldDefPtr> FieldHotnessOrder(hpb::MessageDefPtr message) const;

        int GetTableSlot(hpb::FieldDefPtr field) const;

        uint64_t GetEncodedTag(hpb::FieldDefPtr field) const;

        uint32_t GetWireTypeForField(hpb::FieldDefPtr field) const;

        bool TryFillTableEntry(const DefPoolPair& pools, hpb::FieldDefPtr field, TableEntry& ent) const;

        uint32_t MakeTag(uint32_t field_number, uint32_t wire_type) const {
            return field_number << 3 | wire_type;
        }

        size_t WriteVarint32ToArray(uint64_t val, char* buf) const {
            size_t i = 0;
            do {
                uint8_t byte = val & 0x7fU;
                val >>= 7;
                if (val) byte |= 0x80U;
                buf[i++] = byte;
            } while (val);
            return i;
        }

        std::string HsFilename(hpb::FileDefPtr file) const {
            return StripExtension(file.name()) + "_hs.hs";
        }

        std::string RawFilename(hpb::FileDefPtr file) const {
            return StripExtension(file.name());
        }


    private:
        bool bootstrap;
    };

}  // namespace hpbc

#endif  // HPB_HS_HPB_H_
