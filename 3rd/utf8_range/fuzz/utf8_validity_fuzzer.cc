// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "utf8_validity.h"
#include <string_view>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  utf8_range::IsStructurallyValid(
      std::string_view(reinterpret_cast<const char *>(data), size));
  utf8_range::SpanStructurallyValid(
      std::string_view(reinterpret_cast<const char *>(data), size));
  return 0;
}
