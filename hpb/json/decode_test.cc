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

#include "hpb/json/decode.h"

#include "google/protobuf/struct.hpb.h"
#include "gtest/gtest.h"
#include "hpb/json/test.hpb.h"
#include "hpb/json/test.hpbdefs.h"
#include "hpb/mem/arena.hpp"
#include "hpb/reflection/def.hpp"

static hpb_test_Box* JsonDecode(const char* json, hpb_Arena* a) {
  hpb::Status status;
  hpb::DefPool defpool;
  hpb::MessageDefPtr m(hpb_test_Box_getmsgdef(defpool.ptr()));
  EXPECT_TRUE(m.ptr() != nullptr);

  hpb_test_Box* box = hpb_test_Box_new(a);
  int options = 0;
  bool ok = hpb_JsonDecode(json, strlen(json), box, m.ptr(), defpool.ptr(),
                           options, a, status.ptr());
  return ok ? box : nullptr;
}

struct FloatTest {
  const std::string json;
  float f;
};

static const std::vector<FloatTest> FloatTestsPass = {
    {R"({"f": 0})", 0},
    {R"({"f": 1})", 1},
    {R"({"f": 1.000000})", 1},
    {R"({"f": 1.5e1})", 15},
    {R"({"f": 15e-1})", 1.5},
    {R"({"f": -3.5})", -3.5},
    {R"({"f": 3.402823e38})", 3.402823e38},
    {R"({"f": -3.402823e38})", -3.402823e38},
    {R"({"f": 340282346638528859811704183484516925440.0})",
     340282346638528859811704183484516925440.0},
    {R"({"f": -340282346638528859811704183484516925440.0})",
     -340282346638528859811704183484516925440.0},
};

static const std::vector<FloatTest> FloatTestsFail = {
    {R"({"f": 1z})", 0},
    {R"({"f": 3.4028236e+38})", 0},
    {R"({"f": -3.4028236e+38})", 0},
};

// Decode some floats.
TEST(JsonTest, DecodeFloats) {
  hpb::Arena a;

  for (const auto& test : FloatTestsPass) {
    hpb_test_Box* box = JsonDecode(test.json.c_str(), a.ptr());
    EXPECT_NE(box, nullptr);
    float f = hpb_test_Box_f(box);
    EXPECT_EQ(f, test.f);
  }

  for (const auto& test : FloatTestsFail) {
    hpb_test_Box* box = JsonDecode(test.json.c_str(), a.ptr());
    EXPECT_EQ(box, nullptr);
  }
}
