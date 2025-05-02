//===- exi/Stream/OrderedReader.hpp ---------------------------------===//
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
/// This file defines the in-order readers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Support/Logging.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Stream2/Reader.hpp>
#if EXI_LOGGING
# include <fmt/ranges.h>
#endif

#define METHOD_BAG(CLASSNAME) public OrdReaderMethods<CLASSNAME>

namespace exi {

template <typename Derived>
struct EXI_EMPTY_BASES OrdReaderMethods : public ReaderMethods<Derived> {
  EXI_USE_READER_METHODS(Derived, public)
  void TempFunctionA() {}
  void TempFunctionB() {}
private:
  EXI_CRTP_DEFINE_SUPER(Derived)
};

#define EXI_USE_ORDREADER_METHODS(CLASSNAME, VISIBILITY)                      \
  EXI_USE_ORDREADER_METHODS_I(OrdReaderMethods<CLASSNAME>)                    \
VISIBILITY:

#define EXI_USE_ORDREADER_METHODS_I(IMPL)                                     \
  EXI_USE_READER_METHODS_I(IMPL)                                              \
public:                                                                       \
protected:                                                                    \
  using IMPL::TempFunctionA;                                                  \
  using IMPL::TempFunctionB; // TODO: Temp, remove when done.

#define DEBUG_TYPE "ByteReader"

/// The bases for BitReader/ByteReader, which consume data in the order it
/// appears. This allows for a much simpler implementation.
class OrderedReader : public ReaderBase, METHOD_BAG(OrderedReader) {
protected:
  /// The current stream data.
  buffer_t Stream;
  /// The offset of the current stream in bytes.
  size_type ByteOffset = 0;
  /// The current word, cached data from the stream.
  word_t Store = 0;

  static_assert(kBitsPerWord >= 64, "Work on this...");

  /// Creates a mask for the current word type.
  inline static constexpr word_t MakeNBitMask(unsigned Bits) EXI_READNONE {
    return (~word_t(0) >> (kBitsPerWord - Bits));
  }

public:
  OrderedReader() = default;
  explicit OrderedReader(buffer_t Stream) : Stream(Stream) {}
  explicit OrderedReader(StrRef Buf) : Stream(arrayRefFromStringRef(Buf)) {}
  explicit OrderedReader(MemoryBufferRef MB) : OrderedReader(MB.getBuffer()) {}

  EXI_USE_ORDREADER_METHODS(OrderedReader, public)

  /// Decodes a UInt size, then reads a unicode string to the buffer.
  /// Should only be used for URIs and Prefixes.
  virtual ExiResult<StrRef> decodeString(
    SmallVecImpl<char>& Data) = 0;

  /// Reads a unicode string to the buffer.
  virtual ExiResult<StrRef> readString(
    u64 Size, SmallVecImpl<char>& Data) = 0;

  /// Return size of the stream in bytes.
  usize sizeInBytes() const { return Stream.size(); }

protected:
  ExiResult<unsigned> fillStoreImpl() {
    if EXI_UNLIKELY(ByteOffset >= Stream.size())
      // Read of an empty buffer.
      return Err(ExiError::OOB);

    // Read the next "word" from the stream.
    const u8* WordPtr = Stream.data() + ByteOffset;
    unsigned BytesRead;
    if EXI_LIKELY(Stream.size() >= ByteOffset + sizeof(word_t)) {
      // Read full "word" of data.
      BytesRead = sizeof(word_t);
      // TODO: Change this on arm?
      Store = support::endian::read<word_t, endianness::little>(WordPtr);
    } else {
      // Partial read.
      BytesRead = Stream.size() - ByteOffset;
      ByteOffset = 0;
      for (unsigned Ix = 0; Ix != BytesRead; ++Ix)
        Store |= word_t(WordPtr[Ix]) << (Ix * 8);
    }

    ByteOffset += BytesRead;
    return Ok(BytesRead);
  }

private:
  virtual void anchor();
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "BitReader"

class BitReader final : public OrderedReader, METHOD_BAG(BitReader) {
  using BaseT = OrderedReader;
  using BaseT::Store;
  /// This is the number of bits in Store that are valid. This is always from
  /// `[0 ... bitsizeof(size_t) - 1]` inclusive.
  unsigned BitsInStore = 0;

  /// Masks shifts to avoid UB.
  static constexpr word_t ShiftMask = (kBitsPerWord - 1);

