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


cmake_minimum_required(VERSION 3.10)

project(hpb C CXX)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CPM_0.32.3)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 3rd party dependencies
CPMAddPackage(
        NAME utf8_range
        SOURCE_DIR "${PROJECT_SOURCE_DIR}/3rd/utf8_range"
)
include_directories(${utf8_range_SOURCE_DIR})

CPMAddPackage(
        NAME absl
        SOURCE_DIR "${PROJECT_SOURCE_DIR}/3rd/abseil"
)
include_directories(${absl_SOURCE_DIR})

find_package(Protobuf REQUIRED)
set(DEPS_LIB
        ${PROTOBUF_LIBRARIES}
        utf8_range
        absl::strings
        absl::base
        absl::memory
        absl::hash
        absl::flat_hash_map
        absl::flat_hash_set
        absl::hashtablez_sampler
        absl::raw_logging_internal
        absl::str_format_internal
        absl::str_format
        absl::time
        absl::time_zone
        absl::synchronization
        absl::status
        absl::statusor
        absl::stacktrace
        absl::span
        absl::optional
        absl::log
        )

add_subdirectory(google)
add_subdirectory(hpb)
add_subdirectory(hpbc)
#add_subdirectory(hs)

