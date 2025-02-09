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
#include "core/Common/STLExtras.hpp"
#include "core/Support/Debug.hpp"
#include "core/Support/Endian.hpp"
#include "core/Support/MemoryBuffer.hpp"
#include "core/Support/raw_ostream.hpp"

#define DEBUG_TYPE "BitStream"
#define READ_FAST_PATH 1

using namespace exi;

using WordT = BitStreamBase::WordType;
static constexpr auto kEndianness = endianness::big;

template <typename IntT = WordT>
ALWAYS_INLINE static constexpr IntT GetIMask(i64 Bits) noexcept {
  constexpr unsigned kBits = bitsizeof_v<IntT>;
  if EXI_UNLIKELY(Bits <= 0)
    return 0;
  return ~IntT(0u) >> (kBits - unsigned(Bits));
}

[[nodiscard]] static inline WordT ByteSwap(WordT I) noexcept {
  using namespace exi::support;
  return endian::byte_swap<WordT, kEndianness>(I);
}

[[nodiscard]] static inline WordT
 ReadWordBit(const u8* Data, u64 StartBit) noexcept {
  using namespace exi::support;
  return endian::readAtBitAlignment<
    WordT, kEndianness, support::unaligned>(Data, StartBit);
}

static inline void WriteWordBit(u8* Data, WordT I, u64 StartBit) noexcept {
  using namespace exi::support;
  return endian::writeAtBitAlignment<
    WordT, kEndianness, support::unaligned>(Data, I, StartBit);
}

//////////////////////////////////////////////////////////////////////////

static ArrayRef<u8> GetU8Buffer(StrRef Buffer) {
  return ArrayRef<u8>(
    reinterpret_cast<const u8*>(Buffer.begin()),
    reinterpret_cast<const u8*>(Buffer.end()));
}

static MutArrayRef<u8> GetU8Buffer(MutArrayRef<char> Buffer) {
  return MutArrayRef<u8>(
    reinterpret_cast<u8*>(Buffer.begin()),
    reinterpret_cast<u8*>(Buffer.end()));
}

static inline i64 CheckReadWriteSizes(const i64 NMax, i64& Len) {
  const i64 NReads = (Len >= 0) ? Len : NMax;
  if EXI_UNLIKELY(NReads > NMax) {
    exi_invariant(NReads <= NMax, "Read/Write exceeds length!");
    return (Len = NMax);
  }

  return (Len = NReads);
}

ALWAYS_INLINE static APInt
 PeekBitsAPImpl(BitStreamIn thiz, i64 Bits) {
  return thiz.readBits(Bits);
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
    Result <<= CHAR_BIT;
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
  if EXI_LIKELY(BaseType::canAccessWords()) {
    const u8* Ptr = BaseType::getCurrentBytePtr();
    const usize BitAlign = BaseType::bitOffset();
    const WordT Read = ReadWordBit(Ptr, BitAlign);
    return Read & GetIMask(Bits);
  }
#endif
  tail_return peekBitsSlow(Bits);
}

