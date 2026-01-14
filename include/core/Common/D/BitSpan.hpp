//===- Common/D/BitSpan.hpp -----------------------------------------===//
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

#pragma once

#include <Common/Fundamental.hpp>
#include <Support/MathExtras.hpp>

namespace exi {

/// The default word type for bit spans.
using BitSpanWordType = uptr;

/// Abstract BitSpan.
template <typename T = BitSpanWordType> class BitSpan;
/// Abstract MutBitSpan.
template <typename T = BitSpanWordType> class MutBitSpan;

/// Alias for `BitSpan<T>`.
template <typename T = BitSpanWordType> using bit_span = BitSpan<T>;
/// Alias for `MutBitSpan<T>`.
template <typename T = BitSpanWordType> using mut_bit_span = MutBitSpan<T>;

//////////////////////////////////////////////////////////////////////////
// Traits

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

} // namespace exi
