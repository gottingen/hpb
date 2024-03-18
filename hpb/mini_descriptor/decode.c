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

#include "hpb/mini_descriptor/decode.h"

#include <inttypes.h>
#include <stdlib.h>

#include "hpb/base/string_view.h"
#include "hpb/mem/arena.h"
#include "hpb/mini_descriptor/internal/base92.h"
#include "hpb/mini_descriptor/internal/decoder.h"
#include "hpb/mini_descriptor/internal/modifiers.h"
#include "hpb/mini_descriptor/internal/wire_constants.h"

// Must be last.
#include "hpb/port/def.inc"

// Note: we sort by this number when calculating layout order.
typedef enum {
  kHpb_LayoutItemType_OneofCase,   // Oneof case.
  kHpb_LayoutItemType_OneofField,  // Oneof field data.
  kHpb_LayoutItemType_Field,       // Non-oneof field data.

  kHpb_LayoutItemType_Max = kHpb_LayoutItemType_Field,
} hpb_LayoutItemType;

#define kHpb_LayoutItem_IndexSentinel ((uint16_t)-1)

typedef struct {
  // Index of the corresponding field.  When this is a oneof field, the field's
  // offset will be the index of the next field in a linked list.
  uint16_t field_index;
  uint16_t offset;
  hpb_FieldRep rep;
  hpb_LayoutItemType type;
} hpb_LayoutItem;

typedef struct {
  hpb_LayoutItem* data;
  size_t size;
  size_t capacity;
} hpb_LayoutItemVector;

typedef struct {
  hpb_MdDecoder base;
  hpb_MiniTable* table;
  hpb_MiniTableField* fields;
  hpb_MiniTablePlatform platform;
  hpb_LayoutItemVector vec;
  hpb_Arena* arena;
} hpb_MtDecoder;

// In each field's offset, we temporarily store a presence classifier:
enum PresenceClass {
  kNoPresence = 0,
  kHasbitPresence = 1,
  kRequiredPresence = 2,
  kOneofBase = 3,
  // Negative values refer to a specific oneof with that number.  Positive
  // values >= kOneofBase indicate that this field is in a oneof, and specify
  // the next field in this oneof's linked list.
};

static bool hpb_MtDecoder_FieldIsPackable(hpb_MiniTableField* field) {
  return (field->mode & kHpb_FieldMode_Array) &&
         hpb_FieldType_IsPackable(field->HPB_PRIVATE(descriptortype));
}

typedef struct {
  uint16_t submsg_count;
  uint16_t subenum_count;
} hpb_SubCounts;

static void hpb_MiniTable_SetTypeAndSub(hpb_MiniTableField* field,
                                        hpb_FieldType type,
                                        hpb_SubCounts* sub_counts,
                                        uint64_t msg_modifiers,
                                        bool is_proto3_enum) {
  if (is_proto3_enum) {
    HPB_ASSERT(type == kHpb_FieldType_Enum);
    type = kHpb_FieldType_Int32;
    field->mode |= kHpb_LabelFlags_IsAlternate;
  } else if (type == kHpb_FieldType_String &&
             !(msg_modifiers & kHpb_MessageModifier_ValidateUtf8)) {
    type = kHpb_FieldType_Bytes;
    field->mode |= kHpb_LabelFlags_IsAlternate;
  }

  field->HPB_PRIVATE(descriptortype) = type;

  if (hpb_MtDecoder_FieldIsPackable(field) &&
      (msg_modifiers & kHpb_MessageModifier_DefaultIsPacked)) {
    field->mode |= kHpb_LabelFlags_IsPacked;
  }

  if (type == kHpb_FieldType_Message || type == kHpb_FieldType_Group) {
    field->HPB_PRIVATE(submsg_index) = sub_counts->submsg_count++;
  } else if (type == kHpb_FieldType_Enum) {
    // We will need to update this later once we know the total number of
    // submsg fields.
    field->HPB_PRIVATE(submsg_index) = sub_counts->subenum_count++;
  } else {
    field->HPB_PRIVATE(submsg_index) = kHpb_NoSub;
  }
}

