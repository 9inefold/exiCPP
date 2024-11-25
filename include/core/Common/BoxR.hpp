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
#include <Common/_CheckAlloc.hpp>
#include <Support/Casting.hpp>
#include <compare>
#include <concepts>
#include <new>

namespace exi {

template <typename T>
using BoxAllocator = Allocator<T>;

template <typename T, class A = BoxAllocator<T>>
class Box;

template <typename T, class A>
class Box {
  static_assert(std::move_constructible<A>, "Allocator must be movable!");
public:
  using Self = Box;
  using Traits = std::allocator_traits<A>;

  using type = T*; // TODO: Use PtrTraits
  using pointer = T*;
  using deleter_type = A;
  using allocator_type = A;
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
   Box(rhs.Ptr, std::move(rhs.Alloc)) {
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
  // Operators

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
template <typename T, class A>
struct Box<T[], A>;

// Traits

template <typename T, typename A = BoxAllocator<T>>
using box_alloc_t = typename Box<T, A>::allocator_type;

template <typename T, typename A = BoxAllocator<T>>
using box_ptr_t = typename Box<T, A>::pointer;

////////////////////////////////////////////////////////////////////////
// Operators

template <typename T, class At, typename U, class Au>
constexpr bool operator==(const Box<T, At>& x, const Box<U, Au>& y) {
  return (x.get() == y.get());
}

template <typename T, class A>
constexpr bool operator==(const Box<T, A>& lhs, std::nullptr_t) {
  return !lhs;
}

template <typename T, class At, typename U, class Au>
requires std::three_way_comparable_with<box_ptr_t<T, At>, box_ptr_t<U, Au>>
constexpr auto operator<=>(const Box<T, At>& x, const Box<U, Au>& y)
 -> std::compare_three_way_result_t<box_ptr_t<T, At>, box_ptr_t<U, Au>> {
  return std::compare_three_way()(x.get(), y.get());
}

template <typename T, class A>
requires std::three_way_comparable<box_ptr_t<T, A>>
constexpr auto operator<=>(const Box<T, A>& x, std::nullptr_t)
 -> std::compare_three_way_result_t<box_ptr_t<T, A>> {
  using Ptr = box_ptr_t<T, A>;
  return std::compare_three_way()(x.get(), static_cast<Ptr>(nullptr));
}

////////////////////////////////////////////////////////////////////////
// Free Functions

template <typename T, class A = BoxAllocator<T>>
auto make_box(auto&&...args) -> Box<T, A> {
  return Box<T, A>::New(EXI_FWD(args)...);
}

template <typename T, class A>
auto make_box_in(A&& alloc, auto&&...args) -> Box<T, A> {
  return Box<T, A>::NewIn(std::move(alloc), EXI_FWD(args)...);
}

//======================================================================//
// cast
//======================================================================//

template <typename To, typename From, typename Derived = void>
struct BoxedCast : public CastIsPossible<To, From *> {
  using Self = H::SelfType<Derived, BoxedCast<To, From>>;
  using CastResultType = Box<std::remove_reference_t<cast_retty_t<To, From>>>;

  static inline CastResultType doCast(Box<From> &&f) {
    return CastResultType((typename CastResultType::element_type *)f.release());
  }

  static inline CastResultType castFailed() { return CastResultType(nullptr); }

  static inline CastResultType doCastIfPossible(Box<From> &f) {
    if (!Self::isPossible(f.get()))
      return castFailed();
    return doCast(std::move(f));
  }
};

/// A CastInfo specialized for `Box`.
template <typename To, typename From>
struct CastInfo<To, Box<From>> : public BoxedCast<To, From> {};

template <typename To, typename From>
[[nodiscard]] inline decltype(auto) cast(Box<From> &&Val) {
  assert(isa<To>(Val) && "cast<Ty>() argument of incompatible type!");
  return CastInfo<To, Box<From>>::doCast(std::move(Val));
}

template <typename To, typename From>
[[nodiscard]] inline decltype(auto) dyn_cast(Box<From> &Val) {
  assert(H::isPresent(Val) && "dyn_cast on a non-existent value");
  return CastInfo<To, Box<From>>::doCastIfPossible(Val);
}

template <class X, class Y>
[[nodiscard]] inline auto cast_if_present(Box<Y> &&Val) {
  if (!H::isPresent(Val))
    return BoxedCast<X, Y>::castFailed();
  return BoxedCast<X, Y>::doCast(std::move(Val));
}

template <class X, class Y> auto cast_or_null(Box<Y> &&Val) {
  return cast_if_present<X>(Box(Val));
}

template <class X, class Y>
[[nodiscard]] inline
  typename CastInfo<X, Box<Y>>::CastResultType
 unique_dyn_cast(Box<Y> &Val) {
  if (!isa<X>(Val))
    return nullptr;
  return cast<X>(std::move(Val));
}

template <class X, class Y>
[[nodiscard]] inline auto unique_dyn_cast(Box<Y> &&Val) {
  return unique_dyn_cast<X, Y>(Val);
}

template <class X, class Y>
[[nodiscard]] inline typename CastInfo<X, Box<Y>>::CastResultType
unique_dyn_cast_or_null(Box<Y> &Val) {
  if (!Val)
    return nullptr;
  return unique_dyn_cast<X, Y>(Val);
}

template <class X, class Y>
[[nodiscard]] inline auto unique_dyn_cast_or_null(Box<Y> &&Val) {
  return unique_dyn_cast_or_null<X, Y>(Val);
}

} // namespace exi