  using BaseT::MakeNBitMask;

  /// Creates a mask for the current word type.
  inline static constexpr word_t MakeMask(unsigned Bits) EXI_READNONE {
    return (~word_t(0) >> Bits);
  }

public:
  EXI_USE_ORDREADER_METHODS(BitReader, public)

  ExiResult<bool> readBit() override {
    if EXI_UNLIKELY(BitsInStore == 0) {
      // Refill the store and grab the next set of bits.
      // No need for other checks, as it will error when the stream is empty.
      exi_try_r(fillStore());
      // TODO: Remove sanity check.
      exi_invariant(BitsInStore > 0);
    }

    const word_t Out = Store & 0x1;

    Store >>= 1;
    BitsInStore -= 1;

    return static_cast<bool>(Out);
  }

  ExiResult<u8> readByte() override {
    // Handle 8 bit read with
    return this->readNBits<8, u8>();
  }
  
  ExiResult<u64> readBits64(unsigned Bits) override {
    exi_invariant(Bits <= kBitsPerWord,
      "Cannot return more than BitsPerWord bits!");

    if (Bits == 0)
      // Do nothing...
      return 0;

    if (Bits <= BitsInStore) {
      const word_t Out = Store & MakeMask(BitsInStore - Bits);
      Store >>= (Bits & ShiftMask);
      BitsInStore -= Bits;
      return Out;
    }

    tail_return this->readPartialBits64(Bits);
  }

  ExiResult<u64> readUInt() override {
    tail_return this->readNByteUInt<8>();
  }

  ExiResult<StrRef> decodeString(SmallVecImpl<char>& Data) override {
    const auto R = this->readUInt();
    if EXI_UNLIKELY(R.is_err())
      return Err(R.error());
    return this->readString(*R, Data);
  }

  ExiResult<StrRef> readString(
   u64 Size, SmallVecImpl<char>& Data) override {
    Data.clear();
    if (Size == 0)
      return ""_str;

    Data.reserve(Size);
    for (u64 Ix = 0; Ix < Size; ++Ix) {
      ExiResult<u64> Rune = this->readNByteUInt<4>();
      if EXI_UNLIKELY(Rune.is_err()) {
        LOG_ERROR("Invalid Rune at [{}:{}].", Ix, Size);
        return Err(Rune.error());
      }

      auto Buf = RuneEncoder::Encode(*Rune);
      Data.append(Buf.data(), Buf.data() + Buf.size());
      
      LOG_EXTRA(">>> {}: {}", Buf.str(), 
        fmt::format("0x{:02X}", fmt::join(Buf, " 0x")));
    }

    return StrRef(Data.data(), Data.size());
  }

  ExiError fillStore() {
    const auto R = BaseT::fillStoreImpl();
    if EXI_UNLIKELY(R.is_err())
      return R.error();
    
    const unsigned BytesRead = *R;
    BitsInStore = (BytesRead * 8);
  
    return ExiError::OK;
  }

  StreamKind getStreamKind() const override {
    return SK_Bit;
  }

private:
  template <unsigned Bits, std::integral Ret = u64>
  inline ExiResult<Ret> readNBits() {
    static_assert(Bits <= 64, "Read is too big!");

    if constexpr (Bits == 0)
      return static_cast<Ret>(0);

    if EXI_LIKELY(BitsInStore >= Bits) {
      // Create a simple static mask for the bits.
      static constexpr word_t Mask = MakeNBitMask(Bits);
      const word_t Out = Store & Mask;

      Store >>= Bits;
      BitsInStore -= Bits;
  
      return promotion_cast<Ret>(Out);
    }

    // Handle fallback case with a partial read.
    Result Out = this->readPartialBits64(Bits);
    if constexpr (std::same_as<u64, Ret>) {
      return std::move(Out);
    } else {
      if EXI_UNLIKELY(Out.is_err())
        return Err(Out.error());
      return promotion_cast<Ret>(*Out);
    }
  }

