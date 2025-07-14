//===- exi/Basic/BitBuffer.hpp --------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file implements BitReadBuffer and BitWriteBuffer. They will be used for
/// copying data across stream types.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Support/IntCast.hpp>
#include <exi/Basic/Except.hpp>

static_assert(kCHAR_BIT == 8,
  "Weird platform... if you need support open an issue.");

namespace exi {

template <typename T> class SmallVecImpl;

using bit_word_t = u64;

class BitBufferBase {
protected:
  /// Check N can fit in a `u32`.
  static constexpr usize Check(usize N) {
    AssertIntCast<u32>(N);
    return N;
  }
  /// Checked cast into a `u32`.
  static constexpr u32 Cast(usize N) {
    return IntCast<u32>(N);
  }
  /// Get the byte offset from a global bit offset.
  EXI_INLINE static constexpr usize GetBytes(usize Bits) {
    return Bits >> 3;
  }
  /// Get the local bit offset from a global bit offset.
  EXI_INLINE static constexpr usize GetBits(usize Bits) {
    return Bits & usize(0b111);
  }
};

//===----------------------------------------------------------------===//
// BitReadBuffer
//===----------------------------------------------------------------===//

/// A readable buffer with bit-level positioning.
class EXI_EMPTY_BASES BitReadBuffer : public BitBufferBase {
  using buffer_t = ArrayRef<u8>;

  buffer_t Data;
  u32 Offset = 0;
  u32 BitOffset = 0;

  constexpr BitReadBuffer(buffer_t Data, usize Bytes, usize Bits) :
   Data(Data), Offset(Cast(Bytes)), BitOffset(u32(Bits)) {
    // FIXME: Do a bit fixup?
    exi_invariant(Bits < 8, "Illegal bit offset!");
  }

public:
  explicit constexpr BitReadBuffer(buffer_t Data) : Data(Data) {}

  static BitReadBuffer FromBits(buffer_t Data, usize Bits) {
    return BitReadBuffer(Data, GetBytes(Bits), GetBits(Bits));
  }
  static BitReadBuffer FromBytes(buffer_t Data, usize Bytes) {
    return BitReadBuffer(Data, Bytes, 0);
  }
  static BitReadBuffer FromBytesAndBits(
   buffer_t Data, usize Bytes, usize Bits) {
    if EXI_NEVER(Bits >= 8)
      Throw<argument_error>("Bit offset >= 8!");
    return BitReadBuffer(Data, Bytes, Bits);
  }

  buffer_t arr() const { return Data; }
  usize bytes() const { return Offset; }
  usize bits() const { return BitOffset; }
  
  bool aligned() const { return bits() == 0; }
  usize aligned_bytes() const {
    return aligned() ? Offset : Offset + 1;
  }
};

//===----------------------------------------------------------------===//
// BitWriteBuffer
//===----------------------------------------------------------------===//

/// A writable buffer with bit-level positioning.
class EXI_EMPTY_BASES BitWriteBuffer : public BitBufferBase {
  using buffer_t = SmallVecImpl<u8>;

  buffer_t* Data;
  u32 Offset = 0;
  u32 BitOffset = 0;

  BitWriteBuffer(buffer_t* Data, usize Bytes, usize Bits) :
   Data(Data), Offset(Cast(Bytes)), BitOffset(u32(Bits)) {
    // FIXME: Do a bit fixup?
    exi_invariant(Bits < 8, "Illegal bit offset!");
  }

public:
  explicit BitWriteBuffer(buffer_t& Data) : Data(&Data) {}

  static BitWriteBuffer FromBits(buffer_t& Data, usize Bits) {
    return BitWriteBuffer(&Data, GetBytes(Bits), GetBits(Bits));
  }
  static BitWriteBuffer FromBytes(buffer_t& Data, usize Bytes) {
    return BitWriteBuffer(&Data, Bytes, 0);
  }
  static BitWriteBuffer FromBytesAndBits(
   buffer_t& Data, usize Bytes, usize Bits) {
    if EXI_NEVER(Bits >= 8)
      Throw<argument_error>("Bit offset >= 8!");
    return BitWriteBuffer(&Data, Bytes, Bits);
  }

  buffer_t& arr() const { return *Data; }
  usize bytes() const { return Offset; }
  usize bits() const { return BitOffset; }

  bool aligned() const { return bits() == 0; }
  usize aligned_bytes() const {
    return aligned() ? Offset : Offset + 1;
  }
};

} // namespace exi
