//===- Common/Option.hpp --------------------------------------------===//
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
/// This file defines the Option class, which is a custom implementation of
/// std::optional with extra functionality. It supports custom storage and
/// reference types.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/Fundamental.hpp>
#include <Support/ErrorHandle.hpp>
#include <new>
#include <optional>
#include <utility>

// TODO: Annotate more lifetimebounds

namespace exi {

template <typename T> class Option;

namespace option_detail {

template <typename T>
concept is_const = std::is_const_v<T>;

template <typename T>
concept not_const = !is_const<T>;

template <typename T>
concept abstract = std::is_abstract_v<T>;

template <typename T>
concept concrete = !abstract<T>;

template <class Derived, class Base>
concept valid_constness = is_const<Base> || not_const<Derived>;

/// Must be a derived class, but cannot be the same.
template <class Derived, class Base>
concept explicitly_derived
  = std::derived_from<Derived, Base>
  && !std::same_as<Derived, Base>;

/// Same as `explicitly_derived`, but removes qualifiers.
template <class Derived, class Base>
concept explicitly_derived_ex = explicitly_derived<
  std::remove_cvref_t<Derived>, std::remove_cvref_t<Base>>
  && valid_constness<Derived, Base>;

//////////////////////////////////////////////////////////////////////////

template <typename F, typename T>
using TransformImpl = std::remove_cv_t<std::invoke_result_t<F, T&>>;

template <typename F, typename T>
using Transform = Option<TransformImpl<F, T>>;

//////////////////////////////////////////////////////////////////////////

template <typename T> struct RefStorage;
struct EmptyTag {}; // TODO: [[no_unique_address]]

/// Default storage for `Option`, this is trivially destructible.
template <typename T,
  bool = std::is_trivially_destructible_v<T>>
struct Storage {
  union {
    char Empty;
    T Data;
  };
  bool Active = false;
public:
  constexpr Storage() : Empty() {}
  inline constexpr Storage(
    std::in_place_t, auto&&...Args) :
   Data(EXI_FWD(Args)...), Active(true) {}
  
  EXI_INLINE constexpr void reset() noexcept {}
};

/// Default storage for `Option`, this is NOT trivially destructible.
template <typename T>
struct Storage<T, false> {
  union {
    char Empty;
    T Data;
  };
  bool Active = false;
public:
  constexpr Storage() : Empty() {}
  inline constexpr Storage(
    std::in_place_t, auto&&...Args) : 
   Data(EXI_FWD(Args)...), Active(true) {}
  
  inline constexpr ~Storage() { reset(); }
  constexpr void reset() noexcept {
    if (Active)
      Data.~T();
    Active = false;
  }
};

} // namespace option_detail

/// The default storage provider for `Option`.
/// Works like `std::optional`.
template <typename T>
class OptionStorage : public option_detail::Storage<T> {
  using BaseT = option_detail::Storage<T>;

  inline constexpr void construct(auto&& Other) {
    exi_invariant(!this->has_value(),
      "Cannot construct from active OptionStorage!");
    if (Other.Active) [[likely]] {
      std::construct_at(std::addressof(BaseT::Data), EXI_FWD(Other).Data);
      BaseT::Active = true;
    }
  }

  inline constexpr void assign(auto&& Other) {
    if (BaseT::Active == Other.Active) {
      if (BaseT::Active)
        BaseT::Data = EXI_FWD(Other).Data;
    } else {
      if (BaseT::Active)
        BaseT::reset();
      else
        this->construct(EXI_FWD(Other));
    }
  }

public:
  using BaseT::BaseT;

  constexpr OptionStorage(const OptionStorage& Other) {
    this->construct(Other);
  }
  constexpr OptionStorage(OptionStorage&& Other) noexcept {
    this->construct(std::move(Other));
  }

  constexpr OptionStorage& operator=(const T& V) noexcept {
    BaseT::Data = V;
    BaseT::Active = true;
    return *this;
  }

  constexpr OptionStorage& operator=(T&& V) noexcept {
    BaseT::Data = std::move(V);
    BaseT::Active = true;
    return *this;
  }

