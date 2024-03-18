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

#ifndef HPBC_PLUGIN_H_
#define HPBC_PLUGIN_H_

#include <stdio.h>

#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif


// begin:github_only
#include "google/protobuf/compiler/plugin.hpb.h"
#include "google/protobuf/descriptor.hpb.h"
// end:github_only

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "hpb/reflection/def.hpp"

// Must be last.
#include "hpb/port/def.inc"

namespace hpbc {

    inline std::vector<std::pair<std::string, std::string>> ParseGeneratorParameter(
            const absl::string_view text) {
        std::vector<std::pair<std::string, std::string>> ret;
        for (absl::string_view sp: absl::StrSplit(text, ',', absl::SkipEmpty())) {
            std::string::size_type equals_pos = sp.find_first_of('=');
            std::pair<std::string, std::string> value;
            if (equals_pos == std::string::npos) {
                value.first = std::string(sp);
            } else {
                value.first = std::string(sp.substr(0, equals_pos));
                value.second = std::string(sp.substr(equals_pos + 1));
            }
            ret.push_back(std::move(value));
        }
        return ret;
    }

    class Plugin {
    public:
        Plugin() { ReadRequest(); }

        ~Plugin() { WriteResponse(); }

        absl::string_view parameter() const {
            return ToStringView(
                    HPB_DESC(compiler_CodeGeneratorRequest_parameter)(request_));
        }

        template<class T>
        void GenerateFilesRaw(T &&func) {
            absl::flat_hash_set<absl::string_view> files_to_generate;
            size_t size;
            const hpb_StringView *file_to_generate = HPB_DESC(
                    compiler_CodeGeneratorRequest_file_to_generate)(request_, &size);
            for (size_t i = 0; i < size; i++) {
                files_to_generate.insert(
                        {file_to_generate[i].data, file_to_generate[i].size});
            }

            const HPB_DESC(FileDescriptorProto) *const *files =
                    HPB_DESC(compiler_CodeGeneratorRequest_proto_file)(request_, &size);
            for (size_t i = 0; i < size; i++) {
                hpb::Status status;
                absl::string_view name =
                        ToStringView(HPB_DESC(FileDescriptorProto_name)(files[i]));
                func(files[i], files_to_generate.contains(name));
            }
        }

        template<class T>
        void GenerateFiles(T &&func) {
            GenerateFilesRaw(
                    [this, &func](const HPB_DESC(FileDescriptorProto) *file_proto,
                                  bool generate) {
                        hpb::Status status;
                        hpb::FileDefPtr file = pool_.AddFile(file_proto, &status);
                        if (!file) {
                            absl::string_view name =
                                    ToStringView(HPB_DESC(FileDescriptorProto_name)(file_proto));
                            ABSL_LOG(FATAL) << "Couldn't add file " << name
                                            << " to DefPool: " << status.error_message();
                        }
                        if (generate) func(file);
                    });
        }

        void SetError(absl::string_view error) {
            char *data =
                    static_cast<char *>(hpb_Arena_Malloc(arena_.ptr(), error.size()));
            memcpy(data, error.data(), error.size());
            HPB_DESC(compiler_CodeGeneratorResponse_set_error)
                    (response_, hpb_StringView_FromDataAndSize(data, error.size()));
        }

        void AddOutputFile(absl::string_view filename, absl::string_view content) {
            HPB_DESC(compiler_CodeGeneratorResponse_File) *file = HPB_DESC(
                    compiler_CodeGeneratorResponse_add_file)(response_, arena_.ptr());
            HPB_DESC(compiler_CodeGeneratorResponse_File_set_name)
                    (file, StringDup(filename));
            HPB_DESC(compiler_CodeGeneratorResponse_File_set_content)
                    (file, StringDup(content));
        }

    private:
        hpb::Arena arena_;
        hpb::DefPool pool_;
        HPB_DESC(compiler_CodeGeneratorRequest) *request_;
        HPB_DESC(compiler_CodeGeneratorResponse) *response_;

        static absl::string_view ToStringView(hpb_StringView sv) {
            return absl::string_view(sv.data, sv.size);
        }

        hpb_StringView StringDup(absl::string_view s) {
            char *data =
                    reinterpret_cast<char *>(hpb_Arena_Malloc(arena_.ptr(), s.size()));
            memcpy(data, s.data(), s.size());
            return hpb_StringView_FromDataAndSize(data, s.size());
        }

        std::string ReadAllStdinBinary() {
            std::string data;
#ifdef _WIN32
            _setmode(_fileno(stdin), _O_BINARY);
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            char buf[4096];
            while (size_t len = fread(buf, 1, sizeof(buf), stdin)) {
                data.append(buf, len);
            }
            return data;
        }

        void ReadRequest() {
            std::string data = ReadAllStdinBinary();
            request_ = HPB_DESC(compiler_CodeGeneratorRequest_parse)(
                    data.data(), data.size(), arena_.ptr());
            if (!request_) {
                ABSL_LOG(FATAL) << "Failed to parse CodeGeneratorRequest";
            }
            response_ = HPB_DESC(compiler_CodeGeneratorResponse_new)(arena_.ptr());
            HPB_DESC(compiler_CodeGeneratorResponse_set_supported_features)
                    (response_,
                     HPB_DESC(compiler_CodeGeneratorResponse_FEATURE_PROTO3_OPTIONAL));
        }

        void WriteResponse() {
            size_t size;
            char *serialized = HPB_DESC(compiler_CodeGeneratorResponse_serialize)(
                    response_, arena_.ptr(), &size);
            if (!serialized) {
                ABSL_LOG(FATAL) << "Failed to serialize CodeGeneratorResponse";
            }

            if (fwrite(serialized, 1, size, stdout) != size) {
                ABSL_LOG(FATAL) << "Failed to write response to stdout";
            }
        }
    };

}  // namespace hpbc

#include "hpb/port/undef.inc"

#endif  // HPBC_PLUGIN_H_
