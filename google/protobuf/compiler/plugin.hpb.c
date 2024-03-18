#include <stddef.h>
#include "hpb/generated_code_support.h"
#include "google/protobuf/compiler/plugin.hpb.h"

#include "google/protobuf/descriptor.hpb.h"
static hpb_Arena* hpb_BootstrapArena() {
  static hpb_Arena* arena = NULL;
  if (!arena) arena = hpb_Arena_New();
  return arena;
}

const hpb_MiniTable* google_protobuf_compiler_Version_msg_init() {
  static hpb_MiniTable* mini_table = NULL;
  static const char* mini_descriptor = "$(((1";
  if (mini_table) return mini_table;
  mini_table =
      hpb_MiniTable_Build(mini_descriptor, strlen(mini_descriptor),
                          hpb_BootstrapArena(), NULL);
  return mini_table;
}

const hpb_MiniTable* google_protobuf_compiler_CodeGeneratorRequest_msg_init() {
  static hpb_MiniTable* mini_table = NULL;
  static const char* mini_descriptor = "$E13kG";
  if (mini_table) return mini_table;
  mini_table =
      hpb_MiniTable_Build(mini_descriptor, strlen(mini_descriptor),
                          hpb_BootstrapArena(), NULL);
  hpb_MiniTable_SetSubMessage(mini_table, (hpb_MiniTableField*)hpb_MiniTable_FindFieldByNumber(mini_table, 15), google_protobuf_FileDescriptorProto_msg_init());
  hpb_MiniTable_SetSubMessage(mini_table, (hpb_MiniTableField*)hpb_MiniTable_FindFieldByNumber(mini_table, 3), google_protobuf_compiler_Version_msg_init());
  return mini_table;
}

const hpb_MiniTable* google_protobuf_compiler_CodeGeneratorResponse_msg_init() {
  static hpb_MiniTable* mini_table = NULL;
  static const char* mini_descriptor = "$1,lG";
  if (mini_table) return mini_table;
  mini_table =
      hpb_MiniTable_Build(mini_descriptor, strlen(mini_descriptor),
                          hpb_BootstrapArena(), NULL);
  hpb_MiniTable_SetSubMessage(mini_table, (hpb_MiniTableField*)hpb_MiniTable_FindFieldByNumber(mini_table, 15), google_protobuf_compiler_CodeGeneratorResponse_File_msg_init());
  return mini_table;
}

const hpb_MiniTable* google_protobuf_compiler_CodeGeneratorResponse_File_msg_init() {
  static hpb_MiniTable* mini_table = NULL;
  static const char* mini_descriptor = "$11l13";
  if (mini_table) return mini_table;
  mini_table =
      hpb_MiniTable_Build(mini_descriptor, strlen(mini_descriptor),
                          hpb_BootstrapArena(), NULL);
  hpb_MiniTable_SetSubMessage(mini_table, (hpb_MiniTableField*)hpb_MiniTable_FindFieldByNumber(mini_table, 16), google_protobuf_GeneratedCodeInfo_msg_init());
  return mini_table;
}

const hpb_MiniTableEnum* google_protobuf_compiler_CodeGeneratorResponse_Feature_enum_init() {
  static const hpb_MiniTableEnum* mini_table = NULL;
  static const char* mini_descriptor = "!$";
  if (mini_table) return mini_table;
  mini_table =
      hpb_MiniTableEnum_Build(mini_descriptor, strlen(mini_descriptor),
                              hpb_BootstrapArena(), NULL);
  return mini_table;
}
