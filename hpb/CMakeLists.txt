#
# Copyright 2024 The titan-search Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# base
set(HPB_SRC
        base/status.c
        mem/arena.c
        mem/alloc.c
        lex/atoi.c
        lex/round_trip.c
        lex/strtod.c
        lex/unicode.c
        hash/common.c
        io/chunked_input_stream.c
        io/chunked_output_stream.c
        io/tokenizer.c
        mini_table/compat.c
        mini_table/extension_registry.c
        mini_table/message.c
        mini_table/internal/message.c
        mini_descriptor/build_enum.c
        mini_descriptor/decode.c
        mini_descriptor/link.c
        mini_descriptor/internal/base92.c
        mini_descriptor/internal/encode.c
        message/accessors.c
        message/copy.c
        message/message.c
        message/promote.c
        collections/array.c
        collections/map.c
        collections/map_sorter.c
        wire/decode.c
        wire/decode_fast.c
        wire/encode.c
        wire/eps_copy_input_stream.c
        wire/reader.c
        reflection/def_builder.c
        reflection/def_pool.c
        reflection/def_type.c
        reflection/desc_state.c
        reflection/enum_def.c
        reflection/enum_reserved_range.c
        reflection/enum_value_def.c
        reflection/extension_range.c
        reflection/field_def.c
        reflection/file_def.c
        reflection/message.c
        reflection/message_def.c
        reflection/message_reserved_range.c
        reflection/method_def.c
        reflection/oneof_def.c
        reflection/service_def.c
        json/decode.c
        json/encode.c
        text/encode.c
        util/compare.c
        util/def_to_proto.c
        util/required_fields.c
)
add_library(hpb_base STATIC ${HPB_SRC})
target_include_directories(hpb_base PRIVATE ${hpb_SOURCE_DIR})
target_link_libraries(hpb_base hpb_proto ${DEPS_LIB})

add_library(hpb SHARED hpb_so.c)
target_link_libraries(hpb hpb_base hpb_proto)
target_include_directories(hpb PRIVATE ${hpb_SOURCE_DIR})

