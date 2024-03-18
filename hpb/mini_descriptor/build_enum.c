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

#include "hpb/mini_descriptor/build_enum.h"

#include "hpb/mini_descriptor/internal/decoder.h"
#include "hpb/mini_descriptor/internal/wire_constants.h"
#include "hpb/mini_table/internal/enum.h"

// Must be last.
#include "hpb/port/def.inc"

typedef struct {
  hpb_MdDecoder base;
  hpb_Arena* arena;
  hpb_MiniTableEnum* enum_table;
  uint32_t enum_value_count;
  uint32_t enum_data_count;
  uint32_t enum_data_capacity;
} hpb_MdEnumDecoder;

static size_t hpb_MiniTableEnum_Size(size_t count) {
  return sizeof(hpb_MiniTableEnum) + count * sizeof(uint32_t);
}

static hpb_MiniTableEnum* _hpb_MiniTable_AddEnumDataMember(hpb_MdEnumDecoder* d,
                                                           uint32_t val) {
  if (d->enum_data_count == d->enum_data_capacity) {
    size_t old_sz = hpb_MiniTableEnum_Size(d->enum_data_capacity);
    d->enum_data_capacity = HPB_MAX(2, d->enum_data_capacity * 2);
    size_t new_sz = hpb_MiniTableEnum_Size(d->enum_data_capacity);
    d->enum_table = hpb_Arena_Realloc(d->arena, d->enum_table, old_sz, new_sz);
    hpb_MdDecoder_CheckOutOfMemory(&d->base, d->enum_table);
  }
  d->enum_table->data[d->enum_data_count++] = val;
  return d->enum_table;
}

static void hpb_MiniTableEnum_BuildValue(hpb_MdEnumDecoder* d, uint32_t val) {
  hpb_MiniTableEnum* table = d->enum_table;
  d->enum_value_count++;
  if (table->value_count || (val > 512 && d->enum_value_count < val / 32)) {
    if (table->value_count == 0) {
      assert(d->enum_data_count == table->mask_limit / 32);
    }
    table = _hpb_MiniTable_AddEnumDataMember(d, val);
    table->value_count++;
  } else {
    uint32_t new_mask_limit = ((val / 32) + 1) * 32;
    while (table->mask_limit < new_mask_limit) {
      table = _hpb_MiniTable_AddEnumDataMember(d, 0);
      table->mask_limit += 32;
    }
    table->data[val / 32] |= 1ULL << (val % 32);
  }
}

static hpb_MiniTableEnum* hpb_MtDecoder_DoBuildMiniTableEnum(
    hpb_MdEnumDecoder* d, const char* data, size_t len) {
  // If the string is non-empty then it must begin with a version tag.
  if (len) {
    if (*data != kHpb_EncodedVersion_EnumV1) {
      hpb_MdDecoder_ErrorJmp(&d->base, "Invalid enum version: %c", *data);
    }
    data++;
    len--;
  }

  hpb_MdDecoder_CheckOutOfMemory(&d->base, d->enum_table);

  // Guarantee at least 64 bits of mask without checking mask size.
  d->enum_table->mask_limit = 64;
  d->enum_table = _hpb_MiniTable_AddEnumDataMember(d, 0);
  d->enum_table = _hpb_MiniTable_AddEnumDataMember(d, 0);

  d->enum_table->value_count = 0;

  const char* ptr = data;
  uint32_t base = 0;

  while (ptr < d->base.end) {
    char ch = *ptr++;
    if (ch <= kHpb_EncodedValue_MaxEnumMask) {
      uint32_t mask = _hpb_FromBase92(ch);
      for (int i = 0; i < 5; i++, base++, mask >>= 1) {
        if (mask & 1) hpb_MiniTableEnum_BuildValue(d, base);
      }
    } else if (kHpb_EncodedValue_MinSkip <= ch &&
               ch <= kHpb_EncodedValue_MaxSkip) {
      uint32_t skip;
      ptr = hpb_MdDecoder_DecodeBase92Varint(&d->base, ptr, ch,
                                             kHpb_EncodedValue_MinSkip,
                                             kHpb_EncodedValue_MaxSkip, &skip);
      base += skip;
    } else {
      hpb_MdDecoder_ErrorJmp(&d->base, "Unexpected character: %c", ch);
    }
  }

  return d->enum_table;
}

static hpb_MiniTableEnum* hpb_MtDecoder_BuildMiniTableEnum(
    hpb_MdEnumDecoder* const decoder, const char* const data, size_t const len) {
  if (HPB_SETJMP(decoder->base.err) != 0) return NULL;
  return hpb_MtDecoder_DoBuildMiniTableEnum(decoder, data, len);
}

hpb_MiniTableEnum* hpb_MiniDescriptor_BuildEnum(const char* data, size_t len,
                                                hpb_Arena* arena,
                                                hpb_Status* status) {
  hpb_MdEnumDecoder decoder = {
      .base =
          {
              .end = HPB_PTRADD(data, len),
              .status = status,
          },
      .arena = arena,
      .enum_table = hpb_Arena_Malloc(arena, hpb_MiniTableEnum_Size(2)),
      .enum_value_count = 0,
      .enum_data_count = 0,
      .enum_data_capacity = 1,
  };

  return hpb_MtDecoder_BuildMiniTableEnum(&decoder, data, len);
}
