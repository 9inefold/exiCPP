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
#include "exi/Basic/ErrorCodes.hpp"

#define EXI_HAS_BITSTREAMOUT 1

namespace exi {
class APInt;
class MemoryBuffer;
class StrRef;
class WritableMemoryBuffer;

template <typename T> class SmallVecImpl;

static_assert(kCHAR_BIT == 8,
  "Weird platform... if you need support open an issue.");

namespace H {

template <typename T>
struct RefOrValue {
  using type = std::remove_cv_t<T>;
};

template <typename T>
struct RefOrValue<const T&> {
  using type = std::remove_volatile_t<T>;
};

template <typename T>
struct RefOrValue<T*> {
  using type = typename RefOrValue<T&>::type;
};

} // namespace H

template <typename T>
using ref_or_value_t = typename H::RefOrValue<T>::type;

//======================================================================//
// BitStream*
//======================================================================//

/// The base for BitStream types. Provides common definitions.
struct StreamBase {
  using size_type = u64;
  using WordType  = u64;

  static constexpr size_type kWordSize = sizeof(WordType);
  static constexpr size_type kBitsPerWord = bitsizeof_v<WordType>;

  static constexpr size_type kMaxCapacityBytes = max_v<size_type> / kCHAR_BIT;
  static constexpr size_type kMask = (kCHAR_BIT - 1ull);
};

// TODO: Add options for little endian?

/// The interface for BitStream types. Provides a simple interface for reading
/// the current position in bits and bytes, and wraps a "stream" buffer.
template <class BufferT> class BitStreamCommon : public StreamBase {
  static_assert(sizeof(typename BufferT::value_type) == 1,
    "BufferT::value_type must be bytes!");
  static_assert(std::is_pointer_v<typename BufferT::iterator>,
    "BufferT::iterator must be a pointer!");
public:
  using value_type = typename BufferT::value_type;
  using pointer = typename BufferT::iterator;
  using const_pointer = typename BufferT::const_iterator;
  using ref = ref_or_value_t<pointer>;
  using const_ref = ref_or_value_t<const_pointer>;

protected:
  BufferT Stream;
  size_type Position = 0;

public:
  void skip(i64 Bits) {
    exi_assert(Bits >= 0);
    Position += Bits;
  }
  void skipBytes(i64 Bytes) {
    exi_assert(Bytes >= 0);
    Position += (Bytes * kCHAR_BIT);
  }

  /// The overall offset in bits.
  EXI_INLINE size_type bitPos() const { return Position; }
  /// The overall offset in bytes, clipped.
  size_type bytePos() const { return (Position / kCHAR_BIT); }
  /// The overall offset in bits, clipped.
  size_type byteBitPos() const { return (Position & ~kMask); }
  /// The offset from the start of the current byte in bits.
  size_type bitOffset() const { return (Position & kMask); }
  /// The offset from the next byte in bits.
  size_type farBitOffset() const { return kCHAR_BIT - bitOffset(); }
  /// The offset from the next unaligned byte in bits.
  size_type farBitOffsetInclusive() const { return (farBitOffset() & kMask); }

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
  bool isByteAligned() const { return bitOffset() == 0; }

  void setStream(BufferT NewStream) {
    this->Stream = NewStream;
    this->Position = 0;
  }

protected:
  BitStreamCommon(BufferT Stream) : Stream(Stream) {
    exi_assert(Stream.size() <= kMaxCapacityBytes,
      "Stream size exceeds max capacity.");
  }

  ExiError ec() const noexcept {
    if EXI_UNLIKELY(isFull())
      return ExiError::FULL;
    return ExiError::OK;
  }

  /// Aligns stream up to the next byte.
  /// @returns `false` if capacity is reached, `true` otherwise.
  template <bool CheckFull = false> bool align() {
    if (!isByteAligned())
      Position += (kCHAR_BIT - bitOffset());
    if constexpr (CheckFull)
      return EXI_LIKELY(notFull());
    return true;
  }

  /// Aligns stream down to the next byte.
  /// @returns `false` if capacity is reached, `true` otherwise.
  template <bool CheckFull = false> bool alignDown() {
    this->Position = byteBitPos();
    if constexpr (CheckFull)
      return EXI_LIKELY(notFull());
    return true;
  }

  pointer getCurrentBytePtr() {
    return &currentByteRef();
  }
  const_pointer getCurrentBytePtr() const {
    return &currentByteRef();
  }

  ref getCurrentByte() {
    return currentByteRef();
  }
  const_ref getCurrentByte() const {
    return currentByteRef();
  }

  /// Check if `N` bits can be read.
  inline bool canAccessBits(unsigned N) const {
    return EXI_LIKELY(Position + N <= capacity());
  }

  /// Check if `N` bytes can be read.
  inline bool canAccessBytes(unsigned N) const {
    return canAccessBits(N * kCHAR_BIT);
  }

  /// Check if `N` "words" can be read.
  bool canAccessWords(unsigned N = 1) const {
    const size_type Pos = (Position + 7) / kCHAR_BIT;
    const size_type Offset = Pos + (N * kWordSize);
    return Offset <= capacityInBytes();
  }

private:
  ALWAYS_INLINE decltype(auto) currentByteRef() {
    exi_invariant(!isFull());
    return Stream[bytePos()];
  }
  ALWAYS_INLINE decltype(auto) currentByteRef() const {
    exi_invariant(!isFull());
    return Stream[bytePos()];
  }
};

//======================================================================//
// BitStreamIn
//======================================================================//

class BitStreamIn : public BitStreamCommon<ArrayRef<u8>> {
  // TODO: friend class ...
public:
  using StreamType = ArrayRef<u8>;
  using BaseType = BitStreamCommon<StreamType>;
public:
  BitStreamIn(StreamType Stream) : BaseType(Stream) {}
private:
  /// Constructs a `InBitStream` from a `StrRef`.
  /// Used for constructing from a `MemoryBuffer`.
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

