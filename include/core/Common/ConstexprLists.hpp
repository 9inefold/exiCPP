//===- Common/ConstexprLists.hpp ------------------------------------===//
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
/// This file defines some utilities for constexpr lists.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <type_traits>
#include <utility>

namespace exi {

/// Used when expanding packs to get a sequence of type T.
/// Eg.
///    void Foo(Unfold<T, II>... Args);
template <typename T, usize>
using Unfold = T;

//======================================================================//
// TypePackElement
//======================================================================//

#if EXI_HAS_BUILTIN(__type_pack_element)
template <usize I, typename...TT>
using TypePackElement = __type_pack_element<I, TT...>;
#else
template <usize I, typename...TT>
using TypePackElement = std::tuple_element_t<I, std::tuple<TT...>>;
#endif

//======================================================================//
// idxseq
//======================================================================//

template <usize I>
using idx_c = std::integral_constant<usize, I>;

template <usize...II> struct idxseq {
  using value_type = usize;
  static constexpr usize kSize = sizeof...(II);
  static constexpr usize size() noexcept { return kSize; }
};

#if EXI_HAS_BUILTIN(__make_integer_seq)
namespace idxseq_detail {
template <typename, usize...II>
using proxy = idxseq<II...>;
}
/// Uses __make_integer_seq.
template <usize N>
using make_idxseq = __make_integer_seq<idxseq_detail::proxy, usize, N>;
#elif EXI_HAS_BUILTIN(__integer_pack)
/// Uses __integer_pack.
template <usize N>
using make_idxseq = idxseq<__integer_pack(N)...>;
#else
# include "D/IdxSeq.mac"
/// Uses a custom algorithm which is described in `"I/IdxSeq.mac"`.
template <usize N>
using make_idxseq = idxseq_detail::from<N>;
#endif

//////////////////////////////////////////////////////////////////////////

template <usize...II>
using stdseq = std::index_sequence<II...>;

template <usize I>
using make_stdseq = std::make_index_sequence<I>;

//======================================================================//
// typeseq
//======================================================================//

template <typename T>
struct type_c {
  using type = T;
};

template <typename...TT> struct typeseq {
  static constexpr usize kSize = sizeof...(TT);
  static constexpr usize size() noexcept { return kSize; }

  template <usize I>
  using At = TypePackElement<I, TT...>;
};

} // namespace exi