u64 BitStreamIn::peekBits64(i64 Bits) const {
  exi_invariant(Bits >= 0 && Bits <= 64, "Invalid bit size!");
  Bits = std::min(Bits, 64ll);
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

//======================================================================//
// OutBitStream
//======================================================================//

BitStreamOut::BitStreamOut(MutArrayRef<char> Buffer) :
 BitStreamOut::BaseType(GetU8Buffer(Buffer)) { 
}

BitStreamOut BitStreamOut::New(WritableMemoryBuffer& MB) {
  return BitStreamOut(MB.getBuffer());
}

Option<BitStreamOut> BitStreamOut::New(WritableMemoryBuffer* MB) {
  if EXI_UNLIKELY(!MB)
    return nullopt;
  return BitStreamOut::New(*MB);
}

//////////////////////////////////////////////////////////////////////////
// Writing

void BitStreamOut::writeBit(safe_bool Value) {
  if (BaseType::isFull()) {
    DEBUG_ONLY(dbgs() << "Unable to write bit.\n");
    return;
  }

  const u64 Pos = BaseType::farBitOffset() - 1;
  const u8 Curr = (Value.data() << Pos);
  BaseType::getCurrentByte() |= Curr;
  BaseType::skip(1);
}

void BitStreamOut::writeSingleByte(u8 Byte, i64 Bits) {
#ifdef EXPENSIVE_CHECKS
  exi_assert(exi::isUIntN(Bits, Byte));
#endif
  //
  // Writing a single byte is simpler than multiple, but still not simple.
  // Here's some visual examples:
  //
  // Stream[3], int7:
  // 11110000 00000000
  //     1111 111
  //
  // Stream[2], int3:
  // 11100000 00000000
  //    111
  // Stream[4], int3:
  // 11111000 00000000
  //      111
  //
  const i64 FarBits = BaseType::farBitOffset();
  const i64 FirstWrite = FarBits - Bits;
  BaseType::getCurrentByte()
    |= SignAwareSHL(Byte, FirstWrite);
  
  // Exit early if our value fits in neatly.
  if (FarBits >= Bits) {
    BaseType::skip(Bits);
    return;
  }

  // Otherwise, handle trailing bits.
  BaseType::align();
  const i64 SecondWrite = -FirstWrite;
  // Byte &= GetIMask<u8>(SecondWrite);
  BaseType::getCurrentByte()
    |= (Byte << (kCHAR_BIT - SecondWrite));
  BaseType::skip(SecondWrite);
}

void BitStreamOut::writeBitsSlow(u64 Value, i64 Bits) {
  //
  // Writing bits is more complex than reading, as it uses a different endianness. 
  // For example, let's say words are u32, and we have 0x12345678.
  // If we want to write 20 bits, we have to mask and then byteswap:
  //
  // [12][34][56][78] & MASK ->
  // [00][04][56][78] > FLIP ->
  // [78][56][04][00]
  //
  // We then shift over by the unused bytes, and write the partial bits.
  // Once we have that we can shift over to the full bits,
  // and then write the last 2 bytes easily.
  //
  // [78][56][04][00] > SHIFT ->
  // [00][78][56][04] > WRITE4 + SHIFT ->
  // [00][00][78][56] > WRITE8 + SHIFT ->
  // [00][00][00][78] > WRITE8
  //
  Value &= GetIMask(Bits);
  Value = ByteSwap(Value);

  const i64 Bytes = (Bits - 1) / kCHAR_BIT + 1;
  Value >>= (kWordSize - Bytes) * kCHAR_BIT;

  if (i64 Remainder = Bits % kCHAR_BIT; Remainder != 0) {
    const u8 Leading = u8(Value & 0xFF);
    writeSingleByte(Leading, Remainder);
    Bits -= Remainder;
    Value >>= kCHAR_BIT;
  }

  while (Bits > 0) {
    writeSingleByte(Value & 0xFF);
    Bits -= kCHAR_BIT;
    Value >>= kCHAR_BIT;
  }
}

void BitStreamOut::writeBitsImpl(u64 Value, i64 Bits) {
  if EXI_UNLIKELY(Bits == 0)
    return;
#if READ_FAST_PATH && 0
  if EXI_LIKELY(BaseType::canAccessWords()) {
    Value &= GetIMask(Bits);
    u8* Ptr = BaseType::getCurrentBytePtr();
    const usize BitAlign = BaseType::bitOffset();
    const i64 Off = i64(kBitsPerWord) - Bits;
    WriteWordBit(Ptr, (Value << Off), BitAlign);
    BaseType::skip(Bits);
    return;
  }
#endif
  tail_return writeBitsSlow(Value, Bits);
}

void BitStreamOut::writeBits64(u64 Value, i64 Bits) {
  exi_invariant(Bits >= 0 && Bits <= 64, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to write " << Bits << " bits.\n");
    return;
  }

  tail_return writeBitsImpl(Value, Bits);
}

void BitStreamOut::writeBitsAP(const APInt& AP, i64 Bits) {
  ArrayRef<WordType> Words = AP.getData();
  if (i64 Remainder = Bits % kBitsPerWord; Remainder != 0) {
    writeBitsImpl(Words.back(), Remainder);
    Words = Words.drop_back();
    Bits -= Remainder;
  }

  for (WordType Word : exi::reverse(Words)) {
    writeBitsImpl(Word, kBitsPerWord);
    Bits -= kBitsPerWord;
  }

  exi_invariant(Bits == 0);
}

void BitStreamOut::writeBits(const APInt& AP) {
  const i64 Bits = AP.getBitWidth();
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to write " << Bits << " bits.\n");
    return;
  }

  if (Bits <= 64) {
    writeBitsImpl(AP.getSingleWord(), Bits);
    return;
  }

  writeBitsAP(AP, Bits);
}

void BitStreamOut::writeBits(const APInt& AP, i64 Bits) {
  const i64 NBits = CheckReadWriteSizes(AP.getBitWidth(), Bits);
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    DEBUG_ONLY(dbgs() << "Unable to write " << Bits << " bits.\n");
    return;
  }

  if (NBits == AP.getBitWidth()) {
    tail_return writeBitsAP(AP, NBits);
  }

  exi_assert(false, "TODO");
}

void BitStreamOut::write(ArrayRef<u8> Bytes, i64 Len) {
  const i64 NBytes = CheckReadWriteSizes(Bytes.size(), Len);
  if EXI_UNLIKELY(!BaseType::canAccessBytes(NBytes)) {
    DEBUG_ONLY(dbgs() << "Unable to write " << NBytes << " bytes.\n");
    return;
  }

  if (Bytes.empty())
    return;
  
  if (BaseType::isByteAligned()) {
    u8* Ptr = BaseType::getCurrentBytePtr();
    std::memcpy(Ptr, Bytes.data(), NBytes);
    BaseType::skipBytes(NBytes);
    return;
  }

  for (u8 Byte : Bytes) {
    writeBitsImpl(Byte, kCHAR_BIT);
    BaseType::skipBytes(1);
  }
}
