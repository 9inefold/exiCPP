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

#include <core/Common/Poly.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Stream/Reader.hpp>
#if EXI_LOGGING
# include <fmt/ranges.h>
#endif

namespace exi {

#define DEBUG_TYPE "ByteReader"

/// The bases for BitReader/ByteReader, which consume data in the order it
/// appears. This allows for a much simpler implementation.
class OrderedReader : public ReaderBase {
protected:
  /// The current stream data.
  buffer_t Stream;
  /// The offset of the current stream in bytes.
  size_type ByteOffset = 0;
  /// The current word, cached data from the stream.
  word_t Store = 0;

  static_assert(kBitsPerWord >= 64, "Work on this...");
  /// Masks shifts to avoid UB (even larger sizes will use a max of 64 bits).
  static constexpr word_t ShiftMask = 0x3f;
  // This allows for `[0, 2,097,152)`, unicode only requiring `[0, 1,114,112)`.
  static constexpr size_type UnicodeReads = 3;

  /// Creates a mask for the current word type.
  inline static constexpr word_t MakeNBitMask(size_type Bits) EXI_READNONE {
    constexpr_static word_t Mask = ~word_t(0);
    return EXI_LIKELY(Bits != kBitsPerWord) ? ~(Mask << Bits) : Mask;
  }

  /// Get the number of bytes N bits can fit in.
  inline static constexpr size_type MakeByteCount(size_type Bits) EXI_READNONE {
    return ((Bits - 1) >> 3) + 1;
  }

public:
  OrderedReader() = default;
  OrderedReader(proxy_t Proxy) : OrderedReader(Proxy.Bytes) {
    if (Proxy.NBits != 0)
      this->ByteOffset = MakeByteCount(Proxy.NBits);
  }

  explicit OrderedReader(buffer_t Stream) : Stream(Stream) {}
  explicit OrderedReader(StrRef Buf) : Stream(arrayRefFromStringRef(Buf)) {}
  explicit OrderedReader(MemoryBufferRef MB) : OrderedReader(MB.getBuffer()) {}

# define CLASSNAME OrderedReader
# include "D/OrdReaderMethods.mac"

  /// Decodes a UInt size, then reads a unicode string to the buffer.
  /// Should only be used for URIs and Prefixes.
  virtual ExiResult<StrRef> decodeString(
    SmallVecImpl<char>& Data) = 0;

  /// Reads a unicode string to the buffer.
  virtual ExiResult<StrRef> readString(
    u64 Size, SmallVecImpl<char>& Data) = 0;

  virtual proxy_t getProxy() const = 0;
  virtual void setProxy(proxy_t Proxy) = 0;

  /// The position in bits.
  virtual size_type bitPos() const {
    return ByteOffset * 8;
  }

  /// Return size of the stream in bytes.
  usize sizeInBytes() const { return Stream.size(); }

  /// Return if the stream has data or not.
  bool hasData() const { return Stream.size() >= ByteOffset; }

protected:
  ExiResult<size_type> fillStoreImpl() {
    if EXI_UNLIKELY(ByteOffset >= Stream.size())
      // Read of an empty buffer.
      return Err(ExiError::OOB);

    // Read the next "word" from the stream.
    const u8* WordPtr = Stream.data() + ByteOffset;
    size_type BytesRead;
    if EXI_LIKELY(Stream.size() >= ByteOffset + sizeof(word_t)) {
      // Read full "word" of data.
      BytesRead = sizeof(word_t);
      // TODO: Change this on arm?
      // std::memcpy(&Store, WordPtr, sizeof(word_t));
      Store = support::endian::read<word_t, endianness::little>(WordPtr);
    } else {
      // Partial read.
      BytesRead = Stream.size() - ByteOffset;
      ByteOffset = 0;
      for (size_type Ix = 0; Ix != BytesRead; ++Ix)
        Store |= word_t(WordPtr[Ix]) << (Ix * 8);
    }

    ByteOffset += BytesRead;
    if (ByteOffset == Stream.size())
      // Set the byte offset larger than the stream size.
      // This will allow us to introspect on the state later.
      ++ByteOffset;
    return Ok(BytesRead);
  }

  void setProxyBase(proxy_t Proxy) {
    auto [Bytes, NBits] = Proxy;
    this->Stream = Bytes;
    this->ByteOffset = NBits ? MakeByteCount(NBits) : 0;
    this->Store = 0;
  }

private:
  virtual void anchor();
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "BitReader"

class BitReader final : public OrderedReader {
  using BaseT = OrderedReader;
  using BaseT::Store;
  /// This is the number of bits in Store that are valid. This is always from
  /// `[0 ... bitsizeof(size_t) - 1]` inclusive.
  size_type BitsInStore = 0;

