//===- exi/Basic/NBitInt.hpp ----------------------------------------===//
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
/// This file defines a statically sized n-bit integer type.
///
//===----------------------------------------------------------------===//

#pragma once

#include "core/Common/APInt.hpp"
#include "core/Common/Fundamental.hpp"
#include "core/Support/MathExtras.hpp"

#define EXI_BITINT_ALLDATA 1

namespace exi {
class APInt;
class APSInt;
class raw_ostream;

class NBitIntBase;

namespace H {
template <typename T>
concept is_bitint
  = std::derived_from<T, NBitIntBase>
  && !std::same_as<T, NBitIntBase>;

template <typename T>
concept is_bitint_or_int
  = is_bitint<T> || std::integral<T>;

template <bool IsSigned>
using NBitIntValueType
  = std::conditional_t<IsSigned, i64, u64>;
} // namespace H

//===----------------------------------------------------------------===//
// NBitInt*
//===----------------------------------------------------------------===//

template <bool IsSigned, unsigned Bits>
class NBitIntCommon;

/// The base for all `NBitInt`s. Provides some commonly used functions,
/// as well as being useful for determining if an object is bitint classed.
class NBitIntBase {
protected:
  using SInt = H::NBitIntValueType<true>;
  using UInt = H::NBitIntValueType<false>;
  static_assert(sizeof(SInt) == sizeof(UInt));
public:
  static constexpr usize kMaxBits = bitsizeof_v<UInt>;

  static bool IsNBit(SInt Val, unsigned Bits) {
    return exi::isIntN(Bits, Val);
  }
  static bool IsNBit(UInt Val, unsigned Bits) {
    return exi::isUIntN(Bits, Val);
  }

protected:
  static APInt MakeAPInt(SInt Val, unsigned Bits);
  static APInt MakeAPInt(UInt Val, unsigned Bits);

  template <bool Signed, typename InpT>
#if !EXI_INVARIANTS
  ALWAYS_INLINE
#endif
  static void AssertNBitInt(InpT I, unsigned Bits) noexcept {
#if EXI_INVARIANTS
    constexpr bool kSigned = std::is_signed_v<InpT>;
    if (Signed != kSigned) {
      if (!Signed)
        exi_assert(I >= 0, "Negative sign conversion!");
    }
    if (Signed)
      exi_assert(IsNBit(SInt(I), Bits));
    else
      exi_assert(IsNBit(UInt(I), Bits));
#endif // EXI_INVARIANTS
  }

  /// Casts to a type directly. Avoid calling this.
  template <typename OutT, bool Sign, unsigned Bits>
  inline static OutT CastDirectly(
   NBitIntCommon<Sign, Bits> I) noexcept {
    return static_cast<OutT>(I.Data);
  }

  /// Returns `AllData`.
  template <bool Sign, unsigned Bits>
  inline static UInt GetAllDataDirectly(
   NBitIntCommon<Sign, Bits> I) noexcept {
    return I.AllData;
  }
};

/// The common interface for `NBit*Int`s. Uses a bitfield under the hood to
/// represent the data. If `_BitInt` is ever part of the C++ standard, a lot
/// of this can be replaced.
///
/// @tparam IsSigned If the underlying integer type is signed.
/// @tparam Bits The number of bits in the bitfield.
template <bool IsSigned, unsigned Bits>
class NBitIntCommon : public NBitIntBase {
  static_assert(Bits > 0
    && Bits <= NBitIntBase::kMaxBits);

  template <class, class>
  friend struct IntCastCast;
  friend class NBitIntBase;

  using IntT = H::NBitIntValueType<IsSigned>;
  using BaseType = NBitIntBase;
  using SelfType = NBitIntCommon<IsSigned, Bits>;
public:
  using value_type = IntT;
  static constexpr unsigned kBits = Bits;
protected:
  union {
    /// Underlying bitfield.
    value_type Data : Bits;
    /// Whole value (not equivalent).
    BaseType::UInt AllData;
  };
protected:
  /// Hidden implementation which allows for direct construction from a `u64`.
  EXI_INLINE NBitIntCommon(u64 I, bool SetAllData, dummy_t) : AllData{} {
    if (!SetAllData)
      this->Data = I;
    else /*SetAllData*/
      this->AllData = I;
  }

