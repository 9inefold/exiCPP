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
requires is_integral<T>
struct TUnderlyingType<T> {
  using type = T;
};

template <typename T>
requires is_enum<T>
struct TUnderlyingType<T> {
  using type = std::underlying_type_t<T>;
};

} // namespace H

template <typename T>
using UnderlyingType = typename H::TUnderlyingType<T>::type;

/// Returns underlying integer value of an enum. Backport of std::to_underlying.
template <typename E>
[[nodiscard]] constexpr UnderlyingType<E> to_underlying(E V) {
  return static_cast<UnderlyingType<E>>(V);
}

} // namespace exi