static const char kHpb_EncodedToType[] = {
    [kHpb_EncodedType_Double] = kHpb_FieldType_Double,
    [kHpb_EncodedType_Float] = kHpb_FieldType_Float,
    [kHpb_EncodedType_Int64] = kHpb_FieldType_Int64,
    [kHpb_EncodedType_UInt64] = kHpb_FieldType_UInt64,
    [kHpb_EncodedType_Int32] = kHpb_FieldType_Int32,
    [kHpb_EncodedType_Fixed64] = kHpb_FieldType_Fixed64,
    [kHpb_EncodedType_Fixed32] = kHpb_FieldType_Fixed32,
    [kHpb_EncodedType_Bool] = kHpb_FieldType_Bool,
    [kHpb_EncodedType_String] = kHpb_FieldType_String,
    [kHpb_EncodedType_Group] = kHpb_FieldType_Group,
    [kHpb_EncodedType_Message] = kHpb_FieldType_Message,
    [kHpb_EncodedType_Bytes] = kHpb_FieldType_Bytes,
    [kHpb_EncodedType_UInt32] = kHpb_FieldType_UInt32,
    [kHpb_EncodedType_OpenEnum] = kHpb_FieldType_Enum,
    [kHpb_EncodedType_SFixed32] = kHpb_FieldType_SFixed32,
    [kHpb_EncodedType_SFixed64] = kHpb_FieldType_SFixed64,
    [kHpb_EncodedType_SInt32] = kHpb_FieldType_SInt32,
    [kHpb_EncodedType_SInt64] = kHpb_FieldType_SInt64,
    [kHpb_EncodedType_ClosedEnum] = kHpb_FieldType_Enum,
};

static void hpb_MiniTable_SetField(hpb_MtDecoder* d, uint8_t ch,
                                   hpb_MiniTableField* field,
                                   uint64_t msg_modifiers,
                                   hpb_SubCounts* sub_counts) {
  static const char kHpb_EncodedToFieldRep[] = {
      [kHpb_EncodedType_Double] = kHpb_FieldRep_8Byte,
      [kHpb_EncodedType_Float] = kHpb_FieldRep_4Byte,
      [kHpb_EncodedType_Int64] = kHpb_FieldRep_8Byte,
      [kHpb_EncodedType_UInt64] = kHpb_FieldRep_8Byte,
      [kHpb_EncodedType_Int32] = kHpb_FieldRep_4Byte,
      [kHpb_EncodedType_Fixed64] = kHpb_FieldRep_8Byte,
      [kHpb_EncodedType_Fixed32] = kHpb_FieldRep_4Byte,
      [kHpb_EncodedType_Bool] = kHpb_FieldRep_1Byte,
      [kHpb_EncodedType_String] = kHpb_FieldRep_StringView,
      [kHpb_EncodedType_Bytes] = kHpb_FieldRep_StringView,
      [kHpb_EncodedType_UInt32] = kHpb_FieldRep_4Byte,
      [kHpb_EncodedType_OpenEnum] = kHpb_FieldRep_4Byte,
      [kHpb_EncodedType_SFixed32] = kHpb_FieldRep_4Byte,
      [kHpb_EncodedType_SFixed64] = kHpb_FieldRep_8Byte,
      [kHpb_EncodedType_SInt32] = kHpb_FieldRep_4Byte,
      [kHpb_EncodedType_SInt64] = kHpb_FieldRep_8Byte,
      [kHpb_EncodedType_ClosedEnum] = kHpb_FieldRep_4Byte,
  };

  char pointer_rep = d->platform == kHpb_MiniTablePlatform_32Bit
                         ? kHpb_FieldRep_4Byte
                         : kHpb_FieldRep_8Byte;

  int8_t type = _hpb_FromBase92(ch);
  if (ch >= _hpb_ToBase92(kHpb_EncodedType_RepeatedBase)) {
    type -= kHpb_EncodedType_RepeatedBase;
    field->mode = kHpb_FieldMode_Array;
    field->mode |= pointer_rep << kHpb_FieldRep_Shift;
    field->offset = kNoPresence;
  } else {
    field->mode = kHpb_FieldMode_Scalar;
    field->offset = kHasbitPresence;
    if (type == kHpb_EncodedType_Group || type == kHpb_EncodedType_Message) {
      field->mode |= pointer_rep << kHpb_FieldRep_Shift;
    } else if ((unsigned long)type >= sizeof(kHpb_EncodedToFieldRep)) {
      hpb_MdDecoder_ErrorJmp(&d->base, "Invalid field type: %d", (int)type);
    } else {
      field->mode |= kHpb_EncodedToFieldRep[type] << kHpb_FieldRep_Shift;
    }
  }
  if ((unsigned long)type >= sizeof(kHpb_EncodedToType)) {
    hpb_MdDecoder_ErrorJmp(&d->base, "Invalid field type: %d", (int)type);
  }
  hpb_MiniTable_SetTypeAndSub(field, kHpb_EncodedToType[type], sub_counts,
                              msg_modifiers, type == kHpb_EncodedType_OpenEnum);
}