  template <unsigned Bytes = 8>
  inline ExiResult<u64> readNByteUInt() {
    static_assert(Bytes <= 8, "Read is too big!");

    // While the codegen for this loop is identical for the multiplication/bitwise
    // variants on Clang, it makes a difference on GCC. Because of this, I've
    // updated it to use bitwise operations.

    u64 Multiplier = 0, Value = 0;

    // TODO: Add some bit hacks to this?
    for (isize N = 0; N < isize(Bytes); ++N) {
      const Result R = this->readNBits<8>();
      if EXI_UNLIKELY(R.is_err())
        return R;
      
      const auto Octet = *R;
      const u64 Lo = Octet & 0b0111'1111;

      // Same as (Lo * 2^Multiplier)
      Value += (Lo << Multiplier);
      if (Octet & 0b1000'0000) {
        // Same as (Multiplier *= 128).
        Multiplier += 7;
        continue;
      }

      return Ok(Value);
    }

    LOG_WARN("uint exceeded {} octets.\n", Bytes);
    return Err(ErrorCode::kInvalidEXIInput);
  }

  /// Do a read where the result can't fit in the current space.
  inline ExiResult<u64> readPartialBits64(unsigned Bits) {
    word_t Out = BitsInStore ? Store : 0;
    const unsigned HeadBits = (Bits - BitsInStore);

    // Refill the store and grab the next set of bits.
    exi_try_r(fillStore());
    // Check for overlong reads of the buffer.
    if EXI_UNLIKELY(HeadBits > BitsInStore)
      return Err(ExiError::OOB);
    
    const word_t R = Store & MakeMask(BitsInStore - HeadBits);

    Store >>= (HeadBits & ShiftMask);
    BitsInStore -= HeadBits;

    Out |= R << (Bits - HeadBits);
    return Out;
  }

  void anchor() override;
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "ByteReader"

class ByteReader final : public OrderedReader, METHOD_BAG(ByteReader) {
  using BaseT = OrderedReader;
  using BaseT::Store;
  /// This is the number of bytes in Store that are valid. This is always from
  /// `[0 ... sizeof(size_t) - 1]` inclusive.
  unsigned BytesInStore = 0;

  /// Masks shifts to avoid UB.
  static constexpr word_t ShiftMask = (kBitsPerWord - 1);

  using BaseT::MakeNBitMask;

  /// Get the byte-aligned shift required for N bits.
  inline static constexpr unsigned MakeBitShift(unsigned Bits) EXI_READONLY {
    exi_invariant(Bits > 0);
    // Only use high bits.
    constexpr_static unsigned Mask = ~unsigned(0b111) & ShiftMask;
    // Subtracts 1 so multiples of 8 aren't given an extra byte, masks off the
    // low bits to get the significant digits, then goes to the next byte.
    return ((Bits - 1) & Mask) + 8;
  }

  /// Get the number of bytes N bits can fit in.
  inline static constexpr unsigned MakeByteCount(unsigned Bits) EXI_READNONE {
    return ((Bits - 1) >> 3) + 1;
  }

public:
  EXI_USE_ORDREADER_METHODS(ByteReader, public)

  ExiResult<bool> readBit() override {
    return this->readNBits<1, bool>();
  }

  ExiResult<u8> readByte() override {
    return this->readNBits<8, u8>();
  }

  ExiResult<u64> readBits64(unsigned Bits) override {
    exi_invariant(Bits <= kBitsPerWord,
      "Cannot return more than BitsPerWord bits!");

    if (Bits == 0)
      // Do nothing...
      return 0;

    const unsigned Bytes = MakeByteCount(Bits);
    if (Bytes <= BytesInStore) {
      const word_t Out = Store & MakeNBitMask(Bits);
      Store >>= MakeBitShift(Bits);
      BytesInStore -= Bytes;
      return Out;
    }

    tail_return this->readPartialBits64(Bits);
  }

  ExiResult<u64> readUInt() override {
    u64 Multiplier = 0, Value = 0;

    // While the codegen for this loop is identical for the multiplication/bitwise
    // variants on Clang, it makes a difference on GCC. Because of this, I've
    // updated it to use bitwise operations.

    // TODO: Add some bit hacks to this? Should be even easier.
    for (isize N = 0; N < 8; ++N) {
      const Result R = this->readNBits<8>();
      if EXI_UNLIKELY(R.is_err())
        return R;
      
      const auto Octet = *R;
      const u64 Lo = Octet & 0b0111'1111;

      // Same as (Lo * 2^Multiplier)
      Value += (Lo << Multiplier);
      if (Octet & 0b1000'0000) {
        // Same as (Multiplier *= 128).
        Multiplier += 7;
        continue;
      }

      return Ok(Value);
    }

    LOG_WARN("uint exceeded 8 octets.\n");
    return Err(ErrorCode::kInvalidEXIInput);
  }

