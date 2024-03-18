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

// Class for parsing tokenized text from a ZeroCopyInputStream.

#ifndef HPB_IO_TOKENIZER_H_
#define HPB_IO_TOKENIZER_H_

#include "hpb/base/status.h"
#include "hpb/base/string_view.h"
#include "hpb/io/zero_copy_input_stream.h"
#include "hpb/mem/arena.h"

// Must be included last.
#include "hpb/port/def.inc"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  kHpb_TokenType_Start,  // Next() has not yet been called.
  kHpb_TokenType_End,    // End of input reached. "text" is empty.

  // A sequence of letters, digits, and underscores, not starting with a digit.
  // It is an error for a number to be followed by an identifier with no space
  // in between.
  kHpb_TokenType_Identifier,

  // A sequence of digits representing an integer. Normally the digits are
  // decimal, but a prefix of "0x" indicates a hex number and a leading zero
  // indicates octal, just like with C numeric literals. A leading negative
  // sign is NOT included in the token; it's up to the parser to interpret the
  // unary minus operator on its own.
  kHpb_TokenType_Integer,

  // A floating point literal, with a fractional part and/or an exponent.
  // Always in decimal. Again, never negative.
  kHpb_TokenType_Float,

  // A quoted sequence of escaped characters.
  // Either single or double quotes can be used, but they must match.
  // A string literal cannot cross a line break.
  kHpb_TokenType_String,

  // Any other printable character, like '!' or '+'.
  // Symbols are always a single character, so "!+$%" is four tokens.
  kHpb_TokenType_Symbol,

  // A sequence of whitespace.
  // This token type is only produced if report_whitespace() is true.
  // It is not reported for whitespace within comments or strings.
  kHpb_TokenType_Whitespace,

  // A newline ('\n'). This token type is only produced if report_whitespace()
  // is true and report_newlines() is also true.
  // It is not reported for newlines in comments or strings.
  kHpb_TokenType_Newline,
} hpb_TokenType;

typedef enum {
  // Set to allow floats to be suffixed with the letter 'f'. Tokens which would
  // otherwise be integers but which have the 'f' suffix will be forced to be
  // interpreted as floats. For all other purposes, the 'f' is ignored.
  kHpb_TokenizerOption_AllowFAfterFloat = 1 << 0,

  // If set, whitespace tokens are reported by Next().
  kHpb_TokenizerOption_ReportWhitespace = 1 << 1,

  // If set, newline tokens are reported by Next().
  // This is a superset of ReportWhitespace.
  kHpb_TokenizerOption_ReportNewlines = 1 << 2,

  // By default the tokenizer expects C-style (/* */) comments.
  // If set, it expects shell-style (#) comments instead.
  kHpb_TokenizerOption_CommentStyleShell = 1 << 3,
} hpb_Tokenizer_Option;

typedef struct hpb_Tokenizer hpb_Tokenizer;

// Can be passed a flat array and/or a ZCIS as input.
// The array will be read first (if non-NULL), then the stream (if non-NULL).
hpb_Tokenizer* hpb_Tokenizer_New(const void* data, size_t size,
                                 hpb_ZeroCopyInputStream* input, int options,
                                 hpb_Arena* arena);

void hpb_Tokenizer_Fini(hpb_Tokenizer* t);

// Advance the tokenizer to the next input token. Returns True on success.
// Returns False and (clears *status on EOF, sets *status on error).
bool hpb_Tokenizer_Next(hpb_Tokenizer* t, hpb_Status* status);

// Accessors for inspecting current/previous parse tokens,
// which are opaque to the tokenizer (to reduce copying).

hpb_TokenType hpb_Tokenizer_Type(const hpb_Tokenizer* t);
int hpb_Tokenizer_Column(const hpb_Tokenizer* t);
int hpb_Tokenizer_EndColumn(const hpb_Tokenizer* t);
int hpb_Tokenizer_Line(const hpb_Tokenizer* t);
int hpb_Tokenizer_TextSize(const hpb_Tokenizer* t);
const char* hpb_Tokenizer_TextData(const hpb_Tokenizer* t);

// External helper: validate an identifier.
bool hpb_Tokenizer_IsIdentifier(const char* data, int size);

// Parses a TYPE_INTEGER token. Returns false if the result would be
// greater than max_value. Otherwise, returns true and sets *output to the
// result. If the text is not from a Token of type TYPE_INTEGER originally
// parsed by a Tokenizer, the result is undefined (possibly an assert
// failure).
bool hpb_Parse_Integer(const char* text, uint64_t max_value, uint64_t* output);

// Parses a TYPE_FLOAT token. This never fails, so long as the text actually
// comes from a TYPE_FLOAT token parsed by Tokenizer. If it doesn't, the
// result is undefined (possibly an assert failure).
double hpb_Parse_Float(const char* text);

// Parses a TYPE_STRING token. This never fails, so long as the text actually
// comes from a TYPE_STRING token parsed by Tokenizer. If it doesn't, the
// result is undefined (possibly an assert failure).
hpb_StringView hpb_Parse_String(const char* text, hpb_Arena* arena);

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "hpb/port/undef.inc"

#endif  // HPB_IO_TOKENIZER_H_