  ////////////////////////////////////////////////////////////////////////
  // Peeking

  /// Peeks a single bit.
  ExiError peekBit(bool& Out) const;

  /// Peeks a single bit.
  /// @attention This function ignores errors.
  bool peekBit() const {
    bool Out;
    (void) peekBit(Out);
    return Out;
  }

  /// Peeks a single byte.
  ExiError peekByte(u8& Out) const;

  /// Peeks a single byte.
  /// @attention This function ignores errors.
  u8 peekByte() const {
    u8 Out;
    (void) peekByte(Out);
    return Out;
  }

  /// Peeks a variable number of bits (max of 64).
  ExiError peekBits64(u64& Out, i64 Bits) const;

  /// Peeks a variable number of bits (max of 64).
  /// @attention This function ignores errors.
  u64 peekBits64(i64 Bits) const {
    u64 Out;
    (void) peekBits64(Out, Bits);
    return Out;
  }

  /// Peeks a variable number of bits.
  ExiError peekBits(APInt& AP, i64 Bits) const;

  /// Peeks a variable number of bits.
  /// @attention This function ignores errors.
  APInt peekBits(i64 Bits) const;

  /// Peeks a static number of bits (max of 64).
  template <unsigned Bits>
  ExiError peekBits(ubit<Bits>& Out) const {
    u64 ProxyOut;
    const ExiError Status
      = peekBits64(ProxyOut, Bits);
    Out = ubit<Bits>::FromBits(ProxyOut);
    return Status;
  }

  /// Peeks a static number of bits (max of 64).
  /// @attention This function ignores errors.
  template <unsigned Bits> ubit<Bits> peekBits() const {
    return ubit<Bits>::FromBits(peekBits64(Bits));
  }

  /// Peeks a sequence of bytes. This function has no returning variant.
  ExiError peek(MutArrayRef<u8> Out, i64 Bytes = -1) const {
    BitStreamIn thiz(*this);
    return thiz.read(Out, Bytes);
  }

  ////////////////////////////////////////////////////////////////////////
  // Reading

  /// Peeks a single bit.
  ExiError readBit(bool& Out);

  /// Peeks a single bit.
  /// @attention This function ignores errors.
  bool readBit() {
    const bool Result = peekBit();
    BaseType::skip(1);
    return Result;
  }

  /// Reads a single byte.
  ExiError readByte(u8& Out) {
    const ExiError Status = peekByte(Out);
    BaseType::skip(8);
    return Status;
  }