  /// Invoke `BaseType::AssertNBitInt` with the current values.
  template <typename InpT>
  EXI_INLINE static void AssertNBitInt(InpT I) noexcept {
#if EXI_INVARIANTS
    BaseType::AssertNBitInt<IsSigned>(I, Bits);
#endif // EXI_INVARIANTS
  }

public:
  constexpr NBitIntCommon() : NBitIntCommon(0) {}
  constexpr NBitIntCommon(const SelfType&) noexcept = default;
  constexpr NBitIntCommon(SelfType&&) noexcept = default;
  constexpr NBitIntCommon& operator=(const SelfType&) noexcept = default;
  constexpr NBitIntCommon& operator=(SelfType&&) noexcept = default;

  /// We have to initialize `AllData` first,
  /// as otherwise it won't fill in the padding bits. Agh!
  template <std::integral InpT>
  EXI_INLINE NBitIntCommon(InpT I) : AllData{} {
    SelfType::AssertNBitInt(I);
    this->Data = static_cast<IntT>(I);
  }

  template <std::integral InpT>
  NBitIntCommon& operator=(InpT I) {
    SelfType::AssertNBitInt(I);
    this->Data = static_cast<IntT>(I);
    return *this;
  }

  template <unsigned InBits>
  requires(InBits != Bits)
  NBitIntCommon(NBitIntCommon<IsSigned, InBits> I) :
   NBitIntCommon(I.data()) {
  }

  template <unsigned InBits>
  requires(InBits != Bits)
  NBitIntCommon& operator=(NBitIntCommon<IsSigned, InBits> I) {
    return SelfType::operator=(I.Data);
  }

  constexpr bool operator<=>(
    const NBitIntCommon&) const = default;
  
  friend bool operator==(const NBitIntCommon& LHS, NBitIntCommon RHS) noexcept {
    return LHS.Data == RHS.Data;
  }
  template <std::integral InpT>
  friend bool operator==(const NBitIntCommon& LHS, InpT RHS) noexcept {
    SelfType::AssertNBitInt(RHS);
    return LHS.Data == RHS;
  }
  template <std::integral InpT>
  friend bool operator==(InpT LHS, const NBitIntCommon& RHS) noexcept {
    SelfType::AssertNBitInt(LHS);
    return LHS == RHS.Data;
  }

  ////////////////////////////////////////////////////////////////////////
  // Conversions

  EXI_INLINE constexpr IntT data() const { return Data; }
  EXI_INLINE constexpr IntT value() const { return Data; }
  EXI_INLINE constexpr IntT toInt() const { return Data; }

  EXI_INLINE APInt toAPInt() const {
    return BaseType::MakeAPInt(Data, Bits);
  }

  template <std::integral OutT>
  explicit(!H::same_sign<IntT, OutT>)
   constexpr operator OutT() const {
    return Data;
  }

  explicit operator APInt() const {
    return this->toAPInt();
  }
};

//////////////////////////////////////////////////////////////////////////
// NBit[S|U]Int

template <unsigned Bits>
class NBitSInt : public NBitIntCommon<true, Bits> {
  using NBitIntCommon<true, Bits>::Data;
public:
  using BaseType = NBitIntCommon<true, Bits>;
  using SelfType = NBitSInt<Bits>;
  using BaseType::BaseType;

  static constexpr NBitSInt FromBits(u64 Val) noexcept {
    exi_assert(NBitIntBase::IsNBit(Val, Bits));
    return NBitSInt(Val, false, dummy_v);
  }

  template <bool Sign, unsigned InBits>
  static constexpr NBitSInt FromBits(
   NBitIntCommon<Sign, InBits> Val) noexcept {
    const auto All = NBitIntBase::GetAllDataDirectly(Val);
    exi_assert(NBitIntBase::IsNBit(All, Bits));
    return NBitSInt(All, true, dummy_v);
  }

  template <std::integral InpT>
  NBitSInt& operator=(InpT I) {
    BaseType::operator=(I);
    return *this;
  }

