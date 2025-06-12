//===- Support/Alignment.hpp ----------------------------------------===//
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
//
// This file contains types to represent alignments.
// They are instrumented to guarantee some invariants are preserved and prevent
// invalid manipulations.
//
// - Align represents an alignment in bytes, it is always set and always a valid
// power of two, its minimum value is 1 which means no alignment requirements.
//
// - MaybeAlign is an optional type, it may be undefined or set. When it's set
// you can get the underlying Align type by using the value() method.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Option.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/MathExtras.hpp>

namespace exi {

#define ALIGN_CHECK_ISPOSITIVE(decl) \
  exi_assert((decl > 0), (#decl " should be defined"))

/// This struct is a compact representation of a valid (non-zero power of two)
/// alignment.
/// It is suitable for use as static global constants.
struct Align {
private:
  u8 ShiftValue = 0; /// The log2 of the required alignment.
                     /// ShiftValue is less than 64 by construction.

  friend struct MaybeAlign;
  friend constexpr unsigned Log2(Align);
  friend constexpr bool operator==(Align Lhs, Align Rhs);
  friend constexpr bool operator!=(Align Lhs, Align Rhs);
  friend constexpr bool operator<=(Align Lhs, Align Rhs);
  friend constexpr bool operator>=(Align Lhs, Align Rhs);
  friend constexpr bool operator<(Align Lhs, Align Rhs);
  friend constexpr bool operator>(Align Lhs, Align Rhs);
  friend unsigned encodeAlign(struct MaybeAlign A);
  friend struct MaybeAlign decodeMaybeAlign(unsigned Value);

  struct LogValue { u8 ShiftValue; };
  constexpr Align(LogValue Val) : ShiftValue(Val.ShiftValue) {}

public:
  /// Default is byte-aligned.
  constexpr Align() = default;
  /// Do not perform checks in case of copy/move construct/assign, because the
  /// checks have been performed when building `Other`.
  constexpr Align(const Align &Other) = default;
  constexpr Align(Align &&Other) = default;
  constexpr Align &operator=(const Align &Other) = default;
  constexpr Align &operator=(Align &&Other) = default;

  explicit constexpr Align(u64 Value) {
    exi_assert(Value > 0, "Value must not be 0");
    exi_assert(exi::isPowerOf2_64(Value), "Alignment is not a power of 2");
    ShiftValue = Log2_64(Value);
    exi_assert(ShiftValue < 64, "Broken invariant");
  }

  /// This is a hole in the type system and should not be abused.
  /// Needed to interact with C for instance.
  constexpr u64 value() const { return u64(1) << ShiftValue; }

  // Returns the previous alignment.
  constexpr Align previous() const {
    exi_assert(ShiftValue != 0, "Undefined operation");
    Align Out;
    Out.ShiftValue = ShiftValue - 1;
    return Out;
  }

  /// Allow constructions of constexpr Align.
  template <usize kValue> constexpr static Align Constant() {
    return LogValue{CTLog2<kValue>()};
  }

  /// Allow constructions of constexpr Align from types.
  /// Compile time equivalent to Align(alignof(T)).
  template <typename T> constexpr static Align Of() {
    return Constant<std::alignment_of_v<T>>();
  }
};

inline consteval Align operator""_align(u64 Value) {
  return Align(Value);
}

/// Treats the value 0 as a 1, so Align is always at least 1.
inline Align assumeAligned(u64 Value) {
  return Value ? Align(Value) : Align();
}

/// This struct is a compact representation of a valid (power of two) or
/// undefined (0) alignment.
struct MaybeAlign : public Option<Align> {
private:
  using UP = Option<Align>;

public:
  /// Default is undefined.
  MaybeAlign() = default;
  /// Do not perform checks in case of copy/move construct/assign, because the
  /// checks have been performed when building `Other`.
  MaybeAlign(const MaybeAlign &Other) = default;
  MaybeAlign &operator=(const MaybeAlign &Other) = default;
  MaybeAlign(MaybeAlign &&Other) = default;
  MaybeAlign &operator=(MaybeAlign &&Other) = default;

  constexpr MaybeAlign(std::nullopt_t None) : UP(None) {}
  constexpr MaybeAlign(Align Value) : UP(Value) {}
  explicit MaybeAlign(u64 Value) {
    exi_assert((Value == 0 || exi::isPowerOf2_64(Value)),
           "Alignment is neither 0 nor a power of 2");
    if (Value)
      emplace(Value);
  }

  /// For convenience, returns a valid alignment or 1 if undefined.
  Align valueOrOne() const { return value_or(Align()); }
};

/// Checks that SizeInBytes is a multiple of the alignment.
inline constexpr bool isAligned(Align Lhs, u64 SizeInBytes) {
  return SizeInBytes % Lhs.value() == 0;
}

/// Checks that Addr is a multiple of the alignment.
inline bool isAddrAligned(Align Lhs, const void *Addr) {
  return isAligned(Lhs, reinterpret_cast<uintptr_t>(Addr));
}

/// Returns a multiple of A needed to store `Size` bytes.
inline constexpr u64 alignTo(u64 Size, Align A) {
  const u64 Value = A.value();
  // The following line is equivalent to `(Size + Value - 1) / Value * Value`.

  // The division followed by a multiplication can be thought of as a right
  // shift followed by a left shift which zeros out the extra bits produced in
  // the bump; `~(Value - 1)` is a mask where all those bits being zeroed out
  // are just zero.

  // Most compilers can generate this code but the pattern may be missed when
  // multiple functions gets inlined.
  return (Size + Value - 1) & ~(Value - 1U);
}

/// If non-zero \p Skew is specified, the return value will be a minimal integer
/// that is greater than or equal to \p Size and equal to \p A * N + \p Skew for
/// some integer N. If \p Skew is larger than \p A, its value is adjusted to '\p
/// Skew mod \p A'.
///
/// Examples:
/// \code
///   alignTo(5, Align(8), 7) = 7
///   alignTo(17, Align(8), 1) = 17
///   alignTo(~0LL, Align(8), 3) = 3
/// \endcode
inline constexpr u64 alignTo(u64 Size, Align A, u64 Skew) {
  const u64 Value = A.value();
  Skew %= Value;
  return alignTo(Size - Skew, A) + Skew;
}

/// Aligns `Addr` to `Alignment` bytes, rounding up.
inline uintptr_t alignAddr(const void *Addr, Align Alignment) {
  const uintptr_t ArithAddr = reinterpret_cast<uintptr_t>(Addr);
  exi_assert(uintptr_t(ArithAddr + Alignment.value() - 1) >= ArithAddr,
         "Overflow");
  return alignTo(ArithAddr, Alignment);
}

/// Returns the offset to the next integer (mod 2**64) that is greater than
/// or equal to \p Value and is a multiple of \p Align.
inline constexpr u64 offsetToAlignment(u64 Value, Align Alignment) {
  return alignTo(Value, Alignment) - Value;
}

/// Returns the necessary adjustment for aligning `Addr` to `Alignment`
/// bytes, rounding up.
inline u64 offsetToAlignedAddr(const void *Addr, Align Alignment) {
  return offsetToAlignment(reinterpret_cast<uintptr_t>(Addr), Alignment);
}

/// Returns the log2 of the alignment.
inline constexpr unsigned Log2(Align A) { return A.ShiftValue; }

/// Returns the alignment that satisfies both alignments.
/// Same semantic as MinAlign.
inline Align commonAlignment(Align A, u64 Offset) {
  return Align(MinAlign(A.value(), Offset));
}

/// Returns a representation of the alignment that encodes undefined as 0.
inline unsigned encodeAlign(MaybeAlign A) { return A ? A->ShiftValue + 1 : 0; }

/// Dual operation of the encode function above.
inline MaybeAlign decodeMaybeAlign(unsigned Value) {
  if (Value == 0)
    return MaybeAlign();
  Align Out;
  Out.ShiftValue = Value - 1;
  return Out;
}

/// Returns a representation of the alignment, the encoded value is positive by
/// definition.
inline unsigned encodeAlign(Align A) { return encodeAlign(MaybeAlign(A)); }

/// Comparisons between Align and scalars. Rhs must be positive.
inline constexpr bool operator==(Align Lhs, u64 Rhs) {
  ALIGN_CHECK_ISPOSITIVE(Rhs);
  return Lhs.value() == Rhs;
}
inline constexpr bool operator!=(Align Lhs, u64 Rhs) {
  ALIGN_CHECK_ISPOSITIVE(Rhs);
  return Lhs.value() != Rhs;
}
inline constexpr bool operator<=(Align Lhs, u64 Rhs) {
  ALIGN_CHECK_ISPOSITIVE(Rhs);
  return Lhs.value() <= Rhs;
}
inline constexpr bool operator>=(Align Lhs, u64 Rhs) {
  ALIGN_CHECK_ISPOSITIVE(Rhs);
  return Lhs.value() >= Rhs;
}
inline constexpr bool operator<(Align Lhs, u64 Rhs) {
  ALIGN_CHECK_ISPOSITIVE(Rhs);
  return Lhs.value() < Rhs;
}
inline constexpr bool operator>(Align Lhs, u64 Rhs) {
  ALIGN_CHECK_ISPOSITIVE(Rhs);
  return Lhs.value() > Rhs;
}

/// Comparisons operators between Align.
inline constexpr bool operator==(Align Lhs, Align Rhs) {
  return Lhs.ShiftValue == Rhs.ShiftValue;
}
inline constexpr bool operator!=(Align Lhs, Align Rhs) {
  return Lhs.ShiftValue != Rhs.ShiftValue;
}
inline constexpr bool operator<=(Align Lhs, Align Rhs) {
  return Lhs.ShiftValue <= Rhs.ShiftValue;
}
inline constexpr bool operator>=(Align Lhs, Align Rhs) {
  return Lhs.ShiftValue >= Rhs.ShiftValue;
}
inline constexpr bool operator<(Align Lhs, Align Rhs) {
  return Lhs.ShiftValue < Rhs.ShiftValue;
}
inline constexpr bool operator>(Align Lhs, Align Rhs) {
  return Lhs.ShiftValue > Rhs.ShiftValue;
}

// Don't allow relational comparisons with MaybeAlign.
bool operator<=(Align Lhs, MaybeAlign Rhs) = delete;
bool operator>=(Align Lhs, MaybeAlign Rhs) = delete;
bool operator<(Align Lhs, MaybeAlign Rhs) = delete;
bool operator>(Align Lhs, MaybeAlign Rhs) = delete;

bool operator<=(MaybeAlign Lhs, Align Rhs) = delete;
bool operator>=(MaybeAlign Lhs, Align Rhs) = delete;
bool operator<(MaybeAlign Lhs, Align Rhs) = delete;
bool operator>(MaybeAlign Lhs, Align Rhs) = delete;

bool operator<=(MaybeAlign Lhs, MaybeAlign Rhs) = delete;
bool operator>=(MaybeAlign Lhs, MaybeAlign Rhs) = delete;
bool operator<(MaybeAlign Lhs, MaybeAlign Rhs) = delete;
bool operator>(MaybeAlign Lhs, MaybeAlign Rhs) = delete;

// Allow equality comparisons between Align and MaybeAlign.
inline bool operator==(MaybeAlign Lhs, Align Rhs) { return Lhs && *Lhs == Rhs; }
inline bool operator!=(MaybeAlign Lhs, Align Rhs) { return !(Lhs == Rhs); }
inline bool operator==(Align Lhs, MaybeAlign Rhs) { return Rhs == Lhs; }
inline bool operator!=(Align Lhs, MaybeAlign Rhs) { return !(Rhs == Lhs); }
// Allow equality comparisons with MaybeAlign.
inline bool operator==(MaybeAlign Lhs, MaybeAlign Rhs) {
  return (Lhs && Rhs && (*Lhs == *Rhs)) || (!Lhs && !Rhs);
}
inline bool operator!=(MaybeAlign Lhs, MaybeAlign Rhs) { return !(Lhs == Rhs); }
// Allow equality comparisons with std::nullopt.
inline bool operator==(MaybeAlign Lhs, std::nullopt_t) { return !bool(Lhs); }
inline bool operator!=(MaybeAlign Lhs, std::nullopt_t) { return bool(Lhs); }
inline bool operator==(std::nullopt_t, MaybeAlign Rhs) { return !bool(Rhs); }
inline bool operator!=(std::nullopt_t, MaybeAlign Rhs) { return bool(Rhs); }

#undef ALIGN_CHECK_ISPOSITIVE

} // namespace exi
