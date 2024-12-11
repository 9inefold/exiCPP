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
/// This file defines the FunctionRef class
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
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
template <typename Fn> class FunctionRef;

namespace H {

//////////////////////////////////////////////////////////////////////////
// Traits

template <class CB, typename Ret, typename...Params>
concept is_functionref = std::same_as<CB, FunctionRef<Ret(Params...)>>;

template <class CB, typename Ret, typename...Params>
concept CB_has_valid_function_signature
 = requires (CB callable, Params...params) {
  { callable(std::forward<Params>(params)...) } -> std::convertible_to<Ret>;
};

template <class CB, typename Ret, typename...Params>
concept CB_is_valid_signature
  = std::is_void_v<Ret>
  || CB_has_valid_function_signature<CB, Ret, Params...>;

template <class CB, typename Ret, typename...Params>
concept CB_is_valid_functionref
  = !is_functionref<CB, Ret, Params...>
  && CB_is_valid_signature<CB, Ret, Params...>;

//////////////////////////////////////////////////////////////////////////
// is_func_ptr

template <typename T>
struct IsFuncPtr : std::false_type {};

template <typename Ret, typename...Params>
struct IsFuncPtr<Ret(Params...)> : std::true_type {};

template <typename Ret, typename...Params>
struct IsFuncPtr<Ret(Params...) noexcept> : std::true_type {};

template <typename Ret, typename...Params>
struct IsFuncPtr<Ret(*)(Params...)> : std::true_type {};

template <typename Ret, typename...Params>
struct IsFuncPtr<Ret(*)(Params...) noexcept> : std::true_type {};

template <typename CB>
concept is_func_ptr = IsFuncPtr<std::remove_cvref_t<CB>>::value;

//////////////////////////////////////////////////////////////////////////
// callback_type

template <class CB, bool = is_func_ptr<CB>>
struct CallbackType {
  using UType = std::remove_reference_t<CB>;
  using value_type = UType;
  using type = value_type*;
  static uptr GetStorage(value_type& callable) {
    return reinterpret_cast<uptr>(&callable);
  }
};

template <typename CB>
requires (hasInlineFunctionPtrs)
struct CallbackType<CB, true> {
  using UType = std::remove_pointer_t<std::remove_cvref_t<CB>>;
  using value_type = UType*;
  using type = value_type;
  static uptr GetStorage(value_type callable) {
    return reinterpret_cast<uptr>(callable);
  }
};

template <typename CB>
using callback_type_t = CallbackType<CB>::type;

template <typename CB>
EXI_INLINE uptr callback_storage(CB &&callable EXI_LIFETIMEBOUND) noexcept {
  return CallbackType<CB>::GetStorage(callable);
}

} // namespace H

//////////////////////////////////////////////////////////////////////////
// FunctionRef

template <typename Ret, typename...Params>
class FunctionRef<Ret(Params...)> {
  Ret (*callback)(uptr callable, Params...params) = nullptr;
  uptr callable;

  template <class CB>
  static Ret CallbackFn(uptr callable, Params...params) {
    using Cast = H::callback_type_t<CB>;
    return (*reinterpret_cast<Cast>(callable))(
        std::forward<Params>(params)...);
  }

public:
  FunctionRef() = default;
  FunctionRef(std::nullptr_t) {}

  template <class CB>
  requires H::CB_is_valid_functionref<CB, Ret, Params...>
  FunctionRef(CB &&callable EXI_LIFETIMEBOUND) :
   callback(&CallbackFn<std::remove_reference_t<CB>>),
   callable(H::callback_storage(EXI_FWD(callable))) {
  }

  Ret operator()(Params ...params) const {
    return callback(callable, std::forward<Params>(params)...);
  }

  explicit operator bool() const { return callback; }

  bool operator==(const FunctionRef<Ret(Params...)> &Other) const {
    return callable == Other.callable;
  }
};

/// Alias for `FunctionRef`.
template <class Fn>
using function_ref = FunctionRef<Fn>;

} // namespace exi
