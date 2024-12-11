//===- Common/String.hpp --------------------------------------------===//
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

#define EXI_CUSTOM_STRREF 1
#include <Common/_Str.hpp>
#include <string_view>
#if EXI_CUSTOM_STRREF
# include <Common/StrRef.hpp>
#endif

namespace exi {

#if !EXI_CUSTOM_STRREF
using StrRef = StrSpan;
#endif

/// From LLVM.
/// A wrapper around a string literal that serves as a proxy for constructing
/// global tables of StringRefs with the length computed at compile time.
/// In order to avoid the invocation of a global constructor, StringLiteral
/// should *only* be used in a constexpr context, as such:
///
/// constexpr StringLiteral S("test");
///
class StringLiteral : public StrRef {
private:
  constexpr StringLiteral(const char *Str, size_t N) : StrRef(Str, N) {}

public:
  template <size_t N>
  constexpr StringLiteral(const char (&Str)[N])
#if defined(__clang__) && __has_attribute(enable_if)
DIAGNOSTIC_PUSH()
CLANG_IGNORED("-Wgcc-compat")
      __attribute((enable_if(__builtin_strlen(Str) == N - 1,
                             "invalid string literal")))
DIAGNOSTIC_POP()
#endif
      : StrRef(Str, N - 1) {
  }

  // Explicit construction for strings like "foo\0bar".
  template <size_t N>
  static constexpr StringLiteral withInnerNUL(const char (&Str)[N]) {
    return StringLiteral(Str, N - 1);
  }
};



} // namespace exi
