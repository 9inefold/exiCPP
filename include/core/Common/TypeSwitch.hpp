//===- Common/TypeSwitch.hpp ----------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
/// This file implements the StringSwitch template, which mimics a switch()
/// statement whose cases are string literals.
///
//===----------------------------------------------------------------===//

#pragma once

// TODO

// #include "llvm/ADT/STLExtras.h"
#include <Common/Option.hpp>
#include <Support/Casting.hpp>

namespace exi {
namespace H {

template <typename DerivedT, typename T> class TypeSwitchBase {
public:
  TypeSwitchBase(const T &value) : value(value) {}
  TypeSwitchBase(TypeSwitchBase &&other) : value(other.value) {}
  ~TypeSwitchBase() = default;

  /// TypeSwitchBase is not copyable.
  TypeSwitchBase(const TypeSwitchBase &) = delete;
  void operator=(const TypeSwitchBase &) = delete;
  void operator=(TypeSwitchBase &&other) = delete;

  /// Invoke a case on the derived class with multiple case types.
  template <typename CaseT, typename CaseT2, typename... CaseTs,
            typename CallableT>
  // This is marked always_inline and nodebug so it doesn't show up in stack
  // traces at -O0 (or other optimization levels).  Large TypeSwitch's are
  // common, are equivalent to a switch, and don't add any value to stack
  // traces.
  EXI_ALWAYS_INLINE EXI_NODEBUG DerivedT &
  Case(CallableT &&caseFn) {
    DerivedT& D = static_cast<DerivedT&>(*this);
    return D.template Case<CaseT>(caseFn)
      .template Case<CaseT2, CaseTs...>(caseFn);
  }

  /// Invoke a case on the derived class, inferring the type of the Case from
  /// the first input of the given callable.
  /// Note: This inference rules for this overload are very simple: strip
  ///       pointers and references.
  template <typename CallableT> DerivedT &Case(CallableT &&caseFn) {
    // TODO: function_traits
    using Traits = function_traits<std::decay_t<CallableT>>;
    using CaseT = std::remove_cv_t<std::remove_pointer_t<
      std::remove_reference_t<typename Traits::template arg_t<0>>>>;

    DerivedT &derived = static_cast<DerivedT &>(*this);
    return derived.template Case<CaseT>(std::forward<CallableT>(caseFn));
  }

protected:
  /// Attempt to dyn_cast the given `value` to `CastT`.
  template <typename CastT, typename ValueT>
  static decltype(auto) castValue(ValueT &&value) {
    return dyn_cast<CastT>(value);
  }

  /// The root value we are switching on.
  const T value;
};

} // namespace H

/// This class implements a switch-like dispatch statement for a value of 'T'
/// using dyn_cast functionality. Each `Case<T>` takes a callable to be invoked
/// if the root value isa<T>, the callable is invoked with the Result of
/// dyn_cast<T>() as a parameter.
///
/// Example:
///  Operation *op = ...;
///  LogicalResult Result = TypeSwitch<Operation *, LogicalResult>(op)
///    .Case<ConstantOp>([](ConstantOp op) { ... })
///    .Default([](Operation *op) { ... });
///
template <typename T, typename ResultT = void>
class TypeSwitch : public H::TypeSwitchBase<TypeSwitch<T, ResultT>, T> {
public:
  using BaseType = H::TypeSwitchBase<TypeSwitch<T, ResultT>, T>;
  using BaseType::BaseType;
  using BaseType::Case;
  TypeSwitch(TypeSwitch &&other) = default;

  /// Add a case on the given type.
  template <typename CaseT, typename CallableT>
  TypeSwitch<T, ResultT> &Case(CallableT &&caseFn) {
    if (Result)
      return *this;

    // Check to see if CaseT applies to 'value'.
    if (auto caseValue = BaseType::template castValue<CaseT>(this->value))
      Result.emplace(caseFn(caseValue));
    return *this;
  }

  /// As a default, invoke the given callable within the root value.
  template <typename CallableT>
  [[nodiscard]] ResultT Default(CallableT &&defaultFn) {
    if (Result)
      return std::move(*Result);
    return defaultFn(this->value);
  }
  /// As a default, return the given value.
  [[nodiscard]] ResultT Default(ResultT defaultResult) {
    if (Result)
      return std::move(*Result);
    return defaultResult;
  }

  [[nodiscard]] operator ResultT() {
    exi_assert(Result.has_value(), "Fell off the end of a type-switch");
    return std::move(*Result);
  }

private:
  /// The pointer to the Result of this switch statement, once known,
  /// null before that.
  Option<ResultT> Result;
};

/// Specialization of TypeSwitch for void returning callables.
template <typename T>
class TypeSwitch<T, void>
    : public H::TypeSwitchBase<TypeSwitch<T, void>, T> {
public:
  using BaseType = H::TypeSwitchBase<TypeSwitch<T, void>, T>;
  using BaseType::BaseType;
  using BaseType::Case;
  TypeSwitch(TypeSwitch &&other) = default;

  /// Add a case on the given type.
  template <typename CaseT, typename CallableT>
  TypeSwitch<T, void> &Case(CallableT &&caseFn) {
    if (foundMatch)
      return *this;

    // Check to see if any of the types apply to 'value'.
    if (auto caseValue = BaseType::template castValue<CaseT>(this->value)) {
      caseFn(caseValue);
      foundMatch = true;
    }
    return *this;
  }

  /// As a default, invoke the given callable within the root value.
  template <typename CallableT> void Default(CallableT &&defaultFn) {
    if (!foundMatch)
      defaultFn(this->value);
  }

private:
  /// A flag detailing if we have already found a match.
  bool foundMatch = false;
};

} // namespace exi
