//===- exi/Stream/BitStreamOut.cpp ----------------------------------===//
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
#include "core/Support/Logging.hpp"
#include "core/Support/raw_ostream.hpp"

#define DEBUG_TYPE "BitStream"

using namespace exi;

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

ExiError BitStreamOut::writeBit(safe_bool Value) {
  if (BaseType::isFull()) {
    LOG_WARN(dbgs() << "Unable to write bit.\n");
    return ExiError::FULL;
  }

  const u64 Pos = BaseType::farBitOffset() - 1;
  const u8 Curr = (Value.data() << Pos);
  BaseType::getCurrentByte() |= Curr;
  BaseType::skip(1);
  return ExiError::OK;
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
#if READ_FAST_PATH
  if EXI_LIKELY(BaseType::canAccessWords()) {
    Value &= GetIMask(Bits);
    u8* Ptr = BaseType::getCurrentBytePtr();
    const usize BitAlign = BaseType::bitOffset();
    const i64 Off = kBitsPerWord - Bits;
    WriteWordBit(Ptr, (Value << Off), BitAlign);
    BaseType::skip(Bits);
    return;
  }
#endif
  tail_return writeBitsSlow(Value, Bits);
}

ExiError BitStreamOut::writeBits64(u64 Value, i64 Bits) {
  exi_invariant(Bits >= 0 && Bits <= 64, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    LOG_WARN(dbgs() << "Unable to write " << Bits << " bits.\n");
    return ExiError::FULL;
  }

  writeBitsImpl(Value, Bits);
  return ExiError::OK;
}

ExiError BitStreamOut::writeBitsAP(const APInt& AP, i64 Bits) {
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
  return ExiError::OK;
}

ExiError BitStreamOut::writeBits(const APInt& AP) {
  const i64 Bits = AP.getBitWidth();
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    LOG_WARN(dbgs() << "Unable to write " << Bits << " bits.\n");
    return ExiError::FULL;
  }

  if (Bits <= 64) {
    writeBitsImpl(AP.getSingleWord(), Bits);
    return ExiError::OK;
  }

  return writeBitsAP(AP, Bits);
}

ExiError BitStreamOut::writeBits(const APInt& AP, i64 Bits) {
  const i64 NBits = CheckReadWriteSizes(AP.getBitWidth(), Bits);
  if EXI_UNLIKELY(!BaseType::canAccessBits(Bits)) {
    LOG_WARN(dbgs() << "Unable to write " << Bits << " bits.\n");
    return ExiError::FULL;
  }

  if (NBits == AP.getBitWidth()) {
    tail_return writeBitsAP(AP, NBits);
  }

  exi_assert(false, "TODO");
  return ExiError::TODO;
}

ExiError BitStreamOut::writeByte(u8 Byte) {
  if EXI_UNLIKELY(!BaseType::canAccessBits(8)) {
    LOG_WARN(dbgs() << "Unable to write byte.\n");
    return ExiError::FULL;
  }

  writeSingleByte(Byte, 8);
  return ExiError::OK;
}

ExiError BitStreamOut::write(ArrayRef<u8> In, i64 Bytes) {
  const i64 NBytes = CheckReadWriteSizes(In.size(), Bytes);
  if EXI_UNLIKELY(!BaseType::canAccessBytes(NBytes)) {
    LOG_WARN(dbgs() << "Unable to write " << NBytes << " bytes.\n");
    return ExiError::FULL;
  }

  if (NBytes == 0)
    return ExiError::OK;
  
  if (BaseType::isByteAligned()) {
    u8* Ptr = BaseType::getCurrentBytePtr();
    std::memcpy(Ptr, In.data(), NBytes);
    BaseType::skipBytes(NBytes);
    return ExiError::OK;
  }

  for (u8 Byte : In) {
    writeBitsImpl(Byte, kCHAR_BIT);
    BaseType::skipBytes(1);
  }

  return ExiError::OK;
}

MutArrayRef<u8> BitStreamOut::getWrittenBytes() {
  if (BaseType::isFull())
    return BaseType::Stream;
  // Mask current byte.
  const usize N = BaseType::farBitOffsetInclusive();
  const u8 Mask = (0xFF << N);
  BaseType::getCurrentByte() &= Mask;

  // Return the array being written.
  const usize EndPos = BaseType::bytePos();
  if (BaseType::isByteAligned()) {
    // Return extra space if not byte aligned.
    return BaseType::Stream
      .take_front(EndPos + 1);
  }
  return BaseType::Stream.take_front(EndPos);
}
