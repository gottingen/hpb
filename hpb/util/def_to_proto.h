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

#ifndef HPB_UTIL_DEF_TO_PROTO_H_
#define HPB_UTIL_DEF_TO_PROTO_H_

#include "hpb/reflection/def.h"

#ifdef __cplusplus
extern "C" {
#endif

// Functions for converting defs back to the equivalent descriptor proto.
// Ultimately the goal is that a round-trip proto->def->proto is lossless.  Each
// function returns a new proto created in arena `a`, or NULL if memory
// allocation failed.
google_protobuf_DescriptorProto* hpb_MessageDef_ToProto(const hpb_MessageDef* m,
                                                        hpb_Arena* a);
google_protobuf_EnumDescriptorProto* hpb_EnumDef_ToProto(const hpb_EnumDef* e,
                                                         hpb_Arena* a);
google_protobuf_EnumValueDescriptorProto* hpb_EnumValueDef_ToProto(
    const hpb_EnumValueDef* e, hpb_Arena* a);
google_protobuf_FieldDescriptorProto* hpb_FieldDef_ToProto(
    const hpb_FieldDef* f, hpb_Arena* a);
google_protobuf_OneofDescriptorProto* hpb_OneofDef_ToProto(
    const hpb_OneofDef* o, hpb_Arena* a);
google_protobuf_FileDescriptorProto* hpb_FileDef_ToProto(const hpb_FileDef* f,
                                                         hpb_Arena* a);
google_protobuf_MethodDescriptorProto* hpb_MethodDef_ToProto(
    const hpb_MethodDef* m, hpb_Arena* a);
google_protobuf_ServiceDescriptorProto* hpb_ServiceDef_ToProto(
    const hpb_ServiceDef* s, hpb_Arena* a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // HPB_UTIL_DEF_TO_PROTO_H_
