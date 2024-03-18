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

#ifndef HPB_BASE_STATUS_H_
#define HPB_BASE_STATUS_H_

#include <stdarg.h>

// Must be last.
#include "hpb/port/def.inc"

#define _kHpb_Status_MaxMessage 127

typedef struct {
  bool ok;
  char msg[_kHpb_Status_MaxMessage];  // Error message; NULL-terminated.
} hpb_Status;

#ifdef __cplusplus
extern "C" {
#endif

HPB_API const char* hpb_Status_ErrorMessage(const hpb_Status* status);
HPB_API bool hpb_Status_IsOk(const hpb_Status* status);

// These are no-op if |status| is NULL.
HPB_API void hpb_Status_Clear(hpb_Status* status);
void hpb_Status_SetErrorMessage(hpb_Status* status, const char* msg);
void hpb_Status_SetErrorFormat(hpb_Status* status, const char* fmt, ...)
    HPB_PRINTF(2, 3);
void hpb_Status_VSetErrorFormat(hpb_Status* status, const char* fmt,
                                va_list args) HPB_PRINTF(2, 0);
void hpb_Status_VAppendErrorFormat(hpb_Status* status, const char* fmt,
                                   va_list args) HPB_PRINTF(2, 0);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif /* HPB_BASE_STATUS_H_ */
