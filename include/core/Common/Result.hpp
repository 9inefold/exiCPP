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

// EXI_NO_UNIQUE_ADDRESS

namespace exi {

template <typename T, typename E> class Result;

/// Recreation of `std::unexpect_t`.
struct unexpect_t { explicit constexpr unexpect_t() = default; };
/// Recreation of `std::unexpect_t`.
inline constexpr unexpect_t unexpect;

/// Reimplementation of `std::unexpected`.
template <typename E> class Unexpect {
  static_assert(!std::same_as<E, std::in_place_t>);
  static_assert(!std::same_as<E, unexpect_t>);
  E Data;
public:
  constexpr Unexpect(const Unexpect&) = default;
  constexpr Unexpect(Unexpect&&) = default;

  template <typename X = E>
  constexpr explicit Unexpect(X&& Val) : Data(EXI_FWD(Val)) {}
  
  constexpr explicit Unexpect(
    std::in_place_t, auto&&...Args)
   : Data(EXI_FWD(Args)...) {}
  
  constexpr Unexpect& operator=(const Unexpect&) = default;
  constexpr Unexpect& operator=(Unexpect&&) = default;

  constexpr E& error() & noexcept { return Data; }
  constexpr const E& error() const& noexcept { return Data; }
  constexpr E&& error() && noexcept { return std::move(Data); }
};

/// Reimplementation of `std::unexpected`.
template <typename E> class Unexpect<E&> {
  static_assert(!std::same_as<E, std::in_place_t>);
  static_assert(!std::same_as<E, unexpect_t>);
  E* Data = nullptr;
public:
  constexpr Unexpect(const Unexpect&) = default;
  constexpr Unexpect(Unexpect&&) = default;

  template <typename X = E>
  requires std::convertible_to<X*, E*>
  constexpr explicit Unexpect(
    X& Val EXI_LIFETIMEBOUND) :
   Data(std::addressof(Val)) {}

  constexpr Unexpect& operator=(const Unexpect&) = default;
  constexpr Unexpect& operator=(Unexpect&&) = default;

  constexpr E& error() const noexcept { return *Data; }
};

template <typename E> Unexpect(E&) -> Unexpect<E&>;
template <typename E> Unexpect(const E&) -> Unexpect<const E&>;
template <typename E> Unexpect(E&&) -> Unexpect<E>;

template <typename E> Unexpect(Unexpect<E&>) -> Unexpect<E&>;
template <typename E> Unexpect(Unexpect<const E&>) -> Unexpect<const E&>;
template <typename E> Unexpect(Unexpect<E>) -> Unexpect<E>;

template <typename E>
constexpr decltype(auto) Err(E&& Val) noexcept {
  return Unexpect(EXI_FWD(Val));
}

//////////////////////////////////////////////////////////////////////////

namespace result_detail {

using option_detail::is_const;
using option_detail::not_const;
using option_detail::abstract;
using option_detail::concrete;
using option_detail::EmptyTag;

template <typename T>
concept trivially_destructible
  =  std::is_lvalue_reference_v<T>
  || std::is_trivially_destructible_v<T>;

template <typename T>
concept not_trivially_destructible = !trivially_destructible<T>;

template <typename T, typename E>
concept needs_destructor
  =  not_trivially_destructible<T>
  || not_trivially_destructible<E>;

//////////////////////////////////////////////////////////////////////////

template <typename T> struct ResultType {
  static_assert(!std::is_void_v<T>);
  static_assert(!std::is_rvalue_reference_v<T>);
  using type = T;
  using value_type = T;
  using storage_type = T;
public:
  ALWAYS_INLINE static constexpr
   T& Get(T& Val) noexcept { return Val; }
  ALWAYS_INLINE static constexpr
   const T& Get(const T& Val) noexcept { return Val; }
  ALWAYS_INLINE static constexpr
   T&& Get(T&& Val) noexcept { return std::move(Val); }

  ALWAYS_INLINE static constexpr
   void Dtor(T& Val) noexcept { Val.~T(); }
};

template <typename T> struct ResultType<T&> {
  using type = std::remove_const_t<T>;
  using value_type = T&;
  using storage_type = T*;
public:
  ALWAYS_INLINE static constexpr T& Get(T* Val) noexcept { return *Val; }
  ALWAYS_INLINE static constexpr void Dtor(T*) noexcept {}
};

template <typename T>
using result_storage = typename ResultType<T>::storage_type;

//////////////////////////////////////////////////////////////////////////

template <typename T, typename E> class Storage;

/// Default storage for `Result`, this is trivially destructible.
template <typename T, typename E>
struct StorageBase {
protected:
  using value_storage = result_storage<T>;
  using error_storage = result_storage<E>;
public:
  EXI_NO_UNIQUE_ADDRESS union {
    EXI_NO_UNIQUE_ADDRESS EmptyTag Empty;
    EXI_NO_UNIQUE_ADDRESS value_storage Data;
    EXI_NO_UNIQUE_ADDRESS error_storage Unex;
  } X;
  bool Active = true;
public:
  inline constexpr StorageBase(
    std::in_place_type_t<T>, auto&&...Args)
   : X{.Data = value_storage(EXI_FWD(Args)...)}, Active(true) {}
  inline constexpr StorageBase(
    std::in_place_type_t<E>, auto&&...Args)
   : X{.Unex = error_storage(EXI_FWD(Args)...)}, Active(false) {}
protected:
  inline constexpr StorageBase(EmptyTag) : X{.Empty = {}} {}
  
public:
  EXI_INLINE constexpr void reset() noexcept {}
};

/// Default storage for `Result`, this is NOT trivially destructible.
template <typename T, typename E>
requires needs_destructor<T, E>
struct StorageBase<T, E> {
private:
  using value_storage = result_storage<T>;
  using error_storage = result_storage<E>;
public:
  EXI_NO_UNIQUE_ADDRESS union {
    EXI_NO_UNIQUE_ADDRESS EmptyTag Empty;
    EXI_NO_UNIQUE_ADDRESS value_storage Data;
    EXI_NO_UNIQUE_ADDRESS error_storage Unex;
  } X;
  bool Active = true;
public:
  inline constexpr StorageBase(
    std::in_place_type_t<T>, auto&&...Args)
   : X{.Data = value_storage(EXI_FWD(Args)...)}, Active(true) {}
  inline constexpr StorageBase(
    std::in_place_type_t<E>, auto&&...Args)
   : X{.Unex = error_storage(EXI_FWD(Args)...)}, Active(false) {}
protected:
  inline constexpr StorageBase(EmptyTag) : X{.Empty = {}} {}

public:
  inline constexpr ~StorageBase() { reset(); }

