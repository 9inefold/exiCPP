//===- Common/D/OptionStorage.hpp -----------------------------------===//
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
/// This file defines the OptionStorage class, which can be specialized to
/// allow for more compact Option representations.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Support/ErrorHandle.hpp>
#include <concepts>
#include <utility>

namespace exi {

/// The storage provider for `Option<T>`.
/// You must implement the following API:
///
/// ```cpp
/// class OptionStorage<MyType: T> {
///   OptionStorage() : Empty() {}
///   OptionStorage(in_place_t, auto&&...Args) : Active(Args...) {}
///
///   OptionStorage& operator=(const T&);
///   OptionStorage& operator=(T&&);
///   
///   T& emplace(auto&&...Args);
///   void reset();
///   
///   T& value() &;
///   const T& value() const&;
///   T&& value() &&;
///   
///   T& value_unchecked() &;
///   const T& value_unchecked() const&;
///   T&& value_unchecked() &&;
///   
///   bool has_value() const;
/// }
/// ```
///
/// You are encouraged to make these `constexpr`, but it is not a requirement.
template <typename T> class OptionStorage;

/// Used to implement the `value`/`value_unchecked` functions when there is a
/// single shared value source for data.
/// @param CONSTEXPR The keyword that should be passed to all implementations.
/// @param TYPE The base type (eg. the `T` in `Option<T>`)
/// @param EXPR The expression the implementation recieves.
#define EXI_OPTIONSTORAGE_IMPL_VALUE(CONSTEXPR, TYPE, EXPR...)                \
CONSTEXPR const TYPE& value() const& EXI_LIFETIMEBOUND {                      \
  exi_invariant(has_value(), "value is inactive!");                           \
  return EXPR;                                                                \
}                                                                             \
CONSTEXPR TYPE& value() & EXI_LIFETIMEBOUND {                                 \
  exi_invariant(has_value(), "value is inactive!");                           \
  return EXPR;                                                                \
}                                                                             \
CONSTEXPR TYPE&& value() && {                                                 \
  exi_invariant(has_value(), "value is inactive!");                           \
  return std::move(EXPR);                                                     \
}                                                                             \
                                                                              \
CONSTEXPR const TYPE& value_unchecked() const& EXI_LIFETIMEBOUND {            \
  return EXPR;                                                                \
}                                                                             \
CONSTEXPR TYPE& value_unchecked() & EXI_LIFETIMEBOUND {                       \
  return EXPR;                                                                \
}                                                                             \
CONSTEXPR TYPE&& value_unchecked() && {                                       \
  return std::move(EXPR);                                                     \
}

} // namespace exi
