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

#include <exi/Stream2/Reader.hpp>

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
  /// This is the number of bits in Store that are valid. This is always from
  /// `[0 ... bitsizeof(size_t) - 1]` inclusive.
  unsigned BitsInStore = 0;

  /// Creates a mask for the current word type.
  ALWAYS_INLINE static constexpr word_t MakeMask(unsigned Bits) {
    return (~word_t(0) >> Bits);
  }

  /// Creates a mask for the current word type.
  ALWAYS_INLINE static constexpr word_t MakeNBitMask(unsigned Bits) {
    return (word_t(1) << Bits) - 1;
  }

public:
  OrderedReader() = default;
  explicit OrderedReader(buffer_t Stream) : Stream(Stream) {}
  explicit OrderedReader(StrRef Buf) : Stream(arrayRefFromStringRef(Buf)) {}
  explicit OrderedReader(MemoryBufferRef MB) : OrderedReader(MB.getBuffer()) {}

  EXI_USE_ORDREADER_METHODS(OrderedReader, public)

  /// Decodes a UInt size, then reads a unicode string to the buffer.
  virtual ExiResult<StrRef> decodeString(
    SmallVecImpl<char>& Data) = 0;

  /// Reads a unicode string to the buffer.
  virtual ExiResult<StrRef> readString(
    unsigned Size, SmallVecImpl<char>& Data) = 0;

  ExiError fillStore() {
    if EXI_UNLIKELY(ByteOffset >= Stream.size())
      // Read of an empty buffer.
      return ExiError::OOB;

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
    BitsInStore = (BytesRead * 8);
    return ExiError::OK;
  }

  /// Return size of the stream in bytes.
  usize sizeInBytes() const { return Stream.size(); }

private:
  virtual void anchor();
};

class BitReader final : public OrderedReader, METHOD_BAG(BitReader) {
  using BaseT = OrderedReader;
  using BaseT::Store;
  using BaseT::BitsInStore;

  using BaseT::MakeMask;
  using BaseT::MakeNBitMask;

  static_assert(kBitsPerWord >= 64, "Work on this...");
  /// Masks shifts to avoid UB.
  static constexpr word_t ShiftMask = (kBitsPerWord - 1);

public:
  EXI_USE_ORDREADER_METHODS(BitReader, public)

  ExiResult<bool> readBit() override {
    if EXI_UNLIKELY(BitsInStore == 0) {
      // Refill the store and grab the next set of bits.
      // No need for other checks, as it will error when the stream is empty.
      exi_try_r(BaseT::fillStore());
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

  }

  StreamKind getStreamKind() const override {
    return SK_Bit;
  }

private:
  template <unsigned Bits, std::integral Ret = u64>
  EXI_INLINE ExiResult<Ret> readNBits() {
    if EXI_LIKELY(BitsInStore >= Bits) {
      // Create a simple static mask for the bits.
      static constexpr word_t Mask = MakeNBitMask(Bits);
      const word_t Out = Store & Mask;

      Store >>= Bits;
      BitsInStore -= Bits;
  
      return promotion_cast<Ret>(Out);
    }

    // Handle fallback case with a partial read.
    if constexpr (std::same_as<u64, Ret>) {
      tail_return this->readPartialBits64(Bits);
    } else {
      Result Out = this->readPartialBits64(Bits);
      if EXI_UNLIKELY(Out.is_err())
        return Err(Out.error());
      return promotion_cast<Ret>(*Out);
    }
  }

  /// Do a read where the result can't fit in the current space.
  inline ExiResult<u64> readPartialBits64(unsigned Bits) {
    word_t Out = BitsInStore ? Store : 0;
    const unsigned HeadBits = (Bits - BitsInStore);

    // Refill the store and grab the next set of bits.
    exi_try_r(BaseT::fillStore());
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

#if 0
class ByteReader final : public OrderedReader, METHOD_BAG(ByteReader) {
  using BaseT = OrderedReader;
  using BaseT::MakeMask;
public:
  EXI_USE_ORDREADER_METHODS(ByteReader, public)

  StreamKind getStreamKind() const override {
    return SK_Byte;
  }

private:
  void anchor() override;
};
#endif

} // namespace exi

#undef METHOD_BAG