  constexpr void reset() noexcept {
    if (Active) {
      if (not_trivially_destructible<T>)
        ResultType<T>::Dtor(X.Data);
    } else {
      if (not_trivially_destructible<E>)
        ResultType<E>::Dtor(X.Unex);
    }
  }
};

/// Implements functions for `Result<T, ?>`.
template <typename T, typename E>
class EXI_EMPTY_BASES StorageData : public StorageBase<T, E> {
  using BaseT = StorageBase<T, E>;

  ALWAYS_INLINE constexpr T& reference() noexcept {
    return BaseT::X.Data;
  }

  ALWAYS_INLINE constexpr const T& reference() const noexcept {
    return BaseT::X.Data;
  }

  ALWAYS_INLINE constexpr T* address() noexcept {
    return std::addressof(BaseT::X.Data);
  }

protected:
  using BaseT::Active;
  using BaseT::X;
public:
  using BaseT::BaseT;
  using BaseT::reset;

  constexpr StorageData() : BaseT(std::in_place_type<T>) {}

  explicit constexpr StorageData(std::in_place_t, auto&&...Args) :
   BaseT(std::in_place_type<T>, EXI_FWD(Args)...) {}
  
  template <class U = std::remove_cv_t<T>>
  constexpr explicit(!std::is_convertible_v<U, T>) StorageData(U&& Val) :
   BaseT(std::in_place_type<T>, EXI_FWD(Val)) {}

