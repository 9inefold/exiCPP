//===- Common/D/Expect.hpp ------------------------------------------===//
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
/// This file defines the unexpect[_t] tag, Expect<T> and Unexpect<E>.
///
//===----------------------------------------------------------------===//

#include <Common/Features.hpp>
#include <type_traits>

namespace exi {

template <typename T, typename E> class Result;

/// Recreation of `std::unexpect_t`.
struct unexpect_t { explicit constexpr unexpect_t() = default; };
/// Recreation of `std::unexpect_t`.
inline constexpr unexpect_t unexpect;

//////////////////////////////////////////////////////////////////////////

/// Alternate tag for constructing `Result` values.
template <typename T> class Expect {
  static_assert(!std::same_as<T, std::in_place_t>);
  static_assert(!std::same_as<T, unexpect_t>);
  template <typename U, typename E> friend class Result;
  T Data;
public:
  constexpr Expect(const Expect&) = default;
  constexpr Expect(Expect&&) = default;

  template <typename U = T>
  constexpr explicit Expect(U&& Val) : Data(EXI_FWD(Val)) {}
  
  constexpr explicit Expect(
    std::in_place_t, auto&&...Args)
   : Data(EXI_FWD(Args)...) {}
  
  constexpr Expect& operator=(const Expect&) = default;
  constexpr Expect& operator=(Expect&&) = default;

  constexpr T& value() & noexcept { return Data; }
  constexpr const T& value() const& noexcept { return Data; }
  constexpr T&& value() && noexcept { return std::move(Data); }
};

/// Alternate tag for constructing `Result` values.
template <typename T> class Expect<T&> {
  static_assert(!std::same_as<T, std::in_place_t>);
  static_assert(!std::same_as<T, unexpect_t>);
  template <typename U, typename E> friend class Result;
  T* Data = nullptr;
public:
  constexpr Expect(const Expect&) = default;
  constexpr Expect(Expect&&) = default;

  template <typename U = T>
  requires std::convertible_to<U*, T*>
  constexpr explicit Expect(
    U& Val EXI_LIFETIMEBOUND) :
   Data(std::addressof(Val)) {}

  constexpr Expect& operator=(const Expect&) = default;
  constexpr Expect& operator=(Expect&&) = default;

  constexpr T& value() const noexcept { return *Data; }
};

template <typename T> Expect(T&) -> Expect<T&>;
template <typename T> Expect(const T&) -> Expect<const T&>;
template <typename T> Expect(T&&) -> Expect<T>;

template <typename T> Expect(Expect<T&>) -> Expect<T&>;
template <typename T> Expect(Expect<const T&>) -> Expect<const T&>;
template <typename T> Expect(Expect<T>) -> Expect<T>;

template <typename T>
EXI_INLINE constexpr decltype(auto) Ok(T&& Val) noexcept {
  return Expect(EXI_FWD(Val));
}

namespace H {
template <typename>
struct IsExpect : std::false_type {};

template <typename E>
struct IsExpect<Expect<E>> : std::true_type {};
} // namespace H

template <typename T>
concept is_expect = H::IsExpect<T>::value;

//////////////////////////////////////////////////////////////////////////

/// Reimplementation of `std::unexpected`.
template <typename E> class Unexpect {
  static_assert(!std::same_as<E, std::in_place_t>);
  static_assert(!std::same_as<E, unexpect_t>);
  template <typename T, typename G> friend class Result;
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
  template <typename T, typename G> friend class Result;
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
EXI_INLINE constexpr decltype(auto) Err(E&& Val) noexcept {
  return Unexpect(EXI_FWD(Val));
}

namespace H {
template <typename>
struct IsUnexpect : std::false_type {};

template <typename E>
struct IsUnexpect<Unexpect<E>> : std::true_type {};
} // namespace H

template <typename T>
concept is_unexpect = H::IsUnexpect<T>::value;

} // namespace exi
