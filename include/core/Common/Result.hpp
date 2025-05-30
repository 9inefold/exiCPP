//===- Common/Result.hpp --------------------------------------------===//
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
/// This file defines the Result class, which is an implementation of
/// std::expected.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/Option.hpp>
#include <Common/D/Expect.hpp>

// TODO: expect/expect_error

namespace exi {

template <typename T>
concept is_result_proxy = is_expect<T> || is_unexpect<T>;

namespace result_detail {

using option_detail::is_const;
using option_detail::not_const;
using option_detail::abstract;
using option_detail::concrete;
using option_detail::EmptyTag;
struct ImplInvokeTag {};

//////////////////////////////////////////////////////////////////////////

template <typename T> struct ResultType {
  static_assert(!std::is_void_v<T>);
  static_assert(!std::is_rvalue_reference_v<T>);
  using type = T;
  using value_type = T;
  using storage_type = T;
public:
  ALWAYS_INLINE static constexpr
   T& Get(T& Val) { return Val; }
  ALWAYS_INLINE static constexpr
   const T& Get(const T& Val) { return Val; }
  ALWAYS_INLINE static constexpr
   T&& Get(T&& Val) { return std::move(Val); }

  ALWAYS_INLINE static constexpr
   decltype(auto) Set(auto&& Val) { return EXI_FWD(Val); }
};

template <typename T> struct ResultType<T&> {
  using type = std::remove_const_t<T>;
  using value_type = T&;
  using storage_type = T*;
public:
  ALWAYS_INLINE static constexpr
   T& Get(T* Val) { return *Val; }
  ALWAYS_INLINE static constexpr
   auto* Set(auto& Val) { return std::addressof(Val); }
};

template <typename T>
using result_storage = typename ResultType<T>::storage_type;

template <typename T>
concept trivially_copy_constructible
  =  std::is_copy_constructible_v<T>
  && std::is_trivially_constructible_v<T>;

template <typename T>
concept trivially_move_constructible
  =  std::is_copy_constructible_v<T>
  && std::is_trivially_constructible_v<T>;

template <typename T>
concept trivially_destructible
  =  std::is_lvalue_reference_v<T>
  || std::is_trivially_destructible_v<T>;

template <typename T>
concept trivial_copy_ctor = trivially_copy_constructible<result_storage<T>>;

template <typename T>
concept trivial_move_ctor = trivially_move_constructible<result_storage<T>>;

template <typename T>
concept trivial_dtor = trivially_destructible<result_storage<T>>;

template <typename T, typename U>
concept can_assign = std::is_assignable_v<
  result_storage<T>,
  std::conditional_t<
    std::is_reference_v<T>,
    result_storage<U>, U
  >
>;

//////////////////////////////////////////////////////////////////////////

template <typename T, typename E> class Storage;

template <typename T, typename E> class StorageBase {
protected:
  union Impl {
    EXI_NO_UNIQUE_ADDRESS result_storage<T> Data;
    EXI_NO_UNIQUE_ADDRESS result_storage<E> Unex;
  public:
    constexpr Impl(const Impl&) = delete;
    constexpr Impl(const Impl&)
      requires(trivial_copy_ctor<T> && trivial_copy_ctor<E>) = default;
    
    constexpr Impl(Impl&&) = delete;
    constexpr Impl(Impl&&)
      requires(trivial_move_ctor<T> && trivial_move_ctor<E>) = default;
    
    constexpr Impl& operator=(const Impl&) = delete;
    constexpr Impl& operator=(Impl&&) = delete;

    ALWAYS_INLINE constexpr explicit Impl(
      std::in_place_t, auto&&...Args)
     : Data(EXI_FWD(Args)...) {}
    
    ALWAYS_INLINE constexpr explicit Impl(
      unexpect_t, auto&&...Args)
     : Unex(EXI_FWD(Args)...) {}

    constexpr ~Impl() requires(trivial_dtor<T> && trivial_dtor<E>) = default;
    constexpr ~Impl() {}
  };

  EXI_NO_UNIQUE_ADDRESS Impl X;
  bool Active = true;

public:
  constexpr StorageBase(const StorageBase&) = delete;
  constexpr StorageBase(const StorageBase&)
    requires(trivial_copy_ctor<T> && trivial_copy_ctor<E>) = default;
  
  constexpr StorageBase(StorageBase&&) = delete;
  constexpr StorageBase(StorageBase&&)
    requires(trivial_move_ctor<T> && trivial_move_ctor<E>) = default;
  
  constexpr StorageBase& operator=(const StorageBase&) = delete;
  constexpr StorageBase& operator=(StorageBase&&) = delete;

  constexpr ~StorageBase()
    requires(trivial_dtor<T> && trivial_dtor<E>) = default;
  constexpr ~StorageBase()
    requires(!trivial_dtor<T> || !trivial_dtor<E>) { reset(); }

  inline constexpr StorageBase(std::in_place_t, auto&&...Args)
   : X(std::in_place, EXI_FWD(Args)...), Active(true) {}
  inline constexpr StorageBase(unexpect_t, auto&&...Args)
   : X(unexpect, EXI_FWD(Args)...), Active(false) {}

protected:
  inline constexpr StorageBase(ImplInvokeTag, bool IsActive, auto&& O) :
   X(StorageBase::MakeUnion(IsActive, EXI_FWD(O))), Active(IsActive) {}

  EXI_INLINE constexpr Impl& get_union() { return X; }
  EXI_INLINE constexpr const Impl& get_union() const { return X; }

  EXI_INLINE constexpr void construct(std::in_place_t, auto&&...Args) {
    std::construct_at(std::addressof(X.Data), EXI_FWD(Args)...);
    this->Active = true;
  }

  EXI_INLINE constexpr void construct(unexpect_t, auto&&...Args) {
    std::construct_at(std::addressof(X.Unex), EXI_FWD(Args)...);
    this->Active = false;
  }

  EXI_INLINE constexpr void reset() 
   requires(trivial_dtor<T> && trivial_dtor<E>) {}
  
  constexpr void reset() 
   requires(!trivial_dtor<T> || !trivial_dtor<E>) {
    if (Active) {
      if constexpr (!trivial_dtor<T>)
        std::destroy_at(std::addressof(X.Data));
    } else {
      if constexpr (!trivial_dtor<E>)
        std::destroy_at(std::addressof(X.Unex));
    }
  }

  constexpr void reset_union() 
   requires(trivial_dtor<T> && trivial_dtor<E>) {
    std::destroy_at(&X);
  }

  constexpr void reset_union() 
   requires(!trivial_dtor<T> || !trivial_dtor<E>) {
    this->reset();
    std::destroy_at(&X);
  }

private:
  /// Used to force copy elision on construction.
  ALWAYS_INLINE static constexpr Impl MakeUnion(bool IsActive, auto&& O) {
    if (IsActive)
      return Impl(std::in_place, EXI_FWD(O).Data);
    else
      return Impl(unexpect, EXI_FWD(O).Unex);
  }
};

/// Implements functions for `Result<?, E>`.
template <typename T, typename E>
class EXI_EMPTY_BASES StorageUnex : public StorageBase<T, E> {
  using BaseT = StorageBase<T, E>;

