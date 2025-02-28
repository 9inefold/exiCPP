//===- exi/Stream/BitStreamWriter.cpp -------------------------------===//
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

#include "BitStreamCommon.hpp"
#include <exi/Stream/BitStreamWriter.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/raw_ostream.hpp>

#define DEBUG_TYPE "BitStream"

using namespace exi;

BitStreamWriter::BitStreamWriter(MutArrayRef<char> Buffer) :
 BitStreamWriter::BaseT(GetU8Buffer(Buffer)) { 
}

BitStreamWriter BitStreamWriter::New(WritableMemoryBuffer& MB) {
  return BitStreamWriter(MB.getBuffer());
}

Option<BitStreamWriter> BitStreamWriter::New(WritableMemoryBuffer* MB) {
  if EXI_UNLIKELY(!MB)
    return nullopt;
  return BitStreamWriter::New(*MB);
}

//////////////////////////////////////////////////////////////////////////
// Writing

ExiError BitStreamWriter::writeBit(safe_bool Value) {
  if (BaseT::isFull()) {
    LOG_WARN("Unable to write bit.\n");
    return ExiError::Full(1);
  }

  const u64 Pos = BaseT::farBitOffset() - 1;
  const u8 Curr = (Value.data() << Pos);
  BaseT::getCurrentByte() |= Curr;
  BaseT::skip(1);
  return ExiError::OK;
}

void BitStreamWriter::writeSingleByte(u8 Byte, i64 Bits) {
  exi_expensive_invariant(exi::isUIntN(Bits, Byte));

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
  const i64 FarBits = BaseT::farBitOffset();
  const i64 FirstWrite = FarBits - Bits;
  BaseT::getCurrentByte()
    |= SignAwareSHL(Byte, FirstWrite);
  
  // Exit early if our value fits in neatly.
  if (FarBits >= Bits) {
    BaseT::skip(Bits);
    return;
  }

  // Otherwise, handle trailing bits.
  BaseT::align();
  const i64 SecondWrite = -FirstWrite;
  // Byte &= GetIMask<u8>(SecondWrite);
  BaseT::getCurrentByte()
    |= (Byte << (kCHAR_BIT - SecondWrite));
  BaseT::skip(SecondWrite);
}

#if 0
void BitStreamWriter::writeBitsSlow(u64 Value, i64 Bits) {
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
#endif

void BitStreamWriter::writeBitsSlow(u64 Value, i64 Bits) {
  // Assumption to possibly unroll loop.
  exi_assume(Bits <= 64);
  while (Bits > 0) {
    writeSingleByte(Value & 0xFF);
    Bits -= CHAR_BIT;
    Value >>= CHAR_BIT;
  }
}

void BitStreamWriter::writeBitsImpl(u64 Value, i64 Bits) {
  if EXI_UNLIKELY(Bits == 0)
    return;
#if READ_FAST_PATH
  if EXI_LIKELY(BaseT::canAccessWords()) {
    Value &= GetIMask(Bits);
    u8* Ptr = BaseT::getCurrentBytePtr();
    const usize BitAlign = BaseT::bitOffset();
    const i64 Off = kBitsPerWord - Bits;
    WriteWordBit(Ptr, (Value << Off), BitAlign);
    BaseT::skip(Bits);
    return;
  }
#endif
  tail_return writeBitsSlow(Value, Bits);
}

ExiError BitStreamWriter::writeBits64(u64 Value, i64 Bits) {
  exi_invariant(Bits >= 0 && Bits <= 64, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to write {} bits.\n", Bits);
    return ExiError::Full(Bits);
  }

  writeBitsImpl(Value, Bits);
  return ExiError::OK;
}

ExiError BitStreamWriter::writeBitsAP(const APInt& AP, i64 Bits) {
  const u64* Ptr = AP.getRawData();
  while (Bits >= i64(kBitsPerWord)) {
    writeBitsImpl(*Ptr, i64(kBitsPerWord));
    Bits -= i64(kBitsPerWord);
    Ptr += 1;
  }

  if (Bits > 0)
    writeBitsImpl(*Ptr, Bits);

  return ExiError::OK;
}

ExiError BitStreamWriter::writeBits(const APInt& AP) {
  const i64 Bits = AP.getBitWidth();
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to write {} bits.\n", Bits);
    return ExiError::Full(Bits);
  }

  if (Bits <= 64) {
    writeBitsImpl(AP.getSingleWord(), Bits);
    return ExiError::OK;
  }

  return writeBitsAP(AP, Bits);
}

ExiError BitStreamWriter::writeBits(const APInt& AP, i64 Bits) {
  const i64 NBits = CheckReadWriteSizes(AP.getBitWidth(), Bits);
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to write {} bits.\n", Bits);
    return ExiError::Full(Bits);
  }

  if (NBits == AP.getBitWidth()) {
    tail_return writeBitsAP(AP, NBits);
  }

  exi_assert(false, "TODO");
  return ExiError::TODO;
}

ExiError BitStreamWriter::writeByte(u8 Byte) {
  if EXI_UNLIKELY(!BaseT::canAccessBits(8)) {
    LOG_WARN("Unable to write byte.\n");
    return ExiError::Full(8);
  }

  writeSingleByte(Byte, 8);
  return ExiError::OK;
}

ExiError BitStreamWriter::write(ArrayRef<u8> In, i64 Bytes) {
  const i64 NBytes = CheckReadWriteSizes(In.size(), Bytes);
  if EXI_UNLIKELY(!BaseT::canAccessBytes(NBytes)) {
    LOG_WARN("Unable to write {} bytes.\n", NBytes);
    return ExiError::Full(NBytes * kCHAR_BIT);
  }

  if (NBytes == 0)
    return ExiError::OK;
  
  if (BaseT::isByteAligned()) {
    u8* Ptr = BaseT::getCurrentBytePtr();
    std::memcpy(Ptr, In.data(), NBytes);
    BaseT::skipBytes(NBytes);
    return ExiError::OK;
  }

  for (u8 Byte : In) {
    writeBitsImpl(Byte, kCHAR_BIT);
    BaseT::skipBytes(1);
  }

  return ExiError::OK;
}

MutArrayRef<u8> BitStreamWriter::getWrittenBytes() {
  if (BaseT::isFull())
    return BaseT::Stream;
  // Mask current byte.
  const usize N = BaseT::farBitOffsetInclusive();
  const u8 Mask = (0xFF << N);
  BaseT::getCurrentByte() &= Mask;

  // Return the array being written.
  const usize EndPos = BaseT::bytePos();
  if (BaseT::isByteAligned()) {
    // Return extra space if not byte aligned.
    return BaseT::Stream
      .take_front(EndPos + 1);
  }
  return BaseT::Stream.take_front(EndPos);
}
