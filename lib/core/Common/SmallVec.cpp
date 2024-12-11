//===- Common/SmallVec.cpp -------------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
/// This file implements the SmallVec class.
///
//===----------------------------------------------------------------===//

#include <Common/SmallVec.hpp>
#include <Common/String.hpp>
#include <Support/SafeAlloc.hpp>
#include <Support/ErrorHandle.hpp>
#include <fmt/format.h>
#if EXI_EXCEPTIONS
# include <stdexcept>
#endif

using namespace exi;

// Check that no bytes are wasted and everything is well-aligned.
namespace {
// These structures may cause binary compat warnings on AIX. Suppress the
// warning since we are only using these types for the static assertions below.
#if defined(_AIX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waix-compat"
#endif
struct Struct16B {
  alignas(16) void *X;
};
struct Struct32B {
  alignas(32) void *X;
};
#if defined(_AIX)
#pragma GCC diagnostic pop
#endif
}
static_assert(sizeof(SmallVec<void *, 0>) ==
                  sizeof(unsigned) * 2 + sizeof(void *),
              "wasted space in SmallVec size 0");
static_assert(alignof(SmallVec<Struct16B, 0>) >= alignof(Struct16B),
              "wrong alignment for 16-byte aligned T");
static_assert(alignof(SmallVec<Struct32B, 0>) >= alignof(Struct32B),
              "wrong alignment for 32-byte aligned T");
static_assert(sizeof(SmallVec<Struct16B, 0>) >= alignof(Struct16B),
              "missing padding for 16-byte aligned T");
static_assert(sizeof(SmallVec<Struct32B, 0>) >= alignof(Struct32B),
              "missing padding for 32-byte aligned T");
static_assert(sizeof(SmallVec<void *, 1>) ==
                  sizeof(unsigned) * 2 + sizeof(void *) * 2,
              "wasted space in SmallVec size 1");

static_assert(sizeof(SmallVec<char, 0>) ==
                  sizeof(void *) * 2 + sizeof(void *),
              "1 byte elements have word-sized type for size and capacity");

/// Report that MinSize doesn't fit into this vector's size type. Throws
/// std::length_error or calls report_fatal_error.
[[noreturn]] static void report_size_overflow(usize MinSize, usize MaxSize);
static void report_size_overflow(usize MinSize, usize MaxSize) {
  Str Reason = fmt::format(
    "SmallVec unable to grow. Requested capacity ({})"
    " is larger than maximum value for size type ({})",
    MinSize, MaxSize
  );
#ifdef EXI_EXCEPTIONS
  throw std::length_error(Reason);
#else
  report_fatal_error(Reason);
#endif
}

/// Report that this vector is already at maximum capacity. Throws
/// std::length_error or calls report_fatal_error.
[[noreturn]] static void report_at_maximum_capacity(usize MaxSize);
static void report_at_maximum_capacity(usize MaxSize) {
  std::string Reason = fmt::format(
    "SmallVec capacity unable to grow. Already at maximum size {}",
    MaxSize
  );
#ifdef EXI_EXCEPTIONS
  throw std::length_error(Reason);
#else
  report_fatal_error(Reason);
#endif
}

// Note: Moving this function into the header may cause performance regression.
template <class Size_T>
static usize getNewCapacity(
 usize MinSize, [[maybe_unused]] usize TSize, usize OldCapacity) {
  constexpr usize MaxSize = std::numeric_limits<Size_T>::max();

  // Ensure we can fit the new capacity.
  // This is only going to be applicable when the capacity is 32 bit.
  if (MinSize > MaxSize)
    report_size_overflow(MinSize, MaxSize);

  // Ensure we can meet the guarantee of space for at least one more element.
  // The above check alone will not catch the case where grow is called with a
  // default MinSize of 0, but the current capacity cannot be increased.
  // This is only going to be applicable when the capacity is 32 bit.
  if (OldCapacity == MaxSize)
    report_at_maximum_capacity(MaxSize);

  // In theory 2*capacity can overflow if the capacity is 64 bit, but the
  // original capacity would never be large enough for this to be a problem.
  usize NewCapacity = 2 * OldCapacity + 1; // Always grow.
  return std::clamp(NewCapacity, MinSize, MaxSize);
}

/// If vector was first created with capacity 0, getFirstEl() points to the
/// memory right after, an area unallocated. If a subsequent allocation,
/// that grows the vector, happens to return the same pointer as getFirstEl(),
/// get a new allocation, otherwise isSmall() will falsely return that no
/// allocation was done (true) and the memory will not be freed in the
/// destructor. If a VSize is given (vector size), also copy that many
/// elements to the new allocation - used if realloca fails to increase
/// space, and happens to allocate precisely at BeginX.
/// This is unlikely to be called often, but resolves a memory leak when the
/// situation does occur.
static void *replaceAllocation(void *NewElts, usize TSize, usize NewCapacity,
                               usize VSize = 0) {
  void *NewEltsReplace = exi::safe_malloc(NewCapacity * TSize);
  if (VSize)
    memcpy(NewEltsReplace, NewElts, VSize * TSize);
  free(NewElts);
  return NewEltsReplace;
}

// Note: Moving this function into the header may cause performance regression.
template <class Size_T>
void *SmallVecBase<Size_T>::mallocForGrow(void *FirstEl, usize MinSize,
                                             usize TSize,
                                             usize &NewCapacity) {
  NewCapacity = getNewCapacity<Size_T>(MinSize, TSize, this->capacity());
  // Even if capacity is not 0 now, if the vector was originally created with
  // capacity 0, it's possible for the malloc to return FirstEl.
  void *NewElts = exi::safe_malloc(NewCapacity * TSize);
  if (NewElts == FirstEl)
    NewElts = replaceAllocation(NewElts, TSize, NewCapacity);
  return NewElts;
}

// Note: Moving this function into the header may cause performance regression.
template <class Size_T>
void SmallVecBase<Size_T>::grow_pod(void *FirstEl, usize MinSize,
                                       usize TSize) {
  usize NewCapacity = getNewCapacity<Size_T>(MinSize, TSize, this->capacity());
  void *NewElts;
  if (BeginX == FirstEl) {
    NewElts = exi::safe_malloc(NewCapacity * TSize);
    if (NewElts == FirstEl)
      NewElts = replaceAllocation(NewElts, TSize, NewCapacity);

    // Copy the elements over.  No need to run dtors on PODs.
    memcpy(NewElts, this->BeginX, size() * TSize);
  } else {
    // If this wasn't grown from the inline copy, grow the allocated space.
    NewElts = exi::safe_realloc(this->BeginX, NewCapacity * TSize);
    if (NewElts == FirstEl)
      NewElts = replaceAllocation(NewElts, TSize, NewCapacity, size());
  }

  this->set_allocation_range(NewElts, NewCapacity);
}

template class exi::SmallVecBase<u32>;

// Disable the u64 instantiation for 32-bit builds.
// Both u32 and u64 instantiations are needed for 64-bit builds.
// This instantiation will never be used in 32-bit builds, and will cause
// warnings when sizeof(Size_T) > sizeof(usize).
#if SIZE_MAX > UINT32_MAX
template class exi::SmallVecBase<u64>;

// Assertions to ensure this #if stays in sync with SmallVecSizeType.
static_assert(sizeof(SmallVecSizeType<char>) == sizeof(u64),
              "Expected SmallVecBase<u64> variant to be in use.");
#else
static_assert(sizeof(SmallVecSizeType<char>) == sizeof(u32),
              "Expected SmallVecBase<u32> variant to be in use.");
#endif
