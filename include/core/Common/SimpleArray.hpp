//===- Common/SimpleArray.hpp ---------------------------------------===//
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
///
/// \file
/// This file a simple array wrapper. This can reduce instantiations in
/// templates and is simpler to parse.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <cstring>
#include <type_traits>

namespace exi {

template <typename T, usize N>
struct SimpleArray { T Data[N]; };

template <typename T, typename...Rest>
SimpleArray(T&&, Rest&&...) -> SimpleArray<
  std::decay_t<T>, sizeof...(Rest) + 1>;

template <typename T, usize N>
constexpr void swap(SimpleArray<T, N>& LHS, SimpleArray<T, N>& RHS) {
  constexpr_static bool TriviallySwappable
    =  std::is_trivially_copyable_v<T>
    && std::is_trivially_constructible_v<T>;

  if (std::is_constant_evaluated() || !TriviallySwappable) {
    using std::swap;
    for (usize Ix = 0; Ix < N; ++Ix)
      swap(LHS.Data[Ix], RHS.Data[Ix]);
    return;
  }
  
  if constexpr (TriviallySwappable) {
#if __AVX__
    constexpr usize Align = (sizeof(LHS) > 64) ? 32 : 16;
#elif __SSE__
    constexpr usize Align = (sizeof(LHS) > 16) ? 16 : 8;
#else
    constexpr usize Align = alignof(std::max_align_t);
#endif
    // 32 bytes is the largest simd register can be without downclocking.
    // Will still be fast without AVX support. 
    if constexpr (sizeof(LHS) <= 32) {
      alignas(Align) SimpleArray<T, N> Store = LHS;
      LHS = RHS;
      RHS = Store;
    } else {
      alignas(Align) SimpleArray<T, N> Store;
      std::memcpy(&Store, &LHS, sizeof(LHS));
      std::memcpy(&LHS, &RHS,   sizeof(LHS));
      std::memcpy(&RHS, &Store, sizeof(LHS));
    }
  } else {
    exi_unreachable("swap failed");
  }
}

} // namespace exi
