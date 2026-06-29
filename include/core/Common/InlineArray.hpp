//===- Common/InlineStr.hpp -----------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file implements a string with the size inline.
/// Functions are defined in Common/InlineArray.cpp
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ArrayRef.hpp>
#include <Common/Fundamental.hpp>
#include <Support/Alloc.hpp>
#include <Support/Allocator.hpp>
#include <Support/IntCast.hpp>
#include <Common/D/TrivialTraits.hpp>
#include <Support/D/FlexArray.hpp>
#include <concepts>

namespace exi {

static_assert(kHasFlexibleArrayMembers,
  "InlineArray requires compiler support for Flexible Array Members!");

struct InlineStr;

template <class T, typename SizeType>
inline constexpr usize kMinInlineArrayAlign
  = alignof(T) > alignof(SizeType) ? alignof(T) : alignof(SizeType);

template <class T>
using InlineArraySizeType =
    std::conditional_t<sizeof(T) < 4 && sizeof(void*) >= 8, u64, u32>;

template <class Impl, class Type>
struct InlineArrayCommon {
  template <class AllocatorT>
  static Impl* NewUninit(AllocatorT& Alloc, usize Size) {
    const usize AllocSize = sizeof(Impl) + (sizeof(Type) * Size);
    Impl* Out = (Impl*)Alloc.Allocate(AllocSize, alignof(Impl));
    Out->Size = IntCast<typename Impl::size_type>(Size);
    return Out;
  }

  template <class AllocatorT, class U>
  requires std::same_as<Type, std::remove_const_t<U>>
  static Impl* New(AllocatorT& Alloc, U* Data, usize Size) {
    Impl* Out = NewUninit(Alloc, Size);
    FastUninitCopy(Out->Data, Data, Size);
    return Out;
  }
  static Impl* New(Type* Data, usize Size) {
    MallocAllocator Alloc {};
    if constexpr (H::trivially_copy_constructible<Type>)
      return New(Alloc, static_cast<const Type*>(Data), Size);
    else
      return New(Alloc, Data, Size);
  }
  static Impl* New(const Type* Data, usize Size) {
    MallocAllocator Alloc {};
    return New(Alloc, Data, Size);
  }

  EXI_INLINE static Impl* New(ArrayRef<Type> Data) {
    return New(Data.data(), Data.size());
  }
  EXI_INLINE static Impl* New(MutArrayRef<Type> Data) {
    return New(Data.data(), Data.size());
  }
  template <class AllocatorT>
  EXI_INLINE static Impl* New(AllocatorT& Alloc, ArrayRef<Type> Data) {
    return New(Alloc, Data.data(), Data.size());
  }
  template <class AllocatorT>
  EXI_INLINE static Impl* New(AllocatorT& Alloc, MutArrayRef<Type> Data) {
    if constexpr (H::trivially_copy_constructible<Type>)
      return New(Alloc, static_cast<const Type*>(Data.data()), Data.size());
    else
      return New(Alloc, Data.data(), Data.size());
  }

  template <class AllocatorT>
  static void Delete(AllocatorT& Alloc, Impl* Data) {
    exi_invariant(Data != nullptr);
    const usize AllocSize = sizeof(Impl) + Data->size_in_bytes();
    if constexpr (H::trivially_destructible<Impl>)
      std::destroy_at(Data);
    Alloc.Deallocate(Data, AllocSize, alignof(Impl));
  }
  ALWAYS_INLINE static void Delete(Impl* Data) {
    MallocAllocator Alloc {};
    return InlineArrayCommon::Delete(Alloc, Data);
  }
};

/// An array with the size allocated inline.
/// Memory is allocated with the following layout:
///
///   [ Size ][ Elements ]
///
/// This allows for more convenient inline arrays.
template <class T, std::integral SizeType = InlineArraySizeType<T>>
struct alignas(kMinInlineArrayAlign<T, SizeType>) InlineArray final
    : public InlineArrayCommon<InlineArray<T, SizeType>, T> {
  static_assert(!std::is_const_v<T>, "Inline array type cannot be const!");
  static_assert(!std::is_reference_v<T>, "Inline array type cannot be a reference!");
  friend struct InlineStr;

  using BaseT = InlineArrayCommon<InlineArray, T>;
  using value_type = T;
  using size_type = SizeType;
  size_type Size;
  char Data[];

public:
  InlineArray(const InlineArray&) = delete;
  InlineArray& operator=(const InlineArray&) = delete;
  InlineArray(InlineArray&&) = delete;
  InlineArray& operator=(InlineArray&&) = delete;

  constexpr ~InlineArray() requires(H::trivially_destructible<T>) = default;
  constexpr ~InlineArray() requires(!H::trivially_destructible<T>) {
    std::destroy_n(this->Data, this->Size);
  }

  using BaseT::New;
  using BaseT::Delete;

  MutArrayRef<T> arr() { return MutArrayRef(this->Data, this->Size); }
  ArrayRef<T> arr() const { return ArrayRef(this->Data, this->Size); }
  usize size() const { return this->Size; }
  usize size_in_bytes() const { return this->Size * sizeof(T); }
};

extern template struct InlineArray<char, u16>;

} // namespace exi
