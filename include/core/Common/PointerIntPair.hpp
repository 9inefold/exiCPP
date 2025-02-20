//===- Common/PointerIntPair.hpp -------------------------------------===//
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
/// This file defines the PointerIntPair class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/Fundamental.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/PointerLikeTraits.hpp>
#include <Support/type_traits.hpp>
#include <cassert>
#include <cstring>
#include <limits>

namespace exi {

namespace H {
template <typename Ptr> struct PunnedPointer {
  static_assert(sizeof(Ptr) == sizeof(iptr));

  // Asserts that allow us to let the compiler implement the destructor and
  // copy/move constructors
  static_assert(std::is_trivially_destructible<Ptr>::value);
  static_assert(std::is_trivially_copy_constructible<Ptr>::value);
  static_assert(std::is_trivially_move_constructible<Ptr>::value);

  explicit constexpr PunnedPointer(iptr I = 0) : Data(I) {}

  constexpr PunnedPointer &operator=(iptr V) {
    Data = V;
    return *this;
  }

  constexpr iptr asInt() const { return Data; }
  constexpr operator iptr() const { return asInt(); }

  Ptr *getPointerAddress() {
    return reinterpret_cast<Ptr *>(&Data);
  }
  illegal_constexpr const Ptr *getPointerAddress() const {
    return FOLD_CXPR(reinterpret_cast<const Ptr *>(&Data));
  }

private:
  alignas(Ptr) iptr Data;
};
} // namespace H

template <typename T, typename Enable> struct DenseMapInfo;
template <typename PointerT, unsigned IntBits, typename PtrTraits>
struct PointerIntPairInfo;

/// PointerIntPair - This class implements a pair of a pointer and small
/// integer.  It is designed to represent this in the space required by one
/// pointer by bitmangling the integer into the low part of the pointer.  This
/// can only be done for small integers: typically up to 3 bits, but it depends
/// on the number of bits available according to PointerLikeTypeTraits for the
/// type.
///
/// Note that PointerIntPair always puts the IntVal part in the highest bits
/// possible.  For example, PointerIntPair<void*, 1, bool> will put the bit for
/// the bool into bit #2, not bit #0, which allows the low two bits to be used
/// for something else.  For example, this allows:
///   `PointerIntPair<PointerIntPair<void*, 1, bool>, 1, bool>`
/// ... and the two bools will land in different bits.
template <typename PointerTy, unsigned IntBits, typename IntType = unsigned,
          typename PtrTraits = PointerLikeTypeTraits<PointerTy>,
          typename Info = PointerIntPairInfo<PointerTy, IntBits, PtrTraits>>
class PointerIntPair {
  // Used by MSVC visualizer and generally helpful for debugging/visualizing.
  using InfoTy = Info;
  H::PunnedPointer<PointerTy> Value;

public:
  constexpr PointerIntPair() = default;

  PointerIntPair(PointerTy PtrVal, IntType IntVal) {
    setPointerAndInt(PtrVal, IntVal);
  }

  explicit PointerIntPair(PointerTy PtrVal) { initWithPointer(PtrVal); }

  PointerTy getPointer() const { return Info::getPointer(Value); }

  IntType getInt() const { return (IntType)Info::getInt(Value); }

  inline PointerTy operator->() const {
    const PointerTy Ptr = Info::getPointer(Value); 
    exi_invariant(PtrTraits::getAsVoidPointer(Ptr) != nullptr);
    return Ptr;
  }

  void setPointer(PointerTy PtrVal) & {
    Value = Info::updatePointer(Value, PtrVal);
  }

  void setInt(IntType IntVal) & {
    Value = Info::updateInt(Value, static_cast<iptr>(IntVal));
  }

  void initWithPointer(PointerTy PtrVal) & {
    Value = Info::updatePointer(0, PtrVal);
  }

  void setPointerAndInt(PointerTy PtrVal, IntType IntVal) & {
    Value = Info::updateInt(Info::updatePointer(0, PtrVal),
                            static_cast<iptr>(IntVal));
  }

  PointerTy const *getAddrOfPointer() const {
    return const_cast<PointerIntPair *>(this)->getAddrOfPointer();
  }

  PointerTy *getAddrOfPointer() {
    exi_assert(Value == reinterpret_cast<iptr>(getPointer()),
              "Can only return the address if IntBits is cleared and "
              "PtrTraits doesn't change the pointer");
    return Value.getPointerAddress();
  }

  void *getOpaqueValue() const {
    return reinterpret_cast<void *>(Value.asInt());
  }

  void setFromOpaqueValue(void *Val) & {
    Value = reinterpret_cast<iptr>(Val);
  }

  static PointerIntPair getFromOpaqueValue(void *V) {
    PointerIntPair P;
    P.setFromOpaqueValue(V);
    return P;
  }

  // Allow PointerIntPairs to be created from const void * if and only if the
  // pointer type could be created from a const void *.
  static PointerIntPair getFromOpaqueValue(const void *V) {
    (void)PtrTraits::getFromVoidPointer(V);
    return getFromOpaqueValue(const_cast<void *>(V));
  }

  bool operator==(const PointerIntPair &RHS) const {
    return Value == RHS.Value;
  }

  bool operator!=(const PointerIntPair &RHS) const {
    return Value != RHS.Value;
  }

  bool operator<(const PointerIntPair &RHS) const { return Value < RHS.Value; }
  bool operator>(const PointerIntPair &RHS) const { return Value > RHS.Value; }

  bool operator<=(const PointerIntPair &RHS) const {
    return Value <= RHS.Value;
  }

