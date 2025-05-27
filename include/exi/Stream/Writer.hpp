//===- exi/Stream/Writer.hpp ----------------------------------------===//
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
/// This file defines the base for writers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/CRTPTraits.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Support/Endian.hpp>
#include <core/Support/MemoryBufferRef.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/NBitInt.hpp>
#include <exi/Stream/Stream.hpp>

namespace exi {

/// The base for all writer types, allows for a single base API.
class EXI_EMPTY_BASES WriterBase : public StreamBase {
public:
  using buffer_t = SmallVecImpl<char>;
  using proxy_t  = StreamProxy<buffer_t&>;
  using BufferT  = buffer_t;
  using ProxyT   = proxy_t;

public:
  /// Writes a single bit.
  virtual void writeBit(bool Val) = 0;

  /// Writes a single byte.
  virtual void writeByte(u8 Val) = 0;

  /// Writes a variable number of bits (max of 64).
  virtual void writeBits64(u64 Val, size_type Bits) = 0;

  /// Writes an `Unsigned Integer` with a maximum of 8 octets.
  /// See https://www.w3.org/TR/exi/#encodingUnsignedInteger.
  virtual ExiResult<u64> writeUInt() = 0;

  /// Returns the type of the current stream.
  virtual StreamKind getStreamKind() const = 0;
  
  /// Make virtual.
  virtual ~WriterBase() = default;

private:
  virtual void anchor();
};

} // namespace exi
