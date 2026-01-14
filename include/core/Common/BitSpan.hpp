//===- Common/BitSpan.hpp --------------------------------------------===//
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
/// This file implements the BitSpan class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ArrayRef.hpp>
#include <Common/Fundamental.hpp>
#include <Common/D/BitSpan.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/MathExtras.hpp>

namespace exi {

class BitVector;

/// Traits shared across `BitSpan`/`BitVector`/`bitset`.
// TODO: Move this kind of stuff to its own file?
template <typename BitWord = BitSpanWordType> struct BitSpanTraits {
  static constexpr usize kBitwordSize = bitsizeof_v<BitWord>;
  static constexpr BitWord kIdxSize = Log2_64(u64(kBitwordSize - 1)) + 1;
  static constexpr BitWord kOffMask = ~BitWord(~BitWord(0) << kIdxSize);

  ALWAYS_INLINE static constexpr usize Idx(usize I) {
    return I >> kIdxSize;
  }
  ALWAYS_INLINE static constexpr usize Off(usize I) {
    return I & kOffMask;
  }
  ALWAYS_INLINE static constexpr BitWord Mask(usize I) {
    return BitWord(1) << Off(I);
  }

  static constexpr unsigned NumBitWords(usize I) {
    return (I + kBitwordSize - 1) / kBitwordSize;
  }
};

namespace H {
/// Encapsulation of a single bit.
template <typename T> class bit_reference {
  using TraitsT = BitSpanTraits<T>;
  T* WordRef = nullptr;
  usize BitPos = 0;

public:
  template <class BufferT>
  constexpr bit_reference(BufferT& b EXI_LIFETIMEBOUND, usize I)
   : WordRef(&b[TraitsT::Idx(I)]),
     BitPos(TraitsT::Off(I)) {}
  
  constexpr bit_reference(MutArrayRef<T> b, usize I)
   : WordRef(&b[TraitsT::Idx(I)]),
     BitPos(TraitsT::Off(I)) {}

  bit_reference() = delete;
  bit_reference(const bit_reference&) = default;

  constexpr bit_reference& operator=(bit_reference O) {
    *this = bool(O);
    return *this;
  }

  constexpr bit_reference& operator=(bool V) {
    if (V)
      *WordRef |= T(1) << BitPos;
    else
      *WordRef &= ~(T(1) << BitPos);
    return *this;
  }

  constexpr operator bool() const {
    return (*WordRef & (T(1) << BitPos)) != 0;
  }
};

/// Encapsulation of a single bit.
template <typename T> class bit_reference<const T> {
  using TraitsT = BitSpanTraits<T>;
  const T* WordRef = nullptr;
  usize BitPos = 0;

public:
  template <class BufferT>
  constexpr bit_reference(const BufferT& b EXI_LIFETIMEBOUND, usize I)
   : WordRef(&b[TraitsT::Idx(I)]),
     BitPos(TraitsT::Off(I)) {}
  
  constexpr bit_reference(ArrayRef<T> b, usize I)
   : WordRef(&b[TraitsT::Idx(I)]),
     BitPos(TraitsT::Off(I)) {}

  bit_reference() = delete;
  bit_reference(const bit_reference&) = default;

  constexpr operator bool() const {
    return (*WordRef & (T(1) << BitPos)) != 0;
  }
};
} // namespace H

template <typename T> class BitSpan {
  COMPILE_FAILURE(BitSpan, "Currently unimplemented!")
};

template <typename T> class MutBitSpan {
  COMPILE_FAILURE(MutBitSpan, "Currently unimplemented!")
};

/// BitSpan - Represents a constant reference to a sequence of bits. It allows
/// various APIs to take and modify consecutive elements easily and
/// conveniently.
///
/// This class does not own the underlying data, it is expected to be used in
/// situations where the data resides in some other buffer, whose lifetime
/// extends past that of the BitSpan. For this reason, it is not in general
/// safe to store a BitSpan.
///
/// This is intended to be trivially copyable, so it should be passed by
/// value.
template <> class BitSpan<BitSpanWordType> {
  using T = BitSpanWordType;
  friend class BitVector;

protected:
  using BitWord = T;
  using Tr = BitSpanTraits<BitWord>;
  static constexpr unsigned kBitwordSize = bitsizeof_v<BitWord>; 

  const BitWord* Bits = nullptr; // Actual bits.
  unsigned Words = 0; // Size in words.
  unsigned Size = 0; // Size in bits.

public:
  using type = BitWord;
  using size_type = unsigned;
  using reference = H::bit_reference<const BitWord>;
  static constexpr size_type npos = ~size_type(0) - size_type(2);

  /// Constructs empty BitSpan.
  constexpr BitSpan() = default;
  /// Constructs a new BitSpan from a BitVector.
  BitSpan(const BitVector& EXI_LIFETIMEBOUND);
  /// Constructs a new BitSpan from a BitVector.
  ALWAYS_INLINE BitSpan(BitVector& BV EXI_LIFETIMEBOUND)
   : BitSpan(const_cast<const BitVector&>(BV)) {}

protected:
  /// Constructs new BitSpan from Bits.
  constexpr BitSpan(const BitWord* Bits, unsigned SizeInBits)
   : Bits(Bits), Words(Tr::NumBitWords(SizeInBits)), Size(SizeInBits) {}

public:
  /// size - Returns the number of bits in this BitSpan.
  constexpr size_type size() const { return Size; }
  /// data - Returns the underlying data of this BitSpan.
  constexpr const BitWord* data() const { return Bits; }

  ALWAYS_INLINE constexpr reference operator[](size_type I) const {
    exi_invariant(I < Size, "Invalid offset!");
    return reference(arr(), I);
  }

  /// test - Tests the value of a specific bit.
  constexpr bool test(size_type I) const {
    exi_invariant(I < Size, "Invalid offset!");
    return (Bits[Tr::Idx(I)] & Tr::Mask(I)) != 0;
  }

  /// count - Returns the number of bits which are set.
  size_type count() const {
    size_type NumBits = 0;
    for (auto Bit : arr())
      NumBits += exi::popcount(Bit);
    return NumBits;
  }

  /// any - Returns true if any bit is set.
  constexpr bool any() const {
    for (const BitWord V : arr())
      if (V != 0)
        return true;
    return false;
  }

  /// all - Returns true if all bits are set.
  constexpr bool all() const {
    for (usize I = 0; I < (Size / kBitwordSize); ++I)
      if (Bits[I] != ~BitWord(0))
        return false;

    // If bits remain check that they are ones.
    // The unused bits should always be zero.
    if (unsigned Remainder = Size % kBitwordSize) {
      const BitWord RemainderMask = (BitWord(1) << Remainder) - 1ull;
      exi_invariant((Bits[Size / kBitwordSize] & ~RemainderMask) == 0);
      return Bits[Size / kBitwordSize] == RemainderMask;
    }

    return true;
  }

  /// none - Returns true if none of the bits are set.
  ALWAYS_INLINE constexpr bool none() const {
    return !any();
  }

  ALWAYS_INLINE constexpr ArrayRef<BitWord> arr() const {
    return ArrayRef(Bits, Words);
  }

  BitVector to_bitvector() const;
};

/// MutBitSpan - Represents a mutable reference to a sequence of bits.
/// It allows various APIs to take and modify consecutive elements easily and
/// conveniently.
///
/// This class does not own the underlying data, it is expected to be used in
/// situations where the data resides in some other buffer, whose lifetime
/// extends past that of the MutBitSpan. For this reason, it is not in
/// general safe to store a MutBitSpan.
///
/// This is intended to be trivially copyable, so it should be passed by
/// value.
template <> class MutBitSpan<BitSpanWordType>
    : public BitSpan<BitSpanWordType> {
  using T = BitSpanWordType;
  friend class BitVector;

  using Super = BitSpan<BitSpanWordType>;
  using Super::BitWord;
  using Super::Tr;
  using Super::kBitwordSize;

public:
  using Super::type;
  using Super::size_type;
  using reference = H::bit_reference<BitWord>;

  /// Constructs an empty MutBitSpan.
  constexpr MutBitSpan() = default;
  /// Constructs a new MutBitSpan from a BitVector.
  MutBitSpan(BitVector& EXI_LIFETIMEBOUND);

protected:
  /// Constructs new BitSpan from Bits.
  ALWAYS_INLINE constexpr MutBitSpan(BitWord* Bits, unsigned SizeInBits)
   : Super(Bits, SizeInBits) {}

public:
  using Super::size;
  /// data - Returns the underlying data of this BitSpan.
  ALWAYS_INLINE illegal_constexpr BitWord* data() const {
    return FOLD_CXPR(const_cast<BitWord*>(Super::Bits));
  }

  ALWAYS_INLINE reference operator[](size_type I) const {
    exi_invariant(I < Super::Size, "Invalid offset!");
    return reference(arr(), I);
  }

  // Set, reset, flip
  MutBitSpan& set() {
    initWords(true);
    clearUnusedBits();
    return *this;
  }

  MutBitSpan& set(unsigned I) {
    exi_invariant(I < size(), "Invalid offset!");
    data()[Tr::Idx(I)] |= Tr::Mask(I);
    return *this;
  }

  /// set - Efficiently set a range of bits in [I, E)
  MutBitSpan& set(unsigned I, unsigned E) {
    exi_assert(I <= E, "Attempted to set backwards range!");
    exi_assert(E <= size(), "Attempted to set out-of-bounds range!");

    if (I == E) return *this;

    if (Tr::Idx(I) == Tr::Idx(E)) {
      BitWord EMask = Tr::Mask(E);
      BitWord IMask = Tr::Mask(I);
      BitWord Mask = EMask - IMask;
      data()[Tr::Idx(I)] |= Mask;
      return *this;
    }

    BitWord PrefixMask = ~BitWord(0) << Tr::Off(I);
    data()[Tr::Idx(I)] |= PrefixMask;
    I = alignTo(I, kBitwordSize);

    for (; I + kBitwordSize <= E; I += kBitwordSize)
      data()[Tr::Idx(I)] = ~BitWord(0);

    BitWord PostfixMask = Tr::Mask(E) - 1;
    if (I < E)
      data()[Tr::Idx(I)] |= PostfixMask;

    return *this;
  }

  MutBitSpan& reset() {
    initWords(false);
    return *this;
  }

  MutBitSpan& reset(unsigned I) {
    exi_invariant(I < size(), "Invalid offset!");
    data()[Tr::Idx(I)] &= ~Tr::Mask(I);
    return *this;
  }

  /// reset - Efficiently reset a range of bits in [I, E)
  MutBitSpan& reset(unsigned I, unsigned E) {
    exi_assert(I <= E, "Attempted to reset backwards range!");
    exi_assert(E <= size(), "Attempted to reset out-of-bounds range!");

    if (I == E) return *this;

    if (Tr::Idx(I) == Tr::Idx(E)) {
      BitWord EMask = Tr::Mask(E);
      BitWord IMask = Tr::Mask(I);
      BitWord Mask = EMask - IMask;
      data()[Tr::Idx(I)] &= ~Mask;
      return *this;
    }

    BitWord PrefixMask = ~BitWord(0) << Tr::Off(I);
    data()[Tr::Idx(I)] &= ~PrefixMask;
    I = alignTo(I, kBitwordSize);

    for (; I + kBitwordSize <= E; I += kBitwordSize)
      data()[Tr::Idx(I)] = BitWord(0);

    BitWord PostfixMask = Tr::Mask(E) - 1;
    if (I < E)
      data()[Tr::Idx(I)] &= ~PostfixMask;

    return *this;
  }

  MutBitSpan& flip() {
    for (auto& Bit : arr())
      Bit = ~Bit;
    clearUnusedBits();
    return *this;
  }

  MutBitSpan& flip(unsigned I) {
    exi_invariant(I < size(), "Invalid offset!");
    data()[Tr::Idx(I)] ^= Tr::Mask(I);
    return *this;
  }

  MutArrayRef<BitWord> arr() const {
    return MutArrayRef(data(), Words);
  }

private:
  /// Sets the unused bits to zero. Allows for cleaner bitwise operations.
  void clearUnusedBits() {
    if (unsigned ExtraBits = Size % kBitwordSize) {
      const BitWord ExtraBitMask = ~BitWord(0) << ExtraBits;
      data()[Super::Size / kBitwordSize] &= ~ExtraBitMask;
    }
  }

  void initWords(bool V) {
    std::fill(data(), data() + Super::Words, 0 - BitWord(V));
  }
};

} // namespace exi