  /// Reads a single byte.
  /// @attention This function ignores errors.
  u8 readByte() {
    const u8 Result = peekByte();
    BaseType::skip(8);
    return Result;
  }

  /// Reads a variable number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  ExiError readBits64(u64& Out, i64 Bits);

  /// Reads a variable number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  /// @attention This function ignores errors.
  u64 readBits64(i64 Bits) {
    const u64 Result = peekBits64(Bits);
    BaseType::skip(Bits);
    return Result;
  }

  /// Reads a variable number of bits.
  /// This means data is peeked, then the position is advanced.
  ExiError readBits(APInt& AP, i64 Bits);

  /// Reads a variable number of bits.
  /// This means data is peeked, then the position is advanced.
  /// @attention This function ignores errors.
  APInt readBits(i64 Bits);

  /// Reads a static number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  template <unsigned Bits>
  ExiError readBits(ubit<Bits>& Out) {
    u64 ProxyOut;
    const ExiError Status
      = readBits64(ProxyOut, Bits);
    Out = ubit<Bits>::FromBits(ProxyOut);
    return Status;
  }

  /// Reads a static number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  /// @attention This function ignores errors.
  template <unsigned Bits> ubit<Bits> readBits() {
    return ubit<Bits>::FromBits(readBits64(Bits));
  }

  /// Reads a sequence of bytes.
  ExiError read(MutArrayRef<u8> Bytes, i64 Len = -1);

private:
  using BaseType::getCurrentByte;

  u64 peekUnalignedBits() const;
  u64 peekBitsImpl(i64 Bits) const;
  u64 peekBitsSlow(i64 Bits) const EXI_READONLY;

  u64 readUnalignedBits();
  APInt readBitsAPLarge(i64 Bits);
};

//======================================================================//
// BitStreamOut
//======================================================================//

// TODO: BitStreamOut
class BitStreamOut : public BitStreamCommon<MutArrayRef<u8>> {
  // TODO: friend class ...
public:
  using StreamType = MutArrayRef<u8>;
  using BaseType = BitStreamCommon<StreamType>;
public:
  BitStreamOut(StreamType Stream) : BaseType(Stream) {}
private:
  /// Constructs a `InBitStream` from a `MutArrayRef<char>`.
  /// Used for constructing from a `WritableMemoryBuffer`.
  BitStreamOut(MutArrayRef<char> Buffer);
public:
  /// Creates a `BitStreamIn` from an `ArrayRef`.
  static BitStreamOut New(MutArrayRef<u8> Stream) { 
    return BitStreamOut(Stream);
  }
  /// Maybe creates a `BitStreamIn` from a `MemoryBuffer`.
  static BitStreamOut New(WritableMemoryBuffer& MB);
  /// Maybe creates a `BitStreamIn` from a `MemoryBuffer*`.
  static Option<BitStreamOut> New(WritableMemoryBuffer* MB);

  ////////////////////////////////////////////////////////////////////////
  // Writing

  /// Writes a single bit.
  ExiError writeBit(safe_bool Value);
  /// Writes a single byte.
  ExiError writeByte(u8 Byte);

  /// Writes a variable number of bits (max of 64).
  ExiError writeBits64(u64 Value, i64 Bits);

  /// Writes a variable number of bits.
  ExiError writeBits(const APInt& AP);
  /// Writes a variable number of bits (specified).
  ExiError writeBits(const APInt& AP, i64 Bits);

  /// Reads a static number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  template <unsigned Bits>
  ExiError writeBits(ubit<Bits> Value) {
    return writeBits64(Value.data(), Bits);
  }

  /// Writes an array of bytes with an optional length.
  ExiError write(ArrayRef<u8> In, i64 Bytes = -1);
  /// Gets all the written bytes from the buffer.
  MutArrayRef<u8> getWrittenBytes();

private:
  using BaseType::getCurrentByte;

  void writeSingleByte(u8 Byte, i64 Bits = 8);
  void writeBitsImpl(u64 Value, i64 Bits);
  void writeBitsSlow(u64 Value, i64 Bits);

  ExiError writeBitsAP(const APInt& AP, i64 Bits);
};

} // namespace exi
