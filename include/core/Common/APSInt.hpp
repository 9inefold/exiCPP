//===- Common/APSInt.hpp --------------------------------------------===//
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
/// This file implements the APSInt class, which is a simple class that
/// represents an arbitrary sized integer that knows its signedness.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/APInt.hpp>

namespace exi {

/// An arbitrary precision integer that knows its signedness.
class [[nodiscard]] APSInt : public APInt {
  bool IsUnsigned = false;

public:
  /// Default constructor that creates an uninitialized APInt.
  explicit APSInt() = default;

  /// Create an APSInt with the specified width, default to unsigned.
  explicit APSInt(u32 BitWidth, bool isUnsigned = true)
      : APInt(BitWidth, 0), IsUnsigned(isUnsigned) {}

  explicit APSInt(APInt I, bool isUnsigned = true)
      : APInt(std::move(I)), IsUnsigned(isUnsigned) {}

  /// Construct an APSInt from a string representation.
  ///
  /// This constructor interprets the string \p Str using the radix of 10.
  /// The interpretation stops at the end of the string. The bit width of the
  /// constructed APSInt is determined automatically.
  ///
  /// \param Str the string to be interpreted.
  explicit APSInt(StrRef Str);

  /// Determine sign of this APSInt.
  ///
  /// \returns true if this APSInt is negative, false otherwise
  bool isNegative() const { return isSigned() && APInt::isNegative(); }

  /// Determine if this APSInt Value is non-negative (>= 0)
  ///
  /// \returns true if this APSInt is non-negative, false otherwise
  bool isNonNegative() const { return !isNegative(); }

  /// Determine if this APSInt Value is positive.
  ///
  /// This tests if the value of this APSInt is positive (> 0). Note
  /// that 0 is not a positive value.
  ///
  /// \returns true if this APSInt is positive.
  bool isStrictlyPositive() const { return isNonNegative() && !isZero(); }

  APSInt &operator=(APInt RHS) {
    // Retain our current sign.
    APInt::operator=(std::move(RHS));
    return *this;
  }

  APSInt &operator=(u64 RHS) {
    // Retain our current sign.
    APInt::operator=(RHS);
    return *this;
  }

  // Query sign information.
  bool isSigned() const { return !IsUnsigned; }
  bool isUnsigned() const { return IsUnsigned; }
  void setIsUnsigned(bool Val) { IsUnsigned = Val; }
  void setIsSigned(bool Val) { IsUnsigned = !Val; }

  /// Append this APSInt to the specified SmallString.
  void toString(SmallVecImpl<char> &Str, unsigned Radix = 10) const {
    APInt::toString(Str, Radix, isSigned());
  }
  using APInt::toString;

  /// If this int is representable using an i64.
  bool isRepresentableByInt64() const {
    // For unsigned values with 64 active bits, they technically fit into a
    // i64, but the user may get negative numbers and has to manually cast
    // them to unsigned. Let's not bet the user has the sanity to do that and
    // not give them a vague value at the first place.
    return isSigned() ? isSignedIntN(64) : isIntN(63);
  }

  /// Get the correctly-extended \c i64 value.
  i64 getExtValue() const {
    exi_assert(isRepresentableByInt64(), "Too many bits for i64");
    return isSigned() ? getSExtValue() : getZExtValue();
  }

  std::optional<i64> tryExtValue() const {
    return isRepresentableByInt64() ? std::optional<i64>(getExtValue())
                                    : std::nullopt;
  }

  APSInt trunc(u32 width) const {
    return APSInt(APInt::trunc(width), IsUnsigned);
  }

  APSInt extend(u32 width) const {
    if (IsUnsigned)
      return APSInt(zext(width), IsUnsigned);
    else
      return APSInt(sext(width), IsUnsigned);
  }

  APSInt extOrTrunc(u32 width) const {
    if (IsUnsigned)
      return APSInt(zextOrTrunc(width), IsUnsigned);
    else
      return APSInt(sextOrTrunc(width), IsUnsigned);
  }

