//===- Support/IntCast.hpp ------------------------------------------===//
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
/// This file provides checks for casting between integral types.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/Limits.hpp>
#include <type_traits>

namespace exi {
/// Some template parameter helpers to optimize for bitwidth, for functions that
/// take multiple arguments.

namespace H {

template <typename T, typename U>
concept both_int_ = std::is_integral_v<T> && std::is_integral_v<U>;

template <typename T, typename U>
concept both_int = both_int_<T, U> && both_int_<U, T>;

template <typename T, typename U>
concept same_sign_ = (std::is_signed_v<T> == std::is_signed_v<U>);

template <typename T, typename U>
concept same_sign = same_sign_<T, U> && same_sign_<U, T>;

} // namespace H

/// Check if casting one number to another has the same representation.
/// This overload only considers values of the same sign.
template <typename To, typename From>
requires (H::both_int<To, From> && H::same_sign<To, From>)
constexpr bool CheckIntCast(const From X) {
  if constexpr (sizeof(To) >= sizeof(From)) {
    return true;
  } else {
    if constexpr (std::is_signed_v<To>) {
      if (X < 0)
        return (X > From(min_v<To>));
    }
    // Either unsigned or positive.
    return (X < From(max_v<To>));
  }
}

/// Check if casting one number to another has the same representation.
/// This overload considers values of different signs.
template <typename To, typename From>
requires (H::both_int<To, From> && !H::same_sign<To, From>)
constexpr bool CheckIntCast(const From X) {
  if constexpr (std::is_unsigned_v<To>) {
    // Check if `X` is negative, as this will
    // always result in a different representation.
    if (X < 0)
      return false;
  }

  if constexpr (sizeof(To) >= sizeof(From)) {
    // We already confirmed that `X` can't be negative,
    // so no more checks are required.
    return true;
  } else if constexpr (std::is_signed_v<To>) {
    using UTo = std::make_unsigned_t<To>;
    return (X < From(static_cast<UTo>(max_v<To>)));
  } else /* To is unsigned */ {
    using UFrom = std::make_unsigned_t<From>;
    return (X < From(static_cast<UFrom>(max_v<To>)));
  }
}

/// Assert that casting results in the same representation.
template <typename To, typename From>
constexpr void AssertIntCast(From X) {
  exi_assert(CheckIntCast<To>(X), "Int conversion is lossy.");
}

/// Cast that checks if the result is the same representation.
template <typename To, typename From>
EXI_INLINE constexpr To IntCast(From X) {
  AssertIntCast<To>(X);
  return static_cast<To>(X);
}

/// Cast that checks if the result is the same representation.
/// If they would differ, returns 0.
template <typename To, typename From>
constexpr inline To IntCastOrZero(From X) {
  if EXI_UNLIKELY(!CheckIntCast<To>(X))
    // TODO: Add warning in debug if this occurs?
    return static_cast<To>(0);
  return static_cast<To>(X);
}

} // namespace exi
