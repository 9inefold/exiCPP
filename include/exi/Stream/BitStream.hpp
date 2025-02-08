//===- exi/Stream/BitStream.hpp -------------------------------------===//
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
/// This file defines the base for stream operations.
///
//===----------------------------------------------------------------===//

#pragma once

#include "core/Common/ArrayRef.hpp"
#include "core/Common/Option.hpp"
#include "core/Support/ErrorHandle.hpp"
#include "core/Support/Limits.hpp"
#include "exi/Basic/NBitInt.hpp"

namespace exi {
class APInt;
class MemoryBuffer;
class StrRef;
class WritableMemoryBuffer;

template <typename T> class SmallVecImpl;

static_assert(kCHAR_BIT == 8,
  "Weird platform... if you need support open an issue.");

//===----------------------------------------------------------------===//
//                             BitStreams
//===----------------------------------------------------------------===//

/// The base for BitStream types. Provides a simple interface for reading
/// the current position in bits and bytes, and wraps a "stream" buffer.
template <class BufferT> class BitStreamBase {
public:
  using size_type = u64;
  static constexpr usize kMaxCapacity = max_v<size_type> / kCHAR_BIT;
  static constexpr usize kMask = (kCHAR_BIT - 1ull);
protected:
  BufferT Stream;
  size_type Position = 0;

public:
  /// The overall offset in bits.
  EXI_INLINE size_type bitPos() const { return Position; }
  /// The overall offset in bytes, clipped.
  size_type bytePos() const { return (Position / kCHAR_BIT); }
  /// The overall offset in bits, clipped.
  size_type byteBitPos() const { return (Position & ~kMask); }
  /// The offset from the start of the current byte in bits.
  size_type nearByteOffset() const { return (Position & kMask); }
  /// The offset from the next byte in bits.
  size_type farByteOffset() const { return kCHAR_BIT - nearByteOffset(); }
  /// The offset from the next unaligned byte in bits.
  size_type farByteOffsetInclusive() const { return (farByteOffset() & kMask); }

  /// The capacity in bits.
  size_type capacity() const { return (Stream.size() * kCHAR_BIT); }
  size_type capacityInBytes() const { return Stream.size(); }

  /// The remaining capacity in bits.
  size_type space() const { return capacity() - bitPos(); }
  /// The remaining capacity in bytes.
  size_type spaceInBytes() const { return capacityInBytes() - bytePos(); }

  explicit operator bool() const { return this->isFull(); }
  /// Checks if the current position is past the capacity.
  EXI_INLINE bool isFull() const { return bytePos() >= capacityInBytes(); }
  /// Checks if the current position is NOT past the capacity.
  EXI_INLINE bool notFull() const { return bytePos() < capacityInBytes(); }
  /// Checks if the current position is byte aligned.
  bool isByteAligned() const { return nearByteOffset() == 0; }

protected:
  BitStreamBase(BufferT Stream) : Stream(Stream) {
    exi_assert(Stream.size() <= kMaxCapacity,
      "Stream size exceeds max capacity.");
  }

  void setStream(BufferT NewStream) {
    this->Stream = NewStream;
    this->Position = 0;
  }

  /// Aligns stream up to the next byte.
  /// @returns `false` if capacity is reached, `true` otherwise.
  bool align() {
    if (!isByteAligned())
      Position += (kCHAR_BIT - nearByteOffset());
    return EXI_LIKELY(notFull());
  }

  /// Aligns stream down to the next byte.
  /// @returns `false` if capacity is reached, `true` otherwise.
  bool alignDown() {
    this->Position = nearByteOffset();
    return EXI_LIKELY(notFull());
  }

  bool canReadNBits(unsigned N) const {
    return Position + N <= capacity();
  }
};

class BitStreamIn : public BitStreamBase<ArrayRef<u8>> {
  // TODO: friend class ...
public:
  using StreamType = ArrayRef<u8>;
  using BaseType = BitStreamBase<StreamType>;
public:
  BitStreamIn(StreamType Stream) : BaseType(Stream) {}
private:
  /// Constructs a `InBitStream` from a `StrRef`.
  BitStreamIn(StrRef Buffer);
public:
  /// Creates a `BitStreamIn` from an `ArrayRef`.
  static BitStreamIn New(ArrayRef<u8> Stream) { 
    return BitStreamIn(Stream);
  }
  /// Maybe creates a `BitStreamIn` from a `MemoryBuffer`.
  static BitStreamIn New(const MemoryBuffer& MB);
  /// Maybe creates a `BitStreamIn` from a `MemoryBuffer*`.
  static Option<BitStreamIn> New(const MemoryBuffer* MB);

  void skip(i64 Bits) { BaseType::Position += Bits; }

  ////////////////////////////////////////////////////////////////////////
  // Reading

  /// Peeks a single bit.
  bool peekBit() const;

  /// Peeks a variable number of bits (max of 64).
  u64 peekBits(i64 Bits) const;

  /// Peeks a variable number of bits.
  APInt peekBitsAP(i64 Bits) const;

  /// Peeks a static number of bits (max of 64).
  template <unsigned Bits> ubit<Bits> peekBits() const {
    return ubit<Bits>::FromBits(peekBits(Bits));
  }

  /// Peeks a single bit.
  bool readBit() {
    const bool Result = peekBit();
    this->skip(1);
    return Result;
  }

  /// Reads a variable number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  u64 readBits(i64 Bits);

  /// Reads a variable number of bits.
  /// This means data is peeked, then the position is advanced.
  APInt readBitsAP(i64 Bits);

  /// Reads a static number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  template <unsigned Bits> ubit<Bits> readBits() {
    return ubit<Bits>::FromBits(readBits(Bits));
  }

private:
  u8 getCurrentByte() const;
  u64 peekUnalignedBits() const;
  u64 peekBitsImpl(i64 Bits) const;

  u64 readUnalignedBits();
  APInt readBitsAPLarge(i64 Bits);
};

// TODO: OutBitStream
class OutBitStream;

} // namespace exi
