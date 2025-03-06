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

#include <core/Support/ErrorHandle.hpp>
#include <exi/Basic/NBitInt.hpp>
#include <exi/Stream/Stream.hpp>

namespace exi {
class APInt;
class MemoryBuffer;

template <typename T> class SmallVecImpl;

class BitStreamReader : public BitStreamCommon<ArrayRef<u8>> {
  // TODO: friend class ...
  using StreamT = ArrayRef<u8>;
  using BaseT = BitStreamCommon<StreamT>;
public:
  template <typename AnyT>
  BitStreamReader(BitConsumerProxy<AnyT> Other) : BaseT(Other) {}
  BitStreamReader(StreamT Stream) : BaseT(Stream) {}
private:
  /// Constructs a `BitStreamReader` from a `StrRef`.
  /// Used for constructing from a `MemoryBuffer`.
  BitStreamReader(StrRef Buffer);

public:
  /// Creates a `BitStreamReader` from an `ArrayRef`.
  static BitStreamReader New(ArrayRef<u8> Stream) { 
    return BitStreamReader(Stream);
  }
  /// Maybe creates a `BitStreamReader` from a `MemoryBuffer`.
  static BitStreamReader New(const MemoryBuffer& MB);
  /// Maybe creates a `BitStreamReader` from a `MemoryBuffer*`.
  static Option<BitStreamReader> New(const MemoryBuffer* MB);

private:
  using BaseT::getCurrentByte;

  u64 peekUnalignedBits() const {
    const u64 Pos = BaseT::bitOffset();
    const u64 Curr = getCurrentByte();
    return Curr & (0xFF >> Pos);
  }

  u64 readUnalignedBits() {
    const u64 Peeked = peekUnalignedBits();
    BaseT::skip(BaseT::farBitOffsetInclusive());
    return Peeked;
  }

  u64 peekBitsImpl(i64 Bits) const;
  u64 peekBitsSlow(i64 Bits) const EXI_READONLY;

  APInt readBitsAPLarge(i64 Bits);

  ExiError checkPeekBits(i64 Bits) const;
  ExiError checkReadBits(i64 Bits) const;

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
    }
    Out = peekBitsImpl(8);
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

  /// Reads a sequence of bytes.
  ExiError read(MutArrayRef<u8> Bytes, i64 Len = -1);
};

} // namespace exi