  static constexpr size_type ByteAlignMask = 0b111;

  using BaseT::ShiftMask;
  using BaseT::UnicodeReads;
  using BaseT::MakeNBitMask;

  /// Creates a mask for the current word type.
  inline static constexpr word_t MakeMask(size_type Bits) EXI_READNONE {
    // return (~word_t(0) >> Bits);
    return MakeNBitMask(Bits);
  }

  /// Creates a reverse mask for the current word type.
  inline static constexpr word_t MakeReverseMask(size_type Bits) EXI_READNONE {
    constexpr_static word_t Mask = ~word_t(0);
    return EXI_LIKELY(Bits != kBitsPerWord) ? ~(Mask >> Bits) : Mask;
  }

public:
  using BaseT::BaseT;
# define CLASSNAME BitReader
# define UNDEF_OVERRIDES
# include "D/OrdReaderMethods.mac"

  BitReader(proxy_t Proxy) : OrderedReader() {
    this->setProxy(Proxy);
  }

  ExiResult<bool> readBit() override {
    if EXI_UNLIKELY(BitsInStore == 0) {
      // Refill the store and grab the next set of bits.
      // No need for other checks, as it will error when the stream is empty.
      exi_try_r(fillStore());
    }

    const word_t Out = Store >> (kBitsPerWord - 1);
    Store <<= 1;
    BitsInStore -= 1;
    return (Out != 0);
  }

  ExiResult<u8> readByte() override {
    // Handle 8 bit read with
    tail_return this->readNBits<8, u8>();
  }
  
