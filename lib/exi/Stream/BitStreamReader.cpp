//===- exi/Stream/BitStreamReader.cpp -------------------------------===//
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
/// This file implements the BitStreamReader class.
///
//===----------------------------------------------------------------===//

#include "BitStreamCommon.hpp"
#include <exi/Stream/BitStreamReader.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/raw_ostream.hpp>

#define DEBUG_TYPE "BitStream"

using namespace exi;

ALWAYS_INLINE static APInt
 PeekBitsAPImpl(BitStreamReader thiz, i64 Bits) noexcept {
  return thiz.readBits(Bits);
}

BitStreamReader BitStreamReader::New(const MemoryBuffer& MB EXI_LIFETIMEBOUND) {
  return BitStreamReader(MB.getBuffer());
}

Option<BitStreamReader> BitStreamReader::New(const MemoryBuffer* MB) {
  if EXI_UNLIKELY(!MB)
    return nullopt;
  return BitStreamReader::New(*MB);
}

ExiError BitStreamReader::checkPeekBits(i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to peek {} bits.\n", Bits);
    return ExiError::Full(Bits);
  }
  return ExiError::OK;
}

ExiError BitStreamReader::checkReadBits(i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to read {} bits.\n", Bits);
    return ExiError::Full(Bits);
  }
  return ExiError::OK;
}

void BitStreamReader::anchor() {}

//////////////////////////////////////////////////////////////////////////
// Peeking

#if 0
u64 BitStreamReader::peekBitsSlow(i64 Bits) const {
  exi_invariant(Bits > 0);
  u64 Result = 0;
  u64 Pos = BaseT::bytePos();
  if (!BaseT::isByteAligned()) {
    // We really want to avoid this.
    const u64 Peeked = peekUnalignedBits();
    const i64 Off = BaseT::farBitOffset();
    if (Bits < Off)
      // If we need less bits than the retrieved amount, return those.
      return Peeked >> (Off - Bits);
    // Otherwise, continue.
    Result = Peeked;
    Bits -= Off;
    ++Pos;
  }

  /// Our data is aligned here.
  while (Bits >= 8) {
    Result <<= CHAR_BIT;
    Result  |= BaseT::Stream[Pos++];
    Bits -= CHAR_BIT;
  }

  if (Bits > 0) {
    exi_invariant(Bits >= 0, "Negative bit number??");
    u64 Curr = BaseT::Stream[Pos];
    Curr >>= (CHAR_BIT - Bits);
    // Add remaining bits.
    Result <<= Bits;
    Result  |= Curr;
  }

  return Result;
}
#endif

u64 BitStreamReader::peekBitsFast(i64 Bits) const {
  exi_invariant(Bits > 0);
  i64 Offset = 0, Pos = BaseT::bytePos();
  u64 Result = 0;

  // Assumption to possibly unroll loop.
  // TODO: Profile
  exi_assume(Bits <= 64);
  while (Bits >= 8) {
    Result |= u64(BaseT::Stream[Pos]) << Offset;
    Pos += 1;
    Offset += 8;
    Bits -= 8;
  }

  if (Bits != 0) {
    // exi_invariant(Bits >= 0, "Negative bit number??");
    u64 Curr = u64(BaseT::Stream[Pos]) >> (8 - Bits);
    Curr &= 0xFF >> (8 - Bits);
    Result |= Curr << Offset;
  }

  return Result;
}

u64 BitStreamReader::peekBitsSlow(i64 Bits) const {
  exi_invariant(Bits > 0);
  i64 Pos = 0, Offset = 0;
  u64 Result = 0;

  // Assumption to possibly unroll loop.
  // TODO: Profile
  exi_assume(Bits <= 64);
  while (Bits >= 8) {
    Result |= peekByteSlow(Pos) << Offset;
    Pos += 1;
    Offset += 8;
    Bits -= 8;
  }

  if (Bits != 0) {
    u64 Curr = peekByteSlow(Pos) >> (8 - Bits);
    Curr &= 0xFF >> (8 - Bits);
    Result |= Curr << Offset;
  }

  return Result;
}

/// Peeks without any extra checks. Those are assumed to have already been done.
u64 BitStreamReader::peekBitsImpl(i64 Bits) const {
  if (Bits == 0)
    return 0;
#if READ_FAST_PATH
  // Check if whole word can be read.
  // TODO: Profile this!
  if EXI_LIKELY(BaseT::canAccessWords(1)) {
    const u8* Ptr = BaseT::getCurrentBytePtr();
    const usize BitAlign = BaseT::bitOffset();
    const WordT Read = ReadWordBit(Ptr, BitAlign);
    return Read >> (kBitsPerWord - Bits);
  }
#endif
  if (BaseT::isByteAligned())
    tail_return peekBitsFast(Bits);
  else
    tail_return peekBitsSlow(Bits);
}

