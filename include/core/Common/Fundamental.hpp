//===- Common/Fundamental.hpp ---------------------------------------===//
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

#include "Features.hpp"
#include <climits>
#include <cstdint>
#include <type_traits>

template <typename T>
EXI_CONST std::size_t sizeof_v = sizeof(T);

template <>
EXI_CONST std::size_t sizeof_v<void> = 0ull;

template <typename T>
EXI_CONST std::size_t bitsizeof_v = (sizeof_v<T> * CHAR_BIT);

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

using isize = std::make_signed_t<std::size_t>;
using usize = std::size_t;

//////////////////////////////////////////////////////////////////////////
// Floats

using f32 = float;
using f64 = double;

static_assert(bitsizeof_v<f32> == 32);
static_assert(bitsizeof_v<f64> == 64);

//////////////////////////////////////////////////////////////////////////
// Miscellaneous

namespace exi {

struct Dummy_ {};
/// The default placeholder type.
using dummy_t = Dummy_;

struct Void_ {
  ALWAYS_INLINE constexpr
   Void_* operator&() {
    return nullptr;
  }
  ALWAYS_INLINE constexpr
   const Void_* operator&() const {
    return nullptr;
  }
};

template <typename Ret, typename...Args>
using function_t = Ret(Args...);

template <typename Ret, class Obj, typename...Args>
using method_t = Ret(Obj::*)(Args...);

} // namespace exi
