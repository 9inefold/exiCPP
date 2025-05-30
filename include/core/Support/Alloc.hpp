//===- Support/Alloc.hpp --------------------------------------------===//
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
/// This file defines the allocators used by the program.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <cstdlib>
#include <memory>
#if EXI_USE_MIMALLOC
# include <mimalloc.h>
#else
# include <cstring>
#endif

#if defined(EXPENSIVE_CHECKS) && EXI_USE_MIMALLOC
# define EXI_CHECK_ALLOC_PTR(PTR, MSG) do {                                   \
  if EXI_UNLIKELY(!::exi::exi_check_alloc(PTR)) {                             \
    ::exi::fatal_alloc_error(MSG);                                            \
  }                                                                           \
} while(false)
#else
# define EXI_CHECK_ALLOC_PTR(PTR, MSG) do { } while(false)
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

} // namespace exi
