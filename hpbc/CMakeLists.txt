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
set(HPBC_COMMON_SRC
        common.cc
        names.cc
        file_layout.cc
        c_hpb.cc
        hs_hpb.cc
        hsc_hpb.cc
)
add_library(hpbc_common STATIC ${HPBC_COMMON_SRC})
target_link_libraries(hpbc_common hpb protobuf::libprotobuf)
target_include_directories(hpbc_common PRIVATE ${hpb_SOURCE_DIR})

add_executable(protoc-gen-hpb protoc-gen-hpb.cc)
target_link_libraries(protoc-gen-hpb hpb_base hpbc_common ${DEPS_LIB})
target_include_directories(protoc-gen-hpb PRIVATE ${hpb_SOURCE_DIR})

add_executable(protoc-gen-hs protoc-gen-hs.cc)
target_link_libraries(protoc-gen-hs hpb_base hpbc_common ${DEPS_LIB})
target_include_directories(protoc-gen-hs PRIVATE ${hpb_SOURCE_DIR})

add_executable(protoc-gen-hpbb protoc-gen-hpbb.cc)
target_link_libraries(protoc-gen-hpbb hpb_base hpbc_common ${DEPS_LIB})
target_include_directories(protoc-gen-hpbb PRIVATE ${hpb_SOURCE_DIR})


file(MAKE_DIRECTORY ${hpb_SOURCE_DIR}/hpbc)
file(COPY ${PROTOBUF_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto DESTINATION ${hpb_SOURCE_DIR}/google/protobuf/compiler)
add_custom_command(
        OUTPUT code_generator_request_hpbb_h code_generator_request_hpbb_c
        COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTOC_FLAGS} -I${hpb_SOURCE_DIR} --plugin=${hpb_BINARY_DIR}/hpbc/protoc-gen-hpbb --hpbb_out=${hpb_BINARY_DIR} ${hpb_SOURCE_DIR}/hpbc/code_generator_request.proto
        DEPENDS protoc-gen-hpbb
)

add_custom_command(
        OUTPUT code_generator_request_hpb_h code_generator_request_hpb_c
        COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTOC_FLAGS} -I${hpb_SOURCE_DIR} --plugin=${hpb_BINARY_DIR}/hpbc/protoc-gen-hpb --hpb_out=${hpb_BINARY_DIR} ${hpb_SOURCE_DIR}/hpbc/code_generator_request.proto
        DEPENDS protoc-gen-hpb
)

add_executable(protoc-gen-hpbdefs protoc-gen-hpbdefs.cc)
target_link_libraries(protoc-gen-hpbdefs hpb_base hpbc_common ${DEPS_LIB})
target_include_directories(protoc-gen-hpbdefs PRIVATE ${hpb_SOURCE_DIR})

add_custom_command(
        OUTPUT plugin_hpbdefs_h plugin_hpbdefs_c
        COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTOC_FLAGS} -I${hpb_SOURCE_DIR} --plugin=${hpb_BINARY_DIR}/hpbc/protoc-gen-hpbdefs --hpbdefs_out=${hpb_BINARY_DIR} ${hpb_SOURCE_DIR}/google/protobuf/compiler/plugin.proto
        DEPENDS protoc-gen-hpbdefs
)

add_custom_command(
        OUTPUT code_generator_request_hpbdefs_h code_generator_request_hpbdefs_c
        COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTOC_FLAGS} -I${hpb_SOURCE_DIR} --plugin=${hpb_BINARY_DIR}/hpbc/protoc-gen-hpbdefs --hpbdefs_out=${hpb_BINARY_DIR} ${hpb_SOURCE_DIR}/hpbc/code_generator_request.proto
        DEPENDS protoc-gen-hpbdefs
)

set(HPBC_LIB_SRC
        code_generator_request_hpbb_c
        code_generator_request_hpb_c
        code_generator_request.c
        plugin_hpbdefs_c
        code_generator_request_hpbdefs_c
        file_layout.cc
        keywords.cc
        names.cc
        get_used_fields.c
        subprocess.cc
        hpbdev.c
        hpbc_so.c
)
add_library(hpbc SHARED ${HPBC_LIB_SRC})
target_include_directories(hpbc PRIVATE ${hpb_SOURCE_DIR})
target_include_directories(hpbc PRIVATE ${hpb_BINARY_DIR})