  /// Constructs a value `T` in place, then returns a reference.
  constexpr T& emplace(auto&&...Args) noexcept EXI_LIFETIMEBOUND {
    BaseT::reset();
    std::construct_at(address(), EXI_FWD(Args)...);
    BaseT::Active = true;
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

  constexpr bool has_value() const noexcept { return BaseT::Active; }

  template <typename U> constexpr T value_or(U&& Alt) const& {
    return has_value() ? reference() : EXI_FWD(Alt);
  }
  template <typename U> T value_or(U&& Alt) && {
    return has_value() ? std::move(reference()) : EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<T&, ?>`.
template <typename T, typename E>
class EXI_EMPTY_BASES StorageData<T&, E> : public StorageBase<T&, E> {
  using BaseT = StorageBase<T&, E>;

  ALWAYS_INLINE constexpr T& reference() const noexcept {
    return *BaseT::X.Data;
  }

  ALWAYS_INLINE constexpr T* address() const noexcept {
    return BaseT::X.Data;
  }

protected:
  using BaseT::Active;
  using BaseT::X;
public:
  using BaseT::BaseT;
  using BaseT::reset;

  explicit constexpr StorageData(std::in_place_t, T& In EXI_LIFETIMEBOUND) :
   BaseT(std::in_place_type<T&>, std::addressof(In)) {}

  template <class U = std::remove_cv_t<T>>
  constexpr explicit(!std::is_convertible_v<U*, T*>) StorageData(U& Val) :
   BaseT(std::in_place_type<T&>, std::addressof(Val)) {}

  /// Creates a value `T&` in place, then returns a reference.
  constexpr T& emplace(T& In EXI_LIFETIMEBOUND) noexcept {
    BaseT::reset();
    BaseT::X.Data = std::addressof(In);
    BaseT::Active = true;
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

  EXI_INLINE constexpr bool has_value() const noexcept {
    return BaseT::Active;
  }

  template <typename U> constexpr T& value_or(U&& Alt EXI_LIFETIMEBOUND) const {
    if (has_value())
      return reference();
    else
      return EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<?, E>`.
template <typename T, typename E>
class EXI_EMPTY_BASES StorageUnex : public StorageData<T, E> {
  using BaseT = StorageData<T, E>;
  using BaseT::Active;
  using BaseT::X;

  ALWAYS_INLINE constexpr E& reference() noexcept {
    return BaseT::X.Unex;
  }

  ALWAYS_INLINE constexpr const E& reference() const noexcept {
    return BaseT::X.Unex;
  }

  ALWAYS_INLINE constexpr E* address() noexcept {
    return std::addressof(BaseT::X.Unex);
  }

public:
  using BaseT::BaseT;
  using BaseT::has_value;
  using BaseT::reset;

  explicit constexpr StorageUnex(unexpect_t, auto&&...Args) :
   BaseT(std::in_place_type<E>, EXI_FWD(Args)...) {}
  
  template <typename G>
  constexpr explicit(!std::is_convertible_v<std::add_const_t<G>&, E>)
    StorageUnex(Unexpect<G&> V) : StorageUnex(unexpect, V.error()) {
  }

  template <typename G>
  constexpr explicit(!std::is_convertible_v<const G&, E>)
    StorageUnex(const Unexpect<G>& V) : StorageUnex(unexpect, V.error()) {
  }

  template <typename G>
  constexpr explicit(!std::is_convertible_v<G, E>)
    StorageUnex(Unexpect<G>&& V) : StorageUnex(unexpect, std::move(V).error()) {
  }

  /// Constructs an error `E` in place, then returns a reference.
  constexpr E& emplace_error(auto&&...Args) noexcept EXI_LIFETIMEBOUND {
    BaseT::reset();
    std::construct_at(address(), EXI_FWD(Args)...);
    BaseT::Active = false;
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

  constexpr bool has_error() const noexcept { return !BaseT::Active; }

  template <typename G> constexpr E error_or(G&& Alt) const& {
    return has_error() ? reference() : EXI_FWD(Alt);
  }
  template <typename G> E error_or(G&& Alt) && {
    return has_error() ? std::move(reference()) : EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<?, E&>`.
template <typename T, typename E>
class EXI_EMPTY_BASES StorageUnex<T, E&> : public StorageData<T, E&> {
  using BaseT = StorageData<T, E&>;
  using BaseT::Active;
  using BaseT::X;

  ALWAYS_INLINE constexpr E& reference() const noexcept {
    return *BaseT::X.Unex;
  }

  ALWAYS_INLINE constexpr E* address() noexcept {
    return BaseT::X.Unex;
  }

public:
  using BaseT::BaseT;
  using BaseT::has_value;
  using BaseT::reset;

  explicit constexpr StorageUnex(unexpect_t, E& In EXI_LIFETIMEBOUND) :
   BaseT(std::in_place_type<E&>, std::addressof(In)) {}
  
  template <typename G>
  constexpr explicit(!std::is_convertible_v<G&, E&>)
    StorageUnex(Unexpect<G&> V) : StorageUnex(unexpect, V.error()) {
  }

  /// Creates an error `E&` in place, then returns a reference.
  constexpr E& emplace_error(E& In EXI_LIFETIMEBOUND) noexcept {
    BaseT::reset();
    BaseT::X.Unex = std::addressof(In);
    BaseT::Active = false;
    return In;
  }

  constexpr E* error_data() const {
    exi_assert(has_error(), "error is inactive!");
    return address();
  }
  constexpr T& error() const {
    exi_assert(has_error(), "error is inactive!");
    return reference();
  }

  constexpr bool has_error() const noexcept { return !BaseT::Active; }

  template <typename G> constexpr E& error_or(G&& Alt EXI_LIFETIMEBOUND) const {
    if (has_value())
      return reference();
    else
      return EXI_FWD(Alt);
  }
};

/// Implements functions for `Result<?, ?>`.
template <typename T, typename E>
class EXI_EMPTY_BASES Storage : public StorageUnex<T, E> {
  using ErrT = StorageUnex<T, E>;
  using ValT = StorageUnex<T, E>;
public:
  using ErrT::ErrT;
  using ErrT::reset;

  constexpr bool is_ok() const noexcept { return ValT::has_value(); }
  constexpr bool is_err() const noexcept { return ErrT::has_error(); }
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
  using BaseT = result_detail::Storage<T, E>;
public:
  using BaseT::BaseT;
};

} // namespace exi