static void hpb_MtDecoder_ModifyField(hpb_MtDecoder* d,
                                      uint32_t message_modifiers,
                                      uint32_t field_modifiers,
                                      hpb_MiniTableField* field) {
  if (field_modifiers & kHpb_EncodedFieldModifier_FlipPacked) {
    if (!hpb_MtDecoder_FieldIsPackable(field)) {
      hpb_MdDecoder_ErrorJmp(&d->base,
                             "Cannot flip packed on unpackable field %" PRIu32,
                             field->number);
    }
    field->mode ^= kHpb_LabelFlags_IsPacked;
  }

  bool singular = field_modifiers & kHpb_EncodedFieldModifier_IsProto3Singular;
  bool required = field_modifiers & kHpb_EncodedFieldModifier_IsRequired;

  // Validate.
  if ((singular || required) && field->offset != kHasbitPresence) {
    hpb_MdDecoder_ErrorJmp(&d->base,
                           "Invalid modifier(s) for repeated field %" PRIu32,
                           field->number);
  }
  if (singular && required) {
    hpb_MdDecoder_ErrorJmp(
        &d->base, "Field %" PRIu32 " cannot be both singular and required",
        field->number);
  }

  if (singular) field->offset = kNoPresence;
  if (required) {
    field->offset = kRequiredPresence;
  }
}

static void hpb_MtDecoder_PushItem(hpb_MtDecoder* d, hpb_LayoutItem item) {
  if (d->vec.size == d->vec.capacity) {
    size_t new_cap = HPB_MAX(8, d->vec.size * 2);
    d->vec.data = realloc(d->vec.data, new_cap * sizeof(*d->vec.data));
    hpb_MdDecoder_CheckOutOfMemory(&d->base, d->vec.data);
    d->vec.capacity = new_cap;
  }
  d->vec.data[d->vec.size++] = item;
}

static void hpb_MtDecoder_PushOneof(hpb_MtDecoder* d, hpb_LayoutItem item) {
  if (item.field_index == kHpb_LayoutItem_IndexSentinel) {
    hpb_MdDecoder_ErrorJmp(&d->base, "Empty oneof");
  }
  item.field_index -= kOneofBase;

  // Push oneof data.
  item.type = kHpb_LayoutItemType_OneofField;
  hpb_MtDecoder_PushItem(d, item);

  // Push oneof case.
  item.rep = kHpb_FieldRep_4Byte;  // Field Number.
  item.type = kHpb_LayoutItemType_OneofCase;
  hpb_MtDecoder_PushItem(d, item);
}

size_t hpb_MtDecoder_SizeOfRep(hpb_FieldRep rep,
                               hpb_MiniTablePlatform platform) {
  static const uint8_t kRepToSize32[] = {
      [kHpb_FieldRep_1Byte] = 1,
      [kHpb_FieldRep_4Byte] = 4,
      [kHpb_FieldRep_StringView] = 8,
      [kHpb_FieldRep_8Byte] = 8,
  };
  static const uint8_t kRepToSize64[] = {
      [kHpb_FieldRep_1Byte] = 1,
      [kHpb_FieldRep_4Byte] = 4,
      [kHpb_FieldRep_StringView] = 16,
      [kHpb_FieldRep_8Byte] = 8,
  };
  HPB_ASSERT(sizeof(hpb_StringView) ==
             HPB_SIZE(kRepToSize32, kRepToSize64)[kHpb_FieldRep_StringView]);
  return platform == kHpb_MiniTablePlatform_32Bit ? kRepToSize32[rep]
                                                  : kRepToSize64[rep];
}

size_t hpb_MtDecoder_AlignOfRep(hpb_FieldRep rep,
                                hpb_MiniTablePlatform platform) {
  static const uint8_t kRepToAlign32[] = {
      [kHpb_FieldRep_1Byte] = 1,
      [kHpb_FieldRep_4Byte] = 4,
      [kHpb_FieldRep_StringView] = 4,
      [kHpb_FieldRep_8Byte] = 8,
  };
  static const uint8_t kRepToAlign64[] = {
      [kHpb_FieldRep_1Byte] = 1,
      [kHpb_FieldRep_4Byte] = 4,
      [kHpb_FieldRep_StringView] = 8,
      [kHpb_FieldRep_8Byte] = 8,
  };
  HPB_ASSERT(HPB_ALIGN_OF(hpb_StringView) ==
             HPB_SIZE(kRepToAlign32, kRepToAlign64)[kHpb_FieldRep_StringView]);
  return platform == kHpb_MiniTablePlatform_32Bit ? kRepToAlign32[rep]
                                                  : kRepToAlign64[rep];
}

