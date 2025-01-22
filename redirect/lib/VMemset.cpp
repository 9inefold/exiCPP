//===- VMemset.cpp --------------------------------------------------===//
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
#include <immintrin.h>

#if !defined(__clang__) && defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

using namespace re;

#ifdef __GNUC__
# define RE_DEF_VECTOR(ty, n) ty \
 __attribute__((__vector_size__(n), __aligned__(n)))
#else
# define RE_DEF_VECTOR(ty, n) AlignArray<ty, n, n>
#endif

namespace {

template <typename T, usize Size, usize Align = alignof(T)>
struct alignas(Align) AlignArray {
  using type = T;
  static constexpr usize size = Size;
public:
  T Sto[Size];
};

using Gv128 = RE_DEF_VECTOR(u8, 16);
using Gv256 = RE_DEF_VECTOR(u8, 32);
using Gv512 = RE_DEF_VECTOR(u8, 64);

template <typename> struct is_vector : std::false_type { };
template <> struct is_vector<Gv128>  : std::true_type  { };
template <> struct is_vector<Gv256>  : std::true_type  { };
template <> struct is_vector<Gv512>  : std::true_type  { };

template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

#if defined(__AVX512F__)
  inline constexpr usize vector_size_v = 64;
  using v128 = Gv128;
  using v256 = Gv256;
  using v512 = Gv512;
#elif defined(__AVX__)
  inline constexpr usize vector_size_v = 32;
  using v128 = Gv128;
  using v256 = Gv256;
  using v512 = AlignArray<v256, 2>;
#elif defined(__SSE2__)
  inline constexpr usize vector_size_v = 16;
  using v128 = Gv128;
  using v256 = AlignArray<v128, 2>;
  using v512 = AlignArray<v128, 4>;
#else
  inline constexpr usize vector_size_v = 8;
  using v128 = AlignArray<u64, 2>;
  using v256 = AlignArray<u64, 4>;
  using v512 = AlignArray<u64, 8>;
#endif

} // namespace `anonymous`

template <typename T>
static inline constexpr void Memset_store(u8* Dst, T Val) noexcept {
  if constexpr (std::is_scalar_v<T> || is_vector_v<T>) {
#if HAS_BUILTIN(__builtin_memcpy_inline)
  __builtin_memcpy_inline(Dst, &Val, sizeof(Val));
#else
  *ptr_cast<T>(Dst) = Val;
#endif
  } else /*AlignArray*/ {
    using U = typename T::type;
    for (usize Ix = 0; Ix < T::size; ++Ix)
      Memset_store<U>(Dst + (Ix * sizeof(U)), Val.Sto[Ix]);
  }
}

template <typename T>
inline constexpr T Memset_splat(u8 Val) {
  if constexpr (std::is_scalar_v<T>) {
    return T(~0) / T(0xFF) * T(Val);
  } else if constexpr(is_vector_v<T>) {
    T out;
    for (usize Ix = 0; Ix < sizeof(T); ++Ix)
      out[Ix] = Val;
    return out;
  } else /*AlignArray*/ {
    using U = typename T::type;
    const auto u = Memset_splat<U>(Val);
    T out;
    for (usize Ix = 0; Ix < T::size; ++Ix)
      out.Sto[Ix] = u;
    return out;
  }
}

#define RE_MEMSET_FN(name) \
 inline void name(u8* Dst, \
  u8 Val, [[maybe_unused]] usize Len = 0)

template <typename BlockType>
RE_MEMSET_FN(Memset_block) {
  Memset_store<BlockType>(Dst, 
    Memset_splat<BlockType>(Val));
}

template <typename BlockType, typename...Next>
RE_MEMSET_FN(Memset_block_seq) {
  Memset_block<BlockType>(Dst, Val);
  if constexpr (sizeof...(Next) > 0) {
    tail_return Memset_block_seq<Next...>(
      Dst + sizeof(BlockType), Val, Len);
  }
}

template <typename BlockType>
RE_MEMSET_FN(Memset_last_block) {
  tail_return Memset_block<BlockType>(
    Dst + Len - sizeof(BlockType), Val);
}

template <typename BlockType>
RE_MEMSET_FN(Memset_first_last_block) {
  Memset_block<BlockType>(Dst, Val);
  tail_return Memset_last_block<BlockType>(Dst, Val, Len);
}

template <typename BlockType>
inline void Memset_loop_and_last_off(
 u8*  Dst, u8 Val, usize Len, usize Off) {
  do {
    Memset_block<BlockType>(Dst + Off, Val);
    Off += sizeof(BlockType);
  } while(Off < Len - sizeof(BlockType));
  Memset_last_block<BlockType>(Dst, Val, Len);
}

template <typename BlockType>
[[gnu::flatten]]
RE_MEMSET_FN(Memset_loop_and_last) {
  return Memset_loop_and_last_off<BlockType>(Dst, Val, Len, 0);
}

//======================================================================//
// Implementation
//======================================================================//

[[gnu::always_inline]] RE_MEMSET_FN(Memset_small) {
  if (Len == 1)
    tail_return Memset_block<u8>(Dst, Val, Len);
  if (Len == 2)
    tail_return Memset_block<u16>(Dst, Val, Len);
  if (Len == 3)
    tail_return Memset_block_seq<u16, u8>(Dst, Val, Len);
  // else:
  tail_return Memset_block<u32>(Dst, Val, Len);
}

[[gnu::always_inline]] RE_MEMSET_FN(Memset_dispatch) {
  if (Len == 0)
    return;
  if (Len < 5)
    tail_return Memset_small(Dst, Val, Len);
  if (Len <= 8)
    tail_return Memset_first_last_block<u32>(Dst, Val, Len);
  if (Len <= 16)
    tail_return Memset_first_last_block<u64>(Dst, Val, Len);
  if (Len <= 32)
    tail_return Memset_first_last_block<v128>(Dst, Val, Len);
  if (Len <= 64)
    tail_return Memset_first_last_block<v256>(Dst, Val, Len);
  if (Len <= 128)
    tail_return Memset_first_last_block<v512>(Dst, Val, Len);
  // else:
  Memset_block<v256>(Dst, Val);
  align_to_next_boundary<32>(Dst, Len);
  tail_return Memset_loop_and_last<v256>(Dst, Val, Len);
}

void* re::VMemset(void* Dst, u8 Val, usize Len) {
  re_assert(Dst || !Len);
  if UNLIKELY(Len == 0)
    return Dst;
  Memset_dispatch((u8*)Dst, Val, Len);
  return Dst;
}

void* re::VBzero(void* Dst, usize Len) {
  re_assert(Dst || !Len);
  if UNLIKELY(Len == 0)
    return Dst;
  Memset_dispatch((u8*)Dst, u8(0U), Len);
  return Dst;
}
