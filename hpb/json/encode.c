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

#include "hpb/json/encode.h"

#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "hpb/collections/map.h"
#include "hpb/lex/round_trip.h"
#include "hpb/port/vsnprintf_compat.h"
#include "hpb/reflection/message.h"
#include "hpb/wire/decode.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct {
  char *buf, *ptr, *end;
  size_t overflow;
  int indent_depth;
  int options;
  const hpb_DefPool* ext_pool;
  jmp_buf err;
  hpb_Status* status;
  hpb_Arena* arena;
} jsonenc;

static void jsonenc_msg(jsonenc* e, const hpb_Message* msg,
                        const hpb_MessageDef* m);
static void jsonenc_scalar(jsonenc* e, hpb_MessageValue val,
                           const hpb_FieldDef* f);
static void jsonenc_msgfield(jsonenc* e, const hpb_Message* msg,
                             const hpb_MessageDef* m);
static void jsonenc_msgfields(jsonenc* e, const hpb_Message* msg,
                              const hpb_MessageDef* m, bool first);
static void jsonenc_value(jsonenc* e, const hpb_Message* msg,
                          const hpb_MessageDef* m);

HPB_NORETURN static void jsonenc_err(jsonenc* e, const char* msg) {
  hpb_Status_SetErrorMessage(e->status, msg);
  longjmp(e->err, 1);
}

HPB_PRINTF(2, 3)
HPB_NORETURN static void jsonenc_errf(jsonenc* e, const char* fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  hpb_Status_VSetErrorFormat(e->status, fmt, argp);
  va_end(argp);
  longjmp(e->err, 1);
}

static hpb_Arena* jsonenc_arena(jsonenc* e) {
  /* Create lazily, since it's only needed for Any */
  if (!e->arena) {
    e->arena = hpb_Arena_New();
  }
  return e->arena;
}

static void jsonenc_putbytes(jsonenc* e, const void* data, size_t len) {
  size_t have = e->end - e->ptr;
  if (HPB_LIKELY(have >= len)) {
    memcpy(e->ptr, data, len);
    e->ptr += len;
  } else {
    if (have) {
      memcpy(e->ptr, data, have);
      e->ptr += have;
    }
    e->overflow += (len - have);
  }
}

static void jsonenc_putstr(jsonenc* e, const char* str) {
  jsonenc_putbytes(e, str, strlen(str));
}

HPB_PRINTF(2, 3)
static void jsonenc_printf(jsonenc* e, const char* fmt, ...) {
  size_t n;
  size_t have = e->end - e->ptr;
  va_list args;

  va_start(args, fmt);
  n = _hpb_vsnprintf(e->ptr, have, fmt, args);
  va_end(args);

  if (HPB_LIKELY(have > n)) {
    e->ptr += n;
  } else {
    e->ptr = HPB_PTRADD(e->ptr, have);
    e->overflow += (n - have);
  }
}

static void jsonenc_nanos(jsonenc* e, int32_t nanos) {
  int digits = 9;

  if (nanos == 0) return;
  if (nanos < 0 || nanos >= 1000000000) {
    jsonenc_err(e, "error formatting timestamp as JSON: invalid nanos");
  }

  while (nanos % 1000 == 0) {
    nanos /= 1000;
    digits -= 3;
  }

  jsonenc_printf(e, ".%.*" PRId32, digits, nanos);
}