static const char* hpb_MtDecoder_DecodeOneofField(hpb_MtDecoder* d,
                                                  const char* ptr,
                                                  char first_ch,
                                                  hpb_LayoutItem* item) {
  uint32_t field_num;
  ptr = hpb_MdDecoder_DecodeBase92Varint(
      &d->base, ptr, first_ch, kHpb_EncodedValue_MinOneofField,
      kHpb_EncodedValue_MaxOneofField, &field_num);
  hpb_MiniTableField* f =
      (void*)hpb_MiniTable_FindFieldByNumber(d->table, field_num);

  if (!f) {
    hpb_MdDecoder_ErrorJmp(&d->base,
                           "Couldn't add field number %" PRIu32
                           " to oneof, no such field number.",
                           field_num);
  }
  if (f->offset != kHasbitPresence) {
    hpb_MdDecoder_ErrorJmp(
        &d->base,
        "Cannot add repeated, required, or singular field %" PRIu32
        " to oneof.",
        field_num);
  }

  // Oneof storage must be large enough to accommodate the largest member.
  int rep = f->mode >> kHpb_FieldRep_Shift;
  if (hpb_MtDecoder_SizeOfRep(rep, d->platform) >
      hpb_MtDecoder_SizeOfRep(item->rep, d->platform)) {
    item->rep = rep;
  }
  // Prepend this field to the linked list.
  f->offset = item->field_index;
  item->field_index = (f - d->fields) + kOneofBase;
  return ptr;
}

static const char* hpb_MtDecoder_DecodeOneofs(hpb_MtDecoder* d,
                                              const char* ptr) {
  hpb_LayoutItem item = {.rep = 0,
                         .field_index = kHpb_LayoutItem_IndexSentinel};
  while (ptr < d->base.end) {
    char ch = *ptr++;
    if (ch == kHpb_EncodedValue_FieldSeparator) {
      // Field separator, no action needed.
    } else if (ch == kHpb_EncodedValue_OneofSeparator) {
      // End of oneof.
      hpb_MtDecoder_PushOneof(d, item);
      item.field_index = kHpb_LayoutItem_IndexSentinel;  // Move to next oneof.
    } else {
      ptr = hpb_MtDecoder_DecodeOneofField(d, ptr, ch, &item);
    }
  }

  // Push final oneof.
  hpb_MtDecoder_PushOneof(d, item);
  return ptr;
}

static const char* hpb_MtDecoder_ParseModifier(hpb_MtDecoder* d,
                                               const char* ptr, char first_ch,
                                               hpb_MiniTableField* last_field,
                                               uint64_t* msg_modifiers) {
  uint32_t mod;
  ptr = hpb_MdDecoder_DecodeBase92Varint(&d->base, ptr, first_ch,
                                         kHpb_EncodedValue_MinModifier,
                                         kHpb_EncodedValue_MaxModifier, &mod);
  if (last_field) {
    hpb_MtDecoder_ModifyField(d, *msg_modifiers, mod, last_field);
  } else {
    if (!d->table) {
      hpb_MdDecoder_ErrorJmp(&d->base,
                             "Extensions cannot have message modifiers");
    }
    *msg_modifiers = mod;
  }

  return ptr;
}

static void hpb_MtDecoder_AllocateSubs(hpb_MtDecoder* d,
                                       hpb_SubCounts sub_counts) {
  uint32_t total_count = sub_counts.submsg_count + sub_counts.subenum_count;
  size_t subs_bytes = sizeof(*d->table->subs) * total_count;
  hpb_MiniTableSub* subs = hpb_Arena_Malloc(d->arena, subs_bytes);
  hpb_MdDecoder_CheckOutOfMemory(&d->base, subs);
  uint32_t i = 0;
  for (; i < sub_counts.submsg_count; i++) {
    subs[i].submsg = &_kHpb_MiniTable_Empty;
  }
  if (sub_counts.subenum_count) {
    hpb_MiniTableField* f = d->fields;
    hpb_MiniTableField* end_f = f + d->table->field_count;
    for (; f < end_f; f++) {
      if (f->HPB_PRIVATE(descriptortype) == kHpb_FieldType_Enum) {
        f->HPB_PRIVATE(submsg_index) += sub_counts.submsg_count;
      }
    }
    for (; i < sub_counts.submsg_count + sub_counts.subenum_count; i++) {
      subs[i].subenum = NULL;
    }
  }
  d->table->subs = subs;
}

