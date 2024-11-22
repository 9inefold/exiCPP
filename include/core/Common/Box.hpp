//===- Common/Map.hpp -----------------------------------------------===//
//===- Common/Box.hpp -----------------------------------------------===//
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
EXI_INLINE bool check_allocation(A& Alloc, const T* ptr) noexcept {
  if constexpr (has__check_alloc<A, T>) {
    return Alloc.check_alloc(ptr);
  } else if constexpr (has__checkAlloc<A, T>) {
    return Alloc.checkAlloc(ptr);
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

//======================================================================//
// Box
//======================================================================//

template <typename T, class A = Allocator<T>>
class Box;

template <typename T, class A>
class Box {
  static_assert(std::move_constructible<A>, "Allocator must be movable!");
public:
  using Self = Box;
  using Traits = std::allocator_traits<A>;

  using type = T*;
  using deleter_type = A;
  using element_type = T;
public:
  constexpr Box() noexcept : Ptr(), Alloc() {}
  constexpr Box(std::nullptr_t) noexcept : Box() {}

  Box(const Box&) = delete;
  Box& operator=(const Box&) = delete;

  ~Box() { this->clear(); }

  ////////////////////////////////////////////////////////////////////////
  // Ctors

  template <H::is_same_or_polymorphic_super<T> U>
  Box(Box<U, A>&& rhs) noexcept :
   Box(rhs.Ptr, std::move(rhs.A)) {
    rhs.clear();
  }

  template <H::is_same_or_polymorphic_super<T> U>
  requires(std::constructible_from<A>)
  Box(U* ptr) noexcept : Ptr(ptr), Alloc() {
    exi_invariant(CheckAlloc(Alloc, ptr), "Invalid allocation!");
  }

  template <H::is_same_or_polymorphic_super<T> U>
  requires(std::copy_constructible<A>)
  Box(U* ptr, const A& Alloc) noexcept : Ptr(ptr), Alloc(Alloc) {
    exi_invariant(CheckAlloc(Alloc, ptr), "Invalid allocation!");
  }

  Box(T* ptr, A&& Alloc) noexcept : Ptr(ptr), Alloc(std::move(Alloc)) {
    exi_invariant(CheckAlloc(Alloc, ptr), "Invalid allocation!");
  }

  ////////////////////////////////////////////////////////////////////////
  // Assignment

  template <H::is_same_or_polymorphic_super<T> U>
  Box& operator=(Box<U, A>&& rhs) noexcept {
    this->reset(rhs.Ptr);
    this->Alloc = std::move(rhs.Alloc);
    rhs.reset();
    return *this;
  }

  template <H::is_same_or_polymorphic_super<T> U>
  Box& operator=(U* ptr) noexcept {
    exi_invariant(CheckAlloc(this->Alloc, ptr), "Invalid allocation!");
    this->reset(ptr);
    return *this;
  }

  ////////////////////////////////////////////////////////////////////////
  // Functions

  template <typename...Args>
  requires(std::constructible_from<A>)
  [[nodiscard]] static Self New(Args&&...args) {
    return Self::NewIn(A(), EXI_FWD(args)...);
  }

  template <typename...Args>
  [[nodiscard]] static Self NewIn(A&& Alloc, Args&&...args) {
    T* ptr = Traits::allocate(Alloc, 1);
    if EXI_UNLIKELY(!ptr) {
      return Self(nullptr, std::move(Alloc));
    }

    Traits::construct(Alloc, ptr, EXI_FWD(args)...);
    return Self(ptr, std::move(Alloc));
  }

  template <typename U>
  requires(std::constructible_from<A>)
  [[nodiscard]] static Self From(U&& u) {
    return Self::NewIn(A(), EXI_FWD(u));
  }

  template <typename U>
  [[nodiscard]] static Self FromIn(U&& u, A&& Alloc) {
    return Self::NewIn(std::move(Alloc), EXI_FWD(u));
  }

  template <H::is_same_or_polymorphic_super<T> U>
  requires(std::constructible_from<A>)
  [[nodiscard]] static Self FromRaw(U* ptr) {
    return Self(ptr, A());
  }

  template <H::is_same_or_polymorphic_super<T> U>
  [[nodiscard]] static Self FromRaw(U* ptr, A&& Alloc) {
    return Self(ptr, std::move(Alloc));
  }

public:
  static constexpr A& Allocator(Self& B) noexcept {
    return B.Alloc;
  }

  constexpr A& allocator() noexcept {
    return this->Alloc;
  }

  EXI_INLINE void clear() noexcept {
    this->reset(nullptr);
  }

  // TODO: downcast[Unchecked]

  T* get() noexcept { return Ptr; }
  const T* get() const noexcept { return Ptr; }

  [[nodiscard]] T* leak() noexcept { return this->release(); }
  [[nodiscard]] T* release() noexcept {
    T* out = nullptr;
    std::swap(this->Ptr, out);
    return out;
  }

  void reset(T* ptr = type()) noexcept {
    exi_invariant(CheckAlloc(Alloc, ptr), "Invalid allocation!");
    T* oldPtr = this->get();
    this->Ptr = ptr;
    if (oldPtr) {
      Traits::destroy(Alloc, oldPtr);
      Traits::deallocate(Alloc, oldPtr, 1);
    }
  }

  ////////////////////////////////////////////////////////////////////////

  constexpr explicit operator bool() const noexcept {
    return this->hasPtr();
  }

  T& operator*() noexcept {
    exi_invariant(this->hasPtr(), "nullptr dereference!");
    return *get();
  }

  const T& operator*() const noexcept {
    exi_invariant(this->hasPtr(), "nullptr dereference!");
    return *get();
  }

  T* operator->() noexcept {
    exi_invariant(this->hasPtr(), "nullptr dereference!");
    return get();
  }

  const T* operator->() const noexcept {
    exi_invariant(this->hasPtr(), "nullptr dereference!");
    return get();
  }

private:
  static bool CheckAlloc(A& Alloc, const T* ptr) noexcept {
    return (ptr == nullptr) || check_box_alloc(Alloc, ptr);
  }

  bool hasPtr() const noexcept {
    return (this->Ptr != nullptr);
  }

private:
  type Ptr = nullptr;
  EXI_NO_UNIQUE_ADDRESS A Alloc;
};

// TODO: Implement
template <typename T, class Alloc>
struct Box<T[], Alloc>;

} // namespace exi
