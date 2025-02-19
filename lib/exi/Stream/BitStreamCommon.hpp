//===- exi/Stream/BitStreamCommon.hpp -------------------------------===//
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
/// This file defines the common interface for stream operations.
///
//===----------------------------------------------------------------===//

#include "exi/Stream/BitStream.hpp"
#include "core/Common/APInt.hpp"
#include "core/Common/SmallVec.hpp"
#include "core/Common/StrRef.hpp"
#include "core/Common/STLExtras.hpp"
#include "core/Support/Endian.hpp"
#include "core/Support/MemoryBuffer.hpp"

#define READ_FAST_PATH 0

namespace exi {

using WordT = StreamBase::WordType;
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

static ArrayRef<u8> GetU8Buffer(StrRef Buffer) noexcept {
  return ArrayRef<u8>(Buffer.bytes_begin(), Buffer.size());
}

static MutArrayRef<u8> GetU8Buffer(MutArrayRef<char> Buffer) noexcept {
  return MutArrayRef<u8>(
    reinterpret_cast<u8*>(Buffer.begin()),
    Buffer.size());
}

static inline i64 CheckReadWriteSizes(const i64 NMax, i64& Len) noexcept {
  const i64 NReads = (Len >= 0) ? Len : NMax;
  if EXI_UNLIKELY(NReads > NMax) {
    exi_invariant(NReads <= NMax, "Read/Write exceeds length!");
    return (Len = NMax);
  }

  return (Len = NReads);
}

ALWAYS_INLINE static APInt
 PeekBitsAPImpl(BitStreamIn thiz, i64 Bits) noexcept {
  return thiz.readBits(Bits);
}

} // namespace exi
