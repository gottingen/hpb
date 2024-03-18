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

#include "hpbc/c_hpb.h"

namespace hpbc {

    const char* Chpb::kEnumsInit = "enums_layout";
    const char* Chpb::kExtensionsInit = "extensions_layout";
    const char* Chpb::kMessagesInit = "messages_layout";

    void Chpb::GenerateFile(const DefPoolPair& pools, hpb::FileDefPtr file,Plugin* plugin) {
        Output h_output;
        WriteHeader(pools, file, h_output);
        plugin->AddOutputFile(HeaderFilename(file), h_output.output());

        Output c_output;
        WriteSource(pools, file, c_output);
        plugin->AddOutputFile(SourceFilename(file), c_output.output());
    }

    void Chpb::WriteHeader(const DefPoolPair& pools, hpb::FileDefPtr file,Output& output) {
        EmitFileWarning(file.name(), output);
        output(
                "#ifndef $0_HPB_H_\n"
                "#define $0_HPB_H_\n\n"
                "#include \"hpb/generated_code_support.h\"\n",
                ToPreproc(file.name()));

        for (int i = 0; i < file.public_dependency_count(); i++) {
            if (i == 0) {
                output("/* Public Imports. */\n");
            }
            output("#include \"$0\"\n", HeaderFilename(file.public_dependency(i)));
            if (i == file.public_dependency_count() - 1) {
                output("\n");
            }
        }

        output(
                "// Must be last. \n"
                "#include \"hpb/port/def.inc\"\n"
                "\n"
                "#ifdef __cplusplus\n"
                "extern \"C\" {\n"
                "#endif\n"
                "\n");

        const std::vector<hpb::MessageDefPtr> this_file_messages =
                SortedMessages(file);
        const std::vector<hpb::FieldDefPtr> this_file_exts = SortedExtensions(file);

        // Forward-declare types defined in this file.
        for (auto message : this_file_messages) {
            output("typedef struct $0 $0;\n", ToCIdent(message.full_name()));
        }
        for (auto message : this_file_messages) {
            ForwardDeclareMiniTableInit(message, output);
        }
        for (auto ext : this_file_exts) {
            output("extern const hpb_MiniTableExtension $0;\n", ExtensionLayout(ext));
        }

        // Forward-declare types not in this file, but used as submessages.
        // Order by full name for consistent ordering.
        std::map<std::string, hpb::MessageDefPtr> forward_messages;

        for (auto message : this_file_messages) {
            for (int i = 0; i < message.field_count(); i++) {
                hpb::FieldDefPtr field = message.field(i);
                if (field.ctype() == kHpb_CType_Message &&
                    field.file() != field.message_type().file()) {
                    forward_messages[field.message_type().full_name()] =
                            field.message_type();
                }
            }
        }
        for (auto ext : this_file_exts) {
            if (ext.file() != ext.containing_type().file()) {
                forward_messages[ext.containing_type().full_name()] =
                        ext.containing_type();
            }
        }
        for (const auto& pair : forward_messages) {
            output("struct $0;\n", MessageName(pair.second));
        }
        for (const auto& pair : forward_messages) {
            ForwardDeclareMiniTableInit(pair.second, output);
        }

        if (!this_file_messages.empty()) {
            output("\n");
        }

        std::vector<hpb::EnumDefPtr> this_file_enums = SortedEnums(file);

        for (auto enumdesc : this_file_enums) {
            output("typedef enum {\n");
            DumpEnumValues(enumdesc, output);
            output("} $0;\n\n", ToCIdent(enumdesc.full_name()));
        }

        output("\n");

        if (file.syntax() == kHpb_Syntax_Proto2) {
            for (const auto enumdesc : this_file_enums) {
                if (bootstrap) {
                    output("extern const hpb_MiniTableEnum* $0();\n", EnumInit(enumdesc));
                } else {
                    output("extern const hpb_MiniTableEnum $0;\n", EnumInit(enumdesc));
                }
            }
        }

        output("\n");
        for (auto message : this_file_messages) {
            GenerateMessageInHeader(message, pools, output);
        }

        for (auto ext : this_file_exts) {
            GenerateExtensionInHeader(pools, ext, output);
        }

        output("extern const hpb_MiniTableFile $0;\n\n", FileLayoutName(file));

        if (absl::string_view(file.name()) == "google/protobuf/descriptor.proto" ||
            absl::string_view(file.name()) == "net/proto2/proto/descriptor.proto") {
            // This is gratuitously inefficient with how many times it rebuilds
            // MessageLayout objects for the same message. But we only do this for one
            // proto (descriptor.proto) so we don't worry about it.
            hpb::MessageDefPtr max32_message;
            hpb::MessageDefPtr max64_message;
            size_t max32 = 0;
            size_t max64 = 0;
            for (const auto message : this_file_messages) {
                if (absl::EndsWith(message.name(), "Options")) {
                    size_t size32 = pools.GetMiniTable32(message)->size;
                    size_t size64 = pools.GetMiniTable64(message)->size;
                    if (size32 > max32) {
                        max32 = size32;
                        max32_message = message;
                    }
                    if (size64 > max64) {
                        max64 = size64;
                        max64_message = message;
                    }
                }
            }

            output("/* Max size 32 is $0 */\n", max32_message.full_name());
            output("/* Max size 64 is $0 */\n", max64_message.full_name());
            output("#define _HPB_MAXOPT_SIZE HPB_SIZE($0, $1)\n\n", max32, max64);
        }

        output(
                "#ifdef __cplusplus\n"
                "}  /* extern \"C\" */\n"
                "#endif\n"
                "\n"
                "#include \"hpb/port/undef.inc\"\n"
                "\n"
                "#endif  /* $0_HPB_H_ */\n",
                ToPreproc(file.name()));
    }

    void Chpb::ForwardDeclareMiniTableInit(hpb::MessageDefPtr message, Output& output) const {
        if (bootstrap) {
            output("extern const hpb_MiniTable* $0();\n", MessageInitName(message));
        } else {
            output("extern const hpb_MiniTable $0;\n", MessageInitName(message));
        }
    }

    std::string Chpb::MessageInitName(hpb::MessageDefPtr descriptor) const {
        return absl::StrCat(MessageName(descriptor), "_msg_init");
    }

    std::string Chpb::ExtensionLayout(hpb::FieldDefPtr ext) const {
        return absl::StrCat(ExtensionIdentBase(ext), "_", ext.name(), "_ext");
    }

    std::string Chpb::ExtensionIdentBase(hpb::FieldDefPtr ext) const {
        assert(ext.is_extension());
        std::string ext_scope;
        if (ext.extension_scope()) {
            return MessageName(ext.extension_scope());
        } else {
            return ToCIdent(ext.file().package());
        }
    }

    void Chpb::DumpEnumValues(hpb::EnumDefPtr desc, Output& output) const {
        std::vector<hpb::EnumValDefPtr> values;
        values.reserve(desc.value_count());
        for (int i = 0; i < desc.value_count(); i++) {
            values.push_back(desc.value(i));
        }
        std::sort(values.begin(), values.end(),
                  [](hpb::EnumValDefPtr a, hpb::EnumValDefPtr b) {
                      return a.number() < b.number();
                  });

        for (size_t i = 0; i < values.size(); i++) {
            auto value = values[i];
            output("  $0 = $1", EnumValueSymbol(value), value.number());
            if (i != values.size() - 1) {
                output(",");
            }
            output("\n");
        }
    }

