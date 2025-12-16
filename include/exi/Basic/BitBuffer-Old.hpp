//===- exi/Basic/BitBuffer.hpp --------------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
  /// Get the byte offset from a global bit offset.
  EXI_INLINE static constexpr usize GetBytes64(usize Bits) {
    return Bits >> 6;
  }
  /// Get the local bit offset from a global bit offset.
  EXI_INLINE static constexpr usize GetBits64(usize Bits) {
    return Bits & usize(0b111'111);
  }
};

//===----------------------------------------------------------------===//
// BitReadBuffer
//===----------------------------------------------------------------===//

class EXI_EMPTY_BASES BitReadBufferBase : public BitBufferBase {
protected:
  u32 Offset = 0;
  u32 BitOffset = 0;

  constexpr BitReadBufferBase() = default;
  constexpr BitReadBufferBase(usize Bytes, usize Bits) :
   Offset(Cast(Bytes)), BitOffset(u32(Bits)) {
    if EXI_NEVER(Bits >= 8)
      Throw<argument_error>("Bit offset >= 8!");
  }

public:
  usize bytes() const { return Offset; }
  usize bits() const { return BitOffset; }

  bool aligned() const { return bits() == 0; }
  usize aligned_bytes() const {
    return aligned() ? Offset : Offset + 1;
  }
};

template <class Derived, typename BufferT>
class BitReadBufferTemplate : public BitReadBufferBase {
protected:
  BufferT Data;

  constexpr BitReadBufferTemplate(BufferT Data, usize Bytes, usize Bits) :
   BitReadBufferBase(Bytes, Bits), Data(std::move(Data)) {}

public:
  explicit constexpr BitReadBufferTemplate(BufferT Data) : 
   BitReadBufferBase(), Data(std::move(Data)) {}

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

class OwningBitReadBuffer;

/// A readable buffer with bit-level positioning.
class BitReadBuffer
    : public BitReadBufferTemplate<BitReadBuffer, ArrayRef<u8>> {
  friend class BitReadBufferTemplate;
  using buffer_t = ArrayRef<u8>;
public:
  using BitReadBufferTemplate::BitReadBufferTemplate;
  inline BitReadBuffer(const OwningBitReadBuffer& Buf);
  buffer_t arr() const { return BitReadBufferTemplate::Data; }
};

/// A readable owning buffer with bit-level positioning.
class OwningBitReadBuffer
    : public BitReadBufferTemplate<OwningBitReadBuffer, OwningArrayRef<u8>> {
  friend class BitReadBufferTemplate;
  friend class BitReadBuffer;
  using buffer_t = OwningArrayRef<u8>;
public:
  using BitReadBufferTemplate::BitReadBufferTemplate;
  ArrayRef<u8> arr() const { return BitReadBufferTemplate::Data; }
};

BitReadBuffer::BitReadBuffer(const OwningBitReadBuffer& Buf) : 
 BitReadBuffer(Buf.Data, Buf.Offset, Buf.BitOffset) {
}

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
    if EXI_NEVER(Bits >= 64)
      Throw<argument_error>("Bit offset >= 64!");
  }

public:
  explicit BitWriteBuffer(buffer_t& Data) : Data(&Data) {}

  static BitWriteBuffer FromBits(buffer_t& Data, usize Bits) {
    return BitWriteBuffer(&Data, GetBytes64(Bits), GetBits64(Bits));
  }
  static BitWriteBuffer FromBytes(buffer_t& Data, usize Bytes) {
    return BitWriteBuffer(&Data, Bytes, 0);
  }
  static BitWriteBuffer FromBytesAndBits(
   buffer_t& Data, usize Bytes, usize Bits) {
    return BitWriteBuffer(&Data, Bytes, Bits);
  }

  buffer_t& arr() const { return *Data; }
  usize bytes() const { return Offset; }
  usize bits() const { return BitOffset; }

  bool aligned() const { return bits() == 0; }
  usize aligned_bytes() const {
    return aligned() ? Offset : Offset + 8;
  }
};

} // namespace exi
