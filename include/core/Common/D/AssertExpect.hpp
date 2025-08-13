//===- Common/D/AssertExpect.hpp ------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file defines the AssertExpect class, which is a utility class for
/// implementing the expect and assert_expect functions.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/StrRef.hpp>
#include <Support/ErrorHandle.hpp>
#include <concepts>

// TODO: Annotate more lifetimebounds

namespace exi {
namespace H {

/// CRTP utility class for splitting out `expect`/`assert_expect` functions.
/// Requires the user to implement `has_value` and `value_unchecked`.
/// @tparam Owning If the class owns the type, or if it is a reference wrapper.
template <class Derived, bool Owning> class AssertExpect;

/// Splits out implementation details for the `assert_expect` function.
template <class Derived> class AssertExpectCommon {
protected:
  ALWAYS_INLINE constexpr Derived& super() {
    static_assert(std::derived_from<Derived, AssertExpectCommon<Derived>>);
    return *static_cast<Derived*>(this);
  }
  ALWAYS_INLINE constexpr const Derived& super() const {
    static_assert(std::derived_from<Derived, AssertExpectCommon<Derived>>);
    return *static_cast<const Derived*>(this);
  }
public:
  EXI_INLINE EXI_NODEBUG void
   assert_expect(const char* Msg) const {
    if EXI_UNLIKELY(!super().has_value())
      exi::report_fatal_error(Msg);
  }
  EXI_INLINE EXI_NODEBUG void
   assert_expect(StrRef Msg) const {
    if EXI_UNLIKELY(!super().has_value())
      exi::report_fatal_error(Msg);
  }
  EXI_INLINE EXI_NODEBUG void
   assert_expect(const Twine& Msg) const {
    if EXI_UNLIKELY(!super().has_value())
      exi::report_fatal_error(Msg);
  }
};

/// CRTP utility class for splitting out `expect`/`assert_expect` functions.
/// @tparam Derived Must own the data.
template <class Derived>
class AssertExpect<Derived, true> : public AssertExpectCommon<Derived> {
  using AssertExpectCommon<Derived>::super;
public:
  EXI_FLATTEN const auto& expect(const char* Msg) const& {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  EXI_FLATTEN auto& expect(const char* Msg) & {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  EXI_FLATTEN auto&& expect(const char* Msg) && {
    this->assert_expect(Msg);
    return std::move(super().value_unchecked());
  }

  const auto& expect(StrRef Msg) const& {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  auto& expect(StrRef Msg) & {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  auto&& expect(StrRef Msg) && {
    this->assert_expect(Msg);
    return std::move(super().value_unchecked());
  }

  const auto& expect(const Twine& Msg) const& {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  auto& expect(const Twine& Msg) & {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  auto&& expect(const Twine& Msg) && {
    this->assert_expect(Msg);
    return std::move(super().value_unchecked());
  }
};

/// CRTP utility class for splitting out `expect`/`assert_expect` functions.
/// @tparam Derived Must be a non-owning reference wrapping class.
template <class Derived>
class AssertExpect<Derived, false> : public AssertExpectCommon<Derived> {
  using AssertExpectCommon<Derived>::super;
public:
  EXI_FLATTEN auto& expect(const char* Msg) const {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  EXI_FLATTEN auto& expect(StrRef Msg) const {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
  EXI_FLATTEN auto& expect(const Twine& Msg) const {
    this->assert_expect(Msg);
    return super().value_unchecked();
  }
};

} // namespace H
} // namespace exi
