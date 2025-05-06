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
#include <core/Common/Fundamental.hpp>
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

class ReaderBase;
class OrderedReader;
class ChannelReader;

class BitReader;
class ByteReader;
class BlockReader;
// TODO: DeflateReader

class WriterBase;

class BitWriter;
class ByteWriter;
class BlockWriter;
// TODO: DeflateWriter

/// The base for BitStream types. Provides common definitions.
struct StreamBase {
  enum StreamKind {
    SK_Bit,     // Bit Packed
    SK_Byte,    // Byte Packed
    SK_Block,   // Precompression
    SK_Deflate, // Compression
  };

  using size_type = usize;
  using word_t    = u64;
  using WordType  = word_t;

  static constexpr size_type kWordSize = sizeof(WordType);
  static constexpr size_type kBitsPerWord = bitsizeof_v<WordType>;

  static constexpr size_type kMaxCapacityBytes = max_v<size_type> / kCHAR_BIT;
  static constexpr size_type kMask = (kCHAR_BIT - 1ull);
};

/// A proxy type for passing around consumed bits. Useful when swapping
/// between stream types (generally between the header and body).
template <class BufferT> class StreamProxy {
public:
  BufferT Bytes;
  StreamBase::word_t NBits;
public:
  StreamProxy(BufferT Bytes, u64 Bits) :
   Bytes(Bytes), NBits(Bits) {
  }
};

} // namespace exi
