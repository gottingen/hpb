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

// IWYU pragma: private, include "hpb/reflection/def.h"

// Declarations common to all public def types.

#ifndef HPB_REFLECTION_COMMON_H_
#define HPB_REFLECTION_COMMON_H_

// begin:github_only
#include "google/protobuf/descriptor.hpb.h"
// end:github_only

typedef enum { kHpb_Syntax_Proto2 = 2, kHpb_Syntax_Proto3 = 3 } hpb_Syntax;

// Forward declarations for circular references.
typedef struct hpb_DefPool hpb_DefPool;
typedef struct hpb_EnumDef hpb_EnumDef;
typedef struct hpb_EnumReservedRange hpb_EnumReservedRange;
typedef struct hpb_EnumValueDef hpb_EnumValueDef;
typedef struct hpb_ExtensionRange hpb_ExtensionRange;
typedef struct hpb_FieldDef hpb_FieldDef;
typedef struct hpb_FileDef hpb_FileDef;
typedef struct hpb_MessageDef hpb_MessageDef;
typedef struct hpb_MessageReservedRange hpb_MessageReservedRange;
typedef struct hpb_MethodDef hpb_MethodDef;
typedef struct hpb_OneofDef hpb_OneofDef;
typedef struct hpb_ServiceDef hpb_ServiceDef;

// EVERYTHING BELOW THIS LINE IS INTERNAL - DO NOT USE /////////////////////////

typedef struct hpb_DefBuilder hpb_DefBuilder;

#endif /* HPB_REFLECTION_COMMON_H_ */
