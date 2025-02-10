//===- Common/SSelect.hpp -------------------------------------------===//
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
/// This file provides a "Static Select" utility. It allows for selection
/// from a predefined list of values given an input.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Support/ErrorHandle.hpp>
#include <concepts>
#include <type_traits>

namespace exi {
namespace H {

template <class T, class...TT>
concept all_same = (true && ... && std::same_as<T, TT>);

template <typename...TT>
concept any_const = (false || ... || std::is_const_v<TT>);

template <typename...TT>
concept all_lvalue_references =
  (true && ... && std::is_lvalue_reference_v<TT>);

template <typename To, typename...Froms>
concept all_convertible =
  (true && ... && std::is_convertible_v<Froms, To>);

template <class Derived, class Base>
concept derived_from_base =
  std::derived_from<
    std::remove_cvref_t<Derived>,
    std::remove_cvref_t<Base>>;

template <class Base, class...DD>
concept all_derived =
  (true && ... && derived_from_base<DD, Base>);

template <class MaybeDerived, class Base>
concept same_or_derived
  = std::same_as<MaybeDerived, Base>
  || derived_from_base<MaybeDerived, Base>;

template <class Base, class...DDs>
concept all_same_or_derived =
  (true && ... && same_or_derived<DDs, Base>);

//////////////////////////////////////////////////////////////////////////
// SimplifySSelectRef

template <class Base, class...Rest>
struct SimplifySSelectRef {
  using base_type = std::conditional_t<
    any_const<std::remove_reference_t<Rest>...>,
      const Base, Base>;
  static constexpr bool kSharedBase
    = all_same_or_derived<Base, std::remove_cvref_t<Rest>...>;
  using type = std::conditional_t<kSharedBase, base_type&, Base>;
};

template <class Base, class...Rest>
struct SimplifySSelectRef<const Base, Rest...> {
  static constexpr bool kSharedBase
    = all_same_or_derived<Base, std::remove_cvref_t<Rest>...>;
  using type = std::conditional_t<kSharedBase, const Base&, Base>;
};

template <class Base, class...Rest>
using simplify_sselect_ref_t = typename
  SimplifySSelectRef<Base, Rest...>::type;

//////////////////////////////////////////////////////////////////////////
// simplify_sselect_t

template <class First, class...Rest>
struct SimplifySSelect {
  using type = std::remove_cvref_t<First>;
  static_assert(all_convertible<type, Rest...>);
};

template <class First, class...Rest>
requires all_lvalue_references<Rest...>
struct SimplifySSelect<First&, Rest...> {
  using type = simplify_sselect_ref_t<First, Rest...>;
};

template <class First, class...Rest>
requires all_lvalue_references<Rest...>
struct SimplifySSelect<const First&, Rest...> {
  using type = simplify_sselect_ref_t<const First, Rest...>;
};

template <class First, class...Rest>
using simplify_sselect_t =
  typename SimplifySSelect<First, Rest...>::type;

//////////////////////////////////////////////////////////////////////////
// Selection Implementation

template <typename SelectT> struct SSelectImpl {
  ALWAYS_INLINE SelectT pick(SelectT S0) {
    return S0;
  }

  ALWAYS_INLINE SelectT pick(SelectT S0, SelectT S1) {
    if (Data == 0)
      return S0;
    else
      return S1;
  }

  SelectT pick(SelectT S0, SelectT S1, SelectT S2) {
    if (Data == 0)
      return S0;
    else if (Data == 1)
      return S1;
    else
      return S2;
  }

  SelectT pick(SelectT S0, SelectT S1, SelectT S2, SelectT S3) {
    if (Data == 0)
      return S0;
    else if (Data == 1)
      return S1;
    else if (Data == 2)
      return S2;
    else
      return S3;
  }

  SelectT pick(SelectT S0, SelectT S1, SelectT S2, SelectT S3, SelectT S4) {
    if (Data == 0)
      return S0;
    else if (Data == 1)
      return S1;
    else if (Data == 2)
      return S2;
    else if (Data == 3)
      return S3;
    else
      return S4;
  }

  SelectT pick(SelectT S0, SelectT S1, SelectT S2,
               SelectT S3, SelectT S4, SelectT S5, auto&&...Rest) {
    if (Data < 4)
      return pick(S0, S1, S2, S3, S4);
    Data -= 5;
    return pick(S5, EXI_FWD(Rest)...);
  }

public:
  u64 Data = 0;
};

} // namespace H

//////////////////////////////////////////////////////////////////////////
// SSelect

/// Selection adaptor, simplifies some stuff.
/// Use like `SSelect<N>(V).pick(arg1, ..., argN)`.
template <usize NOpts, bool OnesIndexed = false>
class SSelect {
  static_assert(NOpts != 0, "Invalid opt number!");
public:
  static constexpr usize value = NOpts;
  static constexpr usize kMin = OnesIndexed ? 1 : 0;
  static constexpr usize kMax = OnesIndexed ? (NOpts - 1) : NOpts;
protected:
  const u64 Data = kMin;
public:
  explicit constexpr SSelect(u64 Value) :
   Data(SSelect::FixValue(Value)) {}

  static constexpr u64 FixValue(u64 Value) noexcept {
    if constexpr (OnesIndexed) {
      exi_invariant(Value != 0,
        "Value cannot be zero when ones indexed!");
      if EXI_UNLIKELY(Value == 0)
        return 1u;
    }
    exi_invariant(Value <= kMax, "Value out of range!");
    if EXI_UNLIKELY(Value > kMax)
      return kMax;
    return Value;
  }

  /// Select value from a sequence of potential values.
  template <typename FirstT, typename...RestT>
  requires(sizeof...(RestT) == (NOpts - 1))
  constexpr auto pick(
    FirstT&& First EXI_LIFETIMEBOUND,
    RestT&&...Rest EXI_LIFETIMEBOUND) const
   -> H::simplify_sselect_t<FirstT, RestT...> {
    using RetT = H::simplify_sselect_t<FirstT, RestT...>;
    using PickT = H::SSelectImpl<RetT>;
    return PickT(Data - kMin).pick(EXI_FWD(First), EXI_FWD(Rest)...);
  }

  u64 data() const { return Data; }
};

/// Empty selection adaptor, does nothing.
template <> class SSelect<0, false> {
  static constexpr usize value = 0;
  explicit constexpr SSelect(u64 Value) {
    exi_invariant(Value == 0,
      "Value cannot be zero when ones indexed!");
  }
private:
  const u64 Data = 0;
};

//////////////////////////////////////////////////////////////////////////
// SSelBool

class SSelBool : public SSelect<2, false> {
  using BaseType = SSelect<2, false>;
  using BaseType::BaseType;
  using BaseType::data;
public:
  explicit constexpr SSelBool(bool Value) : BaseType(Value) {}
  bool data() const { return BaseType::Data; }
};

} // namespace exi
