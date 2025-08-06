
//===- Common/EnumeratedArray.hpp -----------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
/// This file defines an array indexed by an enum.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/EnumTraits.hpp>
#include <Support/D/MemOps.hpp>
#include <Support/ErrorHandle.hpp>
#include <cassert>
#include <iterator>
#include <memory>

namespace exi {

/// Default type for EnumeratedArray.
/// Uses a signed type to allow ranges with negative values.
using EnumIdxDefaultType = isize;

template <typename IdxT = EnumIdxDefaultType, typename E>
constexpr IdxT enum_distance(E First, E Last) {
  return static_cast<IdxT>(Last) - static_cast<IdxT>(First);
}

/// An array which is indexed by an enum value. The range and index type can be
/// customized to allow for easier subrange access.
///
/// @tparam ValueT The type of the array.
/// @tparam Enum The enum type to index with.
/// @tparam Last The end, inclusive. Defaults to `Enum::Last`.
/// @tparam First The start, inclusive. Defaults to `Enum(0)`.
template <typename ValueT, exi::is_enum Enum,
          Enum Last = Enum::Last,
          Enum First = Enum(),
          typename IdxT = EnumIdxDefaultType,
          IdxT Size = 1 + enum_distance<IdxT>(First, Last)>
struct EnumeratedArray {
  static_assert(Size > 0, "Enum arrays cannot be zero-sized!");

  using iterator = ValueT*;
  using const_iterator = const ValueT*;

  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;

  using value_type = ValueT;
  using reference = ValueT&;
  using const_reference = const ValueT&;
  using pointer = ValueT*;
  using const_pointer = const ValueT*;

  using SelfT = EnumeratedArray;
  ValueT Underlying[Size];

public:
  /// Initializes with a size exactly matching the underlying array.
  static constexpr EnumeratedArray New(ArrayRef<ValueT> Init) {
    exi_assert(Init.size() == Size, "Incorrect initializer size");
    SelfT Arr;
    FastDefaultCopy(Arr.Underlying, Init.begin(), Size);
    return Arr;
  }
  /// Initializes with a size that may not match the underlying array.
  template <bool TrailingUninitialized = false>
  static constexpr EnumeratedArray Partial(ArrayRef<ValueT> Init) {
    if (Init.size() >= Size)
      tail_return SelfT::New(Init.take_front(Size)); 
    
    SelfT Arr;
    // Copy front elements from the input.
    FastDefaultCopy(Arr.Underlying, Init.begin(), Init.size());
    // Initialize the trailing elements, if necessary.
    if constexpr (!TrailingUninitialized) {
      // Don't leave remaining elements uninitialized.
      if constexpr (std::is_trivially_constructible_v<ValueT>) {
        // POD types, ensure they have a value.
        const IdxT Start = IdxT(Init.size());
        for (IdxT Ix = Start; Ix < Size; ++Ix)
          Arr.Underlying[Ix] = ValueT{};
      }
    }
    return Arr;
  }
  static constexpr EnumeratedArray Fill(const ValueT& V) {
    SelfT Arr;
    for (IdxT Ix = 0; Ix < Size; ++Ix)
      Arr.Underlying[Ix] = V;
    return Arr;
  }

  static ALWAYS_INLINE constexpr IdxT ToIndex(Enum Index) {
    return static_cast<IdxT>(Index)
         - static_cast<IdxT>(First);
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
          typename IdxT = EnumIdxDefaultType>
using EnumArray = EnumeratedArray<ValueT, Enum,
          RangeT::Last, RangeT::First,
          IdxT, static_cast<IdxT>(RangeT::size)>;

} // namespace exi
