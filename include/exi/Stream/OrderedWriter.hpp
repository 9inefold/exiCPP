//===- exi/Stream/OrderedWriter.hpp ---------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
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
/// This file defines the in-order writers.
/// Sections adapted from: llvm-project/llvm/include/llvm/Bitstream/BitstreamWriter.h
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ManualRebind.hpp>
#include <core/Common/Poly.hpp>
#include <core/Common/Ref.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Stream/Writer.hpp>
#if EXI_LOGGING
# include <fmt/ranges.h>
#endif

namespace exi {

#define DEBUG_TYPE "OrderedWriter"

/// The bases for BitWriter/ByteWriter, which consume data in the order it
/// appears. This allows for a much simpler implementation.
// HACK: Proxy copying sucks! Please fix!
class OrderedWriter : public WriterBase {
public:
  /// @brief Default flushing threshold in MiB.
  static constexpr size_type kFlushThreshold = 512;
  /// @brief Currently uses MiB.
  static constexpr size_type kFlushUnits = 20;

protected:
  /// Owned buffer, used as the buffer if the input stream is not
  /// a `raw_svector_ostream`.
  SmallVec<char, 0> OwnBuffer;
  /// Internal buffer for unflushed bytes (unless there is no stream to flush
  /// to, case in which these are "the bytes").
  SmallVecImpl<char>& Buffer;

  /// The file stream that Buffer flushes to. If FS is a `raw_fd_stream`, the
  /// writer will incrementally flush. Otherwise flushing will happen at the end
  /// of the object's lifetime.
  ManualRebindPtr<raw_ostream> FS = nullptr;

  /// The threshold (unit B) to flush to FS, if FS is a `raw_fd_stream`.
  const u64 FlushThreshold = 0;

  /// A value in the range [0, 64), specifies the next bit to use.
  size_type BitsInStore = 0;

  /// The current value. Only bits < BitsInStore are valid.
  word_t Store = 0;

  ////////////////////////////////////////////////////////////////////////
  // Stream Utilities

private:
  void flushAndClear() {
    exi_assert(FS);
    exi_assert(!Buffer.empty());
    FS->write(Buffer.data(), Buffer.size());
    Buffer.clear();
  }

  SmallVecImpl<char>& getInternalBufferFromStream(raw_ostream& Strm) {
    if (auto* SV = dyn_cast<raw_svector_ostream>(&Strm))
      return SV->buffer();
    return OwnBuffer;
  }

  SmallVecImpl<char>& getInternalBufferFromExternalSource(OrderedWriter& O) {
    if (O.isOwnBuffer())
      return O.Buffer;
    return OwnBuffer;
  }

  bool isOwnBuffer() const {
    return &Buffer == &OwnBuffer;
  }

protected:
  /// If the related file stream is a `raw_fd_stream`, flush the buffer if its
  /// size is above a threshold. If \p OnClosing is true, flushing happens
  /// regardless of thresholds.
  void flushToFile(bool OnClosing = false) {
    if (!FS || Buffer.empty())
      return;
    if (OnClosing)
      return flushAndClear();
    if (fdStream() && Buffer.size() > FlushThreshold)
      flushAndClear();
  }

  void writeWord(word_t Val) {
    Val = support::endian::byte_swap<word_t, endianness::little>(Val);
    Buffer.append(reinterpret_cast<const char*>(&Val),
                  reinterpret_cast<const char*>(&Val + 1));
  }

  /// Will only be used when copying by proxy.
  void writePartialWord(word_t Val, usize N) {
    Val = support::endian::byte_swap<word_t, endianness::little>(Val);
    const char* Ptr = reinterpret_cast<const char*>(&Val);
    N = std::min(N, sizeof(word_t));
    Buffer.append(Ptr, Ptr + N);
  }

  void writeBytes(ArrayRef<char> Bytes) {
    Buffer.append(Bytes.begin(), Bytes.end());
  }

  raw_fd_stream* fdStream() {
    return dyn_cast_or_null<raw_fd_stream>(&*FS);
  }

  const raw_fd_stream* fdStream() const {
    return dyn_cast_or_null<raw_fd_stream>(&*FS);
  }

