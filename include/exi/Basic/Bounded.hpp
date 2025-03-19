//===- exi/Basic/Bounded.hpp ----------------------------------------===//
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
/// This file defines an integer with bounds.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Fundamental.hpp>
#include <core/Support/Limits.hpp>
#include <core/Support/IntCast.hpp>
#include <compare>
#include <concepts>
#include <type_traits>

namespace exi {

/// Type of an unbounded tag.
struct unbounded_t {
  using _secret_tag = exi::Dummy_::_secret_tag;
  EXI_ALWAYS_INLINE EXI_NODEBUG constexpr explicit
   unbounded_t(_secret_tag, _secret_tag) noexcept {}
};

/// A tag marking a `Bounded<...>` as unbounded.
inline constexpr unbounded_t unbounded {
  unbounded_t::_secret_tag{}, unbounded_t::_secret_tag{}};

/// Type of a typed unbounded tag.
template <std::integral T> struct unbounded_of_t {
  EXI_ALWAYS_INLINE EXI_NODEBUG constexpr explicit
   unbounded_of_t(unbounded_t::_secret_tag,
                   unbounded_t::_secret_tag) noexcept {}
};

/// A typed tag marking a `Bounded<T>` as unbounded.
template <std::integral T>
inline constexpr unbounded_of_t<T> unbounded_of {
  unbounded_t::_secret_tag{}, unbounded_t::_secret_tag{}};

//////////////////////////////////////////////////////////////////////////
// Comparisons

inline constexpr bool operator==(unbounded_t, unbounded_t) {
  return true;
}

template <typename T>
constexpr bool operator==(unbounded_t, unbounded_of_t<T>) {
  return true;
}

template <typename T>
constexpr bool operator==(unbounded_of_t<T>, unbounded_t) {
  return true;
}

template <typename T, typename U>
constexpr bool operator==(unbounded_of_t<T>, unbounded_of_t<U>) {
  return true;
}

//===----------------------------------------------------------------===//
// Bounded
//===----------------------------------------------------------------===//

/// Integral used for situations where an "unbounded" state is required.
/// It is in the unbounded state by default.
template <std::integral T> class Bounded {
  template <std::integral> friend class Bounded; 
  using StrongOrd = std::strong_ordering;
  using WeakOrd = std::weak_ordering;

  using unbounded_tag = unbounded_of_t<T>;
  static constexpr T kUnbounded = exi::max_v<T>;
  T Data = kUnbounded;

  template <typename Int>
  static constexpr T FromOther(Int X) noexcept {
    return exi::IntCastOr<T>(X, kUnbounded);
  }

  template <typename Int>
  static constexpr T FromOther(Bounded<Int> Other) noexcept {
    if (Other.unbounded())
      return kUnbounded;
    return exi::IntCastOr<T>(Other.Data, kUnbounded);
  }

public:
  static constexpr unbounded_tag Unbounded() noexcept {
    return unbounded_of<T>;
  }

  ////////////////////////////////////////////////////////////////////////
  // Constructors

  constexpr Bounded() noexcept = default;
  constexpr Bounded(unbounded_t) : Data(kUnbounded) {}
  constexpr Bounded(unbounded_tag) : Data(kUnbounded) {}

  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  explicit constexpr Bounded(Int Data)
      : Bounded(FromOther(Data)) {}
  constexpr Bounded(T Data) : Data(Data) {
    exi_invariant(Data != kUnbounded);
  }

  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  explicit constexpr Bounded(Bounded<Int> Other)
      : Data(FromOther(Other)) {}
  constexpr Bounded(const Bounded&) = default;
  constexpr Bounded(Bounded&&) noexcept = default;

  ////////////////////////////////////////////////////////////////////////
  // Assignment

  constexpr Bounded& operator=(unbounded_t) noexcept {
    this->Data = kUnbounded;
    return *this;
  }
  constexpr Bounded& operator=(unbounded_tag) noexcept {
    this->Data = kUnbounded;
    return *this;
  }

  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  constexpr Bounded& operator=(Int Data) {
    this->Data = FromOther(Data);
    return *this;
  }

  constexpr Bounded& operator=(T Data) noexcept {
    exi_invariant(Data != kUnbounded);
    this->Data = Data;
    return *this;
  }

  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  constexpr Bounded& operator=(Bounded<Int> Other) {
    this->Data = FromOther(Other);
    return *this;
  }
  
  constexpr Bounded& operator=(const Bounded&) = default;
  constexpr Bounded& operator=(Bounded&&) noexcept = default;

  ////////////////////////////////////////////////////////////////////////
  // Observers

  EXI_INLINE constexpr T data() const { return Data; }
  EXI_INLINE constexpr T value() const { return Data; }

  EXI_INLINE constexpr bool bounded() const {
    return Data != kUnbounded;
  }
  EXI_INLINE constexpr bool unbounded() const {
    return Data == kUnbounded;
  }

  ////////////////////////////////////////////////////////////////////////
  // Operators
  // FIXME: Possibly use partial_ordering? This is essentially an INF value.

  constexpr T operator*() const { return Data; }
  constexpr operator T() const { return Data; }
  constexpr explicit operator bool() const { return Data; }

  constexpr bool operator==(const Bounded&) const = default;
  constexpr StrongOrd operator<=>(const Bounded&) const = default;

  constexpr bool operator==(T Val) const {
    return this->Data == Val;
  }
  constexpr StrongOrd operator<=>(T Val) const {
    return this->Data <=> Val;
  }

  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  constexpr bool operator==(Bounded<Int> RHS) const {
    return this->Data == FromOther(RHS);
  }
  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  constexpr WeakOrd operator<=>(Bounded<Int> RHS) const {
    return this->Data <=> FromOther(RHS);
  }

  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  constexpr bool operator==(Int RHS) const {
    return this->Data == FromOther(RHS);
  }
  template <std::integral Int>
  requires(!std::same_as<T, Int>)
  constexpr WeakOrd operator<=>(Int RHS) const {
    return this->Data <=> FromOther(RHS);
  }

  constexpr bool operator==(unbounded_t) const {
    return this->unbounded();
  }
  constexpr StrongOrd operator<=>(unbounded_t) const {
    return Data <=> kUnbounded;
  }

  template <std::integral Int>
  constexpr bool operator==(unbounded_of_t<Int>) const {
    return this->unbounded();
  }
  template <std::integral Int>
  constexpr WeakOrd operator<=>(unbounded_of_t<Int>) const {
    return Data <=> kUnbounded;
  }
};

template <typename T>
Bounded(unbounded_of_t<T>) -> Bounded<T>;

template <std::integral T> Bounded(T) -> Bounded<T>;

} // namespace exi
