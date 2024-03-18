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

#ifndef HPB_REFLECTION_MESSAGE_H_
#define HPB_REFLECTION_MESSAGE_H_

#include "hpb/collections/map.h"
#include "hpb/reflection/common.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

// Returns a mutable pointer to a map, array, or submessage value. If the given
// arena is non-NULL this will construct a new object if it was not previously
// present. May not be called for primitive fields.
HPB_API hpb_MutableMessageValue hpb_Message_Mutable(hpb_Message* msg,
                                                    const hpb_FieldDef* f,
                                                    hpb_Arena* a);

// Returns the field that is set in the oneof, or NULL if none are set.
HPB_API const hpb_FieldDef* hpb_Message_WhichOneof(const hpb_Message* msg,
                                                   const hpb_OneofDef* o);

// Clear all data and unknown fields.
void hpb_Message_ClearByDef(hpb_Message* msg, const hpb_MessageDef* m);

// Clears any field presence and sets the value back to its default.
HPB_API void hpb_Message_ClearFieldByDef(hpb_Message* msg,
                                         const hpb_FieldDef* f);

// May only be called for fields where hpb_FieldDef_HasPresence(f) == true.
HPB_API bool hpb_Message_HasFieldByDef(const hpb_Message* msg,
                                       const hpb_FieldDef* f);

// Returns the value in the message associated with this field def.
HPB_API hpb_MessageValue hpb_Message_GetFieldByDef(const hpb_Message* msg,
                                                   const hpb_FieldDef* f);

// Sets the given field to the given value. For a msg/array/map/string, the
// caller must ensure that the target data outlives |msg| (by living either in
// the same arena or a different arena that outlives it).
//
// Returns false if allocation fails.
HPB_API bool hpb_Message_SetFieldByDef(hpb_Message* msg, const hpb_FieldDef* f,
                                       hpb_MessageValue val, hpb_Arena* a);

// Iterate over present fields.
//
// size_t iter = kHpb_Message_Begin;
// const hpb_FieldDef *f;
// hpb_MessageValue val;
// while (hpb_Message_Next(msg, m, ext_pool, &f, &val, &iter)) {
//   process_field(f, val);
// }
//
// If ext_pool is NULL, no extensions will be returned.  If the given symtab
// returns extensions that don't match what is in this message, those extensions
// will be skipped.

#define kHpb_Message_Begin -1

bool hpb_Message_Next(const hpb_Message* msg, const hpb_MessageDef* m,
                      const hpb_DefPool* ext_pool, const hpb_FieldDef** f,
                      hpb_MessageValue* val, size_t* iter);

// Clears all unknown field data from this message and all submessages.
HPB_API bool hpb_Message_DiscardUnknown(hpb_Message* msg,
                                        const hpb_MessageDef* m, int maxdepth);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_REFLECTION_MESSAGE_H_ */