  OrderedWriter(OrderedWriter&& O) :
   OwnBuffer(std::move(O.OwnBuffer)),
   Buffer(getInternalBufferFromExternalSource(O)),
   FS(std::move(O.FS)), FlushThreshold(O.FlushThreshold),
   BitsInStore(O.BitsInStore), Store(O.Store) {
    exi_invariant(O.OwnBuffer.empty());
    std::launder(&O)->invalidate();
  }

  struct invalid_state_tag {};
  OrderedWriter(invalid_state_tag) :
   Buffer(OwnBuffer), FS(nullptr),
   FlushThreshold(0) {}

public:
  /// Create a BitstreamWriter over a raw_ostream \p OutStream.
  /// If \p OutStream is a raw_svector_ostream, the BitstreamWriter will write
  /// directly to the latter's buffer. In all other cases, the BitstreamWriter
  /// will use an internal buffer and flush at the end of its lifetime.
  ///
  /// In addition, if \p is a raw_fd_stream supporting seek, tell, and read
  /// (besides write), the BitstreamWriter will also flush incrementally, when a
  /// subblock is finished, and if the FlushThreshold is passed.
  ///
  /// NOTE: \p FlushThreshold's unit is MiB.
  OrderedWriter(raw_ostream& Strm, u32 FlushThreshold = 512)
      : Buffer(getInternalBufferFromStream(Strm)),
        FS(!isa<raw_svector_ostream>(Strm) ? &Strm : nullptr),
        FlushThreshold(u64(FlushThreshold) << kFlushUnits) {}

  /// Convenience constructor for users that start with a vector - avoids
  /// needing to wrap it in a raw_svector_ostream.
  OrderedWriter(SmallVecImpl<char>& Buf) : 
   Buffer(Buf), FS(nullptr), FlushThreshold(0) {}
  
  ~OrderedWriter() {
    this->flushToWord();
    this->flushToFile(/*OnClosing=*/true);
  }

  void flushToWord() {
    if EXI_UNLIKELY(!BitsInStore)
      return;
    
    // Switch to "big endian".
    // TODO: Update this for big endian systems.
    writeWord(exi::byteswap(Store));
    BitsInStore = 0;
    Store = 0;
  }

  usize size() const { return Buffer.size(); }
  usize bitsInStore() const { return BitsInStore; }

  ////////////////////////////////////////////////////////////////////////
  // Implementation

  /// Writes a single byte. Same in both versions.
  void writeByte(u8 Val) override final {
    this->writeNBits(Val, 8);
  }

  /// Writes an `Unsigned Integer` with a maximum of 8 octets.
  /// See https://www.w3.org/TR/exi/#encodingUnsignedInteger.
  void writeUInt(u64 Val) override final {
    this->writeNByteUInt<8>(Val);
  }

  /// Decodes a UInt size, then writes a unicode string to the buffer.
  /// Should only be used for URIs and Prefixes.
  void encodeString(StrRef Str) {
    this->writeUInt(Str.size());
    tail_return this->writeString(Str);
  }

  /// Writes a unicode string to the buffer.
  void writeString(StrRef Str) {
    if (Str.empty())
      return;
    
    RuneDecoder Decoder(Str);
    for (Rune Val : Decoder) {
      this->writeNByteUInt<UnicodeReads>(Val);
      // TODO: Log!!
    }
  }

  /// Writes a static number of bits (max of 64).
  template <unsigned Bits>
  void writeBits(ubit<Bits> Val) {
    this->writeNBits<u64>(Val, Bits);
  }

  /// Writes a `BitBuffer`.
  void writeBitBuffer(BitBuffer Val) {
    exi_todo("implement writeBitBuffer");
  }

protected:
  /// Writes a variable number of bits (max of 64).
  template <typename IntT = u64>
  ALWAYS_INLINE void writeNBits(IntT Val, size_type Bits) {
    Store |= Val << BitsInStore;
    if (Bits + BitsInStore < kBitsPerWord) {
      BitsInStore += Bits;
      return;
    }

    this->writeWord(Store);

    if (BitsInStore)
      Store = Val >> (kBitsPerWord - BitsInStore);
    else
      Store = 0;
    BitsInStore = (BitsInStore + Bits) & ShiftMask;
  }

