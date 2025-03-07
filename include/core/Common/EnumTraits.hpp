//===- Common/EnumTraits.hpp ----------------------------------------===//
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

#include <Common/Fundamental.hpp>
#include <concepts>
#include <type_traits>

/// Defines common bitwise operations for types.
/// @param T The base type to provide overloads.
/// @param U The cast-to type.
#define EXI_MARK_BITWISE_EX(T, U)            \
  inline constexpr bool operator!(T L)       \
  { return !(U)(L); }                        \
  inline constexpr T operator~(T L)          \
  { return T(~(U)(L)); }                     \
  inline constexpr T operator|(T L, T R)     \
  { return T((U)(L) | (U)(R)); }             \
  inline constexpr T operator&(T L, T R)     \
  { return T((U)(L) & (U)(R)); }             \
  inline constexpr T operator^(T L, T R)     \
  { return T((U)(L) ^ (U)(R)); }             \
  inline constexpr T& operator|=(T& L, T R)  \
  { return L = T((U)(L) | (U)(R)); }         \
  inline constexpr T& operator&=(T& L, T R)  \
  { return L = T((U)(L) & (U)(R)); }         \
  inline constexpr T& operator^=(T& L, T R)  \
  { return L = T((U)(L) ^ (U)(R)); }

/// Defines common bitwise operations for enum types.
/// @param T The base type to provide overloads.
#define EXI_MARK_BITWISE(T)                       \
 static_assert(std::is_enum_v<T>,                 \
  "MARK_BITWISE can only be used with enums.");   \
 EXI_MARK_BITWISE_EX(T, std::underlying_type_t<T>)

namespace exi {

template <typename T>
concept is_integral = std::integral<T>;

template <typename T>
concept is_enum = std::is_enum_v<T>;

template <typename T>
concept is_integral_ex = is_integral<T> || is_enum<T>;

using std::integral;

template <typename T>
concept integral_ex = is_integral_ex<T>;

//////////////////////////////////////////////////////////////////////////
// UnderlyingType

namespace H {

template <typename T>
struct TUnderlyingType {
  COMPILE_FAILURE(T, "Invalid type.");
};

template <typename T>
requires exi::is_integral<T>
struct TUnderlyingType<T> {
  using type = T;
};

template <typename T>
requires exi::is_enum<T>
struct TUnderlyingType<T> {
  using type = std::underlying_type_t<T>;
};

} // namespace H

template <typename T>
using UnderlyingType = typename H::TUnderlyingType<T>::type;

/// Returns underlying integer value of an enum. Backport of std::to_underlying.
template <typename E>
[[nodiscard]] constexpr UnderlyingType<E> to_underlying(E Val) {
  return static_cast<UnderlyingType<E>>(Val);
}

//////////////////////////////////////////////////////////////////////////
// EnumBounds

/// Creates `enum_first` for your type.
#define EXI_MARK_ENUM_FIRST(TYPE, FIRST)                                      \
  inline constexpr TYPE enum_first(TYPE) noexcept { return TYPE::FIRST; }
/// Creates `enum_last` for your type.
#define EXI_MARK_ENUM_LAST(TYPE, LAST)                                        \
  inline constexpr TYPE enum_last(TYPE) noexcept { return TYPE::LAST; }

/// Creates `enum_first` and `enum_last` for your type.
#define EXI_MARK_ENUM_BOUNDS(TYPE, FIRST, LAST)                               \
  EXI_MARK_ENUM_FIRST(TYPE, FIRST)                                            \
  EXI_MARK_ENUM_LAST(TYPE,  LAST)

namespace H {
template <typename E>
concept has_enum_first = exi::is_enum<E>
  && requires (E Val) { enum_first(Val); };

template <typename E>
concept has_enum_Last = exi::is_enum<E> && requires { E::Last; };
} // namespace H

/// Provides an interface for customizing the first enum value.
/// If `EnumT()` is the first element, it is defaulted. Otherwise, the ADL
/// function `enum_first` should be provided.
template <exi::is_enum E>
consteval E get_enum_first() noexcept {
  if constexpr (!H::has_enum_first<E>)
    return E();
  else
    return enum_first(E());
}

/// Provides an interface for customizing the last enum value.
/// If `EnumT::Last` exists, that will be used. Otherwise, the ADL function
/// `enum_last` must be provided.
template <exi::is_enum E>
consteval E get_enum_last() noexcept {
  if constexpr (H::has_enum_Last<E>)
    return E::Last;
  else
    return enum_last(E());
}

/// Provides a range from [First, Last) for an enum.
/// See `get_enum_first` and `get_enum_last` for more details.
template <exi::is_enum E> struct EnumRange {
  /// Underlying type of the enum.
  using type = UnderlyingType<E>;
  /// Adaptor for `get_enum_first`, see for more details.
  static constexpr E First = get_enum_first<E>();
  /// Adaptor for `get_enum_last`, see for more details.
  static constexpr E Last = get_enum_last<E>();
  /// Difference between `First` and `Last`.
  static constexpr type size = 1 + (type(Last) - type(First));
};

/// Adaptor for `get_enum_first`, see for more details.
template <exi::is_enum E>
inline constexpr E kEnumFirst = EnumRange<E>::First;

/// Adaptor for `get_enum_last`, see for more details.
template <exi::is_enum E>
inline constexpr E kEnumLast = EnumRange<E>::Last;

/// Adaptor for `EnumRange::size`, see for more details.
template <exi::is_enum E>
inline constexpr UnderlyingType<E> kEnumSize = EnumRange<E>::size;

} // namespace exi
