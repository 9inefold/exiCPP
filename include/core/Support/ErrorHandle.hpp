//===- Support/ErrorHandle.hpp --------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
#include <cassert>

// TODO: Macro "compiler" would make this all a lot nicer...

#if !defined(NDEBUG) || EXI_INVARIANTS
# define EXI_ASSERTS 1
#else
# undef EXI_ASSERTS
#endif

namespace exi {
namespace H {

enum AssertionKind : int {
  ASK_Assert,
  ASK_Assume,
  ASK_Invariant,
  ASK_Unreachable,
};

} // namespace H

class StrRef;
class Twine;

/// @brief Reports a fatal error.
[[noreturn]] void report_fatal_error(
  const char* Msg, bool GenCrashDiag = true);
/// @brief Reports a fatal error.
[[noreturn]] void report_fatal_error(
  StrRef Msg, bool GenCrashDiag = true);
/// @brief Reports a fatal error.
[[noreturn]] void report_fatal_error(
  const Twine& Msg, bool GenCrashDiag = true);

/// @brief Reports a fatal allocation error.
/// If exceptions are enabled, throws `std::bad_alloc`, otherwise aborts.
[[noreturn]] void fatal_alloc_error(const char* Msg) EXI_NOEXCEPT;

[[noreturn]] void exi_assert_impl(
 H::AssertionKind Kind, const char* Msg = nullptr,
 const char* File = nullptr, unsigned Line = 0
);

[[noreturn]] inline void exi_unreachable_impl() {
#ifdef EXI_UNREACHABLE
  EXI_UNREACHABLE;
#endif
  EXI_DBGTRAP;
}

} // namespace exi

#define exi_try(...) do {                                                     \
  auto&& _u_Err = (__VA_ARGS__);                                              \
  if EXI_UNLIKELY(_u_Err) {                                                   \
    return _u_Err;                                                            \
  }                                                                           \
} while(0)

/// Simplified assertion handler, provides required arguments for you.
#define exi_fail(KIND, MSG) ::exi::exi_assert_impl(                           \
  ::exi::H::KIND, MSG, EXI_FUNCTION, __LINE__)

/// Simplified assertion handler, provides required arguments for you.
#define exi_fail_stringify(KIND, ...) exi_fail(KIND, "`" #__VA_ARGS__ "`")

#ifndef NDEBUG
# define exi_unreachable(MSG) exi_fail(ASK_Unreachable, MSG)
#elif !defined(EXI_UNREACHABLE)
# define exi_unreachable(MSG) ::exi::exi_unreachable_impl()
#elif EXI_OPTIMIZE_UNREACHABLE && !EXI_DEBUG
# define exi_unreachable(MSG) EXI_UNREACHABLE
#else
# define exi_unreachable(MSG) do {                                            \
    EXI_TRAP;                                                                 \
    EXI_UNREACHABLE;                                                          \
  } while(0)
#endif

#if EXI_ASSERTS
# define exi_assume(...) do {                                                 \
    if EXI_UNLIKELY(!static_cast<bool>(__VA_ARGS__))                          \
      exi_fail_stringify(ASK_Assume, __VA_ARGS__);                            \
  } while(0)
#elif defined(EXI_ASSUME)
# define exi_assume(...) EXI_ASSUME(__VA_ARGS__)
#else
# define exi_assume(...) (void(0))
#endif

/// Provides runtime assertion checking for a generic kind.
#define exi_assertRT_(KIND, EXPR, ...) void(EXI_LIKELY((EXPR))                \
  ? (void(0))                                                                 \
  : (exi_fail(KIND, ("`" #EXPR "`" __VA_OPT__(". Reason: ") #__VA_ARGS__))))

#if EXI_HAS_BUILTIN(__builtin_is_constant_evaluated)
# define EXI_HAS_CTASSERT 1
/// This version will have better error messages, as it uses unreachable instead
/// of invoking a non-constexpr function.
# define exi_assertCT_(EXPR, ...) do {                                        \
    if (__builtin_is_constant_evaluated()) {                                  \
      if (!static_cast<bool>(EXPR))                                           \
        asm("constexpr assertion failed.");                                   \
    }                                                                         \
  } while(0)
#else
# define exi_assertCT_(EXPR) (void(0))
#endif

#define exi_assert_(KIND, EXPR, ...) do {                                     \
  exi_assertCT_(EXPR __VA_OPT__(,) __VA_ARGS__);                              \
  exi_assertRT_(KIND, EXPR __VA_OPT__(,) __VA_ARGS__);                        \
} while(0)

#if EXI_ASSERTS
/// Takes `(condition, "message")`, asserts in debug mode.
# define exi_assert(EXPR, ...) \
 exi_assert_(ASK_Assert, EXPR __VA_OPT__(,) __VA_ARGS__)
#else
# define exi_assert(EXPR, ...) exi_assertCT_(EXPR)
#endif

#if EXI_ASSERTS
# define exi_assert_eq(LHS, RHS, ...) do {                                    \
  if EXI_UNLIKELY((LHS) != (RHS)) {                                           \
    auto Str = fmt::format("`{}`. Reason: '{}' != '{}'" __VA_OPT__(" ({})"),  \
      XSTRINGIFY(LHS == RHS), (LHS), (RHS) __VA_OPT__(,) __VA_ARGS__);        \
    exi_fail(ASK_Assert, Str.c_str());                                        \
  }                                                                           \
} while(0)
# define exi_assert_neq(LHS, RHS, ...) do {                                   \
  if EXI_UNLIKELY((LHS) == (RHS)) {                                           \
    auto Str = fmt::format("`{}`. Reason: '{}' == '{}'" __VA_OPT__(" ({})"),  \
      XSTRINGIFY(LHS != RHS), (LHS), (RHS) __VA_OPT__(,) __VA_ARGS__);        \
    exi_fail(ASK_Assert, Str.c_str());                                        \
  }                                                                           \
} while(0)
#else
# define exi_assert_eq(LHS, RHS, ...)  exi_assertCT_((LHS) == (RHS))
# define exi_assert_neq(LHS, RHS, ...) exi_assertCT_((LHS) != (RHS))
#endif

#if EXI_INVARIANTS
/// Takes `(condition, "message")`, checks when invariants on.
# define exi_invariant(EXPR, ...)                                             \
 exi_assert_(ASK_Invariant, EXPR __VA_OPT__(,) __VA_ARGS__)
#else
/// Noop in this mode.
# define exi_invariant(EXPR, ...) exi_assertCT_(EXPR __VA_OPT__(,) __VA_ARGS__)
#endif

#ifdef EXPENSIVE_CHECKS
/// Takes `(condition, "message")`, checks when `EXPENSIVE_CHECKS` are on.
# define exi_expensive_invariant(EXPR, ...)                                   \
 exi_assert_(ASK_Invariant, EXPR __VA_OPT__(,) __VA_ARGS__)
#else
/// Noop in this mode.
# define exi_expensive_invariant(EXPR, ...) (void(0))
#endif