ExiError BitStreamReader::peekBits(APInt& AP, i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to peek {} bits.\n", Bits);
    AP = APInt::getZero(0);
    return ExiError::Full(Bits);
  }

  if EXI_UNLIKELY(Bits == 0) {
    AP = APInt::getZero(0);
    return ExiError::OK;
  } else if (Bits <= 64) {
    // Handle small cases.
    AP = APInt(Bits, peekBitsImpl(Bits), false, true);
    return ExiError::OK;
  }
  
  // Handle bits over a size of 64.
  // Copies and calls `readBits`.
  AP = PeekBitsAPImpl(*this, Bits);
  return ExiError::OK;
}

APInt BitStreamReader::peekBits(i64 Bits) const {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to peek {} bits.\n", Bits);
    return APInt::getZero(0);
  }

  if EXI_UNLIKELY(Bits == 0)
    return APInt::getZero(0);
  else if (Bits <= 64)
    // Handle small cases.
    return APInt(Bits, peekBitsImpl(Bits), false, true);
  
  // Handle bits over a size of 64.
  // Copies and calls `readBits`.
  return PeekBitsAPImpl(*this, Bits);
}

//////////////////////////////////////////////////////////////////////////
// Reading

/// For reading `APInts` on the slow path.
APInt BitStreamReader::readBitsAPLarge(i64 Bits) {
  // static_assert(false, "Make sure to reverse the array!");
  exi_invariant(Bits > 64, "Invalid bit size!");
  const i64 OldBits = Bits;

  static_assert(APInt::kAPIntBitsPerWord == kBitsPerWord);
  const i64 NumWholeBytes = (Bits + 63) / i64(kBitsPerWord);
  SmallVec<APInt::WordType> Buf;
  Buf.reserve(NumWholeBytes);

  i64 UsedWords = 0;
  while (Bits >= i64(kBitsPerWord)) {
    Buf.push_back(peekBitsImpl(kBitsPerWord));
    BaseT::skip(kBitsPerWord);
    Bits -= i64(kBitsPerWord);
  }

  if (Bits > 0) {
    Buf.push_back(peekBitsImpl(Bits));
    BaseT::skip(Bits);
  }

  exi_invariant(Bits == 0);
  return APInt(OldBits, ArrayRef(Buf));
}

ExiError BitStreamReader::readBits(APInt& AP, i64 Bits) {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to read {} bits.\n", Bits);
    AP = APInt(0, 0, false, true);
    return ExiError::Full(Bits);
  }

  if (Bits <= 64) {
    // Handle small cases.
    AP = APInt(Bits, peekBitsImpl(Bits), false, true);
    BaseT::skip(Bits);
    return ExiError::OK;
  }

  AP = readBitsAPLarge(Bits);
  return ExiError::OK;
}

APInt BitStreamReader::readBits(i64 Bits) {
  exi_invariant(Bits >= 0, "Invalid bit size!");
  if EXI_UNLIKELY(!BaseT::canAccessBits(Bits)) {
    LOG_WARN("Unable to read {} bits.\n", Bits);
    return APInt(0, 0, false, true);
  }

  if (Bits <= 64) {
    // Handle small cases.
    APInt AP(Bits, peekBitsImpl(Bits), false, true);
    BaseT::skip(Bits);
    return AP;
  }

  return readBitsAPLarge(Bits);
}

ExiError BitStreamReader::readUInt(u64& Out) {
  u64 Multiplier = 1;
  u64 Value = 0;
  Out = 0;

  static constexpr i64 OctetsPerWord = kOctetsPerWord;
  for (i64 N = 0; N < OctetsPerWord; ++N) {
    u8 Octet;
    exi_try(readByte(Octet));
    const u64 Lo = Octet & 0b0111'1111;

    Value += (Lo * Multiplier);
    if (Octet & 0b1000'0000) {
      Multiplier *= 128;
      continue;
    }

    Out = Value;
    return ExiError::OK;
  }

  LOG_WARN("uint exceeded {} octets.\n", OctetsPerWord);
  return ErrorCode::kInvalidEXIInput;
}

ExiError BitStreamReader::read(MutArrayRef<u8> Out, i64 Bytes) {
  const i64 NBytes = CheckReadWriteSizes(Out.size(), Bytes);
  if EXI_UNLIKELY(!BaseT::canAccessBytes(NBytes)) {
    LOG_WARN("Unable to read {} bytes.\n", NBytes);
    return ExiError::Full(NBytes * kCHAR_BIT);
  }

  if (NBytes == 0)
    return ExiError::OK;
  
  if (BaseT::isByteAligned()) {
    const u8* Ptr = BaseT::getCurrentBytePtr();
    std::memcpy(Out.data(), Ptr, NBytes);
    BaseT::skipBytes(NBytes);
    return ExiError::OK;
  }

  for (u8& Byte : Out) {
    Byte = peekBitsImpl(kCHAR_BIT);
    BaseT::skipBytes(1);
  }

  return ExiError::OK;
}