static void jsonenc_timestamp(jsonenc* e, const hpb_Message* msg,
                              const hpb_MessageDef* m) {
  const hpb_FieldDef* seconds_f = hpb_MessageDef_FindFieldByNumber(m, 1);
  const hpb_FieldDef* nanos_f = hpb_MessageDef_FindFieldByNumber(m, 2);
  int64_t seconds = hpb_Message_GetFieldByDef(msg, seconds_f).int64_val;
  int32_t nanos = hpb_Message_GetFieldByDef(msg, nanos_f).int32_val;
  int L, N, I, J, K, hour, min, sec;

  if (seconds < -62135596800) {
    jsonenc_err(e,
                "error formatting timestamp as JSON: minimum acceptable value "
                "is 0001-01-01T00:00:00Z");
  } else if (seconds > 253402300799) {
    jsonenc_err(e,
                "error formatting timestamp as JSON: maximum acceptable value "
                "is 9999-12-31T23:59:59Z");
  }

  /* Julian Day -> Y/M/D, Algorithm from:
   * Fliegel, H. F., and Van Flandern, T. C., "A Machine Algorithm for
   *   Processing Calendar Dates," Communications of the Association of
   *   Computing Machines, vol. 11 (1968), p. 657.  */
  seconds += 62135596800;  // Ensure seconds is positive.
  L = (int)(seconds / 86400) - 719162 + 68569 + 2440588;
  N = 4 * L / 146097;
  L = L - (146097 * N + 3) / 4;
  I = 4000 * (L + 1) / 1461001;
  L = L - 1461 * I / 4 + 31;
  J = 80 * L / 2447;
  K = L - 2447 * J / 80;
  L = J / 11;
  J = J + 2 - 12 * L;
  I = 100 * (N - 49) + I + L;

  sec = seconds % 60;
  min = (seconds / 60) % 60;
  hour = (seconds / 3600) % 24;

  jsonenc_printf(e, "\"%04d-%02d-%02dT%02d:%02d:%02d", I, J, K, hour, min, sec);
  jsonenc_nanos(e, nanos);
  jsonenc_putstr(e, "Z\"");
}

static void jsonenc_duration(jsonenc* e, const hpb_Message* msg,
                             const hpb_MessageDef* m) {
  const hpb_FieldDef* seconds_f = hpb_MessageDef_FindFieldByNumber(m, 1);
  const hpb_FieldDef* nanos_f = hpb_MessageDef_FindFieldByNumber(m, 2);
  int64_t seconds = hpb_Message_GetFieldByDef(msg, seconds_f).int64_val;
  int32_t nanos = hpb_Message_GetFieldByDef(msg, nanos_f).int32_val;
  bool negative = false;

  if (seconds > 315576000000 || seconds < -315576000000 ||
      (seconds != 0 && nanos != 0 && (seconds < 0) != (nanos < 0))) {
    jsonenc_err(e, "bad duration");
  }

  if (seconds < 0) {
    negative = true;
    seconds = -seconds;
  }
  if (nanos < 0) {
    negative = true;
    nanos = -nanos;
  }

  jsonenc_putstr(e, "\"");
  if (negative) {
    jsonenc_putstr(e, "-");
  }
  jsonenc_printf(e, "%" PRId64, seconds);
  jsonenc_nanos(e, nanos);
  jsonenc_putstr(e, "s\"");
}

static void jsonenc_enum(int32_t val, const hpb_FieldDef* f, jsonenc* e) {
  const hpb_EnumDef* e_def = hpb_FieldDef_EnumSubDef(f);

  if (strcmp(hpb_EnumDef_FullName(e_def), "google.protobuf.NullValue") == 0) {
    jsonenc_putstr(e, "null");
  } else {
    const hpb_EnumValueDef* ev =
        (e->options & hpb_JsonEncode_FormatEnumsAsIntegers)
            ? NULL
            : hpb_EnumDef_FindValueByNumber(e_def, val);

    if (ev) {
      jsonenc_printf(e, "\"%s\"", hpb_EnumValueDef_Name(ev));
    } else {
      jsonenc_printf(e, "%" PRId32, val);
    }
  }
}