  ALWAYS_INLINE constexpr E& reference() {
    return BaseT::X.Unex;
  }

  ALWAYS_INLINE constexpr const E& reference() const {
    return BaseT::X.Unex;
  }

  ALWAYS_INLINE constexpr E* address() {
    return std::addressof(BaseT::X.Unex);
  }

protected:
  template <typename G>
  static constexpr bool can_copy_error
    = std::is_convertible_v<const G&, E>;
  
  template <typename G>
  static constexpr bool can_move_error
    = std::is_convertible_v<G, E>;

  ALWAYS_INLINE constexpr E& get_unex() & {
    return BaseT::X.Unex;
  }
  ALWAYS_INLINE constexpr const E& get_unex() const& {
    return BaseT::X.Unex;
  }
  ALWAYS_INLINE constexpr E&& get_unex() && {
    return std::move(BaseT::X.Unex);
  }

public:
  using BaseT::BaseT;
  
  constexpr StorageUnex(Unexpect<void>)
   requires std::is_default_constructible_v<E> : BaseT(unexpect) {
  }

  template <typename G>
  constexpr explicit(!std::is_convertible_v<std::add_const_t<G>&, E>)
    StorageUnex(Unexpect<G&> V) : BaseT(unexpect, V.error()) {
  }

  template <typename G>
  constexpr explicit(!std::is_convertible_v<const G&, E>)
    StorageUnex(const Unexpect<G>& V) : BaseT(unexpect, V.error()) {
  }

