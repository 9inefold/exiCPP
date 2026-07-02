//===- exi/Encode/CharacterSet.hpp ----------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file defines the class representing restricted character sets.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/bitset.hpp>
#include <concepts>

namespace exi {

/// A restricted character set.
/// TODO: Handle things beyond ascii!
class CharacterFilter {
  using FilterT = StrRef::filter_t;
  FilterT CharFilter;
public:
  constexpr CharacterFilter(StrRef S) : CharacterFilter(S.data(), S.size()) {}
  template <std::integral T>
  constexpr CharacterFilter(ArrayRef<char> A) : CharacterFilter(A.data(), A.size()) {}
  template <std::integral T>
  inline constexpr CharacterFilter(const T* Data, usize Len);

  /// Checks if an individual character is in range.
  bool isInRange(char C) const { return CharFilter.test((u8)C); }
  /// Checks if a string is all in range.
  bool allInRange(StrRef S) const;
};

template <std::integral T>
constexpr CharacterFilter::CharacterFilter(const T* Data, usize Len) : CharFilter() {
  using UType = std::make_unsigned_t<T>;
  for (usize Ix = 0; Ix < Len; ++Ix) {
    const usize C = UType(Data[Ix]);
    if EXI_ALWAYS(usize(C) < CharFilter.size())
      CharFilter.set(C);
  }
}

} // namespace exi
