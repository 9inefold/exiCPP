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

#include <Common/Features.hpp>
#include <Common/D/Scalar128.hpp>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>

EXI_CONST std::size_t kCHAR_BIT = CHAR_BIT;

template <typename T>
EXI_CONST std::size_t sizeof_v = sizeof(T);

template <>
EXI_CONST std::size_t sizeof_v<void> = 0ull;

template <typename T>
EXI_CONST std::size_t bitsizeof_v = (sizeof_v<T> * kCHAR_BIT);

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

#if EXI_HAS_I128
using i128 = __int128;
using u128 = unsigned __int128;
#endif

using ptrdiff = std::ptrdiff_t;

using isize = std::make_signed_t<std::size_t>;
using usize = std::size_t;

namespace exi {
namespace H {

template <usize N> struct SIntN;
template <usize N> struct UIntN;

template <> struct SIntN<1UL>  { using type = i8; };
template <> struct SIntN<2UL>  { using type = i16; };
template <> struct SIntN<4UL>  { using type = i32; };
template <> struct SIntN<8UL>  { using type = i64; };
#if EXI_HAS_I128
template <> struct SIntN<16UL> { using type = i128; };
#endif

template <> struct UIntN<1UL>  { using type = u8; };
template <> struct UIntN<2UL>  { using type = u16; };
template <> struct UIntN<4UL>  { using type = u32; };
template <> struct UIntN<8UL>  { using type = u64; };
#if EXI_HAS_I128
template <> struct UIntN<16UL> { using type = u128; };
#endif

} // namespace H

template <usize N> using intn_t  = typename H::SIntN<N>::type;
template <usize N> using uintn_t = typename H::UIntN<N>::type;
template <typename T> using intty_t  = intn_t<sizeof(T)>;
template <typename T> using uintty_t = uintn_t<sizeof(T)>;

} // namespace exi

using iptr = std::intptr_t;
using uptr = std::uintptr_t;

using ihalfptr = exi::intn_t<sizeof(void*) / 2>;
using uhalfptr = exi::uintn_t<sizeof(void*) / 2>;

#if EXI_HAS_I128
using ilargest = i128;
using ulargest = u128;
#else
using ilargest = i64;
using ulargest = u64;
#endif

//////////////////////////////////////////////////////////////////////////
// Floats

using f32 = float;
using f64 = double;

#if EXI_HAS_F128
using f128 = __float128;
#endif

static_assert(bitsizeof_v<f32> == 32);
static_assert(bitsizeof_v<f64> == 64);

//////////////////////////////////////////////////////////////////////////
// Miscellaneous

namespace exi {

struct Dummy_ {
  struct _secret_tag {
    explicit constexpr _secret_tag() = default;
  };
  EXI_ALWAYS_INLINE EXI_NODEBUG constexpr explicit
   Dummy_(_secret_tag, _secret_tag) noexcept {}
};

/// The default placeholder type.
using dummy_t = Dummy_;
/// The default placeholder value.
EXI_CONST dummy_t dummy_v = Dummy_{
  Dummy_::_secret_tag{}, Dummy_::_secret_tag{}};

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

EXI_CONST bool kHasInlineFunctionPtrs
  = (sizeof(function_t<void>*) == sizeof(void*));

} // namespace exi
