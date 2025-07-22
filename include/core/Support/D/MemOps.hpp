//===- Support/D/MemOps.hpp -----------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// Implements some simple array copying utilities.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <cstring>
#include <memory>
#include <type_traits>

namespace exi {

enum class MemOp { Init, Uninit };

namespace H {

/// Implements byte-wise copies for `FastCopy`.
template <bool PotentiallyOverlapping, typename T, typename U>
ALWAYS_INLINE constexpr void FastTrivialCopy(T* Dst, U* Src, usize N) {
  if constexpr (!PotentiallyOverlapping)
    exi___builtin_memcpy(Dst, Src, sizeof(T) * N);
  else
    exi___builtin_memmove(Dst, Src, sizeof(T) * N);
}

/// Implements element-wise copies for `FastCopy`.
template <MemOp OP, typename T>
ALWAYS_INLINE constexpr void FastNontrivialCopy(T* Dst, const T* Src, usize N) {
  if constexpr (OP == MemOp::Uninit)
    std::uninitialized_copy(Src, Src + N, Dst);
  else {
    for (usize Ix = 0; Ix < N; ++Ix)
      Dst[Ix] = Src[Ix];
  }
}

/// Implements element-wise moves for `FastMove`.
template <MemOp OP, typename T>
ALWAYS_INLINE constexpr void FastNontrivialMove(T* Dst, T* Src, usize N) {
  if constexpr (OP == MemOp::Uninit)
    std::uninitialized_move(Src, Src + N, Dst);
  else {
    for (usize Ix = 0; Ix < N; ++Ix)
      Dst[Ix] = std::move(Src[Ix]);
  }
}

} // namespace H

/// Copies an array from `Src` to `Dst`.
template <bool PotentiallyOverlapping = false, typename T>
exi_mem_constexpr void FastCopy(T* Dst, const T* Src, usize N) {
  if constexpr (std::is_trivially_copyable_v<T>)
    H::FastTrivialCopy<PotentiallyOverlapping>(Dst, Src, N);
  else
    H::FastNontrivialCopy<MemOp::Init>(Dst, Src, N);
}

/// Moves an array from `Src` to `Dst`.
template <bool PotentiallyOverlapping = false, typename T>
exi_mem_constexpr void FastMove(T* Dst, T* Src, usize N) {
  if constexpr (std::is_trivially_copyable_v<T>)
    H::FastTrivialCopy<PotentiallyOverlapping>(Dst, Src, N);
  else
    H::FastNontrivialMove<MemOp::Init>(Dst, Src, N);
}

/// Copies an array from `Src` to uninitialized `Dst`.
template <bool PotentiallyOverlapping = false, typename T>
exi_mem_constexpr void FastUninitCopy(T* Dst, const T* Src, usize N) {
  if constexpr (std::is_trivially_copyable_v<T>)
    H::FastTrivialCopy<PotentiallyOverlapping>(Dst, Src, N);
  else
    H::FastNontrivialCopy<MemOp::Uninit>(Dst, Src, N);
}

/// Moves an array from `Src` to uninitialized `Dst`.
template <bool PotentiallyOverlapping = false, typename T>
exi_mem_constexpr void FastUninitMove(T* Dst, T* Src, usize N) {
  if constexpr (std::is_trivially_copyable_v<T>)
    H::FastTrivialCopy<PotentiallyOverlapping>(Dst, Src, N);
  else
    H::FastNontrivialMove<MemOp::Uninit>(Dst, Src, N);
}

} // namespace exi