  template <typename G>
  constexpr explicit(!std::is_convertible_v<G, E>)
    StorageUnex(Unexpect<G>&& V) : BaseT(unexpect, std::move(V).error()) {
  }

  /// Constructs an error `E` in place, then returns a reference.
  constexpr E& emplace_error(auto&&...Args) EXI_LIFETIMEBOUND {
    BaseT::reset();
    BaseT::construct(unexpect, EXI_FWD(Args)...);
    return reference();
  }

  constexpr const auto* error_data() const {
    exi_assert(has_error(), "error is inactive!");
    return &BaseT::X.Unex;
  }
  constexpr auto* error_data() {
    exi_assert(has_error(), "error is inactive!");
    return &BaseT::X.Unex;
  }

  constexpr const E& error() const& EXI_LIFETIMEBOUND {
    exi_assert(has_error(), "error is inactive!");
    return BaseT::X.Unex;
  }
  constexpr E& error() & EXI_LIFETIMEBOUND {
    exi_assert(has_error(), "error is inactive!");
    return BaseT::X.Unex;
  }
  constexpr E&& error() && EXI_LIFETIMEBOUND {
    exi_assert(has_error(), "error is inactive!");
    return std::move(BaseT::X.Unex);
  }

  constexpr bool has_error() const { return !BaseT::Active; }

