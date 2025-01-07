//===- Driver.cpp ---------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//

#include <Common/Box.hpp>
#include <Common/Map.hpp>
#include <Common/String.hpp>
#include <Common/SmallStr.hpp>
#include <Common/SmallVec.hpp>
#include <Common/StringSwitch.hpp>
#include <Support/Chrono.hpp>
#include <Support/Casting.hpp>
#include <Support/FmtBuffer.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/raw_ostream.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

#include <tuple>

using namespace exi;
using namespace exi::sys;

int main() {
  SmallVec<usize, 3> V;
  usize N = 7;
  for (usize Ix = 0; Ix < std::numeric_limits<usize>::digits10; ++Ix) {
    V.push_back(N);
    N *= 10;
    N += 7;
  }
  fmt::println("V: {}", fmt::join(V, ", "));

  // TODO: Test raw_ostream
  TimePoint<> TP = sys::now();
  fmt::println("TimePoint<>: {}", TP);
  TimePoint<> TP2 = sys::now();
  fmt::println("Duration: {}", (TP2 - TP));

  std::allocator<char> CA {};
  char* P = CA.allocate(128);
  if (mi_is_in_heap_region(P))
    fmt::println("In heap!");
  CA.deallocate(P, 128);

  SmallStr<256> Inl {};
  {
    raw_svector_ostream V(Inl);
    V << "hello" << ' ' << "world" << '!';
  }

  int I = StringSwitch<int>(Inl.str())
    .Case("Hello",            0)
    .CaseLower("Hello",       1)
    .StartsWith("Hello",      2)
    .StartsWithLower("Hello", 3)
    .Default(4);
  exi_assert(I == 3, "StringSwitch failed!");
  fmt::println("{}", I);
}
