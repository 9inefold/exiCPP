//===- Support/Alloc.hpp --------------------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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
/// This file defines the allocators used by the program.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Support/D/FlexArray.hpp>
#include <cstdlib>
#include <cstring>
#include <memory>
#if EXI_USE_MIMALLOC
# include <mimalloc.h>
#endif

#if defined(EXPENSIVE_CHECKS) && EXI_USE_MIMALLOC
# define EXI_CHECK_ALLOC_PTR(PTR, MSG) do {                                   \
  if EXI_UNLIKELY(!::exi::exi_check_alloc(PTR)) {                             \
    ::exi::fatal_alloc_error(MSG);                                            \
  }                                                                           \
} while(false)
#else
# define EXI_CHECK_ALLOC_PTR(PTR, MSG) ((void)0)
#endif

namespace exi {

// Only use the default allocator for now.
// Since we always override new/delete, it should work the same.
// Helps with compatibility.
#if EXI_USE_MIMALLOC && 0
template <typename T>
using Allocator = mi_stl_allocator<T>;
#else
template <typename T>
using Allocator = std::allocator<T>;
#endif

/// @brief Reports a fatal allocation error.
/// If exceptions are enabled, throws `std::bad_alloc`, otherwise aborts.
[[noreturn]] void fatal_alloc_error(const char* Msg) EXI_NOEXCEPT;

EXI_INLINE bool exi_check_alloc(const void* ptr) {
#if EXI_USE_MIMALLOC
  return ::mi_is_in_heap_region(ptr);
#else
  return true;
#endif
}

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_malloc(usize size) {
#if EXI_USE_MIMALLOC
  return ::mi_malloc(size);
#else
  return std::malloc(size);
#endif
}

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_zalloc(usize size) {
#if EXI_USE_MIMALLOC
  return ::mi_zalloc(size);
#else
  void* const ptr = std::malloc(size);
  return std::memset(ptr, 0, size);
#endif
}

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_calloc(usize num, usize size) {
#if EXI_USE_MIMALLOC
  return ::mi_calloc(num, size);
#else
  return std::calloc(num, size);
#endif
}

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_realloc(void* ptr, usize newSize) {
  EXI_CHECK_ALLOC_PTR(ptr, "Invalid pointer in exi_realloc");
#if EXI_USE_MIMALLOC
  return ::mi_realloc(ptr, newSize);
#else
  return std::realloc(ptr, newSize);
#endif
}

EXI_INLINE void exi_free(void* ptr) {
  EXI_CHECK_ALLOC_PTR(ptr, "Invalid pointer in exi_realloc");
#if EXI_USE_MIMALLOC
  ::mi_free(ptr);
#else
  std::free(ptr);
#endif
}

//////////////////////////////////////////////////////////////////////////
// new-like

namespace new_detail {

template <class T>
using NewBlockSizeType =
  std::conditional_t<sizeof(T) < 4 && sizeof(void *) >= 8, u64, u32>;

template <typename T> struct InlineArr {
  NewBlockSizeType<T> Size;
  T Data[FLEX_ARRAY];
};

static_assert(kHasFlexibleArrayMembers);

// TODO: Add optional tracking?

template <typename T>
ALWAYS_INLINE EXI_FLATTEN void* _new_func(usize size) {
  if constexpr (std::is_trivially_default_constructible_v<T>)
    return exi::exi_zalloc(size);
  else
    return exi::exi_malloc(size);
}

template <typename T>
ALWAYS_INLINE EXI_FLATTEN T* new_impl(auto&&...Args) {
  T* Ptr = (T*)_new_func<T>(sizeof(T));
  return std::construct_at(Ptr, EXI_FWD(Args)...);
}

template <typename T>
ALWAYS_INLINE EXI_FLATTEN T* new_arr_raw_impl(usize N) {
  using BlockType = InlineArr<T>;
  auto* Block = static_cast<BlockType*>(
    exi::exi_malloc(sizeof(T) * N + sizeof(BlockType)));
  Block->Size = N;
  return Block->Data;
}

template <typename T>
ALWAYS_INLINE EXI_FLATTEN T* new_arr_impl(usize N) {
  using BlockType = InlineArr<T>;
  auto* Block = static_cast<BlockType*>(
    _new_func<T>(sizeof(T) * N + sizeof(BlockType)));
  Block->Size = N;
  // TODO: Check Size == N
  if constexpr (!std::is_trivially_default_constructible_v<T>)
    std::uninitialized_default_construct_n(Block->Data, N);
  return Block->Data;
}

template <typename T>
ALWAYS_INLINE EXI_FLATTEN void delete_impl(T* Ptr) {
  if EXI_NEVER(!Ptr) return;
  std::destroy_at(Ptr);
  exi::exi_free(Ptr);
}

template <typename T>
ALWAYS_INLINE EXI_FLATTEN void delete_arr_impl(T* Ptr) {
  using BlockType = InlineArr<T>;
  constexpr uptr kOffset = offsetof(BlockType, Data);
  if EXI_NEVER(!Ptr) return;
  auto* Block = reinterpret_cast<BlockType*>(
    reinterpret_cast<char*>(Ptr) - kOffset);
  if constexpr (!std::is_trivially_destructible_v<T>)
    std::destroy_n(Block->Data, Block->Size);
  exi::exi_free(Block);
}

} // namespace new_detail

/// A proxy class that lets you call custom allocators with `new`.
template <typename T> struct GlobalNew {
  ALWAYS_INLINE T* operator()(auto&&...Args) const {
    return new_detail::new_impl<T>(EXI_FWD(Args)...);
  }
  ALWAYS_INLINE T* operator[](usize N) const {
    return new_detail::new_arr_impl<T>(N);
  }
};

/// A proxy class that lets you call custom allocators with `new`.
template <typename T> struct GlobalNew<T[]> {
  ALWAYS_INLINE T* operator()(usize N) const {
    return new_detail::new_arr_impl<T>(N);
  }
};

/// A proxy class that lets you call custom allocators with `delete`.
template <typename T> struct GlobalDelete {
  constexpr GlobalDelete() noexcept = default;
  GlobalDelete(const GlobalDelete<dummy_t>&) = delete;
  GlobalDelete(const GlobalDelete<dummy_t[]>&) = delete;

  template <class U> requires (std::is_convertible_v<U*, T*>)
  ALWAYS_INLINE GlobalDelete(const GlobalDelete<U>&) noexcept {}

  ALWAYS_INLINE void operator()(T* Ptr) const noexcept {
    new_detail::delete_impl<T>(Ptr);
  }
  ALWAYS_INLINE void operator[](T* Ptr) const {
    new_detail::delete_arr_impl<T>(Ptr);
  }
};

/// A proxy class that lets you call custom allocators with `delete`.
template <typename T> struct GlobalDelete<T[]> {
  constexpr GlobalDelete() noexcept = default;
  GlobalDelete(const GlobalDelete<dummy_t>&) = delete;
  GlobalDelete(const GlobalDelete<dummy_t[]>&) = delete;

  template <class U> requires (std::is_convertible_v<U(*)[], T(*)[]>)
  ALWAYS_INLINE GlobalDelete(const GlobalDelete<U[]>&) noexcept {}
  
  ALWAYS_INLINE void operator()(T* Ptr) const noexcept {
    new_detail::delete_arr_impl<T>(Ptr);
  }
};

/// A dummy class that lets you call custom allocators with `delete`.
template <> struct GlobalDelete<dummy_t> {
  constexpr GlobalDelete() noexcept = default;
  template <class U> GlobalDelete(const GlobalDelete<U>&) = delete;

  template <typename T>
  ALWAYS_INLINE void operator()(T* Ptr) const {
    new_detail::delete_impl<T>(Ptr);
  }
  template <typename T>
  ALWAYS_INLINE void operator[](T* Ptr) const {
    new_detail::delete_arr_impl<T>(Ptr);
  }
};

/// A proxy object that lets you call custom allocators with `new`.
/// ```cpp
///  X = _new<T>(Args...);
///  _delete(X);
///  // or...
///  X = _new<T[]>(N);
///  _delete<T[]>(X);
///  // or...
///  X = _new<T>[N];
///  _delete[X];
/// ```
template <typename T>
inline constexpr GlobalNew<T> _new;

/// A proxy object that lets you call custom allocators with `delete`.
/// ```cpp
///  X = _new<T>(Args...);
///  _delete(X);
///  // or...
///  X = _new<T[]>(N);
///  _delete<T[]>(X);
///  // or...Delete<U[]>&) noexcept 
///  X = _new<T>[N];
///  _delete[X];
/// ```
inline constexpr GlobalDelete<dummy_t> _delete;

} // namespace exi
