//===- exi/Stream/BitStreamWriter.hpp -------------------------------===//
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
/// This file implements the BitStreamWriter class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Option.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <exi/Basic/NBitInt.hpp>
#include <exi/Stream/Stream.hpp>

namespace exi {
class APInt;
class WritableMemoryBuffer;

// TODO: BitStreamWriter
class BitStreamWriter : public BitStreamCommon<MutArrayRef<u8>> {
  // TODO: friend class ...
  using StreamT = MutArrayRef<u8>;
  using BaseT = BitStreamCommon<StreamT>;
public:
  BitStreamWriter(StreamT Stream) : BaseT(Stream) {}
private:
  /// Constructs a `BitStreamWriter` from a `MutArrayRef<char>`.
  /// Used for constructing from a `WritableMemoryBuffer`.
  BitStreamWriter(MutArrayRef<char> Buffer);

public:
  /// Creates a `BitStreamReader` from an `ArrayRef`.
  static BitStreamWriter New(MutArrayRef<u8> Stream) { 
    return BitStreamWriter(Stream);
  }
  /// Maybe creates a `BitStreamReader` from a `MemoryBuffer`.
  static BitStreamWriter New(WritableMemoryBuffer& MB);
  /// Maybe creates a `BitStreamReader` from a `MemoryBuffer*`.
  static Option<BitStreamWriter> New(WritableMemoryBuffer* MB);

private:
	using BaseT::getCurrentByte;

  void writeSingleByte(u8 Byte, i64 Bits = 8);
  void writeBitsImpl(u64 Value, i64 Bits);
  void writeBitsSlow(u64 Value, i64 Bits);

  ExiError writeBitsAP(const APInt& AP, i64 Bits);

public:
  ////////////////////////////////////////////////////////////////////////
  // Writing

  /// Writes a single bit.
  ExiError writeBit(safe_bool Value);
  /// Writes a single byte.
  ExiError writeByte(u8 Byte);

  /// Writes a variable number of bits (max of 64).
  ExiError writeBits64(u64 Value, i64 Bits);

  /// Writes a variable number of bits.
  ExiError writeBits(const APInt& AP);
  /// Writes a variable number of bits (specified).
  ExiError writeBits(const APInt& AP, i64 Bits);

  /// Reads a static number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  template <unsigned Bits>
  ExiError writeBits(ubit<Bits> Value) {
    return writeBits64(Value.data(), Bits);
  }

  /// Writes an array of bytes with an optional length.
  ExiError write(ArrayRef<u8> In, i64 Bytes = -1);
  /// Gets all the written bytes from the buffer.
  MutArrayRef<u8> getWrittenBytes();
};

} // namespace exi
