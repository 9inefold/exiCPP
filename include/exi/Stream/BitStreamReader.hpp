//===- exi/Stream/BitStreamReader.hpp -------------------------------===//
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
/// This file implements the BitStreamReader class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/StringExtras.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/MemoryBufferRef.hpp>
#include <exi/Basic/NBitInt.hpp>
#include <exi/Stream/Stream.hpp>

namespace exi {
class APInt;
class MemoryBuffer;

template <typename T> class SmallVecImpl;

class BitStreamReader : public BitStreamCommon<ArrayRef<u8>> {
  friend class ByteStreamReader;
  using StreamT = ArrayRef<u8>;
  using BaseT = BitStreamCommon<StreamT>;
public:
  template <typename AnyT>
  BitStreamReader(BitConsumerProxy<AnyT> Other) : BaseT(Other) {}
  /// Constructs a `BitStreamReader` from the real stream type.
  BitStreamReader(StreamT Stream) : BaseT(Stream) {}
  /// Constructs a `BitStreamReader` from a `MemoryBufferRef`.
  BitStreamReader(MemoryBufferRef Buffer) :
   BitStreamReader(Buffer.getBuffer()) {}

  virtual ~BitStreamReader() = default;

private:
  /// Constructs a `BitStreamReader` from a `StrRef`.
  /// Used for constructing from a `MemoryBuffer[Ref]`.
  BitStreamReader(StrRef Buffer) :
   BaseT(exi::arrayRefFromStringRef(Buffer)) {}

public:
  /// Creates a `BitStreamReader` from an `ArrayRef`.
  static BitStreamReader New(ArrayRef<u8> Stream) { 
    return BitStreamReader(Stream);
  }
  /// Maybe creates a `BitStreamReader` from a `MemoryBuffer`.
  static BitStreamReader New(const MemoryBuffer& MB EXI_LIFETIMEBOUND);
  /// Maybe creates a `BitStreamReader` from a `MemoryBuffer*`.
  static Option<BitStreamReader> New(const MemoryBuffer* MB);

private:
  using BaseT::getCurrentByte;

  u64 peekUnalignedBits() const {
    const u64 Pos = BaseT::bitOffset();
    const u64 Curr = BaseT::getCurrentByte();
    return Curr & (0xFF >> Pos);
  }

  u64 readUnalignedBits() {
    const u64 Peeked = peekUnalignedBits();
    BaseT::skip(BaseT::farBitOffsetInclusive());
    return Peeked;
  }

  /// Dispatch peek implementation.
  u64 peekBitsImpl(i64 Bits) const;
  /// Used for peeking byte-aligned streams.
  u64 peekBitsFast(i64 Bits) const;
  /// Used for peeking unaligned streams.
  u64 peekBitsSlow(i64 Bits) const;

  /// Peeks byte at offset in unaligned stream.
  u64 peekByteSlow(const i64 POff = 0) const {
    exi_assume(POff >= 0);
    exi_expensive_invariant(!BaseT::isByteAligned(),
      "If aligned, access bytes directly.");
    
    const u64 Pos = BaseT::bytePos() + POff;
    const auto Off = BaseT::farBitOffset();

    if EXI_UNLIKELY(Pos + 1 == BaseT::Stream.size()) {
      // If we happen to be at the end of the stream, just get the rest.
      // TODO: Add boolean template masking switch? May be better for users.
      return BaseT::Stream[Pos] << (8 - Off);
    }

    u64 Result = BaseT::Stream[Pos] << (8 - Off);
    Result |= BaseT::Stream[Pos + 1] >> Off;

    return Result;
  }

  APInt readBitsAPLarge(i64 Bits);

  ExiError checkPeekBits(i64 Bits) const;
  ExiError checkReadBits(i64 Bits) const;

  virtual void anchor();

public:
  using BaseT::align;
  bool atEndOfStream() const { return BaseT::isFull(); }

  ////////////////////////////////////////////////////////////////////////
  // Peeking

