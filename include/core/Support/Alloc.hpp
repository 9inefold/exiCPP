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

namespace exi {

#if EXI_USE_MIMALLOC
template <typename T>
using Allocator = mi_stl_allocator<T>;
#else
template <typename T>
using Allocator = std::allocator<T>;
#endif

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_malloc(usize size) noexcept {
#if EXI_USE_MIMALLOC
  return ::mi_malloc(size);
#else
  return std::malloc(size);
#endif
}

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_zalloc(usize size) noexcept {
#if EXI_USE_MIMALLOC
  return ::mi_zalloc(size);
#else
  void* const ptr = std::malloc(size);
  return std::memset(ptr, 0, size);
#endif
}

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_calloc(usize num, usize size) noexcept {
#if EXI_USE_MIMALLOC
  return ::mi_calloc(num, size);
#else
  return std::calloc(num, size);
#endif
}

EXI_RETURNS_NOALIAS EXI_INLINE
 void* exi_realloc(void* ptr, usize newSize) noexcept {
#if EXI_USE_MIMALLOC
  return ::mi_realloc(ptr, newSize);
#else
  return std::realloc(ptr, newSize);
#endif
}

EXI_INLINE void exi_free(void* ptr) noexcept {
#if EXI_USE_MIMALLOC
  ::mi_free(ptr);
#else
  std::free(ptr);
#endif
}

EXI_INLINE bool exi_check_alloc(const void* ptr) noexcept {
#if EXI_USE_MIMALLOC
  return ::mi_is_in_heap_region(ptr);
#else
  return true;
#endif
}

} // namespace exi
