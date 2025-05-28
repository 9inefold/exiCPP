//===- exi/Stream/OrderedReader.hpp ---------------------------------===//
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
/// This file defines the in-order readers.
/// Sections adapted from: llvm-project/llvm/include/llvm/Bitstream/BitstreamWriter.h
///
//===----------------------------------------------------------------===//

#pragma once

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
class OrderedWriter : public WriterBase {
  /// Owned buffer, used as the buffer if the input stream is not
  /// a `raw_svector_ostream`.
  SmallVec<char, 0> OwnBuffer;
  /// Internal buffer for unflushed bytes (unless there is no stream to flush
  /// to, case in which these are "the bytes").
  Ref<SmallVecImpl<char>> Buffer;

  /// The file stream that Buffer flushes to. If FS is a `raw_fd_stream`, the
  /// writer will incrementally flush. Otherwise flushing will happen at the end
  /// of the object's lifetime.
  raw_ostream* const FS = nullptr;

  /// The threshold (unit B) to flush to FS, if FS is a `raw_fd_stream`.
  const u64 FlushThreshold = 0;

  /// A value in the range [0, 64), specifies the next bit to use.
  size_type BitsInStore = 0;

  /// The current value. Only bits < BitsInStore are valid.
  word_t Store = 0;

  /// Creates a mask for the current word type.
  inline static constexpr word_t MakeNBitMask(size_type Bits) EXI_READNONE {
    constexpr_static word_t Mask = ~word_t(0);
    return EXI_LIKELY(Bits != kBitsPerWord) ? ~(Mask << Bits) : Mask;
  }

  /// Get the number of bytes N bits can fit in.
  inline static constexpr size_type MakeByteCount(size_type Bits) EXI_READNONE {
    exi_invariant(Bits != 0);
    return ((Bits - 1) >> 3) + 1zu;
  }

  ////////////////////////////////////////////////////////////////////////
  // Stream Utilities

  void flushAndClear() {
    exi_assert(FS);
    exi_assert(!Buffer->empty());
    FS->write(Buffer->data(), Buffer->size());
    Buffer->clear();
  }

  SmallVecImpl<char>& getInternalBufferFromStream(raw_ostream& Strm) {
    if (auto* SV = dyn_cast<raw_svector_ostream>(&Strm))
      return SV->buffer();
    return OwnBuffer;
  }

protected:
  /// If the related file stream is a `raw_fd_stream`, flush the buffer if its
  /// size is above a threshold. If \p OnClosing is true, flushing happens
  /// regardless of thresholds.
  void FlushToFile(bool OnClosing = false) {
    if (!FS || Buffer->empty())
      return;
    if (OnClosing)
      return flushAndClear();
    if (fdStream() && Buffer->size() > FlushThreshold)
      flushAndClear();
  }

  void WriteWord(word_t Val) {
    Val = support::endian::byte_swap<word_t, endianness::little>(Val);
    Buffer->append(reinterpret_cast<const char*>(&Val),
                   reinterpret_cast<const char*>(&Val + 1));
  }

  void WriteBytes(ArrayRef<char> Bytes) {
    Buffer->append(Bytes.begin(), Bytes.end());
  }

  raw_fd_stream* fdStream() {
    return dyn_cast_or_null<raw_fd_stream>(FS);
  }

  const raw_fd_stream* fdStream() const {
    return dyn_cast_or_null<raw_fd_stream>(FS);
  }

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
  /// NOTE: \p FlushThreshold's unit is MB.
  OrderedWriter(raw_ostream& Strm, u32 FlushThreshold = 512)
      : Buffer(getInternalBufferFromStream(Strm)),
        FS(!isa<raw_svector_ostream>(Strm) ? &Strm : nullptr),
        FlushThreshold(u64(FlushThreshold) << 20) {}

  /// Convenience constructor for users that start with a vector - avoids
  /// needing to wrap it in a raw_svector_ostream.
  OrderedWriter(SmallVecImpl<char>& Buf) : 
   Buffer(Buf), FS(nullptr), FlushThreshold(0) {}

  ~OrderedWriter() {
    this->FlushToWord();
    this->FlushToFile(/*OnClosing=*/true);
  }

  void FlushToWord() {
    if (BitsInStore) {
      // Switch to "big endian".
      // TODO: Update this for big endian systems.
      WriteWord(exi::byteswap(Store));
      BitsInStore = 0;
      Store = 0;
    }
  }

private:
  virtual void anchor();
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "BitWriter"

class BitWriter final : public OrderedWriter {
  using BaseT = OrderedWriter;

private:
  void anchor() override;
};

#undef DEBUG_TYPE
#define DEBUG_TYPE "ByteWriter"

class ByteWriter final : public OrderedWriter {
  using BaseT = OrderedWriter;

private:
  void anchor() override;
};

#undef DEBUG_TYPE

/// @brief Inline virtual for ordered writers.
// using OrdWriter = Poly<OrderedWriter, BitWriter, ByteWriter>;

} // namespace exi

#undef METHOD_BAG
