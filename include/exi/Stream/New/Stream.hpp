//===- exi/Stream/Stream.hpp ----------------------------------------===//
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

#include <core/Common/ArrayRef.hpp>
#include <core/Common/CRTPTraits.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Limits.hpp>
#include <exi/Basic/ErrorCodes.hpp>

namespace exi {

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

template <class BufferT> class BitStreamCommon;

class BitStreamReader;
class BitStreamWriter;

class ByteStreamReader;
class ByteStreamWriter;

/// The base for BitStream types. Provides common definitions.
struct StreamBase {
  using size_type = u64;
  using WordType  = u64;

  static constexpr size_type kWordSize = sizeof(WordType);
  static constexpr size_type kBitsPerWord = bitsizeof_v<WordType>;

  static constexpr size_type kMaxCapacityBytes = max_v<size_type> / kCHAR_BIT;
  static constexpr size_type kMask = (kCHAR_BIT - 1ull);
};

/// A proxy type for passing around consumed bits. Useful when swapping
/// between stream types (generally between the header and body).
template <class BufferT> class BitConsumerProxy {
  template <typename> friend class BitStreamCommon;
  BufferT Bytes;
  StreamBase::WordType Bits;
public:
  BitConsumerProxy(BufferT Bytes, u64 Bits) :
   Bytes(Bytes), Bits(Bits) {
  }
};

/// The interface for Stream types.
template <class Derived> class StreamCommon : public StreamBase {
	using BufferT = typename Derived::buffer_type;
  static_assert(sizeof(typename BufferT::value_type) == 1,
    "BufferT::value_type must be bytes!");
  static_assert(std::is_pointer_v<typename BufferT::iterator>,
    "BufferT::iterator must be a pointer!");
	
	EXI_CRTP_DEFINE_SUPER(Derived)
	EXI_CRTP_REQUIRE(Derived, Stream)
	EXI_CRTP_REQUIRE(Derived, Position)

public:
  using value_type = typename BufferT::value_type;
  using pointer = typename BufferT::iterator;
  using const_pointer = typename BufferT::const_iterator;
  using ref = ref_or_value_t<pointer>;
  using const_ref = ref_or_value_t<const_pointer>;

  void skip(i64 Bits) {
    exi_assert(Bits >= 0);
    super()->Position += Bits;
  }
  void skipBytes(i64 Bytes) {
    exi_assert(Bytes >= 0);
    super()->Position += (Bytes * kCHAR_BIT);
  }

  /// The overall offset in bits.
  EXI_INLINE size_type bitPos() const { return super()->Position; }
  /// The overall offset in bytes, clipped.
  size_type bytePos() const { return (bitPos() / kCHAR_BIT); }
  /// The overall offset in bits, clipped.
  size_type byteBitPos() const { return (bitPos() & ~kMask); }
  /// The offset from the start of the current byte in bits.
  size_type bitOffset() const { return (bitPos() & kMask); }
  /// The offset from the next byte in bits.
  size_type farBitOffset() const { return kCHAR_BIT - bitOffset(); }
  /// The offset from the next unaligned byte in bits.
  size_type farBitOffsetInclusive() const { return (farBitOffset() & kMask); }
  /// Checks if the current position is byte aligned.
  bool isByteAligned() const { return bitOffset() == 0; }

protected:
  constexpr StreamCommon() = default;
};

/// The interface for *StreamReader types. Provides a simple interface for
/// reading the current position in bits and bytes, and wraps a "stream" buffer.
class StreamReader : public StreamCommon<StreamReader> {
	friend class StreamCommon<StreamReader>;
	using Super = StreamCommon<StreamReader>;
	using buffer_type = ArrayRef<u8>;
	using proxy_type = BitConsumerProxy<BufferT>;

protected:
  buffer_type Stream;
  // StreamBase::WordType Store = 0;
  size_type BitCapacity = 0;
  size_type Position = 0;

public:
	using Super::bitPos;
	using Super::bytePos;
	using Super::byteBitPos;
	using Super::bitOffset;
	using Super::farBitOffset;
	using Super::farBitOffsetInclusive;
	using Super::isByteAligned;

  /// The capacity in bits.
  size_type capacity() const { return BitCapacity; }
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

  void setStream(buffer_type NewStream) {
    exi_assert(NewStream.size() <= kMaxCapacityBytes,
      "Stream size exceeds max capacity.");
    this->Stream = NewStream;
    this->BitCapacity = NewStream.size() * kCHAR_BIT;
    this->Position = 0;
  }

  proxy_type getProxy() const {
    return proxy_type(Stream, Position);
  }

