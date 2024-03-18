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

#include "hpb/mem/arena.h"

#include <array>
#include <atomic>
#include <thread>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/synchronization/notification.h"

// Must be last.
#include "hpb/port/def.inc"

namespace {

TEST(ArenaTest, ArenaFuse) {
  hpb_Arena* arena1 = hpb_Arena_New();
  hpb_Arena* arena2 = hpb_Arena_New();

  EXPECT_TRUE(hpb_Arena_Fuse(arena1, arena2));

  hpb_Arena_Free(arena1);
  hpb_Arena_Free(arena2);
}

/* Do nothing allocator for testing */
extern "C" void* TestAllocFunc(hpb_alloc* alloc, void* ptr, size_t oldsize,
                               size_t size) {
  return hpb_alloc_global.func(alloc, ptr, oldsize, size);
}

TEST(ArenaTest, FuseWithInitialBlock) {
  char buf1[1024];
  char buf2[1024];
  hpb_Arena* arenas[] = {hpb_Arena_Init(buf1, 1024, &hpb_alloc_global),
                         hpb_Arena_Init(buf2, 1024, &hpb_alloc_global),
                         hpb_Arena_Init(NULL, 0, &hpb_alloc_global)};
  int size = sizeof(arenas) / sizeof(arenas[0]);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      if (i == j) {
        // Fuse to self is always allowed.
        EXPECT_TRUE(hpb_Arena_Fuse(arenas[i], arenas[j]));
      } else {
        EXPECT_FALSE(hpb_Arena_Fuse(arenas[i], arenas[j]));
      }
    }
  }

  for (int i = 0; i < size; ++i) hpb_Arena_Free(arenas[i]);
}

class Environment {
 public:
  ~Environment() {
    for (auto& atom : arenas_) {
      auto* a = atom.load(std::memory_order_relaxed);
      if (a != nullptr) hpb_Arena_Free(a);
    }
  }

  void RandomNewFree(absl::BitGen& gen) {
    auto* old = SwapRandomly(gen, hpb_Arena_New());
    if (old != nullptr) hpb_Arena_Free(old);
  }

  void RandomFuse(absl::BitGen& gen) {
    std::array<hpb_Arena*, 2> old;
    for (auto& o : old) {
      o = SwapRandomly(gen, nullptr);
      if (o == nullptr) o = hpb_Arena_New();
    }

    EXPECT_TRUE(hpb_Arena_Fuse(old[0], old[1]));
    for (auto& o : old) {
      o = SwapRandomly(gen, o);
      if (o != nullptr) hpb_Arena_Free(o);
    }
  }

  void RandomPoke(absl::BitGen& gen) {
    switch (absl::Uniform(gen, 0, 2)) {
      case 0:
        RandomNewFree(gen);
        break;
      case 1:
        RandomFuse(gen);
        break;
      default:
        break;
    }
  }

 private:
  hpb_Arena* SwapRandomly(absl::BitGen& gen, hpb_Arena* a) {
    return arenas_[absl::Uniform<size_t>(gen, 0, arenas_.size())].exchange(
        a, std::memory_order_acq_rel);
  }

  std::array<std::atomic<hpb_Arena*>, 100> arenas_ = {};
};

TEST(ArenaTest, FuzzSingleThreaded) {
  Environment env;

  absl::BitGen gen;
  auto end = absl::Now() + absl::Seconds(0.5);
  while (absl::Now() < end) {
    env.RandomPoke(gen);
  }
}

#ifdef HPB_USE_C11_ATOMICS

TEST(ArenaTest, FuzzFuseFreeRace) {
  Environment env;

  absl::Notification done;
  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&]() {
      absl::BitGen gen;
      while (!done.HasBeenNotified()) {
        env.RandomNewFree(gen);
      }
    });
  }

  absl::BitGen gen;
  auto end = absl::Now() + absl::Seconds(2);
  while (absl::Now() < end) {
    env.RandomFuse(gen);
  }
  done.Notify();
  for (auto& t : threads) t.join();
}

TEST(ArenaTest, FuzzFuseFuseRace) {
  Environment env;

  absl::Notification done;
  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&]() {
      absl::BitGen gen;
      while (!done.HasBeenNotified()) {
        env.RandomFuse(gen);
      }
    });
  }

  absl::BitGen gen;
  auto end = absl::Now() + absl::Seconds(2);
  while (absl::Now() < end) {
    env.RandomFuse(gen);
  }
  done.Notify();
  for (auto& t : threads) t.join();
}

#endif

}  // namespace
