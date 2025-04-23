//===- Common/Unwrap.hpp --------------------------------------------===//
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
/// This file defines the $unwrap macro, which can be used to nicely unwrap.
/// It uses ADL to determine if and what should be unwrapped. To unwrap your
/// types, define exi_unwrap_chk and exi_unwrap_fail in the type's namespace.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Common/D/Expect.hpp>
#include <concepts>

#if defined(_MSC_VER) && !defined(__GNUC__)
/// Unwrapping not possible on MSVC.
# define EXI_UNWRAP(VAL, ...) [&]() {                                         \
  static_assert("$unwrap not available on MSVC!");                            \
  auto&& _u_Obj = VAL;                                                        \
  return ::exi::Expect{*EXI_FWD(_u_Obj)};                                     \
}().value()
/// Unwrapping not possible on MSVC.
# define EXI_UNWRAP_RAW(VAL, ...) [&]() {                                     \
  static_assert("$unwrap_raw not available on MSVC!");                        \
  auto&& _u_Obj = VAL;                                                        \
  return EXI_FWD(_u_Obj);                                                     \
}()
#elif EXI_IS_LANG_SERVER && 0
/// Uses GNU Statement Exprs to unwrap values.
# define EXI_UNWRAP(VAL, ...) (::exi::Expect{*VAL}).value()
/// Uses GNU Statement Exprs to unwrap values. Gets raw value on success.
# define EXI_UNWRAP_RAW(VAL, ...) VAL
#else
/// Uses GNU Statement Exprs to unwrap values.
# define EXI_UNWRAP(VAL, ...) ({                                              \
  auto&& _u_Obj = VAL;                                                        \
  if EXI_UNLIKELY(!::exi::H::unwrap_chk(_u_Obj))                              \
    return ::exi::H::unwrap_fail(EXI_FWD(_u_Obj), ##__VA_ARGS__);             \
  ::exi::Expect{*EXI_FWD(_u_Obj)};                                            \
}).value()
/// Uses GNU Statement Exprs to unwrap values. Gets raw value on success.
# define EXI_UNWRAP_RAW(VAL, ...) ({                                          \
  auto&& _u_Obj = VAL;                                                        \
  if EXI_UNLIKELY(!::exi::H::unwrap_chk(_u_Obj))                              \
    return ::exi::H::unwrap_fail(EXI_FWD(_u_Obj), ##__VA_ARGS__);             \
  EXI_FWD(_u_Obj);                                                            \
})
#endif

#define $unwrap(VAL, ...) (EXI_UNWRAP((VAL) __VA_OPT__(,) __VA_ARGS__))
#define $unwrap_raw(VAL, ...) (EXI_UNWRAP_RAW((VAL) __VA_OPT__(,) __VA_ARGS__))

namespace exi::H {

/// Checks if `exi_chk` has been defined for a type.
template <typename T>
concept has__exi_unwrap_chk = requires(const T& Data) {
  { exi_unwrap_chk(Data) } -> std::same_as<bool>;
};

/// Checks if `exi_unwrap_fail` has been defined for a type.
template <typename T, typename...Args>
concept has__exi_unwrap_fail = requires(T Data, Args...Extra) {
  exi_unwrap_fail(EXI_FWD(Data), EXI_FWD(Extra)...);
};

template <typename T>
EXI_REQUIRES_IF(has__exi_unwrap_chk<T>, "Missing exi_unwrap_chk!")
ALWAYS_INLINE constexpr bool unwrap_chk(const T& Data) noexcept {
  using namespace ::exi;
  return exi_unwrap_chk(Data);
}

template <typename T, typename...Args>
EXI_REQUIRES_IF((has__exi_unwrap_fail<T, Args...>), "Missing exi_unwrap_fail!")
ALWAYS_INLINE constexpr decltype(auto)
 unwrap_fail(T&& Data, Args&&...Extra) noexcept {
  using namespace ::exi;
  return exi_unwrap_fail(EXI_FWD(Data), EXI_FWD(Extra)...);
}

// TODO: Add void tag...

} // namespace exi::H
