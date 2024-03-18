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

#ifndef HPBC_FILE_LAYOUT_H_
#define HPBC_FILE_LAYOUT_H_

#include <string>

// begin:google_only
// #ifndef HPB_BOOTSTRAP_STAGE0
// #include "net/proto2/proto/descriptor.hpb.h"
// #else
// #include "google/protobuf/descriptor.hpb.h"
// #endif
// end:google_only

// begin:github_only
#include "google/protobuf/descriptor.hpb.h"
// end:github_only

#include "absl/container/flat_hash_map.h"
#include "hpb/base/status.hpp"
#include "hpb/mini_descriptor/decode.h"
#include "hpb/reflection/def.h"
#include "hpb/reflection/def.hpp"

// Must be last
#include "hpb/port/def.inc"

namespace hpbc {

    std::vector<hpb::EnumDefPtr> SortedEnums(hpb::FileDefPtr file);

    // Ordering must match hpb/def.c!
    //
    // The ordering is significant because each hpb_MessageDef* will point at the
    // corresponding hpb_MiniTable and we just iterate through the list without
    // any search or lookup.
    std::vector<hpb::MessageDefPtr> SortedMessages(hpb::FileDefPtr file);

    // Ordering must match hpb/def.c!
    //
    // The ordering is significant because each hpb_FieldDef* will point at the
    // corresponding hpb_MiniTableExtension and we just iterate through the list
    // without any search or lookup.
    std::vector<hpb::FieldDefPtr> SortedExtensions(hpb::FileDefPtr file);

    std::vector<hpb::FieldDefPtr> FieldNumberOrder(hpb::MessageDefPtr message);

    // DefPoolPair is a pair of DefPools: one for 32-bit and one for 64-bit.
    class DefPoolPair {
    public:
        DefPoolPair() {
            pool32_._SetPlatform(kHpb_MiniTablePlatform_32Bit);
            pool64_._SetPlatform(kHpb_MiniTablePlatform_64Bit);
        }

        hpb::FileDefPtr AddFile(const HPB_DESC(FileDescriptorProto) *file_proto,
                                hpb::Status *status) {
            hpb::FileDefPtr file32 = pool32_.AddFile(file_proto, status);
            hpb::FileDefPtr file64 = pool64_.AddFile(file_proto, status);
            if (!file32) return file32;
            return file64;
        }

        const hpb_MiniTable *GetMiniTable32(hpb::MessageDefPtr m) const {
            return pool32_.FindMessageByName(m.full_name()).mini_table();
        }

        const hpb_MiniTable *GetMiniTable64(hpb::MessageDefPtr m) const {
            return pool64_.FindMessageByName(m.full_name()).mini_table();
        }

        const hpb_MiniTableField *GetField32(hpb::FieldDefPtr f) const {
            return GetFieldFromPool(&pool32_, f);
        }

        const hpb_MiniTableField *GetField64(hpb::FieldDefPtr f) const {
            return GetFieldFromPool(&pool64_, f);
        }

    private:
        static const hpb_MiniTableField *GetFieldFromPool(const hpb::DefPool *pool,
                                                          hpb::FieldDefPtr f) {
            if (f.is_extension()) {
                return pool->FindExtensionByName(f.full_name()).mini_table();
            } else {
                return pool->FindMessageByName(f.containing_type().full_name())
                        .FindFieldByNumber(f.number())
                        .mini_table();
            }
        }

        hpb::DefPool pool32_;
        hpb::DefPool pool64_;
    };

}  // namespace hpbc

#include "hpb/port/undef.inc"

#endif  // HPBC_FILE_LAYOUT_H_