  template <unsigned InBits>
  requires(InBits != Bits)
  NBitSInt& operator=(NBitSInt<InBits> I) {
    BaseType::operator=(I.Data);
    return *this;
  }
};

template <unsigned Bits>
class NBitUInt : public NBitIntCommon<false, Bits> {
  using NBitIntCommon<false, Bits>::Data;
public:
  using BaseType = NBitIntCommon<false, Bits>;
  using SelfType = NBitUInt<Bits>;
  using BaseType::BaseType;

  static constexpr NBitUInt FromBits(u64 Val) noexcept {
    exi_assert(NBitIntBase::IsNBit(Val, Bits));
    return NBitUInt(Val, false, dummy_v);
  }

  template <bool Sign, unsigned InBits>
  static constexpr NBitUInt FromBits(
   NBitIntCommon<Sign, InBits> Val) noexcept {
    const auto All = NBitIntBase::GetAllDataDirectly(Val);
    exi_assert(NBitIntBase::IsNBit(All, Bits));
    return NBitUInt(All, true, dummy_v);
  }

  template <std::integral InpT>
  NBitUInt& operator=(InpT I) {
    BaseType::operator=(I);
    return *this;
  }

  template <unsigned InBits>
  requires(InBits != Bits)
  NBitUInt& operator=(NBitUInt<InBits> I) {
    BaseType::operator=(I.Data);
    return *this;
  }
};

//////////////////////////////////////////////////////////////////////////
// Aliases

/// An arbitrary bitness signed integer.
template <unsigned Bits> using ibit = NBitSInt<Bits>;
/// An arbitrary bitness unsigned integer.
template <unsigned Bits> using ubit = NBitUInt<Bits>;

//////////////////////////////////////////////////////////////////////////
// Streaming

template <unsigned Bits>
inline raw_ostream& operator<<(
 raw_ostream& OS, ibit<Bits> I) noexcept {
  APInt(I).print(OS, true);
  return OS;
}

template <unsigned Bits>
inline raw_ostream& operator<<(
 raw_ostream& OS, ubit<Bits> I) noexcept {
  APInt(I).print(OS, false);
  return OS;
}

//===----------------------------------------------------------------===//
// IntCast
//===----------------------------------------------------------------===//

template <class FromT>
requires H::is_bitint<FromT>
struct IntCastByValue<FromT> {
  static constexpr bool value = true;
};

//////////////////////////////////////////////////////////////////////////
// IntCastIsPossible

template <class To, class From>
requires(std::integral<To> && H::is_bitint<From>)
struct IntCastIsPossible<To, From> {
  using FromT = IntCastFrom<From>;
  using IntT = typename From::value_type;
  // For `NBitIntCommon<Signed, Bits>` -> `Int`.
  static constexpr bool isPossible(FromT X) noexcept {
    return CheckIntCast<To, IntT>(X.data());
  }
};

template <class To, class From>
requires(H::is_bitint<To> && H::is_bitint_or_int<From>)
struct IntCastIsPossible<To, From> {
  using FromT = IntCastFrom<From>;
  using IntT = typename To::value_type;
public:
  /// @returns `Value.Data` if `NBitInt`, otherwise identity.
  EXI_INLINE static constexpr IntT GetValue(FromT X) noexcept {
    if constexpr (H::is_bitint<From>)
      return static_cast<IntT>(X.data());
    else
      return static_cast<IntT>(X);
  }

  /// For `AnyInt` -> `NBitIntCommon<Sign, Bits>`
  static constexpr bool isPossible(FromT X) noexcept {
    // Implemented above.
    if (!CheckIntCast<IntT>(X))
      return false;
    return NBitIntBase::IsNBit(
      GetValue(X), To::kBits);
  }
};

//////////////////////////////////////////////////////////////////////////
// IntCastCast

template <class To, class From>
requires H::is_bitint<To>
struct IntCastCast<To, From>
  : public IntCastIsPossible<To, From> /*Must implement GetValue*/,
    public IntCastDefaultFailure<To, From> {
  using IPT = IntCastIsPossible<To, From>;
  using FromT = IntCastFrom<From>;
public:
  static inline To doCast(FromT X) {
    return To(IPT::GetValue(X), false, dummy_v);
  }
};

} // namespace exi