    std::string Chpb::EnumValueSymbol(hpb::EnumValDefPtr value) const {
        return ToCIdent(value.full_name());
    }

    void Chpb::GenerateMessageInHeader(hpb::MessageDefPtr message,
                                 const DefPoolPair& pools,
                                 Output& output) const {
        output("/* $0 */\n\n", message.full_name());
        std::string msg_name = ToCIdent(message.full_name());
        if (!HPB_DESC(MessageOptions_map_entry)(message.options())) {
            GenerateMessageFunctionsInHeader(message, output);
        }

        for (int i = 0; i < message.real_oneof_count(); i++) {
            GenerateOneofInHeader(message.oneof(i), pools, msg_name, output);
        }

        auto field_names = CreateFieldNameMap(message);
        for (auto field : FieldNumberOrder(message)) {
            GenerateClear(field, pools, msg_name, field_names, output);
            GenerateGetters(field, pools, msg_name, field_names, output);
            GenerateHazzer(field, pools, msg_name, field_names, output);
        }

        output("\n");

        for (auto field : FieldNumberOrder(message)) {
            GenerateSetters(field, pools, msg_name, field_names, output);
        }

        output("\n");
    }


    void Chpb::GenerateMessageFunctionsInHeader(hpb::MessageDefPtr message,Output& output) const {
        // TODO(b/235839510): The generated code here does not check the return values
        // from hpb_Encode(). How can we even fix this without breaking other things?
        output(
                R"cc(
        HPB_INLINE $0* $0_new(hpb_Arena* arena) {
          return ($0*)_hpb_Message_New($1, arena);
        }
        HPB_INLINE $0* $0_parse(const char* buf, size_t size, hpb_Arena* arena) {
          $0* ret = $0_new(arena);
          if (!ret) return NULL;
          if (hpb_Decode(buf, size, ret, $1, NULL, 0, arena) != kHpb_DecodeStatus_Ok) {
            return NULL;
          }
          return ret;
        }
        HPB_INLINE $0* $0_parse_ex(const char* buf, size_t size,
                                   const hpb_ExtensionRegistry* extreg,
                                   int options, hpb_Arena* arena) {
          $0* ret = $0_new(arena);
          if (!ret) return NULL;
          if (hpb_Decode(buf, size, ret, $1, extreg, options, arena) !=
              kHpb_DecodeStatus_Ok) {
            return NULL;
          }
          return ret;
        }
        HPB_INLINE char* $0_serialize(const $0* msg, hpb_Arena* arena, size_t* len) {
          char* ptr;
          (void)hpb_Encode(msg, $1, 0, arena, &ptr, len);
          return ptr;
        }
        HPB_INLINE char* $0_serialize_ex(const $0* msg, int options,
                                         hpb_Arena* arena, size_t* len) {
          char* ptr;
          (void)hpb_Encode(msg, $1, options, arena, &ptr, len);
          return ptr;
        }
      )cc",
                MessageName(message), MessageMiniTableRef(message));
    }

    std::string Chpb::MessageMiniTableRef(hpb::MessageDefPtr descriptor) const {
        if (bootstrap) {
            return absl::StrCat(MessageInitName(descriptor), "()");
        } else {
            return absl::StrCat("&", MessageInitName(descriptor));
        }
    }


    void Chpb::GenerateOneofInHeader(hpb::OneofDefPtr oneof, const DefPoolPair& pools, absl::string_view msg_name, Output& output) const {
        std::string fullname = ToCIdent(oneof.full_name());
        output("typedef enum {\n");
        for (int j = 0; j < oneof.field_count(); j++) {
            hpb::FieldDefPtr field = oneof.field(j);
            output("  $0_$1 = $2,\n", fullname, field.name(), field.number());
        }
        output(
                "  $0_NOT_SET = 0\n"
                "} $0_oneofcases;\n",
                fullname);
        output(
                R"cc(
        HPB_INLINE $0_oneofcases $1_$2_case(const $1* msg) {
          const hpb_MiniTableField field = $3;
          return ($0_oneofcases)hpb_Message_WhichOneofFieldNumber(msg, &field);
        }
      )cc",
                fullname, msg_name, oneof.name(),
                FieldInitializer(pools, oneof.field(0)));
    }

    std::string Chpb::FieldInitializer(hpb::FieldDefPtr field,
                                 const hpb_MiniTableField* field64,
                                 const hpb_MiniTableField* field32) const {
        if (bootstrap) {
            ABSL_CHECK(!field.is_extension());
            return absl::Substitute(
                    "*hpb_MiniTable_FindFieldByNumber($0, $1)",
                    MessageMiniTableRef(field.containing_type()), field.number());
        } else {
            return absl::Substitute(
                    "{$0, $1, $2, $3, $4, $5}", field64->number,
                    ArchDependentSize(field32->offset, field64->offset),
                    ArchDependentSize(field32->presence, field64->presence),
                    field64->HPB_PRIVATE(submsg_index) == kHpb_NoSub
                    ? "kHpb_NoSub"
                    : absl::StrCat(field64->HPB_PRIVATE(submsg_index)).c_str(),
                    field64->HPB_PRIVATE(descriptortype), GetModeInit(field32, field64));
        }
    }

    std::string Chpb::FieldInitializer(const DefPoolPair& pools, hpb::FieldDefPtr field) const {
        return FieldInitializer(field, pools.GetField64(field),
                                pools.GetField32(field));
    }

    std::string Chpb::ArchDependentSize(int64_t size32, int64_t size64) const {
        if (size32 == size64) return absl::StrCat(size32);
        return absl::Substitute("HPB_SIZE($0, $1)", size32, size64);
    }

    std::string Chpb::GetModeInit(const hpb_MiniTableField* field32,
                            const hpb_MiniTableField* field64) const {
        std::string ret;
        uint8_t mode32 = field32->mode;
        switch (mode32 & kHpb_FieldMode_Mask) {
            case kHpb_FieldMode_Map:
                ret = "(int)kHpb_FieldMode_Map";
                break;
            case kHpb_FieldMode_Array:
                ret = "(int)kHpb_FieldMode_Array";
                break;
            case kHpb_FieldMode_Scalar:
                ret = "(int)kHpb_FieldMode_Scalar";
                break;
            default:
                break;
        }

        if (mode32 & kHpb_LabelFlags_IsPacked) {
            absl::StrAppend(&ret, " | (int)kHpb_LabelFlags_IsPacked");
        }

        if (mode32 & kHpb_LabelFlags_IsExtension) {
            absl::StrAppend(&ret, " | (int)kHpb_LabelFlags_IsExtension");
        }

        if (mode32 & kHpb_LabelFlags_IsAlternate) {
            absl::StrAppend(&ret, " | (int)kHpb_LabelFlags_IsAlternate");
        }

        absl::StrAppend(&ret, " | ((int)", GetFieldRep(field32, field64),
                        " << kHpb_FieldRep_Shift)");
        return ret;
    }


    std::string Chpb::GetFieldRep(const hpb_MiniTableField* field32,
                            const hpb_MiniTableField* field64) const {
        switch (_hpb_MiniTableField_GetRep(field32)) {
            case kHpb_FieldRep_1Byte:
                return "kHpb_FieldRep_1Byte";
                break;
            case kHpb_FieldRep_4Byte: {
                if (_hpb_MiniTableField_GetRep(field64) == kHpb_FieldRep_4Byte) {
                    return "kHpb_FieldRep_4Byte";
                } else {
                    assert(_hpb_MiniTableField_GetRep(field64) == kHpb_FieldRep_8Byte);
                    return "HPB_SIZE(kHpb_FieldRep_4Byte, kHpb_FieldRep_8Byte)";
                }
                break;
            }
            case kHpb_FieldRep_StringView:
                return "kHpb_FieldRep_StringView";
                break;
            case kHpb_FieldRep_8Byte:
                return "kHpb_FieldRep_8Byte";
                break;
        }
        HPB_UNREACHABLE();
    }


    void Chpb::GenerateExtensionInHeader(const DefPoolPair& pools, hpb::FieldDefPtr ext,
                                   Output& output) const {
        output(
                R"cc(
        HPB_INLINE bool $0_has_$1(const struct $2* msg) {
          return _hpb_Message_HasExtensionField(msg, &$3);
        }
      )cc",
                ExtensionIdentBase(ext), ext.name(), MessageName(ext.containing_type()),
                ExtensionLayout(ext));

        output(
                R"cc(
        HPB_INLINE void $0_clear_$1(struct $2* msg) {
          _hpb_Message_ClearExtensionField(msg, &$3);
        }
      )cc",
                ExtensionIdentBase(ext), ext.name(), MessageName(ext.containing_type()),
                ExtensionLayout(ext));

        if (ext.IsSequence()) {
            // TODO(b/259861668): We need generated accessors for repeated extensions.
        } else {
            output(
                    R"cc(
          HPB_INLINE $0 $1_$2(const struct $3* msg) {
            const hpb_MiniTableExtension* ext = &$4;
            HPB_ASSUME(!hpb_IsRepeatedOrMap(&ext->field));
            HPB_ASSUME(_hpb_MiniTableField_GetRep(&ext->field) == $5);
            $0 default_val = $6;
            $0 ret;
            _hpb_Message_GetExtensionField(msg, ext, &default_val, &ret);
            return ret;
          }
        )cc",
                    CTypeConst(ext), ExtensionIdentBase(ext), ext.name(),
                    MessageName(ext.containing_type()), ExtensionLayout(ext),
                    GetFieldRep(pools, ext), FieldDefault(ext));
            output(
                    R"cc(
          HPB_INLINE void $1_set_$2(struct $3* msg, $0 val, hpb_Arena* arena) {
            const hpb_MiniTableExtension* ext = &$4;
            HPB_ASSUME(!hpb_IsRepeatedOrMap(&ext->field));
            HPB_ASSUME(_hpb_MiniTableField_GetRep(&ext->field) == $5);
            bool ok = _hpb_Message_SetExtensionField(msg, ext, &val, arena);
            HPB_ASSERT(ok);
          }
        )cc",
                    CTypeConst(ext), ExtensionIdentBase(ext), ext.name(),
                    MessageName(ext.containing_type()), ExtensionLayout(ext),
                    GetFieldRep(pools, ext));
        }
    }

    std::string Chpb::CTypeConst(hpb::FieldDefPtr field) const {
        return CTypeInternal(field, true);
    }

    std::string Chpb::CTypeInternal(hpb::FieldDefPtr field, bool is_const) const {
        std::string maybe_const = is_const ? "const " : "";
        switch (field.ctype()) {
            case kHpb_CType_Message: {
                std::string maybe_struct =
                        field.file() != field.message_type().file() ? "struct " : "";
                return maybe_const + maybe_struct + MessageName(field.message_type()) +
                       "*";
            }
            case kHpb_CType_Bool:
                return "bool";
            case kHpb_CType_Float:
                return "float";
            case kHpb_CType_Int32:
            case kHpb_CType_Enum:
                return "int32_t";
            case kHpb_CType_UInt32:
                return "uint32_t";
            case kHpb_CType_Double:
                return "double";
            case kHpb_CType_Int64:
                return "int64_t";
            case kHpb_CType_UInt64:
                return "uint64_t";
            case kHpb_CType_String:
            case kHpb_CType_Bytes:
                return "hpb_StringView";
            default:
                abort();
        }
    }


    std::string Chpb::FieldDefault(hpb::FieldDefPtr field) const {
        switch (field.ctype()) {
            case kHpb_CType_Message:
                return "NULL";
            case kHpb_CType_Bytes:
            case kHpb_CType_String: {
                hpb_StringView str = field.default_value().str_val;
                return absl::Substitute(
                        "hpb_StringView_FromString(\"$0\")",
                        absl::CEscape(absl::string_view(str.data, str.size)));
            }
            case kHpb_CType_Int32:
                return absl::Substitute("(int32_t)$0", field.default_value().int32_val);
            case kHpb_CType_Int64:
                if (field.default_value().int64_val == INT64_MIN) {
                    // Special-case to avoid:
                    //   integer literal is too large to be represented in a signed integer
                    //   type, interpreting as unsigned
                    //   [-Werror,-Wimplicitly-unsigned-literal]
                    //   int64_t default_val = (int64_t)-9223372036854775808ll;
                    //
                    // More info here: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52661
                    return "INT64_MIN";
                } else {
                    return absl::Substitute("(int64_t)$0ll",
                                            field.default_value().int64_val);
                }
            case kHpb_CType_UInt32:
                return absl::Substitute("(uint32_t)$0u",
                                        field.default_value().uint32_val);
            case kHpb_CType_UInt64:
                return absl::Substitute("(uint64_t)$0ull",
                                        field.default_value().uint64_val);
            case kHpb_CType_Float:
                return FloatToCLiteral(field.default_value().float_val);
            case kHpb_CType_Double:
                return DoubleToCLiteral(field.default_value().double_val);
            case kHpb_CType_Bool:
                return field.default_value().bool_val ? "true" : "false";
            case kHpb_CType_Enum:
                // Use a number instead of a symbolic name so that we don't require
                // this enum's header to be included.
                return absl::StrCat(field.default_value().int32_val);
        }
        ABSL_ASSERT(false);
        return "XXX";
    }

    std::string Chpb::FloatToCLiteral(float value) const {
        if (value == std::numeric_limits<float>::infinity()) {
            return "kHpb_FltInfinity";
        } else if (value == -std::numeric_limits<float>::infinity()) {
            return "-kHpb_FltInfinity";
        } else if (std::isnan(value)) {
            return "kHpb_NaN";
        } else {
            return absl::StrCat(value);
        }
    }

    std::string Chpb::DoubleToCLiteral(double value) const {
        if (value == std::numeric_limits<double>::infinity()) {
            return "kHpb_Infinity";
        } else if (value == -std::numeric_limits<double>::infinity()) {
            return "-kHpb_Infinity";
        } else if (std::isnan(value)) {
            return "kHpb_NaN";
        } else {
            return absl::StrCat(value);
        }
    }

    void Chpb::GenerateClear(hpb::FieldDefPtr field, const DefPoolPair& pools,absl::string_view msg_name,
                       const NameToFieldDefMap& field_names,
                       Output& output) const {
        if (field == field.containing_type().map_key() ||
            field == field.containing_type().map_value()) {
            // Cannot be cleared.
            return;
        }
        std::string resolved_name = ResolveFieldName(field, field_names);
        output(
                R"cc(
        HPB_INLINE void $0_clear_$1($0* msg) {
          const hpb_MiniTableField field = $2;
          _hpb_Message_ClearNonExtensionField(msg, &field);
        }
      )cc",
                msg_name, resolved_name, FieldInitializer(pools, field));
    }

    void Chpb::GenerateGetters(hpb::FieldDefPtr field, const DefPoolPair& pools,
                         absl::string_view msg_name,
                         const NameToFieldDefMap& field_names,Output& output) const {
        if (field.IsMap()) {
            GenerateMapGetters(field, pools, msg_name, field_names, output);
        } else if (HPB_DESC(MessageOptions_map_entry)(
                field.containing_type().options())) {
            GenerateMapEntryGetters(field, msg_name, output);
        } else if (field.IsSequence()) {
            GenerateRepeatedGetters(field, pools, msg_name, field_names,output);
        } else {
            GenerateScalarGetters(field, pools, msg_name, field_names, output);
        }
    }

    void Chpb::GenerateMapGetters(hpb::FieldDefPtr field, const DefPoolPair& pools,
                            absl::string_view msg_name,
                            const NameToFieldDefMap& field_names, Output& output) const {
        std::string resolved_name = ResolveFieldName(field, field_names);
        output(
                R"cc(
        HPB_INLINE size_t $0_$1_size(const $0* msg) {
          const hpb_MiniTableField field = $2;
          const hpb_Map* map = hpb_Message_GetMap(msg, &field);
          return map ? _hpb_Map_Size(map) : 0;
        }
      )cc",
                msg_name, resolved_name, FieldInitializer(pools, field));
        output(
                R"cc(
        HPB_INLINE bool $0_$1_get(const $0* msg, $2 key, $3* val) {
          const hpb_MiniTableField field = $4;
          const hpb_Map* map = hpb_Message_GetMap(msg, &field);
          if (!map) return false;
          return _hpb_Map_Get(map, &key, $5, val, $6);
        }
      )cc",
                msg_name, resolved_name, MapKeyCType(field), MapValueCType(field),
                FieldInitializer(pools, field), MapKeySize(field, "key"),
                MapValueSize(field, "*val"));
        output(
                R"cc(
        HPB_INLINE $0 $1_$2_next(const $1* msg, size_t* iter) {
          const hpb_MiniTableField field = $3;
          const hpb_Map* map = hpb_Message_GetMap(msg, &field);
          if (!map) return NULL;
          return ($0)_hpb_map_next(map, iter);
        }
      )cc",
                CTypeConst(field), msg_name, resolved_name,
                FieldInitializer(pools, field));
    }

    std::string Chpb::CType(hpb::FieldDefPtr field) const {
        return CTypeInternal(field, false);
    }


    std::string Chpb::MapKeyCType(hpb::FieldDefPtr map_field) const {
        return CType(map_field.message_type().map_key());
    }


    std::string Chpb::MapValueCType(hpb::FieldDefPtr map_field) const {
        return CType(map_field.message_type().map_value());
    }

    std::string Chpb::MapKeySize(hpb::FieldDefPtr map_field, absl::string_view expr) const {
        return map_field.message_type().map_key().ctype() == kHpb_CType_String
               ? "0"
               : absl::StrCat("sizeof(", expr, ")");
    }
    std::string Chpb::MapValueSize(hpb::FieldDefPtr map_field, absl::string_view expr) const {
        return map_field.message_type().map_value().ctype() == kHpb_CType_String
               ? "0"
               : absl::StrCat("sizeof(", expr, ")");
    }

    void Chpb::GenerateMapEntryGetters(hpb::FieldDefPtr field, absl::string_view msg_name,
                                 Output& output) const {
        output(
                R"cc(
        HPB_INLINE $0 $1_$2(const $1* msg) {
          $3 ret;
          _hpb_msg_map_$2(msg, &ret, $4);
          return ret;
        }
      )cc",
                CTypeConst(field), msg_name, field.name(), CType(field),
                field.ctype() == kHpb_CType_String ? "0" : "sizeof(ret)");
    }


    void Chpb::GenerateRepeatedGetters(hpb::FieldDefPtr field, const DefPoolPair& pools,
                                 absl::string_view msg_name,
                                 const NameToFieldDefMap& field_names, Output& output) const {
        // Generate getter returning first item and size.
        //
        // Example:
        //   HPB_INLINE const struct Bar* const* name(const Foo* msg, size_t* size)
        output(
                R"cc(
        HPB_INLINE $0 const* $1_$2(const $1* msg, size_t* size) {
          const hpb_MiniTableField field = $3;
          const hpb_Array* arr = hpb_Message_GetArray(msg, &field);
          if (arr) {
            if (size) *size = arr->size;
            return ($0 const*)_hpb_array_constptr(arr);
          } else {
            if (size) *size = 0;
            return NULL;
          }
        }
      )cc",
                CTypeConst(field),                       // $0
                msg_name,                                // $1
                ResolveFieldName(field, field_names),    // $2
                FieldInitializer(pools, field)  // #3
        );
        // Generate private getter returning array or NULL for immutable and hpb_Array
        // for mutable.
        //
        // Example:
        //   HPB_INLINE const hpb_Array* _name_hpbarray(size_t* size)
        //   HPB_INLINE hpb_Array* _name_mutable_hpbarray(size_t* size)
        output(
                R"cc(
        HPB_INLINE const hpb_Array* _$1_$2_$4(const $1* msg, size_t* size) {
          const hpb_MiniTableField field = $3;
          const hpb_Array* arr = hpb_Message_GetArray(msg, &field);
          if (size) {
            *size = arr ? arr->size : 0;
          }
          return arr;
        }
        HPB_INLINE hpb_Array* _$1_$2_$5(const $1* msg, size_t* size, hpb_Arena* arena) {
          const hpb_MiniTableField field = $3;
          hpb_Array* arr = hpb_Message_GetOrCreateMutableArray(
              (hpb_Message*)msg, &field, arena);
          if (size) {
            *size = arr ? arr->size : 0;
          }
          return arr;
        }
      )cc",
                CTypeConst(field),                        // $0
                msg_name,                                 // $1
                ResolveFieldName(field, field_names),     // $2
                FieldInitializer(pools, field),  // $3
                kRepeatedFieldArrayGetterPostfix,         // $4
                kRepeatedFieldMutableArrayGetterPostfix   // $5
        );
    }

    void Chpb::GenerateScalarGetters(hpb::FieldDefPtr field, const DefPoolPair& pools,
                               absl::string_view msg_name,
                               const NameToFieldDefMap& field_names,Output& output) const {
        std::string field_name = ResolveFieldName(field, field_names);
        output(
                R"cc(
        HPB_INLINE $0 $1_$2(const $1* msg) {
          $0 default_val = $3;
          $0 ret;
          const hpb_MiniTableField field = $4;
          _hpb_Message_GetNonExtensionField(msg, &field, &default_val, &ret);
          return ret;
        }
      )cc",
                CTypeConst(field), msg_name, field_name, FieldDefault(field),
                FieldInitializer(pools, field));
    }

    void Chpb::GenerateHazzer(hpb::FieldDefPtr field, const DefPoolPair& pools,
                        absl::string_view msg_name,
                        const NameToFieldDefMap& field_names, Output& output) const {
        std::string resolved_name = ResolveFieldName(field, field_names);
        if (field.has_presence()) {
            output(
                    R"cc(
          HPB_INLINE bool $0_has_$1(const $0* msg) {
            const hpb_MiniTableField field = $2;
            return _hpb_Message_HasNonExtensionField(msg, &field);
          }
        )cc",
                    msg_name, resolved_name, FieldInitializer(pools, field));
        } else if (field.IsMap()) {
            // Do nothing.
        } else if (field.IsSequence()) {
            // TODO(b/259616267): remove.
            output(
                    R"cc(
          HPB_INLINE bool $0_has_$1(const $0* msg) {
            size_t size;
            $0_$1(msg, &size);
            return size != 0;
          }
        )cc",
                    msg_name, resolved_name);
        }
    }

    void Chpb::GenerateSetters(hpb::FieldDefPtr field, const DefPoolPair& pools,
                         absl::string_view msg_name,
                         const NameToFieldDefMap& field_names,Output& output) const {
        if (field.IsMap()) {
            GenerateMapSetters(field, pools, msg_name, field_names, output);
        } else if (field.IsSequence()) {
            GenerateRepeatedSetters(field, pools, msg_name, field_names,
                                    output);
        } else {
            GenerateNonRepeatedSetters(field, pools, msg_name, field_names,output);
        }
    }


    void Chpb::GenerateMapSetters(hpb::FieldDefPtr field, const DefPoolPair& pools,
                            absl::string_view msg_name,
                            const NameToFieldDefMap& field_names,Output& output) const {
        std::string resolved_name = ResolveFieldName(field, field_names);
        output(
                R"cc(
        HPB_INLINE void $0_$1_clear($0* msg) {
          const hpb_MiniTableField field = $2;
          hpb_Map* map = (hpb_Map*)hpb_Message_GetMap(msg, &field);
          if (!map) return;
          _hpb_Map_Clear(map);
        }
      )cc",
                msg_name, resolved_name, FieldInitializer(pools, field));
        output(
                R"cc(
        HPB_INLINE bool $0_$1_set($0* msg, $2 key, $3 val, hpb_Arena* a) {
          const hpb_MiniTableField field = $4;
          hpb_Map* map = _hpb_Message_GetOrCreateMutableMap(msg, &field, $5, $6, a);
          return _hpb_Map_Insert(map, &key, $5, &val, $6, a) !=
                 kHpb_MapInsertStatus_OutOfMemory;
        }
      )cc",
                msg_name, resolved_name, MapKeyCType(field), MapValueCType(field),
                FieldInitializer(pools, field), MapKeySize(field, "key"),
                MapValueSize(field, "val"));
        output(
                R"cc(
        HPB_INLINE bool $0_$1_delete($0* msg, $2 key) {
          const hpb_MiniTableField field = $3;
          hpb_Map* map = (hpb_Map*)hpb_Message_GetMap(msg, &field);
          if (!map) return false;
          return _hpb_Map_Delete(map, &key, $4, NULL);
        }
      )cc",
                msg_name, resolved_name, MapKeyCType(field),
                FieldInitializer(pools, field), MapKeySize(field, "key"));
        output(
                R"cc(
        HPB_INLINE $0 $1_$2_nextmutable($1* msg, size_t* iter) {
          const hpb_MiniTableField field = $3;
          hpb_Map* map = (hpb_Map*)hpb_Message_GetMap(msg, &field);
          if (!map) return NULL;
          return ($0)_hpb_map_next(map, iter);
        }
      )cc",
                CType(field), msg_name, resolved_name,
                FieldInitializer(pools, field));
    }


    void Chpb::GenerateRepeatedSetters(hpb::FieldDefPtr field, const DefPoolPair& pools,
                                 absl::string_view msg_name,
                                 const NameToFieldDefMap& field_names,Output& output) const {
        std::string resolved_name = ResolveFieldName(field, field_names);
        output(
                R"cc(
        HPB_INLINE $0* $1_mutable_$2($1* msg, size_t* size) {
          hpb_MiniTableField field = $3;
          hpb_Array* arr = hpb_Message_GetMutableArray(msg, &field);
          if (arr) {
            if (size) *size = arr->size;
            return ($0*)_hpb_array_ptr(arr);
          } else {
            if (size) *size = 0;
            return NULL;
          }
        }
      )cc",
                CType(field), msg_name, resolved_name,
                FieldInitializer(pools, field));
        output(
                R"cc(
        HPB_INLINE $0* $1_resize_$2($1* msg, size_t size, hpb_Arena* arena) {
          hpb_MiniTableField field = $3;
          return ($0*)hpb_Message_ResizeArrayUninitialized(msg, &field, size, arena);
        }
      )cc",
                CType(field), msg_name, resolved_name,
                FieldInitializer(pools, field));
        if (field.ctype() == kHpb_CType_Message) {
            output(
                    R"cc(
          HPB_INLINE struct $0* $1_add_$2($1* msg, hpb_Arena* arena) {
            hpb_MiniTableField field = $4;
            hpb_Array* arr = hpb_Message_GetOrCreateMutableArray(msg, &field, arena);
            if (!arr || !_hpb_Array_ResizeUninitialized(arr, arr->size + 1, arena)) {
              return NULL;
            }
            struct $0* sub = (struct $0*)_hpb_Message_New($3, arena);
            if (!arr || !sub) return NULL;
            _hpb_Array_Set(arr, arr->size - 1, &sub, sizeof(sub));
            return sub;
          }
        )cc",
                    MessageName(field.message_type()), msg_name, resolved_name,
                    MessageMiniTableRef(field.message_type()),
                    FieldInitializer(pools, field));
        } else {
            output(
                    R"cc(
          HPB_INLINE bool $1_add_$2($1* msg, $0 val, hpb_Arena* arena) {
            hpb_MiniTableField field = $3;
            hpb_Array* arr = hpb_Message_GetOrCreateMutableArray(msg, &field, arena);
            if (!arr || !_hpb_Array_ResizeUninitialized(arr, arr->size + 1, arena)) {
              return false;
            }
            _hpb_Array_Set(arr, arr->size - 1, &val, sizeof(val));
            return true;
          }
        )cc",
                    CType(field), msg_name, resolved_name,
                    FieldInitializer(pools, field));
        }
    }


    void Chpb::GenerateNonRepeatedSetters(hpb::FieldDefPtr field,
                                    const DefPoolPair& pools,
                                    absl::string_view msg_name,
                                    const NameToFieldDefMap& field_names,Output& output) const {
        if (field == field.containing_type().map_key()) {
            // Key cannot be mutated.
            return;
        }

        std::string field_name = ResolveFieldName(field, field_names);

        if (field == field.containing_type().map_value()) {
            output(R"cc(
             HPB_INLINE void $0_set_$1($0 *msg, $2 value) {
               _hpb_msg_map_set_value(msg, &value, $3);
             }
           )cc",
                   msg_name, field_name, CType(field),
                   field.ctype() == kHpb_CType_String ? "0"
                                                      : "sizeof(" + CType(field) + ")");
        } else {
            output(R"cc(
             HPB_INLINE void $0_set_$1($0 *msg, $2 value) {
               const hpb_MiniTableField field = $3;
               _hpb_Message_SetNonExtensionField(msg, &field, &value);
             }
           )cc",
                   msg_name, field_name, CType(field),
                   FieldInitializer(pools, field));
        }

        // Message fields also have a Msg_mutable_foo() accessor that will create
        // the sub-message if it doesn't already exist.
        if (field.ctype() == kHpb_CType_Message &&
            !HPB_DESC(MessageOptions_map_entry)(field.containing_type().options())) {
            output(
                    R"cc(
          HPB_INLINE struct $0* $1_mutable_$2($1* msg, hpb_Arena* arena) {
            struct $0* sub = (struct $0*)$1_$2(msg);
            if (sub == NULL) {
              sub = (struct $0*)_hpb_Message_New($3, arena);
              if (sub) $1_set_$2(msg, sub);
            }
            return sub;
          }
        )cc",
                    MessageName(field.message_type()), msg_name, field_name,
                    MessageMiniTableRef(field.message_type()));
        }
    }

    void Chpb::WriteSource(const DefPoolPair& pools, hpb::FileDefPtr file, Output& output) const {
        if (bootstrap) {
            WriteMiniDescriptorSource(pools, file, output);
        } else {
            WriteMiniTableSource(pools, file, output);
        }
    }


    void Chpb::WriteMiniDescriptorSource(const DefPoolPair& pools, hpb::FileDefPtr file,Output& output) const {
        output(
                "#include <stddef.h>\n"
                "#include \"hpb/generated_code_support.h\"\n"
                "#include \"$0\"\n\n",
                HeaderFilename(file));

        for (int i = 0; i < file.dependency_count(); i++) {
            output("#include \"$0\"\n", HeaderFilename(file.dependency(i)));
        }

        output(
                R"cc(
        static hpb_Arena* hpb_BootstrapArena() {
          static hpb_Arena* arena = NULL;
          if (!arena) arena = hpb_Arena_New();
          return arena;
        }
      )cc");

        output("\n");

        for (const auto msg : SortedMessages(file)) {
            WriteMessageMiniDescriptorInitializer(msg, output);
        }

        for (const auto msg : SortedEnums(file)) {
            WriteEnumMiniDescriptorInitializer(msg, output);
        }
    }


    void Chpb::WriteMessageMiniDescriptorInitializer(hpb::MessageDefPtr msg,
                                               Output& output) const {
        Output resolve_calls;
        for (int i = 0; i < msg.field_count(); i++) {
            hpb::FieldDefPtr field = msg.field(i);
            if (!field.message_type() && !field.enum_subdef()) continue;
            if (field.message_type()) {
                resolve_calls(
                        "hpb_MiniTable_SetSubMessage(mini_table, "
                        "(hpb_MiniTableField*)hpb_MiniTable_FindFieldByNumber(mini_table, "
                        "$0), $1);\n  ",
                        field.number(), MessageMiniTableRef(field.message_type()));
            } else if (field.enum_subdef() && field.enum_subdef().is_closed()) {
                resolve_calls(
                        "hpb_MiniTable_SetSubEnum(mini_table, "
                        "(hpb_MiniTableField*)hpb_MiniTable_FindFieldByNumber(mini_table, "
                        "$0), $1);\n  ",
                        field.number(), EnumMiniTableRef(field.enum_subdef()));
            }
        }

        output(
                R"cc(
        const hpb_MiniTable* $0() {
          static hpb_MiniTable* mini_table = NULL;
          static const char* mini_descriptor = "$1";
          if (mini_table) return mini_table;
          mini_table =
              hpb_MiniTable_Build(mini_descriptor, strlen(mini_descriptor),
                                  hpb_BootstrapArena(), NULL);
          $2return mini_table;
        }
      )cc",
                MessageInitName(msg), msg.MiniDescriptorEncode(), resolve_calls.output());
        output("\n");
    }

    std::string Chpb::EnumMiniTableRef(hpb::EnumDefPtr descriptor) const {
        if (bootstrap) {
            return absl::StrCat(EnumInitName(descriptor), "()");
        } else {
            return absl::StrCat("&", EnumInitName(descriptor));
        }
    }

    std::string Chpb::EnumInitName(hpb::EnumDefPtr descriptor) const {
        return ToCIdent(descriptor.full_name()) + "_enum_init";
    }

    void Chpb::WriteEnumMiniDescriptorInitializer(hpb::EnumDefPtr enum_def,Output& output) const {
        output(
                R"cc(
        const hpb_MiniTableEnum* $0() {
          static const hpb_MiniTableEnum* mini_table = NULL;
          static const char* mini_descriptor = "$1";
          if (mini_table) return mini_table;
          mini_table =
              hpb_MiniTableEnum_Build(mini_descriptor, strlen(mini_descriptor),
                                      hpb_BootstrapArena(), NULL);
          return mini_table;
        }
      )cc",
                EnumInitName(enum_def), enum_def.MiniDescriptorEncode());
        output("\n");
    }


    void Chpb::WriteMiniTableSource(const DefPoolPair& pools, hpb::FileDefPtr file,Output& output) const {
        EmitFileWarning(file.name(), output);

        output(
                "#include <stddef.h>\n"
                "#include \"hpb/generated_code_support.h\"\n"
                "#include \"$0\"\n",
                HeaderFilename(file));

        for (int i = 0; i < file.dependency_count(); i++) {
            output("#include \"$0\"\n", HeaderFilename(file.dependency(i)));
        }

        output(
                "\n"
                "// Must be last.\n"
                "#include \"hpb/port/def.inc\"\n"
                "\n");

        int msg_count = WriteMessages(pools, file, output);
        int ext_count = WriteExtensions(pools, file, output);
        int enum_count = WriteEnums(pools, file, output);

        output("const hpb_MiniTableFile $0 = {\n", FileLayoutName(file));
        output("  $0,\n", msg_count ? kMessagesInit : "NULL");
        output("  $0,\n", enum_count ? kEnumsInit : "NULL");
        output("  $0,\n", ext_count ? kExtensionsInit : "NULL");
        output("  $0,\n", msg_count);
        output("  $0,\n", enum_count);
        output("  $0,\n", ext_count);
        output("};\n\n");

        output("#include \"hpb/port/undef.inc\"\n");
        output("\n");
    }

    int Chpb::WriteMessages(const DefPoolPair& pools, hpb::FileDefPtr file,Output& output) const {
        std::vector<hpb::MessageDefPtr> file_messages = SortedMessages(file);

        if (file_messages.empty()) return 0;

        for (auto message : file_messages) {
            WriteMessage(message, pools, output);
        }

        output("static const hpb_MiniTable *$0[$1] = {\n", kMessagesInit,
               file_messages.size());
        for (auto message : file_messages) {
            output("  &$0,\n", MessageInitName(message));
        }
        output("};\n");
        output("\n");
        return file_messages.size();
    }

    void Chpb::WriteMessage(hpb::MessageDefPtr message, const DefPoolPair& pools, Output& output) const {
        std::string msg_name = ToCIdent(message.full_name());
        std::string fields_array_ref = "NULL";
        std::string submsgs_array_ref = "NULL";
        std::string subenums_array_ref = "NULL";
        const hpb_MiniTable* mt_32 = pools.GetMiniTable32(message);
        const hpb_MiniTable* mt_64 = pools.GetMiniTable64(message);
        std::map<int, std::string> subs;

        for (int i = 0; i < mt_64->field_count; i++) {
            const hpb_MiniTableField* f = &mt_64->fields[i];
            uint32_t index = f->HPB_PRIVATE(submsg_index);
            if (index != kHpb_NoSub) {
                auto pair =
                        subs.emplace(index, GetSub(message.FindFieldByNumber(f->number)));
                ABSL_CHECK(pair.second);
            }
        }

        if (!subs.empty()) {
            std::string submsgs_array_name = msg_name + "_submsgs";
            submsgs_array_ref = "&" + submsgs_array_name + "[0]";
            output("static const hpb_MiniTableSub $0[$1] = {\n", submsgs_array_name,
                   subs.size());

            int i = 0;
            for (const auto& pair : subs) {
                ABSL_CHECK(pair.first == i++);
                output("  $0,\n", pair.second);
            }

            output("};\n\n");
        }

        if (mt_64->field_count > 0) {
            std::string fields_array_name = msg_name + "__fields";
            fields_array_ref = "&" + fields_array_name + "[0]";
            output("static const hpb_MiniTableField $0[$1] = {\n", fields_array_name,
                   mt_64->field_count);
            for (int i = 0; i < mt_64->field_count; i++) {
                WriteMessageField(message.FindFieldByNumber(mt_64->fields[i].number),
                                  &mt_64->fields[i], &mt_32->fields[i], output);
            }
            output("};\n\n");
        }

        std::vector<TableEntry> table;
        uint8_t table_mask = -1;

        table = FastDecodeTable(message, pools);

        if (table.size() > 1) {
            assert((table.size() & (table.size() - 1)) == 0);
            table_mask = (table.size() - 1) << 3;
        }

        std::string msgext = "kHpb_ExtMode_NonExtendable";

        if (message.extension_range_count()) {
            if (HPB_DESC(MessageOptions_message_set_wire_format)(message.options())) {
                msgext = "kHpb_ExtMode_IsMessageSet";
            } else {
                msgext = "kHpb_ExtMode_Extendable";
            }
        }

        output("const hpb_MiniTable $0 = {\n", MessageInitName(message));
        output("  $0,\n", submsgs_array_ref);
        output("  $0,\n", fields_array_ref);
        output("  $0, $1, $2, $3, HPB_FASTTABLE_MASK($4), $5,\n",
               ArchDependentSize(mt_32->size, mt_64->size), mt_64->field_count,
               msgext, mt_64->dense_below, table_mask, mt_64->required_count);
        if (!table.empty()) {
            output("  HPB_FASTTABLE_INIT({\n");
            for (const auto& ent : table) {
                output("    {0x$1, &$0},\n", ent.first,
                       absl::StrCat(absl::Hex(ent.second, absl::kZeroPad16)));
            }
            output("  })\n");
        }
        output("};\n\n");
    }

    std::string Chpb::GetSub(hpb::FieldDefPtr field) const {
        if (auto message_def = field.message_type()) {
            return absl::Substitute("{.submsg = &$0}", MessageInitName(message_def));
        }

        if (auto enum_def = field.enum_subdef()) {
            if (enum_def.is_closed()) {
                return absl::Substitute("{.subenum = &$0}", EnumInit(enum_def));
            }
        }

        return std::string("{.submsg = NULL}");
    }


    int Chpb::WriteExtensions(const DefPoolPair& pools, hpb::FileDefPtr file, Output& output) const {
        auto exts = SortedExtensions(file);

        if (exts.empty()) return 0;

        // Order by full name for consistent ordering.
        std::map<std::string, hpb::MessageDefPtr> forward_messages;

        for (auto ext : exts) {
            forward_messages[ext.containing_type().full_name()] = ext.containing_type();
            if (ext.message_type()) {
                forward_messages[ext.message_type().full_name()] = ext.message_type();
            }
        }

        for (const auto& decl : forward_messages) {
            ForwardDeclareMiniTableInit(decl.second, output);
        }

        for (auto ext : exts) {
            output("const hpb_MiniTableExtension $0 = {\n  ", ExtensionLayout(ext));
            WriteExtension(ext, pools, output);
            output("\n};\n");
        }

        output(
                "\n"
                "static const hpb_MiniTableExtension *$0[$1] = {\n",
                kExtensionsInit, exts.size());

        for (auto ext : exts) {
            output("  &$0,\n", ExtensionLayout(ext));
        }

        output(
                "};\n"
                "\n");
        return exts.size();
    }

    void Chpb::WriteExtension(hpb::FieldDefPtr ext, const DefPoolPair& pools,Output& output) const {
        output("$0,\n", FieldInitializer(pools, ext));
        output("  &$0,\n", MessageInitName(ext.containing_type()));
        output("  $0,\n", GetSub(ext));
    }

    int Chpb::WriteEnums(const DefPoolPair& pools, hpb::FileDefPtr file, Output& output) const {
        if (file.syntax() != kHpb_Syntax_Proto2) return 0;

        std::vector<hpb::EnumDefPtr> this_file_enums = SortedEnums(file);

        for (const auto e : this_file_enums) {
            WriteEnum(e, output);
        }

        if (!this_file_enums.empty()) {
            output("static const hpb_MiniTableEnum *$0[$1] = {\n", kEnumsInit,
                   this_file_enums.size());
            for (const auto e : this_file_enums) {
                output("  &$0,\n", EnumInit(e));
            }
            output("};\n");
            output("\n");
        }

        return this_file_enums.size();
    }

    void Chpb::WriteEnum(hpb::EnumDefPtr e, Output& output) const {
        std::string values_init = "{\n";
        const hpb_MiniTableEnum* mt = e.mini_table();
        uint32_t value_count = (mt->mask_limit / 32) + mt->value_count;
        for (uint32_t i = 0; i < value_count; i++) {
            absl::StrAppend(&values_init, "                0x", absl::Hex(mt->data[i]),
                            ",\n");
        }
        values_init += "    }";

        output(
                R"cc(
        const hpb_MiniTableEnum $0 = {
            $1,
            $2,
            $3,
        };
      )cc",
                EnumInit(e), mt->mask_limit, mt->value_count, values_init);
        output("\n");
    }

    void Chpb::WriteMessageField(hpb::FieldDefPtr field,
                           const hpb_MiniTableField* field64,
                           const hpb_MiniTableField* field32,Output& output) const {
        output("  $0,\n", FieldInitializer(field, field64, field32));
    }

    std::vector<TableEntry> Chpb::FastDecodeTable(hpb::MessageDefPtr message,
                                            const DefPoolPair& pools) const {
        std::vector<TableEntry> table;
        for (const auto field : FieldHotnessOrder(message)) {
            TableEntry ent;
            int slot = GetTableSlot(field);
            // std::cerr << "table slot: " << field->number() << ": " << slot << "\n";
            if (slot < 0) {
                // Tag can't fit in the table.
                continue;
            }
            if (!TryFillTableEntry(pools, field, ent)) {
                // Unsupported field type or offset, hasbit index, etc. doesn't fit.
                continue;
            }
            while ((size_t)slot >= table.size()) {
                size_t size = std::max(static_cast<size_t>(1), table.size() * 2);
                table.resize(size, TableEntry{"_hpb_FastDecoder_DecodeGeneric", 0});
            }
            if (table[slot].first != "_hpb_FastDecoder_DecodeGeneric") {
                // A hotter field already filled this slot.
                continue;
            }
            table[slot] = ent;
        }
        return table;
    }

    std::vector<hpb::FieldDefPtr> Chpb::FieldHotnessOrder(hpb::MessageDefPtr message) const {
        std::vector<hpb::FieldDefPtr> fields;
        size_t field_count = message.field_count();
        fields.reserve(field_count);
        for (size_t i = 0; i < field_count; i++) {
            fields.push_back(message.field(i));
        }
        std::sort(fields.begin(), fields.end(),
                  [](hpb::FieldDefPtr a, hpb::FieldDefPtr b) {
                      return std::make_pair(!a.is_required(), a.number()) <
                             std::make_pair(!b.is_required(), b.number());
                  });
        return fields;
    }

    int Chpb::GetTableSlot(hpb::FieldDefPtr field) const {
        uint64_t tag = GetEncodedTag(field);
        if (tag > 0x7fff) {
            // Tag must fit within a two-byte varint.
            return -1;
        }
        return (tag & 0xf8) >> 3;
    }

    uint64_t Chpb::GetEncodedTag(hpb::FieldDefPtr field) const {
        uint32_t wire_type = GetWireTypeForField(field);
        uint32_t unencoded_tag = MakeTag(field.number(), wire_type);
        char tag_bytes[10] = {0};
        WriteVarint32ToArray(unencoded_tag, tag_bytes);
        uint64_t encoded_tag = 0;
        memcpy(&encoded_tag, tag_bytes, sizeof(encoded_tag));
        // TODO: byte-swap for big endian.
        return encoded_tag;
    }


    uint32_t Chpb::GetWireTypeForField(hpb::FieldDefPtr field) const {
        if (field.packed()) return kHpb_WireType_Delimited;
        switch (field.type()) {
            case kHpb_FieldType_Double:
            case kHpb_FieldType_Fixed64:
            case kHpb_FieldType_SFixed64:
                return kHpb_WireType_64Bit;
            case kHpb_FieldType_Float:
            case kHpb_FieldType_Fixed32:
            case kHpb_FieldType_SFixed32:
                return kHpb_WireType_32Bit;
            case kHpb_FieldType_Int64:
            case kHpb_FieldType_UInt64:
            case kHpb_FieldType_Int32:
            case kHpb_FieldType_Bool:
            case kHpb_FieldType_UInt32:
            case kHpb_FieldType_Enum:
            case kHpb_FieldType_SInt32:
            case kHpb_FieldType_SInt64:
                return kHpb_WireType_Varint;
            case kHpb_FieldType_Group:
                return kHpb_WireType_StartGroup;
            case kHpb_FieldType_Message:
            case kHpb_FieldType_String:
            case kHpb_FieldType_Bytes:
                return kHpb_WireType_Delimited;
        }
        HPB_UNREACHABLE();
    }


    bool Chpb::TryFillTableEntry(const DefPoolPair& pools, hpb::FieldDefPtr field, TableEntry& ent) const {
        const hpb_MiniTable* mt = pools.GetMiniTable64(field.containing_type());
        const hpb_MiniTableField* mt_f =
                hpb_MiniTable_FindFieldByNumber(mt, field.number());
        std::string type = "";
        std::string cardinality = "";
        switch (hpb_MiniTableField_Type(mt_f)) {
            case kHpb_FieldType_Bool:
                type = "b1";
                break;
            case kHpb_FieldType_Enum:
                if (hpb_MiniTableField_IsClosedEnum(mt_f)) {
                    // We don't have the means to test proto2 enum fields for valid values.
                    return false;
                }
                [[fallthrough]];
            case kHpb_FieldType_Int32:
            case kHpb_FieldType_UInt32:
                type = "v4";
                break;
            case kHpb_FieldType_Int64:
            case kHpb_FieldType_UInt64:
                type = "v8";
                break;
            case kHpb_FieldType_Fixed32:
            case kHpb_FieldType_SFixed32:
            case kHpb_FieldType_Float:
                type = "f4";
                break;
            case kHpb_FieldType_Fixed64:
            case kHpb_FieldType_SFixed64:
            case kHpb_FieldType_Double:
                type = "f8";
                break;
            case kHpb_FieldType_SInt32:
                type = "z4";
                break;
            case kHpb_FieldType_SInt64:
                type = "z8";
                break;
            case kHpb_FieldType_String:
                type = "s";
                break;
            case kHpb_FieldType_Bytes:
                type = "b";
                break;
            case kHpb_FieldType_Message:
                type = "m";
                break;
            default:
                return false;  // Not supported yet.
        }

        switch (hpb_FieldMode_Get(mt_f)) {
            case kHpb_FieldMode_Map:
                return false;  // Not supported yet (ever?).
            case kHpb_FieldMode_Array:
                if (mt_f->mode & kHpb_LabelFlags_IsPacked) {
                    cardinality = "p";
                } else {
                    cardinality = "r";
                }
                break;
            case kHpb_FieldMode_Scalar:
                if (mt_f->presence < 0) {
                    cardinality = "o";
                } else {
                    cardinality = "s";
                }
                break;
        }

        uint64_t expected_tag = GetEncodedTag(field);

        // Data is:
        //
        //                  48                32                16                 0
        // |--------|--------|--------|--------|--------|--------|--------|--------|
        // |   offset (16)   |case offset (16) |presence| submsg |  exp. tag (16)  |
        // |--------|--------|--------|--------|--------|--------|--------|--------|
        //
        // - |presence| is either hasbit index or field number for oneofs.

        uint64_t data = static_cast<uint64_t>(mt_f->offset) << 48 | expected_tag;

        if (field.IsSequence()) {
            // No hasbit/oneof-related fields.
        }
        if (field.real_containing_oneof()) {
            uint64_t case_offset = ~mt_f->presence;
            if (case_offset > 0xffff || field.number() > 0xff) return false;
            data |= field.number() << 24;
            data |= case_offset << 32;
        } else {
            uint64_t hasbit_index = 63;  // No hasbit (set a high, unused bit).
            if (mt_f->presence) {
                hasbit_index = mt_f->presence;
                if (hasbit_index > 31) return false;
            }
            data |= hasbit_index << 24;
        }

        if (field.ctype() == kHpb_CType_Message) {
            uint64_t idx = mt_f->HPB_PRIVATE(submsg_index);
            if (idx > 255) return false;
            data |= idx << 16;

            std::string size_ceil = "max";
            size_t size = SIZE_MAX;
            if (field.message_type().file() == field.file()) {
                // We can only be guaranteed the size of the sub-message if it is in the
                // same file as us.  We could relax this to increase the speed of
                // cross-file sub-message parsing if we are comfortable requiring that
                // users compile all messages at the same time.
                const hpb_MiniTable* sub_mt = pools.GetMiniTable64(field.message_type());
                size = sub_mt->size + 8;
            }
            std::vector<size_t> breaks = {64, 128, 192, 256};
            for (auto brk : breaks) {
                if (size <= brk) {
                    size_ceil = std::to_string(brk);
                    break;
                }
            }
            ent.first = absl::Substitute("hpb_p$0$1_$2bt_max$3b", cardinality, type,
                                         expected_tag > 0xff ? "2" : "1", size_ceil);

        } else {
            ent.first = absl::Substitute("hpb_p$0$1_$2bt", cardinality, type,
                                         expected_tag > 0xff ? "2" : "1");
        }
        ent.second = data;
        return true;
    }
}  // namespace hpbc