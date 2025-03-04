//===- Common/MMatch.hpp --------------------------------------------===//
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
/// This file provides a "Multi Match" utility. Can make comparisons more
/// consise, as it will convert any inputs to the base type.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <type_traits>

namespace exi {
namespace H {
template <class T> struct SimplifyMMatch {
  using type = std::add_const_t<T>;
};
template <class T> struct SimplifyMMatch<T&&> {
  using type = typename SimplifyMMatch<T>::type;
};

template <class T> using simplify_mmatch_t
  = typename SimplifyMMatch<T>::type;
} // namespace H

/// Match adaptor, simplifies some stuff.
/// Use like `MMatch(V).is(arg1, arg2, ...)`.
/// Or `mmatch<T>(V).is(arg1, arg2, ...)`.
template <typename T,
  typename ConvT = dummy_t>
class MMatch {
public:
  using type = ConvT;
  using value_type = ConvT;
  explicit constexpr MMatch(T Value) : Data(EXI_FWD(Value)) {}
public:
  /// Checks if data is equal to a single input.
  constexpr bool is(ConvT Value) const {
    return (Data == Value);
  }

  /// Checks if data is equal to any of the inputs.
  constexpr bool is(ConvT First, auto&&...Rest) const
   requires(sizeof...(Rest) > 0) {
    return (is(EXI_FWD(First)) || ... || is(static_cast<ConvT>(EXI_FWD(Rest))));
  }

  /// Checks if data is not equal to any of the inputs.
  EXI_INLINE constexpr bool isnt(ConvT First, auto&&...Rest) const {
    return !is(EXI_FWD(First), EXI_FWD(Rest)...);
  }

  /// Checks if data is in the exclusive range of inputs.
  constexpr bool in(ConvT Lo, ConvT Hi) const {
    return (EXI_FWD(Lo) <= Data) && (Data < EXI_FWD(Hi));
  }

  /// Checks if data is in the inclusive range of inputs.
  constexpr bool iin(ConvT Lo, ConvT Hi) const {
    return (EXI_FWD(Lo) <= Data) && (Data <= EXI_FWD(Hi));
  }

public:
  type Data;
};

/// Match adaptor, simplifies some stuff.
/// Use like `MMatch(V).is(arg1, arg2, ...)`.
template <typename T>
class MMatch<T, dummy_t> {
public:
  using type = H::simplify_mmatch_t<T>;
  using value_type = std::remove_cvref_t<T>;
  explicit constexpr MMatch(T Value) : Data(EXI_FWD(Value)) {}
public:
  /// Checks if data is equal to a single input.
  constexpr bool is(auto&& Value) const {
    return (Data == EXI_FWD(Value));
  }

  /// Checks if data is equal to any of the inputs.
  constexpr bool is(auto&& First, auto&&...Rest) const
   requires(sizeof...(Rest) > 0) {
    return (is(EXI_FWD(First)) || ... || is(EXI_FWD(Rest)));
  }

  /// Checks if data is not equal to any of the inputs.
  EXI_INLINE constexpr bool isnt(auto&& First, auto&&...Rest) const {
    return !is(EXI_FWD(First), EXI_FWD(Rest)...);
  }

  /// Checks if data is in the exclusive range of inputs.
  constexpr bool in(auto&& Lo, auto&& Hi) const {
    return (EXI_FWD(Lo) <= Data) && (Data < EXI_FWD(Hi));
  }

  /// Checks if data is in the inclusive range of inputs.
  constexpr bool iin(auto&& Lo, auto&& Hi) const {
    return (EXI_FWD(Lo) <= Data) && (Data <= EXI_FWD(Hi));
  }

public:
  type Data;
};

template <typename T>
MMatch(T&&) -> MMatch<T>;

template <typename Conv = dummy_t>
EXI_INLINE constexpr auto mmatch(auto&& ToMatch) noexcept {
  using MatchT = MMatch<decltype(ToMatch), Conv>;
  return MatchT(EXI_FWD(ToMatch));
}

} // namespace exi
