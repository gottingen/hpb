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

/* This is a hpb implementation of the hpb conformance tests, see:
 *   https://github.com/google/protobuf/tree/master/conformance
 */

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "conformance/conformance.hpb.h"
#include "conformance/conformance.hpbdefs.h"
#include "google/protobuf/test_messages_proto2.hpbdefs.h"
#include "google/protobuf/test_messages_proto3.hpbdefs.h"
#include "hpb/json/decode.h"
#include "hpb/json/encode.h"
#include "hpb/reflection/message.h"
#include "hpb/text/encode.h"
#include "hpb/wire/decode.h"
#include "hpb/wire/encode.h"

// Must be last.
#include "hpb/port/def.inc"

int test_count = 0;
bool verbose = false; /* Set to true to get req/resp printed on stderr. */

bool CheckedRead(int fd, void* buf, size_t len) {
  size_t ofs = 0;
  while (len > 0) {
    ssize_t bytes_read = read(fd, (char*)buf + ofs, len);

    if (bytes_read == 0) return false;

    if (bytes_read < 0) {
      perror("reading from test runner");
      exit(1);
    }

    len -= bytes_read;
    ofs += bytes_read;
  }

  return true;
}

void CheckedWrite(int fd, const void* buf, size_t len) {
  if ((size_t)write(fd, buf, len) != len) {
    perror("writing to test runner");
    exit(1);
  }
}

typedef struct {
  const conformance_ConformanceRequest* request;
  conformance_ConformanceResponse* response;
  hpb_Arena* arena;
  const hpb_DefPool* symtab;
} ctx;

bool parse_proto(hpb_Message* msg, const hpb_MessageDef* m, const ctx* c) {
  hpb_StringView proto =
      conformance_ConformanceRequest_protobuf_payload(c->request);
  if (hpb_Decode(proto.data, proto.size, msg, hpb_MessageDef_MiniTable(m), NULL,
                 0, c->arena) == kHpb_DecodeStatus_Ok) {
    return true;
  } else {
    static const char msg[] = "Parse error";
    conformance_ConformanceResponse_set_parse_error(
        c->response, hpb_StringView_FromString(msg));
    return false;
  }
}

void serialize_proto(const hpb_Message* msg, const hpb_MessageDef* m,
                     const ctx* c) {
  size_t len;
  char* data;
  hpb_EncodeStatus status =
      hpb_Encode(msg, hpb_MessageDef_MiniTable(m), 0, c->arena, &data, &len);
  if (status == kHpb_EncodeStatus_Ok) {
    conformance_ConformanceResponse_set_protobuf_payload(
        c->response, hpb_StringView_FromDataAndSize(data, len));
  } else {
    static const char msg[] = "Error serializing.";
    conformance_ConformanceResponse_set_serialize_error(
        c->response, hpb_StringView_FromString(msg));
  }
}

void serialize_text(const hpb_Message* msg, const hpb_MessageDef* m,
                    const ctx* c) {
  size_t len;
  size_t len2;
  int opts = 0;
  char* data;

  if (!conformance_ConformanceRequest_print_unknown_fields(c->request)) {
    opts |= HPB_TXTENC_SKIPUNKNOWN;
  }

  len = hpb_TextEncode(msg, m, c->symtab, opts, NULL, 0);
  data = hpb_Arena_Malloc(c->arena, len + 1);
  len2 = hpb_TextEncode(msg, m, c->symtab, opts, data, len + 1);
  HPB_ASSERT(len == len2);
  conformance_ConformanceResponse_set_text_payload(
      c->response, hpb_StringView_FromDataAndSize(data, len));
}

bool parse_json(hpb_Message* msg, const hpb_MessageDef* m, const ctx* c) {
  hpb_StringView json = conformance_ConformanceRequest_json_payload(c->request);
  hpb_Status status;
  int opts = 0;

  if (conformance_ConformanceRequest_test_category(c->request) ==
      conformance_JSON_IGNORE_UNKNOWN_PARSING_TEST) {
    opts |= hpb_JsonDecode_IgnoreUnknown;
  }

  hpb_Status_Clear(&status);
  if (hpb_JsonDecode(json.data, json.size, msg, m, c->symtab, opts, c->arena,
                     &status)) {
    return true;
  } else {
    const char* inerr = hpb_Status_ErrorMessage(&status);
    size_t len = strlen(inerr);
    char* err = hpb_Arena_Malloc(c->arena, len + 1);
    memcpy(err, inerr, strlen(inerr));
    err[len] = '\0';
    conformance_ConformanceResponse_set_parse_error(
        c->response, hpb_StringView_FromString(err));
    return false;
  }
}

void serialize_json(const hpb_Message* msg, const hpb_MessageDef* m,
                    const ctx* c) {
  size_t len;
  size_t len2;
  int opts = 0;
  char* data;
  hpb_Status status;

  hpb_Status_Clear(&status);
  len = hpb_JsonEncode(msg, m, c->symtab, opts, NULL, 0, &status);

  if (len == (size_t)-1) {
    const char* inerr = hpb_Status_ErrorMessage(&status);
    size_t len = strlen(inerr);
    char* err = hpb_Arena_Malloc(c->arena, len + 1);
    memcpy(err, inerr, strlen(inerr));
    err[len] = '\0';
    conformance_ConformanceResponse_set_serialize_error(
        c->response, hpb_StringView_FromString(err));
    return;
  }

  data = hpb_Arena_Malloc(c->arena, len + 1);
  len2 = hpb_JsonEncode(msg, m, c->symtab, opts, data, len + 1, &status);
  HPB_ASSERT(len == len2);
  conformance_ConformanceResponse_set_json_payload(
      c->response, hpb_StringView_FromDataAndSize(data, len));
}

