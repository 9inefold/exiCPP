//===- Common/StringSwitch.hpp --------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
/// This file implements the StringSwitch template, which mimics a switch()
/// statement whose cases are string literals.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/StrRef.hpp>
#include <Common/Option.hpp>
#include <Support/ErrorHandle.hpp>
#include <cstring>

namespace exi {

/// A switch()-like statement whose cases are string literals.
///
/// The StringSwitch class is a simple form of a switch() statement that
/// determines whether the given string matches one of the given string
/// literals. The template type parameter \p T is the type of the value that
/// will be returned from the string-switch expression. For example,
/// the following code switches on the name of a color in \c argv[i]:
///
/// \code{.cpp}
/// Color color = StringSwitch<Color>(argv[i])
///   .Case("red", Red)
///   .Case("orange", Orange)
///   .Case("yellow", Yellow)
///   .Case("green", Green)
///   .Case("blue", Blue)
///   .Case("indigo", Indigo)
///   .Cases("violet", "purple", Violet)
///   .Default(UnknownColor);
/// \endcode
template <typename T, typename Ret = T>
class StringSwitch {
  /// The string we are matching.
  const StrRef St;

  /// The pointer to the result of this switch statement, once known,
  /// null before that.
  Option<T> Result;

public:
  explicit StringSwitch(StrRef S) : St(S), Result() { }

  // StringSwitch is not copyable.
  StringSwitch(const StringSwitch&) = delete;

  // StringSwitch is not assignable due to 'St' being 'const'.
  void operator=(const StringSwitch&) = delete;
  void operator=(StringSwitch &&other) = delete;

  StringSwitch(StringSwitch &&other) : 
   St(other.St), Result(std::move(other.Result)) { }

  ~StringSwitch() = default;

  // Case-sensitive case matchers
  StringSwitch &Case(StringLiteral S, T Value) {
    if (!Result && St == S) {
      Result = std::move(Value);
    }
    return *this;
  }

  StringSwitch& EndsWith(StringLiteral S, T Value) {
    if (!Result && St.ends_with(S)) {
      Result = std::move(Value);
    }
    return *this;
  }

  StringSwitch& StartsWith(StringLiteral S, T Value) {
    if (!Result && St.starts_with(S)) {
      Result = std::move(Value);
    }
    return *this;
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, T Value) {
    return Case(S0, Value).Case(S1, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      T Value) {
    return Case(S0, Value).Cases(S1, S2, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      StringLiteral S3, T Value) {
    return Case(S0, Value).Cases(S1, S2, S3, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      StringLiteral S3, StringLiteral S4, T Value) {
    return Case(S0, Value).Cases(S1, S2, S3, S4, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      StringLiteral S3, StringLiteral S4, StringLiteral S5,
                      T Value) {
    return Case(S0, Value).Cases(S1, S2, S3, S4, S5, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      StringLiteral S3, StringLiteral S4, StringLiteral S5,
                      StringLiteral S6, T Value) {
    return Case(S0, Value).Cases(S1, S2, S3, S4, S5, S6, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      StringLiteral S3, StringLiteral S4, StringLiteral S5,
                      StringLiteral S6, StringLiteral S7, T Value) {
    return Case(S0, Value).Cases(S1, S2, S3, S4, S5, S6, S7, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      StringLiteral S3, StringLiteral S4, StringLiteral S5,
                      StringLiteral S6, StringLiteral S7, StringLiteral S8,
                      T Value) {
    return Case(S0, Value).Cases(S1, S2, S3, S4, S5, S6, S7, S8, Value);
  }

  StringSwitch &Cases(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                      StringLiteral S3, StringLiteral S4, StringLiteral S5,
                      StringLiteral S6, StringLiteral S7, StringLiteral S8,
                      StringLiteral S9, T Value) {
    return Case(S0, Value).Cases(S1, S2, S3, S4, S5, S6, S7, S8, S9, Value);
  }

  // Case-insensitive case matchers.
  StringSwitch &CaseLower(StringLiteral S, T Value) {
    if (!Result && St.equals_insensitive(S))
      Result = std::move(Value);

    return *this;
  }

  StringSwitch &EndsWithLower(StringLiteral S, T Value) {
    if (!Result && St.ends_with_insensitive(S))
      Result = Value;

    return *this;
  }

  StringSwitch &StartsWithLower(StringLiteral S, T Value) {
    if (!Result && St.starts_with_insensitive(S))
      Result = std::move(Value);

    return *this;
  }

  StringSwitch &CasesLower(StringLiteral S0, StringLiteral S1, T Value) {
    return CaseLower(S0, Value).CaseLower(S1, Value);
  }

  StringSwitch &CasesLower(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                           T Value) {
    return CaseLower(S0, Value).CasesLower(S1, S2, Value);
  }

  StringSwitch &CasesLower(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                           StringLiteral S3, T Value) {
    return CaseLower(S0, Value).CasesLower(S1, S2, S3, Value);
  }

  StringSwitch &CasesLower(StringLiteral S0, StringLiteral S1, StringLiteral S2,
                           StringLiteral S3, StringLiteral S4, T Value) {
    return CaseLower(S0, Value).CasesLower(S1, S2, S3, S4, Value);
  }

  [[nodiscard]] Ret Default(T Value) {
    if (Result)
      return std::move(*Result);
    return Value;
  }

  [[nodiscard]] operator Ret() {
    exi_assert(Result.has_value(), "Fell off the end of a string-switch");
    return std::move(*Result);
  }
};

} // namespace exi
