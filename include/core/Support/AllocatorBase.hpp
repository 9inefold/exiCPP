//===- Support/AllocatorBase.hpp -------------------------------------===//
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
/// \file
///
/// This file defines MallocAllocator. MallocAllocator conforms to the LLVM
/// "Allocator" concept which consists of an Allocate method accepting a size
/// and alignment, and a Deallocate accepting a pointer and size. Further, the
/// LLVM "Allocator" concept has overloads of Allocate and Deallocate for
/// setting size and alignment based on the final type. These overloads are
/// typically provided by a base class template \c AllocatorBase.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/CRTPTraits.hpp>
#include <Common/Fundamental.hpp>
#include <Support/SafeAlloc.hpp>
#include <type_traits>

namespace exi {

#define EXI_CRTP_TYPES (AllocatorBase, DerivedT)

/// CRTP base class providing obvious overloads for the core \c
/// Allocate() methods of LLVM-style allocators.
///
/// This base class both documents the full public interface exposed by all
/// LLVM-style allocators, and redirects all of the overloads to a single core
/// set of methods which the derived class must define.
template <typename DerivedT> class AllocatorBase {
  EXI_CRTP_DEFINE_SUPER(DerivedT)
public:
  /// Allocate \a Size bytes of \a Alignment aligned memory. This method
  /// must be implemented by \c DerivedT.
  void *Allocate(usize Size, usize Alignment) {
#ifdef __clang__
    static_assert(static_cast<void *(AllocatorBase::*)(usize, usize)>(
                      &AllocatorBase::Allocate) !=
                      static_cast<void *(DerivedT::*)(usize, usize)>(
                          &DerivedT::Allocate),
                  "Class derives from AllocatorBase without implementing the "
                  "core Allocate(usize, usize) overload!");
#endif
    return super()->Allocate(Size, Alignment);
  }

  /// Deallocate \a Ptr to \a Size bytes of memory allocated by this
  /// allocator.
  void Deallocate(const void *Ptr, usize Size, usize Alignment) {
#ifdef __clang__
    static_assert(
        static_cast<void (AllocatorBase::*)(const void *, usize, usize)>(
            &AllocatorBase::Deallocate) !=
            static_cast<void (DerivedT::*)(const void *, usize, usize)>(
                &DerivedT::Deallocate),
        "Class derives from AllocatorBase without implementing the "
        "core Deallocate(void *) overload!");
#endif
    return super()->Deallocate(Ptr, Size, Alignment);
  }

  // The rest of these methods are helpers that redirect to one of the above
  // core methods.

  /// Allocate space for a sequence of objects without constructing them.
  template <typename T> T *Allocate(usize Num = 1) {
    return static_cast<T *>(Allocate(Num * sizeof(T), alignof(T)));
  }

  /// Deallocate space for a sequence of objects without constructing them.
  template <typename T>
  std::enable_if_t<!std::is_same_v<std::remove_cv_t<T>, void>, void>
  Deallocate(T *Ptr, usize Num = 1) {
    Deallocate(static_cast<const void *>(Ptr), Num * sizeof(T), alignof(T));
  }
};

#undef EXI_CRTP_TYPES

class MallocAllocator : public AllocatorBase<MallocAllocator> {
public:
  void Reset() {}

  EXI_RETURNS_NONNULL void *Allocate(usize Size, usize Alignment) {
    return allocate_buffer(Size, Alignment);
  }

  // Pull in base class overloads.
  using AllocatorBase<MallocAllocator>::Allocate;

  void Deallocate(const void *Ptr, usize Size, usize Alignment) {
    deallocate_buffer(const_cast<void *>(Ptr), Size, Alignment);
  }

  // Pull in base class overloads.
  using AllocatorBase<MallocAllocator>::Deallocate;

  void PrintStats() const {}
};

namespace H {

template <typename Alloc> class AllocatorHolder : Alloc {
public:
  AllocatorHolder() = default;
  AllocatorHolder(const Alloc &A) : Alloc(A) {}
  AllocatorHolder(Alloc &&A) : Alloc(static_cast<Alloc &&>(A)) {}
  Alloc &getAllocator() { return *this; }
  const Alloc &getAllocator() const { return *this; }
};

template <typename Alloc> class AllocatorHolder<Alloc &> {
  Alloc &A;

public:
  AllocatorHolder(Alloc &A) : A(A) {}
  Alloc &getAllocator() { return A; }
  const Alloc &getAllocator() const { return A; }
};

} // namespace H

} // namespace exi