bool parse_input(hpb_Message* msg, const hpb_MessageDef* m, const ctx* c) {
  switch (conformance_ConformanceRequest_payload_case(c->request)) {
    case conformance_ConformanceRequest_payload_protobuf_payload:
      return parse_proto(msg, m, c);
    case conformance_ConformanceRequest_payload_json_payload:
      return parse_json(msg, m, c);
    case conformance_ConformanceRequest_payload_NOT_SET:
      fprintf(stderr, "conformance_hpb: Request didn't have payload.\n");
      return false;
    default: {
      static const char msg[] = "Unsupported input format.";
      conformance_ConformanceResponse_set_skipped(
          c->response, hpb_StringView_FromString(msg));
      return false;
    }
  }
}

void write_output(const hpb_Message* msg, const hpb_MessageDef* m,
                  const ctx* c) {
  switch (conformance_ConformanceRequest_requested_output_format(c->request)) {
    case conformance_UNSPECIFIED:
      fprintf(stderr, "conformance_hpb: Unspecified output format.\n");
      exit(1);
    case conformance_PROTOBUF:
      serialize_proto(msg, m, c);
      break;
    case conformance_TEXT_FORMAT:
      serialize_text(msg, m, c);
      break;
    case conformance_JSON:
      serialize_json(msg, m, c);
      break;
    default: {
      static const char msg[] = "Unsupported output format.";
      conformance_ConformanceResponse_set_skipped(
          c->response, hpb_StringView_FromString(msg));
      break;
    }
  }
}

void DoTest(const ctx* c) {
  hpb_Message* msg;
  hpb_StringView name = conformance_ConformanceRequest_message_type(c->request);
  const hpb_MessageDef* m =
      hpb_DefPool_FindMessageByNameWithSize(c->symtab, name.data, name.size);
#if 0
  // Handy code for limiting conformance tests to a single input payload.
  // This is a hack since the conformance runner doesn't give an easy way to
  // specify what test should be run.
  const char skip[] = "\343>\010\301\002\344>\230?\001\230?\002\230?\003";
  hpb_StringView skip_str = hpb_StringView_FromDataAndSize(skip, sizeof(skip) - 1);
  hpb_StringView pb_payload =
      conformance_ConformanceRequest_protobuf_payload(c->request);
  if (!hpb_StringView_IsEqual(pb_payload, skip_str)) m = NULL;
#endif

  if (!m) {
    static const char msg[] = "Unknown message type.";
    conformance_ConformanceResponse_set_skipped(c->response,
                                                hpb_StringView_FromString(msg));
    return;
  }

  msg = hpb_Message_New(hpb_MessageDef_MiniTable(m), c->arena);

  if (parse_input(msg, m, c)) {
    write_output(msg, m, c);
  }
}

void debug_print(const char* label, const hpb_Message* msg,
                 const hpb_MessageDef* m, const ctx* c) {
  char buf[512];
  hpb_TextEncode(msg, m, c->symtab, HPB_TXTENC_SINGLELINE, buf, sizeof(buf));
  fprintf(stderr, "%s: %s\n", label, buf);
}

bool DoTestIo(hpb_DefPool* symtab) {
  hpb_Status status;
  char* input;
  char* output;
  uint32_t input_size;
  size_t output_size;
  ctx c;

  if (!CheckedRead(STDIN_FILENO, &input_size, sizeof(uint32_t))) {
    /* EOF. */
    return false;
  }

  c.symtab = symtab;
  c.arena = hpb_Arena_New();
  input = hpb_Arena_Malloc(c.arena, input_size);

  if (!CheckedRead(STDIN_FILENO, input, input_size)) {
    fprintf(stderr, "conformance_hpb: unexpected EOF on stdin.\n");
    exit(1);
  }

  c.request = conformance_ConformanceRequest_parse(input, input_size, c.arena);
  c.response = conformance_ConformanceResponse_new(c.arena);

  if (c.request) {
    DoTest(&c);
  } else {
    fprintf(stderr, "conformance_hpb: parse of ConformanceRequest failed: %s\n",
            hpb_Status_ErrorMessage(&status));
  }

  output = conformance_ConformanceResponse_serialize(c.response, c.arena,
                                                     &output_size);

  uint32_t network_out = (uint32_t)output_size;
  CheckedWrite(STDOUT_FILENO, &network_out, sizeof(uint32_t));
  CheckedWrite(STDOUT_FILENO, output, output_size);

  test_count++;

  if (verbose) {
    debug_print("Request", c.request,
                conformance_ConformanceRequest_getmsgdef(symtab), &c);
    debug_print("Response", c.response,
                conformance_ConformanceResponse_getmsgdef(symtab), &c);
    fprintf(stderr, "\n");
  }

  hpb_Arena_Free(c.arena);

  return true;
}

int main(void) {
  hpb_DefPool* symtab = hpb_DefPool_New();

#ifdef REBUILD_MINITABLES
  _hpb_DefPool_LoadDefInitEx(
      symtab, &google_protobuf_test_messages_proto2_proto_upbdefinit, true);
  _hpb_DefPool_LoadDefInitEx(
      symtab, &google_protobuf_test_messages_proto3_proto_upbdefinit, true);
#else
  protobuf_test_messages_proto2_TestAllTypesProto2_getmsgdef(symtab);
  protobuf_test_messages_proto3_TestAllTypesProto3_getmsgdef(symtab);
#endif

  while (1) {
    if (!DoTestIo(symtab)) {
      fprintf(stderr,
              "conformance_hpb: received EOF from test runner "
              "after %d tests, exiting\n",
              test_count);
      hpb_DefPool_Free(symtab);
      return 0;
    }
  }
}
