//===- Memcpy.cpp ---------------------------------------------------===//
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
/// This file implements memcpy. Normally the program calls out to an external
/// implementation, but this adds some extra overhead for the indirect call,
/// and may not be the optimal implementation. By replacing it, we increase code
/// size in exchange for a (potentially) more efficient runtime.
///
//===----------------------------------------------------------------===//

// TODO: Check on linux
#if defined(__GNUC__) && defined(_WIN32)

#include <Common/Fundamental.hpp>

using namespace exi;

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
#if EXI_HAS_BUILTIN(__builtin_memcpy_inline)
  __builtin_memcpy_inline(Dst, Src, BlockSize);
#else
  using Ty = LoadArray<BlockSize>;
  *((Ty*)Dst) = *((const Ty*)Src);
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

template <usize Align, typename T>
static ALWAYS_INLINE EXI_FLATTEN T* exi_assume_aligned(T* Ptr) noexcept {
  return static_cast<T*>(__builtin_assume_aligned(Ptr, Align));
}

template <usize BlockSize, usize Align = BlockSize>
RE_MEMCPY_FN(Copy_aligned_blocks) {
  // Check if Align = N^2.
  static_assert((Align & (Align - 1)) == 0);
  Copy_block<BlockSize>(Dst, Src);
  // Offset from Last Aligned.
  const usize OffLA = uptr(Dst) & (Align - 1U);
  const usize limit = Len + OffLA - BlockSize;
  for (usize Off = BlockSize; Off < limit; Off += BlockSize)
    Copy_block<BlockSize>(
      exi_assume_aligned<BlockSize>(Dst - OffLA + Off), 
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

extern "C" {

// TODO: Do further optimization tests, given this had a ~24% speedup
__declspec(dllexport) EXI_FLATTEN extern inline void* memcpy(
 void* __restrict Dst, const void* __restrict Src, usize Len) {
  // exi_invariant((Dst && Src) || !Len);
  Memcpy_dispatch((u8*)Dst, (const u8*)Src, Len);
  return Dst;
}

} // extern "C"

#endif // __GNUC__
