//===- exicpp/Common/EnumBitwise.hpp --------------------------------===//
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

#pragma once

#ifndef EXICPP_COMMON_MARKBITWISE_HPP
#define EXICPP_COMMON_MARKBITWISE_HPP

#include <type_traits>

/// Defines common bitwise operations for types.
/// @param T The base type to provide overloads.
/// @param U The cast-to type.
#define EXICPP_MARK_BITWISE_EX(T, U)       \
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
{ return L = T(L | R); }                   \
inline constexpr T& operator&=(T& L, T R)  \
{ return L = T(L & R); }                   \
inline constexpr T& operator^=(T& L, T R)  \
{ return L = T(L ^ R); }

/// Defines common bitwise operations for enum types.
/// @param T The base type to provide overloads.
#define EXICPP_MARK_BITWISE(T)                    \
 static_assert(std::is_enum_v<T>,                 \
  "MARK_BITWISE can only be used with enums.");   \
 EXICPP_MARK_BITWISE_EX(T, std::underlying_type_t<T>)

#endif // EXICPP_COMMON_MARKBITWISE_HPP
