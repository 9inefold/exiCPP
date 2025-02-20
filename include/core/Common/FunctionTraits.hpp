//===- Common/FunctionTraits.hpp ------------------------------------===//
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
/// This file defines some helper traits for dealing with function pointers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ConstexprLists.hpp>
#include <Common/Fundamental.hpp>
#include <concepts>

namespace exi {

namespace H {

/// Handles removing `noexcept` from function types.
template <typename T>
struct RemoveNoexcept : std::false_type { using type = T; };
/// Overload for const types.
/// Handles removing `noexcept` from function types.
template <typename T>
struct RemoveNoexcept<const T> : RemoveNoexcept<T> {
  using type = std::add_const_t<typename RemoveNoexcept<T>::type>;
};
/// Overload for raw non-class function types.
template <typename ReturnT, typename...Args>
struct RemoveNoexcept<ReturnT(Args...) noexcept> : std::true_type {
  using type = ReturnT(Args...);
};
/// Overload for non-class function type pointers.
template <typename ReturnT, typename...Args>
struct RemoveNoexcept<ReturnT(*)(Args...) noexcept> : std::true_type {
  using type = ReturnT(*)(Args...);
};
/// Overload for non-class function type references.
template <typename ReturnT, typename...Args>
struct RemoveNoexcept<ReturnT(&)(Args...) noexcept> : std::true_type {
  using type = ReturnT(*)(Args...);
};
/// Overload for class function types.
template <typename ReturnT, class ClassT, typename...Args>
struct RemoveNoexcept<ReturnT(ClassT::*)(Args...) noexcept> : std::true_type {
  using type = ReturnT(ClassT::*)(Args...);
};
/// Overload for const noexcept class function types.
template <typename ReturnT, typename ClassT, typename...Args>
struct RemoveNoexcept<ReturnT(ClassT::*)(Args...) const noexcept>
                    : std::true_type {
  using type = ReturnT(ClassT::*)(Args...) const;
};

//////////////////////////////////////////////////////////////////////////

/// Handles checking types for function pointers.
template <typename T>
struct IsFreeFunctionPtr : std::false_type {};
/// Overload for raw non-class function types.
template <typename ReturnT, typename...Args>
struct IsFreeFunctionPtr<ReturnT(Args...)> : std::true_type {};
/// Overload for non-class function type pointers.
template <typename ReturnT, typename...Args>
struct IsFreeFunctionPtr<ReturnT(*)(Args...)> : std::true_type {};

/// Handles checking types for method pointers.
template <typename T>
struct IsMethodPtr : std::false_type {};
/// Overload for class function types.
template <typename ReturnT, typename ClassT, typename...Args>
struct IsMethodPtr<ReturnT(ClassT::*)(Args...)> : std::true_type {};

} // namespace H

/// Removes the `noexcept` qualifier from function types.
template <typename FuncT>
using remove_noexcept_t = typename H::RemoveNoexcept<FuncT>::type;

/// If a type is `noexcept` qualified.
template <typename FuncT>
concept has_noexcept_v = H::RemoveNoexcept<FuncT>::value;

/// Checks if type is a free function.
template <typename FuncT>
concept is_free_function_ptr
  = H::IsFreeFunctionPtr<remove_noexcept_t<FuncT>>::value;

/// Checks if type is a method.
template <typename FuncT>
concept is_method_ptr = H::IsMethodPtr<remove_noexcept_t<FuncT>>::value;

/// Checks if type is a free function or method.
template <typename FuncT>
concept is_function_ptr = is_free_function_ptr<FuncT> || is_method_ptr<FuncT>;

//======================================================================//
// FunctionTraits
//======================================================================//

/// This class provides various trait information about a callable object.
///   * To access the number of arguments: Traits::num_args
///   * To access the type of an argument: Traits::arg_t<Index>
///   * To access the type of the result:  Traits::result_t
template <typename T, bool isClass = std::is_class<T>::value>
struct FunctionTraits : public FunctionTraits<decltype(&T::operator())> {};

/// Overload for class function types.
template <typename ReturnT, class ClassT, typename...Args>
struct FunctionTraits<ReturnT(ClassT::*)(Args...) const, false> {
  using type = ReturnT(ClassT::*)(Args...) const;

  /// The number of arguments to this function.
  static constexpr usize num_args = sizeof...(Args);

  /// The result type of this function.
  using result_t = ReturnT;