static const char* hpb_MtDecoder_Parse(hpb_MtDecoder* d, const char* ptr,
                                       size_t len, void* fields,
                                       size_t field_size, uint16_t* field_count,
                                       hpb_SubCounts* sub_counts) {
  uint64_t msg_modifiers = 0;
  uint32_t last_field_number = 0;
  hpb_MiniTableField* last_field = NULL;
  bool need_dense_below = d->table != NULL;

  d->base.end = HPB_PTRADD(ptr, len);

  while (ptr < d->base.end) {
    char ch = *ptr++;
    if (ch <= kHpb_EncodedValue_MaxField) {
      if (!d->table && last_field) {
        // For extensions, consume only a single field and then return.
        return --ptr;
      }
      hpb_MiniTableField* field = fields;
      *field_count += 1;
      fields = (char*)fields + field_size;
      field->number = ++last_field_number;
      last_field = field;
      hpb_MiniTable_SetField(d, ch, field, msg_modifiers, sub_counts);
    } else if (kHpb_EncodedValue_MinModifier <= ch &&
               ch <= kHpb_EncodedValue_MaxModifier) {
      ptr = hpb_MtDecoder_ParseModifier(d, ptr, ch, last_field, &msg_modifiers);
      if (msg_modifiers & kHpb_MessageModifier_IsExtendable) {
        d->table->ext |= kHpb_ExtMode_Extendable;
      }
    } else if (ch == kHpb_EncodedValue_End) {
      if (!d->table) {
        hpb_MdDecoder_ErrorJmp(&d->base, "Extensions cannot have oneofs.");
      }
      ptr = hpb_MtDecoder_DecodeOneofs(d, ptr);
    } else if (kHpb_EncodedValue_MinSkip <= ch &&
               ch <= kHpb_EncodedValue_MaxSkip) {
      if (need_dense_below) {
        d->table->dense_below = d->table->field_count;
        need_dense_below = false;
      }
      uint32_t skip;
      ptr = hpb_MdDecoder_DecodeBase92Varint(&d->base, ptr, ch,
                                             kHpb_EncodedValue_MinSkip,
                                             kHpb_EncodedValue_MaxSkip, &skip);
      last_field_number += skip;
      last_field_number--;  // Next field seen will increment.
    } else {
      hpb_MdDecoder_ErrorJmp(&d->base, "Invalid char: %c", ch);
    }
  }

  if (need_dense_below) {
    d->table->dense_below = d->table->field_count;
  }

  return ptr;
}

static void hpb_MtDecoder_ParseMessage(hpb_MtDecoder* d, const char* data,
                                       size_t len) {
  // Buffer length is an upper bound on the number of fields. We will return
  // what we don't use.
  d->fields = hpb_Arena_Malloc(d->arena, sizeof(*d->fields) * len);
  hpb_MdDecoder_CheckOutOfMemory(&d->base, d->fields);

  hpb_SubCounts sub_counts = {0, 0};
  d->table->field_count = 0;
  d->table->fields = d->fields;
  hpb_MtDecoder_Parse(d, data, len, d->fields, sizeof(*d->fields),
                      &d->table->field_count, &sub_counts);

  hpb_Arena_ShrinkLast(d->arena, d->fields, sizeof(*d->fields) * len,
                       sizeof(*d->fields) * d->table->field_count);
  d->table->fields = d->fields;
  hpb_MtDecoder_AllocateSubs(d, sub_counts);
}

int hpb_MtDecoder_CompareFields(const void* _a, const void* _b) {
  const hpb_LayoutItem* a = _a;
  const hpb_LayoutItem* b = _b;
  // Currently we just sort by:
  //  1. rep (smallest fields first)
  //  2. type (oneof cases first)
  //  2. field_index (smallest numbers first)
  // The main goal of this is to reduce space lost to padding.
  // Later we may have more subtle reasons to prefer a different ordering.
  const int rep_bits = hpb_Log2Ceiling(kHpb_FieldRep_Max);
  const int type_bits = hpb_Log2Ceiling(kHpb_LayoutItemType_Max);
  const int idx_bits = (sizeof(a->field_index) * 8);
  HPB_ASSERT(idx_bits + rep_bits + type_bits < 32);
#define HPB_COMBINE(rep, ty, idx) (((rep << type_bits) | ty) << idx_bits) | idx
  uint32_t a_packed = HPB_COMBINE(a->rep, a->type, a->field_index);
  uint32_t b_packed = HPB_COMBINE(b->rep, b->type, b->field_index);
  assert(a_packed != b_packed);
#undef HPB_COMBINE
  return a_packed < b_packed ? -1 : 1;
}

static bool hpb_MtDecoder_SortLayoutItems(hpb_MtDecoder* d) {
  // Add items for all non-oneof fields (oneofs were already added).
  int n = d->table->field_count;
  for (int i = 0; i < n; i++) {
    hpb_MiniTableField* f = &d->fields[i];
    if (f->offset >= kOneofBase) continue;
    hpb_LayoutItem item = {.field_index = i,
                           .rep = f->mode >> kHpb_FieldRep_Shift,
                           .type = kHpb_LayoutItemType_Field};
    hpb_MtDecoder_PushItem(d, item);
  }

  if (d->vec.size) {
    qsort(d->vec.data, d->vec.size, sizeof(*d->vec.data),
          hpb_MtDecoder_CompareFields);
  }

  return true;
}

