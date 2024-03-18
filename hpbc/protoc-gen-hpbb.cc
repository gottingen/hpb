
#include "hpbc/c_hpb.h"

int main(int argc, char** argv) {
  hpbc::DefPoolPair pools;
  hpbc::Plugin plugin;

  plugin.GenerateFilesRaw([&](const HPB_DESC(FileDescriptorProto) * file_proto,
                              bool generate) {
    hpb::Status status;
    hpb::FileDefPtr file = pools.AddFile(file_proto, &status);
    if (!file) {
      absl::string_view name =
          hpbc::ToStringView(HPB_DESC(FileDescriptorProto_name)(file_proto));
      ABSL_LOG(FATAL) << "Couldn't add file " << name
                      << " to DefPool: " << status.error_message();
    }
    hpbc::Chpb chpb(true);
    if (generate) chpb.GenerateFile(pools, file, &plugin);
  });
  return 0;
}
