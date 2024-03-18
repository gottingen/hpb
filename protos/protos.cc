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

#include "protos/protos.h"

#include <atomic>
#include <cstddef>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "protos/protos_extension_lock.h"
#include "hpb/mem/arena.h"
#include "hpb/message/copy.h"
#include "hpb/message/internal/extension.h"
#include "hpb/message/promote.h"
#include "hpb/mini_table/extension.h"
#include "hpb/mini_table/extension_registry.h"
#include "hpb/mini_table/message.h"
#include "hpb/wire/decode.h"
#include "hpb/wire/encode.h"

namespace protos {

// begin:google_only
// absl::Status MessageAllocationError(SourceLocation loc) {
//   return absl::Status(absl::StatusCode::kInternal,
//                       "Hpb message allocation error", loc);
// }
//
// absl::Status ExtensionNotFoundError(int extension_number, SourceLocation loc) {
//   return absl::Status(
//       absl::StatusCode::kInternal,
//       absl::StrFormat("Extension %d not found", extension_number), loc);
// }
//
// absl::Status MessageEncodeError(hpb_EncodeStatus status, SourceLocation loc) {
//   return absl::Status(absl::StatusCode::kInternal,
//                       absl::StrFormat("Hpb message encoding error %d", status),
//                       loc
//
//   );
// }
//
// absl::Status MessageDecodeError(hpb_DecodeStatus status, SourceLocation loc
//
// ) {
//   return absl::Status(absl::StatusCode::kInternal,
//                       absl::StrFormat("Hpb message parse error %d", status), loc
//
//   );
// }
// end:google_only

// begin:github_only
absl::Status MessageAllocationError(SourceLocation loc) {
  return absl::Status(absl::StatusCode::kUnknown,
                      "Hpb message allocation error");
}

absl::Status ExtensionNotFoundError(int ext_number, SourceLocation loc) {
  return absl::Status(absl::StatusCode::kUnknown,
                      absl::StrFormat("Extension %d not found", ext_number));
}

absl::Status MessageEncodeError(hpb_EncodeStatus s, SourceLocation loc) {
  return absl::Status(absl::StatusCode::kUnknown, "Encoding error");
}

absl::Status MessageDecodeError(hpb_DecodeStatus status, SourceLocation loc

) {
  return absl::Status(absl::StatusCode::kUnknown, "Hpb message parse error");
}
// end:github_only

namespace internal {

hpb_ExtensionRegistry* GetUpbExtensions(
    const ExtensionRegistry& extension_registry) {
  return extension_registry.registry_;
}

/**
 * MessageLock(msg) acquires lock on msg when constructed and releases it when
 * destroyed.
 */
class MessageLock {
 public:
  explicit MessageLock(const hpb_Message* msg) : msg_(msg) {
    HpbExtensionLocker locker =
        upb_extension_locker_global.load(std::memory_order_acquire);
    unlocker_ = (locker != nullptr) ? locker(msg) : nullptr;
  }
  MessageLock(const MessageLock&) = delete;
  void operator=(const MessageLock&) = delete;
  ~MessageLock() {
    if (unlocker_ != nullptr) {
      unlocker_(msg_);
    }
  }

 private:
  const hpb_Message* msg_;
  HpbExtensionUnlocker unlocker_;
};

bool HasExtensionOrUnknown(const hpb_Message* msg,
                           const hpb_MiniTableExtension* eid) {
  MessageLock msg_lock(msg);
  return _hpb_Message_Getext(msg, eid) != nullptr ||
         hpb_MiniTable_FindUnknown(msg, eid->field.number,
                                   kHpb_WireFormat_DefaultDepthLimit)
                 .status == kHpb_FindUnknown_Ok;
}

const hpb_Message_Extension* GetOrPromoteExtension(
    hpb_Message* msg, const hpb_MiniTableExtension* eid, hpb_Arena* arena) {
  MessageLock msg_lock(msg);
  const hpb_Message_Extension* ext = _hpb_Message_Getext(msg, eid);
  if (ext == nullptr) {
    hpb_GetExtension_Status ext_status = hpb_MiniTable_GetOrPromoteExtension(
        (hpb_Message*)msg, eid, kHpb_WireFormat_DefaultDepthLimit, arena, &ext);
    if (ext_status != kHpb_GetExtension_Ok) {
      ext = nullptr;
    }
  }
  return ext;
}

absl::StatusOr<absl::string_view> Serialize(const hpb_Message* message,
                                            const hpb_MiniTable* mini_table,
                                            hpb_Arena* arena, int options) {
  MessageLock msg_lock(message);
  size_t len;
  char* ptr;
  hpb_EncodeStatus status =
      hpb_Encode(message, mini_table, options, arena, &ptr, &len);
  if (status == kHpb_EncodeStatus_Ok) {
    return absl::string_view(ptr, len);
  }
  return MessageEncodeError(status);
}

void DeepCopy(hpb_Message* target, const hpb_Message* source,
              const hpb_MiniTable* mini_table, hpb_Arena* arena) {
  MessageLock msg_lock(source);
  hpb_Message_DeepCopy(target, source, mini_table, arena);
}

hpb_Message* DeepClone(const hpb_Message* source,
                       const hpb_MiniTable* mini_table, hpb_Arena* arena) {
  MessageLock msg_lock(source);
  return hpb_Message_DeepClone(source, mini_table, arena);
}

}  // namespace internal

}  // namespace protos