  bool operator>=(const PointerIntPair &RHS) const {
    return Value >= RHS.Value;
  }
};

template <typename PointerT, unsigned IntBits, typename PtrTraits>
struct PointerIntPairInfo {
  static_assert(PtrTraits::NumLowBitsAvailable <
                    std::numeric_limits<uptr>::digits,
                "cannot use a pointer type that has all bits free");
  static_assert(IntBits <= PtrTraits::NumLowBitsAvailable,
                "PointerIntPair with integer size too large for pointer");
  enum MaskAndShiftConstants : uptr {
    /// PointerBitMask - The bits that come from the pointer.
    PointerBitMask =
        ~(uptr)(((iptr)1 << PtrTraits::NumLowBitsAvailable) - 1),

    /// IntShift - The number of low bits that we reserve for other uses, and
    /// keep zero.
    IntShift = (uptr)PtrTraits::NumLowBitsAvailable - IntBits,

    /// IntMask - This is the unshifted mask for valid bits of the int type.
    IntMask = (uptr)(((iptr)1 << IntBits) - 1),

    // ShiftedIntMask - This is the bits for the integer shifted in place.
    ShiftedIntMask = (uptr)(IntMask << IntShift)
  };

  static PointerT getPointer(iptr Value) {
    return PtrTraits::getFromVoidPointer(
        reinterpret_cast<void *>(Value & PointerBitMask));
  }

  static iptr getInt(iptr Value) {
    return (Value >> IntShift) & IntMask;
  }

  static iptr updatePointer(iptr OrigValue, PointerT Ptr) {
    iptr PtrWord =
        reinterpret_cast<iptr>(PtrTraits::getAsVoidPointer(Ptr));
    exi_assert((PtrWord & ~PointerBitMask) == 0,
               "Pointer is not sufficiently aligned");
    // Preserve all low bits, just update the pointer.
    return PtrWord | (OrigValue & ~PointerBitMask);
  }

  static iptr updateInt(iptr OrigValue, iptr Int) {
    iptr IntWord = static_cast<iptr>(Int);
    exi_assert((IntWord & ~IntMask) == 0,
               "Integer too large for field");

    // Preserve all bits other than the ones we are updating.
    return (OrigValue & ~ShiftedIntMask) | IntWord << IntShift;
  }
};

// Provide specialization of DenseMapInfo for PointerIntPair.
template <typename PointerTy, unsigned IntBits, typename IntType>
struct DenseMapInfo<PointerIntPair<PointerTy, IntBits, IntType>, void> {
  using Ty = PointerIntPair<PointerTy, IntBits, IntType>;

  static Ty getEmptyKey() {
    uptr Val = static_cast<uptr>(-1);
    Val <<= PointerLikeTypeTraits<Ty>::NumLowBitsAvailable;
    return Ty::getFromOpaqueValue(reinterpret_cast<void *>(Val));
  }

  static Ty getTombstoneKey() {
    uptr Val = static_cast<uptr>(-2);
    Val <<= PointerLikeTypeTraits<PointerTy>::NumLowBitsAvailable;
    return Ty::getFromOpaqueValue(reinterpret_cast<void *>(Val));
  }

  static unsigned getHashValue(Ty V) {
    uptr IV = reinterpret_cast<uptr>(V.getOpaqueValue());
    return unsigned(IV) ^ unsigned(IV >> 9);
  }

  static bool isEqual(const Ty &LHS, const Ty &RHS) { return LHS == RHS; }
};

// Teach SmallPtrSet that PointerIntPair is "basically a pointer".
template <typename PointerTy, unsigned IntBits, typename IntType,
          typename PtrTraits>
struct PointerLikeTypeTraits<
    PointerIntPair<PointerTy, IntBits, IntType, PtrTraits>> {
  static inline void *
  getAsVoidPointer(const PointerIntPair<PointerTy, IntBits, IntType> &P) {
    return P.getOpaqueValue();
  }

  static inline PointerIntPair<PointerTy, IntBits, IntType>
  getFromVoidPointer(void *P) {
    return PointerIntPair<PointerTy, IntBits, IntType>::getFromOpaqueValue(P);
  }

  static inline PointerIntPair<PointerTy, IntBits, IntType>
  getFromVoidPointer(const void *P) {
    return PointerIntPair<PointerTy, IntBits, IntType>::getFromOpaqueValue(P);
  }

  static constexpr int NumLowBitsAvailable =
      PtrTraits::NumLowBitsAvailable - IntBits;
};

// Allow structured bindings on PointerIntPair.
template <std::size_t I, typename PointerTy, unsigned IntBits, typename IntType,
          typename PtrTraits, typename Info>
decltype(auto)
get(const PointerIntPair<PointerTy, IntBits, IntType, PtrTraits, Info> &Pair) {
  static_assert(I < 2);
  if constexpr (I == 0)
    return Pair.getPointer();
  else
    return Pair.getInt();
}

} // namespace exi

namespace std {
template <typename PointerTy, unsigned IntBits, typename IntType,
          typename PtrTraits, typename Info>
struct tuple_size<
    exi::PointerIntPair<PointerTy, IntBits, IntType, PtrTraits, Info>>
    : std::integral_constant<std::size_t, 2> {};

template <std::size_t I, typename PointerTy, unsigned IntBits, typename IntType,
          typename PtrTraits, typename Info>
struct tuple_element<
    I, exi::PointerIntPair<PointerTy, IntBits, IntType, PtrTraits, Info>>
    : std::conditional<I == 0, PointerTy, IntType> {};
} // namespace std