  const APSInt &operator%=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    if (IsUnsigned)
      *this = urem(RHS);
    else
      *this = srem(RHS);
    return *this;
  }
  const APSInt &operator/=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    if (IsUnsigned)
      *this = udiv(RHS);
    else
      *this = sdiv(RHS);
    return *this;
  }
  APSInt operator%(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return IsUnsigned ? APSInt(urem(RHS), true) : APSInt(srem(RHS), false);
  }
  APSInt operator/(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return IsUnsigned ? APSInt(udiv(RHS), true) : APSInt(sdiv(RHS), false);
  }

  APSInt operator>>(unsigned Amt) const {
    return IsUnsigned ? APSInt(lshr(Amt), true) : APSInt(ashr(Amt), false);
  }
  APSInt &operator>>=(unsigned Amt) {
    if (IsUnsigned)
      lshrInPlace(Amt);
    else
      ashrInPlace(Amt);
    return *this;
  }
  APSInt relativeShr(unsigned Amt) const {
    return IsUnsigned ? APSInt(relativeLShr(Amt), true)
                      : APSInt(relativeAShr(Amt), false);
  }

  inline bool operator<(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return IsUnsigned ? ult(RHS) : slt(RHS);
  }
  inline bool operator>(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return IsUnsigned ? ugt(RHS) : sgt(RHS);
  }
  inline bool operator<=(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return IsUnsigned ? ule(RHS) : sle(RHS);
  }
  inline bool operator>=(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return IsUnsigned ? uge(RHS) : sge(RHS);
  }
  inline bool operator==(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return eq(RHS);
  }
  inline bool operator!=(const APSInt &RHS) const { return !((*this) == RHS); }

  bool operator==(i64 RHS) const {
    return compareValues(*this, get(RHS)) == 0;
  }
  bool operator!=(i64 RHS) const {
    return compareValues(*this, get(RHS)) != 0;
  }
  bool operator<=(i64 RHS) const {
    return compareValues(*this, get(RHS)) <= 0;
  }
  bool operator>=(i64 RHS) const {
    return compareValues(*this, get(RHS)) >= 0;
  }
  bool operator<(i64 RHS) const {
    return compareValues(*this, get(RHS)) < 0;
  }
  bool operator>(i64 RHS) const {
    return compareValues(*this, get(RHS)) > 0;
  }

  // The remaining operators just wrap the logic of APInt, but retain the
  // signedness information.

  APSInt operator<<(unsigned Bits) const {
    return APSInt(static_cast<const APInt &>(*this) << Bits, IsUnsigned);
  }
  APSInt &operator<<=(unsigned Amt) {
    static_cast<APInt &>(*this) <<= Amt;
    return *this;
  }
  APSInt relativeShl(unsigned Amt) const {
    return IsUnsigned ? APSInt(relativeLShl(Amt), true)
                      : APSInt(relativeAShl(Amt), false);
  }

  APSInt &operator++() {
    ++(static_cast<APInt &>(*this));
    return *this;
  }
  APSInt &operator--() {
    --(static_cast<APInt &>(*this));
    return *this;
  }
  APSInt operator++(int) {
    return APSInt(++static_cast<APInt &>(*this), IsUnsigned);
  }
  APSInt operator--(int) {
    return APSInt(--static_cast<APInt &>(*this), IsUnsigned);
  }
  APSInt operator-() const {
    return APSInt(-static_cast<const APInt &>(*this), IsUnsigned);
  }
  APSInt &operator+=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    static_cast<APInt &>(*this) += RHS;
    return *this;
  }
  APSInt &operator-=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    static_cast<APInt &>(*this) -= RHS;
    return *this;
  }
  APSInt &operator*=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    static_cast<APInt &>(*this) *= RHS;
    return *this;
  }
  APSInt &operator&=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    static_cast<APInt &>(*this) &= RHS;
    return *this;
  }
  APSInt &operator|=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    static_cast<APInt &>(*this) |= RHS;
    return *this;
  }
  APSInt &operator^=(const APSInt &RHS) {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    static_cast<APInt &>(*this) ^= RHS;
    return *this;
  }

  APSInt operator&(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return APSInt(static_cast<const APInt &>(*this) & RHS, IsUnsigned);
  }

  APSInt operator|(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return APSInt(static_cast<const APInt &>(*this) | RHS, IsUnsigned);
  }

  APSInt operator^(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return APSInt(static_cast<const APInt &>(*this) ^ RHS, IsUnsigned);
  }

  APSInt operator*(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return APSInt(static_cast<const APInt &>(*this) * RHS, IsUnsigned);
  }
  APSInt operator+(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return APSInt(static_cast<const APInt &>(*this) + RHS, IsUnsigned);
  }
  APSInt operator-(const APSInt &RHS) const {
    exi_assert(IsUnsigned == RHS.IsUnsigned, "Signedness mismatch!");
    return APSInt(static_cast<const APInt &>(*this) - RHS, IsUnsigned);
  }
  APSInt operator~() const {
    return APSInt(~static_cast<const APInt &>(*this), IsUnsigned);
  }

  /// Return the APSInt representing the maximum integer value with the given
  /// bit width and signedness.
  static APSInt getMaxValue(u32 numBits, bool Unsigned) {
    return APSInt(Unsigned ? APInt::getMaxValue(numBits)
                           : APInt::getSignedMaxValue(numBits),
                  Unsigned);
  }

  /// Return the APSInt representing the minimum integer value with the given
  /// bit width and signedness.
  static APSInt getMinValue(u32 numBits, bool Unsigned) {
    return APSInt(Unsigned ? APInt::getMinValue(numBits)
                           : APInt::getSignedMinValue(numBits),
                  Unsigned);
  }

  /// Determine if two APSInts have the same value, zero- or
  /// sign-extending as needed.
  static bool isSameValue(const APSInt &I1, const APSInt &I2) {
    return !compareValues(I1, I2);
  }

  /// Compare underlying values of two numbers.
  static int compareValues(const APSInt &I1, const APSInt &I2) {
    if (I1.getBitWidth() == I2.getBitWidth() && I1.isSigned() == I2.isSigned())
      return I1.IsUnsigned ? I1.compare(I2) : I1.compareSigned(I2);

    // Check for a bit-width mismatch.
    if (I1.getBitWidth() > I2.getBitWidth())
      return compareValues(I1, I2.extend(I1.getBitWidth()));
    if (I2.getBitWidth() > I1.getBitWidth())
      return compareValues(I1.extend(I2.getBitWidth()), I2);

    // We have a signedness mismatch. Check for negative values and do an
    // unsigned compare if both are positive.
    if (I1.isSigned()) {
      exi_assert(!I2.isSigned(), "Expected signed mismatch");
      if (I1.isNegative())
        return -1;
    } else {
      exi_assert(I2.isSigned(), "Expected signed mismatch");
      if (I2.isNegative())
        return 1;
    }

    return I1.compare(I2);
  }

  static APSInt get(i64 X) { return APSInt(APInt(64, X), false); }
  static APSInt getUnsigned(u64 X) { return APSInt(APInt(64, X), true); }

  /// Used to insert APSInt objects, or objects that contain APSInt objects,
  /// into FoldingSets.
  // TODO
  // void Profile(FoldingSetNodeID &ID) const;
};

