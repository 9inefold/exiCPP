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
/// This file implements the BitBuffer class. It is used for copying data across
/// stream types.
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

namespace H {

class BitBufferBase {
protected:
  u32 Offset = 0;
  u32 BitOffset = 0;

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
  /// Get the byte offset from a global bit offset.
  EXI_INLINE static constexpr usize GetBytes64(usize Bits) {
    return Bits >> 6;
  }
  /// Get the local bit offset from a global bit offset.
  EXI_INLINE static constexpr usize GetBits64(usize Bits) {
    return Bits & usize(0b111'111);
  }

  constexpr BitBufferBase() = default;
  constexpr BitBufferBase(usize Bytes, usize Bits) :
   Offset(Cast(Bytes)), BitOffset(u32(Bits)) {
    if EXI_NEVER(Bits >= 8)
      Throw<argument_error>("Bit offset >= 8!");
  }

public:
  usize bytes() const { return Offset; }
  usize bits() const { return BitOffset; }
  usize total_bits() const { return Offset * 8 + BitOffset; }

  bool aligned() const { return bits() == 0; }
  usize aligned_bytes() const {
    return aligned() ? Offset : Offset + 1;
  }
};

template <class Derived, typename BufferT>
class BitBufferTemplate : public BitBufferBase {
protected:
  BufferT Data;

  constexpr BitBufferTemplate(BufferT Data, usize Bytes, usize Bits) :
   BitBufferBase(Bytes, Bits), Data(std::move(Data)) {}

public:
  explicit constexpr BitBufferTemplate(BufferT Data) : 
   BitBufferBase(), Data(std::move(Data)) {}

  static Derived FromBits(BufferT Data, usize Bits) {
    return Derived(std::move(Data), GetBytes(Bits), GetBits(Bits));
  }
  static Derived FromBytes(BufferT Data, usize Bytes) {
    return Derived(std::move(Data), Bytes, 0);
  }
  static Derived FromBytesAndBits(BufferT Data, usize Bytes, usize Bits) {
    if (Bits >= 8)
      // Provide a fixup.
      return FromBits(std::move(Data), (Bytes * 8) + Bits);
    return Derived(std::move(Data), Bytes, Bits);
  }
};

} // namespace H

class OwningBitBuffer;

/// A readable buffer with bit-level positioning.
class BitBuffer : public H::BitBufferTemplate<BitBuffer, ArrayRef<u8>> {
  friend class BitBufferTemplate;
  using buffer_t = ArrayRef<u8>;
public:
  using BitBufferTemplate::BitBufferTemplate;
  inline BitBuffer(const OwningBitBuffer& Buf);
  /// Returns the entire buffer.
  buffer_t arr() const { return BitBufferTemplate::Data; }
  /// Returns the slice of the buffer with fully written bytes. 
  buffer_t full() const { return arr().take_front(Offset); }
  /// Returns the partially written byte (or zero).
  u8 partial() const { return this->aligned() ? 0 : arr()[Offset]; }
};

/// A readable owning buffer with bit-level positioning.
class OwningBitBuffer
    : public H::BitBufferTemplate<OwningBitBuffer, OwningArrayRef<u8>> {
  friend class BitBufferTemplate;
  friend class BitBuffer;
  using buffer_t = OwningArrayRef<u8>;
public:
  using BitBufferTemplate::BitBufferTemplate;
  /// Returns the entire buffer.
  ArrayRef<u8> arr() const { return BitBufferTemplate::Data; }
  /// Returns the slice of the buffer with fully written bytes.
  ArrayRef<u8> full() const { return arr().take_front(Offset); }
  /// Returns the partially written byte (or zero).
  u8 partial() const { return this->aligned() ? 0 : arr()[Offset]; }
};

BitBuffer::BitBuffer(const OwningBitBuffer& Buf) : 
 BitBuffer(Buf.Data, Buf.Offset, Buf.BitOffset) {
}

} // namespace exi
