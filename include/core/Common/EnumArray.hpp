
//===- Common/EnumeratedArray.hpp -----------------------------------------===//
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
/// This file defines traits/utilities for dealing with enums.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/EnumTraits.hpp>
#include <cassert>
#include <iterator>

namespace exi {

/// Default type for EnumeratedArray.
using EAIdxDefaultType = i64;

template <typename ValueT, exi::is_enum Enum,
          Enum Last = Enum::Last,
          Enum First = Enum(),
          typename IdxT = EAIdxDefaultType,
          IdxT Size = 1 + static_cast<IdxT>(Last)>
class EnumeratedArray {
  using SelfT = EnumeratedArray;
  ValueT Underlying[Size];
public:
  using iterator = ValueT*;
  using const_iterator = const ValueT*;

  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;

  using value_type = ValueT;
  using reference = ValueT&;
  using const_reference = const ValueT&;
  using pointer = ValueT*;
  using const_pointer = const ValueT*;

  static ALWAYS_INLINE constexpr IdxT ToIndex(Enum Index) noexcept {
    return static_cast<IdxT>(Index)
         - static_cast<IdxT>(First);
  }

  constexpr EnumeratedArray() = default;
  constexpr EnumeratedArray(const ValueT& V) {
    for (IdxT Ix = 0; Ix < Size; ++Ix) {
      Underlying[Ix] = V;
    }
  }
  constexpr EnumeratedArray(std::initializer_list<ValueT> Init) {
    assert(Init.size() == Size && "Incorrect initializer size");
    for (IdxT Ix = 0; Ix < Size; ++Ix) {
      Underlying[Ix] = *(Init.begin() + Ix);
    }
  }

  constexpr const ValueT& operator[](Enum Index) const {
    const IdxT Ix = SelfT::ToIndex(Index);
    assert(Ix >= 0 && Ix < Size && "Index is out of bounds.");
    return Underlying[Ix];
  }
  constexpr ValueT& operator[](Enum Index) {
    return const_cast<ValueT&>(
        static_cast<const SelfT&>(*this)[Index]);
  }
  static constexpr IdxT size() { return Size; }
  constexpr bool empty() const { return SelfT::size() == 0; }

  constexpr iterator begin() { return Underlying; }
  constexpr iterator end() { return begin() + SelfT::size(); }

  constexpr const_iterator begin() const { return Underlying; }
  constexpr const_iterator end() const { return begin() + SelfT::size(); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }
};

/// Proxy type for `EnumeratedArray`, uses `EnumRange` for inputs by default.
template <typename ValueT, exi::is_enum Enum,
          class RangeT = EnumRange<Enum>,
          typename IdxT = EAIdxDefaultType>
using EnumArray = EnumeratedArray<ValueT, Enum,
          RangeT::Last, RangeT::First,
          IdxT, static_cast<IdxT>(RangeT::size)>;

} // namespace exi
