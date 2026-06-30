//===- Common/ConstexprLiteral.hpp ----------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file defines a utility to generate a template <char...> from
/// a string literal.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ConstexprLists.hpp>
#include <Common/Fundamental.hpp>

#if EXI_HAS_BUILTIN(__integer_pack) && 0
/// Currently gives the following error:
/// sorry, unimplemented: '__integer_pack(...)' is not the entire pattern of the pack expansion
# define EXI_HAS_CHARSEQ_INTEGER_PACK 1
#else
# define EXI_HAS_CHARSEQ_INTEGER_PACK 0
#endif

namespace exi {

/// Represents a constant sequence of characters.
template <char...CC> struct charseq {
  static inline constexpr usize kSize = sizeof...(CC);
  static inline constexpr char kData[kSize + 1] {CC..., '\0'};
  static constexpr usize size() { return kSize; }
  static constexpr const char* data() { return kData; }
};

/// Holds the data for `charseq`.
template <usize N> struct charseq_c {
  static constexpr usize kSize = N - 1;
  char Data[N];
#if EXI_HAS_CHARSEQ_INTEGER_PACK
  consteval charseq_c(const char(&Arr)[N]) : Data{Arr[__integer_pack(N)]...} {}
#else
  template <usize...II>
  consteval charseq_c(const char(&Arr)[N], idxseq<II...>) : Data{Arr[II]...} {}
  consteval charseq_c(const char(&Arr)[N]) : charseq_c(Arr, make_idxseq<N>{}) {}
#endif
};

/// Holds the data for `charseq`.
template <> struct charseq_c<0> {
  static constexpr usize kSize = 0;
  static constexpr char Data[1] = {};
};

#if EXI_HAS_CHARSEQ_INTEGER_PACK

/// Creates a `charseq` from a string literal.
/// eg. `make_charseq<"Hello world!">`
template <charseq_c S>
using make_charseq = charseq<S.Data[__integer_pack(S.kSize)]...>;

#else

namespace H {
template <charseq_c S, usize...II>
auto make_charseq_i(idxseq<II...>) -> charseq<S.Data[II]...>;
}

/// Creates a `charseq` from a string literal.
/// eg. `make_charseq<"Hello world!">`
template <charseq_c S>
using make_charseq = decltype(H::make_charseq_i<S>(make_idxseq<S.kSize>{}));

#endif

//////////////////////////////////////////////////////////////////////////
// Utilities

template <typename S, usize Off = 0>
EXI_FLATTEN constexpr bool matches_seq(const char* In) {
  return [In] <usize...II> (idxseq<II...>) -> bool {
    return ((In[II + Off] == S::kData[II]) && ...);
  } (make_idxseq<S::kSize>{});
}

template <charseq_c S, usize Off = 0>
EXI_FLATTEN constexpr bool matches_seq(const char* In) {
  return [In] <usize...II> (idxseq<II...>) -> bool {
    return ((In[II + Off] == S.Data[II]) && ...);
  } (make_idxseq<S.kSize>{});
}

} // namespace exi
