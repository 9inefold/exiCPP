//===- Support/ErrorHandle.hpp --------------------------------------===//
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
#include <Common/_Char.hpp>

namespace exi {
namespace H {

enum AssertionKind : unsigned {
  ASK_Assert,
  ASK_Invariant,
  ASK_Unreachable,
};

} // namespace H

class Twine;

/// @brief Reports a fatal error.
[[noreturn]] void report_fatal_error(
  StrSpan msg, bool genCrashDiag = true);

/// @brief Reports a fatal allocation error.
/// If exceptions are enabled, throws `std::bad_alloc`, otherwise aborts.
[[noreturn]] void fatal_alloc_error(const char* msg);

[[noreturn]] void exi_assert_impl(
 H::AssertionKind kind, const char* msg = nullptr,
 const char* file = nullptr, unsigned line = 0
);

} // namespace exi

#ifndef NDEBUG
# define exi_unreachable(msg) \
  ::exi::exi_assert_impl(::exi::H::ASK_Unreachable, msg, __FILE__, __LINE__)
#elif !defined(EXI_UNREACHABLE)
# define exi_unreachable(msg) \
  ::exi::exi_unreachable_impl(::exi::H::ASK_Unreachable)
#elif !EXI_DEBUG
# define exi_unreachable(msg) EXI_UNREACHABLE
#else
# define exi_unreachable(msg) do {  \
    EXI_TRAP;                       \
    EXI_UNREACHABLE;                \
  } while(0)
#endif

#define exi_assert_(k, expr, ...) void(EXI_LIKELY((expr))                     \
  ? (void(0))                                                                 \
  : (::exi::exi_assert_impl(::exi::H::k,                                      \
      ("`" #expr "`. " __VA_OPT__("Reason: ") #__VA_ARGS__),  \
      __FILE__, __LINE__)))

#if !defined(NDEBUG) || EXI_INVARIANTS
/// Takes `(condition, "message")`, asserts in debug mode.
# define exi_assert(expr, ...) \
 exi_assert_(ASK_Assert, expr __VA_OPT__(,) __VA_ARGS__)
#else
# define exi_assert(expr, ...) (void(0))
#endif

#if EXI_INVARIANTS
/// Takes `(condition, "message")`, checks when invariants on.
# define exi_invariant(expr, ...) \
 exi_assert_(ASK_Invariant, expr __VA_OPT__(,) __VA_ARGS__)
#else
/// Noop in this mode.
# define exi_invariant(expr, ...) (void(0))
#endif