static void jsonenc_bytes(jsonenc* e, hpb_StringView str) {
  /* This is the regular base64, not the "web-safe" version. */
  static const char base64[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const unsigned char* ptr = (unsigned char*)str.data;
  const unsigned char* end = HPB_PTRADD(ptr, str.size);
  char buf[4];

  jsonenc_putstr(e, "\"");

  while (end - ptr >= 3) {
    buf[0] = base64[ptr[0] >> 2];
    buf[1] = base64[((ptr[0] & 0x3) << 4) | (ptr[1] >> 4)];
    buf[2] = base64[((ptr[1] & 0xf) << 2) | (ptr[2] >> 6)];
    buf[3] = base64[ptr[2] & 0x3f];
    jsonenc_putbytes(e, buf, 4);
    ptr += 3;
  }

  switch (end - ptr) {
    case 2:
      buf[0] = base64[ptr[0] >> 2];
      buf[1] = base64[((ptr[0] & 0x3) << 4) | (ptr[1] >> 4)];
      buf[2] = base64[(ptr[1] & 0xf) << 2];
      buf[3] = '=';
      jsonenc_putbytes(e, buf, 4);
      break;
    case 1:
      buf[0] = base64[ptr[0] >> 2];
      buf[1] = base64[((ptr[0] & 0x3) << 4)];
      buf[2] = '=';
      buf[3] = '=';
      jsonenc_putbytes(e, buf, 4);
      break;
  }

  jsonenc_putstr(e, "\"");
}

static void jsonenc_stringbody(jsonenc* e, hpb_StringView str) {
  const char* ptr = str.data;
  const char* end = HPB_PTRADD(ptr, str.size);

  while (ptr < end) {
    switch (*ptr) {
      case '\n':
        jsonenc_putstr(e, "\\n");
        break;
      case '\r':
        jsonenc_putstr(e, "\\r");
        break;
      case '\t':
        jsonenc_putstr(e, "\\t");
        break;
      case '\"':
        jsonenc_putstr(e, "\\\"");
        break;
      case '\f':
        jsonenc_putstr(e, "\\f");
        break;
      case '\b':
        jsonenc_putstr(e, "\\b");
        break;
      case '\\':
        jsonenc_putstr(e, "\\\\");
        break;
      default:
        if ((uint8_t)*ptr < 0x20) {
          jsonenc_printf(e, "\\u%04x", (int)(uint8_t)*ptr);
        } else {
          /* This could be a non-ASCII byte.  We rely on the string being valid
           * UTF-8. */
          jsonenc_putbytes(e, ptr, 1);
        }
        break;
    }
    ptr++;
  }
}

static void jsonenc_string(jsonenc* e, hpb_StringView str) {
  jsonenc_putstr(e, "\"");
  jsonenc_stringbody(e, str);
  jsonenc_putstr(e, "\"");
}

static bool hpb_JsonEncode_HandleSpecialDoubles(jsonenc* e, double val) {
  if (val == INFINITY) {
    jsonenc_putstr(e, "\"Infinity\"");
  } else if (val == -INFINITY) {
    jsonenc_putstr(e, "\"-Infinity\"");
  } else if (val != val) {
    jsonenc_putstr(e, "\"NaN\"");
  } else {
    return false;
  }
  return true;
}

static void hpb_JsonEncode_Double(jsonenc* e, double val) {
  if (hpb_JsonEncode_HandleSpecialDoubles(e, val)) return;
  char buf[32];
  _hpb_EncodeRoundTripDouble(val, buf, sizeof(buf));
  jsonenc_putstr(e, buf);
}

static void hpb_JsonEncode_Float(jsonenc* e, float val) {
  if (hpb_JsonEncode_HandleSpecialDoubles(e, val)) return;
  char buf[32];
  _hpb_EncodeRoundTripFloat(val, buf, sizeof(buf));
  jsonenc_putstr(e, buf);
}

static void jsonenc_wrapper(jsonenc* e, const hpb_Message* msg,
                            const hpb_MessageDef* m) {
  const hpb_FieldDef* val_f = hpb_MessageDef_FindFieldByNumber(m, 1);
  hpb_MessageValue val = hpb_Message_GetFieldByDef(msg, val_f);
  jsonenc_scalar(e, val, val_f);
}

static const hpb_MessageDef* jsonenc_getanymsg(jsonenc* e,
                                               hpb_StringView type_url) {
  /* Find last '/', if any. */
  const char* end = type_url.data + type_url.size;
  const char* ptr = end;
  const hpb_MessageDef* ret;

  if (!e->ext_pool) {
    jsonenc_err(e, "Tried to encode Any, but no symtab was provided");
  }

  if (type_url.size == 0) goto badurl;

  while (true) {
    if (--ptr == type_url.data) {
      /* Type URL must contain at least one '/', with host before. */
      goto badurl;
    }
    if (*ptr == '/') {
      ptr++;
      break;
    }
  }

  ret = hpb_DefPool_FindMessageByNameWithSize(e->ext_pool, ptr, end - ptr);

  if (!ret) {
    jsonenc_errf(e, "Couldn't find Any type: %.*s", (int)(end - ptr), ptr);
  }

  return ret;

badurl:
  jsonenc_errf(e, "Bad type URL: " HPB_STRINGVIEW_FORMAT,
               HPB_STRINGVIEW_ARGS(type_url));
}

static void jsonenc_any(jsonenc* e, const hpb_Message* msg,
                        const hpb_MessageDef* m) {
  const hpb_FieldDef* type_url_f = hpb_MessageDef_FindFieldByNumber(m, 1);
  const hpb_FieldDef* value_f = hpb_MessageDef_FindFieldByNumber(m, 2);
  hpb_StringView type_url = hpb_Message_GetFieldByDef(msg, type_url_f).str_val;
  hpb_StringView value = hpb_Message_GetFieldByDef(msg, value_f).str_val;
  const hpb_MessageDef* any_m = jsonenc_getanymsg(e, type_url);
  const hpb_MiniTable* any_layout = hpb_MessageDef_MiniTable(any_m);
  hpb_Arena* arena = jsonenc_arena(e);
  hpb_Message* any = hpb_Message_New(any_layout, arena);

  if (hpb_Decode(value.data, value.size, any, any_layout, NULL, 0, arena) !=
      kHpb_DecodeStatus_Ok) {
    jsonenc_err(e, "Error decoding message in Any");
  }

  jsonenc_putstr(e, "{\"@type\":");
  jsonenc_string(e, type_url);

  if (hpb_MessageDef_WellKnownType(any_m) == kHpb_WellKnown_Unspecified) {
    /* Regular messages: {"@type": "...","foo": 1, "bar": 2} */
    jsonenc_msgfields(e, any, any_m, false);
  } else {
    /* Well-known type: {"@type": "...","value": <well-known encoding>} */
    jsonenc_putstr(e, ",\"value\":");
    jsonenc_msgfield(e, any, any_m);
  }

  jsonenc_putstr(e, "}");
}

static void jsonenc_putsep(jsonenc* e, const char* str, bool* first) {
  if (*first) {
    *first = false;
  } else {
    jsonenc_putstr(e, str);
  }
}

static void jsonenc_fieldpath(jsonenc* e, hpb_StringView path) {
  const char* ptr = path.data;
  const char* end = ptr + path.size;

  while (ptr < end) {
    char ch = *ptr;

    if (ch >= 'A' && ch <= 'Z') {
      jsonenc_err(e, "Field mask element may not have upper-case letter.");
    } else if (ch == '_') {
      if (ptr == end - 1 || *(ptr + 1) < 'a' || *(ptr + 1) > 'z') {
        jsonenc_err(e, "Underscore must be followed by a lowercase letter.");
      }
      ch = *++ptr - 32;
    }

    jsonenc_putbytes(e, &ch, 1);
    ptr++;
  }
}

static void jsonenc_fieldmask(jsonenc* e, const hpb_Message* msg,
                              const hpb_MessageDef* m) {
  const hpb_FieldDef* paths_f = hpb_MessageDef_FindFieldByNumber(m, 1);
  const hpb_Array* paths = hpb_Message_GetFieldByDef(msg, paths_f).array_val;
  bool first = true;
  size_t i, n = 0;

  if (paths) n = hpb_Array_Size(paths);

  jsonenc_putstr(e, "\"");

  for (i = 0; i < n; i++) {
    jsonenc_putsep(e, ",", &first);
    jsonenc_fieldpath(e, hpb_Array_Get(paths, i).str_val);
  }

  jsonenc_putstr(e, "\"");
}

static void jsonenc_struct(jsonenc* e, const hpb_Message* msg,
                           const hpb_MessageDef* m) {
  jsonenc_putstr(e, "{");

  const hpb_FieldDef* fields_f = hpb_MessageDef_FindFieldByNumber(m, 1);
  const hpb_Map* fields = hpb_Message_GetFieldByDef(msg, fields_f).map_val;

  if (fields) {
    const hpb_MessageDef* entry_m = hpb_FieldDef_MessageSubDef(fields_f);
    const hpb_FieldDef* value_f = hpb_MessageDef_FindFieldByNumber(entry_m, 2);

    size_t iter = kHpb_Map_Begin;
    bool first = true;

    hpb_MessageValue key, val;
    while (hpb_Map_Next(fields, &key, &val, &iter)) {
      jsonenc_putsep(e, ",", &first);
      jsonenc_string(e, key.str_val);
      jsonenc_putstr(e, ":");
      jsonenc_value(e, val.msg_val, hpb_FieldDef_MessageSubDef(value_f));
    }
  }

  jsonenc_putstr(e, "}");
}

static void jsonenc_listvalue(jsonenc* e, const hpb_Message* msg,
                              const hpb_MessageDef* m) {
  const hpb_FieldDef* values_f = hpb_MessageDef_FindFieldByNumber(m, 1);
  const hpb_MessageDef* values_m = hpb_FieldDef_MessageSubDef(values_f);
  const hpb_Array* values = hpb_Message_GetFieldByDef(msg, values_f).array_val;
  size_t i;
  bool first = true;

  jsonenc_putstr(e, "[");

  if (values) {
    const size_t size = hpb_Array_Size(values);
    for (i = 0; i < size; i++) {
      hpb_MessageValue elem = hpb_Array_Get(values, i);

      jsonenc_putsep(e, ",", &first);
      jsonenc_value(e, elem.msg_val, values_m);
    }
  }

  jsonenc_putstr(e, "]");
}

static void jsonenc_value(jsonenc* e, const hpb_Message* msg,
                          const hpb_MessageDef* m) {
  /* TODO(haberman): do we want a reflection method to get oneof case? */
  size_t iter = kHpb_Message_Begin;
  const hpb_FieldDef* f;
  hpb_MessageValue val;

  if (!hpb_Message_Next(msg, m, NULL, &f, &val, &iter)) {
    jsonenc_err(e, "No value set in Value proto");
  }

  switch (hpb_FieldDef_Number(f)) {
    case 1:
      jsonenc_putstr(e, "null");
      break;
    case 2:
      if (hpb_JsonEncode_HandleSpecialDoubles(e, val.double_val)) {
        jsonenc_err(
            e,
            "google.protobuf.Value cannot encode double values for "
            "infinity or nan, because they would be parsed as a string");
      }
      hpb_JsonEncode_Double(e, val.double_val);
      break;
    case 3:
      jsonenc_string(e, val.str_val);
      break;
    case 4:
      jsonenc_putstr(e, val.bool_val ? "true" : "false");
      break;
    case 5:
      jsonenc_struct(e, val.msg_val, hpb_FieldDef_MessageSubDef(f));
      break;
    case 6:
      jsonenc_listvalue(e, val.msg_val, hpb_FieldDef_MessageSubDef(f));
      break;
  }
}

static void jsonenc_msgfield(jsonenc* e, const hpb_Message* msg,
                             const hpb_MessageDef* m) {
  switch (hpb_MessageDef_WellKnownType(m)) {
    case kHpb_WellKnown_Unspecified:
      jsonenc_msg(e, msg, m);
      break;
    case kHpb_WellKnown_Any:
      jsonenc_any(e, msg, m);
      break;
    case kHpb_WellKnown_FieldMask:
      jsonenc_fieldmask(e, msg, m);
      break;
    case kHpb_WellKnown_Duration:
      jsonenc_duration(e, msg, m);
      break;
    case kHpb_WellKnown_Timestamp:
      jsonenc_timestamp(e, msg, m);
      break;
    case kHpb_WellKnown_DoubleValue:
    case kHpb_WellKnown_FloatValue:
    case kHpb_WellKnown_Int64Value:
    case kHpb_WellKnown_UInt64Value:
    case kHpb_WellKnown_Int32Value:
    case kHpb_WellKnown_UInt32Value:
    case kHpb_WellKnown_StringValue:
    case kHpb_WellKnown_BytesValue:
    case kHpb_WellKnown_BoolValue:
      jsonenc_wrapper(e, msg, m);
      break;
    case kHpb_WellKnown_Value:
      jsonenc_value(e, msg, m);
      break;
    case kHpb_WellKnown_ListValue:
      jsonenc_listvalue(e, msg, m);
      break;
    case kHpb_WellKnown_Struct:
      jsonenc_struct(e, msg, m);
      break;
  }
}

static void jsonenc_scalar(jsonenc* e, hpb_MessageValue val,
                           const hpb_FieldDef* f) {
  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Bool:
      jsonenc_putstr(e, val.bool_val ? "true" : "false");
      break;
    case kHpb_CType_Float:
      hpb_JsonEncode_Float(e, val.float_val);
      break;
    case kHpb_CType_Double:
      hpb_JsonEncode_Double(e, val.double_val);
      break;
    case kHpb_CType_Int32:
      jsonenc_printf(e, "%" PRId32, val.int32_val);
      break;
    case kHpb_CType_UInt32:
      jsonenc_printf(e, "%" PRIu32, val.uint32_val);
      break;
    case kHpb_CType_Int64:
      jsonenc_printf(e, "\"%" PRId64 "\"", val.int64_val);
      break;
    case kHpb_CType_UInt64:
      jsonenc_printf(e, "\"%" PRIu64 "\"", val.uint64_val);
      break;
    case kHpb_CType_String:
      jsonenc_string(e, val.str_val);
      break;
    case kHpb_CType_Bytes:
      jsonenc_bytes(e, val.str_val);
      break;
    case kHpb_CType_Enum:
      jsonenc_enum(val.int32_val, f, e);
      break;
    case kHpb_CType_Message:
      jsonenc_msgfield(e, val.msg_val, hpb_FieldDef_MessageSubDef(f));
      break;
  }
}