  template <typename G> constexpr E error_or(G&& Alt) const& {
    return has_error() ? reference() : EXI_FWD(Alt);
  }
  template <typename G> E error_or(G&& Alt) && {
    return has_error() ? std::move(reference()) : EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<?, E&>`.
template <typename T, typename E>
class EXI_EMPTY_BASES StorageUnex<T, E&> : public StorageBase<T, E&> {
  using BaseT = StorageBase<T, E&>;

  ALWAYS_INLINE constexpr E& reference() const {
    return *BaseT::X.Unex;
  }

  ALWAYS_INLINE constexpr E* address() {
    return BaseT::X.Unex;
  }

protected:
  template <typename G>
  static constexpr bool can_copy_error
    =  std::is_lvalue_reference_v<G>
    && std::is_convertible_v<std::remove_reference_t<G>*, E*>;
  
  template <typename G>
  static constexpr bool can_move_error
    =  std::is_lvalue_reference_v<G>
    && std::is_convertible_v<std::remove_reference_t<G>*, E*>;
  
  ALWAYS_INLINE constexpr E*& get_unex() {
    return BaseT::X.Unex;
  }
  ALWAYS_INLINE constexpr E* const& get_unex() const {
    return BaseT::X.Unex;
  }

public:
  using BaseT::BaseT;

  explicit constexpr StorageUnex(unexpect_t, E& In EXI_LIFETIMEBOUND) :
   BaseT(unexpect, std::addressof(In)) {}
  
  template <typename G>
  constexpr explicit(!std::is_convertible_v<G*, E*>)
    StorageUnex(Unexpect<G&> V) : StorageUnex(unexpect, V.error()) {
  }

  /// Creates an error `E&` in place, then returns a reference.
  constexpr E& emplace_error(E& In EXI_LIFETIMEBOUND) {
    BaseT::reset();
    BaseT::construct(unexpect, std::addressof(In));
    return In;
  }

  constexpr E* error_data() const {
    exi_assert(has_error(), "error is inactive!");
    return address();
  }
  constexpr E& error() const {
    exi_assert(has_error(), "error is inactive!");
    return reference();
  }

  constexpr bool has_error() const { return !BaseT::Active; }

  template <typename G> constexpr E& error_or(G&& Alt EXI_LIFETIMEBOUND) const {
    if (has_error())
      return reference();
    else
      return EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<T, ?>`.
template <typename T, typename E>
class EXI_EMPTY_BASES Storage : public StorageUnex<T, E> {
  using BaseT = StorageUnex<T, E>;
  using error_type = std::remove_reference_t<E>;

  ALWAYS_INLINE constexpr T& reference() {
    return BaseT::X.Data;
  }

  ALWAYS_INLINE constexpr const T& reference() const {
    return BaseT::X.Data;
  }

  ALWAYS_INLINE constexpr T* address() {
    return std::addressof(BaseT::X.Data);
  }

protected:
  template <typename U>
  static constexpr bool can_copy_value
    = std::is_convertible_v<const U&, T>;
  
  template <typename U>
  static constexpr bool can_move_value
    = std::is_convertible_v<U, T>;

  ALWAYS_INLINE constexpr T& get_data() & {
    return BaseT::X.Data;
  }
  ALWAYS_INLINE constexpr const T& get_data() const& {
    return BaseT::X.Data;
  }
  ALWAYS_INLINE constexpr T&& get_data() && {
    return std::move(BaseT::X.Data);
  }

public:
  using BaseT::BaseT;

  constexpr Storage()
    requires(std::is_default_constructible_v<T>)
   : BaseT(std::in_place) {}
  
  template <class U = std::remove_cv_t<T>>
  requires(!is_result_proxy<std::remove_cvref_t<U>>)
  constexpr explicit(!std::is_convertible_v<U, T>) Storage(U&& Val) :
   BaseT(std::in_place, EXI_FWD(Val)) {}
  
  template <typename U>
  constexpr explicit(!std::is_convertible_v<std::add_const_t<U>&, T>)
    Storage(Expect<U&> V) : BaseT(std::in_place, V.value()) {
  }

  template <typename U>
  constexpr explicit(!std::is_convertible_v<const U&, T>)
    Storage(const Expect<U>& V) : BaseT(std::in_place, V.value()) {
  }

  template <typename U>
  constexpr explicit(!std::is_convertible_v<U, T>)
    Storage(Expect<U>&& V) : BaseT(std::in_place, std::move(V).value()) {
  }

  /// Constructs a value `T` in place, then returns a reference.
  constexpr T& emplace(auto&&...Args) EXI_LIFETIMEBOUND {
    BaseT::reset();
    BaseT::construct(std::in_place, EXI_FWD(Args)...);
    return reference();
  }

  constexpr const auto* data() const {
    exi_assert(has_value(), "value is inactive!");
    return &BaseT::X.Data;
  }
  constexpr auto* data() {
    exi_assert(has_value(), "value is inactive!");
    return &BaseT::X.Data;
  }

  constexpr const T& value() const& EXI_LIFETIMEBOUND {
    exi_assert(has_value(), "value is inactive!");
    return BaseT::X.Data;
  }
  constexpr T& value() & EXI_LIFETIMEBOUND {
    exi_assert(has_value(), "value is inactive!");
    return BaseT::X.Data;
  }
  constexpr T&& value() && EXI_LIFETIMEBOUND {
    exi_assert(has_value(), "value is inactive!");
    return std::move(BaseT::X.Data);
  }

  constexpr const auto* operator->() const& {
    exi_assert(has_value(), "value is inactive!");
    return &BaseT::X.Data;
  }
  constexpr auto* operator->() & {
    exi_assert(has_value(), "value is inactive!");
    return &BaseT::X.Data;
  }
  constexpr auto* operator->() && {
    exi_assert(has_value(), "value is inactive!");
    return &BaseT::X.Data;
  }

  constexpr const T& operator*() const& { return value(); }
  constexpr T& operator*() & { return value(); }
  constexpr T&& operator*() && { return std::move(value()); }

  constexpr Result<T&, error_type&> ref()& {
    if (has_value())
      return Ok(reference());
    else
      return Err(BaseT::X.Unex);
  }
  // TODO: Fix this, error_type should be smarter.
  constexpr Result<const T&, const error_type&> ref() const& {
    if (has_value())
      return Ok(reference());
    else
      return Err(BaseT::X.Unex);
  }

  constexpr bool has_value() const { return BaseT::Active; }

  template <typename U> constexpr T value_or(U&& Alt) const& {
    return has_value() ? reference() : EXI_FWD(Alt);
  }
  template <typename U> T value_or(U&& Alt) && {
    return has_value() ? std::move(reference()) : EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<T&, ?>`.
template <typename T, typename E>
class EXI_EMPTY_BASES Storage<T&, E> : public StorageUnex<T&, E> {
  using BaseT = StorageUnex<T&, E>;
  using error_type = std::remove_reference_t<E>;

  ALWAYS_INLINE constexpr T& reference() const {
    return *BaseT::X.Data;
  }

  ALWAYS_INLINE constexpr T* address() const {
    return BaseT::X.Data;
  }

protected:
  template <typename U>
  static constexpr bool can_copy_value
    =  std::is_lvalue_reference_v<U>
    && std::is_convertible_v<std::remove_reference_t<U>*, T*>;
  
  template <typename U>
  static constexpr bool can_move_value
    =  std::is_lvalue_reference_v<U>
    && std::is_convertible_v<std::remove_reference_t<U>*, T*>;

  ALWAYS_INLINE constexpr T*& get_data() {
    return BaseT::X.Data;
  }
  ALWAYS_INLINE constexpr T* const& get_data() const {
    return BaseT::X.Data;
  }

public:
  using BaseT::BaseT;

  explicit constexpr Storage(std::in_place_t, T& In EXI_LIFETIMEBOUND) :
   BaseT(std::in_place, std::addressof(In)) {}

  template <class U = std::remove_cv_t<T>>
  requires(!is_result_proxy<std::remove_cv_t<U>>)
  constexpr explicit(!std::is_convertible_v<U*, T*>) Storage(U& Val) :
   BaseT(std::in_place, std::addressof(Val)) {}
  
  template <typename U>
  constexpr explicit(!std::is_convertible_v<U*, T*>)
    Storage(Expect<U&> V) : Storage(std::in_place, V.value()) {
  }

  /// Creates a value `T&` in place, then returns a reference.
  constexpr T& emplace(T& In EXI_LIFETIMEBOUND) {
    BaseT::reset();
    BaseT::construct(std::in_place, std::addressof(In));
    return In;
  }

  constexpr T* data() const {
    exi_assert(has_value(), "value is inactive!");
    return address();
  }
  constexpr T& value() const {
    exi_assert(has_value(), "value is inactive!");
    return reference();
  }

  constexpr T* operator->() const {
    exi_assert(has_value(), "value is inactive!");
    return address();
  }
  constexpr T& operator*() const {
    exi_assert(has_value(), "value is inactive!");
    return reference();
  }

  constexpr Result<T&, error_type&> ref()& {
    if (has_value())
      return Ok(reference());
    else
      return Err(BaseT::X.Unex);
  }
  // TODO: Fix this, error_type should be smarter.
  constexpr Result<const T&, const error_type&> ref() const& {
    if (has_value())
      return Ok(reference());
    else
      return Err(BaseT::X.Unex);
  }
  constexpr Result<T&, error_type&> ref() && 
   requires(std::is_reference_v<E>) {
    if (has_value())
      return Ok(reference());
    else
      return Err(BaseT::X.Unex);
  }

  EXI_INLINE constexpr bool has_value() const {
    return BaseT::Active;
  }

  template <typename U> constexpr T& value_or(U&& Alt EXI_LIFETIMEBOUND) const {
    if (has_value())
      return reference();
    else
      return EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<T, void>`.
template <typename E>
class Storage<void, E> {
  COMPILE_FAILURE(Storage,
    "Result<void, E> is unimplemented!");
};

/// Implements functions for `Result<T&, void>`.
template <typename E>
class Storage<void, E&> {
  COMPILE_FAILURE(Storage,
    "Result<void, E&> is unimplemented!");
};

} // namespace result_detail

template <typename T, typename E>
class Result : public result_detail::Storage<T, E> {
  static_assert(!std::same_as<T, std::in_place_t>);
  static_assert(!std::same_as<T, unexpect_t>);

  using Tx = result_detail::result_storage<T>;
  using Ex = result_detail::result_storage<E>;

  using BaseT = result_detail::Storage<T, E>;
  using InvokeTag = result_detail::ImplInvokeTag;

  template <typename U, typename G>
  static constexpr bool can_copy
    =  BaseT::template can_copy_value<U>
    && BaseT::template can_copy_error<G>;
  
  template <typename U, typename G>
  static constexpr bool can_move
    =  BaseT::template can_move_value<U>
    && BaseT::template can_move_error<G>;

  using BaseT::get_data;
  using BaseT::get_unex;

  template <typename U, typename G>
  friend class Result;

public:
  using BaseT::BaseT;

  constexpr Result(const Result&) = delete;
  constexpr Result(const Result&) requires(
      result_detail::trivial_copy_ctor<T>
   && result_detail::trivial_copy_ctor<E>) = default;
  
  constexpr Result(const Result& O) requires(
      std::is_copy_constructible_v<Tx>
   && std::is_copy_constructible_v<Ex> && 
    !(result_detail::trivial_copy_ctor<T>
   && result_detail::trivial_copy_ctor<E>)) :
    BaseT(InvokeTag{}, O.is_ok(), O.get_union()) {}
  
  constexpr Result(Result&&) = delete;
  constexpr Result(Result&&) requires(
      result_detail::trivial_move_ctor<T>
   && result_detail::trivial_move_ctor<E>) = default;
  
  constexpr Result(Result&& O) requires(
      std::is_move_constructible_v<Tx>
   && std::is_move_constructible_v<Ex> && 
    !(result_detail::trivial_move_ctor<T>
   && result_detail::trivial_move_ctor<E>)) :
    BaseT(InvokeTag{}, O.is_ok(), std::move(O.get_union())) {}
  
  template <typename U, typename G>
  constexpr explicit(!can_copy<U, G>) Result(const Result<U, G>& O) :
   BaseT(InvokeTag{}, O.is_ok(), O.get_union()) {}
  
  template <typename U, typename G>
  constexpr explicit(!can_move<U, G>) Result(Result<U, G>&& O) :
   BaseT(InvokeTag{}, O.is_ok(), std::move(O.get_union())) {}
  
  constexpr ~Result() = default;

private:
  EXI_INLINE void assignSame(auto&& O) {
    if (this->is_ok())
      this->get_data() = EXI_FWD(O).get_data();
    else
      this->get_unex() = EXI_FWD(O).get_unex();
  }

  EXI_INLINE void constructDifferent(auto&& O) {
    this->reset();
    if (O.is_ok())
      this->construct(std::in_place, EXI_FWD(O).get_data());
    else
      this->construct(unexpect, EXI_FWD(O).get_unex());
  }

public:
  constexpr Result& operator=(const Result& O) = delete;
  constexpr Result& operator=(const Result& O) requires(
      std::is_copy_assignable_v<Tx>
   && std::is_copy_assignable_v<Ex>
   && std::is_copy_constructible_v<Tx>
   && std::is_copy_constructible_v<Ex>) {
    if (this->is_ok() == O.is_ok())
      this->assignSame(O);
    else
      this->constructDifferent(O);
    return *this;
  }

  constexpr Result& operator=(Result&& O) requires(
      std::is_move_assignable_v<Tx>
   && std::is_move_assignable_v<Ex>
   && std::is_move_constructible_v<Tx>
   && std::is_move_constructible_v<Ex>) {
    if (this->is_ok() == O.is_ok())
      this->assignSame(std::move(O));
    else
      this->constructDifferent(std::move(O));
    return *this;
  }

  template <typename U>
  constexpr Result& operator=(U&& Val) requires(
      !std::same_as<std::remove_cvref_t<U>, Result>
   && !is_result_proxy<std::remove_cvref_t<U>>
   && BaseT::template can_move_value<U>
   && result_detail::can_assign<T, U>) {
    if (this->is_ok()) {
      using Proxy = result_detail::ResultType<T>;
      this->get_data() = Proxy::Set(EXI_FWD(Val));
    } else {
      this->reset();
      this->construct(std::in_place, EXI_FWD(Val));
    }
    return *this;
  }

  template <typename U>
  requires(std::is_assignable_v<Result, std::add_lvalue_reference_t<U>>)
  constexpr Result& operator=(const Expect<U>& O) {
    if (this->is_ok()) {
      this->get_data() = O.Data;
    } else {
      this->reset();
      this->construct(std::in_place, O.Data);
    }
    return *this;
  }

  template <typename U>
  requires(std::is_assignable_v<Result, U>)
  constexpr Result& operator=(Expect<U>&& O) {
    if (this->is_ok()) {
      this->get_data() = std::move(O.Data);
    } else {
      this->reset();
      this->construct(std::in_place, std::move(O.Data));
    }
    return *this;
  }

  constexpr Result& operator=(Unexpect<void>)
   requires std::is_default_constructible_v<E> {
    this->reset();
    this->construct(unexpect);
    return *this;
  }

  template <typename G>
  constexpr Result& operator=(const Unexpect<G>& O) {
    if (this->is_ok()) {
      this->reset();
      this->construct(unexpect, O.Data);
    } else {
      this->get_unex() = O.Data;
    }
    return *this;
  }

  template <typename G>
  constexpr Result& operator=(Unexpect<G>&& O) {
    if (this->is_ok()) {
      this->reset();
      this->construct(unexpect, std::move(O.Data));
    } else {
      this->get_unex() = std::move(O.Data);
    }
    return *this;
  }

  /// Returns `true` if value is active.
  constexpr bool is_ok() const { return BaseT::has_value(); }
  /// Returns `true` if error is active.
  constexpr bool is_err() const { return BaseT::has_error(); }
  /// Returns `true` if value is active.
  constexpr explicit operator bool() const { return is_ok(); }
};

//////////////////////////////////////////////////////////////////////////
// Unwrapping

template <typename T, typename E>
ALWAYS_INLINE constexpr bool
 exi_unwrap_chk(const Result<T, E>& Val) {
  return Val.is_ok();
}

template <typename T, typename E>
ALWAYS_INLINE constexpr auto
 exi_unwrap_fail(Result<T, E>& Val) {
  return Err(Val.error());
}

template <typename T, typename E>
ALWAYS_INLINE constexpr auto
 exi_unwrap_fail(const Result<T, E>& Val) {
  return Err(Val.error());
}

template <typename T, typename E>
ALWAYS_INLINE constexpr auto
 exi_unwrap_fail(Result<T, E>&& Val) {
  return Err(std::move(Val).error());
}

template <typename T, typename E>
ALWAYS_INLINE constexpr auto
 exi_unwrap_fail(const Result<T, E&>& Val) {
  return Err(Val.error());
}

template <typename T, typename E>
ALWAYS_INLINE constexpr auto
 exi_unwrap_fail(Result<T, E&>&& Val) {
  return Err(Val.error());
}

template <typename T, typename E>
ALWAYS_INLINE constexpr auto
 exi_unwrap_fail(Result<T&, E&> Val) {
  return Err(Val.error());
}

} // namespace exi
