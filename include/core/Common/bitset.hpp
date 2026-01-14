//===- Common/bitset.hpp ---------------------------------------------===//
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
/// This file implements a more flexible version of std::bitset with backported
/// constexpr functionality.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/BitSpan.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/MathExtras.hpp>
#include <bitset>
//#include <iterator>
#include <type_traits>
//#include <utility>

namespace exi {

class BitVector;
class raw_ostream;

/// Reimplementation of `std::bitset` with extra functionality.
template <usize N> class bitset {
  using BitWord = BitSpanWordType;
  static constexpr usize kBitwordSize = bitsizeof_v<BitWord>;
  static_assert(kBitwordSize == 32 || kBitwordSize == 64,
                "Unsupported word size");

  static constexpr usize kBufferSize = ((N == 0) ? 0 : (N - 1) / kBitwordSize) + 1;
  static_assert(N != 0, "bitset cannot be empty (for now...)");

  // TODO: Move this kind of stuff to its own file?
  static constexpr BitWord kIdxSize = Log2_64(u64(kBitwordSize - 1)) + 1;
  static constexpr BitWord kOffMask = ~BitWord(~BitWord(0) << kIdxSize);

public:
  using size_type = usize;
  static constexpr size_type npos = ~size_type(0) - size_type(2);

  /// Encapsulation of all bits.
  struct buffer {
    BitWord X[kBufferSize] = {};
    ALWAYS_INLINE constexpr BitWord& operator[](usize I) {
      return X[I];
    }
    ALWAYS_INLINE constexpr const BitWord& operator[](usize I) const {
      return X[I];
    }
  };

  /// Encapsulation of a single bit.
  class reference {
    BitWord* WordRef = nullptr;
    size_type BitPos = 0;

  public:
    constexpr reference(buffer& b EXI_LIFETIMEBOUND, size_type I)
     : WordRef(&b[bitset::Idx(I)]),
       BitPos(bitset::Off(I)) {}

    reference() = delete;
    reference(const reference&) = default;

    constexpr reference& operator=(reference O) {
      *this = bool(O);
      return *this;
    }

    constexpr reference& operator=(bool V) {
      if (V)
        *WordRef |= BitWord(1) << BitPos;
      else
        *WordRef &= ~(BitWord(1) << BitPos);
      return *this;
    }

    constexpr operator bool() const {
      return (*WordRef & (BitWord(1) << BitPos)) != 0;
    }
  };

  buffer Bits = {};

  // TODO: Add ctor from std::bitset

  /// size - Returns the number of bits in this bitset.
  static constexpr usize size() { return N; }

  ALWAYS_INLINE constexpr reference operator[](size_type I) {
    exi_invariant(I < size(), "Invalid offset!");
    return reference(*this, I);
  }
  ALWAYS_INLINE constexpr bool operator[](size_type I) const {
    exi_invariant(I < size(), "Invalid offset!");
    return this->test(I);
  }

  /// test - Tests the value of a specific bit.
  constexpr bool test(size_type I) const {
    exi_invariant(I < size(), "Invalid offset!");
    return (Bits[Idx(I)] & Mask(I)) != 0;
  }

  /// clear - Sets all bits to `false`.
  ALWAYS_INLINE constexpr bitset& clear() {
    return this->reset();
  }

  /// count - Returns the number of bits which are set.
  size_type count() const {
    size_type NumBits = 0;
    for (auto Bit : Bits.X)
      NumBits += exi::popcount(Bit);
    return NumBits;
  }

  /// any - Returns true if any bit is set.
  constexpr bool any() const {
    for (const BitWord V : Bits.X)
      if (V != 0)
        return true;
    return false;
  }

  /// all - Returns true if all bits are set.
  constexpr bool all() const {
    for (usize I = 0; I < (N / kBitwordSize); ++I)
      if (Bits[I] != ~BitWord(0))
        return false;

    // If bits remain check that they are ones. The unused bits are always zero.
    constexpr unsigned Remainder = N % kBitwordSize;
    if constexpr (Remainder != 0)
      return Bits[N / kBitwordSize]
        == BitWord((BitWord(1) << Remainder) - 1ull);

    return true;
  }

  /// none - Returns true if none of the bits are set.
  ALWAYS_INLINE constexpr bool none() const {
    return !any();
  }

  constexpr bitset& operator&=(const bitset& O) {
    for (usize I = 0; I < kBufferSize; ++I)
      this->Bits[I] &= O.Bits[I];
    return *this;
  }
  constexpr bitset& operator|=(const bitset& O) {
    for (usize I = 0; I < kBufferSize; ++I)
      this->Bits[I] |= O.Bits[I];
    return *this;
  }
  constexpr bitset& operator^=(const bitset& O) {
    for (usize I = 0; I < kBufferSize; ++I)
      this->Bits[I] ^= O.Bits[I];
    return *this;
  }
  constexpr bitset operator~() const {
    bitset R = *this;
    return R.flip();
  }

  constexpr bitset operator&(const bitset& RHS) {
    bitset LHS = *this;
    LHS &= RHS;
    return LHS;
  }
  constexpr bitset operator|(const bitset& RHS) {
    bitset LHS = *this;
    LHS |= RHS;
    return LHS;
  }
  constexpr bitset operator^(const bitset& RHS) {
    bitset LHS = *this;
    LHS ^= RHS;
    return LHS;
  }

  /// set - Sets all bits to `true`.
  constexpr bitset& set() {
    for (BitWord& Val : Bits.X)
      Val = ~BitWord(0);
    this->clearUnusedBits();
    return *this;
  }

  /// set - Sets specific bit to `Val`.
  constexpr bitset& set(usize I, bool Val = true) {
    exi_invariant(I < size(), "Invalid offset!");
    if (Val)
      Bits[Idx(I)] |= Mask(I);
    else
      Bits[Idx(I)] &= ~Mask(I);
    return *this;
  }

  /// set - Efficiently set a range of bits in [I, E)
  constexpr bitset& set(unsigned I, unsigned E) {
    exi_assert(I <= E, "Attempted to set backwards range!");
    exi_assert(E <= size(), "Attempted to set out-of-bounds range!");

    if (I == E) return *this;

    if (Idx(I) == Idx(E)) {
      BitWord EMask = Mask(E);
      BitWord IMask = Mask(I);
      BitWord Mask = EMask - IMask;
      Bits[Idx(I)] |= Mask;
      return *this;
    }

    BitWord PrefixMask = ~BitWord(0) << Off(I);
    Bits[Idx(I)] |= PrefixMask;
    I = alignTo(I, kBitwordSize);

    for (; I + kBitwordSize <= E; I += kBitwordSize)
      Bits[Idx(I)] = ~BitWord(0);

    BitWord PostfixMask = Mask(E) - 1;
    if (I < E)
      Bits[Idx(I)] |= PostfixMask;

    return *this;
  }

  /// reset - Sets all bits to `false`.
  constexpr bitset& reset() {
    Bits = buffer{};
    return *this;
  }

  /// reset - Sets specific bit to `false`.
  constexpr bitset& reset(usize I) {
    exi_invariant(I < size(), "Invalid offset!");
    Bits[Idx(I)] &= ~Mask(I);
    return *this;
  }

  /// reset - Efficiently reset a range of bits in [I, E)
  constexpr bitset& reset(unsigned I, unsigned E) {
    exi_assert(I <= E, "Attempted to reset backwards range!");
    exi_assert(E <= N, "Attempted to reset out-of-bounds range!");

    if (I == E) return *this;

    if (Idx(I) == Idx(E)) {
      BitWord EMask = Mask(E);
      BitWord IMask = Mask(I);
      BitWord Mask = EMask - IMask;
      Bits[Idx(I)] &= ~Mask;
      return *this;
    }

    BitWord PrefixMask = ~BitWord(0) << Off(I);
    Bits[Idx(I)] &= ~PrefixMask;
    I = alignTo(I, kBitwordSize);

    for (; I + kBitwordSize <= E; I += kBitwordSize)
      Bits[Idx(I)] = BitWord(0);

    BitWord PostfixMask = Mask(E) - 1;
    if (I < E)
      Bits[Idx(I)] &= ~PostfixMask;

    return *this;
  }

  /// flip - Flips the value of all bits.
  constexpr bitset& flip() {
    for (BitWord& Val : Bits.X)
      Val = ~Val;
    this->clearUnusedBits();
    return *this;
  }

  /// flip - Flips the value of a specific bit.
  constexpr bitset& flip(usize I) {
    exi_invariant(I < size(), "Invalid offset!");
    Bits[Idx(I)] ^= Mask(I);
    return *this;
  }

  ////////////////////////////////////////////////////////////////////////
  // Conversion

  constexpr MutBitSpan<BitWord> to_bitspan() {
    return MutBitSpan<BitWord>(*this);
  }

  constexpr BitSpan<BitWord> to_bitspan() const {
    return BitSpan<BitWord>(*this);
  }

  constexpr unsigned long to_ulong() const
   requires(bitsizeof_v<unsigned long> >= N) {
    constexpr BitWord kMask = ~BitWord(0) >> bitsizeof_v<unsigned long>;
    return static_cast<unsigned long>(Bits[0] & kMask);
  }

  constexpr unsigned long to_ullong() const
   requires(bitsizeof_v<unsigned long long> >= N) {
    if constexpr (sizeof(BitWord) == sizeof(unsigned long long)) {
      return static_cast<unsigned long long>(Bits[0]);
    } else if constexpr (sizeof(BitWord) > sizeof(unsigned long long)) {
      const BitWord kMask = ~BitWord(0) >> bitsizeof_v<unsigned long long>;
      return static_cast<unsigned long long>(Bits[0] & kMask);
    } else {
      static_assert(sizeof(BitWord) * 2 == sizeof(unsigned long long));
      return static_cast<unsigned long long>(Bits[0])
           | static_cast<unsigned long long>(Bits[1]) << sizeof(BitWord);
    }
  }

  String to_string() const {
    return to_bitspan().to_string();
  }

private:
  ALWAYS_INLINE static constexpr usize Idx(usize I) {
    return (kBufferSize <= 1) ? 0 : I >> kIdxSize;
  }
  ALWAYS_INLINE static constexpr usize Off(usize I) {
    return I & kOffMask;
  }
  ALWAYS_INLINE static constexpr BitWord Mask(usize I) {
    return BitWord(1) << Off(I);
  }

  /// Sets the unused bits to zero. Allows for cleaner bitwise operations.
  ALWAYS_INLINE constexpr void clearUnusedBits() {
    constexpr unsigned Remainder = N % kBitwordSize;
    if constexpr (Remainder != 0)
      Bits[N / kBitwordSize] &= BitWord((BitWord(1) << Remainder) - 1ull);
  }
};

template <usize N>
inline raw_ostream& operator<<(raw_ostream& OS, const bitset<N>& B) {
  return OS << B.to_bitspan();
}

} // namespace exi
