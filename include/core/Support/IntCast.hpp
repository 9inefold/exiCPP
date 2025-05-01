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
#include <Common/QualTraits.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/Limits.hpp>
#include <concepts>
#include <type_traits>

namespace exi {
/// Some template parameter helpers to optimize for bitwidth, for functions that
/// take multiple arguments.
namespace H {

template <typename T, typename U>
concept both_int_ = std::integral<T> && std::integral<U>;

template <typename T, typename U>
concept both_int = both_int_<T, U> && both_int_<U, T>;

template <typename T, typename U>
concept same_sign_ = (std::is_signed_v<T> == std::is_signed_v<U>);

template <typename T, typename U>
concept same_sign = same_sign_<T, U> && same_sign_<U, T>;

template <class From, class To>
concept simple_convertible_to = requires {
  static_cast<To>(std::declval<From>());
};

} // namespace H

//===----------------------------------------------------------------===//
// IntCastFrom
//===----------------------------------------------------------------===//

/// Used to specialize `IntCastFrom`.
template <class FromT> struct IntCastByValue {
  static constexpr bool value
    = std::is_trivially_copyable_v<FromT>
    && (sizeof(FromT) <= 2 * sizeof(void*));
};

/// By default, passes by value when an object is smaller than two `void*`s,
/// and is trivially copyable. Specialize `IntCastByValue` to change this.
template <class FromT>
using IntCastFrom = std::conditional_t<
  IntCastByValue<FromT>::value,
    const FromT, const FromT&>;

//===----------------------------------------------------------------===//
// IntCastIsPossible
//===----------------------------------------------------------------===//

/// The core of the implementation of `CheckIntCast<X>` is here. This template
/// can be specialized to customize the implementation of CheckIntCast<> without
/// rewriting it from scratch.
template <class To, class From>
struct IntCastIsPossible {
  static constexpr bool isPossible(
   IntCastFrom<From>) noexcept {
    return false;
  }
};

template <class To, class From>
requires (H::both_int<To, From> && H::same_sign<To, From>)
struct IntCastIsPossible<To, From> {
  /// Check if casting one number to another has the same representation.
  /// This overload only considers values of the same sign.
  static constexpr bool isPossible(const From X) noexcept {
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
};

template <class To, class From>
requires (H::both_int<To, From> && !H::same_sign<To, From>)
struct IntCastIsPossible<To, From> {
  /// Check if casting one number to another has the same representation.
  /// This overload considers values of different signs.
  static constexpr bool isPossible(const From X) noexcept {
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
};

//===----------------------------------------------------------------===//
// IntCastCast
//===----------------------------------------------------------------===//

template <class To, class From> struct IntCastCast;

template <class To, class From,
  class Derived = IntCastCast<To, From>>
struct IntCastDefaultFailure {
  using FromT = IntCastFrom<From>;
public:
  static inline To castFailed() { return To(0); }

  static inline To doCastIfPossible(FromT X) {
    if EXI_UNLIKELY(!Derived::isPossible(X))
      // TODO: Add warning in debug if this occurs?
      return castFailed();
    return Derived::doCast(X);
  }
};

template <class To, class From>
struct IntCastCast
  : public IntCastIsPossible<To, From>,
    public IntCastDefaultFailure<To, From> {
  using FromT = IntCastFrom<From>;
  static_assert(H::simple_convertible_to<FromT, To>,
    "Invalid IntCast, \"static_cast<To>(from)\" must be well-formed!");
public:
  static inline To doCast(FromT X) { return static_cast<To>(X); }
};

/// The real trait used by the cast functions. Nice and simple...
template <class To, class From> struct IntCastInfo {
  using SimpleFrom = std::remove_cvref_t<From>;
  using SimplifiedSelf = IntCastCast<To, SimpleFrom>;
  using FromT = IntCastFrom<SimpleFrom>;

  static inline bool isPossible(FromT X) {
    return SimplifiedSelf::isPossible(X);
  }

  static inline decltype(auto) doCast(FromT X) {
    return SimplifiedSelf::doCast(X);
  }

  static inline decltype(auto) castFailed() {
    return SimplifiedSelf::castFailed();
  }

  static inline decltype(auto) doCastIfPossible(FromT X) {
    return SimplifiedSelf::doCastIfPossible(X);
  }
};

//===----------------------------------------------------------------===//
// *IntCast
//===----------------------------------------------------------------===//

template <class To, class From>
constexpr bool CheckIntCast(From X) {
  return IntCastInfo<To, From>::isPossible(X);
}

/// Assert that casting results in the same representation.
template <class To, class From>
#if !EXI_ASSERTS
ALWAYS_INLINE
#endif
constexpr void AssertIntCast(From X) {
#if EXI_ASSERTS
  const bool Lossy = !IntCastInfo<To, From>::isPossible(X);
  exi_assert(not Lossy, "Int conversion is lossy.");
#endif
}

/// Cast that checks if the result is the same representation.
template <class To, class From>
EXI_INLINE constexpr To IntCast(const From X) {
#if EXI_ASSERTS
  AssertIntCast<To, IntCastFrom<From>>(X);
#endif
  return IntCastInfo<To, From>::doCast(X);
}

/// Cast that checks if the result is the same representation.
/// If they would differ, returns 0.
template <class To, class From>
constexpr inline To IntCastOrZero(const From X) {
  return IntCastInfo<To, From>::doCastIfPossible(X);
}

/// Cast that checks if the result is the same representation.
/// If they would differ, returns 0.
template <class To, class From>
constexpr inline To IntCastOr(const From X, const To Else) {
  using TraitsT = IntCastInfo<To, From>;
  if EXI_LIKELY(TraitsT::isPossible(X))
    return TraitsT::doCast(X);
  return std::move(Else);
}

//===----------------------------------------------------------------===//
// promotion_cast
//===----------------------------------------------------------------===//

/// Converts between integer types while keeping the bit representation
/// the same.
template <class To, class From>
requires H::both_int<To, From>
constexpr inline To promotion_cast(const From X) noexcept {
  if constexpr (H::same_sign<To, From>) {
    if constexpr (sizeof(To) >= sizeof(From))
      // Simple case, eg. promotion_cast<u64>(u32);
      return static_cast<To>(X);
    else
      // More complex case, types are different sizes.
      // Assert the representation will stay the same.
      // FIXME: Revert tailcall once cleanup skipping is allowed.
      return IntCast<To>(X);
  } else {
    using U = swap_sign_t<From>;
    if constexpr (sizeof(To) >= sizeof(From))
      // Simple case, eg. promotion_cast<u64>(u32);
      return static_cast<To>(U(X));
    else
      // More complex case, types are different sizes.
      // Assert the representation will stay the same.
      return IntCast<To>(U(X));
  }
}

} // namespace exi