inline bool operator==(i64 V1, const APSInt &V2) { return V2 == V1; }
inline bool operator!=(i64 V1, const APSInt &V2) { return V2 != V1; }
inline bool operator<=(i64 V1, const APSInt &V2) { return V2 >= V1; }
inline bool operator>=(i64 V1, const APSInt &V2) { return V2 <= V1; }
inline bool operator<(i64 V1, const APSInt &V2) { return V2 > V1; }
inline bool operator>(i64 V1, const APSInt &V2) { return V2 < V1; }

inline raw_ostream &operator<<(raw_ostream &OS, const APSInt &I) {
  I.print(OS, I.isSigned());
  return OS;
}

// TODO: Replace this with something more general.
std::string format_as(const APSInt& APS);

#if EXI_HAS_DENSE_MAP
/// Provide DenseMapInfo for APSInt, using the DenseMapInfo for APInt.
template <> struct DenseMapInfo<APSInt, void> {
  static inline APSInt getEmptyKey() {
    return APSInt(DenseMapInfo<APInt, void>::getEmptyKey());
  }

  static inline APSInt getTombstoneKey() {
    return APSInt(DenseMapInfo<APInt, void>::getTombstoneKey());
  }

  static unsigned getHashValue(const APSInt &Key) {
    return DenseMapInfo<APInt, void>::getHashValue(Key);
  }

  static bool isEqual(const APSInt &LHS, const APSInt &RHS) {
    return LHS.getBitWidth() == RHS.getBitWidth() &&
           LHS.isUnsigned() == RHS.isUnsigned() && LHS == RHS;
  }
};
#endif // EXI_HAS_DENSE_MAP

} // namespace exi