static void jsonenc_mapkey(jsonenc* e, hpb_MessageValue val,
                           const hpb_FieldDef* f) {
  jsonenc_putstr(e, "\"");

  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Bool:
      jsonenc_putstr(e, val.bool_val ? "true" : "false");
      break;
    case kHpb_CType_Int32:
      jsonenc_printf(e, "%" PRId32, val.int32_val);
      break;
    case kHpb_CType_UInt32:
      jsonenc_printf(e, "%" PRIu32, val.uint32_val);
      break;
    case kHpb_CType_Int64:
      jsonenc_printf(e, "%" PRId64, val.int64_val);
      break;
    case kHpb_CType_UInt64:
      jsonenc_printf(e, "%" PRIu64, val.uint64_val);
      break;
    case kHpb_CType_String:
      jsonenc_stringbody(e, val.str_val);
      break;
    default:
      HPB_UNREACHABLE();
  }

  jsonenc_putstr(e, "\":");
}

static void jsonenc_array(jsonenc* e, const hpb_Array* arr,
                          const hpb_FieldDef* f) {
  size_t i;
  size_t size = arr ? hpb_Array_Size(arr) : 0;
  bool first = true;

  jsonenc_putstr(e, "[");

  for (i = 0; i < size; i++) {
    jsonenc_putsep(e, ",", &first);
    jsonenc_scalar(e, hpb_Array_Get(arr, i), f);
  }

  jsonenc_putstr(e, "]");
}

