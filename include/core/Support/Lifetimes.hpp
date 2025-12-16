//===- Support/Lifetimes.hpp -----------------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file adds wrappers for explicit lifetime management. Currently
/// unsupported on all (major?) implementations.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/QualTraits.hpp>
#include <Support/ErrorHandle.hpp>
#include <cstring>
#include <memory>
#include <type_traits>
#if EXI_INVARIANTS
# include <Support/Alignment.hpp>
#endif

namespace exi {

/// Copies an array of `T[Count]` of trivial type `T` from `Dst` to `Src`.
template <typename T> requires std::is_trivially_copyable_v<T>
exi_mem_constexpr T* trivial_copy(T* Dst, const T* Src, usize Count) {
  return static_cast<T*>(exi___builtin_memcpy(Dst, Src, sizeof(T) * Count));
}

/// Moves an array of `T[Count]` of trivial type `T` from `Dst` to `Src`.
template <typename T> requires std::is_trivially_copyable_v<T>
exi_mem_constexpr T* trivial_move(T* Dst, const T* Src, usize Count) {
  return static_cast<T*>(exi___builtin_memmove(Dst, Src, sizeof(T) * Count));
}

/// Copies an array of `byte[Bytes]` from `Dst` to `Src`, assuming `Dst` is aligned.
/// Then implicitly creates an array `T[Bytes / sizeof(T)]` of trivial types.
template <typename T, bool SkipChecks = false>
requires std::is_trivially_copyable_v<T>
exi_mem_constexpr T* trivial_copy_bytes(
 void* Dst, const void* Src, usize Bytes) {
  if constexpr (!SkipChecks) {
    exi_expensive_invariant((Bytes % sizeof(T)) == 0);
    exi_invariant(exi::isAddrAligned<T>(Dst));
  }
  return static_cast<T*>(exi___builtin_memcpy(
    EXI_ASSUME_ALIGNED(Dst, alignof(T)), Src, Bytes));
}

/// Moves an array of `byte[Bytes]` from `Dst` to `Src`, assuming `Dst` is aligned.
/// Then implicitly creates an array `T[Bytes / sizeof(T)]` of trivial types.
template <typename T, bool SkipChecks = false>
requires std::is_trivially_copyable_v<T>
exi_mem_constexpr T* trivial_move_bytes(
 void* Dst, const void* Src, usize Bytes) {
  if constexpr (!SkipChecks) {
    exi_expensive_invariant((Bytes % sizeof(T)) == 0);
    exi_invariant(exi::isAddrAligned<T>(Dst));
  }
  return static_cast<T*>(exi___builtin_memmove(
    EXI_ASSUME_ALIGNED(Dst, alignof(T)), Src, Bytes));
}

//////////////////////////////////////////////////////////////////////////
// start_lifetime_as[_array]

#if __cpp_lib_start_lifetime_as >= 202207L
using std::start_lifetime_as;
using std::start_lifetime_as_array;
#else

template <typename T>
T* start_lifetime_as(void* P) noexcept {
  return static_cast<T*>(P);
}

template <typename T>
const T* start_lifetime_as(const void* P) noexcept {
  return static_cast<const T*>(P);
}

template <typename T>
volatile T* start_lifetime_as(volatile void* P) noexcept {
  return static_cast<volatile T*>(P);
}

template <typename T>
const volatile T* start_lifetime_as(const volatile void* P) noexcept {
  return static_cast<const volatile T*>(P);
}

template <typename T>
T* start_lifetime_as_array(void* P, std::size_t N) noexcept {
  (void)N;
  return static_cast<T*>(P);
}

template <typename T>
const T* start_lifetime_as_array(const void* P, std::size_t N) noexcept {
  (void)N;
  return static_cast<const T*>(P);
}

template <typename T>
volatile T* start_lifetime_as_array(volatile void* P, std::size_t N) noexcept {
  (void)N;
  return static_cast<volatile T*>(P);
}

template <typename T>
const volatile T* start_lifetime_as_array(
 const volatile void* P, std::size_t N) noexcept {
  (void)N;
  return static_cast<const volatile T*>(P);
}

#endif

} // namespace exi