  EXI_FLATTEN ExiResult<u64> readBits64(size_type Bits) override {
    exi_invariant(Bits <= kBitsPerWord,
      "Cannot return more than BitsPerWord bits!");

    if (Bits == 0)
      // Do nothing...
      return 0;

    if (Bits <= BitsInStore)
      // Handle cases which don't need loading.
      tail_return this->readFullBits64(Bits);

    // Handle cases which need loading.
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
      ExiResult<u64> Rune = this->readNByteUInt<UnicodeReads>();
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

  EXI_FLATTEN ExiError fillStore() {
    const auto R = BaseT::fillStoreImpl();
    if EXI_UNLIKELY(R.is_err())
      return R.error();
    
    // Switch to "big endian".
    // TODO: Update this for big endian systems.
    Store = exi::byteswap(Store);

    const size_type BytesRead = *R;
    BitsInStore = (BytesRead * 8);
  
    return ExiError::OK;
  }

  proxy_t getProxy() const override {
    return {BaseT::Stream, (ByteOffset * 8) - BitsInStore};
  }

  void setProxy(proxy_t Proxy) override {
    BaseT::setProxyBase(Proxy);
    // Align to the old offset.
    BaseT::ByteOffset -= (BaseT::ByteOffset % sizeof(word_t));
    // Load data into store.
    if (auto E = fillStore()) {
      dbgs() << E << '\n';
      exi_unreachable("unable to load store");
    }

    const auto Off = (Proxy.NBits % kBitsPerWord);
    Store <<= Off;
    BitsInStore -= Off;
  }

  void align() {
    const auto Bits = (BitsInStore & ByteAlignMask);
    Store <<= Bits;
    BitsInStore -= Bits;
  }

  /// The position in bits.
  size_type bitPos() const override {
    return (ByteOffset * 8) - BitsInStore;
  }

  StreamKind getStreamKind() const override {
    return SK_Bit;
  }

private:
  template <size_type Bits, std::integral Ret = u64>
  EXI_FLATTEN inline ExiResult<Ret> readNBits() {
    static_assert(Bits <= 64, "Read is too big!");

    if constexpr (Bits == 0)
      return static_cast<Ret>(0);

    if EXI_LIKELY(BitsInStore >= Bits) {
      const word_t Out = readImpl(Bits);
      Store <<= Bits;
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

  template <size_type Bytes = 8>
  inline ExiResult<u64> readNByteUInt() {
    static_assert(Bytes <= sizeof(word_t), "Read is too large!");

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

    // Put this out of line to maybe prevent some spills?
    tail_return this->failUInt<Bytes>();
  }

  ////////////////////////////////////////////////////////////////////////
  // Dynamic Reads

  ALWAYS_INLINE EXI_NODEBUG u64 readImpl(const size_type Bits) {
    return Store >> (kBitsPerWord - Bits);
  }

  /// Do a read where the result CAN fit in the current space.
  ALWAYS_INLINE ExiResult<u64> readFullBits64(const size_type Bits) {
    return this->readFullBits64V(Bits);
  }

  /// Do a read where the result CAN fit in the current space.
  /// @invariant `Bits` cannot be 0.
  ALWAYS_INLINE u64 readFullBits64V(const size_type Bits) {
    const word_t Out = readImpl(Bits);
    Store <<= Bits;
    BitsInStore -= Bits;
    return Out;
  }

  /// Do a read where the result can't fit in the current space.
  inline ExiResult<u64> readPartialBits64(size_type Bits) {
    exi_invariant(BitsInStore < Bits,
                  "Bits is zero, an invalid shift will occur.");
    const size_type HeadBits = (Bits - BitsInStore);
    const u64 Out = BitsInStore ? readImpl(BitsInStore) : 0;

    // Refill the store and grab the next set of bits.
    exi_try_r(fillStore());
    // Check for overlong reads of the buffer.
    if EXI_UNLIKELY(HeadBits > BitsInStore)
      return Err(ExiError::OOB);
    
    // Always starts off aligned.
    const u64 R = readFullBits64V(HeadBits);
    return (Out << HeadBits) | R;
  }

  ////////////////////////////////////////////////////////////////////////
  // Failure

  template <size_type Bytes = 8>
  EXI_COLD ExiResult<u64> failUInt() {
    // TODO: Add NO_INLINE?
    LOG_WARN("uint exceeded {} octets.\n", Bytes);
    return Err(ErrorCode::kInvalidEXIInput);
  }

  void anchor() override;
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "ByteReader"

class ByteReader final : public OrderedReader {
  using BaseT = OrderedReader;
  using BaseT::Store;
  /// This is the number of bytes in Store that are valid. This is always from
  /// `[0 ... sizeof(size_t) - 1]` inclusive.
  size_type BytesInStore = 0;

  using BaseT::ShiftMask;
  using BaseT::MakeNBitMask;
  using BaseT::MakeByteCount;

  /// Get the byte-aligned shift required for N bits.
  inline static constexpr size_type MakeBitShift(size_type Bits) EXI_READONLY {
    exi_invariant(Bits > 0);
    // Only use high bits.
    constexpr_static size_type Mask = ~size_type(0b111) & ShiftMask;
    // Subtracts 1 so multiples of 8 aren't given an extra byte, masks off the
    // low bits to get the significant digits, then goes to the next byte.
    return ((Bits - 1) & Mask) + 8;
  }

public:
  using BaseT::BaseT;
# define CLASSNAME ByteReader
# include "D/OrdReaderMethods.mac"

  ByteReader(proxy_t Proxy) : OrderedReader() {
    this->setProxy(Proxy);
  }

  ExiResult<bool> readBit() override {
    return this->readNBits<1, bool>();
  }

  ExiResult<u8> readByte() override {
    return this->readNBits<8, u8>();
  }

  ExiResult<u64> readBits64(size_type Bits) override {
    exi_invariant(Bits <= kBitsPerWord,
      "Cannot return more than BitsPerWord bits!");

    if (Bits == 0)
      // Do nothing...
      return 0;

    const size_type Bytes = MakeByteCount(Bits);
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
      // TODO: Use UnicodeReads blocks. May be possible to accelerate further.
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

  proxy_t getProxy() const override {
    return {BaseT::Stream, (ByteOffset - BytesInStore) * 8};
  }

  void setProxy(proxy_t Proxy) override {
    // TODO: setProxy
    exi_unreachable("TODO");
  }

  StreamKind getStreamKind() const override {
    return SK_Byte;
  }

private:
  /// Specialized for cases where only one byte is read. Since there can't be
  /// tearing like with `BitStream`s, this is the smallest unit and needs no
  /// extra logic.
  template <size_type Bits, std::integral Ret = u64>
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

  template <size_type Bits, std::integral Ret = u64>
  inline ExiResult<Ret> readNBits() requires(Bits > 8) {
    static_assert(Bits <= 64, "Read is too big!");

    static constexpr size_type Bytes = MakeByteCount(Bits);
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
  ExiResult<u64> readPartialBytes64(size_type Bytes) {
    word_t Out = BytesInStore ? Store : 0;
    const size_type HeadBytes = (Bytes - BytesInStore);

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
  EXI_INLINE ExiResult<u64> readPartialBits64(size_type Bits) {
    const size_type Bytes = MakeByteCount(Bits);
    Result Out = this->readPartialBytes64(Bytes);

    if EXI_UNLIKELY(Out.is_err())
      return Err(Out.error());
    
    const word_t Mask = MakeNBitMask(Bits);
    return *Out & Mask;
  }

  void anchor() override;
};

#undef DEBUG_TYPE

/// @brief Inline virtual for ordered readers.
using OrdReader = Poly<OrderedReader, BitReader, ByteReader>;

} // namespace exi

#undef METHOD_BAG
