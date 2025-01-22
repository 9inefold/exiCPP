//===- Fundamental.hpp ----------------------------------------------===//
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

#include <Features.hpp>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>

template <typename T>
inline constexpr std::size_t sizeof_v = sizeof(T);

template <>
inline constexpr std::size_t sizeof_v<void> = 0ull;

template <typename T>
inline constexpr std::size_t bitsizeof_v
  = (sizeof_v<T> * std::size_t(CHAR_BIT));

//////////////////////////////////////////////////////////////////////////
// Integrals

using i8  = signed char;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = unsigned char;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using iptr = std::intptr_t;
using uptr = std::uintptr_t;

using byte = u8;
using ptrdiff = std::ptrdiff_t;

using isize = std::make_signed_t<std::size_t>;
using usize = std::size_t;

//////////////////////////////////////////////////////////////////////////
// Generic

namespace re {

template <typename T>
constexpr inline T max(T LHS, T RHS) noexcept {
  return (LHS > RHS) ? LHS : RHS; 
}

template <typename T>
constexpr inline T min(T LHS, T RHS) noexcept {
  return (LHS < RHS) ? LHS : RHS; 
}

} // namespace re
