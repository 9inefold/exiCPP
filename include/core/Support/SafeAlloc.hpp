//===- Support/SafeAlloc.hpp ----------------------------------------===//
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

#pragma once

#include <Support/Alloc.hpp>
#include <Support/ErrorHandle.hpp>

namespace exi {

EXI_RETURNS_NONNULL inline void* safe_malloc(usize size) {
  void* ptr = exi_malloc(size);
  if EXI_UNLIKELY(ptr == nullptr) {
    // It is implementation-defined whether allocation occurs if the space
    // requested is zero (ISO/IEC 9899:2018 7.22.3). Retry, requesting
    // non-zero, if the space requested was zero.
    if (size == 0)
      return safe_malloc(1);
    fatal_alloc_error("Allocation failed");
  }
  return ptr;
}

EXI_RETURNS_NONNULL inline void* safe_calloc(usize num, usize size) {
  void* ptr = exi_calloc(num, size);
  if EXI_UNLIKELY(ptr == nullptr) {
    // It is implementation-defined whether allocation occurs if the space
    // requested is zero (ISO/IEC 9899:2018 7.22.3). Retry, requesting
    // non-zero, if the space requested was zero.
    if (num == 0 || size == 0)
      return safe_malloc(1);
    fatal_alloc_error("Allocation failed");
  }
  return ptr;
}

EXI_RETURNS_NONNULL inline void* safe_realloc(void* ptr, usize size) {
  void* newPtr = exi_realloc(ptr, size);
  if EXI_UNLIKELY(newPtr == nullptr) {
    // It is implementation-defined whether allocation occurs if the space
    // requested is zero (ISO/IEC 9899:2018 7.22.3). Retry, requesting
    // non-zero, if the space requested was zero.
    if (size == 0)
      return safe_malloc(1);
    fatal_alloc_error("Allocation failed");
  }
  return newPtr;
}

EXI_RETURNS_NONNULL EXI_RETURNS_NOALIAS
void* allocate_buffer(usize size, usize align);

void deallocate_buffer(void* ptr, usize size, usize align);

} // namespace exi