static size_t hpb_MiniTable_DivideRoundUp(size_t n, size_t d) {
  return (n + d - 1) / d;
}

static void hpb_MtDecoder_AssignHasbits(hpb_MtDecoder* d) {
  hpb_MiniTable* ret = d->table;
  int n = ret->field_count;
  int last_hasbit = 0;  // 0 cannot be used.

  // First assign required fields, which must have the lowest hasbits.
  for (int i = 0; i < n; i++) {
    hpb_MiniTableField* field = (hpb_MiniTableField*)&ret->fields[i];
    if (field->offset == kRequiredPresence) {
      field->presence = ++last_hasbit;
    } else if (field->offset == kNoPresence) {
      field->presence = 0;
    }
  }
  ret->required_count = last_hasbit;

  if (ret->required_count > 63) {
    hpb_MdDecoder_ErrorJmp(&d->base, "Too many required fields");
  }

  // Next assign non-required hasbit fields.
  for (int i = 0; i < n; i++) {
    hpb_MiniTableField* field = (hpb_MiniTableField*)&ret->fields[i];
    if (field->offset == kHasbitPresence) {
      field->presence = ++last_hasbit;
    }
  }

  ret->size = last_hasbit ? hpb_MiniTable_DivideRoundUp(last_hasbit + 1, 8) : 0;
}

size_t hpb_MtDecoder_Place(hpb_MtDecoder* d, hpb_FieldRep rep) {
  size_t size = hpb_MtDecoder_SizeOfRep(rep, d->platform);
  size_t align = hpb_MtDecoder_AlignOfRep(rep, d->platform);
  size_t ret = HPB_ALIGN_UP(d->table->size, align);
  static const size_t max = UINT16_MAX;
  size_t new_size = ret + size;
  if (new_size > max) {
    hpb_MdDecoder_ErrorJmp(
        &d->base, "Message size exceeded maximum size of %zu bytes", max);
  }
  d->table->size = new_size;
  return ret;
}

static void hpb_MtDecoder_AssignOffsets(hpb_MtDecoder* d) {
  hpb_LayoutItem* end = HPB_PTRADD(d->vec.data, d->vec.size);

  // Compute offsets.
  for (hpb_LayoutItem* item = d->vec.data; item < end; item++) {
    item->offset = hpb_MtDecoder_Place(d, item->rep);
  }

  // Assign oneof case offsets.  We must do these first, since assigning
  // actual offsets will overwrite the links of the linked list.
  for (hpb_LayoutItem* item = d->vec.data; item < end; item++) {
    if (item->type != kHpb_LayoutItemType_OneofCase) continue;
    hpb_MiniTableField* f = &d->fields[item->field_index];
    while (true) {
      f->presence = ~item->offset;
      if (f->offset == kHpb_LayoutItem_IndexSentinel) break;
      HPB_ASSERT(f->offset - kOneofBase < d->table->field_count);
      f = &d->fields[f->offset - kOneofBase];
    }
  }

  // Assign offsets.
  for (hpb_LayoutItem* item = d->vec.data; item < end; item++) {
    hpb_MiniTableField* f = &d->fields[item->field_index];
    switch (item->type) {
      case kHpb_LayoutItemType_OneofField:
        while (true) {
          uint16_t next_offset = f->offset;
          f->offset = item->offset;
          if (next_offset == kHpb_LayoutItem_IndexSentinel) break;
          f = &d->fields[next_offset - kOneofBase];
        }
        break;
      case kHpb_LayoutItemType_Field:
        f->offset = item->offset;
        break;
      default:
        break;
    }
  }

  // The fasttable parser (supported on 64-bit only) depends on this being a
  // multiple of 8 in order to satisfy HPB_MALLOC_ALIGN, which is also 8.
  //
  // On 32-bit we could potentially make this smaller, but there is no
  // compelling reason to optimize this right now.
  d->table->size = HPB_ALIGN_UP(d->table->size, 8);
}

static void hpb_MtDecoder_ValidateEntryField(hpb_MtDecoder* d,
                                             const hpb_MiniTableField* f,
                                             uint32_t expected_num) {
  const char* name = expected_num == 1 ? "key" : "val";
  if (f->number != expected_num) {
    hpb_MdDecoder_ErrorJmp(&d->base,
                           "map %s did not have expected number (%d vs %d)",
                           name, expected_num, (int)f->number);
  }

  if (hpb_IsRepeatedOrMap(f)) {
    hpb_MdDecoder_ErrorJmp(
        &d->base, "map %s cannot be repeated or map, or be in oneof", name);
  }

  uint32_t not_ok_types;
  if (expected_num == 1) {
    not_ok_types = (1 << kHpb_FieldType_Float) | (1 << kHpb_FieldType_Double) |
                   (1 << kHpb_FieldType_Message) | (1 << kHpb_FieldType_Group) |
                   (1 << kHpb_FieldType_Bytes) | (1 << kHpb_FieldType_Enum);
  } else {
    not_ok_types = 1 << kHpb_FieldType_Group;
  }

  if ((1 << hpb_MiniTableField_Type(f)) & not_ok_types) {
    hpb_MdDecoder_ErrorJmp(&d->base, "map %s cannot have type %d", name,
                           (int)f->HPB_PRIVATE(descriptortype));
  }
}

