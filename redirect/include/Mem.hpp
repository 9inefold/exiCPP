//===- Mem.hpp ------------------------------------------------------===//
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
/// This file defines simple memory utilities.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>

namespace re {

template <typename Int>
requires std::is_integral_v<Int>
constexpr bool is_pow2(Int V) {
  using UInt = std::make_unsigned_t<Int>;
  const UInt UV = static_cast<UInt>(V);
  return V && ((UV - 1) & UV) == 0;
}

inline constexpr usize align_flat(usize V) noexcept {
  --V;
  V |= V >> 1;
  V |= V >> 2;
  V |= V >> 4;
  V |= V >> 8;
  V |= V >> 16;
  V |= V >> 32;
  return ++V;
}

inline constexpr usize align_ceil(usize V) noexcept {
  return align_flat(++V);
}

inline constexpr usize align_floor(usize V) noexcept {
  return align_flat(++V) >> 1;
}

//////////////////////////////////////////////////////////////////////////

template <usize Align, typename T>
inline T* assume_aligned(T* Ptr) noexcept {
  static_assert(is_pow2(Align), "Align must be a power of 2");
#if EXI_INVARIANTS
  re_assert((uptr(Ptr) & (Align - 1)) == 0);
#endif // EXI_INVARIANTS
  return static_cast<T*>(
    __builtin_assume_aligned(Ptr, Align));
}

template <usize Align>
inline uptr offset_from_last_align(const void* ptr) {
  static_assert(is_pow2(Align), "Align must be a power of 2");
  return Align - (uptr(ptr) & (Align - 1U));
}

template <typename T>
inline void adjust_ptr(ptrdiff_t offset, T*& t, usize& len) {
  t += offset;
  len -= offset;
}

template <typename T, typename U>
inline void adjust_ptrs(ptrdiff_t offset, T*& t, U*& u, usize& len) {
  t += offset;
  u += offset;
  len -= offset;
}

template <usize Align, typename T>
inline void align_to_next_boundary(T*& t, usize& len) {
  static_assert(sizeof(T) == 1);
  adjust_ptr(offset_from_last_align<Align>(t), t, len);
  t = assume_aligned<Align>(t);
}

//////////////////////////////////////////////////////////////////////////
// Casting

template <typename To = void>
To* ptr_cast(auto* Ptr) noexcept {
  if constexpr (std::is_void_v<std::remove_cvref_t<To>>) {
    return static_cast<To*>(Ptr);
  } else {
    return reinterpret_cast<To*>(Ptr);
  }
}

template <typename To = void>
To* ptr_cast(uptr V) noexcept {
  return reinterpret_cast<To*>(V); 
}

template <typename To>
To* aligned_ptr_cast(auto* Ptr) noexcept {
  constexpr usize Align = alignof_v<To>;
  return assume_aligned<Align>(ptr_cast<To>(Ptr));
}

template <typename To>
To* aligned_ptr_cast(uptr Ptr) noexcept {
  constexpr usize Align = alignof_v<To>;
  return assume_aligned<Align>(ptr_cast<To>(Ptr));
}

} // namespace re