  template <typename AnyT>
  void setProxy(BitConsumerProxy<AnyT> Other) {
    this->setStream(Other.Bytes);
    this->Position = Other.Bits;
  }

protected:
  BitStreamCommon(buffer_type Stream) :
   Stream(Stream), BitCapacity(Stream.size() * kCHAR_BIT) {
    exi_assert(Stream.size() <= kMaxCapacityBytes,
      "Stream size exceeds max capacity.");
  }

  template <typename AnyT>
  BitStreamCommon(BitConsumerProxy<AnyT> Other) :
   BitStreamCommon(Other.Bytes) {
    this->Position = Other.Bits;
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

  /// Aligns stream down to the current byte.
  /// @returns `false` if capacity was reached, `true` otherwise.
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
  inline bool canAccessBits(i64 N) const {
    if EXI_UNLIKELY(N < 0)
      return false;
    return EXI_LIKELY(Position + N <= capacity());
  }

  /// Check if `N` bytes can be read.
  inline bool canAccessBytes(i64 N) const {
    return canAccessBits(N * kCHAR_BIT);
  }

  /// Check if `N` "words" can be read.
  bool canAccessWords(i64 N = 1) const {
    if EXI_UNLIKELY(N < 0)
      return false;
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

/// The interface for *StreamWriter types. Provides a simple interface for
/// writing to a buffered stream in bits and bytes.
class StreamWriter : public StreamBase {
public:
  using ProxyT = BitConsumerProxy<BufferT>;
  using value_type = typename BufferT::value_type;
  using pointer = typename BufferT::iterator;
  using const_pointer = typename BufferT::const_iterator;
  using ref = ref_or_value_t<pointer>;
  using const_ref = ref_or_value_t<const_pointer>;

protected:
  BufferT Stream;
  // StreamBase::WordType Store = 0;
  size_type BitCapacity = 0;
  size_type Position = 0;

public:
  void skip(i64 Bits) {
    exi_assert(Bits >= 0);
    super()->Position += Bits;
  }
  void skipBytes(i64 Bytes) {
    exi_assert(Bytes >= 0);
    super()->Position += (Bytes * kCHAR_BIT);
  }

  /// The overall offset in bits.
  EXI_INLINE size_type bitPos() const { return super()->Position; }
  /// The overall offset in bytes, clipped.
  size_type bytePos() const { return (bitPos() / kCHAR_BIT); }
  /// The overall offset in bits, clipped.
  size_type byteBitPos() const { return (bitPos() & ~kMask); }
  /// The offset from the start of the current byte in bits.
  size_type bitOffset() const { return (bitPos() & kMask); }
  /// The offset from the next byte in bits.
  size_type farBitOffset() const { return kCHAR_BIT - bitOffset(); }
  /// The offset from the next unaligned byte in bits.
  size_type farBitOffsetInclusive() const { return (farBitOffset() & kMask); }

  /// The capacity in bits.
  size_type capacity() const { return BitCapacity; }
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
    exi_assert(NewStream.size() <= kMaxCapacityBytes,
      "Stream size exceeds max capacity.");
    this->Stream = NewStream;
    this->BitCapacity = NewStream.size() * kCHAR_BIT;
    this->Position = 0;
  }

  ProxyT getProxy() const {
    return ProxyT(Stream, Position);
  }

  template <typename AnyT>
  void setProxy(BitConsumerProxy<AnyT> Other) {
    this->setStream(Other.Bytes);
    this->Position = Other.Bits;
  }

protected:
  BitStreamCommon(BufferT Stream) :
   Stream(Stream), BitCapacity(Stream.size() * kCHAR_BIT) {
    exi_assert(Stream.size() <= kMaxCapacityBytes,
      "Stream size exceeds max capacity.");
  }

  template <typename AnyT>
  BitStreamCommon(BitConsumerProxy<AnyT> Other) :
   BitStreamCommon(Other.Bytes) {
    this->Position = Other.Bits;
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

  /// Aligns stream down to the current byte.
  /// @returns `false` if capacity was reached, `true` otherwise.
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
  inline bool canAccessBits(i64 N) const {
    if EXI_UNLIKELY(N < 0)
      return false;
    return EXI_LIKELY(Position + N <= capacity());
  }

  /// Check if `N` bytes can be read.
  inline bool canAccessBytes(i64 N) const {
    return canAccessBits(N * kCHAR_BIT);
  }

  /// Check if `N` "words" can be read.
  bool canAccessWords(i64 N = 1) const {
    if EXI_UNLIKELY(N < 0)
      return false;
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

} // namespace exi
