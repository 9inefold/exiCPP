//===- Common/AlignedInt.hpp ----------------------------------------===//
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
/// This file defines an aligned proxy class for integer types.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/DenseMapInfo.hpp>
#include <Common/EnumTraits.hpp>
#include <Common/Fundamental.hpp>
#include <concepts>

#define AL_PREFER(N) EXI_PREFER_NAME(ia##N) EXI_PREFER_NAME(ua##N)

namespace exi {
template <exi::integral_ex IntT>
struct alignas(8) AlignedInt;
} // namespace exi

using ia8  = exi::AlignedInt<i8>;
using ia16 = exi::AlignedInt<i16>;
using ia32 = exi::AlignedInt<i32>;
using ia64 = exi::AlignedInt<i64>;

using ua8  = exi::AlignedInt<u8>;
using ua16 = exi::AlignedInt<u16>;
using ua32 = exi::AlignedInt<u32>;
using ua64 = exi::AlignedInt<u64>;

namespace exi {

template <exi::integral_ex IntT>
struct AL_PREFER(8) AL_PREFER(16)
			 AL_PREFER(32) AL_PREFER(64) alignas(usize) AlignedInt {
  constexpr AlignedInt() = default;

  template <typename T>
  requires std::constructible_from<IntT, T>
  constexpr AlignedInt(T&& I) : Data(EXI_FWD(I)) { }

  template <typename T>
  requires std::assignable_from<IntT, T>
  constexpr AlignedInt& operator=(T&& I) {
    this->Data = EXI_FWD(I);
    return *this;
  }

  template <typename T>
  requires std::convertible_to<T, IntT>
  constexpr operator T() const { return Data; }

public:
	IntT Data = 0;
};

template <typename X, typename Y>
constexpr bool operator==(AlignedInt<X> LHS, AlignedInt<Y> RHS) {
  return LHS.Data == RHS.Data;
}

template <typename IntT, typename T>
constexpr bool operator==(AlignedInt<IntT> LHS, T RHS) {
  return LHS.Data == RHS;
}

template <typename T, typename IntT>
constexpr bool operator==(T LHS, AlignedInt<IntT> RHS) {
  return LHS == RHS.Data;
}

// Provide DenseMapInfo for `AlignedInt<T>`.
template <typename IntT> struct DenseMapInfo<AlignedInt<IntT>> {
	using Ty = AlignedInt<IntT>;
	using InfoT = DenseMapInfo<IntT>;

  static inline Ty getEmptyKey() { return InfoT::getEmptyKey(); }
  static inline Ty getTombstoneKey() { return InfoT::getTombstoneKey(); }

  static unsigned getHashValue(const Ty& Val) {
    return InfoT::getHashValue(Val.Data);
  }

  static bool isEqual(const Ty& LHS, const Ty& RHS) {
		/// Don't use the equality operators, if possible
    return LHS.Data == LHS.Data;
  }
};

} // namespace exi

#undef AL_PREFER