static void jsonenc_map(jsonenc* e, const hpb_Map* map, const hpb_FieldDef* f) {
  jsonenc_putstr(e, "{");

  const hpb_MessageDef* entry = hpb_FieldDef_MessageSubDef(f);
  const hpb_FieldDef* key_f = hpb_MessageDef_FindFieldByNumber(entry, 1);
  const hpb_FieldDef* val_f = hpb_MessageDef_FindFieldByNumber(entry, 2);

  if (map) {
    size_t iter = kHpb_Map_Begin;
    bool first = true;

    hpb_MessageValue key, val;
    while (hpb_Map_Next(map, &key, &val, &iter)) {
      jsonenc_putsep(e, ",", &first);
      jsonenc_mapkey(e, key, key_f);
      jsonenc_scalar(e, val, val_f);
    }
  }

  jsonenc_putstr(e, "}");
}

static void jsonenc_fieldval(jsonenc* e, const hpb_FieldDef* f,
                             hpb_MessageValue val, bool* first) {
  const char* name;

  jsonenc_putsep(e, ",", first);

  if (hpb_FieldDef_IsExtension(f)) {
    // TODO: For MessageSet, I would have expected this to print the message
    // name here, but Python doesn't appear to do this. We should do more
    // research here about what various implementations do.
    jsonenc_printf(e, "\"[%s]\":", hpb_FieldDef_FullName(f));
  } else {
    if (e->options & hpb_JsonEncode_UseProtoNames) {
      name = hpb_FieldDef_Name(f);
    } else {
      name = hpb_FieldDef_JsonName(f);
    }
    jsonenc_printf(e, "\"%s\":", name);
  }

  if (hpb_FieldDef_IsMap(f)) {
    jsonenc_map(e, val.map_val, f);
  } else if (hpb_FieldDef_IsRepeated(f)) {
    jsonenc_array(e, val.array_val, f);
  } else {
    jsonenc_scalar(e, val, f);
  }
}