static void hpb_MtDecoder_ParseMap(hpb_MtDecoder* d, const char* data,
                                   size_t len) {
  hpb_MtDecoder_ParseMessage(d, data, len);
  hpb_MtDecoder_AssignHasbits(d);

  if (HPB_UNLIKELY(d->table->field_count != 2)) {
    hpb_MdDecoder_ErrorJmp(&d->base, "%hu fields in map",
                           d->table->field_count);
    HPB_UNREACHABLE();
  }

  hpb_LayoutItem* end = HPB_PTRADD(d->vec.data, d->vec.size);
  for (hpb_LayoutItem* item = d->vec.data; item < end; item++) {
    if (item->type == kHpb_LayoutItemType_OneofCase) {
      hpb_MdDecoder_ErrorJmp(&d->base, "Map entry cannot have oneof");
    }
  }

  hpb_MtDecoder_ValidateEntryField(d, &d->table->fields[0], 1);
  hpb_MtDecoder_ValidateEntryField(d, &d->table->fields[1], 2);

  // Map entries have a pre-determined layout, regardless of types.
  // NOTE: sync with mini_table/message_internal.h.
  const size_t kv_size = d->platform == kHpb_MiniTablePlatform_32Bit ? 8 : 16;
  const size_t hasbit_size = 8;
  d->fields[0].offset = hasbit_size;
  d->fields[1].offset = hasbit_size + kv_size;
  d->table->size = HPB_ALIGN_UP(hasbit_size + kv_size + kv_size, 8);

  // Map entries have a special bit set to signal it's a map entry, used in
  // hpb_MiniTable_SetSubMessage() below.
  d->table->ext |= kHpb_ExtMode_IsMapEntry;
}

static void hpb_MtDecoder_ParseMessageSet(hpb_MtDecoder* d, const char* data,
                                          size_t len) {
  if (len > 0) {
    hpb_MdDecoder_ErrorJmp(&d->base, "Invalid message set encode length: %zu",
                           len);
  }

  hpb_MiniTable* ret = d->table;
  ret->size = 0;
  ret->field_count = 0;
  ret->ext = kHpb_ExtMode_IsMessageSet;
  ret->dense_below = 0;
  ret->table_mask = -1;
  ret->required_count = 0;
}

static hpb_MiniTable* hpb_MtDecoder_DoBuildMiniTableWithBuf(
    hpb_MtDecoder* decoder, const char* data, size_t len, void** buf,
    size_t* buf_size) {
  hpb_MdDecoder_CheckOutOfMemory(&decoder->base, decoder->table);

  decoder->table->size = 0;
  decoder->table->field_count = 0;
  decoder->table->ext = kHpb_ExtMode_NonExtendable;
  decoder->table->dense_below = 0;
  decoder->table->table_mask = -1;
  decoder->table->required_count = 0;

  // Strip off and verify the version tag.
  if (!len--) goto done;
  const char vers = *data++;

  switch (vers) {
    case kHpb_EncodedVersion_MapV1:
      hpb_MtDecoder_ParseMap(decoder, data, len);
      break;

    case kHpb_EncodedVersion_MessageV1:
      hpb_MtDecoder_ParseMessage(decoder, data, len);
      hpb_MtDecoder_AssignHasbits(decoder);
      hpb_MtDecoder_SortLayoutItems(decoder);
      hpb_MtDecoder_AssignOffsets(decoder);
      break;

    case kHpb_EncodedVersion_MessageSetV1:
      hpb_MtDecoder_ParseMessageSet(decoder, data, len);
      break;

    default:
      hpb_MdDecoder_ErrorJmp(&decoder->base, "Invalid message version: %c",
                             vers);
  }

done:
  *buf = decoder->vec.data;
  *buf_size = decoder->vec.capacity * sizeof(*decoder->vec.data);
  return decoder->table;
}