  /// The type of an argument to this function.
  template <usize Index>
  using arg_t = TypePackElement<Index, Args...>;
};
/// Overload for class function types.
template <typename ReturnT, class ClassT, typename...Args>
struct FunctionTraits<ReturnT(ClassT::*)(Args...), false>
    : public FunctionTraits<ReturnT(ClassT::*)(Args...) const> {
  using type = ReturnT(ClassT::*)(Args...);
};

/// Overload for non-class function types.
template <typename ReturnT, typename...Args>
struct FunctionTraits<ReturnT(*)(Args...), false> {
  using type = ReturnT(Args...);

  /// The number of arguments to this function.
  static constexpr usize num_args = sizeof...(Args);

  /// The result type of this function.
  using result_t = ReturnT;

  /// The type of an argument to this function.
  template <usize Index>
  using arg_t = TypePackElement<Index, Args...>;
};
/// Overload for const non-class function types.
template <typename ReturnT, typename...Args>
struct FunctionTraits<ReturnT(*const)(Args...), false>
    : public FunctionTraits<ReturnT(*)(Args...)> {};
/// Overload for non-class function type references.
template <typename ReturnT, typename...Args>
struct FunctionTraits<ReturnT(&)(Args...), false>
    : public FunctionTraits<ReturnT(*)(Args...)> {};

/// This class provides various trait information about a callable object.
///   * To access the number of arguments: Traits::num_args
///   * To access the type of an argument: Traits::arg_t<Index>
///   * To access the type of the result:  Traits::result_t
template <typename FuncT>
using function_traits = FunctionTraits<remove_noexcept_t<FuncT>>;

//======================================================================//
// FunctionRef Traits
//======================================================================//

template <typename Fn> class FunctionRef;

namespace H {

template <class CallbackT, typename Ret, typename...Args>
concept is_functionref = std::same_as<CallbackT, FunctionRef<Ret(Args...)>>;

template <class CallbackT, typename Ret, typename...Args>
concept callback_matches_signature
 = requires (CallbackT callable, Args...args) {
  { callable(std::forward<Args>(args)...) } -> std::convertible_to<Ret>;
};

template <class CallbackT, typename Ret, typename...Args>
concept callback_is_valid
  = (std::is_void_v<Ret> && std::invocable<CallbackT, Args...>)
  || callback_matches_signature<CallbackT, Ret, Args...>;

template <class CallbackT, typename Ret, typename...Args>
concept is_valid_functionref
  = !is_functionref<CallbackT, Ret, Args...>
  && callback_is_valid<CallbackT, Ret, Args...>;

} // namespace H

//======================================================================//
// Callback Traits
//======================================================================//

/// This class provides functions for wrapping a callable object. Does NOT take
/// ownership of any value.
template <class CallbackT, bool = kHasInlineFunctionPtrs>
struct ICallbackTraits {
  using UType = std::remove_reference_t<CallbackT>;
  using value_type = std::remove_cv_t<UType>;

  static uptr ToStorage(UType& callable EXI_LIFETIMEBOUND) {
    return reinterpret_cast<uptr>(&callable);
  }
  static UType& FromStorage(uptr storage) {
    return *reinterpret_cast<UType*>(storage);
  }
};

/// Overload for non-class function types. Only active when function pointers 
/// can be stored in a `uptr`. We don't specialize pointers directly so
/// the `noexcept` specifier doesn't need to be provided.
template <typename CallbackT>
requires is_free_function_ptr<CallbackT>
struct ICallbackTraits<CallbackT, true> {
  using UType = std::remove_pointer_t<CallbackT>;
  using value_type = UType*;

  static uptr ToStorage(UType* callable) {
    return reinterpret_cast<uptr>(callable);
  }
  static UType& FromStorage(uptr storage) {
    return *reinterpret_cast<UType*>(storage);
  }
};

//////////////////////////////////////////////////////////////////////////
// CallbackTraits

/// This class provides functions for wrapping a callable object.
template <class CallbackT>
struct CallbackTraits : public ICallbackTraits<CallbackT> {};
/// Overload for const non-class function types.
template <class CallbackT>
requires is_free_function_ptr<std::remove_cvref_t<CallbackT>>
struct CallbackTraits<CallbackT>
 : public ICallbackTraits<std::remove_cvref_t<CallbackT>> {};

/// This class provides functions for wrapping a callable object.
template <typename CallbackT>
using callback_traits = CallbackTraits<std::remove_reference_t<CallbackT>>;

} // namespace exi
