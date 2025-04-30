//===- exi/Stream/Reader.hpp ----------------------------------------===//
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
/// This file defines the base for readers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/StringExtras.hpp>
#include <core/Common/MemoryBufferRef.hpp>
#include <core/Support/Endian.hpp>
#include <exi/Stream2/Stream.hpp>

namespace exi {

/// The base for all reader types, allows for a single API.
/// TODO: Investigate if this is actually the best option. It may be better to
/// split up OrderedReader and ChannelReader, and the decoders as well.
class ReaderBase : public StreamBase {
public:
  using buffer_t = ArrayRef<u8>;
  using proxy_t  = StreamProxy<buffer_t>;
  using BufferT  = buffer_t;
  using ProxyT 	 = proxy_t;

public:
  /// Returns the type of the current stream.
  virtual StreamKind getStreamKind() const = 0;
  
  /// Make virtual.
  virtual ~ReaderBase() = default;

private:
  virtual void anchor();
};

/// The bases for BitReader/ByteReader, which consume data in the order it
/// appears. This allows for a much simpler implementation.
class OrderedReader : public ReaderBase {
protected:
  /// The current stream data.
  buffer_t Stream;
  /// The offset of the current stream in bytes.
  size_t ByteOffset = 0;
  /// The current word, cached data from the stream.
  word_t Store = 0;
  /// This is the number of bits in Store that are valid. This is always from
  /// `[0 ... bitsizeof(size_t) - 1]` inclusive.
  unsigned BitsInStore = 0;

public:
  
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
        CurWord |= u64(WordPtr[Ix]) << (Ix * 8);
    }

    ByteOffset += BytesRead;
    BitsInStore = (BytesRead * 8);
    return ExiError::OK;
  }

private:
  virtual void anchor();
};

// TODO: Implement ChannelReader

} // namespace exi
