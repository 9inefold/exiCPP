//===- exi/Stream/BitStream.cpp -------------------------------------===//
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

#include "exi/Stream/BitStream.hpp"
#include "core/Common/StrRef.hpp"
#include "core/Support/Debug.hpp"
#include "core/Support/MemoryBuffer.hpp"
#include "core/Support/raw_ostream.hpp"

#define DEBUG_TYPE "BitStream"

using namespace exi;

static ArrayRef<u8> GetU8Buffer(StrRef Buffer) {
  return ArrayRef<u8>(
    reinterpret_cast<const u8*>(Buffer.begin()),
    reinterpret_cast<const u8*>(Buffer.end()));
}

//======================================================================//
// InBitStream
//======================================================================//

InBitStream::InBitStream(StrRef Buffer) :
 InBitStream::BaseType(GetU8Buffer(Buffer)) { 
}

InBitStream InBitStream::New(const MemoryBuffer& MB) {
  return InBitStream(MB.getBuffer());
}

Option<InBitStream> InBitStream::New(const MemoryBuffer* MB) {
  if EXI_UNLIKELY(!MB)
    return nullopt;
  return InBitStream::New(*MB);
}

//////////////////////////////////////////////////////////////////////////
// Reading

u8 InBitStream::getCurrentByte() const {
  exi_invariant(!BaseType::isFull());
  return BaseType::Stream[BaseType::bytePos()];
}

u64 InBitStream::peekUnalignedBits() const {
  const u64 Pos = BaseType::nearByteOffset();
  const u64 Curr = getCurrentByte();
  return Curr & (0xff >> Pos);
}

bool InBitStream::peekBit() const {
  if (BaseType::isFull()) {
    DEBUG_ONLY(dbgs() << "Unable to read bit.\n");
    return 0;
  }

  const u64 Pos = BaseType::farByteOffset() - 1;
  const u64 Curr = getCurrentByte();
  return (Curr >> Pos) & 0x1;
}

u64 InBitStream::peekBits(i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canReadNBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << Bits << " bits.\n");
    return 0;
  }

  if (Bits == 0)
    return 0;
  
  u64 Result = 0;
  u64 Pos = BaseType::bytePos();
  if (!BaseType::isByteAligned()) {
    // We really want to avoid this.
    const u64 Peeked = peekUnalignedBits();
    const i64 Off = BaseType::farByteOffset();
    if (Bits < Off)
      // If we need less bits than the retrieved amount, return those.
      return Peeked >> (Off - Bits);
    // Otherwise, continue.
    Result = Peeked;
    Bits -= Off;
    ++Pos;
  }

  /// Our data is aligned here.
  while (Bits > 7) {
    Result <<= CHAR_BIT;
    Result  |= BaseType::Stream[Pos++];
    Bits -= CHAR_BIT;
  }

  if (Bits > 0) {
    exi_invariant(Bits >= 0, "Negative bit number??");
    u64 Curr = BaseType::Stream[Pos];
    Curr >>= (CHAR_BIT - Bits);
    // Add remaining bits.
    Result <<= CHAR_BIT;
    Result  |= Curr;
  }

  return Result;
}

u64 InBitStream::readBits(i64 Bits) {
  const u64 Result = peekBits(Bits);
  BaseType::Position += Bits;
  return Result;
}

//======================================================================//
// OutBitStream
//======================================================================//
