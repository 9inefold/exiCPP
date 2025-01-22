//===- VMemcpy.cpp --------------------------------------------------===//
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
/// This file implements VMemcpy.
///
//===----------------------------------------------------------------===//

#include <Strings.hpp>
#include <Mem.hpp>

using namespace re;

namespace {

template <usize N>
struct LoadArray {
  u8 Sto[N];
};

} // namespace `anonymous`

#define RE_MEMCPY_FN(name) \
 static inline void name(u8* Dst, \
  const u8* Src, [[maybe_unused]] usize Len = 0)

template <usize BlockSize>
RE_MEMCPY_FN(Copy_block) {
#if HAS_BUILTIN(__builtin_memcpy_inline)
  __builtin_memcpy_inline(Dst, Src, BlockSize);
#else
  using Ty = LoadArray<BlockSize>;
  *ptr_cast<Ty>(Dst) = *ptr_cast<const Ty>(Src);
#endif
}

template <usize BlockSize>
RE_MEMCPY_FN(Copy_last_block) {
  const usize Off = Len - BlockSize;
  tail_return Copy_block<BlockSize>(Dst + Off, Src + Off, Len);
}

template <usize BlockSize>
RE_MEMCPY_FN(Copy_overlap_block) {
  Copy_block<BlockSize>(Dst, Src);
  tail_return Copy_last_block<BlockSize>(Dst, Src, Len);
}

template <usize BlockSize, usize Align = BlockSize>
RE_MEMCPY_FN(Copy_aligned_blocks) {
  // Check if Align = N^2.
  static_assert(is_pow2(Align));
  Copy_block<BlockSize>(Dst, Src);
  // Offset from Last Aligned.
  const usize OffLA = uptr(Dst) & (Align - 1U);
  const usize limit = Len + OffLA - BlockSize;
  for (usize Off = BlockSize; Off < limit; Off += BlockSize)
    Copy_block<BlockSize>(
      assume_aligned<BlockSize>(Dst - OffLA + Off), 
      Src - OffLA + Off);
  // Copy last block.
  tail_return Copy_last_block<BlockSize>(Dst, Src, Len);
}

//======================================================================//
// Implementation
//======================================================================//

RE_MEMCPY_FN(Memcpy_small) {
  if (Len == 1)
    tail_return Copy_block<1>(Dst, Src, Len);
  if (Len == 2)
    tail_return Copy_block<2>(Dst, Src, Len);
  if (Len == 3)
    tail_return Copy_block<3>(Dst, Src, Len);
  // else:
  tail_return Copy_block<4>(Dst, Src, Len);
}

#if __GNUC__
__attribute__((always_inline))
#endif 
RE_MEMCPY_FN(Memcpy_dispatch) {
  if (Len == 0)
    return;
  if (Len < 5)
    tail_return Memcpy_small(Dst, Src, Len);
  if (Len < 8)
    tail_return Copy_overlap_block<4>(Dst, Src, Len);
  if (Len < 16)
    tail_return Copy_overlap_block<8>(Dst, Src, Len);
  if (Len < 32)
    tail_return Copy_overlap_block<16>(Dst, Src, Len);
  if (Len < 64)
    tail_return Copy_overlap_block<32>(Dst, Src, Len);
  if (Len < 128)
    tail_return Copy_overlap_block<64>(Dst, Src, Len);
  // else:
  tail_return Copy_aligned_blocks<32>(Dst, Src, Len);
}

void* re::VMemcpy(void* Dst, const void* Src, usize Len) {
  re_assert((Dst && Src) || !Len);
  if LIKELY(Len != 0)
    Memcpy_dispatch((u8*)Dst, (const u8*)Src, Len);
  return Dst;
}