  /// Peeks a single bit.
  ExiError peekBit(bool& Out) const {
    if (auto Err = checkPeekBits(1)) [[unlikely]] {
      Out = false;
      return Err;
    }

    const u64 Pos = BaseT::farBitOffset() - 1;
    const u64 Curr = getCurrentByte();
    Out = static_cast<bool>((Curr >> Pos) & 0x1);
    return ExiError::OK;
  }

  /// Peeks a single bit.
  /// @attention This function ignores errors.
  bool peekBit() const {
    bool Out;
    (void) peekBit(Out);
    return Out;
  }

  /// Peeks a single byte.
  ExiError peekByte(u8& Out) const {
    if (auto Err = checkPeekBits(8)) [[unlikely]] {
      Out = 0;
      return Err;
    } else if (BaseT::isByteAligned()) {
      Out = getCurrentByte();
      return ExiError::OK;
    }

    Out = peekByteSlow(0);
    return ExiError::OK;
  }

  /// Peeks a single byte.
  /// @attention This function ignores errors.
  u8 peekByte() const {
    u8 Out;
    (void) peekByte(Out);
    return Out;
  }

  /// Peeks a variable number of bits (max of 64).
  ExiError peekBits64(u64& Out, i64 Bits) const {
    exi_invariant(Bits >= 0 && Bits <= 64, "Invalid bit size!");
    if EXI_UNLIKELY(Bits > 64)
      Bits = 64;

    if (auto Err = checkPeekBits(Bits)) [[unlikely]] {
      Out = 0;
      return Err;
    }

    Out = peekBitsImpl(Bits);
    return ExiError::OK;
  }

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
    BitStreamReader thiz(*this);
    return thiz.read(Out, Bytes);
  }

  ////////////////////////////////////////////////////////////////////////
  // Reading

  /// Reads a single bit.
  ExiError readBit(bool& Out) {
    const ExiError Status = peekBit(Out);
    BaseT::skip(1);
    return Status;
  }

  /// Reads a single bit.
  /// @attention This function ignores errors.
  bool readBit() {
    const bool Result = peekBit();
    BaseT::skip(1);
    return Result;
  }

  /// Reads a single byte.
  ExiError readByte(u8& Out) {
    const ExiError Status = peekByte(Out);
    BaseT::skip(8);
    return Status;
  }

  /// Reads a single byte.
  /// @attention This function ignores errors.
  u8 readByte() {
    const u8 Result = peekByte();
    BaseT::skip(8);
    return Result;
  }

  /// Reads a variable number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  ExiError readBits64(u64& Out, i64 Bits) {
    const ExiError Status = peekBits64(Out, Bits);
    BaseT::skip(Bits);
    return Status;
  }

  /// Reads a variable number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  /// @attention This function ignores errors.
  u64 readBits64(i64 Bits) {
    const u64 Result = peekBits64(Bits);
    BaseT::skip(Bits);
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

  /// Reads an `Unsigned Integer` with a maximum of 8 octets.
  /// See https://www.w3.org/TR/exi/#encodingUnsignedInteger.
  ExiError readUInt(u64& Out);

  /// Reads an `Unsigned Integer` with a maximum of 8 octets.
  /// See https://www.w3.org/TR/exi/#encodingUnsignedInteger.
  /// @attention This function ignores errors.
  u64 readUInt() {
    u64 Out;
    (void) readUInt(Out);
    return Out;
  }

  // TODO: readUInt(APInt&)

  /// Reads a sequence of bytes.
  ExiError read(MutArrayRef<u8> Bytes, i64 Len = -1);
};

class ByteStreamReader final : public BitStreamReader {
public:
  using BitStreamReader::BitStreamReader;
  ByteStreamReader(auto&&...Args) : BitStreamReader(EXI_FWD(Args)...) {
    exi_unreachable("ByteStreamReader is currently unimplemented!");
  }
};

} // namespace exi