  ExiResult<StrRef> decodeString(SmallVecImpl<char>& Data) override {
    const auto R = this->readUInt();
    if EXI_UNLIKELY(R.is_err())
      return Err(R.error());
    return this->readString(*R, Data);
  }

  ExiResult<StrRef> readString(
   u64 Size, SmallVecImpl<char>& Data) override {
    Data.clear();
    if (Size == 0)
      return ""_str;

    Data.reserve(Size);
    for (u64 Ix = 0; Ix < Size; ++Ix) {
      ExiResult<u64> Rune = this->readUInt();
      if EXI_UNLIKELY(Rune.is_err()) {
        LOG_ERROR("Invalid Rune at [{}:{}].", Ix, Size);
        return Err(Rune.error());
      }

      auto Buf = RuneEncoder::Encode(*Rune);
      Data.append(Buf.data(), Buf.data() + Buf.size());
      
      LOG_EXTRA(">>> {}: {}", Buf.str(), 
        fmt::format("0x{:02X}", fmt::join(Buf, " 0x")));
    }

    return StrRef(Data.data(), Data.size());
  }

  ExiError fillStore() {
    const auto R = BaseT::fillStoreImpl();
    if EXI_UNLIKELY(R.is_err())
      return R.error();
    
    BytesInStore = *R;
    return ExiError::OK;
  }

  StreamKind getStreamKind() const override {
    return SK_Byte;
  }

private:
  /// Specialized for cases where only one byte is read. Since there can't be
  /// tearing like with `BitStream`s, this is the smallest unit and needs no
  /// extra logic.
  template <unsigned Bits, std::integral Ret = u64>
  EXI_INLINE ExiResult<Ret> readNBits() requires(Bits <= 8) {
    if constexpr (Bits == 0)
      return static_cast<Ret>(0);

    if EXI_UNLIKELY(BytesInStore == 0) {
      // Refill the store and grab the next set of bits.
      // No need for other checks, as it will error when the stream is empty.
      exi_try_r(fillStore());
      // TODO: Remove sanity check.
      exi_invariant(BytesInStore > 0);
    }

    static constexpr word_t Mask = MakeNBitMask(Bits);
    const word_t Out = Store & Mask;

    Store >>= 8;
    BytesInStore -= 1;

    return promotion_cast<Ret>(Out);
  }

  template <unsigned Bits, std::integral Ret = u64>
  inline ExiResult<Ret> readNBits() requires(Bits > 8) {
    static_assert(Bits <= 64, "Read is too big!");

    static constexpr unsigned Bytes = MakeByteCount(Bits);
    static constexpr word_t Mask = MakeNBitMask(Bits);
    
    if EXI_LIKELY(BytesInStore >= Bytes) {
      // Create a simple static mask for the bits.
      const word_t Out = Store & Mask;

      Store >>= MakeBitShift(Bits);
      BytesInStore -= Bytes;
  
      return promotion_cast<Ret>(Out);
    }

    // Handle fallback case with a partial read.
    Result Out = this->readPartialBytes64(Bytes);
    if EXI_UNLIKELY(Out.is_err())
      return Err(Out.error());
    return promotion_cast<Ret>(*Out & Mask);
  }

  /// Do a read where the result can't fit in the current space.
  ExiResult<u64> readPartialBytes64(unsigned Bytes) {
    word_t Out = BytesInStore ? Store : 0;
    const unsigned HeadBytes = (Bytes - BytesInStore);

    // Refill the store and grab the next set of bits.
    exi_try_r(fillStore());
    // Check for overlong reads of the buffer.
    if EXI_UNLIKELY(HeadBytes > BytesInStore)
      return Err(ExiError::OOB);
    
    const word_t R = Store;

    Store >>= (HeadBytes * 8);
    BytesInStore -= HeadBytes;

    Out |= R << ((Bytes - HeadBytes) * 8);
    return Out;
  }

  /// Do a read where the result can't fit in the current space.
  EXI_INLINE ExiResult<u64> readPartialBits64(unsigned Bits) {
    const unsigned Bytes = MakeByteCount(Bits);
    Result Out = this->readPartialBytes64(Bytes);

    if EXI_UNLIKELY(Out.is_err())
      return Err(Out.error());
    
    const word_t Mask = MakeNBitMask(Bits);
    return *Out & Mask;
  }

  void anchor() override;
};

#undef DEBUG_TYPE

} // namespace exi

#undef METHOD_BAG
