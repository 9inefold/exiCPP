//===- Common/CompressedPair.hpp ------------------------------------===//
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
/// This file defines a pair which can potentially be smaller than its
/// constituents normally could be.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <utility>

namespace exi {

// TODO: Expand on this...

/// Defines a pair where the key and value may be reordered. This allows for
/// data to potentially be smaller than the combined size of their constituents.
template <typename K, typename V>
struct CompressedPair {
  EXI_NO_UNIQUE_ADDRESS K Key;
  EXI_NO_UNIQUE_ADDRESS V Value;

public:
  template <usize I>
  EXI_INLINE auto&& get() & { return GetImpl<I>(*this); }

  template <usize I>
  EXI_INLINE auto&& get() const& { return GetImpl<I>(*this); }

  template <usize I>
  EXI_INLINE auto&& get() && { return GetImpl<I>(*this); }

private:
  template <usize I, class Self>
  ALWAYS_INLINE static auto&& GetImpl(Self&& self) {
    static_assert(I < 2, "Index out of bounds!");
    if constexpr (I == 0)
      return EXI_FWD(self).Key;
    else if constexpr (I == 1)
      return EXI_FWD(self).Value;
  }
};

} // namespace exi

namespace std {

template <typename K, typename V>
struct tuple_size<exi::CompressedPair<K, V>>
  : integral_constant<usize, 2> {};

template <usize I, typename K, typename V>
struct tuple_element<I, exi::CompressedPair<K, V>>
    : conditional<I == 0, K, V> {
  static_assert(I < 2, "Index out of bounds!");
};

} // namespace std
