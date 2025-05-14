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

#include <core/Common/CRTPTraits.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Support/Endian.hpp>
#include <core/Support/MemoryBufferRef.hpp>
#include <exi/Basic/NBitInt.hpp>
#include <exi/Stream/Stream.hpp>

namespace exi {

/// The base for all reader types, allows for a single API.
/// TODO: Investigate if this is actually the best option. It may be better to
/// split up OrderedReader and ChannelReader, and the decoders as well.
class EXI_EMPTY_BASES ReaderBase : public StreamBase {
public:
  using buffer_t = ArrayRef<u8>;
  using proxy_t  = StreamProxy<buffer_t>;
  using BufferT  = buffer_t;
  using ProxyT   = proxy_t;

public:
# define CLASSNAME ReaderBase
# include "D/ReaderMethods.mac"

  /// Reads a single bit.
  virtual ExiResult<bool> readBit() = 0;

  /// Reads a single byte.
  virtual ExiResult<u8> readByte() = 0;

  /// Reads a variable number of bits (max of 64).
  virtual ExiResult<u64> readBits64(size_type Bits) = 0;

  /// Reads an `Unsigned Integer` with a maximum of 8 octets.
  /// See https://www.w3.org/TR/exi/#encodingUnsignedInteger.
  virtual ExiResult<u64> readUInt() = 0;

  /// Returns the type of the current stream.
  virtual StreamKind getStreamKind() const = 0;
  
  /// Make virtual.
  virtual ~ReaderBase() = default;

private:
  virtual void anchor();
};

// TODO: Implement ChannelReader

} // namespace exi
