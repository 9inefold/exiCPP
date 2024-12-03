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
#include <Common/SmallVec.hpp>
#include <Support/MemoryBuffer.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

using namespace exi;

int main() {
  SmallVec<usize, 3> V;
  usize N = 7;
  for (usize Ix = 0; Ix < std::numeric_limits<usize>::digits10; ++Ix) {
    V.push_back(N);
    N *= 10;
    N += 7;
  }

  fmt::println("V: {}", fmt::join(V, ", "));
}
