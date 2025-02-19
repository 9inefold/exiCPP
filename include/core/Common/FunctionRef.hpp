//===- Common/FunctionRef.hpp ---------------------------------------===//
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
/// This file defines the FunctionRef class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Common/FunctionTraits.hpp>
#include <concepts>
#include <type_traits>
#include <utility>

namespace exi {
/// An efficient, type-erasing, non-owning reference to a callable. This is
/// intended for use as the type of a function parameter that is not used
/// after the function in question returns.
///
/// This class does not own the callable, so it is not in general safe to store
/// a FunctionRef.
template <typename Ret, typename...Params>
class FunctionRef<Ret(Params...)> {
  using CallbackT = Ret(*)(uptr callable, Params...params);
  CallbackT Callback = nullptr;
  uptr Callable;

  template <class CB>
  static Ret CallbackFn(uptr callable, Params...params) {
    using TraitsT = callback_traits<CB>;
    return (TraitsT::FromStorage(callable))(
        std::forward<Params>(params)...);
  }

public:
  FunctionRef() = default;
  FunctionRef(std::nullptr_t) {}

  template <H::is_valid_functionref<Ret, Params...> CB>
  FunctionRef(CB &&callable EXI_LIFETIMEBOUND) :
   Callback(&CallbackFn<std::remove_reference_t<CB>>),
   Callable(callback_traits<CB>::ToStorage(callable)) {
  }

  Ret operator()(Params...params) const {
    return Callback(Callable, std::forward<Params>(params)...);
  }

  explicit operator bool() const { return Callback; }

  bool operator==(const FunctionRef<Ret(Params...)> &Other) const {
    return this->Callable == Other.Callable;
  }
};

/// Alias for `FunctionRef`.
template <class Fn>
using function_ref = FunctionRef<Fn>;

} // namespace exi
