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

#ifndef HPB_MINI_TABLE_EXTENSION_REGISTRY_H_
#define HPB_MINI_TABLE_EXTENSION_REGISTRY_H_

#include "hpb/mem/arena.h"
#include "hpb/mini_table/extension.h"
#include "hpb/mini_table/message.h"

// Must be last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

/* Extension registry: a dynamic data structure that stores a map of:
 *   (hpb_MiniTable, number) -> extension info
 *
 * hpb_decode() uses hpb_ExtensionRegistry to look up extensions while parsing
 * binary format.
 *
 * hpb_ExtensionRegistry is part of the mini-table (msglayout) family of
 * objects. Like all mini-table objects, it is suitable for reflection-less
 * builds that do not want to expose names into the binary.
 *
 * Unlike most mini-table types, hpb_ExtensionRegistry requires dynamic memory
 * allocation and dynamic initialization:
 * * If reflection is being used, then hpb_DefPool will construct an appropriate
 *   hpb_ExtensionRegistry automatically.
 * * For a mini-table only build, the user must manually construct the
 *   hpb_ExtensionRegistry and populate it with all of the extensions the user
 * cares about.
 * * A third alternative is to manually unpack relevant extensions after the
 *   main parse is complete, similar to how Any works. This is perhaps the
 *   nicest solution from the perspective of reducing dependencies, avoiding
 *   dynamic memory allocation, and avoiding the need to parse uninteresting
 *   extensions.  The downsides are:
 *     (1) parse errors are not caught during the main parse
 *     (2) the CPU hit of parsing comes during access, which could cause an
 *         undesirable stutter in application performance.
 *
 * Users cannot directly get or put into this map. Users can only add the
 * extensions from a generated module and pass the extension registry to the
 * binary decoder.
 *
 * A hpb_DefPool provides a hpb_ExtensionRegistry, so any users who use
 * reflection do not need to populate a hpb_ExtensionRegistry directly.
 */

typedef struct hpb_ExtensionRegistry hpb_ExtensionRegistry;

// Creates a hpb_ExtensionRegistry in the given arena.
// The arena must outlive any use of the extreg.
HPB_API hpb_ExtensionRegistry* hpb_ExtensionRegistry_New(hpb_Arena* arena);

HPB_API bool hpb_ExtensionRegistry_Add(hpb_ExtensionRegistry* r,
                                       const hpb_MiniTableExtension* e);

// Adds the given extension info for the array |e| of size |count| into the
// registry. If there are any errors, the entire array is backed out.
// The extensions must outlive the registry.
// Possible errors include OOM or an extension number that already exists.
// TODO(salo): There is currently no way to know the exact reason for failure.
bool hpb_ExtensionRegistry_AddArray(hpb_ExtensionRegistry* r,
                                    const hpb_MiniTableExtension** e,
                                    size_t count);

// Looks up the extension (if any) defined for message type |t| and field
// number |num|. Returns the extension if found, otherwise NULL.
HPB_API const hpb_MiniTableExtension* hpb_ExtensionRegistry_Lookup(
    const hpb_ExtensionRegistry* r, const hpb_MiniTable* t, uint32_t num);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_MINI_TABLE_EXTENSION_REGISTRY_H_