  template <size_type Bytes = 8>
  inline void writeNByteUInt(word_t Val) {
    static_assert(Bytes <= sizeof(word_t), "Read is too large!");

    for (isize N = 0; N < isize(Bytes); ++N) {
      if (Val < (1 << 7)) {
        this->writeByte(Val);
        return;
      }
      
      const u64 Data = Val & 0b0111'1111;
      this->writeByte(Data | 0b1000'0000);

      Val >>= 7;
    }

    // Put this out of line to maybe prevent some spills?
    return this->failUInt<Bytes>();
  }

  void align() {
    const auto Bits = (BitsInStore & ByteAlignMask);
    this->writeBits64(0, Bits);
  }

  template <StreamKind Kind> static consteval bool IsBitPacked() {
    static_assert(Kind == SK_Bit || Kind == SK_Byte);
    return Kind == SK_Bit;
  }

  /// Unified implementation for `writeBit`.
  template <StreamKind Kind>
  ALWAYS_INLINE void writeBitImpl(bool Val) {
    if constexpr (IsBitPacked<Kind>())
      this->writeNBits(Val, 1);
    else
      this->writeByte(Val);
  }

  /// Unified implementation for `writeBits64`.
  template <StreamKind Kind>
  ALWAYS_INLINE void writeBits64Impl(u64 Val, size_type Bits) {
    exi_invariant(Bits <= kBitsPerWord,
      "Cannot write more than BitsPerWord bits!");
    
    if (Bits == 0)
      // Do nothing...
      return;
    
    if constexpr (IsBitPacked<Kind>())
      this->writeNBits(Val, Bits);
    else
      this->writeNBits(Val, MakeByteCount(Bits));
  }

  /// Sets the stream in an invalid state.
  template <std::derived_from<OrderedWriter> T>
  static void invalidate(T* self) {
    self->~T();
    new (self) T(invalid_state_tag{});
  }

private:
  template <size_type Bytes = 8>
  EXI_ERROR_CC void failUInt() {
    // TODO: Add NO_INLINE?
    LOG_WARN("uint exceeded {} octets.\n", Bytes);
    //return Err(ErrorCode::kInvalidEXIInput);
  }

  virtual void invalidate() = 0;
  void anchor() override;
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "BitWriter"

class BitWriter final : public OrderedWriter {
  using BaseT = OrderedWriter;
  using BaseT::BitsInStore;
  using BaseT::Store;
public:
  using OrderedWriter::OrderedWriter;

  template <std::derived_from<OrderedWriter> Strm>
  BitWriter(Strm&& O) : OrderedWriter(std::move(O)) {}

  template <std::derived_from<OrderedWriter> Strm>
  BitWriter(const Strm& O) = delete;

  /// Writes a single bit.
  void writeBit(bool Val) override {
    BaseT::writeBitImpl<SK_Bit>(Val);
  }

  /// Writes a variable number of bits (max of 64).
  void writeBits64(u64 Val, size_type Bits) override {
    BaseT::writeBits64Impl<SK_Bit>(Val, Bits);
  }

  // Expose alignment for bit packing.
  using BaseT::align;

  StreamKind getStreamKind() const override {
    return SK_Bit;
  }

private:
  void invalidate() override {
    BaseT::invalidate(this);
  }

  void anchor() override;
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "ByteWriter"

class ByteWriter final : public OrderedWriter {
  using BaseT = OrderedWriter;
  using BaseT::BitsInStore;
  using BaseT::Store;
  using StreamBase::MakeByteCount;

  static BitWriter&& AlignForMove(BitWriter&& O) {
    O.align();
    return std::move(O);
  }

public:
  using OrderedWriter::OrderedWriter;
  ByteWriter(ByteWriter&& O) : OrderedWriter(std::move(O)) {}
  ByteWriter(BitWriter&& O)  : OrderedWriter(AlignForMove(std::move(O))) {}

  /// Writes a single bit.
  void writeBit(bool Val) override {
    BaseT::writeBitImpl<SK_Byte>(Val);
  }

  /// Writes a variable number of bits (max of 64).
  void writeBits64(u64 Val, size_type Bits) override {
    BaseT::writeBits64Impl<SK_Byte>(Val, Bits);
  }

  StreamKind getStreamKind() const override {
    return SK_Byte;
  }

private:
  void invalidate() override {
    BaseT::invalidate(this);
  }

  void anchor() override;
};

#undef DEBUG_TYPE

/// @brief Inline virtual for ordered writers.
using OrdWriter = Poly<OrderedWriter, BitWriter, ByteWriter>;

} // namespace exi

#undef METHOD_BAG
