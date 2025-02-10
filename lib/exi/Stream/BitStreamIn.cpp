//===- exi/Stream/BitStreamIn.cpp -----------------------------------===//
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

#include "BitStreamCommon.hpp"
#include "core/Support/Debug.hpp"
#include "core/Support/raw_ostream.hpp"

#define DEBUG_TYPE "BitStreamIn"

using namespace exi;

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

u64 BitStreamIn::peekUnalignedBits() const {
  const u64 Pos = BaseType::bitOffset();
  const u64 Curr = getCurrentByte();
  return Curr & (0xFF >> Pos);
}

u64 BitStreamIn::readUnalignedBits() {
  const u64 Peeked = peekUnalignedBits();
  BaseType::skip(BaseType::farBitOffsetInclusive());
  return Peeked;
}

safe_bool BitStreamIn::peekBit() const {
  if (BaseType::isFull()) {
    DEBUG_ONLY(dbgs() << "Unable to read bit.\n");
    return 0;
  }

  const u64 Pos = BaseType::farBitOffset() - 1;
  const u64 Curr = getCurrentByte();
  return safe_bool::FromBits((Curr >> Pos) & 0x1);
}

u64 BitStreamIn::peekBitsSlow(i64 Bits) const {
  exi_invariant(Bits > 0);
  u64 Result = 0;
  u64 Pos = BaseType::bytePos();
  if (!BaseType::isByteAligned()) {
    // We really want to avoid this.
    const u64 Peeked = peekUnalignedBits();
    const i64 Off = BaseType::farBitOffset();
    if (Bits < Off)
      // If we need less bits than the retrieved amount, return those.
      // TODO: Check this
      return Peeked >> (Off - Bits);
    // Otherwise, continue.
    Result = Peeked;
    Bits -= Off;
    ++Pos;
  }

  /// Our data is aligned here.
  while (Bits >= 8) {
    Result <<= CHAR_BIT;
    Result  |= BaseType::Stream[Pos++];
    Bits -= CHAR_BIT;
  }

  if (Bits > 0) {
    exi_invariant(Bits >= 0, "Negative bit number??");
    u64 Curr = BaseType::Stream[Pos];
    Curr >>= (CHAR_BIT - Bits);
    // Add remaining bits.
    Result <<= Bits;
    Result  |= Curr;
  }

  // TODO: Check if we need ByteSwap(Result)
  return Result;
}

/// Peeks without any extra checks. Those are assumed to have already been done.
u64 BitStreamIn::peekBitsImpl(i64 Bits) const {
  if EXI_UNLIKELY(Bits == 0)
    return 0;
#if READ_FAST_PATH
  // Check if whole word can be read.
  // TODO: Profile this!
  if EXI_LIKELY(BaseType::canAccessWords(1)) {
    const u8* Ptr = BaseType::getCurrentBytePtr();
    const usize BitAlign = BaseType::bitOffset();
    const WordT Read = ReadWordBit(Ptr, BitAlign);
    return Read >> (kBitsPerWord - Bits);
  }
#endif
  tail_return peekBitsSlow(Bits);
}

u64 BitStreamIn::peekBits64(i64 Bits) const {
  exi_invariant(Bits >= 0 && Bits <= 64, "Invalid bit size!");
  if EXI_UNLIKELY(Bits > 64)
    Bits = 64;
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << Bits << " bits.\n");
    return 0;
  }

  tail_return peekBitsImpl(Bits);
}

APInt BitStreamIn::peekBits(i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << Bits << " bits.\n");
    return APInt::getZero(0);
  }

  if (Bits <= 64)
    // Handle small cases.
    return APInt(Bits, peekBitsImpl(Bits), false, true);
  
  // Handle bits over a size of 64.
  // Copies and calls `readBits`.
  return PeekBitsAPImpl(*this, Bits);
}

u8 BitStreamIn::peekByte() const {
  if EXI_UNLIKELY(!BaseType::canAccessBits(8)) {
    DEBUG_ONLY(dbgs() << "Unable to read byte.\n");
    return 0;
  }
  return peekBitsImpl(8);
}

u64 BitStreamIn::readBits64(i64 Bits) {
  const u64 Result = peekBits64(Bits);
  BaseType::skip(Bits);
  return Result;
}

/// For reading `APInts` on the slow path.
APInt BitStreamIn::readBitsAPLarge(i64 Bits) {
  // static_assert(false, "Make sure to reverse the array!");
  exi_invariant(Bits > 64, "Invalid bit size!");
  const i64 OldBits = Bits;

  static_assert(APInt::kAPIntBitsPerWord == kBitsPerWord);
  const i64 NumWholeBytes = (Bits + 63) / kBitsPerWord;
  SmallVec<APInt::WordType> Buf(NumWholeBytes);

  i64 SpareWords = Buf.size();
  if (i64 Remainder = Bits % kBitsPerWord; Remainder != 0) {
    const u64 Read = peekBitsImpl(Remainder);
    BaseType::skip(Remainder);
    Buf[--SpareWords] = Read;
    Bits -= Remainder;
  }

  while (Bits > 0) {
    const u64 Read = peekBitsImpl(kBitsPerWord);
    BaseType::skip(kBitsPerWord);
    // Read in chunks of 64 bits.
    Buf[--SpareWords] = Read;
    Bits -= kBitsPerWord;
  }

  exi_invariant(Bits == 0);
  return APInt(OldBits, ArrayRef(Buf).drop_front(SpareWords));
}

APInt BitStreamIn::readBits(i64 Bits) {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << Bits << " bits.\n");
    return APInt(0, 0, false, true);
  }

  if (Bits <= 64) {
    // Handle small cases.
    APInt AP(Bits, peekBitsImpl(Bits), false, true);
    BaseType::skip(Bits);
    return AP;
  }

  return readBitsAPLarge(Bits);
}

void BitStreamIn::read(MutArrayRef<u8> Bytes, i64 Len) {
  const i64 NBytes = CheckReadWriteSizes(Bytes.size(), Len);
  if EXI_UNLIKELY(!BaseType::canAccessBytes(NBytes)) {
    DEBUG_ONLY(dbgs() << "Unable to read " << NBytes << " bytes.\n");
    return;
  }

  if (Bytes.empty())
    return;
  
  if (BaseType::isByteAligned()) {
    const u8* Ptr = BaseType::getCurrentBytePtr();
    std::memcpy(Bytes.data(), Ptr, NBytes);
    BaseType::skipBytes(NBytes);
    return;
  }

  for (u8& Byte : Bytes) {
    Byte = peekBitsImpl(kCHAR_BIT);
    BaseType::skipBytes(1);
  }
}