static hpb_MiniTable* hpb_MtDecoder_BuildMiniTableWithBuf(
    hpb_MtDecoder* const decoder, const char* const data, const size_t len,
    void** const buf, size_t* const buf_size) {
  if (HPB_SETJMP(decoder->base.err) != 0) {
    *buf = decoder->vec.data;
    *buf_size = decoder->vec.capacity * sizeof(*decoder->vec.data);
    return NULL;
  }

  return hpb_MtDecoder_DoBuildMiniTableWithBuf(decoder, data, len, buf,
                                               buf_size);
}

hpb_MiniTable* hpb_MiniTable_BuildWithBuf(const char* data, size_t len,
                                          hpb_MiniTablePlatform platform,
                                          hpb_Arena* arena, void** buf,
                                          size_t* buf_size,
                                          hpb_Status* status) {
  hpb_MtDecoder decoder = {
      .base = {.status = status},
      .platform = platform,
      .vec =
          {
              .data = *buf,
              .capacity = *buf_size / sizeof(*decoder.vec.data),
              .size = 0,
          },
      .arena = arena,
      .table = hpb_Arena_Malloc(arena, sizeof(*decoder.table)),
  };

  return hpb_MtDecoder_BuildMiniTableWithBuf(&decoder, data, len, buf,
                                             buf_size);
}

static const char* hpb_MtDecoder_DoBuildMiniTableExtension(
    hpb_MtDecoder* decoder, const char* data, size_t len,
    hpb_MiniTableExtension* ext, const hpb_MiniTable* extendee,
    hpb_MiniTableSub sub) {
  // If the string is non-empty then it must begin with a version tag.
  if (len) {
    if (*data != kHpb_EncodedVersion_ExtensionV1) {
      hpb_MdDecoder_ErrorJmp(&decoder->base, "Invalid ext version: %c", *data);
    }
    data++;
    len--;
  }

  uint16_t count = 0;
  hpb_SubCounts sub_counts = {0, 0};
  const char* ret = hpb_MtDecoder_Parse(decoder, data, len, ext, sizeof(*ext),
                                        &count, &sub_counts);
  if (!ret || count != 1) return NULL;

  hpb_MiniTableField* f = &ext->field;

  f->mode |= kHpb_LabelFlags_IsExtension;
  f->offset = 0;
  f->presence = 0;

  if (extendee->ext & kHpb_ExtMode_IsMessageSet) {
    // Extensions of MessageSet must be messages.
    if (!hpb_IsSubMessage(f)) return NULL;

    // Extensions of MessageSet must be non-repeating.
    if ((f->mode & kHpb_FieldMode_Mask) == kHpb_FieldMode_Array) return NULL;
  }

  ext->extendee = extendee;
  ext->sub = sub;

  return ret;
}

static const char* hpb_MtDecoder_BuildMiniTableExtension(
    hpb_MtDecoder* const decoder, const char* const data, const size_t len,
    hpb_MiniTableExtension* const ext, const hpb_MiniTable* const extendee,
    const hpb_MiniTableSub sub) {
  if (HPB_SETJMP(decoder->base.err) != 0) return NULL;
  return hpb_MtDecoder_DoBuildMiniTableExtension(decoder, data, len, ext,
                                                 extendee, sub);
}

const char* _hpb_MiniTableExtension_Init(const char* data, size_t len,
                                         hpb_MiniTableExtension* ext,
                                         const hpb_MiniTable* extendee,
                                         hpb_MiniTableSub sub,
                                         hpb_MiniTablePlatform platform,
                                         hpb_Status* status) {
  hpb_MtDecoder decoder = {
      .base = {.status = status},
      .arena = NULL,
      .table = NULL,
      .platform = platform,
  };

  return hpb_MtDecoder_BuildMiniTableExtension(&decoder, data, len, ext,
                                               extendee, sub);
}

hpb_MiniTableExtension* _hpb_MiniTableExtension_Build(
    const char* data, size_t len, const hpb_MiniTable* extendee,
    hpb_MiniTableSub sub, hpb_MiniTablePlatform platform, hpb_Arena* arena,
    hpb_Status* status) {
  hpb_MiniTableExtension* ext =
      hpb_Arena_Malloc(arena, sizeof(hpb_MiniTableExtension));
  if (HPB_UNLIKELY(!ext)) return NULL;

  const char* ptr = _hpb_MiniTableExtension_Init(data, len, ext, extendee, sub,
                                                 platform, status);
  if (HPB_UNLIKELY(!ptr)) return NULL;

  return ext;
}

hpb_MiniTable* _hpb_MiniTable_Build(const char* data, size_t len,
                                    hpb_MiniTablePlatform platform,
                                    hpb_Arena* arena, hpb_Status* status) {
  void* buf = NULL;
  size_t size = 0;
  hpb_MiniTable* ret = hpb_MiniTable_BuildWithBuf(data, len, platform, arena,
                                                  &buf, &size, status);
  free(buf);
  return ret;
}
