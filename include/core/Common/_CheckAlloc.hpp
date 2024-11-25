//===- Common/_CheckAlloc.hpp ---------------------------------------===//
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
#include <concepts>
#include <new>

namespace exi {
namespace H {

template <typename T>
concept _is_polymorphic = std::is_polymorphic_v<T>;

template <class MaybeDerived, class MaybeBase>
concept is_same_or_polymorphic_super
  = std::same_as<MaybeDerived, MaybeBase>
  || (_is_polymorphic<MaybeDerived>
    && std::derived_from<MaybeDerived, MaybeBase>);

template <class Alloc, typename T>
concept is_allocator_ptr_type
  = is_same_or_polymorphic_super<T,
    typename std::allocator_traits<Alloc>::value_type>;

template <class A, typename T>
concept has__check_alloc = requires(A& Alloc, const T* ptr) {
  { Alloc.check_alloc(ptr) } -> std::convertible_to<bool>;
};

template <class A, typename T>
concept has__checkAlloc = requires(A& Alloc, const T* ptr) {
  { Alloc.checkAlloc(ptr) } -> std::convertible_to<bool>;
};

template <class A, typename T>
concept has__CheckAlloc = requires(const T* ptr) {
  { A::CheckAlloc(ptr) } -> std::convertible_to<bool>;
};


template <class A, typename T>
EXI_INLINE bool check_allocation(A& Alloc, const T* ptr) noexcept {
  if constexpr (has__check_alloc<A, T>) {
    return Alloc.check_alloc(ptr);
  } else if constexpr (has__checkAlloc<A, T>) {
    return Alloc.checkAlloc(ptr);
  } else if constexpr (has__CheckAlloc<A, T>) {
    return A::CheckAlloc(ptr);
  } else {
    return false;
  }
}

} // namespace H

template <typename T>
bool check_box_alloc(Allocator<T>&, const T* ptr) noexcept {
  return exi_check_alloc(ptr);
}

template <class A, typename T>
requires(H::is_allocator_ptr_type<A, T>)
bool check_box_alloc(A& Alloc, const T* ptr) noexcept {
  return H::check_allocation(Alloc, ptr);
}

template <class A, typename T>
requires(!H::is_allocator_ptr_type<A, T>)
constexpr bool check_box_alloc(A& Alloc, const T* ptr) noexcept {
  return false;
}

} // namespace exi