static void jsonenc_msgfields(jsonenc* e, const hpb_Message* msg,
                              const hpb_MessageDef* m, bool first) {
  hpb_MessageValue val;
  const hpb_FieldDef* f;

  if (e->options & hpb_JsonEncode_EmitDefaults) {
    /* Iterate over all fields. */
    int i = 0;
    int n = hpb_MessageDef_FieldCount(m);
    for (i = 0; i < n; i++) {
      f = hpb_MessageDef_Field(m, i);
      if (!hpb_FieldDef_HasPresence(f) || hpb_Message_HasFieldByDef(msg, f)) {
        jsonenc_fieldval(e, f, hpb_Message_GetFieldByDef(msg, f), &first);
      }
    }
  } else {
    /* Iterate over non-empty fields. */
    size_t iter = kHpb_Message_Begin;
    while (hpb_Message_Next(msg, m, e->ext_pool, &f, &val, &iter)) {
      jsonenc_fieldval(e, f, val, &first);
    }
  }
}

static void jsonenc_msg(jsonenc* e, const hpb_Message* msg,
                        const hpb_MessageDef* m) {
  jsonenc_putstr(e, "{");
  jsonenc_msgfields(e, msg, m, true);
  jsonenc_putstr(e, "}");
}

static size_t jsonenc_nullz(jsonenc* e, size_t size) {
  size_t ret = e->ptr - e->buf + e->overflow;

  if (size > 0) {
    if (e->ptr == e->end) e->ptr--;
    *e->ptr = '\0';
  }

  return ret;
}

static size_t hpb_JsonEncoder_Encode(jsonenc* const e,
                                     const hpb_Message* const msg,
                                     const hpb_MessageDef* const m,
                                     const size_t size) {
  if (HPB_SETJMP(e->err) != 0) return -1;

  jsonenc_msgfield(e, msg, m);
  if (e->arena) hpb_Arena_Free(e->arena);
  return jsonenc_nullz(e, size);
}

size_t hpb_JsonEncode(const hpb_Message* msg, const hpb_MessageDef* m,
                      const hpb_DefPool* ext_pool, int options, char* buf,
                      size_t size, hpb_Status* status) {
  jsonenc e;

  e.buf = buf;
  e.ptr = buf;
  e.end = HPB_PTRADD(buf, size);
  e.overflow = 0;
  e.options = options;
  e.ext_pool = ext_pool;
  e.status = status;
  e.arena = NULL;

  return hpb_JsonEncoder_Encode(&e, msg, m, size);
}
