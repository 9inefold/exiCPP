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
#include "core/Common/APInt.hpp"
#include "core/Common/SmallVec.hpp"
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

ALWAYS_INLINE static APInt
 PeekBitsAPImpl(BitStreamIn thiz, i64 Bits) {
  return thiz.readBitsAP(Bits);
}

//======================================================================//
// BitStreamIn
//======================================================================//

BitStreamIn::BitStreamIn(StrRef Buffer) :
 BitStreamIn::BaseType(GetU8Buffer(Buffer)) { 
}

BitStreamIn BitStreamIn::New(const MemoryBuffer& MB) {
  return BitStreamIn(MB.getBuffer());
}

Option<BitStreamIn> BitStreamIn::New(const MemoryBuffer* MB) {
  if EXI_UNLIKELY(!MB)
    return nullopt;
  return BitStreamIn::New(*MB);
}

//////////////////////////////////////////////////////////////////////////
// Reading

u8 BitStreamIn::getCurrentByte() const {
  exi_invariant(!BaseType::isFull());
  return BaseType::Stream[BaseType::bytePos()];
}

u64 BitStreamIn::peekUnalignedBits() const {
  const u64 Pos = BaseType::nearByteOffset();
  const u64 Curr = getCurrentByte();
  return Curr & (0xff >> Pos);
}

u64 BitStreamIn::readUnalignedBits() {
  const u64 Peeked = peekUnalignedBits();
  this->skip(BaseType::farByteOffsetInclusive());
  return Peeked;
}

bool BitStreamIn::peekBit() const {
  if (BaseType::isFull()) {
    DEBUG_ONLY(dbgs() << "Unable to read bit.\n");
    return 0;
  }

  const u64 Pos = BaseType::farByteOffset() - 1;
  const u64 Curr = getCurrentByte();
  return (Curr >> Pos) & 0x1;
}

/// Peeks without any extra checks. Those are assumed to have already been done.
u64 BitStreamIn::peekBitsImpl(i64 Bits) const {
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

u64 BitStreamIn::peekBits(i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canReadNBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << Bits << " bits.\n");
    return 0;
  }

  return peekBitsImpl(Bits);
}

APInt BitStreamIn::peekBitsAP(i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canReadNBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << Bits << " bits.\n");
    return APInt::getZero(0);
  }

  if (Bits <= 64)
    // Handle small cases.
    return APInt(Bits, peekBitsImpl(Bits), false, true);
  
  // Handle bits over a size of 64.
  // Copies and calls `readBitsAP`.
  return PeekBitsAPImpl(*this, Bits);
}

u64 BitStreamIn::readBits(i64 Bits) {
  const u64 Result = peekBits(Bits);
  this->skip(Bits);
  return Result;
}

/// For reading `APInts` on the slow path.
APInt BitStreamIn::readBitsAPLarge(i64 Bits) {
  exi_invariant(Bits > 64, "Invalid bit size!");
  const i64 OldBits = Bits;

  constexpr i64 kBitsPerWord = APInt::kAPIntBitsPerWord;
  const i64 NumWholeBytes = Bits / kBitsPerWord;
  SmallVec<APInt::WordType> Buf(NumWholeBytes + 1);

  i64 WrittenBytes = 0;
  while (Bits >= kBitsPerWord) {
    const u64 Read = readBits(kBitsPerWord);
    // Read in chunks of 64 bits.
    Buf[WrittenBytes++] = exi::byteswap(Read);
    Bits -= kBitsPerWord;
  }

  if (Bits != 0) {
    u64 Read = readBits(Bits);
    // Get the remaining bits.
    Buf[WrittenBytes++] = Read;
    Bits = 0;
  }

  this->skip(OldBits);
  return APInt(OldBits, Buf);
}

APInt BitStreamIn::readBitsAP(i64 Bits) {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canReadNBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << Bits << " bits.\n");
    return APInt(0, 0, false, true);
  }

  if (Bits <= 64) {
    // Handle small cases.
    APInt AP(Bits, peekBitsImpl(Bits), false, true);
    this->skip(Bits);
    return AP;
  }

  return readBitsAPLarge(Bits);
}

//======================================================================//
// OutBitStream
//======================================================================//