  constexpr OptionStorage& operator=(const OptionStorage& Other) {
    this->assign(Other);
    return *this;
  }
  constexpr OptionStorage& operator=(OptionStorage&& Other) {
    this->assign(std::move(Other));
    return *this;
  }

  inline constexpr T& emplace(auto&&...Args) {
    BaseT::reset();
    new (std::addressof(BaseT::Data)) T(EXI_FWD(Args)...);
    BaseT::Active = true;
    return value();
  }

  inline constexpr void reset() { BaseT::reset(); }

  constexpr const T& value() const& EXI_LIFETIMEBOUND {
    exi_invariant(has_value(), "value is inactive!");
    return BaseT::Data;
  }

  constexpr T& value() & EXI_LIFETIMEBOUND {
    exi_invariant(has_value(), "value is inactive!");
    return BaseT::Data;
  }

  constexpr T&& value() && {
    exi_invariant(has_value(), "value is inactive!");
    return std::move(BaseT::Data);
  }

  constexpr bool has_value() const {
    return BaseT::Active;
  } 
};

// Optional type which internal storage can be specialized by providing
// OptionStorage. The interface follows std::optional.
template <typename T> class Option {
  EXI_NO_UNIQUE_ADDRESS OptionStorage<T> Storage;

  EXI_INLINE static constexpr void DoPartialSwap(
   Option& Active, Option& Inactive) noexcept {
    exi_invariant(Active.is_some() && Inactive.is_none());
    // Move value to Inactive.
    Inactive.emplace(std::move(Active.value()));
    // Destroy old value.
    Active.reset();
  }

public:
  using type = T;
  using value_type = T;

  constexpr Option() = default;
  constexpr Option(std::nullopt_t) {}

  constexpr Option(const T& V) : Storage(std::in_place, V) {}
  constexpr Option(const Option& O) = default;

  constexpr Option(T&& V)
      : Storage(std::in_place, std::move(V)) {}
  constexpr Option(Option&& O) = default;

  constexpr Option(Option<T&> O)
      : Option(O ? *O : Option()) {}

  constexpr Option(std::in_place_t, auto&&...Args)
      : Storage(std::in_place, EXI_FWD(Args)...) {}

  // Allow conversion from std::optional<T>.
  constexpr Option(const std::optional<T>& V)
    requires option_detail::concrete<T>
      : Option(V ? *V : Option()) {}

  explicit constexpr Option(std::optional<T>&& V)
    requires option_detail::concrete<T>
      : Option(V ? std::move(*V) : Option()) {}

  Option& operator=(T&& V) {
    Storage = std::move(V);
    return *this;
  }
  Option& operator=(Option&& O) = default;

  /// Create a new object by constructing it in place with the given arguments.
  constexpr T& emplace(auto&&...Args) {
    return Storage.emplace(EXI_FWD(Args)...);
  }

  constexpr void swap(Option& O) noexcept {
    if (this->has_value()) {
      if (O.has_value()) {
        using std::swap;
        std::swap(this->value(), O.value());
      } else {
        // Swap half inactive.
        return Option::DoPartialSwap(*this, O);
      }
    } else if (O.has_value()) {
      // Swap half inactive.
      return Option::DoPartialSwap(O, *this);
    }
  }

  constexpr Option& operator=(const T& V) {
    Storage = V;
    return *this;
  }
  constexpr Option& operator=(const Option& O) = default;

  void reset() { Storage.reset(); }

  constexpr const auto* data() const { return &Storage.value(); }
  constexpr auto* data() { return &Storage.value(); }

  constexpr const T& value() const& { return Storage.value(); }
  constexpr T& value() & { return Storage.value(); }
  constexpr T&& value() && { return std::move(Storage.value()); }
  
  constexpr explicit operator bool() const { return has_value(); }
  constexpr bool has_value() const { return Storage.has_value(); }
  constexpr bool is_some() const { return Storage.has_value(); }
  constexpr bool is_none() const { return !Storage.has_value(); }

  constexpr const auto* operator->() const& { return &Storage.value(); }
  constexpr auto* operator->() & { return &Storage.value(); }
  constexpr auto* operator->() && { return &Storage.value(); }

  constexpr const T& operator*() const& { return Storage.value(); }
  constexpr T& operator*() & { return Storage.value(); }
  constexpr T&& operator*() && { return std::move(Storage.value()); }

  /// Reserved for `Option<T>`. Converts the contained value to an `Option<T&>`,
  /// if it exists.
  constexpr Option<T&> ref()& noexcept {
    return has_value() ? std::addressof(value()) : nullptr;
  }
  /// Reserved for `Option<T>`. Converts the contained value to
  /// an `Option<const T&>`, if it exists.
  constexpr Option<const T&> ref() const& noexcept {
    return has_value() ? std::addressof(value()) : nullptr;
  }

  constexpr T& ref_or(T& Alt)& noexcept {
    return ref().value_or(Alt);
  }
  constexpr const T& ref_or(const T& Alt EXI_LIFETIMEBOUND) const& noexcept {
    return ref().value_or(Alt);
  }

  const T& expect(const Twine& Msg) const& {
    this->assert_expect(Msg);
    return Storage.value();
  }
  T& expect(const Twine& Msg) & {
    this->assert_expect(Msg);
    return Storage.value();
  }
  T&& expect(const Twine& Msg) && {
    this->assert_expect(Msg);
    return std::move(Storage.value());
  }

  template <typename U> constexpr T value_or(U&& Alt) const& {
    return has_value() ? **this : EXI_FWD(Alt);
  }
  template <typename U> T value_or(U&& Alt) && {
    return has_value() ? std::move(**this) : EXI_FWD(Alt);
  }

  template <typename F> auto transform(F&& Fn) const& {
    using OutT = option_detail::Transform<F, const T&>;
    return has_value() ? OutT(EXI_FWD(Fn)(**this)) : std::nullopt;
  }
  template <typename F> auto transform(F&& Fn) & {
    using OutT = option_detail::Transform<F, T&>;
    return has_value() ? OutT(EXI_FWD(Fn)(**this)) : std::nullopt;
  }
  template <typename F> auto transform(F&& Fn) && {
    using OutT = option_detail::Transform<F, T&&>;
    return has_value()
      ? OutT(EXI_FWD(Fn)(std::move(**this))) : std::nullopt;
  }

private:
  EXI_INLINE EXI_NODEBUG void
   assert_expect(const Twine& Msg) const {
    if EXI_UNLIKELY(!has_value())
      exi::report_fatal_error(Msg);
  }
};

template <typename T> Option(T) -> Option<T>;

//////////////////////////////////////////////////////////////////////////
// Option<T&>

namespace option_detail {

/// Default storage for `Option<T&>`, this is for concrete `T`s.
template <typename T> struct RefStorage {
  T* Storage = nullptr;
public:
  using type = std::remove_const_t<T>;
  using value_type = T&;

  constexpr RefStorage() = default;
  constexpr RefStorage(std::nullopt_t) {}

  constexpr RefStorage(T& In EXI_LIFETIMEBOUND) noexcept
      : Storage(std::addressof(In)) {}
  constexpr RefStorage(T* In) noexcept
      : Storage(In) {}
  RefStorage(T&&) = delete;

  template <option_detail::explicitly_derived_ex<type> U>
  inline constexpr RefStorage(Option<U&> O) : Storage(O.data()) {}

  template <std::same_as<type> U>
  requires option_detail::is_const<T> 
  inline constexpr RefStorage(Option<U&> O) : Storage(O.data()) {}

  // Allow conversion from std::optional<T>.
  explicit constexpr RefStorage(std::optional<type>& V)
      : Storage(V ? &*V : nullptr) {}
  explicit constexpr RefStorage(const std::optional<type>& V)
    requires option_detail::not_const<T>
      : Storage(V ? &*V : nullptr) {}
  constexpr RefStorage(std::optional<type>&&) = delete;

  // Allow conversion from Option<T>.
  constexpr RefStorage(Option<type>& O)
      : Storage(O ? &*O : nullptr) {}
  constexpr RefStorage(const Option<type>& V)
    requires option_detail::not_const<T>
      : Storage(V ? &*V : nullptr) {}
  constexpr RefStorage(Option<type>&&) = delete;

  constexpr bool has_value() const { return Storage != nullptr; }
  constexpr bool is_some() const { return has_value(); }
  constexpr bool is_none() const { return !has_value(); }

  /// Reserved for `Option<T&>`. Converts the contained reference to
  /// an `Option<T>`, if it exists.
  constexpr Option<type> deref() const {
    return has_value() ? Option(**this) : std::nullopt;
  }

  template <typename U> constexpr type deref_or(U&& Alt) const {
    return deref().value_or(EXI_FWD(Alt));
  }
};

/// Default storage for `Option<T&>`, this is for abstract `T`s.
template <typename T> requires abstract<T> struct RefStorage<T> {
  T* Storage = nullptr;
public:
  using type = std::remove_const_t<T>;
  using value_type = T&;

  constexpr RefStorage() = default;
  constexpr RefStorage(std::nullopt_t) {}

  constexpr RefStorage(T& In EXI_LIFETIMEBOUND) noexcept
      : Storage(std::addressof(In)) {}
  constexpr RefStorage(T* In) noexcept
      : Storage(In) {}
  RefStorage(T&&) = delete;

  template <option_detail::explicitly_derived_ex<type> U>
  inline constexpr RefStorage(Option<U&> O) : Storage(O.data()) {}

  template <std::same_as<type> U>
  requires option_detail::is_const<T> 
  inline constexpr RefStorage(Option<U&> O) : Storage(O.data()) {}

  constexpr bool has_value() const { return Storage != nullptr; }
};

} // namespace option_detail

// Optional type specialized for direct references.
template <typename T> class Option<T&>
    : public option_detail::RefStorage<T> {
  using BaseT = option_detail::RefStorage<T>;
  using BaseT::Storage;
public:
  using type = BaseT::type;
  using value_type = BaseT::value_type;

  using BaseT::BaseT;
  constexpr Option(const Option/*T&*/& O) noexcept : BaseT(O.Storage) {}
  constexpr Option(Option&& O) noexcept : BaseT(O.Storage) {}

  /// Create a new object by constructing it in place with the given arguments.
  constexpr T& emplace(T& In EXI_LIFETIMEBOUND) {
    Storage = std::addressof(In);
    return In;
  }
  constexpr T& emplace(T&&) = delete;

  constexpr void swap(Option& O) noexcept {
    using std::swap;
    swap(BaseT::Storage, O.Storage);
  }

  constexpr Option& operator=(T& In EXI_LIFETIMEBOUND) {
    Storage = std::addressof(In);
    return *this;
  }
  constexpr Option& operator=(const Option&) = default;

  Option& operator=(T&& V) = delete;
  Option& operator=(Option&& O) = default;

  void reset() { Storage = nullptr; }

  constexpr T* data() const { return Storage; }
  constexpr T& value() const {
    exi_assert(has_value(), "value is inactive!");
    return *Storage;
  }
  
  constexpr explicit operator bool() const { return has_value(); }
  using BaseT::has_value;

  constexpr T* operator->() const {
    exi_invariant(has_value(), "value is inactive!");
    return Storage;
  }

  constexpr T& operator*() const {
    exi_invariant(has_value(), "value is inactive!");
    return *Storage;
  }

  T& expect(const Twine& Msg) const {
    if EXI_UNLIKELY(!has_value())
      exi::report_fatal_error(Msg);
    return *Storage;
  }

  template <typename U> constexpr T& value_or(U&& Alt) const {
    if (has_value())
      return **this;
    else
      return EXI_FWD(Alt);
  }

  template <typename F> auto transform(F&& Fn) const {
    using OutT = option_detail::Transform<F, T&>;
    return has_value() ? OutT(EXI_FWD(Fn)(**this)) : std::nullopt;
  }
};

//////////////////////////////////////////////////////////////////////////
// Other Functions

using std::nullopt_t;
using std::nullopt;

template <typename T>
constexpr void swap(Option<T>& LHS, Option<T>& RHS) noexcept {
  LHS.swap(RHS);
}

template <typename T, typename U>
constexpr bool operator==(const Option<T>& X,
                          const Option<U>& Y) {
  if (X && Y)
    return *X == *Y;
  return X.has_value() == Y.has_value();
}

template <typename T, typename U>
constexpr bool operator!=(const Option<T>& X,
                          const Option<U>& Y) {
  return !(X == Y);
}

template <typename T, typename U>
constexpr bool operator<(const Option<T>& X,
                         const Option<U>& Y) {
  if (X && Y)
    return *X < *Y;
  return X.has_value() < Y.has_value();
}

template <typename T, typename U>
constexpr bool operator<=(const Option<T>& X,
                          const Option<U>& Y) {
  return !(Y < X);
}

template <typename T, typename U>
constexpr bool operator>(const Option<T>& X,
                         const Option<U>& Y) {
  return Y < X;
}

template <typename T, typename U>
constexpr bool operator>=(const Option<T>& X,
                          const Option<U>& Y) {
  return !(X < Y);
}

template <typename T>
constexpr bool operator==(const Option<T>& X, std::nullopt_t) {
  return !X;
}

template <typename T>
constexpr bool operator==(std::nullopt_t, const Option<T>& X) {
  return X == std::nullopt;
}

template <typename T>
constexpr bool operator!=(const Option<T>& X, std::nullopt_t) {
  return !(X == std::nullopt);
}

template <typename T>
constexpr bool operator!=(std::nullopt_t, const Option<T>& X) {
  return X != std::nullopt;
}

template <typename T>
constexpr bool operator<(const Option<T> &, std::nullopt_t) {
  return false;
}

template <typename T>
constexpr bool operator<(std::nullopt_t, const Option<T>& X) {
  return X.has_value();
}

template <typename T>
constexpr bool operator<=(const Option<T>& X, std::nullopt_t) {
  return !(std::nullopt < X);
}

template <typename T>
constexpr bool operator<=(std::nullopt_t, const Option<T>& X) {
  return !(X < std::nullopt);
}

template <typename T>
constexpr bool operator>(const Option<T>& X, std::nullopt_t) {
  return std::nullopt < X;
}

template <typename T>
constexpr bool operator>(std::nullopt_t, const Option<T>& X) {
  return X < std::nullopt;
}

template <typename T>
constexpr bool operator>=(const Option<T>& X, std::nullopt_t) {
  return std::nullopt <= X;
}

template <typename T>
constexpr bool operator>=(std::nullopt_t, const Option<T>& X) {
  return X <= std::nullopt;
}

template <typename T>
constexpr bool operator==(const Option<T>& X, const T& Y) {
  return X && *X == Y;
}

template <typename T>
constexpr bool operator==(const T& X, const Option<T>& Y) {
  return Y && X == *Y;
}

template <typename T>
constexpr bool operator!=(const Option<T>& X, const T& Y) {
  return !(X == Y);
}

template <typename T>
constexpr bool operator!=(const T& X, const Option<T>& Y) {
  return !(X == Y);
}

template <typename T>
constexpr bool operator<(const Option<T>& X, const T& Y) {
  return !X || *X < Y;
}

template <typename T>
constexpr bool operator<(const T& X, const Option<T>& Y) {
  return Y && X < *Y;
}

template <typename T>
constexpr bool operator<=(const Option<T>& X, const T& Y) {
  return !(Y < X);
}

template <typename T>
constexpr bool operator<=(const T& X, const Option<T>& Y) {
  return !(Y < X);
}

template <typename T>
constexpr bool operator>(const Option<T>& X, const T& Y) {
  return Y < X;
}

template <typename T>
constexpr bool operator>(const T& X, const Option<T>& Y) {
  return Y < X;
}

template <typename T>
constexpr bool operator>=(const Option<T>& X, const T& Y) {
  return !(X < Y);
}

template <typename T>
constexpr bool operator>=(const T& X, const Option<T>& Y) {
  return !(X < Y);
}

} // namespace exi
