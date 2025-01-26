//===- Features.hpp -------------------------------------------------===//
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

// At `{TOP_LEVEL}/include/core/Config`.
#include <Config.inc>
#include <cstddef>

#define RE_DEBUG_EXTRA_ 1

#if EXI_DEBUG && RE_DEBUG_EXTRA_
# define RE_DEBUG_EXTRA 1
#else
# define RE_DEBUG_EXTRA 0
#endif

#if !EXI_ON_WIN32
# error This library should only be used on Windows!
#elif !defined(_WIN64)
# error This library does not target 32-bit Windows!
#endif

#ifdef __has_builtin
# define HAS_BUILTIN(x) __has_builtin(x)
#else
# define HAS_BUILTIN(x) 0
#endif

#ifdef __has_attribute
# define HAS_ATTR(x) __has_attribute(x)
#else
# define HAS_ATTR(x) 0
#endif

#if HAS_ATTR(always_inline)
# define RE_ALWAYS_INLINE __attribute__((always_inline))
#else
# define RE_ALWAYS_INLINE inline
#endif

#if defined(_MSC_VER)
# define ALWAYS_INLINE __forceinline
#else
# define ALWAYS_INLINE inline RE_ALWAYS_INLINE
#endif

#if HAS_ATTR(cold)
# define RE_COLD __attribute__((cold))
#else
# define RE_COLD
#endif

#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)

#if HAS_BUILTIN(__builtin_expect)
# define RE_EXPECT(v, expr) \
 (__builtin_expect(static_cast<bool>(expr), v))
#else
# define RE_EXPECT(v, expr) (static_cast<bool>(expr))
#endif

#define LIKELY(...)   RE_EXPECT(1, (__VA_ARGS__))
#define UNLIKELY(...) RE_EXPECT(0, (__VA_ARGS__))

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#if HAS_BUILTIN(__builtin_trap) || defined(__GNUC__)
# define RE_TRAP __builtin_trap()
#elif defined(_MSC_VER)
# define RE_TRAP __debugbreak()
#else
# define RE_TRAP *(volatile int*)0x11 = 0
#endif

#if EXI_DEBUG
# define re_assert(...) [] RE_ALWAYS_INLINE               \
 (auto&& Expr, const char* Msg = "") {                    \
  LIKELY(static_cast<bool>(FWD(Expr))) ? (void(0))        \
    : (void(::re::re_assert_failed(Msg, #__VA_ARGS__)));  \
} (__VA_ARGS__)
#else
# define re_assert(...) (void(0))
#endif

#if __has_cpp_attribute(clang::musttail)
# define HAS_MUSTTAIL 1
# define tail_return [[clang::musttail]] return
#else
# define HAS_MUSTTAIL 0
# define tail_return return
#endif

namespace re {
#if EXI_DEBUG
/// Defined in `Globals.cpp`.
RE_COLD void re_assert_failed(
  const char* Msg, const char* Expr);
#endif // EXI_DEBUG
} // namespace re
